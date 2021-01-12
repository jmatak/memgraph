#include "storage/v2/replication/replication_client.hpp"

#include <algorithm>
#include <type_traits>

#include "storage/v2/durability/durability.hpp"
#include "storage/v2/replication/config.hpp"
#include "storage/v2/replication/enums.hpp"
#include "storage/v2/transaction.hpp"
#include "utils/file_locker.hpp"

namespace storage {

namespace {
template <typename>
[[maybe_unused]] inline constexpr bool always_false_v = false;
}  // namespace

////// ReplicationClient //////
Storage::ReplicationClient::ReplicationClient(
    std::string name, Storage *storage, const io::network::Endpoint &endpoint,
    const replication::ReplicationMode mode,
    const replication::ReplicationClientConfig &config)
    : name_(std::move(name)), storage_(storage), mode_(mode) {
  if (config.ssl) {
    rpc_context_.emplace(config.ssl->key_file, config.ssl->cert_file);
  } else {
    rpc_context_.emplace();
  }

  rpc_client_.emplace(endpoint, &*rpc_context_);
  TryInitializeClient();

  if (config.timeout && replica_state_ != replication::ReplicaState::INVALID) {
    timeout_.emplace(*config.timeout);
    timeout_dispatcher_.emplace();
  }
}

/// @throws rpc::RpcFailedException
void Storage::ReplicationClient::InitializeClient() {
  uint64_t current_commit_timestamp{kTimestampInitialId};

  std::optional<std::string> epoch_id;
  {
    // epoch_id_ can be changed if we don't take this lock
    std::unique_lock engine_guard(storage_->engine_lock_);
    epoch_id.emplace(storage_->epoch_id_);
  }

  auto stream{rpc_client_->Stream<HeartbeatRpc>(
      storage_->last_commit_timestamp_, std::move(*epoch_id))};

  const auto response = stream.AwaitResponse();
  std::optional<uint64_t> branching_point;
  if (response.epoch_id != storage_->epoch_id_ &&
      response.current_commit_timestamp != kTimestampInitialId) {
    const auto &epoch_history = storage_->epoch_history_;
    const auto epoch_info_iter =
        std::find_if(epoch_history.crbegin(), epoch_history.crend(),
                     [&](const auto &epoch_info) {
                       return epoch_info.first == response.epoch_id;
                     });
    if (epoch_info_iter == epoch_history.crend()) {
      branching_point = 0;
    } else if (epoch_info_iter->second != response.current_commit_timestamp) {
      branching_point = epoch_info_iter->second;
    }
  }
  if (branching_point) {
    LOG(ERROR) << "Replica " << name_ << " cannot be used with this instance."
               << " Please start a clean instance of Memgraph server"
               << " on the specified endpoint.";
    return;
  }

  current_commit_timestamp = response.current_commit_timestamp;
  DLOG(INFO) << "Current timestamp on replica: " << current_commit_timestamp;
  DLOG(INFO) << "Current MAIN timestamp: "
             << storage_->last_commit_timestamp_.load();
  if (current_commit_timestamp == storage_->last_commit_timestamp_.load()) {
    DLOG(INFO) << "Replica up to date";
    std::unique_lock client_guard{client_lock_};
    replica_state_.store(replication::ReplicaState::READY);
  } else {
    DLOG(INFO) << "Replica is behind";
    {
      std::unique_lock client_guard{client_lock_};
      replica_state_.store(replication::ReplicaState::RECOVERY);
    }
    thread_pool_.AddTask(
        [=, this] { this->RecoverReplica(current_commit_timestamp); });
  }
}

void Storage::ReplicationClient::TryInitializeClient() {
  try {
    InitializeClient();
  } catch (const rpc::RpcFailedException &) {
    std::unique_lock client_guarde{client_lock_};
    replica_state_.store(replication::ReplicaState::INVALID);
    LOG(ERROR) << "Failed to connect to replica " << name_ << " at "
               << rpc_client_->Endpoint();
  }
}

void Storage::ReplicationClient::HandleRpcFailure() {
  LOG(ERROR) << "Couldn't replicate data to " << name_;
  thread_pool_.AddTask([this] {
    rpc_client_->Abort();
    this->TryInitializeClient();
  });
}

SnapshotRes Storage::ReplicationClient::TransferSnapshot(
    const std::filesystem::path &path) {
  auto stream{rpc_client_->Stream<SnapshotRpc>()};
  replication::Encoder encoder(stream.GetBuilder());
  encoder.WriteFile(path);
  return stream.AwaitResponse();
}

WalFilesRes Storage::ReplicationClient::TransferWalFiles(
    const std::vector<std::filesystem::path> &wal_files) {
  CHECK(!wal_files.empty()) << "Wal files list is empty!";
  auto stream{rpc_client_->Stream<WalFilesRpc>(wal_files.size())};
  replication::Encoder encoder(stream.GetBuilder());
  for (const auto &wal : wal_files) {
    DLOG(INFO) << "Sending wal file: " << wal;
    encoder.WriteFile(wal);
  }

  return stream.AwaitResponse();
}

OnlySnapshotRes Storage::ReplicationClient::TransferOnlySnapshot(
    const uint64_t snapshot_timestamp) {
  auto stream{rpc_client_->Stream<OnlySnapshotRpc>(snapshot_timestamp)};
  replication::Encoder encoder{stream.GetBuilder()};
  encoder.WriteString(storage_->epoch_id_);
  return stream.AwaitResponse();
}

void Storage::ReplicationClient::StartTransactionReplication(
    const uint64_t current_wal_seq_num) {
  std::unique_lock guard(client_lock_);
  const auto status = replica_state_.load();
  switch (status) {
    case replication::ReplicaState::RECOVERY:
      DLOG(INFO) << "Replica " << name_ << " is behind MAIN instance";
      return;
    case replication::ReplicaState::REPLICATING:
      DLOG(INFO) << "Replica " << name_ << " missed a transaction";
      // We missed a transaction because we're still replicating
      // the previous transaction so we need to go to RECOVERY
      // state to catch up with the missing transaction
      // We cannot queue the recovery process here because
      // an error can happen while we're replicating the previous
      // transaction after which the client should go to
      // INVALID state before starting the recovery process
      replica_state_.store(replication::ReplicaState::RECOVERY);
      return;
    case replication::ReplicaState::INVALID:
      HandleRpcFailure();
      return;
    case replication::ReplicaState::READY:
      CHECK(!replica_stream_);
      try {
        replica_stream_.emplace(
            ReplicaStream{this, storage_->last_commit_timestamp_.load(),
                          current_wal_seq_num});
        replica_state_.store(replication::ReplicaState::REPLICATING);
      } catch (const rpc::RpcFailedException &) {
        replica_state_.store(replication::ReplicaState::INVALID);
        HandleRpcFailure();
      }
      return;
  }
}

void Storage::ReplicationClient::IfStreamingTransaction(
    const std::function<void(ReplicaStream &handler)> &callback) {
  // We can only check the state because it guarantees to be only
  // valid during a single transaction replication (if the assumption
  // that this and other transaction replication functions can only be
  // called from a one thread stands)
  if (replica_state_ != replication::ReplicaState::REPLICATING) {
    return;
  }

  try {
    callback(*replica_stream_);
  } catch (const rpc::RpcFailedException &) {
    {
      std::unique_lock client_guard{client_lock_};
      replica_state_.store(replication::ReplicaState::INVALID);
    }
    HandleRpcFailure();
  }
}

void Storage::ReplicationClient::FinalizeTransactionReplication() {
  // We can only check the state because it guarantees to be only
  // valid during a single transaction replication (if the assumption
  // that this and other transaction replication functions can only be
  // called from a one thread stands)
  if (replica_state_ != replication::ReplicaState::REPLICATING) {
    return;
  }

  if (mode_ == replication::ReplicationMode::ASYNC) {
    thread_pool_.AddTask(
        [this] { this->FinalizeTransactionReplicationInternal(); });
  } else if (timeout_) {
    CHECK(mode_ == replication::ReplicationMode::SYNC)
        << "Only SYNC replica can have a timeout.";
    CHECK(timeout_dispatcher_) << "Timeout thread is missing";
    timeout_dispatcher_->WaitForTaskToFinish();

    timeout_dispatcher_->active = true;
    thread_pool_.AddTask([&, this] {
      this->FinalizeTransactionReplicationInternal();
      std::unique_lock main_guard(timeout_dispatcher_->main_lock);
      // TimerThread can finish waiting for timeout
      timeout_dispatcher_->active = false;
      // Notify the main thread
      timeout_dispatcher_->main_cv.notify_one();
    });

    timeout_dispatcher_->StartTimeoutTask(*timeout_);

    // Wait until one of the threads notifies us that they finished executing
    // Both threads should first set the active flag to false
    {
      std::unique_lock main_guard(timeout_dispatcher_->main_lock);
      timeout_dispatcher_->main_cv.wait(
          main_guard, [&] { return !timeout_dispatcher_->active.load(); });
    }

    // TODO (antonio2368): Document and/or polish SEMI-SYNC to ASYNC fallback.
    if (replica_state_ == replication::ReplicaState::REPLICATING) {
      mode_ = replication::ReplicationMode::ASYNC;
      timeout_.reset();
      // This can only happen if we timeouted so we are sure that
      // Timeout task finished
      // We need to delete timeout dispatcher AFTER the replication
      // finished because it tries to acquire the timeout lock
      // and acces the `active` variable`
      thread_pool_.AddTask([this] { timeout_dispatcher_.reset(); });
    }
  } else {
    FinalizeTransactionReplicationInternal();
  }
}

void Storage::ReplicationClient::FinalizeTransactionReplicationInternal() {
  CHECK(replica_stream_) << "Missing stream for transaction deltas";
  try {
    auto response = replica_stream_->Finalize();
    replica_stream_.reset();
    std::unique_lock client_guard(client_lock_);
    if (!response.success ||
        replica_state_ == replication::ReplicaState::RECOVERY) {
      replica_state_.store(replication::ReplicaState::RECOVERY);
      thread_pool_.AddTask([&, this] {
        this->RecoverReplica(response.current_commit_timestamp);
      });
    } else {
      replica_state_.store(replication::ReplicaState::READY);
    }
  } catch (const rpc::RpcFailedException &) {
    replica_stream_.reset();
    {
      std::unique_lock client_guard(client_lock_);
      replica_state_.store(replication::ReplicaState::INVALID);
    }
    HandleRpcFailure();
  }
}

void Storage::ReplicationClient::RecoverReplica(uint64_t replica_commit) {
  while (true) {
    auto file_locker = storage_->file_retainer_.AddLocker();

    const auto steps = GetRecoverySteps(replica_commit, &file_locker);
    for (const auto &recovery_step : steps) {
      try {
        std::visit(
            [&, this]<typename T>(T &&arg) {
              using StepType = std::remove_cvref_t<T>;
              if constexpr (std::is_same_v<StepType, RecoverySnapshot>) {
                DLOG(INFO) << "Sending the latest snapshot file: " << arg;
                auto response = TransferSnapshot(arg);
                replica_commit = response.current_commit_timestamp;
                DLOG(INFO) << "Current timestamp on replica: "
                           << replica_commit;
              } else if constexpr (std::is_same_v<StepType, RecoveryWals>) {
                DLOG(INFO) << "Sending the latest wal files";
                auto response = TransferWalFiles(arg);
                replica_commit = response.current_commit_timestamp;
                DLOG(INFO) << "Current timestamp on replica: "
                           << replica_commit;
              } else if constexpr (std::is_same_v<StepType,
                                                  RecoveryCurrentWal>) {
                std::unique_lock transaction_guard(storage_->engine_lock_);
                if (storage_->wal_file_ &&
                    storage_->wal_file_->SequenceNumber() ==
                        arg.current_wal_seq_num) {
                  storage_->wal_file_->DisableFlushing();
                  transaction_guard.unlock();
                  DLOG(INFO) << "Sending current wal file";
                  replica_commit = ReplicateCurrentWal();
                  DLOG(INFO)
                      << "Current timestamp on replica: " << replica_commit;
                  storage_->wal_file_->EnableFlushing();
                }
              } else if constexpr (std::is_same_v<StepType,
                                                  RecoveryFinalSnapshot>) {
                DLOG(INFO) << "Snapshot timestamp is the latest";
                auto response = TransferOnlySnapshot(arg.snapshot_timestamp);
                if (response.success) {
                  replica_commit = response.current_commit_timestamp;
                }
              } else {
                static_assert(always_false_v<T>,
                              "Missing type from variant visitor");
              }
            },
            recovery_step);
      } catch (const rpc::RpcFailedException &) {
        {
          std::unique_lock client_guard{client_lock_};
          replica_state_.store(replication::ReplicaState::INVALID);
        }
        HandleRpcFailure();
      }
    }

    // To avoid the situation where we read a correct commit timestamp in
    // one thread, and after that another thread commits a different a
    // transaction and THEN we set the state to READY in the first thread,
    // we set this lock before checking the timestamp.
    // We will detect that the state is invalid during the next commit,
    // because AppendDeltasRpc sends the last commit timestamp which
    // replica checks if it's the same last commit timestamp it received
    // and we will go to recovery.
    // By adding this lock, we can avoid that, and go to RECOVERY immediately.
    std::unique_lock client_guard{client_lock_};
    if (storage_->last_commit_timestamp_.load() == replica_commit) {
      replica_state_.store(replication::ReplicaState::READY);
      return;
    }
  }
}

uint64_t Storage::ReplicationClient::ReplicateCurrentWal() {
  auto stream = TransferCurrentWalFile();
  stream.AppendFilename(storage_->wal_file_->Path().filename());
  utils::InputFile file;
  CHECK(file.Open(storage_->wal_file_->Path()))
      << "Failed to open current WAL file!";
  const auto [buffer, buffer_size] = storage_->wal_file_->CurrentFileBuffer();
  stream.AppendSize(file.GetSize() + buffer_size);
  stream.AppendFileData(&file);
  stream.AppendBufferData(buffer, buffer_size);
  auto response = stream.Finalize();
  return response.current_commit_timestamp;
}

/// This method tries to find the optimal path for recoverying a single replica.
/// Based on the last commit transfered to replica it tries to update the
/// replica using durability files - WALs and Snapshots. WAL files are much
/// smaller in size as they contain only the Deltas (changes) made during the
/// transactions while Snapshots contain all the data. For that reason we prefer
/// WALs as much as possible. As the WAL file that is currently being updated
/// can change during the process we ignore it as much as possible. Also, it
/// uses the transaction lock so lokcing it can be really expensive. After we
/// fetch the list of finalized WALs, we try to find the longest chain of
/// sequential WALs, starting from the latest one, that will update the recovery
/// with the all missed updates. If the WAL chain cannot be created, replica is
/// behind by a lot, so we use the regular recovery process, we send the latest
/// snapshot and all the necessary WAL files, starting from the newest WAL that
/// contains a timestamp before the snapshot. If we registered the existence of
/// the current WAL, we add the sequence number we read from it to the recovery
/// process. After all the other steps are finished, if the current WAL contains
/// the same sequence number, it's the same WAL we read while fetching the
/// recovery steps, so we can safely send it to the replica. There's also one
/// edge case, if MAIN instance restarted and the snapshot contained the last
/// change (creation of that snapshot) the latest timestamp is contained in it.
/// As no changes were made to the data, we only need to send the timestamp of
/// the snapshot so replica can set its last timestamp to that value.
std::vector<Storage::ReplicationClient::RecoveryStep>
Storage::ReplicationClient::GetRecoverySteps(
    const uint64_t replica_commit,
    utils::FileRetainer::FileLocker *file_locker) {
  // First check if we can recover using the current wal file only
  // otherwise save the seq_num of the current wal file
  // This lock is also necessary to force the missed transaction to finish.
  std::optional<uint64_t> current_wal_seq_num;
  if (std::unique_lock transtacion_guard(storage_->engine_lock_);
      storage_->wal_file_) {
    current_wal_seq_num.emplace(storage_->wal_file_->SequenceNumber());
  }

  auto locker_acc = file_locker->Access();
  auto wal_files = durability::GetWalFiles(
      storage_->wal_directory_, storage_->uuid_, current_wal_seq_num);
  CHECK(wal_files) << "Wal files could not be loaded";

  auto snapshot_files = durability::GetSnapshotFiles(
      storage_->snapshot_directory_, storage_->uuid_);
  std::optional<durability::SnapshotDurabilityInfo> latest_snapshot;
  if (!snapshot_files.empty()) {
    std::sort(snapshot_files.begin(), snapshot_files.end());
    latest_snapshot.emplace(std::move(snapshot_files.back()));
  }

  std::vector<RecoveryStep> recovery_steps;

  // No finalized WAL files were found. This means the difference is contained
  // inside the current WAL or the snapshot was loaded back without any WALs
  // after.
  if (wal_files->empty()) {
    if (current_wal_seq_num) {
      recovery_steps.emplace_back(RecoveryCurrentWal{*current_wal_seq_num});
    } else {
      CHECK(latest_snapshot);
      locker_acc.AddFile(latest_snapshot->path);
      recovery_steps.emplace_back(
          RecoveryFinalSnapshot{latest_snapshot->start_timestamp});
    }
    return recovery_steps;
  }

  // Find the longest chain of WALs for recovery.
  // The chain consists ONLY of sequential WALs.
  auto rwal_it = wal_files->rbegin();

  // if the last finalized WAL is before the replica commit
  // then we can recovery only from current WAL or from snapshot
  // if the main just recovered
  if (rwal_it->to_timestamp <= replica_commit) {
    if (current_wal_seq_num) {
      recovery_steps.emplace_back(RecoveryCurrentWal{*current_wal_seq_num});
    } else {
      CHECK(latest_snapshot);
      locker_acc.AddFile(latest_snapshot->path);
      recovery_steps.emplace_back(
          RecoveryFinalSnapshot{latest_snapshot->start_timestamp});
    }
    return recovery_steps;
  }

  uint64_t previous_seq_num{rwal_it->seq_num};
  for (; rwal_it != wal_files->rend(); ++rwal_it) {
    // If the difference between two consecutive wal files is not 0 or 1
    // we have a missing WAL in our chain
    if (previous_seq_num - rwal_it->seq_num > 1) {
      break;
    }

    // Find first WAL that contains up to replica commit, i.e. WAL
    // that is before the replica commit or conatins the replica commit
    // as the last committed transaction OR we managed to find the first WAL
    // file.
    if (replica_commit >= rwal_it->from_timestamp || rwal_it->seq_num == 0) {
      if (replica_commit >= rwal_it->to_timestamp) {
        // We want the WAL after because the replica already contains all the
        // commits from this WAL
        --rwal_it;
      }
      std::vector<std::filesystem::path> wal_chain;
      auto distance_from_first = std::distance(rwal_it, wal_files->rend() - 1);
      // We have managed to create WAL chain
      // We need to lock these files and add them to the chain
      for (auto result_wal_it = wal_files->begin() + distance_from_first;
           result_wal_it != wal_files->end(); ++result_wal_it) {
        locker_acc.AddFile(result_wal_it->path);
        wal_chain.push_back(std::move(result_wal_it->path));
      }

      recovery_steps.emplace_back(std::in_place_type_t<RecoveryWals>{},
                                  std::move(wal_chain));

      if (current_wal_seq_num) {
        recovery_steps.emplace_back(RecoveryCurrentWal{*current_wal_seq_num});
      }
      return recovery_steps;
    }

    previous_seq_num = rwal_it->seq_num;
  }

  CHECK(latest_snapshot) << "Invalid durability state, missing snapshot";
  // We didn't manage to find a WAL chain, we need to send the latest snapshot
  // with its WALs
  locker_acc.AddFile(latest_snapshot->path);
  recovery_steps.emplace_back(std::in_place_type_t<RecoverySnapshot>{},
                              std::move(latest_snapshot->path));

  std::vector<std::filesystem::path> recovery_wal_files;
  auto wal_it = wal_files->begin();
  for (; wal_it != wal_files->end(); ++wal_it) {
    // Assuming recovery process is correct the snashpot should
    // always retain a single WAL that contains a transaction
    // before its creation
    if (latest_snapshot->start_timestamp < wal_it->to_timestamp) {
      if (latest_snapshot->start_timestamp < wal_it->from_timestamp) {
        CHECK(wal_it != wal_files->begin()) << "Invalid durability files state";
        --wal_it;
      }
      break;
    }
  }

  for (; wal_it != wal_files->end(); ++wal_it) {
    locker_acc.AddFile(wal_it->path);
    recovery_wal_files.push_back(std::move(wal_it->path));
  }

  // We only have a WAL before the snapshot
  if (recovery_wal_files.empty()) {
    locker_acc.AddFile(wal_files->back().path);
    recovery_wal_files.push_back(std::move(wal_files->back().path));
  }

  recovery_steps.emplace_back(std::in_place_type_t<RecoveryWals>{},
                              std::move(recovery_wal_files));

  if (current_wal_seq_num) {
    recovery_steps.emplace_back(RecoveryCurrentWal{*current_wal_seq_num});
  }

  return recovery_steps;
}

////// TimeoutDispatcher //////
void Storage::ReplicationClient::TimeoutDispatcher::WaitForTaskToFinish() {
  // Wait for the previous timeout task to finish
  std::unique_lock main_guard(main_lock);
  main_cv.wait(main_guard, [&] { return finished; });
}

void Storage::ReplicationClient::TimeoutDispatcher::StartTimeoutTask(
    const double timeout) {
  timeout_pool.AddTask([timeout, this] {
    finished = false;
    using std::chrono::steady_clock;
    const auto timeout_duration =
        std::chrono::duration_cast<steady_clock::duration>(
            std::chrono::duration<double>(timeout));
    const auto end_time = steady_clock::now() + timeout_duration;
    while (active && (steady_clock::now() < end_time)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::unique_lock main_guard(main_lock);
    finished = true;
    active = false;
    main_cv.notify_one();
  });
}
////// ReplicaStream //////
Storage::ReplicationClient::ReplicaStream::ReplicaStream(
    ReplicationClient *self, const uint64_t previous_commit_timestamp,
    const uint64_t current_seq_num)
    : self_(self),
      stream_(self_->rpc_client_->Stream<AppendDeltasRpc>(
          previous_commit_timestamp, current_seq_num)) {
  replication::Encoder encoder{stream_.GetBuilder()};
  encoder.WriteString(self_->storage_->epoch_id_);
}

void Storage::ReplicationClient::ReplicaStream::AppendDelta(
    const Delta &delta, const Vertex &vertex, uint64_t final_commit_timestamp) {
  replication::Encoder encoder(stream_.GetBuilder());
  EncodeDelta(&encoder, &self_->storage_->name_id_mapper_,
              self_->storage_->config_.items, delta, vertex,
              final_commit_timestamp);
}

void Storage::ReplicationClient::ReplicaStream::AppendDelta(
    const Delta &delta, const Edge &edge, uint64_t final_commit_timestamp) {
  replication::Encoder encoder(stream_.GetBuilder());
  EncodeDelta(&encoder, &self_->storage_->name_id_mapper_, delta, edge,
              final_commit_timestamp);
}

void Storage::ReplicationClient::ReplicaStream::AppendTransactionEnd(
    uint64_t final_commit_timestamp) {
  replication::Encoder encoder(stream_.GetBuilder());
  EncodeTransactionEnd(&encoder, final_commit_timestamp);
}

void Storage::ReplicationClient::ReplicaStream::AppendOperation(
    durability::StorageGlobalOperation operation, LabelId label,
    const std::set<PropertyId> &properties, uint64_t timestamp) {
  replication::Encoder encoder(stream_.GetBuilder());
  EncodeOperation(&encoder, &self_->storage_->name_id_mapper_, operation, label,
                  properties, timestamp);
}

AppendDeltasRes Storage::ReplicationClient::ReplicaStream::Finalize() {
  return stream_.AwaitResponse();
}

////// CurrentWalHandler //////
Storage::ReplicationClient::CurrentWalHandler::CurrentWalHandler(
    ReplicationClient *self)
    : self_(self), stream_(self_->rpc_client_->Stream<CurrentWalRpc>()) {}

void Storage::ReplicationClient::CurrentWalHandler::AppendFilename(
    const std::string &filename) {
  replication::Encoder encoder(stream_.GetBuilder());
  encoder.WriteString(filename);
}

void Storage::ReplicationClient::CurrentWalHandler::AppendSize(
    const size_t size) {
  replication::Encoder encoder(stream_.GetBuilder());
  encoder.WriteUint(size);
}

void Storage::ReplicationClient::CurrentWalHandler::AppendFileData(
    utils::InputFile *file) {
  replication::Encoder encoder(stream_.GetBuilder());
  encoder.WriteFileData(file);
}

void Storage::ReplicationClient::CurrentWalHandler::AppendBufferData(
    const uint8_t *buffer, const size_t buffer_size) {
  replication::Encoder encoder(stream_.GetBuilder());
  encoder.WriteBuffer(buffer, buffer_size);
}

CurrentWalRes Storage::ReplicationClient::CurrentWalHandler::Finalize() {
  return stream_.AwaitResponse();
}
}  // namespace storage
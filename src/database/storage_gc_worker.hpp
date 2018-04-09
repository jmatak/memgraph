#pragma once

#include "communication/rpc/client_pool.hpp"
#include "database/storage_gc.hpp"
#include "distributed/storage_gc_rpc_messages.hpp"

#include "transactions/engine_worker.hpp"
#include "transactions/transaction.hpp"

namespace database {
class StorageGcWorker : public StorageGc {
 public:
  StorageGcWorker(Storage &storage, tx::Engine &tx_engine, int pause_sec,
                  communication::rpc::ClientPool &master_client_pool,
                  int worker_id)
      : StorageGc(storage, tx_engine, pause_sec),
        master_client_pool_(master_client_pool),
        worker_id_(worker_id) {}

  void CollectCommitLogGarbage(tx::transaction_id_t oldest_active) final {
    auto safe_to_delete = GetClogSafeTransaction(oldest_active);
    if (safe_to_delete) {
      // We first need to delete transactions that we can delete to be sure that
      // the locks are released as well. Otherwise some new transaction might
      // try to aquire a lock which hasn't been released (if the transaction
      // cache cleaner was not scheduled at this time), and take a look into the
      // commit log which no longer contains that transaction id
      dynamic_cast<tx::WorkerEngine &>(tx_engine_)
          .ClearTransactionalCache(*safe_to_delete);
      master_client_pool_.Call<distributed::RanLocalGcRpc>(*safe_to_delete,
                                                           worker_id_);
      tx_engine_.GarbageCollectCommitLog(*safe_to_delete);
    }
  }

  communication::rpc::ClientPool &master_client_pool_;
  int worker_id_;
};
}  // namespace database
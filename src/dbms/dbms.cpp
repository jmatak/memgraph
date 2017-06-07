#include "gflags/gflags.h"

#include "dbms/dbms.hpp"

DEFINE_string(SNAPSHOT_DIRECTORY, "snapshots",
              "Relative path to directory in which to save snapshots.");
DEFINE_bool(RECOVER_ON_STARTUP, false, "Recover database on startup.");

std::unique_ptr<GraphDbAccessor> Dbms::active() {
  return std::make_unique<GraphDbAccessor>(
      *active_db.load(std::memory_order_acquire));
}

std::unique_ptr<GraphDbAccessor> Dbms::active(const std::string &name,
                                              const fs::path &snapshot_db_dir) {
  auto acc = dbs.access();
  // create db if it doesn't exist
  auto it = acc.find(name);
  if (it == acc.end()) {
    it = acc.emplace(name, std::forward_as_tuple(name),
                     std::forward_as_tuple(name, snapshot_db_dir))
             .first;
  }

  // set and return active db
  auto &db = it->second;
  active_db.store(&db, std::memory_order_release);
  return std::make_unique<GraphDbAccessor>(db);
}

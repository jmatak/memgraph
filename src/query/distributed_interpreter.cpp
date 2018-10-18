#include "query/distributed_interpreter.hpp"

#include "database/distributed/distributed_graph_db.hpp"
#include "distributed/plan_dispatcher.hpp"
#include "query/plan/distributed.hpp"
#include "query/plan/planner.hpp"
#include "query/plan/rule_based_planner.hpp"
#include "query/plan/vertex_count_cache.hpp"

namespace query {

namespace {

class DistributedLogicalPlan final : public LogicalPlan {
 public:
  DistributedLogicalPlan(plan::DistributedPlan plan, double cost,
                         distributed::PlanDispatcher *plan_dispatcher)
      : plan_(std::move(plan)), plan_dispatcher_(plan_dispatcher), cost_(cost) {
    CHECK(plan_dispatcher_);
    for (const auto &plan_pair : plan_.worker_plans) {
      const auto &plan_id = plan_pair.first;
      const auto &worker_plan = plan_pair.second;
      plan_dispatcher_->DispatchPlan(plan_id, worker_plan, plan_.symbol_table);
    }
  }

  ~DistributedLogicalPlan() {
    for (const auto &plan_pair : plan_.worker_plans) {
      const auto &plan_id = plan_pair.first;
      try {
        plan_dispatcher_->RemovePlan(plan_id);
      } catch (const communication::rpc::RpcFailedException &) {
        // We ignore RPC exceptions here because the other side can be possibly
        // shutting down. TODO: If that is not the case then something is really
        // wrong with the cluster!
      }
    }
  }

  const plan::LogicalOperator &GetRoot() const override {
    return *plan_.master_plan;
  }
  double GetCost() const override { return cost_; }
  const SymbolTable &GetSymbolTable() const override {
    return plan_.symbol_table;
  }

 private:
  plan::DistributedPlan plan_;
  distributed::PlanDispatcher *plan_dispatcher_{nullptr};
  double cost_;
};

}  // namespace

DistributedInterpreter::DistributedInterpreter(database::Master *db)
    : plan_dispatcher_(&db->plan_dispatcher()) {}

std::unique_ptr<LogicalPlan> DistributedInterpreter::MakeLogicalPlan(
    Query *query, AstStorage ast_storage, Context *context) {
  auto vertex_counts = plan::MakeVertexCountCache(context->db_accessor_);
  auto planning_context = plan::MakePlanningContext(
      ast_storage, context->symbol_table_, query, vertex_counts);
  std::unique_ptr<plan::LogicalOperator> tmp_logical_plan;
  double cost;
  std::tie(tmp_logical_plan, cost) = plan::MakeLogicalPlan(
      planning_context, context->evaluation_context_.parameters,
      FLAGS_query_cost_planner);
  auto plan = MakeDistributedPlan(*tmp_logical_plan, context->symbol_table_,
                                  next_plan_id_);
  VLOG(10) << "[Interpreter] Created plan for distributed execution "
           << next_plan_id_ - 1;
  return std::make_unique<DistributedLogicalPlan>(std::move(plan), cost,
                                                  plan_dispatcher_);
}

}  // namespace query
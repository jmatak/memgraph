#pragma once

#include "gtest/gtest.h"

#include "query/context.hpp"
#include "query/frontend/ast/ast.hpp"
#include "query/interpret/frame.hpp"
#include "query/plan/operator.hpp"
#include "query_common.hpp"

namespace query {
void PrintTo(const query::EdgeAtom::Direction &dir, std::ostream *os) {
  switch (dir) {
    case query::EdgeAtom::Direction::IN:
      *os << "IN";
      break;
    case query::EdgeAtom::Direction::OUT:
      *os << "OUT";
      break;
    case query::EdgeAtom::Direction::BOTH:
      *os << "BOTH";
      break;
  }
}
}  // namespace query

#ifdef MG_SINGLE_NODE
using VertexAddress = mvcc::VersionList<Vertex> *;
using EdgeAddress = mvcc::VersionList<Edge> *;
#endif
#ifdef MG_DISTRIBUTED
using VertexAddress = storage::Address<mvcc::VersionList<Vertex>>;
using EdgeAddress = storage::Address<mvcc::VersionList<Edge>>;
#endif

const auto kVertexCount = 6;
// Maps vertices to workers
const std::vector<int> kVertexLocations = {0, 1, 1, 0, 2, 2};
// Edge list in form of (from, to, edge_type).
const std::vector<std::tuple<int, int, std::string>> kEdges = {
    {0, 1, "a"}, {1, 2, "b"}, {2, 4, "b"}, {2, 5, "a"}, {4, 1, "a"},
    {4, 5, "a"}, {5, 3, "b"}, {5, 4, "a"}, {5, 5, "b"}};

// Filters input edge list by edge type and direction and returns a list of
// pairs representing valid directed edges.
std::vector<std::pair<int, int>> GetEdgeList(
    const std::vector<std::tuple<int, int, std::string>> &edges,
    query::EdgeAtom::Direction dir,
    const std::vector<std::string> &edge_types) {
  std::vector<std::pair<int, int>> ret;
  for (const auto &e : edges) {
    if (edge_types.empty() || utils::Contains(edge_types, std::get<2>(e)))
      ret.emplace_back(std::get<0>(e), std::get<1>(e));
  }
  switch (dir) {
    case query::EdgeAtom::Direction::OUT:
      break;
    case query::EdgeAtom::Direction::IN:
      for (auto &e : ret) std::swap(e.first, e.second);
      break;
    case query::EdgeAtom::Direction::BOTH:
      std::transform(
          ret.begin(), ret.end(), std::back_inserter(ret),
          [](const auto &e) { return std::make_pair(e.second, e.first); });
      break;
  }
  return ret;
}

// Floyd-Warshall algorithm. Given a graph, returns its distance matrix. If
// there is no path between two vertices, corresponding matrix entry will be
// -1.
std::vector<std::vector<int>> FloydWarshall(
    int num_vertices, const std::vector<std::pair<int, int>> &edges) {
  int inf = std::numeric_limits<int>::max();
  std::vector<std::vector<int>> dist(num_vertices,
                                     std::vector<int>(num_vertices, inf));

  for (const auto &e : edges) dist[e.first][e.second] = 1;
  for (int i = 0; i < num_vertices; ++i) dist[i][i] = 0;

  for (int k = 0; k < num_vertices; ++k) {
    for (int i = 0; i < num_vertices; ++i) {
      for (int j = 0; j < num_vertices; ++j) {
        if (dist[i][k] == inf || dist[k][j] == inf) continue;
        dist[i][j] = std::min(dist[i][j], dist[i][k] + dist[k][j]);
      }
    }
  }

  for (int i = 0; i < num_vertices; ++i)
    for (int j = 0; j < num_vertices; ++j)
      if (dist[i][j] == inf) dist[i][j] = -1;

  return dist;
}

class Yield : public query::plan::LogicalOperator {
 public:
  Yield(const std::shared_ptr<query::plan::LogicalOperator> &input,
        const std::vector<query::Symbol> &modified_symbols,
        const std::vector<std::vector<query::TypedValue>> &values)
      : input_(input ? input : std::make_shared<query::plan::Once>()),
        modified_symbols_(modified_symbols),
        values_(values) {}

  std::unique_ptr<query::plan::Cursor> MakeCursor(
      database::GraphDbAccessor &dba) const override {
    return std::make_unique<YieldCursor>(this, input_->MakeCursor(dba));
  }
  std::vector<query::Symbol> ModifiedSymbols(
      const query::SymbolTable &) const override {
    return modified_symbols_;
  }
  bool HasSingleInput() const override { return true; }
  std::shared_ptr<query::plan::LogicalOperator> input() const override {
    return input_;
  }
  void set_input(std::shared_ptr<query::plan::LogicalOperator> input) override {
    input_ = input;
  }
  bool Accept(query::plan::HierarchicalLogicalOperatorVisitor &) override {
    LOG(FATAL) << "Please go away, visitor!";
  }

  std::shared_ptr<query::plan::LogicalOperator> input_;
  std::vector<query::Symbol> modified_symbols_;
  std::vector<std::vector<query::TypedValue>> values_;

  class YieldCursor : public query::plan::Cursor {
   public:
    YieldCursor(const Yield *self,
                std::unique_ptr<query::plan::Cursor> input_cursor)
        : self_(self),
          input_cursor_(std::move(input_cursor)),
          pull_index_(self_->values_.size()) {}
    bool Pull(query::Frame &frame, query::Context &context) override {
      if (pull_index_ == self_->values_.size()) {
        if (!input_cursor_->Pull(frame, context)) return false;
        pull_index_ = 0;
      }
      for (size_t i = 0; i < self_->values_[pull_index_].size(); ++i) {
        frame[self_->modified_symbols_[i]] = self_->values_[pull_index_][i];
      }
      pull_index_++;
      return true;
    }
    void Reset() override {
      input_cursor_->Reset();
      pull_index_ = self_->values_.size();
    }

    void Shutdown() override {}
   private:
    const Yield *self_;
    std::unique_ptr<query::plan::Cursor> input_cursor_;
    size_t pull_index_;
  };
};

std::vector<std::vector<query::TypedValue>> PullResults(
    query::plan::LogicalOperator *last_op, query::Context *context,
    std::vector<query::Symbol> output_symbols) {
  auto cursor = last_op->MakeCursor(context->db_accessor_);
  std::vector<std::vector<query::TypedValue>> output;
  {
    query::Frame frame(context->symbol_table_.max_position());
    while (cursor->Pull(frame, *context)) {
      output.emplace_back();
      for (const auto &symbol : output_symbols) {
        output.back().push_back(frame[symbol]);
      }
    }
  }
  return output;
}

/* Various types of lambdas.
 * NONE           - No filter lambda used.
 * USE_FRAME      - Block a single edge or vertex. Tests if frame is sent over
 *                  the network properly in distributed BFS.
 * USE_FRAME_NULL - Block a single node or vertex, but lambda returns null
 *                  instead of false.
 * USE_CTX -        Block a vertex by checking if its ID is equal to a
 *                  parameter. Tests if evaluation context is sent over the
 *                  network properly in distributed BFS.
 * ERROR -          Lambda that evaluates to an integer instead of null or
 *                  boolean.In distributed BFS, it will fail on worker other
 *                  than master, to test if errors are propagated correctly.
 */

enum class FilterLambdaType { NONE, USE_FRAME, USE_FRAME_NULL, USE_CTX, ERROR };

// Common interface for single-node and distributed Memgraph.
class Database {
 public:
  virtual std::unique_ptr<database::GraphDbAccessor> Access() = 0;
  virtual void AdvanceCommand(tx::TransactionId tx_id) = 0;
  virtual std::unique_ptr<query::plan::LogicalOperator> MakeBfsOperator(
      query::Symbol source_sym, query::Symbol sink_sym, query::Symbol edge_sym,
      query::EdgeAtom::Direction direction,
      const std::vector<storage::EdgeType> &edge_types,
      const std::shared_ptr<query::plan::LogicalOperator> &input,
      bool existing_node, query::Expression *lower_bound,
      query::Expression *upper_bound,
      const query::plan::ExpansionLambda &filter_lambda) = 0;
  virtual std::pair<std::vector<VertexAddress>,
                    std::vector<EdgeAddress>>
  BuildGraph(database::GraphDbAccessor *dba,
             const std::vector<int> &vertex_locations,
             const std::vector<std::tuple<int, int, std::string>> &edges) = 0;

  virtual ~Database() {}
};

// Returns an operator that yields vertices given by their address. We will also
// include query::TypedValue::Null to account for the optional match case.
std::unique_ptr<query::plan::LogicalOperator> YieldVertices(
    database::GraphDbAccessor *dba,
    std::vector<VertexAddress> vertices, query::Symbol symbol,
    std::shared_ptr<query::plan::LogicalOperator> input_op) {
  std::vector<std::vector<query::TypedValue>> frames;
  frames.push_back(std::vector<query::TypedValue>{query::TypedValue::Null});
  for (const auto &vertex : vertices) {
    frames.push_back(
        std::vector<query::TypedValue>{VertexAccessor(vertex, *dba)});
  }
  return std::make_unique<Yield>(input_op, std::vector<query::Symbol>{symbol},
                                 frames);
}

// Returns an operator that yields edges and vertices given by their address.
std::unique_ptr<query::plan::LogicalOperator> YieldEntities(
    database::GraphDbAccessor *dba,
    std::vector<VertexAddress> vertices,
    std::vector<EdgeAddress> edges, query::Symbol symbol,
    std::shared_ptr<query::plan::LogicalOperator> input_op) {
  std::vector<std::vector<query::TypedValue>> frames;
  for (const auto &vertex : vertices) {
    frames.push_back(
        std::vector<query::TypedValue>{VertexAccessor(vertex, *dba)});
  }
  for (const auto &edge : edges) {
    frames.push_back(std::vector<query::TypedValue>{EdgeAccessor(edge, *dba)});
  }
  return std::make_unique<Yield>(input_op, std::vector<query::Symbol>{symbol},
                                 frames);
}

template <class TRecord>
auto GetProp(const RecordAccessor<TRecord> &rec, std::string prop,
             database::GraphDbAccessor *dba) {
  return rec.PropsAt(dba->Property(prop));
}

// Checks if the given path is actually a path from source to sink and if all
// of its edges exist in the given edge list.
void CheckPath(database::GraphDbAccessor *dba, const VertexAccessor &source,
               const VertexAccessor &sink,
               const std::vector<query::TypedValue> &path,
               const std::vector<std::pair<int, int>> &edges) {
  VertexAccessor curr = source;
  for (const auto &edge_tv : path) {
    ASSERT_TRUE(edge_tv.IsEdge());
    auto edge = edge_tv.ValueEdge();

    ASSERT_TRUE(edge.from() == curr || edge.to() == curr);
    auto next = edge.from_is(curr) ? edge.to() : edge.from();

    int from = GetProp(curr, "id", dba).Value<int64_t>();
    int to = GetProp(next, "id", dba).Value<int64_t>();
    ASSERT_TRUE(utils::Contains(edges, std::make_pair(from, to)));

    curr = next;
  }
  ASSERT_EQ(curr, sink);
}

// Given a list of BFS results of form (from, to, path, blocked entity),
// checks if all paths are valid and returns the distance matrix.
std::vector<std::vector<int>> CheckPathsAndExtractDistances(
    database::GraphDbAccessor *dba,
    const std::vector<std::pair<int, int>> edges,
    const std::vector<std::vector<query::TypedValue>> &results) {
  std::vector<std::vector<int>> distances(kVertexCount,
                                          std::vector<int>(kVertexCount, -1));

  for (size_t i = 0; i < kVertexCount; ++i) distances[i][i] = 0;

  for (const auto &row : results) {
    auto source = GetProp(row[0].ValueVertex(), "id", dba).Value<int64_t>();
    auto sink = GetProp(row[1].ValueVertex(), "id", dba).Value<int64_t>();
    distances[source][sink] = row[2].ValueList().size();
    CheckPath(dba, row[0].ValueVertex(), row[1].ValueVertex(),
              row[2].ValueList(), edges);
  }
  return distances;
}

void BfsTest(Database *db, int lower_bound, int upper_bound,
             query::EdgeAtom::Direction direction,
             std::vector<std::string> edge_types, bool known_sink,
             FilterLambdaType filter_lambda_type) {
  auto dba_ptr = db->Access();
  auto &dba = *dba_ptr;
  query::AstStorage storage;
  query::Context context(*dba_ptr);
  query::Symbol blocked_sym =
      context.symbol_table_.CreateSymbol("blocked", true);
  query::Symbol source_sym = context.symbol_table_.CreateSymbol("source", true);
  query::Symbol sink_sym = context.symbol_table_.CreateSymbol("sink", true);
  query::Symbol edges_sym = context.symbol_table_.CreateSymbol("edges", true);
  query::Symbol inner_node_sym =
      context.symbol_table_.CreateSymbol("inner_node", true);
  query::Symbol inner_edge_sym =
      context.symbol_table_.CreateSymbol("inner_edge", true);
  query::Identifier *blocked = IDENT("blocked");
  query::Identifier *inner_node = IDENT("inner_node");
  query::Identifier *inner_edge = IDENT("inner_edge");
  context.symbol_table_[*blocked] = blocked_sym;
  context.symbol_table_[*inner_node] = inner_node_sym;
  context.symbol_table_[*inner_edge] = inner_edge_sym;

  std::vector<VertexAddress> vertices;
  std::vector<EdgeAddress> edges;

  std::tie(vertices, edges) =
      db->BuildGraph(dba_ptr.get(), kVertexLocations, kEdges);

  db->AdvanceCommand(dba_ptr->transaction_id());

  std::shared_ptr<query::plan::LogicalOperator> input_op;

  query::Expression *filter_expr;

  // First build a filter lambda and an operator yielding blocked entities.
  switch (filter_lambda_type) {
    case FilterLambdaType::NONE:
      // No filter lambda, nothing is ever blocked.
      input_op = std::make_shared<Yield>(
          nullptr, std::vector<query::Symbol>{blocked_sym},
          std::vector<std::vector<query::TypedValue>>{
              {query::TypedValue::Null}});
      filter_expr = nullptr;
      break;
    case FilterLambdaType::USE_FRAME:
      // We block each entity in the graph and run BFS.
      input_op =
          YieldEntities(dba_ptr.get(), vertices, edges, blocked_sym, nullptr);
      filter_expr = AND(NEQ(inner_node, blocked), NEQ(inner_edge, blocked));
      break;
    case FilterLambdaType::USE_FRAME_NULL:
      // We block each entity in the graph and run BFS.
      input_op =
          YieldEntities(dba_ptr.get(), vertices, edges, blocked_sym, nullptr);
      filter_expr = IF(AND(NEQ(inner_node, blocked), NEQ(inner_edge, blocked)),
                       LITERAL(true), LITERAL(PropertyValue::Null));
      break;
    case FilterLambdaType::USE_CTX:
      // We only block vertex #5 and run BFS.
      input_op = std::make_shared<Yield>(
          nullptr, std::vector<query::Symbol>{blocked_sym},
          std::vector<std::vector<query::TypedValue>>{
              {VertexAccessor(vertices[5], *dba_ptr)}});
      filter_expr = NEQ(PROPERTY_LOOKUP(inner_node, PROPERTY_PAIR("id")),
                        PARAMETER_LOOKUP(0));
      context.evaluation_context_.parameters.Add(0, 5);
      break;
    case FilterLambdaType::ERROR:
      // Evaluate to 42 for vertex #5 which is on worker 1.
      filter_expr =
          IF(EQ(PROPERTY_LOOKUP(inner_node, PROPERTY_PAIR("id")), LITERAL(5)),
             LITERAL(42), LITERAL(true));
  }

  // We run BFS once from each vertex for each blocked entity.
  input_op = YieldVertices(dba_ptr.get(), vertices, source_sym, input_op);

  // If the sink is known, we run BFS for all posible combinations of source,
  // sink and blocked entity.
  if (known_sink) {
    input_op = YieldVertices(dba_ptr.get(), vertices, sink_sym, input_op);
  }

  std::vector<storage::EdgeType> storage_edge_types;
  for (const auto &t : edge_types) {
    storage_edge_types.push_back(dba_ptr->EdgeType(t));
  }

  input_op = db->MakeBfsOperator(
      source_sym, sink_sym, edges_sym, direction, storage_edge_types, input_op,
      known_sink, lower_bound == -1 ? nullptr : LITERAL(lower_bound),
      upper_bound == -1 ? nullptr : LITERAL(upper_bound),
      query::plan::ExpansionLambda{inner_edge_sym, inner_node_sym,
                                   filter_expr});

  std::vector<std::vector<query::TypedValue>> results;

  // An exception should be thrown on one of the pulls.
  if (filter_lambda_type == FilterLambdaType::ERROR) {
    EXPECT_THROW(PullResults(input_op.get(), &context,
                             std::vector<query::Symbol>{
                                 source_sym, sink_sym, edges_sym, blocked_sym}),
                 query::QueryRuntimeException);
    return;
  }

  results = PullResults(
      input_op.get(), &context,
      std::vector<query::Symbol>{source_sym, sink_sym, edges_sym, blocked_sym});

  // Group results based on blocked entity and compare them to results
  // obtained by running Floyd-Warshall.
  for (size_t i = 0; i < results.size();) {
    int j = i;
    auto blocked = results[j][3];
    while (j < results.size() &&
           query::TypedValue::BoolEqual{}(results[j][3], blocked))
      ++j;

    SCOPED_TRACE(fmt::format("blocked entity = {}", blocked));

    // When an edge is blocked, it is blocked in both directions so we remove
    // it before modifying edge list to account for direction and edge types;
    auto edges = kEdges;
    if (blocked.IsEdge()) {
      int from =
          GetProp(blocked.ValueEdge(), "from", dba_ptr.get()).Value<int64_t>();
      int to =
          GetProp(blocked.ValueEdge(), "to", dba_ptr.get()).Value<int64_t>();
      edges.erase(std::remove_if(edges.begin(), edges.end(),
                                 [from, to](const auto &e) {
                                   return std::get<0>(e) == from &&
                                          std::get<1>(e) == to;
                                 }),
                  edges.end());
    }

    // Now add edges in opposite direction if necessary.
    auto edges_blocked = GetEdgeList(edges, direction, edge_types);

    // When a vertex is blocked, we remove all edges that lead into it.
    if (blocked.IsVertex()) {
      int id =
          GetProp(blocked.ValueVertex(), "id", dba_ptr.get()).Value<int64_t>();
      edges_blocked.erase(
          std::remove_if(edges_blocked.begin(), edges_blocked.end(),
                         [id](const auto &e) { return e.second == id; }),
          edges_blocked.end());
    }

    auto correct_with_bounds = FloydWarshall(kVertexCount, edges_blocked);

    if (lower_bound == -1) lower_bound = 0;
    if (upper_bound == -1) upper_bound = kVertexCount;

    // Remove paths whose length doesn't satisfy given length bounds.
    for (int a = 0; a < kVertexCount; ++a) {
      for (int b = 0; b < kVertexCount; ++b) {
        if (a != b && (correct_with_bounds[a][b] < lower_bound ||
                       correct_with_bounds[a][b] > upper_bound))
          correct_with_bounds[a][b] = -1;
      }
    }

    int num_results = 0;
    for (int a = 0; a < kVertexCount; ++a)
      for (int b = 0; b < kVertexCount; ++b)
        if (a != b && correct_with_bounds[a][b] != -1) {
          ++num_results;
        }
    // There should be exactly 1 successful pull for each existing path.
    EXPECT_EQ(j - i, num_results);

    auto distances = CheckPathsAndExtractDistances(
        dba_ptr.get(), edges_blocked,
        std::vector<std::vector<query::TypedValue>>(results.begin() + i,
                                                    results.begin() + j));

    // The distances should also match.
    EXPECT_EQ(distances, correct_with_bounds);

    i = j;
  }

  dba_ptr->Abort();
}
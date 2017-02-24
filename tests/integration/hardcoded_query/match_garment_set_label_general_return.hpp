#include <iostream>
#include <string>

#include "query/plan_interface.hpp"
#include "storage/edge_accessor.hpp"
#include "storage/vertex_accessor.hpp"
#include "using.hpp"

using std::cout;
using std::endl;

// General query type: MATCH (g:garment {garment_id: 1234}) SET g:'GENERAL'
// RETURN g

bool run_general_query(GraphDbAccessor &db_accessor,
                       const TypedValueStore<> &args, Stream &stream,
                       const std::string &general_label) {
  stream.write_field("g");
  for (auto vertex : db_accessor.vertices()) {
    if (vertex.has_label(db_accessor.label("garment"))) {
      auto prop = vertex.PropsAt(db_accessor.property("garment_id"));
      if (prop.type_ == TypedValue::Type::Null) continue;
      auto cmp = prop == args.at(0);
      if (cmp.type_ != TypedValue::Type::Bool) continue;
      if (cmp.Value<bool>() != true) continue;
      vertex.add_label(db_accessor.label(general_label));
      stream.write_vertex_record(vertex);
    }
  }
  stream.write_meta("rw");
  db_accessor.transaction_.commit();
  return true;
}

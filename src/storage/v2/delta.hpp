#pragma once

#include <atomic>

#include <glog/logging.h>

#include "storage/v2/property_value.hpp"

namespace storage {

// Forward declarations because we only store pointers here.
struct Vertex;
struct Edge;
struct Delta;

// This class stores one of three pointers (`Delta`, `Vertex` and `Edge`)
// without using additional memory for storing the type. The type is stored in
// the pointer itself in the lower bits. All of those structures contain large
// items in themselves (e.g. `uint64_t`) that require the pointer to be aligned
// to their size (for `uint64_t` it is 8). That means that the pointer will
// always be a multiple of 8 which implies that the lower 3 bits of the pointer
// will always be 0. We can use those 3 bits to store information about the type
// of the pointer stored (2 bits).
class PreviousPtr {
 private:
  static constexpr uintptr_t kDelta = 0b01UL;
  static constexpr uintptr_t kVertex = 0b10UL;
  static constexpr uintptr_t kEdge = 0b11UL;

  static constexpr uintptr_t kMask = 0b11UL;

 public:
  enum class Type {
    DELTA,
    VERTEX,
    EDGE,
  };

  void Set(Delta *delta) {
    uintptr_t value = reinterpret_cast<uintptr_t>(delta);
    CHECK((value & kMask) == 0) << "Invalid pointer!";
    storage_ = value | kDelta;
  }

  void Set(Vertex *vertex) {
    uintptr_t value = reinterpret_cast<uintptr_t>(vertex);
    CHECK((value & kMask) == 0) << "Invalid pointer!";
    storage_ = value | kVertex;
  }

  void Set(Edge *edge) {
    uintptr_t value = reinterpret_cast<uintptr_t>(edge);
    CHECK((value & kMask) == 0) << "Invalid pointer!";
    storage_ = value | kEdge;
  }

  Type GetType() const {
    uintptr_t type = storage_ & kMask;
    if (type == kDelta) {
      return Type::DELTA;
    } else if (type == kVertex) {
      return Type::VERTEX;
    } else if (type == kEdge) {
      return Type::EDGE;
    } else {
      LOG(FATAL) << "Invalid pointer type!";
    }
  }

  Delta *GetDelta() const {
    CHECK((storage_ & kMask) == kDelta) << "Can't convert pointer to delta!";
    return reinterpret_cast<Delta *>(storage_ & ~kMask);
  }

  Vertex *GetVertex() const {
    CHECK((storage_ & kMask) == kVertex) << "Can't convert pointer to vertex!";
    return reinterpret_cast<Vertex *>(storage_ & ~kMask);
  }

  Edge *GetEdge() const {
    CHECK((storage_ & kMask) == kEdge) << "Can't convert pointer to edge!";
    return reinterpret_cast<Edge *>(storage_ & ~kMask);
  }

 private:
  uintptr_t storage_{0};
};

struct Delta {
  enum class Action {
    // Used for both Vertex and Edge
    DELETE_OBJECT,
    RECREATE_OBJECT,
    SET_PROPERTY,

    // Used only for Vertex
    ADD_LABEL,
    REMOVE_LABEL,
    ADD_IN_EDGE,
    ADD_OUT_EDGE,
    REMOVE_IN_EDGE,
    REMOVE_OUT_EDGE,
  };

  // Used for both Vertex and Edge
  struct DeleteObjectTag {};
  struct RecreateObjectTag {};
  struct AddLabelTag {};
  struct RemoveLabelTag {};
  struct SetPropertyTag {};

  // Used only for Vertex
  struct AddInEdgeTag {};
  struct AddOutEdgeTag {};
  struct RemoveInEdgeTag {};
  struct RemoveOutEdgeTag {};

  Delta(DeleteObjectTag, std::atomic<uint64_t> *timestamp, uint64_t command_id)
      : action(Action::DELETE_OBJECT),
        timestamp(timestamp),
        command_id(command_id) {}

  Delta(RecreateObjectTag, std::atomic<uint64_t> *timestamp,
        uint64_t command_id)
      : action(Action::RECREATE_OBJECT),
        timestamp(timestamp),
        command_id(command_id) {}

  Delta(AddLabelTag, uint64_t label, std::atomic<uint64_t> *timestamp,
        uint64_t command_id)
      : action(Action::ADD_LABEL),
        timestamp(timestamp),
        command_id(command_id),
        label(label) {}

  Delta(RemoveLabelTag, uint64_t label, std::atomic<uint64_t> *timestamp,
        uint64_t command_id)
      : action(Action::REMOVE_LABEL),
        timestamp(timestamp),
        command_id(command_id),
        label(label) {}

  Delta(SetPropertyTag, uint64_t key, const PropertyValue &value,
        std::atomic<uint64_t> *timestamp, uint64_t command_id)
      : action(Action::SET_PROPERTY),
        timestamp(timestamp),
        command_id(command_id),
        property({key, value}) {}

  Delta(AddInEdgeTag, uint64_t edge_type, Vertex *vertex, Edge *edge,
        std::atomic<uint64_t> *timestamp, uint64_t command_id)
      : action(Action::ADD_IN_EDGE),
        timestamp(timestamp),
        command_id(command_id),
        vertex_edge({edge_type, vertex, edge}) {}

  Delta(AddOutEdgeTag, uint64_t edge_type, Vertex *vertex, Edge *edge,
        std::atomic<uint64_t> *timestamp, uint64_t command_id)
      : action(Action::ADD_OUT_EDGE),
        timestamp(timestamp),
        command_id(command_id),
        vertex_edge({edge_type, vertex, edge}) {}

  Delta(RemoveInEdgeTag, uint64_t edge_type, Vertex *vertex, Edge *edge,
        std::atomic<uint64_t> *timestamp, uint64_t command_id)
      : action(Action::REMOVE_IN_EDGE),
        timestamp(timestamp),
        command_id(command_id),
        vertex_edge({edge_type, vertex, edge}) {}

  Delta(RemoveOutEdgeTag, uint64_t edge_type, Vertex *vertex, Edge *edge,
        std::atomic<uint64_t> *timestamp, uint64_t command_id)
      : action(Action::REMOVE_OUT_EDGE),
        timestamp(timestamp),
        command_id(command_id),
        vertex_edge({edge_type, vertex, edge}) {}

  Delta(Delta &&other) noexcept
      : action(other.action),
        timestamp(other.timestamp),
        command_id(other.command_id),
        prev(other.prev),
        next(other.next.load()) {
    switch (other.action) {
      case Action::DELETE_OBJECT:
      case Action::RECREATE_OBJECT:
        break;
      case Action::ADD_LABEL:
      case Action::REMOVE_LABEL:
        label = other.label;
        break;
      case Action::SET_PROPERTY:
        property.key = other.property.key;
        new (&property.value) PropertyValue(std::move(other.property.value));
        break;
      case Action::ADD_IN_EDGE:
      case Action::ADD_OUT_EDGE:
      case Action::REMOVE_IN_EDGE:
      case Action::REMOVE_OUT_EDGE:
        vertex_edge = other.vertex_edge;
        break;
    }

    // reset the action of other
    other.DestroyValue();
    other.action = Action::DELETE_OBJECT;
  }

  Delta(const Delta &) = delete;
  Delta &operator=(const Delta &) = delete;
  Delta &operator=(Delta &&other) = delete;

  ~Delta() { DestroyValue(); }

  Action action;

  // TODO: optimize with in-place copy
  std::atomic<uint64_t> *timestamp;
  uint64_t command_id;
  PreviousPtr prev;
  std::atomic<Delta *> next{nullptr};

  union {
    uint64_t label;
    struct {
      uint64_t key;
      storage::PropertyValue value;
    } property;
    struct {
      uint64_t edge_type;
      Vertex *vertex;
      Edge *edge;
    } vertex_edge;
  };

 private:
  void DestroyValue() {
    switch (action) {
      case Action::DELETE_OBJECT:
      case Action::RECREATE_OBJECT:
      case Action::ADD_LABEL:
      case Action::REMOVE_LABEL:
      case Action::ADD_IN_EDGE:
      case Action::ADD_OUT_EDGE:
      case Action::REMOVE_IN_EDGE:
      case Action::REMOVE_OUT_EDGE:
        break;
      case Action::SET_PROPERTY:
        property.value.~PropertyValue();
        break;
    }
  }
};

static_assert(alignof(Delta) >= 8,
              "The Delta should be aligned to at least 8!");

}  // namespace storage

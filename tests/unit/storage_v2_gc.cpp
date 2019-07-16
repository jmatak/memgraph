#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "storage/v2/storage.hpp"

using testing::UnorderedElementsAre;

// TODO: We should implement a more sophisticated stress test to verify that GC
// is working properly in a multithreaded environment.

// A simple test trying to get GC to run while a transaction is still alive and
// then verify that GC didn't delete anything it shouldn't have.
// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(StorageV2Gc, Sanity) {
  storage::Storage storage(
      storage::StorageGcConfig{.type = storage::StorageGcConfig::Type::PERIODIC,
                               .interval = std::chrono::milliseconds(100)});

  std::vector<storage::Gid> vertices;

  {
    auto acc = storage.Access();
    // Create some vertices, but delete some of them immediately.
    for (uint64_t i = 0; i < 1000; ++i) {
      auto vertex = acc.CreateVertex();
      vertices.push_back(vertex.Gid());
    }

    acc.AdvanceCommand();

    for (uint64_t i = 0; i < 1000; ++i) {
      auto vertex = acc.FindVertex(vertices[i], storage::View::OLD);
      ASSERT_TRUE(vertex.has_value());
      if (i % 5 == 0) {
        acc.DeleteVertex(&vertex.value());
      }
    }

    // Wait for GC.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    for (uint64_t i = 0; i < 1000; ++i) {
      auto vertex_old = acc.FindVertex(vertices[i], storage::View::OLD);
      auto vertex_new = acc.FindVertex(vertices[i], storage::View::NEW);
      EXPECT_TRUE(vertex_old.has_value());
      EXPECT_EQ(vertex_new.has_value(), i % 5 != 0);
    }

    acc.Commit();
  }

  // Verify existing vertices and add labels to some of them.
  {
    auto acc = storage.Access();
    for (uint64_t i = 0; i < 1000; ++i) {
      auto vertex = acc.FindVertex(vertices[i], storage::View::OLD);
      EXPECT_EQ(vertex.has_value(), i % 5 != 0);

      if (vertex.has_value()) {
        vertex->AddLabel(3 * i);
        vertex->AddLabel(3 * i + 1);
        vertex->AddLabel(3 * i + 2);
      }
    }

    // Wait for GC.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Verify labels.
    for (uint64_t i = 0; i < 1000; ++i) {
      auto vertex = acc.FindVertex(vertices[i], storage::View::NEW);
      EXPECT_EQ(vertex.has_value(), i % 5 != 0);

      if (vertex.has_value()) {
        auto labels_old = vertex->Labels(storage::View::OLD);
        EXPECT_TRUE(labels_old.IsReturn());
        EXPECT_TRUE(labels_old.GetReturn().empty());

        auto labels_new = vertex->Labels(storage::View::NEW);
        EXPECT_TRUE(labels_new.IsReturn());
        EXPECT_THAT(labels_new.GetReturn(),
                    UnorderedElementsAre(3 * i, 3 * i + 1, 3 * i + 2));
      }
    }

    acc.Commit();
  }

  // Add and remove some edges.
  {
    auto acc = storage.Access();
    for (uint64_t i = 0; i < 1000; ++i) {
      auto from_vertex = acc.FindVertex(vertices[i], storage::View::OLD);
      auto to_vertex =
          acc.FindVertex(vertices[(i + 1) % 1000], storage::View::OLD);
      EXPECT_EQ(from_vertex.has_value(), i % 5 != 0);
      EXPECT_EQ(to_vertex.has_value(), (i + 1) % 5 != 0);

      if (from_vertex.has_value() && to_vertex.has_value()) {
        acc.CreateEdge(&from_vertex.value(), &to_vertex.value(), i);
      }
    }

    // Detach delete some vertices.
    for (uint64_t i = 0; i < 1000; ++i) {
      auto vertex = acc.FindVertex(vertices[i], storage::View::NEW);
      EXPECT_EQ(vertex.has_value(), i % 5 != 0);
      if (vertex.has_value()) {
        if (i % 3 == 0) {
          acc.DetachDeleteVertex(&vertex.value());
        }
      }
    }

    // Wait for GC.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Vertify edges.
    for (uint64_t i = 0; i < 1000; ++i) {
      auto vertex = acc.FindVertex(vertices[i], storage::View::NEW);
      EXPECT_EQ(vertex.has_value(), i % 5 != 0 && i % 3 != 0);
      if (vertex.has_value()) {
        auto out_edges =
            vertex->OutEdges(std::vector<uint64_t>{}, storage::View::NEW);
        if (i % 5 != 4 && i % 3 != 2) {
          EXPECT_EQ(out_edges.GetReturn().size(), 1);
          EXPECT_EQ(out_edges.GetReturn().at(0).EdgeType(), i);
        } else {
          EXPECT_TRUE(out_edges.GetReturn().empty());
        }

        auto in_edges =
            vertex->InEdges(std::vector<uint64_t>{}, storage::View::NEW);
        if (i % 5 != 1 && i % 3 != 1) {
          EXPECT_EQ(in_edges.GetReturn().size(), 1);
          EXPECT_EQ(in_edges.GetReturn().at(0).EdgeType(),
                    (i + 999) % 1000);
        } else {
          EXPECT_TRUE(in_edges.GetReturn().empty());
        }
      }
    }

    acc.Commit();
  }
}
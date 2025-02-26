// Copyright 2021 Memgraph Ltd.
//
// Use of this software is governed by the Business Source License
// included in the file licenses/BSL.txt; by using this file, you agree to be bound by the terms of the Business Source
// License, and you may not use this file except in compliance with the Business Source License.
//
// As of the Change Date specified in that file, in accordance with
// the Business Source License, use of this software will be governed
// by the Apache License, Version 2.0, included in the file
// licenses/APL.txt.

#include <gtest/gtest.h>

#include <sstream>

#include "storage/v2/property_value.hpp"
#include "storage/v2/temporal.hpp"

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, Null) {
  storage::PropertyValue pv;

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::Null);

  ASSERT_TRUE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_FALSE(pv.IsString());
  ASSERT_FALSE(pv.IsList());
  ASSERT_FALSE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueMap(), storage::PropertyValueException);

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueMap(), storage::PropertyValueException);

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "null");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "null");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, Bool) {
  storage::PropertyValue pv(false);

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::Bool);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_TRUE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_FALSE(pv.IsString());
  ASSERT_FALSE(pv.IsList());
  ASSERT_FALSE(pv.IsMap());

  ASSERT_EQ(pv.ValueBool(), false);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueMap(), storage::PropertyValueException);

  const auto &cpv = pv;

  ASSERT_EQ(cpv.ValueBool(), false);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueMap(), storage::PropertyValueException);

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "bool");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "false");
  }
  {
    storage::PropertyValue pvtrue(true);
    std::stringstream ss;
    ss << pvtrue;
    ASSERT_EQ(ss.str(), "true");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, Int) {
  storage::PropertyValue pv(123L);

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::Int);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_TRUE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_FALSE(pv.IsString());
  ASSERT_FALSE(pv.IsList());
  ASSERT_FALSE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_EQ(pv.ValueInt(), 123L);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueMap(), storage::PropertyValueException);

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_EQ(cpv.ValueInt(), 123L);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueMap(), storage::PropertyValueException);

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "int");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "123");
  }

  {
    storage::PropertyValue pvint(123);
    ASSERT_EQ(pvint.type(), storage::PropertyValue::Type::Int);
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, Double) {
  storage::PropertyValue pv(123.5);

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::Double);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_TRUE(pv.IsDouble());
  ASSERT_FALSE(pv.IsString());
  ASSERT_FALSE(pv.IsList());
  ASSERT_FALSE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_EQ(pv.ValueDouble(), 123.5);
  ASSERT_THROW(pv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueMap(), storage::PropertyValueException);

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_EQ(cpv.ValueDouble(), 123.5);
  ASSERT_THROW(cpv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueMap(), storage::PropertyValueException);

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "double");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "123.5");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, StringCopy) {
  std::string str("nandare");
  storage::PropertyValue pv(str);

  ASSERT_EQ(str, "nandare");

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::String);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_TRUE(pv.IsString());
  ASSERT_FALSE(pv.IsList());
  ASSERT_FALSE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_EQ(pv.ValueString(), "nandare");
  ASSERT_THROW(pv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueMap(), storage::PropertyValueException);

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_EQ(cpv.ValueString(), "nandare");
  ASSERT_THROW(cpv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueMap(), storage::PropertyValueException);

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "string");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "nandare");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, StringMove) {
  std::string str("nandare");
  storage::PropertyValue pv(std::move(str));

  ASSERT_EQ(str, "");

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::String);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_TRUE(pv.IsString());
  ASSERT_FALSE(pv.IsList());
  ASSERT_FALSE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_EQ(pv.ValueString(), "nandare");
  ASSERT_THROW(pv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueMap(), storage::PropertyValueException);

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_EQ(cpv.ValueString(), "nandare");
  ASSERT_THROW(cpv.ValueList(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueMap(), storage::PropertyValueException);

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "string");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "nandare");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, ListCopy) {
  std::vector<storage::PropertyValue> vec{storage::PropertyValue("nandare"), storage::PropertyValue(123)};
  storage::PropertyValue pv(vec);

  ASSERT_EQ(vec.size(), 2);
  ASSERT_EQ(vec[0].ValueString(), "nandare");
  ASSERT_EQ(vec[1].ValueInt(), 123);

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::List);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_FALSE(pv.IsString());
  ASSERT_TRUE(pv.IsList());
  ASSERT_FALSE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueString(), storage::PropertyValueException);
  {
    const auto &ret = pv.ValueList();
    ASSERT_EQ(ret.size(), 2);
    ASSERT_EQ(ret[0].ValueString(), "nandare");
    ASSERT_EQ(ret[1].ValueInt(), 123);
  }
  ASSERT_THROW(pv.ValueMap(), storage::PropertyValueException);

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueString(), storage::PropertyValueException);
  {
    const auto &ret = cpv.ValueList();
    ASSERT_EQ(ret.size(), 2);
    ASSERT_EQ(ret[0].ValueString(), "nandare");
    ASSERT_EQ(ret[1].ValueInt(), 123);
  }
  ASSERT_THROW(cpv.ValueMap(), storage::PropertyValueException);

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "list");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "[nandare, 123]");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, ListMove) {
  std::vector<storage::PropertyValue> vec{storage::PropertyValue("nandare"), storage::PropertyValue(123)};
  storage::PropertyValue pv(std::move(vec));

  ASSERT_EQ(vec.size(), 0);

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::List);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_FALSE(pv.IsString());
  ASSERT_TRUE(pv.IsList());
  ASSERT_FALSE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueString(), storage::PropertyValueException);
  {
    const auto &ret = pv.ValueList();
    ASSERT_EQ(ret.size(), 2);
    ASSERT_EQ(ret[0].ValueString(), "nandare");
    ASSERT_EQ(ret[1].ValueInt(), 123);
  }
  ASSERT_THROW(pv.ValueMap(), storage::PropertyValueException);

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueString(), storage::PropertyValueException);
  {
    const auto &ret = cpv.ValueList();
    ASSERT_EQ(ret.size(), 2);
    ASSERT_EQ(ret[0].ValueString(), "nandare");
    ASSERT_EQ(ret[1].ValueInt(), 123);
  }
  ASSERT_THROW(cpv.ValueMap(), storage::PropertyValueException);

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "list");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "[nandare, 123]");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, MapCopy) {
  std::map<std::string, storage::PropertyValue> map{{"nandare", storage::PropertyValue(123)}};
  storage::PropertyValue pv(map);

  ASSERT_EQ(map.size(), 1);
  ASSERT_EQ(map.at("nandare").ValueInt(), 123);

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::Map);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_FALSE(pv.IsString());
  ASSERT_FALSE(pv.IsList());
  ASSERT_TRUE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueList(), storage::PropertyValueException);
  {
    const auto &ret = pv.ValueMap();
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret.at("nandare").ValueInt(), 123);
  }

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueList(), storage::PropertyValueException);
  {
    const auto &ret = cpv.ValueMap();
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret.at("nandare").ValueInt(), 123);
  }

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "map");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "{nandare: 123}");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, MapMove) {
  std::map<std::string, storage::PropertyValue> map{{"nandare", storage::PropertyValue(123)}};
  storage::PropertyValue pv(std::move(map));

  ASSERT_EQ(map.size(), 0);

  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::Map);

  ASSERT_FALSE(pv.IsNull());
  ASSERT_FALSE(pv.IsBool());
  ASSERT_FALSE(pv.IsInt());
  ASSERT_FALSE(pv.IsDouble());
  ASSERT_FALSE(pv.IsString());
  ASSERT_FALSE(pv.IsList());
  ASSERT_TRUE(pv.IsMap());

  ASSERT_THROW(pv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(pv.ValueList(), storage::PropertyValueException);
  {
    const auto &ret = pv.ValueMap();
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret.at("nandare").ValueInt(), 123);
  }

  const auto &cpv = pv;

  ASSERT_THROW(cpv.ValueBool(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueInt(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueDouble(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueString(), storage::PropertyValueException);
  ASSERT_THROW(cpv.ValueList(), storage::PropertyValueException);
  {
    const auto &ret = cpv.ValueMap();
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret.at("nandare").ValueInt(), 123);
  }

  {
    std::stringstream ss;
    ss << pv.type();
    ASSERT_EQ(ss.str(), "map");
  }
  {
    std::stringstream ss;
    ss << pv;
    ASSERT_EQ(ss.str(), "{nandare: 123}");
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, CopyConstructor) {
  std::vector<storage::PropertyValue> vec{storage::PropertyValue(true), storage::PropertyValue(123)};
  std::map<std::string, storage::PropertyValue> map{{"nandare", storage::PropertyValue(false)}};
  std::vector<storage::PropertyValue> data{
      storage::PropertyValue(),
      storage::PropertyValue(true),
      storage::PropertyValue(123),
      storage::PropertyValue(123.5),
      storage::PropertyValue("nandare"),
      storage::PropertyValue(vec),
      storage::PropertyValue(map),
      storage::PropertyValue(storage::TemporalData(storage::TemporalType::Date, 23))};

  for (const auto &item : data) {
    storage::PropertyValue pv(item);
    ASSERT_EQ(pv.type(), item.type());
    switch (item.type()) {
      case storage::PropertyValue::Type::Null:
        ASSERT_TRUE(pv.IsNull());
        break;
      case storage::PropertyValue::Type::Bool:
        ASSERT_EQ(pv.ValueBool(), item.ValueBool());
        break;
      case storage::PropertyValue::Type::Int:
        ASSERT_EQ(pv.ValueInt(), item.ValueInt());
        break;
      case storage::PropertyValue::Type::Double:
        ASSERT_EQ(pv.ValueDouble(), item.ValueDouble());
        break;
      case storage::PropertyValue::Type::String:
        ASSERT_EQ(pv.ValueString(), item.ValueString());
        break;
      case storage::PropertyValue::Type::List:
        ASSERT_EQ(pv.ValueList(), item.ValueList());
        break;
      case storage::PropertyValue::Type::Map:
        ASSERT_EQ(pv.ValueMap(), item.ValueMap());
        break;
      case storage::PropertyValue::Type::TemporalData:
        ASSERT_EQ(pv.ValueTemporalData(), item.ValueTemporalData());
    }
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, MoveConstructor) {
  std::vector<storage::PropertyValue> vec{storage::PropertyValue(true), storage::PropertyValue(123)};
  std::map<std::string, storage::PropertyValue> map{{"nandare", storage::PropertyValue(false)}};
  std::vector<storage::PropertyValue> data{
      storage::PropertyValue(),
      storage::PropertyValue(true),
      storage::PropertyValue(123),
      storage::PropertyValue(123.5),
      storage::PropertyValue("nandare"),
      storage::PropertyValue(vec),
      storage::PropertyValue(map),
      storage::PropertyValue(storage::TemporalData(storage::TemporalType::Date, 23))};

  for (auto &item : data) {
    storage::PropertyValue copy(item);
    storage::PropertyValue pv(std::move(item));
    ASSERT_EQ(item.type(), storage::PropertyValue::Type::Null);
    ASSERT_EQ(pv.type(), copy.type());
    switch (copy.type()) {
      case storage::PropertyValue::Type::Null:
        ASSERT_TRUE(pv.IsNull());
        break;
      case storage::PropertyValue::Type::Bool:
        ASSERT_EQ(pv.ValueBool(), copy.ValueBool());
        break;
      case storage::PropertyValue::Type::Int:
        ASSERT_EQ(pv.ValueInt(), copy.ValueInt());
        break;
      case storage::PropertyValue::Type::Double:
        ASSERT_EQ(pv.ValueDouble(), copy.ValueDouble());
        break;
      case storage::PropertyValue::Type::String:
        ASSERT_EQ(pv.ValueString(), copy.ValueString());
        break;
      case storage::PropertyValue::Type::List:
        ASSERT_EQ(pv.ValueList(), copy.ValueList());
        break;
      case storage::PropertyValue::Type::Map:
        ASSERT_EQ(pv.ValueMap(), copy.ValueMap());
        break;
      case storage::PropertyValue::Type::TemporalData:
        ASSERT_EQ(pv.ValueTemporalData(), copy.ValueTemporalData());
        break;
    }
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, CopyAssignment) {
  std::vector<storage::PropertyValue> vec{storage::PropertyValue(true), storage::PropertyValue(123)};
  std::map<std::string, storage::PropertyValue> map{{"nandare", storage::PropertyValue(false)}};
  std::vector<storage::PropertyValue> data{
      storage::PropertyValue(),
      storage::PropertyValue(true),
      storage::PropertyValue(123),
      storage::PropertyValue(123.5),
      storage::PropertyValue("nandare"),
      storage::PropertyValue(vec),
      storage::PropertyValue(map),
      storage::PropertyValue(storage::TemporalData(storage::TemporalType::Date, 23))};

  for (const auto &item : data) {
    storage::PropertyValue pv(123);
    pv = item;
    ASSERT_EQ(pv.type(), item.type());
    switch (item.type()) {
      case storage::PropertyValue::Type::Null:
        ASSERT_TRUE(pv.IsNull());
        break;
      case storage::PropertyValue::Type::Bool:
        ASSERT_EQ(pv.ValueBool(), item.ValueBool());
        break;
      case storage::PropertyValue::Type::Int:
        ASSERT_EQ(pv.ValueInt(), item.ValueInt());
        break;
      case storage::PropertyValue::Type::Double:
        ASSERT_EQ(pv.ValueDouble(), item.ValueDouble());
        break;
      case storage::PropertyValue::Type::String:
        ASSERT_EQ(pv.ValueString(), item.ValueString());
        break;
      case storage::PropertyValue::Type::List:
        ASSERT_EQ(pv.ValueList(), item.ValueList());
        break;
      case storage::PropertyValue::Type::Map:
        ASSERT_EQ(pv.ValueMap(), item.ValueMap());
        break;
      case storage::PropertyValue::Type::TemporalData:
        ASSERT_EQ(pv.ValueTemporalData(), item.ValueTemporalData());
        break;
    }
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, MoveAssignment) {
  std::vector<storage::PropertyValue> vec{storage::PropertyValue(true), storage::PropertyValue(123)};
  std::map<std::string, storage::PropertyValue> map{{"nandare", storage::PropertyValue(false)}};
  std::vector<storage::PropertyValue> data{
      storage::PropertyValue(),
      storage::PropertyValue(true),
      storage::PropertyValue(123),
      storage::PropertyValue(123.5),
      storage::PropertyValue("nandare"),
      storage::PropertyValue(vec),
      storage::PropertyValue(map),
      storage::PropertyValue(storage::TemporalData(storage::TemporalType::Date, 23))};

  for (auto &item : data) {
    storage::PropertyValue copy(item);
    storage::PropertyValue pv(123);
    pv = std::move(item);
    ASSERT_EQ(item.type(), storage::PropertyValue::Type::Null);
    ASSERT_EQ(pv.type(), copy.type());
    switch (copy.type()) {
      case storage::PropertyValue::Type::Null:
        ASSERT_TRUE(pv.IsNull());
        break;
      case storage::PropertyValue::Type::Bool:
        ASSERT_EQ(pv.ValueBool(), copy.ValueBool());
        break;
      case storage::PropertyValue::Type::Int:
        ASSERT_EQ(pv.ValueInt(), copy.ValueInt());
        break;
      case storage::PropertyValue::Type::Double:
        ASSERT_EQ(pv.ValueDouble(), copy.ValueDouble());
        break;
      case storage::PropertyValue::Type::String:
        ASSERT_EQ(pv.ValueString(), copy.ValueString());
        break;
      case storage::PropertyValue::Type::List:
        ASSERT_EQ(pv.ValueList(), copy.ValueList());
        break;
      case storage::PropertyValue::Type::Map:
        ASSERT_EQ(pv.ValueMap(), copy.ValueMap());
        break;
      case storage::PropertyValue::Type::TemporalData:
        ASSERT_EQ(pv.ValueTemporalData(), copy.ValueTemporalData());
        break;
    }
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, CopyAssignmentSelf) {
  storage::PropertyValue pv("nandare");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
  pv = pv;
#pragma clang diagnostic pop
  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::String);
  ASSERT_EQ(pv.ValueString(), "nandare");
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, MoveAssignmentSelf) {
  storage::PropertyValue pv("nandare");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
  pv = std::move(pv);
#pragma clang diagnostic pop
  ASSERT_EQ(pv.type(), storage::PropertyValue::Type::String);
  ASSERT_EQ(pv.ValueString(), "nandare");
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, Equal) {
  std::vector<storage::PropertyValue> vec{storage::PropertyValue(true), storage::PropertyValue(123)};
  std::map<std::string, storage::PropertyValue> map{{"nandare", storage::PropertyValue(false)}};
  std::vector<storage::PropertyValue> data{storage::PropertyValue(),          storage::PropertyValue(true),
                                           storage::PropertyValue(123),       storage::PropertyValue(123.5),
                                           storage::PropertyValue("nandare"), storage::PropertyValue(vec),
                                           storage::PropertyValue(map)};
  for (const auto &item1 : data) {
    for (const auto &item2 : data) {
      if (item1.type() == item2.type()) {
        ASSERT_TRUE(item1 == item2);
      } else {
        ASSERT_FALSE(item1 == item2);
      }
    }
  }
}

// NOLINTNEXTLINE(hicpp-special-member-functions)
TEST(PropertyValue, Less) {
  std::vector<storage::PropertyValue> vec{storage::PropertyValue(true), storage::PropertyValue(123)};
  std::map<std::string, storage::PropertyValue> map{{"nandare", storage::PropertyValue(false)}};
  std::vector<storage::PropertyValue> data{storage::PropertyValue(),          storage::PropertyValue(true),
                                           storage::PropertyValue(123),       storage::PropertyValue(123.5),
                                           storage::PropertyValue("nandare"), storage::PropertyValue(vec),
                                           storage::PropertyValue(map)};
  for (size_t i = 0; i < data.size(); ++i) {
    for (size_t j = 0; j < data.size(); ++j) {
      auto item1 = data[i];
      auto item2 = data[j];
      if (i < j) {
        ASSERT_TRUE(item1 < item2);
      } else {
        ASSERT_FALSE(item1 < item2);
      }
    }
  }
}

TEST(PropertyValue, NumeralTypesComparison) {
  auto v_int = storage::PropertyValue(2);
  auto v_double = storage::PropertyValue(2.0);
  ASSERT_TRUE(v_int.IsInt());
  ASSERT_TRUE(v_double.IsDouble());
  ASSERT_TRUE(v_int == v_double);
  ASSERT_FALSE(v_int < v_double);
  ASSERT_FALSE(v_double < v_int);
}

TEST(PropertyValue, NestedNumeralTypesComparison) {
  auto v1 = storage::PropertyValue(std::vector<storage::PropertyValue>{storage::PropertyValue(1)});
  auto v2 = storage::PropertyValue(std::vector<storage::PropertyValue>{storage::PropertyValue(1.5)});
  auto v3 = storage::PropertyValue(std::vector<storage::PropertyValue>{storage::PropertyValue(2)});

  auto v1alt = storage::PropertyValue(std::vector<storage::PropertyValue>{storage::PropertyValue(1.0)});
  auto v3alt = storage::PropertyValue(std::vector<storage::PropertyValue>{storage::PropertyValue(2.0)});

  ASSERT_TRUE(v1 == v1alt);
  ASSERT_TRUE(v3 == v3alt);

  ASSERT_FALSE(v1 == v2);
  ASSERT_FALSE(v2 == v1);
  ASSERT_FALSE(v2 == v3);
  ASSERT_FALSE(v3 == v2);
  ASSERT_FALSE(v1 == v3);
  ASSERT_FALSE(v3 == v1);

  ASSERT_TRUE(v1 < v2);
  ASSERT_TRUE(v2 < v3);
  ASSERT_TRUE(v1 < v3);
  ASSERT_FALSE(v2 < v1);
  ASSERT_FALSE(v3 < v2);
  ASSERT_FALSE(v3 < v1);

  ASSERT_TRUE(v1alt < v2);
  ASSERT_TRUE(v2 < v3alt);
  ASSERT_TRUE(v1alt < v3alt);
  ASSERT_FALSE(v2 < v1alt);
  ASSERT_FALSE(v3alt < v2);
  ASSERT_FALSE(v3 < v1alt);
}

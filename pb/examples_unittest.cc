// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "pb/field_list.h"
#include "pb/integer_wrapper.h"
#include "pb/parse.h"
#include "pb/serialize.h"

namespace {

// message AudioConfig {
//   int32 sample_rate = 1;
//   int32 bit_depth = 2;
//   int32 channel_count = 3;
//   sint32 delay_adjustment_ms = 4;
//   fixed64 destination_id = 5;
// }
struct AudioConfig {
  int32_t sample_rate = 0;
  int32_t bit_depth = 0;
  int32_t channel_count = 0;
  pb::sint32_t delay_adjustment_ms = 0;
  pb::fixed64_t destination_id = 0;

  using ProtobufFields =
      pb::FieldList<pb::Field<&AudioConfig::sample_rate, 1>,
                    pb::Field<&AudioConfig::bit_depth, 2>,
                    pb::Field<&AudioConfig::channel_count, 3>,
                    pb::Field<&AudioConfig::delay_adjustment_ms, 4>,
                    pb::Field<&AudioConfig::destination_id, 5>>;
};

// message AudioOfferring {
//   repeated AudioConfig configs = 1;
// }
struct AudioOfferring {
  std::vector<AudioConfig> configs;  // Or, most any STL-like container!

  using ProtobufFields = pb::FieldList<pb::Field<&AudioOfferring::configs, 1>>;
};

// message AudioOfferringResponse {
//   enum Outcome { kSuccess = 0; kFail = 1; }
//   required Outcome result = 1;
//   optional string name = 2;
//   required int32 config_index = 3;
// }
struct AudioOfferringResponse {
  enum { kSuccess = 0, kFail = 1 } result;
  std::optional<std::string> name;
  int32_t config_index = -1;

  using ProtobufFields =
      pb::FieldList<pb::Field<&AudioOfferringResponse::result, 1>,
                    pb::Field<&AudioOfferringResponse::name, 2>,
                    pb::Field<&AudioOfferringResponse::config_index, 3>>;
};

TEST(ExamplesTest, AudioConfigurationNegotiation) {
  const AudioOfferring offer{.configs = {
                                 AudioConfig{
                                     .sample_rate = 48000,
                                     .bit_depth = 24,
                                     .channel_count = 5,
                                     .delay_adjustment_ms = -50,
                                     .destination_id = uint64_t{0xfeeddeadbeef},
                                 },
                                 AudioConfig{
                                     .sample_rate = 44100,
                                     .bit_depth = 16,
                                     .channel_count = 2,
                                     .delay_adjustment_ms = -50,
                                     .destination_id = uint64_t{0xfeeddeadbeef},
                                 },
                             }};

  const auto offer_wire_size = pb::ComputeSerializedSize(offer);
  uint8_t buffer[64];
  ASSERT_LT(0, offer_wire_size);
  ASSERT_GE(static_cast<int>(sizeof(buffer)), offer_wire_size);

  pb::Serialize(offer, buffer);

  AudioOfferring received_offer{};
  bool parse_result =
      pb::MergeFromBuffer(buffer, buffer + offer_wire_size, received_offer);
  ASSERT_TRUE(parse_result);
  ASSERT_EQ(2u, received_offer.configs.size());
  for (std::size_t i = 0; i < 2; ++i) {
    EXPECT_EQ(offer.configs[i].sample_rate,
              received_offer.configs[i].sample_rate);
    EXPECT_EQ(offer.configs[i].bit_depth, received_offer.configs[i].bit_depth);
    EXPECT_EQ(offer.configs[i].channel_count,
              received_offer.configs[i].channel_count);
    EXPECT_EQ(offer.configs[i].delay_adjustment_ms,
              received_offer.configs[i].delay_adjustment_ms);
    EXPECT_TRUE(offer.configs[i].destination_id ==
                received_offer.configs[i].destination_id);
  }

  // << Application logic here, to select one of the configs. >>

  const AudioOfferringResponse response{
      .result = AudioOfferringResponse::kSuccess,
      .name = "Happy Player",
      .config_index = 1,
  };
  const auto response_wire_size = pb::ComputeSerializedSize(response);
  ASSERT_LT(0, response_wire_size);
  ASSERT_GE(static_cast<int>(sizeof(buffer)), response_wire_size);

  pb::Serialize(response, buffer);

  AudioOfferringResponse received_response{};
  parse_result = pb::MergeFromBuffer(buffer, buffer + response_wire_size,
                                     received_response);
  ASSERT_TRUE(parse_result);
  EXPECT_EQ(response.result, received_response.result);
  EXPECT_EQ(response.name.value_or("<NO NAME SENT>"),
            received_response.name.value_or("<NO NAME RECEIVED>"));
  EXPECT_EQ(response.config_index, received_response.config_index);
}

// message Registration {
//   map<string, int32> student_ages = 1;
// }
struct Registration {
  std::map<std::string, int32_t> student_ages;  // Name→Age

  using ProtobufFields =
      pb::FieldList<pb::Field<&Registration::student_ages, 1>>;
};

TEST(ExamplesTest, MapsExample) {
  const Registration registration{
      .student_ages =
          {
              {"alice", 8},
              {"bob", 8},
              {"charlie", 7},
          },
  };

  const auto wire_size = pb::ComputeSerializedSize(registration);
  uint8_t buffer[64];
  ASSERT_LT(0, wire_size);
  ASSERT_GE(static_cast<int>(sizeof(buffer)), wire_size);

  pb::Serialize(registration, buffer);

  const std::unique_ptr<Registration> received_registration =
      pb::Parse<Registration>(buffer, buffer + wire_size);
  ASSERT_TRUE(received_registration);
  ASSERT_EQ(3u, received_registration->student_ages.size());
  EXPECT_EQ(8, received_registration->student_ages["alice"]);
  EXPECT_EQ(8, received_registration->student_ages["bob"]);
  EXPECT_EQ(7, received_registration->student_ages["charlie"]);
}

// message BinaryTree {
//   message Node {
//     string value = 1;
//     optional Node left_child = 2;
//     optional Node right_child = 3;
//   }
//   optional Node root = 1;
// }
struct BinaryTree {
  struct Node {
    std::string value;
    std::unique_ptr<Node> left_child;
    std::unique_ptr<Node> right_child;

    bool is_leaf() const { return !left_child && !right_child; }

    using ProtobufFields = pb::FieldList<pb::Field<&Node::value, 1>,
                                         pb::Field<&Node::left_child, 2>,
                                         pb::Field<&Node::right_child, 3>>;
  };

  std::unique_ptr<Node> root;

  using ProtobufFields = pb::FieldList<pb::Field<&BinaryTree::root, 1>>;
};

TEST(ExamplesTest, BinaryTreeExample) {
  //         B
  //        / \
  //       A   C
  //            \
  //             D
  BinaryTree tree{};
  tree.root = std::make_unique<BinaryTree::Node>();
  tree.root->value = "B";
  tree.root->left_child = std::make_unique<BinaryTree::Node>();
  tree.root->left_child->value = "A";
  tree.root->right_child = std::make_unique<BinaryTree::Node>();
  tree.root->right_child->value = "C";
  tree.root->right_child->right_child = std::make_unique<BinaryTree::Node>();
  tree.root->right_child->right_child->value = "D";

  const auto wire_size = pb::ComputeSerializedSize(tree);
  uint8_t buffer[64];
  ASSERT_LT(0, wire_size);
  ASSERT_GE(static_cast<int>(sizeof(buffer)), wire_size);

  pb::Serialize(tree, buffer);

  BinaryTree received_tree{};
  const bool parse_result =
      pb::MergeFromBuffer(buffer, buffer + wire_size, received_tree);
  ASSERT_TRUE(parse_result);
  ASSERT_TRUE(received_tree.root);
  EXPECT_EQ("B", received_tree.root->value);
  ASSERT_TRUE(received_tree.root->left_child);
  EXPECT_EQ("A", received_tree.root->left_child->value);
  EXPECT_TRUE(received_tree.root->left_child->is_leaf());
  ASSERT_TRUE(received_tree.root->right_child);
  EXPECT_EQ("C", received_tree.root->right_child->value);
  EXPECT_FALSE(received_tree.root->right_child->is_leaf());
  EXPECT_FALSE(received_tree.root->right_child->left_child);
  ASSERT_TRUE(received_tree.root->right_child->right_child);
  EXPECT_EQ("D", received_tree.root->right_child->right_child->value);
  EXPECT_TRUE(received_tree.root->right_child->right_child->is_leaf());
}

// message BTree {
//   message Node {
//     repeated sint32 keys = 1;
//     repeated Node children = 2;
//     repeated std::string data_strings = 3;
//   }
//   Node root;
// }
struct BTree {
  struct Node {
    std::vector<pb::sint32_t> keys;
    std::vector<std::unique_ptr<Node>> children;
    std::vector<std::string> data_strings;

    bool is_leaf() const { return children.empty() && !data_strings.empty(); }

    using ProtobufFields = pb::FieldList<pb::Field<&Node::keys, 1>,
                                         pb::Field<&Node::children, 2>,
                                         pb::Field<&Node::data_strings, 3>>;
  };

  Node root;

  using ProtobufFields = pb::FieldList<pb::Field<&BTree::root, 1>>;
};

TEST(ExamplesTest, BtreeExample) {
  //                            root
  //                          /   |  \
  //                      /       |     \
  //                  /           |         \
  //              /               |             \
  //          -2                  4                 10
  //         /  \                  \                   \
  // -5 -4 -3   -2 -1  0  1  2  3   4  5  6  7  8  9   10 11 12 13
  //  |  |  |    |  |  |  |  |  |   |  |  |  |  |  |    |  |  |  |
  //  A  B  C    D  E  F  G  H  I   J  K  L  M  N  O    P  Q  R  S
  BTree btree{};
  btree.root.keys.push_back(-2);
  btree.root.keys.push_back(4);
  btree.root.keys.push_back(10);
  for (int i = 0; i < 4; ++i) {
    btree.root.children.push_back(std::make_unique<BTree::Node>());
  }
  for (int i = -5; i < -2; ++i) {
    btree.root.children[0]->keys.push_back(i);
    btree.root.children[0]->data_strings.emplace_back(1, 'A' + (i + 5));
  }
  for (int i = -2; i < 4; ++i) {
    btree.root.children[1]->keys.push_back(i);
    btree.root.children[1]->data_strings.emplace_back(1, 'D' + (i + 2));
  }
  for (int i = 4; i < 10; ++i) {
    btree.root.children[2]->keys.push_back(i);
    btree.root.children[2]->data_strings.emplace_back(1, 'J' + (i - 4));
  }
  for (int i = 10; i < 14; ++i) {
    btree.root.children[3]->keys.push_back(i);
    btree.root.children[3]->data_strings.emplace_back(1, 'P' + (i - 10));
  }

  const auto wire_size = pb::ComputeSerializedSize(btree);
  uint8_t buffer[128];
  ASSERT_LT(0, wire_size);
  ASSERT_GE(static_cast<int>(sizeof(buffer)), wire_size);

  pb::Serialize(btree, buffer);

  BTree received_btree{};
  const bool parse_result =
      pb::MergeFromBuffer(buffer, buffer + wire_size, received_btree);
  ASSERT_TRUE(parse_result);
  ASSERT_FALSE(received_btree.root.is_leaf());
  ASSERT_EQ(3u, received_btree.root.keys.size());
  ASSERT_EQ(4u, received_btree.root.children.size());

  std::map<int, std::string> seen;
  for (std::size_t i = 0; i < 4; ++i) {
    ASSERT_TRUE(received_btree.root.children[i]);
    BTree::Node& node = *received_btree.root.children[i];
    ASSERT_TRUE(node.is_leaf());
    ASSERT_TRUE(node.keys.size() == node.data_strings.size());
    if (i == 0) {
      const auto pivot = received_btree.root.keys[0];
      for (const auto& key : node.keys) {
        EXPECT_GT(pivot, key);
      }
    } else {
      const auto pivot = received_btree.root.keys[i - 1];
      for (const auto& key : node.keys) {
        EXPECT_LE(pivot, key);
      }
    }
    for (std::size_t j = 0, end = node.keys.size(); j < end; ++j) {
      seen.insert(
          std::pair<int, std::string>(node.keys[j], node.data_strings[j]));
    }
  }

  EXPECT_EQ(19u, seen.size());
  for (int i = -5; i < 14; ++i) {
    EXPECT_EQ(std::string(1, static_cast<char>('A' + (i + 5))), seen[i]);
  }
}

}  // namespace

/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/wtf/TreeNode.h"

#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

class TestTree : public RefCounted<TestTree>, public TreeNode<TestTree> {
 public:
  static RefPtr<TestTree> Create() { return AdoptRef(new TestTree()); }
};

TEST(TreeNodeTest, AppendChild) {
  RefPtr<TestTree> root = TestTree::Create();
  RefPtr<TestTree> first_child = TestTree::Create();
  RefPtr<TestTree> last_child = TestTree::Create();

  root->AppendChild(first_child.Get());
  EXPECT_EQ(root->FirstChild(), first_child.Get());
  EXPECT_EQ(root->LastChild(), first_child.Get());
  EXPECT_EQ(first_child->Parent(), root.Get());

  root->AppendChild(last_child.Get());
  EXPECT_EQ(root->FirstChild(), first_child.Get());
  EXPECT_EQ(root->LastChild(), last_child.Get());
  EXPECT_EQ(last_child->Previous(), first_child.Get());
  EXPECT_EQ(first_child->Next(), last_child.Get());
  EXPECT_EQ(last_child->Parent(), root.Get());
}

TEST(TreeNodeTest, InsertBefore) {
  RefPtr<TestTree> root = TestTree::Create();
  RefPtr<TestTree> first_child = TestTree::Create();
  RefPtr<TestTree> middle_child = TestTree::Create();
  RefPtr<TestTree> last_child = TestTree::Create();

  // Inserting single node
  root->InsertBefore(last_child.Get(), 0);
  EXPECT_EQ(last_child->Parent(), root.Get());
  EXPECT_EQ(root->FirstChild(), last_child.Get());
  EXPECT_EQ(root->LastChild(), last_child.Get());

  // Then prepend
  root->InsertBefore(first_child.Get(), last_child.Get());
  EXPECT_EQ(first_child->Parent(), root.Get());
  EXPECT_EQ(root->FirstChild(), first_child.Get());
  EXPECT_EQ(root->LastChild(), last_child.Get());
  EXPECT_EQ(first_child->Next(), last_child.Get());
  EXPECT_EQ(first_child.Get(), last_child->Previous());

  // Inserting in the middle
  root->InsertBefore(middle_child.Get(), last_child.Get());
  EXPECT_EQ(middle_child->Parent(), root.Get());
  EXPECT_EQ(root->FirstChild(), first_child.Get());
  EXPECT_EQ(root->LastChild(), last_child.Get());
  EXPECT_EQ(middle_child->Previous(), first_child.Get());
  EXPECT_EQ(middle_child->Next(), last_child.Get());
  EXPECT_EQ(first_child->Next(), middle_child.Get());
  EXPECT_EQ(last_child->Previous(), middle_child.Get());
}

TEST(TreeNodeTest, RemoveSingle) {
  RefPtr<TestTree> root = TestTree::Create();
  RefPtr<TestTree> child = TestTree::Create();
  RefPtr<TestTree> null_node;

  root->AppendChild(child.Get());
  root->RemoveChild(child.Get());
  EXPECT_EQ(child->Next(), null_node.Get());
  EXPECT_EQ(child->Previous(), null_node.Get());
  EXPECT_EQ(child->Parent(), null_node.Get());
  EXPECT_EQ(root->FirstChild(), null_node.Get());
  EXPECT_EQ(root->LastChild(), null_node.Get());
}

class Trio {
 public:
  Trio()
      : root(TestTree::Create()),
        first_child(TestTree::Create()),
        middle_child(TestTree::Create()),
        last_child(TestTree::Create()) {}

  void AppendChildren() {
    root->AppendChild(first_child.Get());
    root->AppendChild(middle_child.Get());
    root->AppendChild(last_child.Get());
  }

  RefPtr<TestTree> root;
  RefPtr<TestTree> first_child;
  RefPtr<TestTree> middle_child;
  RefPtr<TestTree> last_child;
};

TEST(TreeNodeTest, RemoveMiddle) {
  Trio trio;
  trio.AppendChildren();

  trio.root->RemoveChild(trio.middle_child.Get());
  EXPECT_TRUE(trio.middle_child->Orphan());
  EXPECT_EQ(trio.first_child->Next(), trio.last_child.Get());
  EXPECT_EQ(trio.last_child->Previous(), trio.first_child.Get());
  EXPECT_EQ(trio.root->FirstChild(), trio.first_child.Get());
  EXPECT_EQ(trio.root->LastChild(), trio.last_child.Get());
}

TEST(TreeNodeTest, RemoveLast) {
  RefPtr<TestTree> null_node;
  Trio trio;
  trio.AppendChildren();

  trio.root->RemoveChild(trio.last_child.Get());
  EXPECT_TRUE(trio.last_child->Orphan());
  EXPECT_EQ(trio.middle_child->Next(), null_node.Get());
  EXPECT_EQ(trio.root->FirstChild(), trio.first_child.Get());
  EXPECT_EQ(trio.root->LastChild(), trio.middle_child.Get());
}

TEST(TreeNodeTest, RemoveFirst) {
  RefPtr<TestTree> null_node;
  Trio trio;
  trio.AppendChildren();

  trio.root->RemoveChild(trio.first_child.Get());
  EXPECT_TRUE(trio.first_child->Orphan());
  EXPECT_EQ(trio.middle_child->Previous(), null_node.Get());
  EXPECT_EQ(trio.root->FirstChild(), trio.middle_child.Get());
  EXPECT_EQ(trio.root->LastChild(), trio.last_child.Get());
}

TEST(TreeNodeTest, TakeChildrenFrom) {
  RefPtr<TestTree> new_parent = TestTree::Create();
  Trio trio;
  trio.AppendChildren();

  new_parent->TakeChildrenFrom(trio.root.Get());

  EXPECT_FALSE(trio.root->HasChildren());
  EXPECT_TRUE(new_parent->HasChildren());
  EXPECT_EQ(trio.first_child.Get(), new_parent->FirstChild());
  EXPECT_EQ(trio.middle_child.Get(), new_parent->FirstChild()->Next());
  EXPECT_EQ(trio.last_child.Get(), new_parent->LastChild());
}

class TrioWithGrandChild : public Trio {
 public:
  TrioWithGrandChild() : grand_child(TestTree::Create()) {}

  void AppendChildren() {
    Trio::AppendChildren();
    middle_child->AppendChild(grand_child.Get());
  }

  RefPtr<TestTree> grand_child;
};

TEST(TreeNodeTest, TraverseNext) {
  TrioWithGrandChild trio;
  trio.AppendChildren();

  TestTree* order[] = {trio.root.Get(), trio.first_child.Get(),
                       trio.middle_child.Get(), trio.grand_child.Get(),
                       trio.last_child.Get()};

  unsigned order_index = 0;
  for (TestTree *node = trio.root.Get(); node;
       node = TraverseNext(node), order_index++)
    EXPECT_EQ(node, order[order_index]);
  EXPECT_EQ(order_index, sizeof(order) / sizeof(TestTree*));
}

TEST(TreeNodeTest, TraverseNextPostORder) {
  TrioWithGrandChild trio;
  trio.AppendChildren();

  TestTree* order[] = {trio.first_child.Get(), trio.grand_child.Get(),
                       trio.middle_child.Get(), trio.last_child.Get(),
                       trio.root.Get()};

  unsigned order_index = 0;
  for (TestTree *node = TraverseFirstPostOrder(trio.root.Get()); node;
       node = TraverseNextPostOrder(node), order_index++)
    EXPECT_EQ(node, order[order_index]);
  EXPECT_EQ(order_index, sizeof(order) / sizeof(TestTree*));
}

}  // namespace WTF

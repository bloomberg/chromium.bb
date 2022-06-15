// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xfa/fxfa/parser/cxfa_node.h"

#include "fxjs/gc/heap.h"
#include "fxjs/xfa/cjx_node.h"
#include "testing/fxgc_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/cppgc/persistent.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

class TestNode final : public CXFA_Node {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~TestNode() override = default;

 private:
  explicit TestNode(CXFA_Document* doc)
      : CXFA_Node(doc,
                  XFA_PacketType::Form,
                  {XFA_XDPPACKET::kTemplate, XFA_XDPPACKET::kForm},
                  XFA_ObjectType::Node,
                  XFA_Element::Node,
                  {},
                  {},
                  cppgc::MakeGarbageCollected<CJX_Node>(
                      doc->GetHeap()->GetAllocationHandle(),
                      this)) {}
};

}  // namespace

class CXFANodeTest : public FXGCUnitTest {
 public:
  void SetUp() override {
    FXGCUnitTest::SetUp();
    doc_ = cppgc::MakeGarbageCollected<CXFA_Document>(
        heap()->GetAllocationHandle(), nullptr, heap(), nullptr);
    node_ = cppgc::MakeGarbageCollected<TestNode>(heap()->GetAllocationHandle(),
                                                  doc_);
  }

  void TearDown() override {
    node_.Clear();
    doc_.Clear();
    FXGCUnitTest::TearDown();
  }

  CXFA_Document* GetDoc() const { return doc_; }
  CXFA_Node* GetNode() const { return node_; }

 private:
  cppgc::Persistent<CXFA_Document> doc_;
  cppgc::Persistent<TestNode> node_;
};

TEST_F(CXFANodeTest, InsertFirstChild) {
  EXPECT_FALSE(GetNode()->GetFirstChild());
  EXPECT_FALSE(GetNode()->GetLastChild());

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child);

  EXPECT_EQ(GetNode(), child->GetParent());
  EXPECT_EQ(child, GetNode()->GetFirstChild());
  EXPECT_EQ(child, GetNode()->GetLastChild());
  EXPECT_FALSE(child->GetPrevSibling());
  EXPECT_FALSE(child->GetNextSibling());
}

TEST_F(CXFANodeTest, InsertChildByNegativeIndex) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child);

  EXPECT_EQ(GetNode(), child->GetParent());
  EXPECT_FALSE(child->GetNextSibling());
  EXPECT_EQ(child0, child->GetPrevSibling());
  EXPECT_EQ(child, child0->GetNextSibling());
  EXPECT_FALSE(child0->GetPrevSibling());

  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child, GetNode()->GetLastChild());
}

TEST_F(CXFANodeTest, InsertChildByIndex) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* child2 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child2);

  CXFA_Node* child3 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child3);

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(2, child);

  EXPECT_EQ(GetNode(), child->GetParent());

  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child1, child0->GetNextSibling());
  EXPECT_EQ(child, child1->GetNextSibling());
  EXPECT_EQ(child2, child->GetNextSibling());
  EXPECT_EQ(child3, child2->GetNextSibling());
  EXPECT_FALSE(child3->GetNextSibling());

  EXPECT_EQ(child3, GetNode()->GetLastChild());
  EXPECT_EQ(child2, child3->GetPrevSibling());
  EXPECT_EQ(child, child2->GetPrevSibling());
  EXPECT_EQ(child1, child->GetPrevSibling());
  EXPECT_EQ(child0, child1->GetPrevSibling());
  EXPECT_FALSE(child0->GetPrevSibling());
}

TEST_F(CXFANodeTest, InsertChildIndexPastEnd) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(20, child);

  EXPECT_EQ(GetNode(), child->GetParent());
  EXPECT_FALSE(child->GetNextSibling());
  EXPECT_EQ(child1, child->GetPrevSibling());
  EXPECT_EQ(child, child1->GetNextSibling());

  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child, GetNode()->GetLastChild());
}

TEST_F(CXFANodeTest, InsertFirstChildBeforeNullptr) {
  EXPECT_FALSE(GetNode()->GetFirstChild());
  EXPECT_FALSE(GetNode()->GetLastChild());

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(child, nullptr);
  EXPECT_EQ(child, GetNode()->GetFirstChild());
  EXPECT_EQ(child, GetNode()->GetLastChild());
}

TEST_F(CXFANodeTest, InsertBeforeWithNullBefore) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(child, nullptr);

  EXPECT_EQ(GetNode(), child->GetParent());
  EXPECT_FALSE(child->GetNextSibling());
  EXPECT_EQ(child1, child->GetPrevSibling());
  EXPECT_EQ(child, child1->GetNextSibling());

  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child, GetNode()->GetLastChild());
}

TEST_F(CXFANodeTest, InsertBeforeFirstChild) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(child, child0);

  EXPECT_EQ(GetNode(), child->GetParent());
  EXPECT_EQ(child0, child->GetNextSibling());
  EXPECT_FALSE(child->GetPrevSibling());
  EXPECT_EQ(child, child0->GetPrevSibling());

  EXPECT_EQ(child, GetNode()->GetFirstChild());
  EXPECT_EQ(child1, GetNode()->GetLastChild());
}

TEST_F(CXFANodeTest, InsertBefore) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* child2 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child2);

  CXFA_Node* child3 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child3);

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(child, child2);

  EXPECT_EQ(GetNode(), child->GetParent());
  EXPECT_EQ(child2, child->GetNextSibling());
  EXPECT_EQ(child1, child->GetPrevSibling());
  EXPECT_EQ(child, child1->GetNextSibling());
  EXPECT_EQ(child, child2->GetPrevSibling());

  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child3, GetNode()->GetLastChild());
}

TEST_F(CXFANodeTest, RemoveOnlyChild) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);
  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child0, GetNode()->GetLastChild());

  GetNode()->RemoveChildAndNotify(child0, false);
  EXPECT_FALSE(GetNode()->GetFirstChild());
  EXPECT_FALSE(GetNode()->GetLastChild());
  EXPECT_FALSE(child0->GetParent());
  EXPECT_FALSE(child0->GetNextSibling());
  EXPECT_FALSE(child0->GetPrevSibling());
}

TEST_F(CXFANodeTest, RemoveFirstChild) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* child2 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child2);
  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child2, GetNode()->GetLastChild());

  GetNode()->RemoveChildAndNotify(child0, false);
  EXPECT_EQ(child1, GetNode()->GetFirstChild());
  EXPECT_EQ(child2, GetNode()->GetLastChild());
  EXPECT_FALSE(child1->GetPrevSibling());
  EXPECT_FALSE(child0->GetParent());
  EXPECT_FALSE(child0->GetNextSibling());
  EXPECT_FALSE(child0->GetPrevSibling());
}

TEST_F(CXFANodeTest, RemoveLastChild) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* child2 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child2);
  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child2, GetNode()->GetLastChild());

  GetNode()->RemoveChildAndNotify(child2, false);
  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child1, GetNode()->GetLastChild());
  EXPECT_FALSE(child1->GetNextSibling());
  EXPECT_FALSE(child2->GetParent());
  EXPECT_FALSE(child2->GetNextSibling());
  EXPECT_FALSE(child2->GetPrevSibling());
}

TEST_F(CXFANodeTest, RemoveChild) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* child2 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child2);
  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child2, GetNode()->GetLastChild());

  GetNode()->RemoveChildAndNotify(child1, false);
  EXPECT_EQ(child0, GetNode()->GetFirstChild());
  EXPECT_EQ(child2, GetNode()->GetLastChild());
  EXPECT_EQ(child2, child0->GetNextSibling());
  EXPECT_EQ(child0, child2->GetPrevSibling());
  EXPECT_FALSE(child1->GetParent());
  EXPECT_FALSE(child1->GetNextSibling());
  EXPECT_FALSE(child1->GetPrevSibling());
}

TEST_F(CXFANodeTest, InsertChildWithParent) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  child0->InsertChildAndNotify(-1, child1);

  EXPECT_DEATH_IF_SUPPORTED(GetNode()->InsertChildAndNotify(0, child1), "");
}

TEST_F(CXFANodeTest, InsertNullChild) {
  EXPECT_DEATH_IF_SUPPORTED(GetNode()->InsertChildAndNotify(0, nullptr), "");
}

TEST_F(CXFANodeTest, InsertBeforeWithNullChild) {
  EXPECT_DEATH_IF_SUPPORTED(GetNode()->InsertChildAndNotify(nullptr, nullptr),
                            "");
}

TEST_F(CXFANodeTest, InsertBeforeWithBeforeInAnotherParent) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  child0->InsertChildAndNotify(-1, child1);

  CXFA_Node* child =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  EXPECT_DEATH_IF_SUPPORTED(GetNode()->InsertChildAndNotify(child, child1), "");
}

TEST_F(CXFANodeTest, InsertBeforeWithNodeInAnotherParent) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  child0->InsertChildAndNotify(-1, child1);

  EXPECT_DEATH_IF_SUPPORTED(GetNode()->InsertChildAndNotify(child1, nullptr),
                            "");
}

TEST_F(CXFANodeTest, RemoveChildNullptr) {
  EXPECT_DEATH_IF_SUPPORTED(GetNode()->RemoveChildAndNotify(nullptr, false),
                            "");
}

TEST_F(CXFANodeTest, RemoveChildAnotherParent) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  child0->InsertChildAndNotify(-1, child1);

  GetNode()->RemoveChildAndNotify(child1, false);
  EXPECT_EQ(child0, child1->GetParent());
}

TEST_F(CXFANodeTest, AncestorOf) {
  CXFA_Node* child0 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child0);

  CXFA_Node* child1 =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  GetNode()->InsertChildAndNotify(-1, child1);

  CXFA_Node* grandchild =
      GetDoc()->CreateNode(XFA_PacketType::Form, XFA_Element::Ui);
  child0->InsertChildAndNotify(-1, grandchild);

  EXPECT_TRUE(GetNode()->IsAncestorOf(child0));
  EXPECT_TRUE(GetNode()->IsAncestorOf(child1));
  EXPECT_FALSE(child1->IsAncestorOf(child0));
  EXPECT_TRUE(child0->IsAncestorOf(grandchild));
  EXPECT_TRUE(GetNode()->IsAncestorOf(grandchild));
  EXPECT_FALSE(child1->IsAncestorOf(grandchild));
}

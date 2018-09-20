// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_traversal_root.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class StyleTraversalRootTestImpl : public StyleTraversalRoot {
  STACK_ALLOCATED();

 public:
  StyleTraversalRootTestImpl() {}
  void MarkDirty(const Node* node) { dirty_nodes_.insert(node); }
  void MarkChildDirty(const Node* node) { child_dirty_nodes_.insert(node); }
  bool IsSingleRoot() const { return root_type_ == RootType::kSingleRoot; }
  bool IsCommonRoot() const { return root_type_ == RootType::kCommonRoot; }

 private:
#if DCHECK_IS_ON()
  ContainerNode* Parent(const Node& node) const final {
    return node.parentNode();
  }
  bool IsChildDirty(const ContainerNode& node) const final {
    return child_dirty_nodes_.Contains(&node);
  }
#endif  // DCHECK_IS_ON()
  bool IsDirty(const Node& node) const final {
    return dirty_nodes_.Contains(&node);
  }
  void ClearChildDirtyForAncestors(ContainerNode& parent) const final {
    for (ContainerNode* ancestor = &parent; ancestor;
         ancestor = ancestor->parentNode()) {
      child_dirty_nodes_.erase(ancestor);
    }
  }

  HeapHashSet<Member<const Node>> dirty_nodes_;
  mutable HeapHashSet<Member<const Node>> child_dirty_nodes_;
};

class StyleTraversalRootTest : public testing::Test {
 protected:
  enum ElementIndex { kA, kB, kC, kD, kE, kF, kG, kElementCount };
  void SetUp() final {
    document_ = Document::CreateForTest();
    elements_ = new HeapVector<Member<Element>, 7>;
    for (size_t i = 0; i < kElementCount; i++) {
      elements_->push_back(GetDocument().CreateRawElement(HTMLNames::divTag));
    }
    GetDocument().appendChild(DivElement(kA));
    DivElement(kA)->appendChild(DivElement(kB));
    DivElement(kA)->appendChild(DivElement(kC));
    DivElement(kB)->appendChild(DivElement(kD));
    DivElement(kB)->appendChild(DivElement(kE));
    DivElement(kC)->appendChild(DivElement(kF));
    DivElement(kC)->appendChild(DivElement(kG));

    // Tree Looks like this:
    // div#a
    // |-- div#b
    // |   |-- div#d
    // |   `-- div#e
    // `-- div#c
    //     |-- div#f
    //     `-- div#g
  }
  Document& GetDocument() { return *document_; }
  Element* DivElement(ElementIndex index) { return elements_->at(index); }

 private:
  Persistent<Document> document_;
  Persistent<HeapVector<Member<Element>, 7>> elements_;
};

TEST_F(StyleTraversalRootTest, Update_SingleRoot) {
  StyleTraversalRootTestImpl root;
  root.MarkChildDirty(&GetDocument());
  root.MarkDirty(DivElement(kA));

  // A single dirty node becomes a single root.
  root.Update(nullptr, DivElement(kA));
  EXPECT_EQ(DivElement(kA), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());
}

TEST_F(StyleTraversalRootTest, Update_CommonRoot) {
  StyleTraversalRootTestImpl root;
  root.MarkChildDirty(&GetDocument());
  root.MarkChildDirty(DivElement(kA));
  root.MarkDirty(DivElement(kC));

  // Initially make B a single root.
  root.Update(nullptr, DivElement(kB));
  EXPECT_EQ(DivElement(kB), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());

  // Adding C makes A a common root.
  root.MarkDirty(DivElement(kB));
  root.Update(DivElement(kA), DivElement(kC));
  EXPECT_EQ(DivElement(kA), root.GetRootNode());
  EXPECT_FALSE(root.IsSingleRoot());
  EXPECT_TRUE(root.IsCommonRoot());
}

TEST_F(StyleTraversalRootTest, Update_CommonRootDirtySubtree) {
  StyleTraversalRootTestImpl root;
  root.MarkChildDirty(&GetDocument());
  root.MarkDirty(DivElement(kA));
  root.Update(nullptr, DivElement(kA));

  // Marking descendants of a single dirty root makes the single root a common
  // root as long as the new common ancestor is the current root.
  root.MarkChildDirty(DivElement(kA));
  root.MarkChildDirty(DivElement(kB));
  root.MarkDirty(DivElement(kD));
  root.Update(DivElement(kA), DivElement(kD));
  EXPECT_EQ(DivElement(kA), root.GetRootNode());
  EXPECT_FALSE(root.IsSingleRoot());
  EXPECT_TRUE(root.IsCommonRoot());
}

TEST_F(StyleTraversalRootTest, Update_CommonRootDocumentFallback) {
  StyleTraversalRootTestImpl root;

  // Initially make B a common root for D and E.
  root.MarkChildDirty(&GetDocument());
  root.MarkChildDirty(DivElement(kA));
  root.MarkChildDirty(DivElement(kB));
  root.MarkDirty(DivElement(kD));
  root.Update(nullptr, DivElement(kD));
  root.MarkDirty(DivElement(kE));
  root.Update(DivElement(kB), DivElement(kE));
  EXPECT_EQ(DivElement(kB), root.GetRootNode());
  EXPECT_FALSE(root.IsSingleRoot());
  EXPECT_TRUE(root.IsCommonRoot());

  // Adding C falls back to using the document as the root because we don't know
  // if A is above or below the current common root B.
  root.MarkDirty(DivElement(kC));
  root.Update(DivElement(kA), DivElement(kC));
  EXPECT_EQ(&GetDocument(), root.GetRootNode());
  EXPECT_FALSE(root.IsSingleRoot());
  EXPECT_TRUE(root.IsCommonRoot());
}

TEST_F(StyleTraversalRootTest, ChildrenRemoved) {
  StyleTraversalRootTestImpl root;
  // Initially make E a single root.
  root.MarkChildDirty(&GetDocument());
  root.MarkChildDirty(DivElement(kA));
  root.MarkChildDirty(DivElement(kB));
  root.MarkDirty(DivElement(kE));
  root.Update(nullptr, DivElement(kE));
  EXPECT_EQ(DivElement(kE), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());

  // Removing D not affecting E.
  DivElement(kD)->remove();
  root.ChildrenRemoved(*DivElement(kB));
  EXPECT_EQ(DivElement(kE), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());

  // Removing B
  DivElement(kB)->remove();
  root.ChildrenRemoved(*DivElement(kA));
  EXPECT_FALSE(root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());
}

}  // namespace blink

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/v2/public/view.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v2 {

// View ------------------------------------------------------------------------

typedef testing::Test ViewTest;

TEST_F(ViewTest, AddChild) {
  View v1;
  View* v11 = new View;
  v1.AddChild(v11);
  EXPECT_EQ(1U, v1.children().size());
}

TEST_F(ViewTest, RemoveChild) {
  View v1;
  View* v11 = new View;
  v1.AddChild(v11);
  EXPECT_EQ(1U, v1.children().size());
  v1.RemoveChild(v11);
  EXPECT_EQ(0U, v1.children().size());
}

TEST_F(ViewTest, Contains) {
  View v1;

  // Direct descendant.
  View* v11 = new View;
  v1.AddChild(v11);
  EXPECT_TRUE(v1.Contains(v11));

  // Indirect descendant.
  View* v111 = new View;
  v11->AddChild(v111);
  EXPECT_TRUE(v1.Contains(v111));
}

TEST_F(ViewTest, Stacking) {
  View v1;
  View* v11 = new View;
  View* v12 = new View;
  View* v13 = new View;
  v1.AddChild(v11);
  v1.AddChild(v12);
  v1.AddChild(v13);

  // Order: v11, v12, v13
  EXPECT_EQ(3U, v1.children().size());
  EXPECT_EQ(v11, v1.children().front());
  EXPECT_EQ(v13, v1.children().back());

  // Move v11 to front.
  // Resulting order: v12, v13, v11
  v1.StackChildAtTop(v11);
  EXPECT_EQ(v12, v1.children().front());
  EXPECT_EQ(v11, v1.children().back());

  // Move v11 to back.
  // Resulting order: v11, v12, v13
  v1.StackChildAtBottom(v11);
  EXPECT_EQ(v11, v1.children().front());
  EXPECT_EQ(v13, v1.children().back());

  // Move v11 above v12.
  // Resulting order: v12. v11, v13
  v1.StackChildAbove(v11, v12);
  EXPECT_EQ(v12, v1.children().front());
  EXPECT_EQ(v13, v1.children().back());

  // Move v11 below v12.
  // Resulting order: v11, v12, v13
  v1.StackChildBelow(v11, v12);
  EXPECT_EQ(v11, v1.children().front());
  EXPECT_EQ(v13, v1.children().back());
}

// ViewObserver ----------------------------------------------------------------

typedef testing::Test ViewObserverTest;

bool TreeChangeParamsMatch(const ViewObserver::TreeChangeParams& lhs,
                           const ViewObserver::TreeChangeParams& rhs) {
  return lhs.target == rhs.target &&  lhs.old_parent == rhs.old_parent &&
      lhs.new_parent == rhs.new_parent && lhs.receiver == rhs.receiver &&
      lhs.phase == rhs.phase;
}

class TreeChangeObserver : public ViewObserver {
 public:
  explicit TreeChangeObserver(View* observee) : observee_(observee) {
    observee_->AddObserver(this);
  }
  virtual ~TreeChangeObserver() {
    observee_->RemoveObserver(this);
  }

  void Reset() {
      received_params_.clear();
  }

  const std::vector<TreeChangeParams>& received_params() {
    return received_params_;
  }

 private:
  // Overridden from ViewObserver:
  virtual void OnViewTreeChange(const TreeChangeParams& params) OVERRIDE {
    received_params_.push_back(params);
  }

  View* observee_;
  std::vector<TreeChangeParams> received_params_;

  DISALLOW_COPY_AND_ASSIGN(TreeChangeObserver);
};

TEST_F(ViewObserverTest, TreeChange_SimpleAddRemove) {
  View v1;
  TreeChangeObserver o1(&v1);
  EXPECT_TRUE(o1.received_params().empty());

  View v11;
  v11.set_owned_by_parent(false);
  TreeChangeObserver o11(&v11);
  EXPECT_TRUE(o11.received_params().empty());

  // Add.

  v1.AddChild(&v11);

  EXPECT_EQ(1U, o1.received_params().size());
  ViewObserver::TreeChangeParams p1;
  p1.target = &v11;
  p1.receiver = &v1;
  p1.old_parent = NULL;
  p1.new_parent = &v1;
  p1.phase = ViewObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  ViewObserver::TreeChangeParams p11 = p1;
  p11.receiver = &v11;
  p11.phase = ViewObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  p11.phase = ViewObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  o1.Reset();
  o11.Reset();
  EXPECT_TRUE(o1.received_params().empty());
  EXPECT_TRUE(o11.received_params().empty());

  // Remove.

  v1.RemoveChild(&v11);

  EXPECT_EQ(1U, o1.received_params().size());
  p1.target = &v11;
  p1.receiver = &v1;
  p1.old_parent = &v1;
  p1.new_parent = NULL;
  p1.phase = ViewObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  p11.phase = ViewObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));
}

TEST_F(ViewObserverTest, TreeChange_NestedAddRemove) {
  View v1, v11, v111, v1111, v1112;

  // Root tree.
  v11.set_owned_by_parent(false);
  v1.AddChild(&v11);

  // Tree to be attached.
  v111.set_owned_by_parent(false);
  v1111.set_owned_by_parent(false);
  v111.AddChild(&v1111);
  v1112.set_owned_by_parent(false);
  v111.AddChild(&v1112);

  TreeChangeObserver o1(&v1), o11(&v11), o111(&v111), o1111(&v1111),
      o1112(&v1112);
  ViewObserver::TreeChangeParams p1, p11, p111, p1111, p1112;

  // Add.

  v11.AddChild(&v111);

  EXPECT_EQ(1U, o1.received_params().size());
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = NULL;
  p1.new_parent = &v11;
  p1.phase = ViewObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(1U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &v111;
  p111.phase = ViewObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  p111.phase = ViewObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &v1111;
  p1111.phase = ViewObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  p1111.phase = ViewObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &v1112;
  p1112.phase = ViewObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().front()));
  p1112.phase = ViewObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().back()));

  // Remove.
}

}  // namespace v2

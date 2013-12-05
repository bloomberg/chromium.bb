// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_targeter.h"
#include "ui/events/test/test_event_processor.h"
#include "ui/events/test/test_event_target.h"

namespace ui {
namespace test {

class EventProcessorTest : public testing::Test {
 public:
  EventProcessorTest() {}
  virtual ~EventProcessorTest() {}

  // testing::Test:
  virtual void SetUp() OVERRIDE {
    processor_.SetRoot(scoped_ptr<EventTarget>(new TestEventTarget()));
    root()->SetEventTargeter(make_scoped_ptr(new EventTargeter()));
  }

  TestEventTarget* root() {
    return static_cast<TestEventTarget*>(processor_.GetRootTarget());
  }

  void DispatchEvent(Event* event) {
    processor_.OnEventFromSource(event);
  }

 protected:
  TestEventProcessor processor_;

  DISALLOW_COPY_AND_ASSIGN(EventProcessorTest);
};

TEST_F(EventProcessorTest, Basic) {
  scoped_ptr<TestEventTarget> child(new TestEventTarget());
  root()->AddChild(child.Pass());

  MouseEvent mouse(ET_MOUSE_MOVED, gfx::Point(10, 10), gfx::Point(10, 10),
                   EF_NONE);
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->child_at(0)->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));

  root()->RemoveChild(root()->child_at(0));
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
}

template<typename T>
class BoundsEventTargeter : public EventTargeter {
 public:
  virtual ~BoundsEventTargeter() {}

 protected:
  virtual bool SubtreeShouldBeExploredForEvent(
      EventTarget* target, const LocatedEvent& event) OVERRIDE {
    T* t = static_cast<T*>(target);
    return (t->bounds().Contains(event.location()));
  }
};

class BoundsTestTarget : public TestEventTarget {
 public:
  BoundsTestTarget() {}
  virtual ~BoundsTestTarget() {}

  void set_bounds(gfx::Rect rect) { bounds_ = rect; }
  gfx::Rect bounds() const { return bounds_; }

  static void ConvertPointToTarget(BoundsTestTarget* source,
                                   BoundsTestTarget* target,
                                   gfx::Point* location) {
    gfx::Vector2d vector;
    if (source->Contains(target)) {
      for (; target && target != source;
           target = static_cast<BoundsTestTarget*>(target->parent())) {
        vector += target->bounds().OffsetFromOrigin();
      }
      *location -= vector;
    } else if (target->Contains(source)) {
      for (; source && source != target;
           source = static_cast<BoundsTestTarget*>(source->parent())) {
        vector += source->bounds().OffsetFromOrigin();
      }
      *location += vector;
    } else {
      NOTREACHED();
    }
  }

 private:
  // EventTarget:
  virtual void ConvertEventToTarget(EventTarget* target,
                                    LocatedEvent* event) OVERRIDE {
    event->ConvertLocationToTarget(this,
                                   static_cast<BoundsTestTarget*>(target));
  }

  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(BoundsTestTarget);
};

TEST_F(EventProcessorTest, Bounds) {
  scoped_ptr<BoundsTestTarget> parent(new BoundsTestTarget());
  scoped_ptr<BoundsTestTarget> child(new BoundsTestTarget());
  scoped_ptr<BoundsTestTarget> grandchild(new BoundsTestTarget());

  parent->set_bounds(gfx::Rect(0, 0, 30, 30));
  child->set_bounds(gfx::Rect(5, 5, 20, 20));
  grandchild->set_bounds(gfx::Rect(5, 5, 5, 5));

  child->AddChild(scoped_ptr<TestEventTarget>(grandchild.Pass()));
  parent->AddChild(scoped_ptr<TestEventTarget>(child.Pass()));
  root()->AddChild(scoped_ptr<TestEventTarget>(parent.Pass()));

  ASSERT_EQ(1u, root()->child_count());
  ASSERT_EQ(1u, root()->child_at(0)->child_count());
  ASSERT_EQ(1u, root()->child_at(0)->child_at(0)->child_count());

  TestEventTarget* parent_r = root()->child_at(0);
  TestEventTarget* child_r = parent_r->child_at(0);
  TestEventTarget* grandchild_r = child_r->child_at(0);

  // Dispatch a mouse event that falls on the parent, but not on the child. When
  // the default event-targeter used, the event will still reach |grandchild|,
  // because the default targeter does not look at the bounds.
  MouseEvent mouse(ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(1, 1), EF_NONE);
  DispatchEvent(&mouse);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(parent_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(child_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(ET_MOUSE_MOVED));
  grandchild_r->ResetReceivedEvents();

  // Now install a targeter on the parent that looks at the bounds and makes
  // sure the event reaches the target only if the location of the event within
  // the bounds of the target.
  parent_r->SetEventTargeter(scoped_ptr<EventTargeter>(
      new BoundsEventTargeter<BoundsTestTarget>()));
  DispatchEvent(&mouse);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_TRUE(parent_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(child_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(grandchild_r->DidReceiveEvent(ET_MOUSE_MOVED));
  parent_r->ResetReceivedEvents();

  MouseEvent second(ET_MOUSE_MOVED, gfx::Point(12, 12), gfx::Point(12, 12),
                    EF_NONE);
  DispatchEvent(&second);
  EXPECT_FALSE(root()->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(parent_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_FALSE(child_r->DidReceiveEvent(ET_MOUSE_MOVED));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(ET_MOUSE_MOVED));
}

}  // namespace test
}  // namespace ui

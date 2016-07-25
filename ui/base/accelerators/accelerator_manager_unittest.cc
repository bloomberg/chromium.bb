// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_manager.h"

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator_manager_delegate.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
namespace test {

namespace {

class TestTarget : public AcceleratorTarget {
 public:
  TestTarget() : accelerator_pressed_count_(0) {}
  ~TestTarget() override {}

  int accelerator_pressed_count() const {
    return accelerator_pressed_count_;
  }

  void set_accelerator_pressed_count(int accelerator_pressed_count) {
    accelerator_pressed_count_ = accelerator_pressed_count;
  }

  // Overridden from AcceleratorTarget:
  bool AcceleratorPressed(const Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  int accelerator_pressed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTarget);
};

bool TestTarget::AcceleratorPressed(const Accelerator& accelerator) {
  ++accelerator_pressed_count_;
  return true;
}

bool TestTarget::CanHandleAccelerators() const {
  return true;
}

Accelerator GetAccelerator(KeyboardCode code, int mask) {
  return Accelerator(code, mask);
}

// AcceleratorManagerDelegate implementation that records calls to interface
// using the following format.
// . OnAcceleratorRegistered() -> 'Register ' + id
// . OnAcceleratorRegistered() -> 'Unregister' + id
// where the id is specified using SetIdForAccelerator().
class TestAcceleratorManagerDelegate : public AcceleratorManagerDelegate {
 public:
  TestAcceleratorManagerDelegate() {}
  ~TestAcceleratorManagerDelegate() override {}

  void SetIdForAccelerator(const Accelerator& accelerator,
                           const std::string& id) {
    accelerator_to_id_[accelerator] = id;
  }

  std::string GetAndClearCommands() {
    std::string commands;
    std::swap(commands, commands_);
    return commands;
  }

  // AcceleratorManagerDelegate:
  void OnAcceleratorRegistered(const Accelerator& accelerator) override {
    if (!commands_.empty())
      commands_ += " ";
    commands_ += "Register " + accelerator_to_id_[accelerator];
  }
  void OnAcceleratorUnregistered(const Accelerator& accelerator) override {
    if (!commands_.empty())
      commands_ += " ";
    commands_ += "Unregister " + accelerator_to_id_[accelerator];
  }

 private:
  std::map<Accelerator, std::string> accelerator_to_id_;
  std::string commands_;

  DISALLOW_COPY_AND_ASSIGN(TestAcceleratorManagerDelegate);
};

}  // namespace

class AcceleratorManagerTest : public testing::Test {
 public:
  AcceleratorManagerTest() : manager_(&delegate_) {}
  ~AcceleratorManagerTest() override {}

 protected:
  TestAcceleratorManagerDelegate delegate_;
  AcceleratorManager manager_;
};

TEST_F(AcceleratorManagerTest, Register) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  TestTarget target;
  delegate_.SetIdForAccelerator(accelerator_a, "a");
  manager_.Register(accelerator_a, AcceleratorManager::kNormalPriority,
                    &target);
  EXPECT_EQ("Register a", delegate_.GetAndClearCommands());

  // The registered accelerator is processed.
  EXPECT_TRUE(manager_.Process(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());
}

TEST_F(AcceleratorManagerTest, RegisterMultipleTarget) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  delegate_.SetIdForAccelerator(accelerator_a, "a");
  TestTarget target1;
  manager_.Register(accelerator_a, AcceleratorManager::kNormalPriority,
                    &target1);
  EXPECT_EQ("Register a", delegate_.GetAndClearCommands());
  TestTarget target2;
  manager_.Register(accelerator_a, AcceleratorManager::kNormalPriority,
                    &target2);
  // Registering the same command shouldn't notify the delegate.
  EXPECT_TRUE(delegate_.GetAndClearCommands().empty());

  // If multiple targets are registered with the same accelerator, the target
  // registered later processes the accelerator.
  EXPECT_TRUE(manager_.Process(accelerator_a));
  EXPECT_EQ(0, target1.accelerator_pressed_count());
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorManagerTest, Unregister) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  delegate_.SetIdForAccelerator(accelerator_a, "a");
  TestTarget target;
  manager_.Register(accelerator_a, AcceleratorManager::kNormalPriority,
                    &target);
  EXPECT_EQ("Register a", delegate_.GetAndClearCommands());
  const Accelerator accelerator_b(VKEY_B, EF_NONE);
  delegate_.SetIdForAccelerator(accelerator_b, "b");
  manager_.Register(accelerator_b, AcceleratorManager::kNormalPriority,
                    &target);
  EXPECT_EQ("Register b", delegate_.GetAndClearCommands());

  // Unregistering a different accelerator does not affect the other
  // accelerator.
  manager_.Unregister(accelerator_b, &target);
  EXPECT_EQ("Unregister b", delegate_.GetAndClearCommands());
  EXPECT_TRUE(manager_.Process(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());

  // The unregistered accelerator is no longer processed.
  target.set_accelerator_pressed_count(0);
  manager_.Unregister(accelerator_a, &target);
  EXPECT_EQ("Unregister a", delegate_.GetAndClearCommands());
  EXPECT_FALSE(manager_.Process(accelerator_a));
  EXPECT_EQ(0, target.accelerator_pressed_count());
}

TEST_F(AcceleratorManagerTest, UnregisterAll) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  delegate_.SetIdForAccelerator(accelerator_a, "a");
  TestTarget target1;
  manager_.Register(accelerator_a, AcceleratorManager::kNormalPriority,
                    &target1);
  const Accelerator accelerator_b(VKEY_B, EF_NONE);
  delegate_.SetIdForAccelerator(accelerator_b, "b");
  manager_.Register(accelerator_b, AcceleratorManager::kNormalPriority,
                    &target1);
  const Accelerator accelerator_c(VKEY_C, EF_NONE);
  delegate_.SetIdForAccelerator(accelerator_c, "c");
  TestTarget target2;
  manager_.Register(accelerator_c, AcceleratorManager::kNormalPriority,
                    &target2);
  EXPECT_EQ("Register a Register b Register c",
            delegate_.GetAndClearCommands());
  manager_.UnregisterAll(&target1);
  {
    const std::string commands = delegate_.GetAndClearCommands();
    // Ordering is not guaranteed.
    EXPECT_TRUE(commands == "Unregister a Unregister b" ||
                commands == "Unregister b Unregister a");
  }

  // All the accelerators registered for |target1| are no longer processed.
  EXPECT_FALSE(manager_.Process(accelerator_a));
  EXPECT_FALSE(manager_.Process(accelerator_b));
  EXPECT_EQ(0, target1.accelerator_pressed_count());

  // UnregisterAll with a different target does not affect the other target.
  EXPECT_TRUE(manager_.Process(accelerator_c));
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorManagerTest, Process) {
  TestTarget target;

  // Test all 2*2*2 cases (shift/control/alt = on/off).
  for (int mask = 0; mask < 2 * 2 * 2; ++mask) {
    Accelerator accelerator(GetAccelerator(VKEY_A, mask));
    const base::string16 text = accelerator.GetShortcutText();
    manager_.Register(accelerator, AcceleratorManager::kNormalPriority,
                      &target);

    // The registered accelerator is processed.
    const int last_count = target.accelerator_pressed_count();
    EXPECT_TRUE(manager_.Process(accelerator)) << text;
    EXPECT_EQ(last_count + 1, target.accelerator_pressed_count()) << text;

    // The non-registered accelerators are not processed.
    accelerator.set_type(ET_UNKNOWN);
    EXPECT_FALSE(manager_.Process(accelerator)) << text;  // different type
    accelerator.set_type(ET_KEY_RELEASED);
    EXPECT_FALSE(manager_.Process(accelerator)) << text;  // different type

    EXPECT_FALSE(manager_.Process(GetAccelerator(VKEY_UNKNOWN, mask)))
        << text;  // different vkey
    EXPECT_FALSE(manager_.Process(GetAccelerator(VKEY_B, mask)))
        << text;  // different vkey
    EXPECT_FALSE(manager_.Process(GetAccelerator(VKEY_SHIFT, mask)))
        << text;  // different vkey

    for (int test_mask = 0; test_mask < 2 * 2 * 2; ++test_mask) {
      if (test_mask == mask)
        continue;
      const Accelerator test_accelerator(GetAccelerator(VKEY_A, test_mask));
      const base::string16 test_text = test_accelerator.GetShortcutText();
      EXPECT_FALSE(manager_.Process(test_accelerator))
          << text << ", " << test_text;  // different modifiers
    }

    EXPECT_EQ(last_count + 1, target.accelerator_pressed_count()) << text;
    manager_.UnregisterAll(&target);
  }
}

// Verifies delegate is notifed correctly when unregistering and registering
// with the same accelerator.
TEST_F(AcceleratorManagerTest, Reregister) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  TestTarget target;
  delegate_.SetIdForAccelerator(accelerator_a, "a");
  manager_.Register(accelerator_a, AcceleratorManager::kNormalPriority,
                    &target);
  EXPECT_EQ("Register a", delegate_.GetAndClearCommands());
  manager_.UnregisterAll(&target);
  EXPECT_EQ("Unregister a", delegate_.GetAndClearCommands());
  manager_.Register(accelerator_a, AcceleratorManager::kNormalPriority,
                    &target);
  EXPECT_EQ("Register a", delegate_.GetAndClearCommands());
}

}  // namespace test
}  // namespace ui

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/models/accelerator.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/focus/focus_manager_test.h"
#include "ui/views/widget/widget.h"
#include "views/controls/button/text_button.h"
#include "views/controls/textfield/textfield.h"

#if !defined(USE_AURA)
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/controls/tabbed_pane/native_tabbed_pane_wrapper.h"
#endif

#if defined(OS_LINUX)
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#endif

namespace views {

enum FocusTestEventType {
  ON_FOCUS = 0,
  ON_BLUR
};

struct FocusTestEvent {
  FocusTestEvent(FocusTestEventType type, int view_id)
      : type(type),
        view_id(view_id) {
  }

  FocusTestEventType type;
  int view_id;
};

class SimpleTestView : public View {
 public:
  SimpleTestView(std::vector<FocusTestEvent>* event_list, int view_id)
      : event_list_(event_list) {
    set_focusable(true);
    set_id(view_id);
  }

  virtual void OnFocus() {
    event_list_->push_back(FocusTestEvent(ON_FOCUS, id()));
  }

  virtual void OnBlur() {
    event_list_->push_back(FocusTestEvent(ON_BLUR, id()));
  }

 private:
  std::vector<FocusTestEvent>* event_list_;
};

// Tests that the appropriate Focus related methods are called when a View
// gets/loses focus.
TEST_F(FocusManagerTest, ViewFocusCallbacks) {
  std::vector<FocusTestEvent> event_list;
  const int kView1ID = 1;
  const int kView2ID = 2;

  SimpleTestView* view1 = new SimpleTestView(&event_list, kView1ID);
  SimpleTestView* view2 = new SimpleTestView(&event_list, kView2ID);
  GetContentsView()->AddChildView(view1);
  GetContentsView()->AddChildView(view2);

  view1->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(event_list.size()));
  EXPECT_EQ(ON_FOCUS, event_list[0].type);
  EXPECT_EQ(kView1ID, event_list[0].view_id);

  event_list.clear();
  view2->RequestFocus();
  ASSERT_EQ(2, static_cast<int>(event_list.size()));
  EXPECT_EQ(ON_BLUR, event_list[0].type);
  EXPECT_EQ(kView1ID, event_list[0].view_id);
  EXPECT_EQ(ON_FOCUS, event_list[1].type);
  EXPECT_EQ(kView2ID, event_list[1].view_id);

  event_list.clear();
  GetFocusManager()->ClearFocus();
  ASSERT_EQ(1, static_cast<int>(event_list.size()));
  EXPECT_EQ(ON_BLUR, event_list[0].type);
  EXPECT_EQ(kView2ID, event_list[0].view_id);
}

TEST_F(FocusManagerTest, FocusChangeListener) {
  View* view1 = new View();
  view1->set_focusable(true);
  View* view2 = new View();
  view2->set_focusable(true);
  GetContentsView()->AddChildView(view1);
  GetContentsView()->AddChildView(view2);

  TestFocusChangeListener listener;
  AddFocusChangeListener(&listener);

  // Visual Studio 2010 has problems converting NULL to the null pointer for
  // std::pair.  See http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  // It will work if we pass nullptr.
#if defined(_MSC_VER) && _MSC_VER >= 1600
  views::View* null_view = nullptr;
#else
  views::View* null_view = NULL;
#endif

  view1->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(null_view, view1));
  listener.ClearFocusChanges();

  view2->RequestFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view1, view2));
  listener.ClearFocusChanges();

  GetFocusManager()->ClearFocus();
  ASSERT_EQ(1, static_cast<int>(listener.focus_changes().size()));
  EXPECT_TRUE(listener.focus_changes()[0] == ViewPair(view2, null_view));
}

#if !defined(USE_AURA)
class TestTextfield : public Textfield {
 public:
  TestTextfield() {}
  virtual gfx::NativeView TestGetNativeControlView() {
    return native_wrapper_->GetTestingHandle();
  }
};

class TestTabbedPane : public TabbedPane {
 public:
  TestTabbedPane() {}
  virtual gfx::NativeView TestGetNativeControlView() {
    return native_tabbed_pane_->GetTestingHandle();
  }
};

// Tests that NativeControls do set the focused View appropriately on the
// FocusManager.
TEST_F(FocusManagerTest, FAILS_FocusNativeControls) {
  TestTextfield* textfield = new TestTextfield();
  TestTabbedPane* tabbed_pane = new TestTabbedPane();
  TestTextfield* textfield2 = new TestTextfield();

  GetContentsView()->AddChildView(textfield);
  GetContentsView()->AddChildView(tabbed_pane);

  tabbed_pane->AddTab(ASCIIToUTF16("Awesome textfield"), textfield2);

  // Simulate the native view getting the native focus (such as by user click).
  GetWidget()->FocusNativeView(textfield->TestGetNativeControlView());
  EXPECT_EQ(textfield, GetFocusManager()->GetFocusedView());

  GetWidget()->FocusNativeView(tabbed_pane->TestGetNativeControlView());
  EXPECT_EQ(tabbed_pane, GetFocusManager()->GetFocusedView());

  GetWidget()->FocusNativeView(textfield2->TestGetNativeControlView());
  EXPECT_EQ(textfield2, GetFocusManager()->GetFocusedView());
}
#endif

// There is no tabbed pane in Aura.
#if !defined(USE_AURA)
TEST_F(FocusManagerTest, ContainsView) {
  View* view = new View();
  scoped_ptr<View> detached_view(new View());
  TabbedPane* tabbed_pane = new TabbedPane();
  TabbedPane* nested_tabbed_pane = new TabbedPane();
  NativeTextButton* tab_button = new NativeTextButton(
      NULL, ASCIIToUTF16("tab button"));

  GetContentsView()->AddChildView(view);
  GetContentsView()->AddChildView(tabbed_pane);
  // Adding a View inside a TabbedPane to test the case of nested root view.

  tabbed_pane->AddTab(ASCIIToUTF16("Awesome tab"), nested_tabbed_pane);
  nested_tabbed_pane->AddTab(ASCIIToUTF16("Awesomer tab"), tab_button);

  EXPECT_TRUE(GetFocusManager()->ContainsView(view));
  EXPECT_TRUE(GetFocusManager()->ContainsView(tabbed_pane));
  EXPECT_TRUE(GetFocusManager()->ContainsView(nested_tabbed_pane));
  EXPECT_TRUE(GetFocusManager()->ContainsView(tab_button));
  EXPECT_FALSE(GetFocusManager()->ContainsView(detached_view.get()));
}
#endif

// Counts accelerator calls.
class TestAcceleratorTarget : public ui::AcceleratorTarget {
 public:
  explicit TestAcceleratorTarget(bool process_accelerator)
      : accelerator_count_(0), process_accelerator_(process_accelerator) {}

  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) {
    ++accelerator_count_;
    return process_accelerator_;
  }

  int accelerator_count() const { return accelerator_count_; }

 private:
  int accelerator_count_;  // number of times that the accelerator is activated
  bool process_accelerator_;  // return value of AcceleratorPressed

  DISALLOW_COPY_AND_ASSIGN(TestAcceleratorTarget);
};

TEST_F(FocusManagerTest, CallsNormalAcceleratorTarget) {
  FocusManager* focus_manager = GetFocusManager();
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, false, false, false);
  ui::Accelerator escape_accelerator(ui::VKEY_ESCAPE, false, false, false);

  TestAcceleratorTarget return_target(true);
  TestAcceleratorTarget escape_target(true);
  EXPECT_EQ(return_target.accelerator_count(), 0);
  EXPECT_EQ(escape_target.accelerator_count(), 0);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Register targets.
  focus_manager->RegisterAccelerator(return_accelerator, &return_target);
  focus_manager->RegisterAccelerator(escape_accelerator, &escape_target);

  // Checks if the correct target is registered.
  EXPECT_EQ(&return_target,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(&escape_target,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Hitting the return key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(escape_target.accelerator_count(), 0);

  // Hitting the escape key.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(escape_target.accelerator_count(), 1);

  // Register another target for the return key.
  TestAcceleratorTarget return_target2(true);
  EXPECT_EQ(return_target2.accelerator_count(), 0);
  focus_manager->RegisterAccelerator(return_accelerator, &return_target2);
  EXPECT_EQ(&return_target2,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key; return_target2 has the priority.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(return_target2.accelerator_count(), 1);

  // Register a target that does not process the accelerator event.
  TestAcceleratorTarget return_target3(false);
  EXPECT_EQ(return_target3.accelerator_count(), 0);
  focus_manager->RegisterAccelerator(return_accelerator, &return_target3);
  EXPECT_EQ(&return_target3,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key.
  // Since the event handler of return_target3 returns false, return_target2
  // should be called too.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 1);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 1);

  // Unregister return_target2.
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target2);
  EXPECT_EQ(&return_target3,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key. return_target3 and return_target should be called.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 2);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 2);

  // Unregister targets.
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target);
  focus_manager->UnregisterAccelerator(return_accelerator, &return_target3);
  focus_manager->UnregisterAccelerator(escape_accelerator, &escape_target);

  // Now there is no target registered.
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(escape_accelerator));

  // Hitting the return key and the escape key. Nothing should happen.
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(return_target.accelerator_count(), 2);
  EXPECT_EQ(return_target2.accelerator_count(), 2);
  EXPECT_EQ(return_target3.accelerator_count(), 2);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(escape_target.accelerator_count(), 1);
}

// Unregisters itself when its accelerator is invoked.
class SelfUnregisteringAcceleratorTarget : public ui::AcceleratorTarget {
 public:
  SelfUnregisteringAcceleratorTarget(ui::Accelerator accelerator,
                                     FocusManager* focus_manager)
      : accelerator_(accelerator),
        focus_manager_(focus_manager),
        accelerator_count_(0) {
  }

  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) {
    ++accelerator_count_;
    focus_manager_->UnregisterAccelerator(accelerator, this);
    return true;
  }

  int accelerator_count() const { return accelerator_count_; }

 private:
  ui::Accelerator accelerator_;
  FocusManager* focus_manager_;
  int accelerator_count_;

  DISALLOW_COPY_AND_ASSIGN(SelfUnregisteringAcceleratorTarget);
};

TEST_F(FocusManagerTest, CallsSelfDeletingAcceleratorTarget) {
  FocusManager* focus_manager = GetFocusManager();
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, false, false, false);
  SelfUnregisteringAcceleratorTarget target(return_accelerator, focus_manager);
  EXPECT_EQ(target.accelerator_count(), 0);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Register the target.
  focus_manager->RegisterAccelerator(return_accelerator, &target);
  EXPECT_EQ(&target,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key. The target will be unregistered.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(target.accelerator_count(), 1);
  EXPECT_EQ(NULL,
            focus_manager->GetCurrentTargetForAccelerator(return_accelerator));

  // Hitting the return key again; nothing should happen.
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(target.accelerator_count(), 1);
}

class FocusManagerDtorTest : public FocusManagerTest {
 protected:
  typedef std::vector<std::string> DtorTrackVector;

  class FocusManagerDtorTracked : public FocusManager {
   public:
    FocusManagerDtorTracked(Widget* widget, DtorTrackVector* dtor_tracker)
      : FocusManager(widget),
        dtor_tracker_(dtor_tracker) {
    }

    virtual ~FocusManagerDtorTracked() {
      dtor_tracker_->push_back("FocusManagerDtorTracked");
    }

    DtorTrackVector* dtor_tracker_;

   private:
    DISALLOW_COPY_AND_ASSIGN(FocusManagerDtorTracked);
  };

  class TestFocusManagerFactory : public FocusManagerFactory {
   public:
    explicit TestFocusManagerFactory(DtorTrackVector* dtor_tracker)
        : dtor_tracker_(dtor_tracker) {
    }

    FocusManager* CreateFocusManager(Widget* widget) OVERRIDE {
      return new FocusManagerDtorTracked(widget, dtor_tracker_);
    }

   private:
    DtorTrackVector* dtor_tracker_;
    DISALLOW_COPY_AND_ASSIGN(TestFocusManagerFactory);
  };

  class NativeButtonDtorTracked : public NativeTextButton {
   public:
    NativeButtonDtorTracked(const string16& text,
                            DtorTrackVector* dtor_tracker)
        : NativeTextButton(NULL, text),
          dtor_tracker_(dtor_tracker) {
    };
    virtual ~NativeButtonDtorTracked() {
      dtor_tracker_->push_back("NativeButtonDtorTracked");
    }

    DtorTrackVector* dtor_tracker_;
  };

  class WindowDtorTracked : public Widget {
   public:
    explicit WindowDtorTracked(DtorTrackVector* dtor_tracker)
        : dtor_tracker_(dtor_tracker) {
    }

    virtual ~WindowDtorTracked() {
      dtor_tracker_->push_back("WindowDtorTracked");
    }

    DtorTrackVector* dtor_tracker_;
  };

  virtual void SetUp() {
    ViewsTestBase::SetUp();
    FocusManagerFactory::Install(new TestFocusManagerFactory(&dtor_tracker_));
    // Create WindowDtorTracked that uses FocusManagerDtorTracked.
    Widget* widget = new WindowDtorTracked(&dtor_tracker_);
    Widget::InitParams params;
    params.delegate = this;
    params.bounds = gfx::Rect(0, 0, 100, 100);
    widget->Init(params);

    tracked_focus_manager_ =
        static_cast<FocusManagerDtorTracked*>(GetFocusManager());
    widget->Show();
  }

  virtual void TearDown() {
    FocusManagerFactory::Install(NULL);
    ViewsTestBase::TearDown();
  }

  FocusManager* tracked_focus_manager_;
  DtorTrackVector dtor_tracker_;
};

#if !defined(USE_AURA)
TEST_F(FocusManagerDtorTest, FocusManagerDestructedLast) {
  // Setup views hierarchy.
  TabbedPane* tabbed_pane = new TabbedPane();
  GetContentsView()->AddChildView(tabbed_pane);

  NativeButtonDtorTracked* button = new NativeButtonDtorTracked(
      ASCIIToUTF16("button"), &dtor_tracker_);
  tabbed_pane->AddTab(ASCIIToUTF16("Awesome tab"), button);

  // Close the window.
  GetWidget()->Close();
  RunPendingMessages();

  // Test window, button and focus manager should all be destructed.
  ASSERT_EQ(3, static_cast<int>(dtor_tracker_.size()));

  // Focus manager should be the last one to destruct.
  ASSERT_STREQ("FocusManagerDtorTracked", dtor_tracker_[2].c_str());
}
#endif

}  // namespace views

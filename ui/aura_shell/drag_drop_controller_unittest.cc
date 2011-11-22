// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/drag_drop_controller.h"

#include "base/location.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura_shell/desktop_event_filter.h"
#include "ui/aura_shell/test/aura_shell_test_base.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/widget.h"
#include "views/view.h"

namespace aura_shell {
namespace test {

namespace {

// A simple view that makes sure RunShellDrag is invoked on mouse drag.
class DragTestView : public views::View {
 public:
  DragTestView() : views::View() {
    Reset();
  }

  void Reset() {
    num_drag_enters_ = 0;
    num_drag_exits_ = 0;
    num_drag_updates_ = 0;
    num_drops_ = 0;
    drag_done_received_ = false;
  }

  int VerticalDragThreshold() {
    return views::View::GetVerticalDragThreshold();
  }

  int HorizontalDragThreshold() {
    return views::View::GetHorizontalDragThreshold();
  }

  int num_drag_enters_;
  int num_drag_exits_;
  int num_drag_updates_;
  int num_drops_;
  bool drag_done_received_;

 private:
  // View overrides:
  int GetDragOperations(const gfx::Point& press_pt) OVERRIDE {
    return ui::DragDropTypes::DRAG_COPY;
  }

  void WriteDragData(const gfx::Point& p, OSExchangeData* data) OVERRIDE {
    data->SetString(UTF8ToUTF16("I am being dragged"));
  }

  bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    return true;
  }

  bool GetDropFormats(int* formats,
                      std::set<OSExchangeData::CustomFormat>* custom_formats) {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }

  bool CanDrop(const OSExchangeData& data) OVERRIDE {
    return true;
  }

  void OnDragEntered(const views::DropTargetEvent& event) OVERRIDE {
    num_drag_enters_++;
  }

  int OnDragUpdated(const views::DropTargetEvent& event) OVERRIDE {
    num_drag_updates_++;
    return ui::DragDropTypes::DRAG_COPY;
  }

  void OnDragExited() OVERRIDE {
    num_drag_exits_++;
  }

  int OnPerformDrop(const views::DropTargetEvent& event) OVERRIDE {
    num_drops_++;
    return ui::DragDropTypes::DRAG_COPY;
  }

  void OnDragDone() OVERRIDE {
    drag_done_received_ = true;
  }

  DISALLOW_COPY_AND_ASSIGN(DragTestView);
};

class TestDragDropController : public internal::DragDropController {
 public:
  TestDragDropController() : internal::DragDropController() {
    Reset();
  }

  void Reset() {
    drag_start_received_ = false;
    num_drag_updates_ = 0;
    drop_received_ = false;
    drag_string_.clear();
  }

  bool drag_start_received_;
  int num_drag_updates_;
  bool drop_received_;
  string16 drag_string_;

 private:
  void StartDragAndDrop(const ui::OSExchangeData& data,
                        int operation) OVERRIDE {
    DragDropController::StartDragAndDrop(data, operation);
    drag_start_received_ = true;
    data.GetString(&drag_string_);
  }

  void DragUpdate(aura::Window* target,
                  const aura::MouseEvent& event) OVERRIDE {
    DragDropController::DragUpdate(target, event);
    num_drag_updates_++;
  }

  void Drop(aura::Window* target, const aura::MouseEvent& event) OVERRIDE {
    DragDropController::Drop(target, event);
    drop_received_ = true;
  }

  DISALLOW_COPY_AND_ASSIGN(TestDragDropController);
};

views::Widget* CreateNewWidget() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = aura::Desktop::GetInstance();
  params.child = true;
  widget->Init(params);
  widget->Show();
  return widget;
}

void AddViewToWidgetAndResize(views::Widget* widget, views::View* view) {
  if (!widget->GetContentsView()) {
    views::View* contents_view = new views::View;
    widget->SetContentsView(contents_view);
  }

  views::View* contents_view = widget->GetContentsView();
  contents_view->AddChildView(view);
  view->SetBounds(contents_view->width(), 0, 100, 100);
  gfx::Rect contents_view_bounds = contents_view->bounds();
  contents_view_bounds = contents_view_bounds.Union(view->bounds());
  contents_view->SetBoundsRect(contents_view_bounds);
  widget->SetBounds(contents_view_bounds);
}

}  // namespace

class DragDropControllerTest : public AuraShellTestBase {
 public:
  DragDropControllerTest() : AuraShellTestBase() {
  }

  virtual ~DragDropControllerTest() {
    aura::Desktop::GetInstance()->SetProperty(aura::kDesktopDragDropClientKey,
        NULL);
  }

  void SetUp() OVERRIDE {
    AuraShellTestBase::SetUp();
    drag_drop_controller_ = new TestDragDropController;
    drag_drop_controller_->set_should_block_during_drag_drop(false);
    aura::Desktop::GetInstance()->SetProperty(aura::kDesktopDragDropClientKey,
        drag_drop_controller_);
  }

  void TearDown() OVERRIDE {
    delete drag_drop_controller_;
    drag_drop_controller_ = NULL;
    AuraShellTestBase::TearDown();
  }

  void UpdateDragData(ui::OSExchangeData* data) {
    drag_drop_controller_->drag_data_ = data;
  }

 protected:
  TestDragDropController* drag_drop_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DragDropControllerTest);
};

TEST_F(DragDropControllerTest, DragDropInSingleViewTest) {
  views::Widget* widget = CreateNewWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget, drag_view);
  gfx::Point point = gfx::Rect(drag_view->bounds()).CenterPoint();
  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::MouseEvent event1(ui::ET_MOUSE_PRESSED, point, ui::EF_LEFT_BUTTON_DOWN);
  aura::Desktop::GetInstance()->DispatchMouseEvent(&event1);

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    point.Offset(0, 1);
    aura::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, point,
        ui::EF_LEFT_BUTTON_DOWN);
    aura::Desktop::GetInstance()->DispatchMouseEvent(&drag_event);
  }

  aura::MouseEvent event2(ui::ET_MOUSE_RELEASED, point, 0);
  aura::Desktop::GetInstance()->DispatchMouseEvent(&event2);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
      drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
  delete widget;
}

TEST_F(DragDropControllerTest, DragDropInMultipleViewsSingleWidgetTest) {
  views::Widget* widget = CreateNewWidget();
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget, drag_view1);
  gfx::Point point = drag_view1->bounds().CenterPoint();
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget, drag_view2);

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::MouseEvent event1(ui::ET_MOUSE_PRESSED, point, ui::EF_LEFT_BUTTON_DOWN);
  aura::Desktop::GetInstance()->DispatchMouseEvent(&event1);

  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    point.Offset(1, 0);
    aura::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, point,
        ui::EF_LEFT_BUTTON_DOWN);
    aura::Desktop::GetInstance()->DispatchMouseEvent(&drag_event);
  }

  aura::MouseEvent event2(ui::ET_MOUSE_RELEASED, point, 0);
  aura::Desktop::GetInstance()->DispatchMouseEvent(&event2);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates = drag_view1->bounds().width() -
      drag_view1->bounds().CenterPoint().x() - 2;
  EXPECT_EQ(num_expected_updates - drag_view1->HorizontalDragThreshold(),
      drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates - 1;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
  delete widget;
}

TEST_F(DragDropControllerTest, DragDropInMultipleViewsMultipleWidgetsTest) {
  views::Widget* widget1 = CreateNewWidget();
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget1, drag_view1);
  gfx::Point point = drag_view1->bounds().CenterPoint();
  views::Widget* widget2 = CreateNewWidget();
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget2, drag_view2);
  gfx::Rect widget1_bounds = widget1->GetClientAreaScreenBounds();
  gfx::Rect widget2_bounds = widget2->GetClientAreaScreenBounds();
  widget2->SetBounds(gfx::Rect(widget1_bounds.width(), 0,
      widget2_bounds.width(), widget2_bounds.height()));

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::MouseEvent event1(ui::ET_MOUSE_PRESSED, point, ui::EF_LEFT_BUTTON_DOWN);
  aura::Desktop::GetInstance()->DispatchMouseEvent(&event1);

  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    point.Offset(1, 0);
    aura::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, point,
        ui::EF_LEFT_BUTTON_DOWN);
    aura::Desktop::GetInstance()->DispatchMouseEvent(&drag_event);
  }

  aura::MouseEvent event2(ui::ET_MOUSE_RELEASED, point, 0);
  aura::Desktop::GetInstance()->DispatchMouseEvent(&event2);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates = drag_view1->bounds().width() -
      drag_view1->bounds().CenterPoint().x() - 2;
  EXPECT_EQ(num_expected_updates - drag_view1->HorizontalDragThreshold(),
      drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates - 2;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
  delete widget1;
  delete widget2;
}

TEST_F(DragDropControllerTest, ViewRemovedWhileInDragDropTest) {
  views::Widget* widget = CreateNewWidget();
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget, drag_view);
  gfx::Point point = gfx::Rect(drag_view->bounds()).CenterPoint();
  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::MouseEvent event1(ui::ET_MOUSE_PRESSED, point, ui::EF_LEFT_BUTTON_DOWN);
  aura::Desktop::GetInstance()->DispatchMouseEvent(&event1);

  int num_drags_1 = 17;
  for (int i = 0; i < num_drags_1; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    point.Offset(0, 1);
    aura::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, point,
        ui::EF_LEFT_BUTTON_DOWN);
    aura::Desktop::GetInstance()->DispatchMouseEvent(&drag_event);
  }

  drag_view->parent()->RemoveChildView(drag_view);
  // View has been removed. We will not get any of the following drag updates.
  int num_drags_2 = 23;
  for (int i = 0; i < num_drags_2; ++i) {
    UpdateDragData(&data);
    point.Offset(0, 1);
    aura::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, point,
        ui::EF_LEFT_BUTTON_DOWN);
    aura::Desktop::GetInstance()->DispatchMouseEvent(&drag_event);
  }

  aura::MouseEvent event2(ui::ET_MOUSE_RELEASED, point, 0);
  aura::Desktop::GetInstance()->DispatchMouseEvent(&event2);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags_1 + num_drags_2 - 1 - drag_view->VerticalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags_1 - 1 - drag_view->VerticalDragThreshold(),
      drag_view->num_drag_updates_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
  delete widget;
}

}  // namespace test
}  // namespace aura

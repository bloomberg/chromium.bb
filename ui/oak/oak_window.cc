// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/oak/oak_window.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace oak {
namespace internal {
namespace {

const SkColor kBorderColor = SkColorSetRGB(0xCC, 0xCC, 0xCC);

class DetailsView : public views::View {
 public:
  DetailsView() {}
  virtual ~DetailsView() {}

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->sk_canvas()->drawColor(SK_ColorYELLOW);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DetailsView);
};

}  // namespace

// static
views::Widget* OakWindow::instance = NULL;

////////////////////////////////////////////////////////////////////////////////
// OakWindow, public:

OakWindow::OakWindow() : tree_(NULL), tree_container_(NULL), details_(NULL) {
}

OakWindow::~OakWindow() {
  // The tree needs to be destroyed before the model.
  tree_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// OakWindow, views::WidgetDelegateView implementation:

bool OakWindow::CanResize() const {
  return true;
}

bool OakWindow::CanMaximize() const {
  return true;
}

string16 OakWindow::GetWindowTitle() const {
  return ASCIIToUTF16("Oak");
}

views::View* OakWindow::GetContentsView() {
  return this;
}

SkBitmap OakWindow::GetWindowIcon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return *rb.GetImageNamed(IDR_OAK).ToSkBitmap();
}

bool OakWindow::ShouldShowWindowIcon() const {
  return true;
}

void OakWindow::DeleteDelegate() {
  instance = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// OakWindow, views::View overrides:

void OakWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->sk_canvas()->drawColor(SK_ColorWHITE);
  canvas->FillRect(separator_rect_, kBorderColor);
}

void OakWindow::ViewHierarchyChanged(bool is_add,
                                     views::View* parent,
                                     views::View* child) {
  if (is_add && child == this)
    Init();
}

void OakWindow::Layout() {
  gfx::Rect content_bounds = GetLocalBounds();
  content_bounds.Inset(views::kPanelHorizMargin, views::kPanelVertMargin);

  int tree_height =
      (content_bounds.height() / 2) - views::kUnrelatedControlVerticalSpacing;
  gfx::Rect tree_bounds = content_bounds;
  tree_bounds.set_height(tree_height);
  tree_container_->SetBoundsRect(tree_bounds);

  separator_rect_ = content_bounds;
  separator_rect_.set_y(
      tree_bounds.bottom() + views::kRelatedControlVerticalSpacing);
  separator_rect_.set_height(1);

  gfx::Rect details_bounds = content_bounds;
  details_bounds.set_y(
      separator_rect_.bottom() + views::kRelatedControlVerticalSpacing);
  details_bounds.set_height(content_bounds.bottom() - details_bounds.y());
  details_->SetBoundsRect(details_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// OakWindow, views::TreeViewController implementation:

void OakWindow::OnTreeViewSelectionChanged(views::TreeView* tree) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// OakWindow, private:

void OakWindow::Init() {
  tree_model_.reset(
      GenerateModel(GetWidget()->GetNativeView()->GetRootWindow()));
  tree_.reset(new views::TreeView);
  tree_->set_parent_owned(false);
  tree_->SetController(this);
  tree_->SetModel(tree_model_.get());
  tree_container_ = tree_->CreateParentIfNecessary();
  AddChildView(tree_container_);
  details_ = new DetailsView;
  AddChildView(details_);
}

}  // namespace internal

void ShowOakWindow() {
  if (!internal::OakWindow::instance) {
    internal::OakWindow::instance =
        views::Widget::CreateWindowWithBounds(new internal::OakWindow,
                                              gfx::Rect(10, 10, 500, 500));
  }
  internal::OakWindow::instance->Show();
}

}  // namespace oak

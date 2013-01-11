// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_frame_view.h"

#include "grit/ui_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/insets.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// The base spacing value, multiples of which are used in various places.
const int kSpacing = 10;

// static
const char kViewClassName[] = "ui/views/DialogFrameView";

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, public:

DialogFrameView::DialogFrameView(const string16& title)
    : title_(NULL),
      close_(NULL) {
  BubbleBorder* border =
      new BubbleBorder(BubbleBorder::FLOAT, BubbleBorder::SMALL_SHADOW);
  border->set_background_color(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground));
  set_border(border);
  // Update the background, which relies on the border.
  set_background(new BubbleBackground(border));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_ = new Label(title, rb.GetFont(ui::ResourceBundle::MediumFont));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(title_);

  close_ = new LabelButton(this, string16());
  close_->SetImage(CustomButton::STATE_NORMAL,
                   *rb.GetImageNamed(IDR_CLOSE_DIALOG).ToImageSkia());
  close_->SetImage(CustomButton::STATE_HOVERED,
                   *rb.GetImageNamed(IDR_CLOSE_DIALOG_H).ToImageSkia());
  close_->SetImage(CustomButton::STATE_PRESSED,
                   *rb.GetImageNamed(IDR_CLOSE_DIALOG_P).ToImageSkia());
  close_->SetSize(close_->GetPreferredSize());
  close_->set_border(NULL);
  AddChildView(close_);

  // Set the margins for the content view.
  content_margins_ = gfx::Insets(2 * kSpacing + title_->font().GetHeight(),
                                 2 * kSpacing, 2 * kSpacing, 2 * kSpacing);
}

DialogFrameView::~DialogFrameView() {
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, NonClientFrameView:

gfx::Rect DialogFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = GetLocalBounds();
  client_bounds.Inset(GetClientInsets());
  return client_bounds;
}

gfx::Rect DialogFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(-GetClientInsets());
  return window_bounds;
}

int DialogFrameView::NonClientHitTest(const gfx::Point& point) {
  if (close_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  return point.y() < GetClientInsets().top() ? HTCAPTION : HTCLIENT;
}

void DialogFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
}

void DialogFrameView::ResetWindowControls() {
}

void DialogFrameView::UpdateWindowIcon() {
}

void DialogFrameView::UpdateWindowTitle() {
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, View overrides:

std::string DialogFrameView::GetClassName() const {
  return kViewClassName;
}

void DialogFrameView::Layout() {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(border()->GetInsets());
  // Small additional insets yield the desired 10px visual close button insets.
  bounds.Inset(0, 2, 1, 0);
  close_->SetPosition(gfx::Point(bounds.right() - close_->width(), bounds.y()));
  // Small additional insets yield the desired 20px visual title label insets.
  bounds.Inset(2 * kSpacing - 1, kSpacing, close_->width(), 0);
  bounds.set_height(title_->font().GetHeight());
  title_->SetBoundsRect(bounds);
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, ButtonListener overrides:

void DialogFrameView::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == close_)
    GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, private:

gfx::Insets DialogFrameView::GetClientInsets() const {
  gfx::Insets insets = border()->GetInsets();
  insets += content_margins_;
  return insets;
}

}  // namespace views

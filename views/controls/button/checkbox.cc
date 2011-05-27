// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/checkbox.h"

#include "base/logging.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas.h"
#include "views/controls/label.h"

namespace views {

// static
const char Checkbox::kViewClassName[] = "views/Checkbox";

// static
const char CheckboxNt::kViewClassName[] = "views/CheckboxNt";

static const int kCheckboxLabelSpacing = 4;
static const int kLabelFocusPaddingHorizontal = 2;
static const int kLabelFocusPaddingVertical = 1;

////////////////////////////////////////////////////////////////////////////////
// Checkbox, public:

Checkbox::Checkbox() : NativeButtonBase(NULL), checked_(false) {
  Init(std::wstring());
}

Checkbox::Checkbox(const std::wstring& label)
    : NativeButtonBase(NULL, label),
      checked_(false) {
  Init(label);
}

Checkbox::~Checkbox() {
}

// static
int Checkbox::GetTextIndent() {
  return NativeButtonWrapper::GetFixedWidth() + kCheckboxLabelSpacing;
}

void Checkbox::SetMultiLine(bool multiline) {
  label_->SetMultiLine(multiline);
  PreferredSizeChanged();
}

void Checkbox::SetChecked(bool checked) {
  if (checked_ == checked)
    return;
  checked_ = checked;
  if (native_wrapper_)
    native_wrapper_->UpdateChecked();
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, View overrides:

gfx::Size Checkbox::GetPreferredSize() {
  if (!native_wrapper_)
    return gfx::Size();

  gfx::Size prefsize = native_wrapper_->GetView()->GetPreferredSize();
  if (native_wrapper_->UsesNativeLabel())
    return prefsize;

  prefsize.set_width(
      prefsize.width() + kCheckboxLabelSpacing +
          kLabelFocusPaddingHorizontal * 2);
  gfx::Size label_prefsize = label_->GetPreferredSize();
  prefsize.set_width(prefsize.width() + label_prefsize.width());
  prefsize.set_height(
      std::max(prefsize.height(),
               label_prefsize.height() + kLabelFocusPaddingVertical * 2));
  return prefsize;
}

int Checkbox::GetHeightForWidth(int w) {
  if (!native_wrapper_)
    return 0;

  gfx::Size prefsize = native_wrapper_->GetView()->GetPreferredSize();
  if (native_wrapper_->UsesNativeLabel())
    return prefsize.height();

  int width = prefsize.width() + kCheckboxLabelSpacing +
              kLabelFocusPaddingHorizontal * 2;
  return label_->GetHeightForWidth(std::max(prefsize.height(), w - width));
}

void Checkbox::OnEnabledChanged() {
  NativeButtonBase::OnEnabledChanged();
  if (label_)
    label_->SetEnabled(IsEnabled());
}

void Checkbox::Layout() {
  if (!native_wrapper_)
    return;

  if (native_wrapper_->UsesNativeLabel()) {
    label_->SetBounds(0, 0, 0, 0);
    label_->SetVisible(false);
    native_wrapper_->GetView()->SetBounds(0, 0, width(), height());
  } else {
    gfx::Size checkmark_prefsize =
        native_wrapper_->GetView()->GetPreferredSize();
    int label_x = checkmark_prefsize.width() + kCheckboxLabelSpacing +
        kLabelFocusPaddingHorizontal;
    label_->SetBounds(
        label_x, 0, std::max(0, width() - label_x -
            kLabelFocusPaddingHorizontal),
        height());
    int first_line_height = label_->font().GetHeight();
    native_wrapper_->GetView()->SetBounds(
        0, ((first_line_height - checkmark_prefsize.height()) / 2),
        checkmark_prefsize.width(), checkmark_prefsize.height());
  }
  native_wrapper_->GetView()->Layout();
}

std::string Checkbox::GetClassName() const {
  return kViewClassName;
}

bool Checkbox::OnMousePressed(const MouseEvent& event) {
  native_wrapper_->SetPushed(HitTestLabel(event));
  return true;
}

bool Checkbox::OnMouseDragged(const MouseEvent& event) {
  return false;
}

void Checkbox::OnMouseReleased(const MouseEvent& event) {
  OnMouseCaptureLost();
  if (HitTestLabel(event)) {
    SetChecked(!checked());
    ButtonPressed();
  }
}

void Checkbox::OnMouseCaptureLost() {
  native_wrapper_->SetPushed(false);
}

void Checkbox::OnMouseMoved(const MouseEvent& event) {
  native_wrapper_->SetPushed(HitTestLabel(event));
}

void Checkbox::OnMouseEntered(const MouseEvent& event) {
  native_wrapper_->SetPushed(HitTestLabel(event));
}

void Checkbox::OnMouseExited(const MouseEvent& event) {
  native_wrapper_->SetPushed(false);
}

void Checkbox::GetAccessibleState(ui::AccessibleViewState* state) {
  Button::GetAccessibleState(state);
  state->role = ui::AccessibilityTypes::ROLE_CHECKBUTTON;
  state->state = checked() ? ui::AccessibilityTypes::STATE_CHECKED : 0;
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, NativeButton overrides:

void Checkbox::SetLabel(const std::wstring& label) {
  NativeButtonBase::SetLabel(label);
  if (!native_wrapper_->UsesNativeLabel())
    label_->SetText(label);
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, protected:

bool Checkbox::HitTestLabel(const MouseEvent& event) {
  gfx::Point tmp(event.location());
  ConvertPointToView(this, label_, &tmp);
  return label_->HitTest(tmp);
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, View overrides, protected:

void Checkbox::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Our focus border is rendered by the label, so we don't do anything here.
}
void Checkbox::OnFocus() {
  NativeButtonBase::OnFocus();
  label_->set_paint_as_focused(true);
}

void Checkbox::OnBlur() {
  label_->set_paint_as_focused(false);
}


////////////////////////////////////////////////////////////////////////////////
// Checkbox, NativeButton overrides, protected:

NativeButtonWrapper* Checkbox::CreateWrapper() {
  return NativeButtonWrapper::CreateCheckboxWrapper(this);
}

void Checkbox::InitBorder() {
  // No border, so we do nothing.
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, private:

void Checkbox::Init(const std::wstring& label_text) {
  // Checkboxs don't need to enforce a minimum size.
  set_ignore_minimum_size(true);
  label_ = new Label(label_text);
  label_->SetHasFocusBorder(true);
  label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(label_);
}

////////////////////////////////////////////////////////////////////////////////
// CheckboxNt, public:

CheckboxNt::CheckboxNt(const std::wstring& label)
    : TextButtonBase(NULL, label),
      checked_(false) {
  set_border(new TextButtonNativeThemeBorder(this));
}

CheckboxNt::~CheckboxNt() {
}

void CheckboxNt::SetChecked(bool checked) {
  checked_ = checked;
  SchedulePaint();
}

gfx::Size CheckboxNt::GetPreferredSize() {
  gfx::Size prefsize(TextButtonBase::GetPreferredSize());
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State state = GetThemeState(&extra);
  gfx::Size size = gfx::NativeTheme::instance()->GetPartSize(GetThemePart(),
                                                             state,
                                                             extra);
  prefsize.Enlarge(size.width(), 0);
  prefsize.set_height(std::max(prefsize.height(), size.height()));

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

  return prefsize;
}

std::string CheckboxNt::GetClassName() const {
  return kViewClassName;
}

void CheckboxNt::GetAccessibleState(ui::AccessibleViewState* state) {
  TextButtonBase::GetAccessibleState(state);
  state->role = ui::AccessibilityTypes::ROLE_CHECKBUTTON;
  state->state = checked() ? ui::AccessibilityTypes::STATE_CHECKED : 0;
}

void CheckboxNt::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (IsFocusable() || IsAccessibilityFocusableInRootView())) {
    gfx::Rect bounds(GetTextBounds());
    // Increate the bounding box by one on each side so that that focus border
    // does not draw on top of the letters.
    bounds.Inset(-1, -1, -1, -1);
    canvas->DrawFocusRect(bounds.x(), bounds.y(), bounds.width(),
                          bounds.height());
  }
}

void CheckboxNt::NotifyClick(const views::Event& event) {
  SetChecked(!checked());
  RequestFocus();
  TextButtonBase::NotifyClick(event);
}

gfx::NativeTheme::Part CheckboxNt::GetThemePart() const {
  return gfx::NativeTheme::kCheckbox;
}

gfx::Rect CheckboxNt::GetThemePaintRect() const {
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State state = GetThemeState(&extra);
  gfx::Size size(gfx::NativeTheme::instance()->GetPartSize(GetThemePart(),
                                                           state,
                                                           extra));
  gfx::Insets insets = GetInsets();
  gfx::Rect rect(insets.left(), 0, size.width(), height());
  rect.set_x(GetMirroredXForRect(rect));
  return rect;
}

void CheckboxNt::GetExtraParams(gfx::NativeTheme::ExtraParams* params) const {
  TextButtonBase::GetExtraParams(params);
  params->button.is_default = false;
  params->button.checked = checked_;
}

gfx::Rect CheckboxNt::GetTextBounds() const {
  gfx::Rect bounds(TextButtonBase::GetTextBounds());
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State state = GetThemeState(&extra);
  gfx::Size size(gfx::NativeTheme::instance()->GetPartSize(GetThemePart(),
                                                           state,
                                                           extra));
  bounds.Offset(size.width() + kCheckboxLabelSpacing, 0);
  return bounds;
}

}  // namespace views

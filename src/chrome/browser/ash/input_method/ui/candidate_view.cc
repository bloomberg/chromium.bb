// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/candidate_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/ui/candidate_window_constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace ime {

namespace {

// VerticalCandidateLabel is used for rendering candidate text in
// the vertical candidate window.
class VerticalCandidateLabel : public views::Label {
 public:
  METADATA_HEADER(VerticalCandidateLabel);
  VerticalCandidateLabel() = default;
  VerticalCandidateLabel(const VerticalCandidateLabel&) = delete;
  VerticalCandidateLabel& operator=(const VerticalCandidateLabel&) = delete;
  ~VerticalCandidateLabel() override = default;

 private:
  // views::Label:
  // Returns the preferred size, but guarantees that the width has at
  // least kMinCandidateLabelWidth pixels.
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = Label::CalculatePreferredSize();
    size.SetToMax(gfx::Size(kMinCandidateLabelWidth, 0));
    size.SetToMin(gfx::Size(kMaxCandidateLabelWidth, size.height()));
    return size;
  }
};

BEGIN_METADATA(VerticalCandidateLabel, views::Label)
END_METADATA

// The label text is not set in this class.
class ShortcutLabel : public views::Label {
 public:
  METADATA_HEADER(ShortcutLabel);
  explicit ShortcutLabel(ui::CandidateWindow::Orientation orientation)
      : orientation_(orientation) {
    // TODO(tapted): Get this FontList from views::style.
    if (orientation == ui::CandidateWindow::VERTICAL) {
      SetFontList(font_list().Derive(kFontSizeDelta, gfx::Font::NORMAL,
                                     gfx::Font::Weight::BOLD));
    } else {
      SetFontList(font_list().DeriveWithSizeDelta(kFontSizeDelta));
    }
    // TODO(satorux): Maybe we need to use language specific fonts for
    // candidate_label, like Chinese font for Chinese input method?

    // Setup paddings.
    const gfx::Insets kVerticalShortcutLabelInsets(1, 6, 1, 6);
    const gfx::Insets kHorizontalShortcutLabelInsets(1, 3, 1, 0);
    const gfx::Insets insets = (orientation == ui::CandidateWindow::VERTICAL
                                    ? kVerticalShortcutLabelInsets
                                    : kHorizontalShortcutLabelInsets);
    SetBorder(views::CreateEmptyBorder(insets.top(), insets.left(),
                                       insets.bottom(), insets.right()));

    SetElideBehavior(gfx::NO_ELIDE);
  }
  ShortcutLabel(const ShortcutLabel&) = delete;
  ShortcutLabel& operator=(const ShortcutLabel&) = delete;
  ~ShortcutLabel() override = default;

  // views::Label:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    // Add decoration based on the orientation.
    if (orientation_ == ui::CandidateWindow::VERTICAL) {
      // Set the background color.
      SkColor blackish = color_utils::AlphaBlend(
          SK_ColorBLACK,
          GetColorProvider()->GetColor(ui::kColorWindowBackground), 0.25f);
      SetBackground(views::CreateSolidBackground(SkColorSetA(blackish, 0xE0)));
    }
  }

 private:
  const ui::CandidateWindow::Orientation orientation_;
};

BEGIN_METADATA(ShortcutLabel, views::Label)
END_METADATA

// The label text is not set in this class.
class AnnotationLabel : public views::Label {
 public:
  METADATA_HEADER(AnnotationLabel);
  AnnotationLabel() {
    // Change the font size and color.
    SetFontList(font_list().DeriveWithSizeDelta(kFontSizeDelta));
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetElideBehavior(gfx::NO_ELIDE);
  }
  AnnotationLabel(const AnnotationLabel&) = delete;
  AnnotationLabel& operator=(const AnnotationLabel&) = delete;
  ~AnnotationLabel() override = default;

  // views::Label:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    SetEnabledColor(
        GetColorProvider()->GetColor(ui::kColorLabelForegroundSecondary));
  }
};

BEGIN_METADATA(AnnotationLabel, views::Label)
END_METADATA

// Creates the candidate label, and returns it (never returns nullptr).
// The label text is not set in this function.
std::unique_ptr<views::Label> CreateCandidateLabel(
    ui::CandidateWindow::Orientation orientation) {
  std::unique_ptr<views::Label> candidate_label;

  if (orientation == ui::CandidateWindow::VERTICAL) {
    candidate_label = std::make_unique<VerticalCandidateLabel>();
  } else {
    candidate_label = std::make_unique<views::Label>();
  }

  // Change the font size.
  candidate_label->SetFontList(
      candidate_label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  candidate_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  candidate_label->SetElideBehavior(gfx::NO_ELIDE);

  return candidate_label;
}

}  // namespace

CandidateView::CandidateView(PressedCallback callback,
                             ui::CandidateWindow::Orientation orientation)
    : views::Button(std::move(callback)), orientation_(orientation) {
  SetBorder(views::CreateEmptyBorder(1, 1, 1, 1));

  shortcut_label_ = AddChildView(std::make_unique<ShortcutLabel>(orientation));
  candidate_label_ = AddChildView(CreateCandidateLabel(orientation));
  annotation_label_ = AddChildView(std::make_unique<AnnotationLabel>());

  if (orientation == ui::CandidateWindow::VERTICAL)
    infolist_icon_ = AddChildView(std::make_unique<views::View>());

  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  SetProperty(views::kSkipAccessibilityPaintChecks, true);
}

void CandidateView::GetPreferredWidths(int* shortcut_width,
                                       int* candidate_width) {
  *shortcut_width = shortcut_label_->GetPreferredSize().width();
  *candidate_width = candidate_label_->GetPreferredSize().width();
}

void CandidateView::SetWidths(int shortcut_width, int candidate_width) {
  shortcut_width_ = shortcut_width;
  shortcut_label_->SetVisible(shortcut_width_ != 0);
  candidate_width_ = candidate_width;
}

void CandidateView::SetEntry(const ui::CandidateWindow::Entry& entry) {
  std::u16string label = entry.label;
  if (!label.empty() && orientation_ != ui::CandidateWindow::VERTICAL)
    label += u".";
  shortcut_label_->SetText(label);
  candidate_label_->SetText(entry.value);
  annotation_label_->SetText(entry.annotation);
  SetAccessibleName(entry.value);
}

void CandidateView::SetInfolistIcon(bool enable) {
  if (infolist_icon_)
    infolist_icon_->SetVisible(enable);
  SchedulePaint();
}

void CandidateView::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted)
    return;

  highlighted_ = highlighted;
  if (highlighted) {
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, false);
    const ui::ColorProvider* color_provider = GetColorProvider();
    SetBackground(views::CreateSolidBackground(
        color_provider->GetColor(ui::kColorTextfieldSelectionBackground)));
    SetBorder(views::CreateSolidBorder(
        1, color_provider->GetColor(ui::kColorFocusableBorderFocused)));

    // Cancel currently focused one.
    for (View* view : parent()->children()) {
      if (view != this)
        static_cast<CandidateView*>(view)->SetHighlighted(false);
    }
  } else {
    SetBackground(nullptr);
    SetBorder(views::CreateEmptyBorder(1, 1, 1, 1));
  }
  SchedulePaint();
}

void CandidateView::StateChanged(ButtonState old_state) {
  Button::StateChanged(old_state);
  int text_style = GetState() == STATE_DISABLED ? views::style::STYLE_DISABLED
                                                : views::style::STYLE_PRIMARY;
  shortcut_label_->SetEnabledColor(views::style::GetColor(
      *shortcut_label_, views::style::CONTEXT_LABEL, text_style));
  if (GetState() == STATE_PRESSED)
    SetHighlighted(true);
}

bool CandidateView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location())) {
    // Moves the drag target to the sibling view.
    gfx::Point location_in_widget(event.location());
    ConvertPointToWidget(this, &location_in_widget);
    for (View* view : parent()->children()) {
      if (view == this)
        continue;
      gfx::Point location_in_sibling(location_in_widget);
      ConvertPointFromWidget(view, &location_in_sibling);
      if (view->HitTestPoint(location_in_sibling)) {
        GetWidget()->GetRootView()->SetMouseAndGestureHandler(view);
        auto* sibling = static_cast<CandidateView*>(view);
        sibling->SetHighlighted(true);
        return view->OnMouseDragged(ui::MouseEvent(event, this, sibling));
      }
    }

    return false;
  }

  return views::Button::OnMouseDragged(event);
}

void CandidateView::Layout() {
  const int padding_width =
      orientation_ == ui::CandidateWindow::VERTICAL ? 4 : 6;
  int x = 0;
  shortcut_label_->SetBounds(x, 0, shortcut_width_, height());
  if (shortcut_width_ > 0)
    x += shortcut_width_ + padding_width;
  candidate_label_->SetBounds(x, 0, candidate_width_, height());
  x += candidate_width_ + padding_width;

  int right = bounds().right();
  if (infolist_icon_ && infolist_icon_->GetVisible()) {
    infolist_icon_->SetBounds(
        right - kInfolistIndicatorIconWidth - kInfolistIndicatorIconPadding,
        kInfolistIndicatorIconPadding, kInfolistIndicatorIconWidth,
        height() - kInfolistIndicatorIconPadding * 2);
    right -= kInfolistIndicatorIconWidth + kInfolistIndicatorIconPadding * 2;
  }
  annotation_label_->SetBounds(x, 0, right - x, height());
}

gfx::Size CandidateView::CalculatePreferredSize() const {
  const int padding_width =
      orientation_ == ui::CandidateWindow::VERTICAL ? 4 : 6;
  gfx::Size size;
  if (shortcut_label_->GetVisible()) {
    size = shortcut_label_->GetPreferredSize();
    size.SetToMax(gfx::Size(shortcut_width_, 0));
    size.Enlarge(padding_width, 0);
  }
  gfx::Size candidate_size = candidate_label_->GetPreferredSize();
  candidate_size.SetToMax(gfx::Size(candidate_width_, 0));
  size.Enlarge(candidate_size.width() + padding_width, 0);
  size.SetToMax(candidate_size);
  if (annotation_label_->GetVisible()) {
    gfx::Size annotation_size = annotation_label_->GetPreferredSize();
    size.Enlarge(annotation_size.width() + padding_width, 0);
    size.SetToMax(annotation_size);
  }

  // Reserves the margin for infolist_icon even if it's not visible.
  size.Enlarge(kInfolistIndicatorIconWidth + kInfolistIndicatorIconPadding * 2,
               0);
  return size;
}

void CandidateView::SetPositionData(int index, int total) {
  candidate_index_ = index;
  total_candidates_ = total;
}

void CandidateView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kImeCandidate;
  // PosInSet needs to be incremented since |candidate_index_| is 0-based.
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             candidate_index_ + 1);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             total_candidates_);
}

void CandidateView::OnThemeChanged() {
  Button::OnThemeChanged();
  if (infolist_icon_) {
    infolist_icon_->SetBackground(views::CreateSolidBackground(
        GetColorProvider()->GetColor(ui::kColorFocusableBorderFocused)));
  }
}

BEGIN_METADATA(CandidateView, views::Button)
END_METADATA

}  // namespace ime
}  // namespace ui

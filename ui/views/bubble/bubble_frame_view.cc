// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include <algorithm>
#include <utility>

#include "build/build_config.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"

namespace views {

namespace {

// The horizontal padding between the title and the icon.
const int kTitleHorizontalPadding = 5;

// Background color of the footnote view.
const SkColor kFootnoteBackgroundColor = SkColorSetRGB(245, 245, 245);

// Color of the top border of the footnote.
const SkColor kFootnoteBorderColor = SkColorSetRGB(229, 229, 229);

// Get the |vertical| or horizontal amount that |available_bounds| overflows
// |window_bounds|.
int GetOffScreenLength(const gfx::Rect& available_bounds,
                       const gfx::Rect& window_bounds,
                       bool vertical) {
  if (available_bounds.IsEmpty() || available_bounds.Contains(window_bounds))
    return 0;

  //  window_bounds
  //  +---------------------------------+
  //  |             top                 |
  //  |      +------------------+       |
  //  | left | available_bounds | right |
  //  |      +------------------+       |
  //  |            bottom               |
  //  +---------------------------------+
  if (vertical)
    return std::max(0, available_bounds.y() - window_bounds.y()) +
           std::max(0, window_bounds.bottom() - available_bounds.bottom());
  return std::max(0, available_bounds.x() - window_bounds.x()) +
         std::max(0, window_bounds.right() - available_bounds.right());
}

}  // namespace

// static
const char BubbleFrameView::kViewClassName[] = "BubbleFrameView";

BubbleFrameView::BubbleFrameView(const gfx::Insets& title_margins,
                                 const gfx::Insets& content_margins)
    : bubble_border_(nullptr),
      title_margins_(title_margins),
      content_margins_(content_margins),
      title_icon_(new views::ImageView()),
      title_(nullptr),
      close_(nullptr),
      titlebar_extra_view_(nullptr),
      footnote_container_(nullptr),
      close_button_clicked_(false) {
  AddChildView(title_icon_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_ = new Label(base::string16(),
                     rb.GetFontList(ui::ResourceBundle::MediumFont));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->set_collapse_when_hidden(true);
  title_->SetVisible(false);
  title_->SetMultiLine(true);
  AddChildView(title_);

  close_ = CreateCloseButton(this);
  close_->SetVisible(false);
  AddChildView(close_);
}

BubbleFrameView::~BubbleFrameView() {}

// static
LabelButton* BubbleFrameView::CreateCloseButton(ButtonListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  LabelButton* close = new LabelButton(listener, base::string16());
  close->SetImage(CustomButton::STATE_NORMAL,
                  *rb.GetImageNamed(IDR_CLOSE_DIALOG).ToImageSkia());
  close->SetImage(CustomButton::STATE_HOVERED,
                  *rb.GetImageNamed(IDR_CLOSE_DIALOG_H).ToImageSkia());
  close->SetImage(CustomButton::STATE_PRESSED,
                  *rb.GetImageNamed(IDR_CLOSE_DIALOG_P).ToImageSkia());
  close->SetBorder(nullptr);
  close->SetSize(close->GetPreferredSize());
#if !defined(OS_WIN)
  // Windows will automatically create a tooltip for the close button based on
  // the HTCLOSE result from NonClientHitTest().
  close->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
#endif
  return close;
}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = GetContentsBounds();
  client_bounds.Inset(GetInsets());
  if (footnote_container_) {
    client_bounds.set_height(client_bounds.height() -
                             footnote_container_->height());
  }
  return client_bounds;
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Size size(GetSizeForClientSize(client_bounds.size()));
  return bubble_border_->GetBounds(gfx::Rect(), size);
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  if (close_->visible() && close_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  // Allow dialogs to show the system menu and be dragged.
  if (GetWidget()->widget_delegate()->AsDialogDelegate()) {
    gfx::Rect sys_rect(0, 0, title_->x(), title_->y());
    sys_rect.set_origin(gfx::Point(GetMirroredXForRect(sys_rect), 0));
    if (sys_rect.Contains(point))
      return HTSYSMENU;
    if (point.y() < title_->bounds().bottom())
      return HTCAPTION;
  }

  return GetWidget()->client_view()->NonClientHitTest(point);
}

void BubbleFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
  // NOTE: this only provides implementations for the types used by dialogs.
  if ((bubble_border_->arrow() != BubbleBorder::NONE &&
       bubble_border_->arrow() != BubbleBorder::FLOAT) ||
      (bubble_border_->shadow() != BubbleBorder::SMALL_SHADOW &&
       bubble_border_->shadow() != BubbleBorder::NO_SHADOW_OPAQUE_BORDER))
    return;

  // Use a window mask roughly matching the border in the image assets.
  static const int kBorderStrokeSize = 1;
  static const SkScalar kCornerRadius = SkIntToScalar(6);
  const gfx::Insets border_insets = bubble_border_->GetInsets();
  SkRect rect = { SkIntToScalar(border_insets.left() - kBorderStrokeSize),
                  SkIntToScalar(border_insets.top() - kBorderStrokeSize),
                  SkIntToScalar(size.width() - border_insets.right() +
                                kBorderStrokeSize),
                  SkIntToScalar(size.height() - border_insets.bottom() +
                                kBorderStrokeSize) };
  if (bubble_border_->shadow() == BubbleBorder::NO_SHADOW_OPAQUE_BORDER) {
    window_mask->addRoundRect(rect, kCornerRadius, kCornerRadius);
  } else {
    static const int kBottomBorderShadowSize = 2;
    rect.fBottom += SkIntToScalar(kBottomBorderShadowSize);
    window_mask->addRect(rect);
  }
}

void BubbleFrameView::ResetWindowControls() {
  close_->SetVisible(GetWidget()->widget_delegate()->ShouldShowCloseButton());
}

void BubbleFrameView::UpdateWindowIcon() {
  gfx::ImageSkia image;
  if (GetWidget()->widget_delegate()->ShouldShowWindowIcon())
    image = GetWidget()->widget_delegate()->GetWindowIcon();
  title_icon_->SetImage(&image);
}


void BubbleFrameView::UpdateWindowTitle() {
  title_->SetText(GetWidget()->widget_delegate()->GetWindowTitle());
  title_->SetVisible(GetWidget()->widget_delegate()->ShouldShowWindowTitle());
}

void BubbleFrameView::SizeConstraintsChanged() {}

void BubbleFrameView::SetTitleFontList(const gfx::FontList& font_list) {
  title_->SetFontList(font_list);
}

gfx::Insets BubbleFrameView::GetInsets() const {
  gfx::Insets insets = content_margins_;

  const int icon_height = title_icon_->GetPreferredSize().height();
  const int label_height = title_->GetPreferredSize().height();
  const bool has_title = icon_height > 0 || label_height > 0;
  const int title_padding = has_title ? title_margins_.height() : 0;
  const int title_height = std::max(icon_height, label_height) + title_padding;
  const int close_height = close_->visible() ? close_->height() : 0;
  insets += gfx::Insets(std::max(title_height, close_height), 0, 0, 0);
  return insets;
}

gfx::Size BubbleFrameView::GetPreferredSize() const {
  // Get the preferred size of the client area.
  gfx::Size client_size = GetWidget()->client_view()->GetPreferredSize();
  // Expand it to include the bubble border and space for the arrow.
  return GetWindowBoundsForClientBounds(gfx::Rect(client_size)).size();
}

gfx::Size BubbleFrameView::GetMinimumSize() const {
  // Get the minimum size of the client area.
  gfx::Size client_size = GetWidget()->client_view()->GetMinimumSize();
  // Expand it to include the bubble border and space for the arrow.
  return GetWindowBoundsForClientBounds(gfx::Rect(client_size)).size();
}

gfx::Size BubbleFrameView::GetMaximumSize() const {
#if defined(OS_WIN)
  // On Windows, this causes problems, so do not set a maximum size (it doesn't
  // take the drop shadow area into account, resulting in a too-small window;
  // see http://crbug.com/506206). This isn't necessary on Windows anyway, since
  // the OS doesn't give the user controls to resize a bubble.
  return gfx::Size();
#else
#if defined(OS_MACOSX)
  // Allow BubbleFrameView dialogs to be resizable on Mac.
  if (GetWidget()->widget_delegate()->CanResize()) {
    gfx::Size client_size = GetWidget()->client_view()->GetMaximumSize();
    if (client_size.IsEmpty())
      return client_size;
    return GetWindowBoundsForClientBounds(gfx::Rect(client_size)).size();
  }
#endif  // OS_MACOSX
  // Non-dialog bubbles should be non-resizable, so its max size is its
  // preferred size.
  return GetPreferredSize();
#endif
}

void BubbleFrameView::Layout() {
  // The title margins may not be set, but make sure that's only the case when
  // there's no title.
  DCHECK(!title_margins_.IsEmpty() || !title_->visible());

  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(title_margins_);
  if (bounds.IsEmpty())
    return;

  // The close button is positioned somewhat closer to the edge of the bubble.
  gfx::Point close_position = GetContentsBounds().top_right();
  close_position += gfx::Vector2d(-close_->width() - 7, 6);
  close_->SetPosition(close_position);

  gfx::Size title_icon_size(title_icon_->GetPreferredSize());
  gfx::Size title_label_size(title_->GetPreferredSize());
  int padding = 0;
  if (title_icon_size.width() > 0 && title_label_size.width() > 0)
    padding = kTitleHorizontalPadding;
  const int title_height = std::max(title_icon_size.height(),
                                    title_label_size.height());

  const int title_icon_width = std::max(0, close_->x() - bounds.x());
  title_icon_size.SetToMin(gfx::Size(title_icon_width, title_height));
  gfx::Rect title_icon_bounds(
      bounds.x(), bounds.y(), title_icon_size.width(), title_height);
  title_icon_->SetBoundsRect(title_icon_bounds);

  const int title_label_x = title_icon_->bounds().right() + padding;
  const int title_label_width = std::max(0, close_->x() - title_label_x);
  title_label_size.SetToMin(gfx::Size(title_label_width,
                                      title_label_size.height()));
  gfx::Rect title_label_bounds(
      title_label_x, bounds.y(), title_label_size.width(), title_height);
  title_->SetBoundsRect(title_label_bounds);

  bounds.set_width(
      title_icon_size.width() + title_label_size.width() + padding);
  bounds.set_height(title_height);

  if (titlebar_extra_view_) {
    const int extra_width = close_->x() - bounds.right();
    gfx::Size size = titlebar_extra_view_->GetPreferredSize();
    size.SetToMin(gfx::Size(std::max(0, extra_width), size.height()));
    gfx::Rect titlebar_extra_view_bounds(
        close_->x() - size.width(),
        bounds.y(),
        size.width(),
        bounds.height());
    titlebar_extra_view_bounds.Subtract(bounds);
    titlebar_extra_view_->SetBoundsRect(titlebar_extra_view_bounds);
  }

  if (footnote_container_) {
    gfx::Rect local_bounds = GetContentsBounds();
    int height = footnote_container_->GetHeightForWidth(local_bounds.width());
    footnote_container_->SetBounds(local_bounds.x(),
                                   local_bounds.bottom() - height,
                                   local_bounds.width(), height);
  }
}

const char* BubbleFrameView::GetClassName() const {
  return kViewClassName;
}

void BubbleFrameView::ChildPreferredSizeChanged(View* child) {
  if (child == titlebar_extra_view_ || child == title_)
    Layout();
}

void BubbleFrameView::OnThemeChanged() {
  UpdateWindowTitle();
  ResetWindowControls();
  UpdateWindowIcon();
}

void BubbleFrameView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  if (bubble_border_ && bubble_border_->use_theme_background_color()) {
    bubble_border_->set_background_color(GetNativeTheme()->
        GetSystemColor(ui::NativeTheme::kColorId_DialogBackground));
    SchedulePaint();
  }
}

void BubbleFrameView::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == close_) {
    close_button_clicked_ = true;
    GetWidget()->Close();
  }
}

void BubbleFrameView::SetBubbleBorder(scoped_ptr<BubbleBorder> border) {
  bubble_border_ = border.get();
  SetBorder(std::move(border));

  // Update the background, which relies on the border.
  set_background(new views::BubbleBackground(bubble_border_));
}

void BubbleFrameView::SetTitlebarExtraView(scoped_ptr<View> view) {
  if (!view)
    return;

  DCHECK(!titlebar_extra_view_);
  titlebar_extra_view_ = view.release();
  AddChildView(titlebar_extra_view_);
}

void BubbleFrameView::SetFootnoteView(scoped_ptr<View> view) {
  if (!view)
    return;

  DCHECK(!footnote_container_);

  footnote_container_ = new views::View();
  footnote_container_->SetLayoutManager(
      new BoxLayout(BoxLayout::kVertical, content_margins_.left(),
                    content_margins_.top(), 0));
  footnote_container_->set_background(
      Background::CreateSolidBackground(kFootnoteBackgroundColor));
  footnote_container_->SetBorder(
      Border::CreateSolidSidedBorder(1, 0, 0, 0, kFootnoteBorderColor));
  footnote_container_->AddChildView(view.release());
  AddChildView(footnote_container_);
}

gfx::Rect BubbleFrameView::GetUpdatedWindowBounds(const gfx::Rect& anchor_rect,
                                                  gfx::Size client_size,
                                                  bool adjust_if_offscreen) {
  gfx::Size size(GetSizeForClientSize(client_size));

  const BubbleBorder::Arrow arrow = bubble_border_->arrow();
  if (adjust_if_offscreen && BubbleBorder::has_arrow(arrow)) {
    // Try to mirror the anchoring if the bubble does not fit on the screen.
    if (!bubble_border_->is_arrow_at_center(arrow)) {
      MirrorArrowIfOffScreen(true, anchor_rect, size);
      MirrorArrowIfOffScreen(false, anchor_rect, size);
    } else {
      const bool mirror_vertical = BubbleBorder::is_arrow_on_horizontal(arrow);
      MirrorArrowIfOffScreen(mirror_vertical, anchor_rect, size);
      OffsetArrowIfOffScreen(anchor_rect, size);
    }
  }

  // Calculate the bounds with the arrow in its updated location and offset.
  return bubble_border_->GetBounds(anchor_rect, size);
}

gfx::Rect BubbleFrameView::GetAvailableScreenBounds(
    const gfx::Rect& rect) const {
  // The bubble attempts to fit within the current screen bounds.
  return gfx::Screen::GetScreen()
      ->GetDisplayNearestPoint(rect.CenterPoint())
      .work_area();
}

bool BubbleFrameView::IsCloseButtonVisible() const {
  return close_->visible();
}

gfx::Rect BubbleFrameView::GetCloseButtonMirroredBounds() const {
  return close_->GetMirroredBounds();
}

void BubbleFrameView::MirrorArrowIfOffScreen(
    bool vertical,
    const gfx::Rect& anchor_rect,
    const gfx::Size& client_size) {
  // Check if the bounds don't fit on screen.
  gfx::Rect available_bounds(GetAvailableScreenBounds(anchor_rect));
  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));
  if (GetOffScreenLength(available_bounds, window_bounds, vertical) > 0) {
    BubbleBorder::Arrow arrow = bubble_border()->arrow();
    // Mirror the arrow and get the new bounds.
    bubble_border_->set_arrow(
        vertical ? BubbleBorder::vertical_mirror(arrow) :
                   BubbleBorder::horizontal_mirror(arrow));
    gfx::Rect mirror_bounds =
        bubble_border_->GetBounds(anchor_rect, client_size);
    // Restore the original arrow if mirroring doesn't show more of the bubble.
    // Otherwise it should invoke parent's Layout() to layout the content based
    // on the new bubble border.
    if (GetOffScreenLength(available_bounds, mirror_bounds, vertical) >=
        GetOffScreenLength(available_bounds, window_bounds, vertical)) {
      bubble_border_->set_arrow(arrow);
    } else {
      if (parent())
        parent()->Layout();
      SchedulePaint();
    }
  }
}

void BubbleFrameView::OffsetArrowIfOffScreen(const gfx::Rect& anchor_rect,
                                             const gfx::Size& client_size) {
  BubbleBorder::Arrow arrow = bubble_border()->arrow();
  DCHECK(BubbleBorder::is_arrow_at_center(arrow));

  // Get the desired bubble bounds without adjustment.
  bubble_border_->set_arrow_offset(0);
  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));

  gfx::Rect available_bounds(GetAvailableScreenBounds(anchor_rect));
  if (available_bounds.IsEmpty() || available_bounds.Contains(window_bounds))
    return;

  // Calculate off-screen adjustment.
  const bool is_horizontal = BubbleBorder::is_arrow_on_horizontal(arrow);
  int offscreen_adjust = 0;
  if (is_horizontal) {
    if (window_bounds.x() < available_bounds.x())
      offscreen_adjust = available_bounds.x() - window_bounds.x();
    else if (window_bounds.right() > available_bounds.right())
      offscreen_adjust = available_bounds.right() - window_bounds.right();
  } else {
    if (window_bounds.y() < available_bounds.y())
      offscreen_adjust = available_bounds.y() - window_bounds.y();
    else if (window_bounds.bottom() > available_bounds.bottom())
      offscreen_adjust = available_bounds.bottom() - window_bounds.bottom();
  }

  // For center arrows, arrows are moved in the opposite direction of
  // |offscreen_adjust|, e.g. positive |offscreen_adjust| means bubble
  // window needs to be moved to the right and that means we need to move arrow
  // to the left, and that means negative offset.
  bubble_border_->set_arrow_offset(
      bubble_border_->GetArrowOffset(window_bounds.size()) - offscreen_adjust);
  if (offscreen_adjust)
    SchedulePaint();
}

gfx::Size BubbleFrameView::GetSizeForClientSize(
    const gfx::Size& client_size) const {
  // Accommodate the width of the title bar elements.
  int title_bar_width = title_margins_.width() + border()->GetInsets().width();
  gfx::Size title_icon_size = title_icon_->GetPreferredSize();
  gfx::Size title_label_size = title_->GetPreferredSize();
  if (title_icon_size.width() > 0 && title_label_size.width() > 0)
    title_bar_width += kTitleHorizontalPadding;
  title_bar_width += title_icon_size.width();
  if (close_->visible())
    title_bar_width += close_->width() + 1;
  if (titlebar_extra_view_)
    title_bar_width += titlebar_extra_view_->GetPreferredSize().width();

  gfx::Size size(client_size);
  gfx::Insets client_insets = GetInsets();
  size.Enlarge(client_insets.width(), client_insets.height());
  size.SetToMax(gfx::Size(title_bar_width, 0));

  if (footnote_container_)
    size.Enlarge(0, footnote_container_->GetHeightForWidth(size.width()));

  return size;
}

}  // namespace views

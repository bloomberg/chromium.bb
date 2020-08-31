// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include <algorithm>
#include <utility>

#include "build/build_config.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/default_style.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/footnote_container_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/paint_info.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

namespace {

// Get the |vertical| or horizontal amount that |available_bounds| overflows
// |window_bounds|.
int GetOverflowLength(const gfx::Rect& available_bounds,
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

// The height of the progress indicator shown at the top of the bubble frame
// view.
constexpr int kProgressIndicatorHeight = 4;

}  // namespace

// static
const char BubbleFrameView::kViewClassName[] = "BubbleFrameView";

BubbleFrameView::BubbleFrameView(const gfx::Insets& title_margins,
                                 const gfx::Insets& content_margins)
    : title_margins_(title_margins),
      content_margins_(content_margins),
      footnote_margins_(content_margins_),
      title_icon_(new views::ImageView()),
      default_title_(CreateDefaultTitleLabel(base::string16()).release()) {
  AddChildView(title_icon_);

  default_title_->SetVisible(false);
  AddChildView(default_title_);

  auto close = CreateCloseButton(this);
  close->SetVisible(false);
#if defined(OS_WIN)
  // Windows will automatically create a tooltip for the close button based on
  // the HTCLOSE result from NonClientHitTest().
  close->SetTooltipText(base::string16());
  // Specify accessible name instead for screen readers.
  close->SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
#endif
  close_ = AddChildView(std::move(close));

  auto progress_indicator = std::make_unique<ProgressBar>(
      kProgressIndicatorHeight, /*allow_round_corner=*/false);
  progress_indicator->SetBackgroundColor(SK_ColorTRANSPARENT);
  progress_indicator->SetVisible(false);
  progress_indicator_ = AddChildView(std::move(progress_indicator));
}

BubbleFrameView::~BubbleFrameView() = default;

// static
std::unique_ptr<Label> BubbleFrameView::CreateDefaultTitleLabel(
    const base::string16& title_text) {
  auto title = std::make_unique<Label>(title_text, style::CONTEXT_DIALOG_TITLE);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetCollapseWhenHidden(true);
  title->SetMultiLine(true);
  return title;
}

// static
std::unique_ptr<Button> BubbleFrameView::CreateCloseButton(
    ButtonListener* listener) {
  auto close_button = CreateVectorImageButtonWithNativeTheme(
      listener, vector_icons::kCloseRoundedIcon);
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SizeToPreferredSize();
  close_button->SetFocusForPlatform();

  InstallCircleHighlightPathGenerator(close_button.get());

  return close_button;
}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  // When NonClientView asks for this, the size of the frame view has been set
  // (i.e. |this|), but not the client view bounds.
  gfx::Rect client_bounds = GetContentsBounds();
  client_bounds.Inset(GetClientInsetsForFrameWidth(client_bounds.width()));
  // Only account for footnote_container_'s height if it's visible, because
  // content_margins_ adds extra padding even if all child views are invisible.
  if (footnote_container_ && footnote_container_->GetVisible()) {
    client_bounds.set_height(client_bounds.height() -
                             footnote_container_->height());
  }
  return client_bounds;
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Size size(GetFrameSizeForClientSize(client_bounds.size()));
  return bubble_border_->GetBounds(gfx::Rect(), size);
}

bool BubbleFrameView::GetClientMask(const gfx::Size& size, SkPath* path) const {
  // NonClientView calls this after setting the client view size from the return
  // of GetBoundsForClientView(); feeding it back in |size|.
  DCHECK_EQ(GetBoundsForClientView().size(), size);
  DCHECK_EQ(GetWidget()->client_view()->size(), size);

  const int radius = bubble_border_->corner_radius();
  const gfx::Insets insets =
      GetClientInsetsForFrameWidth(GetContentsBounds().width());

  // A mask is not needed if the content does not overlap the rounded corners.
  if ((insets.top() > radius && insets.bottom() > radius) ||
      (insets.left() > radius && insets.right() > radius)) {
    return false;
  }

  // We want to clip the client view to a rounded rect that's consistent with
  // the bubble's rounded border. However, if there is a header, the top of the
  // client view should be straight and flush with that. Likewise, if there is
  // a footer, the client view should be straight and flush with that. Therefore
  // we set the corner radii separately for top and bottom.
  const SkRect rect = SkRect::MakeIWH(size.width(), size.height());
  const SkScalar top_radius = header_view_ ? 0.0f : radius;
  const SkScalar bottom_radius = footnote_container_ ? 0.0f : radius;
  // Format is upper-left x, upper-left y, upper-right x, and so forth,
  // clockwise around the boundary.
  SkScalar radii[]{top_radius,    top_radius,    top_radius,    top_radius,
                   bottom_radius, bottom_radius, bottom_radius, bottom_radius};
  path->addRoundRect(rect, radii);
  return true;
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  if (hit_test_transparent_)
    return HTTRANSPARENT;
  if (close_->GetVisible() && close_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  // Convert to RRectF to accurately represent the rounded corners of the
  // dialog and allow events to pass through the shadows.
  gfx::RRectF round_contents_bounds(gfx::RectF(GetContentsBounds()),
                                    bubble_border_->corner_radius());
  if (bubble_border_->shadow() != BubbleBorder::NO_ASSETS)
    round_contents_bounds.Outset(BubbleBorder::kBorderThicknessDip);
  gfx::RectF rectf_point(point.x(), point.y(), 1, 1);
  if (!round_contents_bounds.Contains(rectf_point))
    return HTTRANSPARENT;

  if (point.y() < title()->bounds().bottom()) {
    auto* dialog_delegate = GetWidget()->widget_delegate()->AsDialogDelegate();
    if (dialog_delegate && dialog_delegate->draggable()) {
      return HTCAPTION;
    }
  }

  return GetWidget()->client_view()->NonClientHitTest(point);
}

void BubbleFrameView::GetWindowMask(const gfx::Size& size,
                                    SkPath* window_mask) {
  if (bubble_border_->shadow() != BubbleBorder::SMALL_SHADOW &&
      bubble_border_->shadow() != BubbleBorder::NO_SHADOW_OPAQUE_BORDER &&
      bubble_border_->shadow() != BubbleBorder::NO_ASSETS)
    return;

  // We don't return a mask for windows with arrows unless they use
  // BubbleBorder::NO_ASSETS.
  if (bubble_border_->shadow() != BubbleBorder::NO_ASSETS &&
      bubble_border_->arrow() != BubbleBorder::NONE &&
      bubble_border_->arrow() != BubbleBorder::FLOAT)
    return;

  // Use a window mask roughly matching the border in the image assets.
  const int kBorderStrokeSize =
      bubble_border_->shadow() == BubbleBorder::NO_ASSETS ? 0 : 1;
  const SkScalar kCornerRadius = SkIntToScalar(bubble_border_->corner_radius());
  const gfx::Insets border_insets = bubble_border_->GetInsets();
  SkRect rect = {
      SkIntToScalar(border_insets.left() - kBorderStrokeSize),
      SkIntToScalar(border_insets.top() - kBorderStrokeSize),
      SkIntToScalar(size.width() - border_insets.right() + kBorderStrokeSize),
      SkIntToScalar(size.height() - border_insets.bottom() +
                    kBorderStrokeSize)};

  if (bubble_border_->shadow() == BubbleBorder::NO_SHADOW_OPAQUE_BORDER ||
      bubble_border_->shadow() == BubbleBorder::NO_ASSETS) {
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
  if (default_title_) {
    const WidgetDelegate* delegate = GetWidget()->widget_delegate();
    default_title_->SetVisible(delegate->ShouldShowWindowTitle() &&
                               !delegate->GetWindowTitle().empty());
    default_title_->SetText(delegate->GetWindowTitle());
  }  // custom_title_'s updates are handled by its creator.
  InvalidateLayout();
}

void BubbleFrameView::SizeConstraintsChanged() {}

void BubbleFrameView::SetTitleView(std::unique_ptr<View> title_view) {
  DCHECK(title_view);
  delete default_title_;
  default_title_ = nullptr;
  delete custom_title_;
  custom_title_ = title_view.get();
  // Keep the title after the icon for focus order.
  AddChildViewAt(title_view.release(), GetIndexOf(title_icon_) + 1);
}

void BubbleFrameView::SetProgress(base::Optional<double> progress) {
  progress_indicator_->SetVisible(progress.has_value());
  if (progress)
    progress_indicator_->SetValue(progress.value());
}

const char* BubbleFrameView::GetClassName() const {
  return kViewClassName;
}

gfx::Size BubbleFrameView::CalculatePreferredSize() const {
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
  DCHECK(!title_margins_.IsEmpty() ||
         (!custom_title_ && !default_title_->GetVisible()));

  const gfx::Rect contents_bounds = GetContentsBounds();

  progress_indicator_->SetBounds(contents_bounds.x(), contents_bounds.y(),
                                 contents_bounds.width(),
                                 kProgressIndicatorHeight);

  gfx::Rect bounds = contents_bounds;
  bounds.Inset(title_margins_);

  int header_bottom = 0;
  int header_height = GetHeaderHeightForFrameWidth(contents_bounds.width());
  if (header_height > 0) {
    header_view_->SetBounds(contents_bounds.x(), contents_bounds.y(),
                            contents_bounds.width(), header_height);
    bounds.Inset(0, header_height, 0, 0);
    header_bottom = header_view_->bounds().bottom();
  }

  if (bounds.IsEmpty())
    return;

  int title_label_right = bounds.right();
  if (close_->GetVisible()) {
    // The close button is positioned somewhat closer to the edge of the bubble.
    const int close_margin =
        LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CLOSE_BUTTON_MARGIN);
    close_->SetPosition(
        gfx::Point(contents_bounds.right() - close_margin - close_->width(),
                   contents_bounds.y() + close_margin));
    // Only reserve space if the close button extends over the header.
    if (close_->bounds().bottom() > header_bottom) {
      title_label_right =
          std::min(title_label_right, close_->x() - close_margin);
    }
  }

  gfx::Size title_icon_pref_size(title_icon_->GetPreferredSize());
  const int title_icon_padding =
      title_icon_pref_size.width() > 0 ? title_margins_.left() : 0;
  const int title_label_x =
      bounds.x() + title_icon_pref_size.width() + title_icon_padding;

  // TODO(tapted): Layout() should skip more surrounding code when !HasTitle().
  // Currently DCHECKs fail since title_insets is 0 when there is no title.
  if (DCHECK_IS_ON() && HasTitle()) {
    gfx::Insets title_insets = GetTitleLabelInsetsFromFrame();
    if (border())
      title_insets += border()->GetInsets();
    DCHECK_EQ(title_insets.left(), title_label_x);
    DCHECK_EQ(title_insets.right(), width() - title_label_right);
  }

  const int title_available_width =
      std::max(1, title_label_right - title_label_x);
  const int title_preferred_height =
      title()->GetHeightForWidth(title_available_width);
  const int title_height =
      std::max(title_icon_pref_size.height(), title_preferred_height);
  title()->SetBounds(title_label_x,
                     bounds.y() + (title_height - title_preferred_height) / 2,
                     title_available_width, title_preferred_height);

  title_icon_->SetBounds(bounds.x(), bounds.y(), title_icon_pref_size.width(),
                         title_height);

  // Only account for footnote_container_'s height if it's visible, because
  // content_margins_ adds extra padding even if all child views are invisible.
  if (footnote_container_ && footnote_container_->GetVisible()) {
    const int width = contents_bounds.width();
    const int height = footnote_container_->GetHeightForWidth(width);
    footnote_container_->SetBounds(
        contents_bounds.x(), contents_bounds.bottom() - height, width, height);
  }
}

void BubbleFrameView::OnThemeChanged() {
  NonClientFrameView::OnThemeChanged();
  UpdateWindowTitle();
  ResetWindowControls();
  UpdateWindowIcon();

  if (bubble_border_ && bubble_border_->use_theme_background_color()) {
    bubble_border_->set_background_color(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_DialogBackground));
    SchedulePaint();
  }
}

void BubbleFrameView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    OnThemeChanged();

  if (!details.is_add && details.parent == footnote_container_ &&
      footnote_container_->children().size() == 1 &&
      details.child == footnote_container_->children().front()) {
    // Setting the footnote_container_ to be hidden and null it. This will
    // remove update the bubble to have no placeholder for the footnote and
    // enable the destructor to delete the footnote_container_ later.
    footnote_container_->SetVisible(false);
    footnote_container_ = nullptr;
  }
}

void BubbleFrameView::VisibilityChanged(View* starting_from, bool is_visible) {
  NonClientFrameView::VisibilityChanged(starting_from, is_visible);
  input_protector_.VisibilityChanged(is_visible);
}

void BubbleFrameView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  // Border comes after children.
}

void BubbleFrameView::PaintChildren(const PaintInfo& paint_info) {
  NonClientFrameView::PaintChildren(paint_info);

  ui::PaintCache paint_cache;
  ui::PaintRecorder recorder(
      paint_info.context(), paint_info.paint_recording_size(),
      paint_info.paint_recording_scale_x(),
      paint_info.paint_recording_scale_y(), &paint_cache);
  OnPaintBorder(recorder.canvas());
}

void BubbleFrameView::ButtonPressed(Button* sender, const ui::Event& event) {
  if (input_protector_.IsPossiblyUnintendedInteraction(event))
    return;

  if (sender == close_) {
    GetWidget()->CloseWithReason(Widget::ClosedReason::kCloseButtonClicked);
  }
}

void BubbleFrameView::SetBubbleBorder(std::unique_ptr<BubbleBorder> border) {
  bubble_border_ = border.get();

  if (footnote_container_)
    footnote_container_->SetCornerRadius(border->corner_radius());

  SetBorder(std::move(border));

  // Update the background, which relies on the border.
  SetBackground(std::make_unique<views::BubbleBackground>(bubble_border_));
}

void BubbleFrameView::SetHeaderView(std::unique_ptr<View> view) {
  if (header_view_) {
    delete header_view_;
    header_view_ = nullptr;
  }

  if (view)
    header_view_ = AddChildViewAt(std::move(view), 0);
}

void BubbleFrameView::SetFootnoteView(std::unique_ptr<View> view) {
  // Remove the old footnote container.
  delete footnote_container_;
  footnote_container_ = nullptr;
  if (view) {
    int radius = bubble_border_ ? bubble_border_->corner_radius() : 0;
    footnote_container_ = AddChildView(std::make_unique<FootnoteContainerView>(
        footnote_margins_, std::move(view), radius));
  }
  InvalidateLayout();
}

View* BubbleFrameView::GetFootnoteView() const {
  if (!footnote_container_)
    return nullptr;

  DCHECK_EQ(1u, footnote_container_->children().size());
  return footnote_container_->children()[0];
}

void BubbleFrameView::SetCornerRadius(int radius) {
  bubble_border_->SetCornerRadius(radius);
}

void BubbleFrameView::SetArrow(BubbleBorder::Arrow arrow) {
  bubble_border_->set_arrow(arrow);
}

void BubbleFrameView::SetBackgroundColor(SkColor color) {
  bubble_border_->set_background_color(color);
  SchedulePaint();
}

SkColor BubbleFrameView::GetBackgroundColor() const {
  return bubble_border_->background_color();
}

gfx::Rect BubbleFrameView::GetUpdatedWindowBounds(
    const gfx::Rect& anchor_rect,
    const BubbleBorder::Arrow delegate_arrow,
    const gfx::Size& client_size,
    bool adjust_to_fit_available_bounds) {
  gfx::Size size(GetFrameSizeForClientSize(client_size));

  if (adjust_to_fit_available_bounds &&
      BubbleBorder::has_arrow(delegate_arrow)) {
    // Get the desired bubble bounds without adjustment.
    bubble_border_->set_arrow_offset(0);
    bubble_border_->set_arrow(delegate_arrow);
    // Try to mirror the anchoring if the bubble does not fit in the available
    // bounds.
    if (bubble_border_->is_arrow_at_center(delegate_arrow) ||
        preferred_arrow_adjustment_ == PreferredArrowAdjustment::kOffset) {
      const bool mirror_vertical =
          BubbleBorder::is_arrow_on_horizontal(delegate_arrow);
      MirrorArrowIfOutOfBounds(mirror_vertical, anchor_rect, size,
                               GetAvailableAnchorWindowBounds());
      MirrorArrowIfOutOfBounds(mirror_vertical, anchor_rect, size,
                               GetAvailableScreenBounds(anchor_rect));
      OffsetArrowIfOutOfBounds(anchor_rect, size,
                               GetAvailableAnchorWindowBounds());
      OffsetArrowIfOutOfBounds(anchor_rect, size,
                               GetAvailableScreenBounds(anchor_rect));
    } else {
      MirrorArrowIfOutOfBounds(true, anchor_rect, size,
                               GetAvailableAnchorWindowBounds());
      MirrorArrowIfOutOfBounds(true, anchor_rect, size,
                               GetAvailableScreenBounds(anchor_rect));
      MirrorArrowIfOutOfBounds(false, anchor_rect, size,
                               GetAvailableAnchorWindowBounds());
      MirrorArrowIfOutOfBounds(false, anchor_rect, size,
                               GetAvailableScreenBounds(anchor_rect));
    }
  }

  // Calculate the bounds with the arrow in its updated location and offset.
  return bubble_border_->GetBounds(anchor_rect, size);
}

void BubbleFrameView::ResetViewShownTimeStampForTesting() {
  input_protector_.ResetForTesting();
}

gfx::Rect BubbleFrameView::GetAvailableScreenBounds(
    const gfx::Rect& rect) const {
  // The bubble attempts to fit within the current screen bounds.
  return display::Screen::GetScreen()
      ->GetDisplayNearestPoint(rect.CenterPoint())
      .work_area();
}

gfx::Rect BubbleFrameView::GetAvailableAnchorWindowBounds() const {
  views::BubbleDialogDelegateView* bubble_delegate_view =
      GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
  if (bubble_delegate_view) {
    views::View* const anchor_view = bubble_delegate_view->GetAnchorView();
    if (anchor_view && anchor_view->GetWidget())
      return anchor_view->GetWidget()->GetWindowBoundsInScreen();
  }
  return gfx::Rect();
}

bool BubbleFrameView::ExtendClientIntoTitle() const {
  return false;
}

bool BubbleFrameView::IsCloseButtonVisible() const {
  return close_->GetVisible();
}

gfx::Rect BubbleFrameView::GetCloseButtonMirroredBounds() const {
  return close_->GetMirroredBounds();
}

void BubbleFrameView::MirrorArrowIfOutOfBounds(
    bool vertical,
    const gfx::Rect& anchor_rect,
    const gfx::Size& client_size,
    const gfx::Rect& available_bounds) {
  if (available_bounds.IsEmpty())
    return;
  // Check if the bounds don't fit in the available bounds.
  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));
  if (GetOverflowLength(available_bounds, window_bounds, vertical) > 0) {
    BubbleBorder::Arrow arrow = bubble_border_->arrow();
    // Mirror the arrow and get the new bounds.
    bubble_border_->set_arrow(vertical
                                  ? BubbleBorder::vertical_mirror(arrow)
                                  : BubbleBorder::horizontal_mirror(arrow));
    gfx::Rect mirror_bounds =
        bubble_border_->GetBounds(anchor_rect, client_size);
    // Restore the original arrow if mirroring doesn't show more of the bubble.
    // Otherwise it should invoke parent's Layout() to layout the content based
    // on the new bubble border.
    if (GetOverflowLength(available_bounds, mirror_bounds, vertical) >=
        GetOverflowLength(available_bounds, window_bounds, vertical)) {
      bubble_border_->set_arrow(arrow);
    } else {
      InvalidateLayout();
      SchedulePaint();
    }
  }
}

void BubbleFrameView::OffsetArrowIfOutOfBounds(
    const gfx::Rect& anchor_rect,
    const gfx::Size& client_size,
    const gfx::Rect& available_bounds) {
  BubbleBorder::Arrow arrow = bubble_border_->arrow();
  DCHECK(BubbleBorder::is_arrow_at_center(arrow) ||
         preferred_arrow_adjustment_ == PreferredArrowAdjustment::kOffset);

  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));
  if (available_bounds.IsEmpty() || available_bounds.Contains(window_bounds))
    return;

  // Calculate off-screen adjustment.
  const bool is_horizontal = BubbleBorder::is_arrow_on_horizontal(arrow);
  int offscreen_adjust = 0;
  if (is_horizontal) {
    // If the window bounds are larger than the available bounds then we want to
    // offset the window to fit as much of it in the available bounds as
    // possible without exiting the other side of the available bounds.
    if (window_bounds.width() > available_bounds.width()) {
      if (window_bounds.x() < available_bounds.x())
        offscreen_adjust = available_bounds.right() - window_bounds.right();
      else
        offscreen_adjust = available_bounds.x() - window_bounds.x();
    } else if (window_bounds.x() < available_bounds.x()) {
      offscreen_adjust = available_bounds.x() - window_bounds.x();
    } else if (window_bounds.right() > available_bounds.right()) {
      offscreen_adjust = available_bounds.right() - window_bounds.right();
    }
  } else {
    if (window_bounds.height() > available_bounds.height()) {
      if (window_bounds.y() < available_bounds.y())
        offscreen_adjust = available_bounds.bottom() - window_bounds.bottom();
      else
        offscreen_adjust = available_bounds.y() - window_bounds.y();
    } else if (window_bounds.y() < available_bounds.y()) {
      offscreen_adjust = available_bounds.y() - window_bounds.y();
    } else if (window_bounds.bottom() > available_bounds.bottom()) {
      offscreen_adjust = available_bounds.bottom() - window_bounds.bottom();
    }
  }

  // For center arrows, arrows are moved in the opposite direction of
  // |offscreen_adjust|, e.g. positive |offscreen_adjust| means bubble
  // window needs to be moved to the right and that means we need to move arrow
  // to the left, and that means negative offset.
  bubble_border_->set_arrow_offset(bubble_border_->arrow_offset() -
                                   offscreen_adjust);
  if (offscreen_adjust)
    SchedulePaint();
}

int BubbleFrameView::GetFrameWidthForClientWidth(int client_width) const {
  // Note that GetMinimumSize() for multiline Labels is typically 0.
  const int title_bar_width = title()->GetMinimumSize().width() +
                              GetTitleLabelInsetsFromFrame().width();
  const int client_area_width = client_width + content_margins_.width();
  const int frame_width = std::max(title_bar_width, client_area_width);
  DialogDelegate* dialog_delegate =
      GetWidget()->widget_delegate()->AsDialogDelegate();
  bool snapping = dialog_delegate &&
                  dialog_delegate->GetDialogButtons() != ui::DIALOG_BUTTON_NONE;
  return snapping ? LayoutProvider::Get()->GetSnappedDialogWidth(frame_width)
                  : frame_width;
}

gfx::Size BubbleFrameView::GetFrameSizeForClientSize(
    const gfx::Size& client_size) const {
  const int frame_width = GetFrameWidthForClientWidth(client_size.width());
  const gfx::Insets client_insets = GetClientInsetsForFrameWidth(frame_width);
  DCHECK_GE(frame_width, client_size.width());
  gfx::Size size(frame_width, client_size.height() + client_insets.height());

  // Only account for footnote_container_'s height if it's visible, because
  // content_margins_ adds extra padding even if all child views are invisible.
  if (footnote_container_ && footnote_container_->GetVisible())
    size.Enlarge(0, footnote_container_->GetHeightForWidth(size.width()));

  return size;
}

bool BubbleFrameView::HasTitle() const {
  return (custom_title_ != nullptr &&
          GetWidget()->widget_delegate()->ShouldShowWindowTitle()) ||
         (default_title_ != nullptr &&
          default_title_->GetPreferredSize().height() > 0) ||
         title_icon_->GetPreferredSize().height() > 0;
}

gfx::Insets BubbleFrameView::GetTitleLabelInsetsFromFrame() const {
  int header_height = GetHeaderHeightForFrameWidth(GetContentsBounds().width());
  int insets_right = 0;
  if (GetWidget()->widget_delegate()->ShouldShowCloseButton()) {
    const int close_margin =
        LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CLOSE_BUTTON_MARGIN);
    // Note: |close_margin| is not applied on the bottom of the icon.
    int close_height = close_margin + close_->height();
    // Only reserve space if the close button extends over the header.
    if (close_height > header_height)
      insets_right = 2 * close_margin + close_->width();
  }

  if (!HasTitle())
    return gfx::Insets(header_height, 0, 0, insets_right);

  insets_right = std::max(insets_right, title_margins_.right());
  const gfx::Size title_icon_pref_size = title_icon_->GetPreferredSize();
  const int title_icon_padding =
      title_icon_pref_size.width() > 0 ? title_margins_.left() : 0;
  const int insets_left =
      title_margins_.left() + title_icon_pref_size.width() + title_icon_padding;
  return gfx::Insets(header_height + title_margins_.top(), insets_left,
                     title_margins_.bottom(), insets_right);
}

gfx::Insets BubbleFrameView::GetClientInsetsForFrameWidth(
    int frame_width) const {
  int header_height = GetHeaderHeightForFrameWidth(frame_width);
  int close_height = 0;
  if (!ExtendClientIntoTitle() &&
      GetWidget()->widget_delegate()->ShouldShowCloseButton()) {
    const int close_margin =
        LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CLOSE_BUTTON_MARGIN);
    // Note: |close_margin| is not applied on the bottom of the icon.
    close_height = close_margin + close_->height();
  }

  if (!HasTitle()) {
    return content_margins_ +
           gfx::Insets(std::max(header_height, close_height), 0, 0, 0);
  }

  const int icon_height = title_icon_->GetPreferredSize().height();
  const int label_height = title()->GetHeightForWidth(
      frame_width - GetTitleLabelInsetsFromFrame().width());
  const int title_height =
      std::max(icon_height, label_height) + title_margins_.height();
  return content_margins_ +
         gfx::Insets(std::max(title_height + header_height, close_height), 0, 0,
                     0);
}

int BubbleFrameView::GetHeaderHeightForFrameWidth(int frame_width) const {
  return header_view_ && header_view_->GetVisible()
             ? header_view_->GetHeightForWidth(frame_width)
             : 0;
}

}  // namespace views

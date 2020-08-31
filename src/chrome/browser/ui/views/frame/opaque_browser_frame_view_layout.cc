// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"

namespace {

constexpr int kCaptionButtonHeight = 18;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLayout, public:

// statics

// The content edge images have a shadow built into them.
const int OpaqueBrowserFrameViewLayout::kContentEdgeShadowThickness = 2;

// The frame border is only visible in restored mode and is hardcoded to 4 px on
// each side regardless of the system window border size.
const int OpaqueBrowserFrameViewLayout::kFrameBorderThickness = 4;

// The frame has a 2 px 3D edge along the top.
const int OpaqueBrowserFrameViewLayout::kTopFrameEdgeThickness = 2;

// The frame has a 1 px 3D edge along the top.
const int OpaqueBrowserFrameViewLayout::kSideFrameEdgeThickness = 1;

// The icon is inset 1 px from the left frame border.
const int OpaqueBrowserFrameViewLayout::kIconLeftSpacing = 1;

// There is a 4 px gap between the icon and the title text.
const int OpaqueBrowserFrameViewLayout::kIconTitleSpacing = 4;

// The horizontal spacing to use in most cases when laying out things near the
// caption button area.
const int OpaqueBrowserFrameViewLayout::kCaptionSpacing = 5;

// The minimum vertical padding between the bottom of the caption buttons and
// the top of the content shadow.
const int OpaqueBrowserFrameViewLayout::kCaptionButtonBottomPadding = 3;

OpaqueBrowserFrameViewLayout::OpaqueBrowserFrameViewLayout()
    : available_space_leading_x_(0),
      available_space_trailing_x_(0),
      minimum_size_for_buttons_(0),
      placed_leading_button_(false),
      placed_trailing_button_(false),
      forced_window_caption_spacing_(-1),
      minimize_button_(nullptr),
      maximize_button_(nullptr),
      restore_button_(nullptr),
      close_button_(nullptr),
      window_icon_(nullptr),
      window_title_(nullptr),
      trailing_buttons_{views::FrameButton::kMinimize,
                        views::FrameButton::kMaximize,
                        views::FrameButton::kClose} {}

OpaqueBrowserFrameViewLayout::~OpaqueBrowserFrameViewLayout() {}

void OpaqueBrowserFrameViewLayout::SetButtonOrdering(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  leading_buttons_ = leading_buttons;
  trailing_buttons_ = trailing_buttons;
}

gfx::Rect OpaqueBrowserFrameViewLayout::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size,
    int total_width) const {
  const int x = available_space_leading_x_;
  const int available_width = available_space_trailing_x_ - x;
  return gfx::Rect(x, GetTabStripInsetsTop(false), std::max(0, available_width),
                   tabstrip_minimum_size.height());
}

gfx::Size OpaqueBrowserFrameViewLayout::GetMinimumSize(
    const views::View* host) const {
  // Ensure that we can fit the main browser view.
  gfx::Size min_size = delegate_->GetBrowserViewMinimumSize();

  // Ensure that we can, at minimum, hold our window controls and a tab strip.
  int top_width = minimum_size_for_buttons_;
  if (delegate_->IsTabStripVisible())
    top_width += delegate_->GetTabstripMinimumSize().width();
  min_size.set_width(std::max(min_size.width(), top_width));

  // Account for the frame.
  const int border_thickness = FrameBorderThickness(false);
  min_size.Enlarge(2 * border_thickness,
                   NonClientTopHeight(false) + border_thickness);

  return min_size;
}

gfx::Rect OpaqueBrowserFrameViewLayout::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopHeight(false);
  int border_thickness = FrameBorderThickness(false);
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int OpaqueBrowserFrameViewLayout::FrameBorderThickness(bool restored) const {
  return !restored && delegate_->IsFrameCondensed() ? 0 : kFrameBorderThickness;
}

int OpaqueBrowserFrameViewLayout::FrameTopBorderThickness(bool restored) const {
  int thickness = FrameBorderThickness(restored);
  if (restored || !delegate_->IsFrameCondensed())
    thickness += kNonClientExtraTopThickness;
  return thickness;
}

int OpaqueBrowserFrameViewLayout::NonClientTopHeight(bool restored) const {
  if (!delegate_->ShouldShowWindowTitle())
    return FrameTopBorderThickness(restored);

  // Adding 2px of vertical padding puts at least 1 px of space on the top and
  // bottom of the element.
  constexpr int kVerticalPadding = 2;
  const int icon_height =
      FrameTopThickness(restored) + delegate_->GetIconSize() + kVerticalPadding;
  const int caption_button_height = DefaultCaptionButtonY(restored) +
                                    kCaptionButtonHeight +
                                    kCaptionButtonBottomPadding;
  int web_app_button_height = 0;
  if (web_app_frame_toolbar_) {
    web_app_button_height =
        web_app_frame_toolbar_->GetPreferredSize().height() + kVerticalPadding;
  }
  return std::max(std::max(icon_height, caption_button_height),
                  web_app_button_height) +
         kContentEdgeShadowThickness;
}

int OpaqueBrowserFrameViewLayout::GetTabStripInsetsTop(bool restored) const {
  const int top = NonClientTopHeight(restored);
  return !restored && delegate_->IsFrameCondensed()
             ? top
             : (top + GetNonClientRestoredExtraThickness());
}

int OpaqueBrowserFrameViewLayout::FrameTopThickness(bool restored) const {
  return IsFrameEdgeVisible(restored) ? kTopFrameEdgeThickness : 0;
}

int OpaqueBrowserFrameViewLayout::FrameSideThickness(bool restored) const {
  return IsFrameEdgeVisible(restored) ? kSideFrameEdgeThickness : 0;
}

int OpaqueBrowserFrameViewLayout::DefaultCaptionButtonY(bool restored) const {
  // Maximized buttons start at window top, since the window has no border. This
  // offset is for the image (the actual clickable bounds extend all the way to
  // the top to take Fitts' Law into account).
  return !restored && delegate_->IsFrameCondensed()
             ? FrameBorderThickness(false)
             : views::NonClientFrameView::kFrameShadowThickness;
}

int OpaqueBrowserFrameViewLayout::CaptionButtonY(views::FrameButton button_id,
                                                 bool restored) const {
  return DefaultCaptionButtonY(restored);
}

gfx::Rect OpaqueBrowserFrameViewLayout::IconBounds() const {
  return window_icon_bounds_;
}

gfx::Rect OpaqueBrowserFrameViewLayout::CalculateClientAreaBounds(
    int width,
    int height) const {
  int top_height = NonClientTopHeight(false);
  int border_thickness = FrameBorderThickness(false);
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

int OpaqueBrowserFrameViewLayout::GetWindowCaptionSpacing(
    views::FrameButton button_id,
    bool leading_spacing,
    bool is_leading_button) const {
  if (leading_spacing) {
    if (is_leading_button) {
      // If we're the first button and maximized, add width to the right
      // hand side of the screen.
      return delegate_->IsFrameCondensed() && is_leading_button
                 ? kFrameBorderThickness -
                       views::NonClientFrameView::kFrameShadowThickness
                 : 0;
    }
    if (forced_window_caption_spacing_ >= 0)
      return forced_window_caption_spacing_;
  }
  return 0;
}

int OpaqueBrowserFrameViewLayout::GetNonClientRestoredExtraThickness() const {
  // Besides the frame border, there's empty space atop the window in restored
  // mode, to use to drag the window around.
  constexpr int kNonClientRestoredExtraThickness = 4;
  int thickness = kNonClientRestoredExtraThickness;
  if (delegate_->EverHasVisibleBackgroundTabShapes()) {
    thickness =
        std::max(thickness, BrowserNonClientFrameView::kMinimumDragHeight);
  }
  return thickness;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLayout, protected:

OpaqueBrowserFrameViewLayout::TopAreaPadding
OpaqueBrowserFrameViewLayout::GetTopAreaPadding(
    bool has_leading_buttons,
    bool has_trailing_buttons) const {
  const int padding = FrameBorderThickness(false);
  return {padding, padding};
}

bool OpaqueBrowserFrameViewLayout::IsFrameEdgeVisible(bool restored) const {
  return delegate_->UseCustomFrame() &&
         (restored || !delegate_->IsFrameCondensed());
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLayout, private:

void OpaqueBrowserFrameViewLayout::LayoutWindowControls() {
  // Keep a list of all buttons that we don't show.
  std::vector<views::FrameButton> buttons_not_shown;
  buttons_not_shown.push_back(views::FrameButton::kMaximize);
  buttons_not_shown.push_back(views::FrameButton::kMinimize);
  buttons_not_shown.push_back(views::FrameButton::kClose);

  if (delegate_->ShouldShowCaptionButtons()) {
    for (const auto& button : leading_buttons_) {
      ConfigureButton(button, ALIGN_LEADING);
      base::Erase(buttons_not_shown, button);
    }

    for (const auto& button : base::Reversed(trailing_buttons_)) {
      ConfigureButton(button, ALIGN_TRAILING);
      base::Erase(buttons_not_shown, button);
    }
  }

  for (const auto& button : buttons_not_shown)
    HideButton(button);
}

void OpaqueBrowserFrameViewLayout::LayoutTitleBar() {
  bool use_hidden_icon_location = true;

  int size = delegate_->GetIconSize();
  bool should_show_icon = delegate_->ShouldShowWindowIcon() && window_icon_;
  bool should_show_title = delegate_->ShouldShowWindowTitle() && window_title_;
  base::Optional<int> icon_spacing;

  if (should_show_icon || should_show_title || web_app_frame_toolbar_) {
    use_hidden_icon_location = false;

    // Our frame border has a different "3D look" than Windows'.  Theirs has
    // a more complex gradient on the top that they push their icon/title
    // below; then the maximized window cuts this off and the icon/title are
    // centered in the remaining space.  Because the apparent shape of our
    // border is simpler, using the same positioning makes things look
    // slightly uncentered with restored windows, so when the window is
    // restored, instead of calculating the remaining space from below the
    // frame border, we calculate from below the 3D edge.
    const int unavailable_px_at_top = FrameTopThickness(false);
    // When the icon is shorter than the minimum space we reserve for the
    // caption button, we vertically center it.  We want to bias rounding to
    // put extra space below the icon, since we'll use the same Y coordinate for
    // the title, and the majority of the font weight is below the centerline.
    const int available_height = NonClientTopHeight(false);
    const int icon_height =
        unavailable_px_at_top + size + kContentEdgeShadowThickness;
    const int y = unavailable_px_at_top + (available_height - icon_height) / 2;

    // Want same spacing adjacent to the icon as above when the icon is the
    // first element in the frame. We'll use this spacing again to ensure
    // appropriate spacing between icon and title.
    icon_spacing = y;
    if (web_app_frame_toolbar_ && leading_buttons_.empty())
      available_space_leading_x_ = FrameSideThickness(false) + *icon_spacing;
    else
      available_space_leading_x_ += kIconLeftSpacing;

    window_icon_bounds_ = gfx::Rect(available_space_leading_x_, y, size, size);
    available_space_leading_x_ += size;
    minimum_size_for_buttons_ += size;

    if (web_app_frame_toolbar_) {
      std::pair<int, int> remaining_bounds =
          web_app_frame_toolbar_->LayoutInContainer(available_space_leading_x_,
                                                    available_space_trailing_x_,
                                                    0, available_height);
      available_space_leading_x_ = remaining_bounds.first;
      available_space_trailing_x_ = remaining_bounds.second;
    }
  }

  if (should_show_icon)
    window_icon_->SetBoundsRect(window_icon_bounds_);

  if (window_title_) {
    SetViewVisibility(window_title_, should_show_title);
    if (should_show_title) {
      window_title_->SetText(delegate_->GetWindowTitle());

      // If possible, make space between icon and title symmetrical with space
      // between icon and frame.
      const int icon_title_spacing = (web_app_frame_toolbar_ && icon_spacing)
                                         ? *icon_spacing
                                         : kIconTitleSpacing;
      const int text_width =
          std::max(0, available_space_trailing_x_ - kCaptionSpacing -
                          available_space_leading_x_ - icon_title_spacing);
      window_title_->SetBounds(available_space_leading_x_ + icon_title_spacing,
                               window_icon_bounds_.y(), text_width,
                               window_icon_bounds_.height());
      available_space_leading_x_ += text_width + icon_title_spacing;
    }
  }

  if (use_hidden_icon_location) {
    if (placed_leading_button_) {
      // There are window button icons on the left. Don't size the hidden window
      // icon that people can double click on to close the window.
      window_icon_bounds_ = gfx::Rect();
    } else {
      // We set the icon bounds to a small rectangle in the top leading corner
      // if there are no icons on the leading side.
      const int frame_thickness = FrameBorderThickness(false);
      window_icon_bounds_ = gfx::Rect(
          frame_thickness + kIconLeftSpacing, frame_thickness, size, size);
    }
  }
}

void OpaqueBrowserFrameViewLayout::ConfigureButton(views::FrameButton button_id,
                                                   ButtonAlignment alignment) {
  switch (button_id) {
    case views::FrameButton::kMinimize: {
      SetViewVisibility(minimize_button_, true);
      SetBoundsForButton(button_id, minimize_button_, alignment);
      break;
    }
    case views::FrameButton::kMaximize: {
      // When the window is restored, we show a maximized button; otherwise, we
      // show a restore button.
      bool is_restored = !delegate_->IsMaximized() && !delegate_->IsMinimized();
      views::Button* invisible_button =
          is_restored ? restore_button_ : maximize_button_;
      SetViewVisibility(invisible_button, false);

      views::Button* visible_button =
          is_restored ? maximize_button_ : restore_button_;
      SetViewVisibility(visible_button, true);
      SetBoundsForButton(button_id, visible_button, alignment);
      break;
    }
    case views::FrameButton::kClose: {
      SetViewVisibility(close_button_, true);
      SetBoundsForButton(button_id, close_button_, alignment);
      break;
    }
  }
}

void OpaqueBrowserFrameViewLayout::HideButton(views::FrameButton button_id) {
  switch (button_id) {
    case views::FrameButton::kMinimize:
      SetViewVisibility(minimize_button_, false);
      break;
    case views::FrameButton::kMaximize:
      SetViewVisibility(restore_button_, false);
      SetViewVisibility(maximize_button_, false);
      break;
    case views::FrameButton::kClose:
      SetViewVisibility(close_button_, false);
      break;
  }
}

void OpaqueBrowserFrameViewLayout::SetBoundsForButton(
    views::FrameButton button_id,
    views::Button* button,
    ButtonAlignment alignment) {
  const int caption_y = CaptionButtonY(button_id, false);

  // There should always be the same number of non-shadow pixels visible to the
  // side of the caption buttons.  In maximized mode we extend buttons to the
  // screen top and the rightmost button to the screen right (or leftmost button
  // to the screen left, for left-aligned buttons) to obey Fitts' Law.
  const bool is_frame_condensed = delegate_->IsFrameCondensed();

  gfx::Size button_size = button->GetPreferredSize();
  if (delegate_->GetFrameButtonStyle() ==
      OpaqueBrowserFrameViewLayoutDelegate::FrameButtonStyle::kMdButton) {
    DCHECK_EQ(std::string(views::FrameCaptionButton::kViewClassName),
              button->GetClassName());
    constexpr int kCaptionButtonCenterSize =
        views::kCaptionButtonWidth -
        2 * views::kCaptionButtonInkDropDefaultCornerRadius;
    const int height = delegate_->GetTopAreaHeight();
    const int corner_radius =
        base::ClampToRange((height - kCaptionButtonCenterSize) / 2, 0,
                           views::kCaptionButtonInkDropDefaultCornerRadius);
    button_size = gfx::Size(views::kCaptionButtonWidth, height);
    button->SetPreferredSize(button_size);
    static_cast<views::FrameCaptionButton*>(button)->set_ink_drop_corner_radius(
        corner_radius);
  } else if (delegate_->GetFrameButtonStyle() ==
             OpaqueBrowserFrameViewLayoutDelegate::FrameButtonStyle::
                 kImageButton) {
    DCHECK_EQ(std::string(views::ImageButton::kViewClassName),
              button->GetClassName());
    auto* const image_button = static_cast<views::ImageButton*>(button);
    image_button->SetImageHorizontalAlignment(
        (alignment == ALIGN_LEADING) ? views::ImageButton::ALIGN_RIGHT
                                     : views::ImageButton::ALIGN_LEFT);
    image_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_BOTTOM);
  }

  TopAreaPadding top_area_padding = GetTopAreaPadding();

  switch (alignment) {
    case ALIGN_LEADING: {
      int extra_width = top_area_padding.leading;
      int button_start_spacing =
          GetWindowCaptionSpacing(button_id, true, !placed_leading_button_);

      available_space_leading_x_ += button_start_spacing;
      minimum_size_for_buttons_ += button_start_spacing;

      bool top_spacing_clickable = is_frame_condensed;
      bool start_spacing_clickable =
          is_frame_condensed && !placed_leading_button_;
      button->SetBounds(
          available_space_leading_x_ - (start_spacing_clickable
                                            ? button_start_spacing + extra_width
                                            : 0),
          top_spacing_clickable ? 0 : caption_y,
          button_size.width() + (start_spacing_clickable
                                     ? button_start_spacing + extra_width
                                     : 0),
          button_size.height() + (top_spacing_clickable ? caption_y : 0));

      int button_end_spacing =
          GetWindowCaptionSpacing(button_id, false, !placed_leading_button_);
      available_space_leading_x_ += button_size.width() + button_end_spacing;
      minimum_size_for_buttons_ += button_size.width() + button_end_spacing;
      placed_leading_button_ = true;
      break;
    }
    case ALIGN_TRAILING: {
      int extra_width = top_area_padding.trailing;
      int button_start_spacing =
          GetWindowCaptionSpacing(button_id, true, !placed_trailing_button_);

      available_space_trailing_x_ -= button_start_spacing;
      minimum_size_for_buttons_ += button_start_spacing;

      bool top_spacing_clickable = is_frame_condensed;
      bool start_spacing_clickable =
          is_frame_condensed && !placed_trailing_button_;
      button->SetBounds(
          available_space_trailing_x_ - button_size.width(),
          top_spacing_clickable ? 0 : caption_y,
          button_size.width() + (start_spacing_clickable
                                     ? button_start_spacing + extra_width
                                     : 0),
          button_size.height() + (top_spacing_clickable ? caption_y : 0));

      int button_end_spacing =
          GetWindowCaptionSpacing(button_id, false, !placed_trailing_button_);
      available_space_trailing_x_ -= button_size.width() + button_end_spacing;
      minimum_size_for_buttons_ += button_size.width() + button_end_spacing;
      placed_trailing_button_ = true;
      break;
    }
  }
}

void OpaqueBrowserFrameViewLayout::SetView(int id, views::View* view) {
  // Why do things this way instead of having an Init() method, where we're
  // passed the views we'll handle? Because OpaqueBrowserFrameView doesn't own
  // all the views which are part of it.
  switch (id) {
    case VIEW_ID_MINIMIZE_BUTTON:
      minimize_button_ = static_cast<views::Button*>(view);
      break;
    case VIEW_ID_MAXIMIZE_BUTTON:
      maximize_button_ = static_cast<views::Button*>(view);
      break;
    case VIEW_ID_RESTORE_BUTTON:
      restore_button_ = static_cast<views::Button*>(view);
      break;
    case VIEW_ID_CLOSE_BUTTON:
      close_button_ = static_cast<views::Button*>(view);
      break;
    case VIEW_ID_WINDOW_ICON:
      window_icon_ = view;
      break;
    case VIEW_ID_WINDOW_TITLE:
      if (view) {
        DCHECK_EQ(std::string(views::Label::kViewClassName),
                  view->GetClassName());
      }
      window_title_ = static_cast<views::Label*>(view);
      break;
    case VIEW_ID_WEB_APP_FRAME_TOOLBAR:
      if (view) {
        DCHECK_EQ(std::string(WebAppFrameToolbarView::kViewClassName),
                  view->GetClassName());
      }
      web_app_frame_toolbar_ = static_cast<WebAppFrameToolbarView*>(view);
      break;
    default:
      NOTREACHED() << "Unknown view id " << id;
      break;
  }
}

OpaqueBrowserFrameViewLayout::TopAreaPadding
OpaqueBrowserFrameViewLayout::GetTopAreaPadding() const {
  return GetTopAreaPadding(!leading_buttons_.empty(),
                           !trailing_buttons_.empty());
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLayout, views::LayoutManager:

void OpaqueBrowserFrameViewLayout::Layout(views::View* host) {
  TRACE_EVENT0("views.frame", "OpaqueBrowserFrameViewLayout::Layout");
  // Reset all our data so that everything is invisible.
  TopAreaPadding top_area_padding = GetTopAreaPadding();
  available_space_leading_x_ = top_area_padding.leading;
  available_space_trailing_x_ = host->width() - top_area_padding.trailing;
  minimum_size_for_buttons_ =
      available_space_leading_x_ + host->width() - available_space_trailing_x_;
  placed_leading_button_ = false;
  placed_trailing_button_ = false;

  LayoutWindowControls();
  LayoutTitleBar();

  // Any buttons/icon/title were laid out based on the frame border thickness,
  // but the tabstrip bounds need to be based on the non-client border thickness
  // on any side where there aren't other buttons forcing a larger inset.
  const int old_button_size =
      available_space_leading_x_ + host->width() - available_space_trailing_x_;
  const int min_button_width = FrameBorderThickness(false);
  available_space_leading_x_ =
      std::max(available_space_leading_x_, min_button_width);
  // The trailing corner is a mirror of the leading one.
  available_space_trailing_x_ =
      std::min(available_space_trailing_x_, host->width() - min_button_width);
  minimum_size_for_buttons_ += (available_space_leading_x_ + host->width() -
                                available_space_trailing_x_ - old_button_size);

  client_view_bounds_ = CalculateClientAreaBounds(
      host->width(), host->height());
}

gfx::Size OpaqueBrowserFrameViewLayout::GetPreferredSize(
    const views::View* host) const {
  // This is never used; NonClientView::CalculatePreferredSize() will be called
  // instead.
  NOTREACHED();
  return gfx::Size();
}

void OpaqueBrowserFrameViewLayout::ViewAdded(views::View* host,
                                             views::View* view) {
  SetView(view->GetID(), view);
}

void OpaqueBrowserFrameViewLayout::ViewRemoved(views::View* host,
                                               views::View* view) {
  SetView(view->GetID(), nullptr);
}

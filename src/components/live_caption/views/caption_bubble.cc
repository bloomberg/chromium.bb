// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/buildflags.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

// The CaptionBubbleLabel needs to be focusable in order for NVDA to enable
// document navigation. It is suspected that other screen readers on Windows and
// Linux will need this behavior, too. VoiceOver and ChromeVox do not need the
// label to be focusable.
#if BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY() && !defined(OS_MAC)
#define NEED_FOCUS_FOR_ACCESSIBILITY
#endif

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
#include "ui/accessibility/platform/ax_platform_node.h"
#endif

namespace {

// Formatting constants
static constexpr int kLineHeightDip = 24;
static constexpr int kNumLinesCollapsed = 2;
static constexpr int kNumLinesExpanded = 8;
static constexpr int kCornerRadiusDip = 4;
static constexpr int kSidePaddingDip = 18;
static constexpr int kButtonDip = 16;
static constexpr int kButtonCircleHighlightPaddingDip = 2;
static constexpr int kMaxWidthDip = 536;
// Margin of the bubble with respect to the context window.
static constexpr int kMinAnchorMarginDip = 20;
static constexpr uint16_t kCaptionBubbleAlpha = 230;  // 90% opacity
static constexpr char kPrimaryFont[] = "Roboto";
static constexpr char kSecondaryFont[] = "Arial";
static constexpr char kTertiaryFont[] = "sans-serif";
static constexpr int kFontSizePx = 16;
static constexpr double kDefaultRatioInParentX = 0.5;
static constexpr double kDefaultRatioInParentY = 1;
static constexpr int kErrorImageSizeDip = 20;
static constexpr int kErrorMessageBetweenChildSpacingDip = 16;
static constexpr int kNoActivityIntervalSeconds = 5;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. These should be the same as
// LiveCaptionSessionEvent in enums.xml.
enum class SessionEvent {
  // We began showing captions for an audio stream.
  kStreamStarted = 0,
  // The audio stream ended and the caption bubble closes.
  kStreamEnded = 1,
  // The close button was clicked, so we stopped listening to an audio stream.
  kCloseButtonClicked = 2,
  kMaxValue = kCloseButtonClicked,
};

void LogSessionEvent(SessionEvent event) {
  base::UmaHistogramEnumeration("Accessibility.LiveCaption.Session", event);
}

std::unique_ptr<views::ImageButton> BuildImageButton(
    views::Button::PressedCallback callback,
    const int tooltip_text_id) {
  auto button = views::CreateVectorImageButton(std::move(callback));
  button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
  button->SizeToPreferredSize();
  views::InstallCircleHighlightPathGenerator(
      button.get(), gfx::Insets(kButtonCircleHighlightPaddingDip));
  return button;
}

// ui::CaptionStyle styles are CSS strings that can sometimes have !important.
// This method removes " !important" if it exists at the end of a CSS string.
std::string MaybeRemoveCSSImportant(std::string css_string) {
  RE2::Replace(&css_string, "\\s+!important", "");
  return css_string;
}

// Parses a CSS color string that is in the form rgba and has a non-zero alpha
// value into an SkColor and sets it to be the value of the passed-in SkColor
// address. Returns whether or not the operation was a success.
//
// `css_string` is the CSS color string passed in. It is in the form
//     rgba(#,#,#,#). r, g, and b are integers between 0 and 255, inclusive.
//     a is a double between 0.0 and 1.0. There may be whitespace in between the
//     commas.
// `sk_color` is the address of an SKColor. If the operation is a success, the
//     function will set sk_color's value to be the parsed SkColor. If the
//     operation is not a success, sk_color's value will not change.
//
// As of spring 2021, all OS's use rgba to define the caption style colors.
// However, the ui::CaptionStyle spec also allows for the use of any valid CSS
// color spec. This function will need to be revisited should ui::CaptionStyle
// colors use non-rgba to define their colors.
bool ParseNonTransparentRGBACSSColorString(std::string css_string,
                                           SkColor* sk_color) {
  std::string rgba = MaybeRemoveCSSImportant(css_string);
  if (rgba.empty())
    return false;
  uint16_t r, g, b;
  double a;
  bool match = RE2::FullMatch(
      rgba, "rgba\\((\\d+),\\s*(\\d+),\\s*(\\d+),\\s*(\\d+\\.?\\d*)\\)", &r, &g,
      &b, &a);
  // If the opacity is set to 0 (fully transparent), we ignore the user's
  // preferred style and use our default color.
  if (!match || a == 0)
    return false;
  uint16_t a_int = base::ClampRound<uint16_t>(a * 255);
#if defined(OS_MAC)
  // On Mac, any opacity lower than 90% leaves rendering artifacts which make
  // it appear like there is a layer of faint text beneath the actual text.
  // TODO(crbug.com/1199419): Fix the rendering issue and then remove this
  // workaround.
  a_int = std::max(kCaptionBubbleAlpha, a_int);
#endif
  *sk_color = SkColorSetARGB(a_int, r, g, b);
  return match;
}

}  // namespace

namespace captions {
// CaptionBubble implementation of BubbleFrameView. This class takes care
// of making the caption draggable.
class CaptionBubbleFrameView : public views::BubbleFrameView {
 public:
  METADATA_HEADER(CaptionBubbleFrameView);
  explicit CaptionBubbleFrameView(std::vector<views::View*> buttons)
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()),
        buttons_(buttons) {
    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::DIALOG_SHADOW,
        gfx::kPlaceholderColor);
    border->SetCornerRadius(kCornerRadiusDip);
    views::BubbleFrameView::SetBubbleBorder(std::move(border));
  }

  ~CaptionBubbleFrameView() override = default;
  CaptionBubbleFrameView(const CaptionBubbleFrameView&) = delete;
  CaptionBubbleFrameView& operator=(const CaptionBubbleFrameView&) = delete;

  // TODO(crbug.com/1055150): This does not work on Linux because the bubble is
  // not a top-level view, so it doesn't receive events. See crbug.com/1074054
  // for more about why it doesn't work.
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // |point| is in coordinates relative to CaptionBubbleFrameView, i.e.
    // (0,0) is the upper left corner of this view. Convert it to screen
    // coordinates to see whether one of the buttons contains this point.
    // If it is, return HTCLIENT, so that the click is sent through to be
    // handled by CaptionBubble::BubblePressed().
    gfx::Point point_in_screen =
        GetBoundsInScreen().origin() + gfx::Vector2d(point.x(), point.y());
    for (views::View* button : buttons_) {
      if (button->GetBoundsInScreen().Contains(point_in_screen))
        return HTCLIENT;
    }

    // Ensure it's within the BubbleFrameView. This takes into account the
    // rounded corners and drop shadow of the BubbleBorder.
    int hit = views::BubbleFrameView::NonClientHitTest(point);

    // After BubbleFrameView::NonClientHitTest processes the bubble-specific
    // hits such as the rounded corners, it checks hits to the bubble's client
    // view. Any hits to ClientFrameView::NonClientHitTest return HTCLIENT or
    // HTNOWHERE. Override these to return HTCAPTION in order to make the
    // entire widget draggable.
    return (hit == HTCLIENT || hit == HTNOWHERE) ? HTCAPTION : hit;
  }

 private:
  std::vector<views::View*> buttons_;
};

BEGIN_METADATA(CaptionBubbleFrameView, views::BubbleFrameView)
END_METADATA

class CaptionBubbleLabelAXModeObserver;

// CaptionBubble implementation of Label. This class takes care of setting up
// the accessible virtual views of the label in order to support braille
// accessibility. The CaptionBubbleLabel is a readonly document with a paragraph
// inside. Inside the paragraph are staticText nodes, one for each visual line
// in the rendered text of the label. These staticText nodes are shown on a
// braille display so that a braille user can read the caption text line by
// line.
class CaptionBubbleLabel : public views::Label {
 public:
  METADATA_HEADER(CaptionBubbleLabel);
#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
  CaptionBubbleLabel() {
    ax_mode_observer_ =
        std::make_unique<CaptionBubbleLabelAXModeObserver>(this);
    SetFocusBehaviorForAccessibility();
  }
#else
  CaptionBubbleLabel() = default;
#endif
  ~CaptionBubbleLabel() override = default;
  CaptionBubbleLabel(const CaptionBubbleLabel&) = delete;
  CaptionBubbleLabel& operator=(const CaptionBubbleLabel&) = delete;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Views are not supposed to be documents (see
    // `ViewAccessibility::IsValidRoleForViews` for more information) but we
    // make an exception here. The CaptionBubbleLabel is designed to be
    // interacted with by a braille display in virtual buffer mode. In order to
    // activate the virtual buffer in NVDA, we set the CaptionBubbleLabel to be
    // a readonly document.
    node_data->role = ax::mojom::Role::kDocument;
    node_data->SetRestriction(ax::mojom::Restriction::kReadOnly);
#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
    // Focusable nodes generally must have a name, but the purpose of focusing
    // this document is to let the user read the static text nodes inside.
    node_data->SetNameFrom(ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
#endif
  }

  void SetText(const std::u16string& text) override {
    views::Label::SetText(text);

    auto& ax_lines = GetViewAccessibility().virtual_children();
    if (text.empty() && !ax_lines.empty()) {
      GetViewAccessibility().RemoveAllVirtualChildViews();
      NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
      return;
    }

    const size_t num_lines = GetRequiredLines();
    size_t start = 0;
    for (size_t i = 0; i < num_lines - 1; ++i) {
      size_t end = GetTextIndexOfLine(i + 1);
      std::u16string substring = text.substr(start, end - start);
      UpdateAXLine(substring, i, gfx::Range(start, end));
      start = end;
    }
    std::u16string substring = text.substr(start, text.size() - start);
    if (!substring.empty()) {
      UpdateAXLine(substring, num_lines - 1, gfx::Range(start, text.size()));
    }

    // Remove all ax_lines that don't have a corresponding line.
    size_t num_ax_lines = ax_lines.size();
    for (size_t i = num_lines; i < num_ax_lines; ++i) {
      GetViewAccessibility().RemoveVirtualChildView(ax_lines.back().get());
      NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
    }
  }

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
  // The CaptionBubbleLabel needs to be focusable in order for NVDA to enable
  // document navigation. Making the CaptionBubbleLabel focusable means it gets
  // a tabstop, so it should only be focusable for screen reader users.
  void SetFocusBehaviorForAccessibility() {
    if (ui::AXPlatformNode::GetAccessibilityMode().has_mode(
            ui::AXMode::kScreenReader)) {
      SetFocusBehavior(FocusBehavior::ALWAYS);
    } else {
      SetFocusBehavior(FocusBehavior::NEVER);
    }
  }
#endif

 private:
  void UpdateAXLine(const std::u16string& line_text,
                    const size_t line_index,
                    const gfx::Range& text_range) {
    auto& ax_lines = GetViewAccessibility().virtual_children();

    // Add a new virtual child for a new line of text.
    DCHECK(line_index <= ax_lines.size());
    if (line_index == ax_lines.size()) {
      auto ax_line = std::make_unique<views::AXVirtualView>();
      ax_line->GetCustomData().role = ax::mojom::Role::kStaticText;
      GetViewAccessibility().AddVirtualChildView(std::move(ax_line));
      NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
    }

    // Set the virtual child's name as line text.
    ui::AXNodeData& ax_node_data = ax_lines[line_index]->GetCustomData();
    if (base::UTF8ToUTF16(ax_node_data.GetStringAttribute(
            ax::mojom::StringAttribute::kName)) != line_text) {
      ax_node_data.SetName(line_text);
      std::vector<gfx::Rect> bounds = GetSubstringBounds(text_range);
      ax_node_data.relative_bounds.bounds = gfx::RectF(bounds[0]);
      ax_lines[line_index]->NotifyAccessibilityEvent(
          ax::mojom::Event::kTextChanged);
    }
  }

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
  std::unique_ptr<CaptionBubbleLabelAXModeObserver> ax_mode_observer_;
#endif
};

BEGIN_METADATA(CaptionBubbleLabel, views::Label)
END_METADATA

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
// A helper class to the CaptionBubbleLabel which observes AXMode changes and
// updates the CaptionBubbleLabel focus behavior in response.
// TODO(crbug.com/1191091): Implement a ui::AXModeObserver::OnAXModeRemoved
// method which observes the removal of AXModes. Without that, the caption
// bubble label will remain focusable once accessibility is enabled, even if
// accessibility is later disabled.
class CaptionBubbleLabelAXModeObserver : public ui::AXModeObserver {
 public:
  explicit CaptionBubbleLabelAXModeObserver(CaptionBubbleLabel* owner)
      : owner_(owner) {
    ui::AXPlatformNode::AddAXModeObserver(this);
  }

  ~CaptionBubbleLabelAXModeObserver() override {
    ui::AXPlatformNode::RemoveAXModeObserver(this);
  }

  CaptionBubbleLabelAXModeObserver(const CaptionBubbleLabelAXModeObserver&) =
      delete;
  CaptionBubbleLabelAXModeObserver& operator=(
      const CaptionBubbleLabelAXModeObserver&) = delete;

  void OnAXModeAdded(ui::AXMode mode) override {
    owner_->SetFocusBehaviorForAccessibility();
  }

 private:
  raw_ptr<CaptionBubbleLabel> owner_;
};
#endif

CaptionBubble::CaptionBubble(base::OnceClosure destroyed_callback,
                             bool hide_on_inactivity)
    : destroyed_callback_(std::move(destroyed_callback)),
      hide_on_inactivity_(hide_on_inactivity),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  // Bubbles that use transparent colors should not paint their ClientViews to a
  // layer as doing so could result in visual artifacts.
  SetPaintClientToLayer(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  // While not shown, the title is still used to identify the window in the
  // window switcher.
  SetShowTitle(false);
  SetTitle(IDS_LIVE_CAPTION_BUBBLE_TITLE);
  set_has_parent(false);

  // No need to set up timer if the bubble is not hidden on inactivity.
  if (!hide_on_inactivity_)
    return;

  inactivity_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, base::Seconds(kNoActivityIntervalSeconds),
      base::BindRepeating(&CaptionBubble::OnInactivityTimeout,
                          base::Unretained(this)),
      tick_clock_);
  inactivity_timer_->Stop();
}

CaptionBubble::~CaptionBubble() {
  if (model_)
    model_->RemoveObserver();
}

gfx::Rect CaptionBubble::GetBubbleBounds() {
  // Bubble bounds are what the computed bubble bounds would be, taking into
  // account the current bubble size.
  gfx::Rect bubble_bounds = views::BubbleDialogDelegateView::GetBubbleBounds();
  // Widget bounds are where the bubble currently is in space.
  gfx::Rect widget_bounds = GetWidget()->GetWindowBoundsInScreen();
  // Use the widget x and y to keep the bubble oriented at its current location,
  // and use the bubble width and height to set the correct bubble size.
  return gfx::Rect(widget_bounds.x(), widget_bounds.y(), bubble_bounds.width(),
                   bubble_bounds.height());
}

void CaptionBubble::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
  UseCompactMargins();
  set_close_on_deactivate(false);
  SetCanActivate(true);

  views::View* header_container = new views::View();
  header_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  views::View* content_container = new views::View();
  content_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(gfx::Insets(0, kSidePaddingDip))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width*/ true));

  auto label = std::make_unique<CaptionBubbleLabel>();
  label->SetMultiLine(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);
  label->SetTooltipText(std::u16string());
  // Render text truncates the end of text that is greater than 10000 chars.
  // While it is unlikely that the text will exceed 10000 chars, it is not
  // impossible, if the speech service sends a very long transcription_result.
  // In order to guarantee that the caption bubble displays the last lines, and
  // in order to ensure that caption_bubble_->GetTextIndexOfLine() is correct,
  // set the truncate_length to 0 to ensure that it never truncates.
  label->SetTruncateLength(0);

  auto title = std::make_unique<views::Label>();
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));
  title->GetViewAccessibility().OverrideIsIgnored(true);

  auto error_text = std::make_unique<views::Label>();
  error_text->SetBackgroundColor(SK_ColorTRANSPARENT);
  error_text->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  error_text->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_ERROR));

  auto error_icon = std::make_unique<views::ImageView>();

  auto error_message = std::make_unique<views::View>();
  error_message
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kErrorMessageBetweenChildSpacingDip))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  error_message->SetVisible(false);

  views::Button::PressedCallback expand_or_collapse_callback =
      base::BindRepeating(&CaptionBubble::ExpandOrCollapseButtonPressed,
                          base::Unretained(this));
  auto expand_button = BuildImageButton(expand_or_collapse_callback,
                                        IDS_LIVE_CAPTION_BUBBLE_EXPAND);
  expand_button->SetVisible(!is_expanded_);

  auto collapse_button = BuildImageButton(
      std::move(expand_or_collapse_callback), IDS_LIVE_CAPTION_BUBBLE_COLLAPSE);
  collapse_button->SetVisible(is_expanded_);

  auto back_to_tab_button = BuildImageButton(
      base::BindRepeating(&CaptionBubble::BackToTabButtonPressed,
                          base::Unretained(this)),
      IDS_LIVE_CAPTION_BUBBLE_BACK_TO_TAB);
  back_to_tab_button->SetVisible(false);

  auto close_button =
      BuildImageButton(base::BindRepeating(&CaptionBubble::CloseButtonPressed,
                                           base::Unretained(this)),
                       IDS_LIVE_CAPTION_BUBBLE_CLOSE);

  back_to_tab_button_ =
      header_container->AddChildView(std::move(back_to_tab_button));
  close_button_ = header_container->AddChildView(std::move(close_button));

  title_ = content_container->AddChildView(std::move(title));
  label_ = content_container->AddChildView(std::move(label));

  error_icon_ = error_message->AddChildView(std::move(error_icon));
  error_text_ = error_message->AddChildView(std::move(error_text));
  error_message_ = content_container->AddChildView(std::move(error_message));

  expand_button_ = content_container->AddChildView(std::move(expand_button));
  collapse_button_ =
      content_container->AddChildView(std::move(collapse_button));

  AddChildView(std::move(header_container));
  AddChildView(std::move(content_container));

  SetCaptionBubbleStyle();
  UpdateContentSize();
}

void CaptionBubble::OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                             views::Widget* widget) const {
  params->type = views::Widget::InitParams::TYPE_WINDOW;
  params->z_order = ui::ZOrderLevel::kFloatingWindow;
  params->visible_on_all_workspaces = true;
  params->name = "LiveCaptionWindow";
}

bool CaptionBubble::ShouldShowCloseButton() const {
  // We draw our own close button so that we can capture the button presses and
  // so we can customize its appearance.
  return false;
}

std::unique_ptr<views::NonClientFrameView>
CaptionBubble::CreateNonClientFrameView(views::Widget* widget) {
  std::vector<views::View*> buttons = {back_to_tab_button_, close_button_,
                                       expand_button_, collapse_button_};
  auto frame = std::make_unique<CaptionBubbleFrameView>(buttons);
  frame_ = frame.get();
  return frame;
}

void CaptionBubble::OnWidgetBoundsChanged(views::Widget* widget,
                                          const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget, GetWidget());
  if (!hide_on_inactivity_)
    return;

  // If the widget is visible and unfocused, probably due to a mouse drag, reset
  // the inactivity timer.
  if (GetWidget()->IsVisible() && !HasFocus())
    inactivity_timer_->Reset();
}

void CaptionBubble::OnWidgetActivationChanged(views::Widget* widget,
                                              bool active) {
  DCHECK_EQ(widget, GetWidget());
  if (!hide_on_inactivity_)
    return;

  if (active) {
    inactivity_timer_->Stop();
  } else {
    inactivity_timer_->Reset();
  }
}

void CaptionBubble::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(title_->GetText());
}

std::u16string CaptionBubble::GetAccessibleWindowTitle() const {
  return title_->GetText();
}

void CaptionBubble::BackToTabButtonPressed() {
  DCHECK(model_);
  DCHECK(model_->GetContext()->IsActivatable());
  model_->GetContext()->Activate();
}

void CaptionBubble::CloseButtonPressed() {
  LogSessionEvent(SessionEvent::kCloseButtonClicked);
  if (model_)
    model_->Close();
}

void CaptionBubble::ExpandOrCollapseButtonPressed() {
  is_expanded_ = !is_expanded_;
  base::UmaHistogramBoolean("Accessibility.LiveCaption.ExpandBubble",
                            is_expanded_);
  views::Button *old_button = collapse_button_, *new_button = expand_button_;
  if (is_expanded_)
    std::swap(old_button, new_button);
  bool button_had_focus = old_button->HasFocus();
  OnIsExpandedChanged();
  // TODO(crbug.com/1055150): Ensure that the button keeps focus on mac.
  if (button_had_focus)
    new_button->RequestFocus();

  if (hide_on_inactivity_)
    inactivity_timer_->Reset();
}

void CaptionBubble::SetModel(CaptionBubbleModel* model) {
  if (model_)
    model_->RemoveObserver();
  model_ = model;
  if (model_) {
    model_->SetObserver(this);
    back_to_tab_button_->SetVisible(model_->GetContext()->IsActivatable());
  } else {
    UpdateBubbleVisibility();
  }
}

void CaptionBubble::OnTextChanged() {
  DCHECK(model_);
  std::string text = model_->GetFullText();
  label_->SetText(base::UTF8ToUTF16(text));
  UpdateBubbleAndTitleVisibility();

  if (hide_on_inactivity_ && GetWidget()->IsVisible())
    inactivity_timer_->Reset();
}

void CaptionBubble::OnErrorChanged() {
  DCHECK(model_);
  bool has_error = model_->HasError();
  label_->SetVisible(!has_error);
  error_message_->SetVisible(has_error);
  expand_button_->SetVisible(!has_error && !is_expanded_);
  collapse_button_->SetVisible(!has_error && is_expanded_);

  // The error is only 1 line, so redraw the bubble.
  Redraw();
}

void CaptionBubble::OnIsExpandedChanged() {
  expand_button_->SetVisible(!is_expanded_);
  collapse_button_->SetVisible(is_expanded_);

  // The change of expanded state may cause the title to change visibility, and
  // it surely causes the content height to change, so redraw the bubble.
  Redraw();
}

void CaptionBubble::UpdateBubbleAndTitleVisibility() {
  // Show the title if there is room for it and no error.
  title_->SetVisible(model_ && !model_->HasError() &&
                     GetNumLinesInLabel() <
                         static_cast<size_t>(GetNumLinesVisible()));
  UpdateBubbleVisibility();
}

void CaptionBubble::UpdateBubbleVisibility() {
  DCHECK(GetWidget());
  // If there is no model set, do not show the bubble.
  if (!model_) {
    Hide();
    return;
  }

  // Hide the widget if the model is closed or the bubble has no activity.
  // Activity is defined as transcription received from the speech service or
  // user interacting with the bubble through focus, pressing buttons, or
  // dragging.
  if (model_->IsClosed() || !HasActivity()) {
    Hide();
    return;
  }

  // Show the widget if it has text or an error to display.
  if (!model_->GetFullText().empty() || model_->HasError()) {
    ShowInactive();
    return;
  }

  // No text and no error. Hide it.
  Hide();
}

void CaptionBubble::UpdateCaptionStyle(
    absl::optional<ui::CaptionStyle> caption_style) {
  caption_style_ = caption_style;
  SetCaptionBubbleStyle();
  Redraw();
}

size_t CaptionBubble::GetTextIndexOfLineInLabel(size_t line) const {
  return label_->GetTextIndexOfLine(line);
}

size_t CaptionBubble::GetNumLinesInLabel() const {
  return label_->GetRequiredLines();
}

int CaptionBubble::GetNumLinesVisible() {
  return is_expanded_ ? kNumLinesExpanded : kNumLinesCollapsed;
}

void CaptionBubble::SetCaptionBubbleStyle() {
  SetTextSizeAndFontFamily();
  SetTextColor();
  SetBackgroundColor();
}

double CaptionBubble::GetTextScaleFactor() {
  double textScaleFactor = 1;
  if (caption_style_) {
    std::string text_size = MaybeRemoveCSSImportant(caption_style_->text_size);
    if (!text_size.empty()) {
      // ui::CaptionStyle states that text_size is percentage as a CSS string.
      bool match =
          RE2::FullMatch(text_size, "(\\d+\\.?\\d*)%", &textScaleFactor);
      textScaleFactor = match ? textScaleFactor / 100 : 1;
    }
  }
  return textScaleFactor;
}

void CaptionBubble::SetTextSizeAndFontFamily() {
  double textScaleFactor = GetTextScaleFactor();

  std::vector<std::string> font_names;
  if (caption_style_) {
    std::string font_family =
        MaybeRemoveCSSImportant(caption_style_->font_family);
    if (!font_family.empty())
      font_names.push_back(font_family);
  }
  font_names.push_back(kPrimaryFont);
  font_names.push_back(kSecondaryFont);
  font_names.push_back(kTertiaryFont);

  const gfx::FontList font_list =
      gfx::FontList(font_names, gfx::Font::FontStyle::NORMAL,
                    kFontSizePx * textScaleFactor, gfx::Font::Weight::NORMAL);
  label_->SetFontList(font_list);
  title_->SetFontList(font_list.DeriveWithStyle(gfx::Font::FontStyle::ITALIC));
  error_text_->SetFontList(font_list);

  label_->SetLineHeight(kLineHeightDip * textScaleFactor);
  label_->SetMaximumWidth(kMaxWidthDip * textScaleFactor - kSidePaddingDip * 2);
  title_->SetLineHeight(kLineHeightDip * textScaleFactor);
  error_text_->SetLineHeight(kLineHeightDip * textScaleFactor);
  error_icon_->SetImageSize(gfx::Size(kErrorImageSizeDip * textScaleFactor,
                                      kErrorImageSizeDip * textScaleFactor));
}

void CaptionBubble::SetTextColor() {
  SkColor text_color = SK_ColorWHITE;  // The default text color is white.
  if (caption_style_)
    ParseNonTransparentRGBACSSColorString(caption_style_->text_color,
                                          &text_color);
  label_->SetEnabledColor(text_color);
  title_->SetEnabledColor(text_color);
  error_text_->SetEnabledColor(text_color);

  error_icon_->SetImage(
      gfx::CreateVectorIcon(vector_icons::kErrorOutlineIcon, text_color));
  views::SetImageFromVectorIcon(back_to_tab_button_, vector_icons::kLaunchIcon,
                                kButtonDip, text_color);
  views::SetImageFromVectorIcon(close_button_, vector_icons::kCloseRoundedIcon,
                                kButtonDip, text_color);
  views::SetImageFromVectorIcon(expand_button_, vector_icons::kCaretDownIcon,
                                kButtonDip, text_color);
  views::SetImageFromVectorIcon(collapse_button_, vector_icons::kCaretUpIcon,
                                kButtonDip, text_color);

  views::InkDrop::Get(back_to_tab_button_)->SetBaseColor(text_color);
  views::InkDrop::Get(close_button_)->SetBaseColor(text_color);
  views::InkDrop::Get(expand_button_)->SetBaseColor(text_color);
  views::InkDrop::Get(collapse_button_)->SetBaseColor(text_color);
}

void CaptionBubble::SetBackgroundColor() {
  // The default background color is Google Grey 900 with 90% opacity.
  SkColor background_color =
      SkColorSetA(gfx::kGoogleGrey900, kCaptionBubbleAlpha);
  if (caption_style_ && !ParseNonTransparentRGBACSSColorString(
                            caption_style_->window_color, &background_color)) {
    ParseNonTransparentRGBACSSColorString(caption_style_->background_color,
                                          &background_color);
  }
  set_color(background_color);
  OnThemeChanged();  // Need to call `OnThemeChanged` after calling `set_color`.
}

void CaptionBubble::UpdateContentSize() {
  double text_scale_factor = GetTextScaleFactor();
  int width = kMaxWidthDip * text_scale_factor;
  int content_height =
      (model_ && model_->HasError())
          ? kLineHeightDip * text_scale_factor
          : kLineHeightDip * GetNumLinesVisible() * text_scale_factor;
  // The title takes up 1 line.
  int label_height = title_->GetVisible()
                         ? content_height - kLineHeightDip * text_scale_factor
                         : content_height;
  label_->SetPreferredSize(gfx::Size(width - kSidePaddingDip, label_height));
  // The header height is the same as the close button height. The footer height
  // is the same as the expand button height.
  SetPreferredSize(gfx::Size(
      width, content_height + close_button_->GetPreferredSize().height() +
                 expand_button_->GetPreferredSize().height()));
}

void CaptionBubble::Redraw() {
  UpdateBubbleAndTitleVisibility();
  UpdateContentSize();
  SizeToContents();
}

void CaptionBubble::ShowInactive() {
  DCHECK(model_);
  if (GetWidget()->IsVisible())
    return;

  GetWidget()->ShowInactive();
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_LIVE_CAPTION_BUBBLE_APPEAR_SCREENREADER_ANNOUNCEMENT));
  LogSessionEvent(SessionEvent::kStreamStarted);

  // If the caption bubble has already been shown, do not reposition it.
  if (has_been_shown_)
    return;
  has_been_shown_ = true;

  // The first time that the caption bubble is shown, place it at the bottom
  // center of the context widget for the currently set model. We do the
  // placement at this time to ensure that the caption bubble is positioned
  // where the user will spot it. If there are multiple browser windows open,
  // and the user plays media on the second window, the caption bubble will show
  // up in the bottom center of the second window, which is where the user is
  // already looking. It also ensures that the caption bubble will appear in the
  // right workspace if a user has Chrome windows open on multiple workspaces.
  if (!model_->GetContext()->GetBounds().has_value())
    return;
  gfx::Rect context_rect = model_->GetContext()->GetBounds().value();

  context_rect.Inset(gfx::Insets(kMinAnchorMarginDip));
  gfx::Rect bubble_bounds = GetBubbleBounds();

  // The placement is based on the ratio between the center of the widget and
  // the center of the context_rect.
  int target_x = context_rect.x() +
                 context_rect.width() * kDefaultRatioInParentX -
                 bubble_bounds.width() / 2.0;
  int target_y = context_rect.y() +
                 context_rect.height() * kDefaultRatioInParentY -
                 bubble_bounds.height() / 2.0;
  gfx::Rect target_bounds = gfx::Rect(target_x, target_y, bubble_bounds.width(),
                                      bubble_bounds.height());
  if (!context_rect.Contains(target_bounds)) {
    target_bounds.AdjustToFit(context_rect);
  }

  GetWidget()->SetBounds(target_bounds);
}

void CaptionBubble::Hide() {
  if (!GetWidget()->IsVisible())
    return;
  GetWidget()->Hide();
  LogSessionEvent(SessionEvent::kStreamEnded);
}

void CaptionBubble::OnInactivityTimeout() {
  Hide();

  // Clear the partial and final text in the caption bubble model and the label.
  // Does not affect the speech service. The speech service will emit a final
  // result after ~10-15 seconds of no audio which the caption bubble will
  // receive but will not display. If the speech service is in the middle of a
  // recognition phrase, and the caption bubble regains activity (such as if the
  // audio stream restarts), the speech service will emit partial results that
  // contain text cleared by the UI.
  if (model_)
    model_->ClearText();
}

bool CaptionBubble::HasActivity() {
  return model_ &&
         ((inactivity_timer_ && inactivity_timer_->IsRunning()) || HasFocus() ||
          !model_->GetFullText().empty() || model_->HasError());
}

views::Label* CaptionBubble::GetLabelForTesting() {
  return static_cast<views::Label*>(label_);
}

base::RetainingOneShotTimer* CaptionBubble::GetInactivityTimerForTesting() {
  return inactivity_timer_.get();
}

BEGIN_METADATA(CaptionBubble, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace captions

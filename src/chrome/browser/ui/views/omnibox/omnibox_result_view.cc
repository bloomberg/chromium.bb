// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include <limits.h>

#include <algorithm>  // NOLINT

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/browser/ui/views/omnibox/remove_suggestion_bubble.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_runner.h"

#if defined(OS_WIN)
#include "base/win/atl.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, public:

OmniboxResultView::OmniboxResultView(
    OmniboxPopupContentsView* popup_contents_view,
    int model_index)
    : popup_contents_view_(popup_contents_view),
      model_index_(model_index),
      is_hovered_(false),
      animation_(new gfx::SlideAnimation(this)) {
  CHECK_GE(model_index, 0);

  AddChildView(suggestion_view_ = new OmniboxMatchCellView(this));
  AddChildView(keyword_view_ = new OmniboxMatchCellView(this));

  keyword_view_->icon()->EnableCanvasFlippingForRTLUI(true);
  keyword_view_->icon()->SetImage(gfx::CreateVectorIcon(
      omnibox::kKeywordSearchIcon, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetColor(OmniboxPart::RESULTS_ICON)));
  keyword_view_->icon()->SizeToPreferredSize();

  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxSuggestionTransparencyOptions)) {
    // TODO(tommycli): Replace this with the real translated string from UX.
    context_menu_contents_.AddItem(COMMAND_REMOVE_SUGGESTION,
                                   base::ASCIIToUTF16("Remove suggestion..."));
    set_context_menu_controller(this);
  }
}

OmniboxResultView::~OmniboxResultView() {}

SkColor OmniboxResultView::GetColor(OmniboxPart part) const {
  if (!AutocompleteMatch::IsSearchType(match_.type)) {
    // These recoloring experiments affect all non-search matches.
    bool color_titles_blue =
        base::FeatureList::IsEnabled(
            omnibox::kUIExperimentBlueTitlesAndGrayUrlsOnPageSuggestions) ||
        base::FeatureList::IsEnabled(
            omnibox::kUIExperimentBlueTitlesOnPageSuggestions);
    bool color_urls_gray = base::FeatureList::IsEnabled(
        omnibox::kUIExperimentBlueTitlesAndGrayUrlsOnPageSuggestions);

    // If these recoloring experiments are enabled, change |part| to fetch an
    // alternate color for this text.
    if (color_titles_blue && part == OmniboxPart::RESULTS_TEXT_DEFAULT)
      part = OmniboxPart::RESULTS_TEXT_URL;
    else if (color_urls_gray && part == OmniboxPart::RESULTS_TEXT_URL)
      part = OmniboxPart::RESULTS_TEXT_DIMMED;
  }

  return GetOmniboxColor(part, GetTint(), GetThemeState());
}

void OmniboxResultView::SetMatch(const AutocompleteMatch& match) {
  match_ = match.GetMatchWithContentsAndDescriptionPossiblySwapped();
  animation_->Reset();
  is_hovered_ = false;
  suggestion_view_->OnMatchUpdate(this, match_);
  keyword_view_->OnMatchUpdate(this, match_);

  // Set up possible button.
  if (match.ShouldShowButton()) {
    if (!OmniboxFieldTrial::IsTabSwitchLogicReversed()) {
      suggestion_tab_switch_button_ = std::make_unique<OmniboxTabSwitchButton>(
          popup_contents_view_, this,
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_HINT),
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_SHORT_HINT),
          omnibox::kSwitchIcon);
    } else {
      suggestion_tab_switch_button_ = std::make_unique<OmniboxTabSwitchButton>(
          // TODO(krb): Make official strings when we accept the feature.
          popup_contents_view_, this, base::ASCIIToUTF16("Open in this tab"),
          base::ASCIIToUTF16("Open"), omnibox::kSwitchIcon);
    }

    suggestion_tab_switch_button_->set_owned_by_client();
    AddChildView(suggestion_tab_switch_button_.get());
  } else {
    suggestion_tab_switch_button_.reset();
  }

  Invalidate();
  Layout();
}

void OmniboxResultView::ShowKeyword(bool show_keyword) {
  if (show_keyword)
    animation_->Show();
  else
    animation_->Hide();
}

void OmniboxResultView::Invalidate() {
  bool high_contrast =
      GetNativeTheme() && GetNativeTheme()->UsesHighContrastColors();
  // TODO(tapted): Consider using background()->SetNativeControlColor() and
  // always have a background.
  SetBackground((GetThemeState() == OmniboxPartState::NORMAL && !high_contrast)
                    ? nullptr
                    : views::CreateSolidBackground(
                          GetColor(OmniboxPart::RESULTS_BACKGROUND)));

  // Reapply the dim color to account for the highlight state.
  suggestion_view_->separator()->ApplyTextColor(
      OmniboxPart::RESULTS_TEXT_DIMMED);
  keyword_view_->separator()->ApplyTextColor(OmniboxPart::RESULTS_TEXT_DIMMED);
  if (suggestion_tab_switch_button_)
    suggestion_tab_switch_button_->UpdateBackground();

  // Recreate the icons in case the color needs to change.
  // Note: if this is an extension icon or favicon then this can be done in
  //       SetMatch() once (rather than repeatedly, as happens here). There may
  //       be an optimization opportunity here.
  // TODO(dschuyler): determine whether to optimize the color changes.
  suggestion_view_->icon()->SetImage(GetIcon().ToImageSkia());
  keyword_view_->icon()->SetImage(gfx::CreateVectorIcon(
      omnibox::kKeywordSearchIcon, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetColor(OmniboxPart::RESULTS_ICON)));

  // Answers use their own styling for additional content text and the
  // description text, whereas non-answer suggestions use the match text and
  // calculated classifications for the description text.
  bool blue_search_query = base::FeatureList::IsEnabled(
      omnibox::kUIExperimentBlueSearchLoopAndSearchQuery);
  if (match_.answer) {
    const bool reverse = OmniboxFieldTrial::IsReverseAnswersEnabled() &&
                         !match_.IsExceptedFromLineReversal();
    if (reverse) {
      suggestion_view_->content()->SetText(match_.answer->second_line());
      suggestion_view_->description()->SetText(match_.contents,
                                               match_.contents_class, true);
      suggestion_view_->description()->ApplyTextColor(
          blue_search_query ? OmniboxPart::RESULTS_TEXT_URL
                            : OmniboxPart::RESULTS_TEXT_DIMMED);
      suggestion_view_->description()->AppendExtraText(
          match_.answer->first_line());
    } else {
      suggestion_view_->content()->SetText(match_.contents,
                                           match_.contents_class);
      suggestion_view_->content()->ApplyTextColor(
          blue_search_query ? OmniboxPart::RESULTS_TEXT_URL
                            : OmniboxPart::RESULTS_TEXT_DEFAULT);
      suggestion_view_->content()->AppendExtraText(match_.answer->first_line());
      suggestion_view_->description()->SetText(match_.answer->second_line(),
                                               true);
    }
  } else if (match_.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
             match_.type == AutocompleteMatchType::PEDAL) {
    // Entities use match text and calculated classifications, but with style
    // adjustments like answers above.  Pedals do likewise.
    suggestion_view_->content()->SetText(match_.contents,
                                         match_.contents_class);
    if (blue_search_query) {
      suggestion_view_->content()->ApplyTextColor(
          OmniboxPart::RESULTS_TEXT_URL);
    }
    suggestion_view_->description()->SetText(match_.description,
                                             match_.description_class, -1);
    suggestion_view_->description()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DIMMED);
  } else {
    // Content and description use match text and calculated classifications.
    suggestion_view_->content()->SetText(match_.contents,
                                         match_.contents_class);
    suggestion_view_->description()->SetText(match_.description,
                                             match_.description_class);

    // Normally, OmniboxTextView caches its appearance, but in high contrast,
    // selected-ness changes the text colors, so the styling of the text part of
    // the results needs to be recomputed.
    if (high_contrast) {
      suggestion_view_->content()->ReapplyStyling();
      suggestion_view_->description()->ReapplyStyling();
    }

    // If the blue search query experiment is on, search suggestions are
    // recolored at the end.
    if (blue_search_query && AutocompleteMatch::IsSearchType(match_.type)) {
      suggestion_view_->content()->ApplyTextColor(
          OmniboxPart::RESULTS_TEXT_URL);
    }
  }

  AutocompleteMatch* keyword_match = match_.associated_keyword.get();
  // Setting the keyword_view_ invisible is a minor optimization (it avoids
  // some OnPaint calls); it is not required.
  keyword_view_->SetVisible(keyword_match);
  if (keyword_match) {
    keyword_view_->content()->SetText(keyword_match->contents,
                                      keyword_match->contents_class);
    keyword_view_->description()->SetText(keyword_match->description,
                                          keyword_match->description_class);
    keyword_view_->description()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DIMMED);
  }
}

void OmniboxResultView::OnSelected() {
  DCHECK(IsSelected());

  // The text is also accessible via text/value change events in the omnibox but
  // this selection event allows the screen reader to get more details about the
  // list and the user's position within it.
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

bool OmniboxResultView::IsSelected() const {
  return popup_contents_view_->IsSelectedIndex(model_index_);
}

OmniboxPartState OmniboxResultView::GetThemeState() const {
  if (IsSelected()) {
    return is_hovered_ ? OmniboxPartState::HOVERED_AND_SELECTED
                       : OmniboxPartState::SELECTED;
  }
  return is_hovered_ ? OmniboxPartState::HOVERED : OmniboxPartState::NORMAL;
}

OmniboxTint OmniboxResultView::GetTint() const {
  return popup_contents_view_->GetTint();
}

void OmniboxResultView::OnMatchIconUpdated() {
  // The new icon will be fetched during Invalidate().
  Invalidate();
  SchedulePaint();
}

void OmniboxResultView::SetRichSuggestionImage(const gfx::ImageSkia& image) {
  suggestion_view_->SetImage(image);
  Layout();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

// |button| is the tab switch button.
void OmniboxResultView::ButtonPressed(views::Button* button,
                                      const ui::Event& event) {
  if (!(OmniboxFieldTrial::IsTabSwitchLogicReversed() &&
        match_.ShouldShowTabMatch())) {
    OpenMatch(WindowOpenDisposition::SWITCH_TO_TAB, event.time_stamp());
  } else {
    OpenMatch(WindowOpenDisposition::CURRENT_TAB, event.time_stamp());
  }
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides:

void OmniboxResultView::Layout() {
  views::View::Layout();
  // NOTE: While animating the keyword match, both matches may be visible.
  int suggestion_width = width();
  AutocompleteMatch* keyword_match = match_.associated_keyword.get();
  if (keyword_match) {
    const int max_kw_x =
        suggestion_width - OmniboxMatchCellView::GetTextIndent();
    suggestion_width = animation_->CurrentValueBetween(max_kw_x, 0);
  }
  if (suggestion_tab_switch_button_) {
    suggestion_tab_switch_button_->ProvideWidthHint(suggestion_width);
    const gfx::Size ts_button_size =
        suggestion_tab_switch_button_->GetPreferredSize();
    if (ts_button_size.width() > 0) {
      suggestion_tab_switch_button_->SetSize(ts_button_size);

      // Give the tab switch button a right margin matching the text.
      suggestion_width -=
          ts_button_size.width() + OmniboxMatchCellView::kMarginRight;

      // Center the button vertically.
      const int vertical_margin =
          (suggestion_view_->height() - ts_button_size.height()) / 2;
      suggestion_tab_switch_button_->SetPosition(
          gfx::Point(suggestion_width, vertical_margin));
      suggestion_tab_switch_button_->SetVisible(true);
    } else {
      suggestion_tab_switch_button_->SetVisible(false);
    }
  }
  keyword_view_->SetBounds(suggestion_width, 0, width(), height());
  suggestion_view_->SetBounds(0, 0, suggestion_width, height());
}

bool OmniboxResultView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    popup_contents_view_->SetSelectedLine(model_index_);
  return true;
}

bool OmniboxResultView::OnMouseDragged(const ui::MouseEvent& event) {
  if (HitTestPoint(event.location())) {
    // When the drag enters or remains within the bounds of this view, either
    // set the state to be selected or hovered, depending on the mouse button.
    if (event.IsOnlyLeftMouseButton()) {
      if (!IsSelected())
        popup_contents_view_->SetSelectedLine(model_index_);
      if (suggestion_tab_switch_button_) {
        gfx::Point point_in_child_coords(event.location());
        View::ConvertPointToTarget(this, suggestion_tab_switch_button_.get(),
                                   &point_in_child_coords);
        if (suggestion_tab_switch_button_->HitTestPoint(
                point_in_child_coords)) {
          SetMouseHandler(suggestion_tab_switch_button_.get());
          return false;
        }
      }
    } else {
      SetHovered(true);
    }
    return true;
  }

  // When the drag leaves the bounds of this view, cancel the hover state and
  // pass control to the popup view.
  SetHovered(false);
  SetMouseHandler(popup_contents_view_);
  return false;
}

void OmniboxResultView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() || event.IsOnlyLeftMouseButton()) {
    WindowOpenDisposition disposition =
        event.IsOnlyLeftMouseButton()
            ? WindowOpenDisposition::CURRENT_TAB
            : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    if (OmniboxFieldTrial::IsTabSwitchLogicReversed() &&
        match_.ShouldShowTabMatch()) {
      disposition = WindowOpenDisposition::SWITCH_TO_TAB;
    }
    OpenMatch(disposition, event.time_stamp());
  }
}

void OmniboxResultView::OnMouseMoved(const ui::MouseEvent& event) {
  SetHovered(true);
}

void OmniboxResultView::OnMouseExited(const ui::MouseEvent& event) {
  SetHovered(false);
}

void OmniboxResultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Get the label without the ", n of m" positional text appended.
  // The positional info is provided via
  // ax::mojom::IntAttribute::kPosInSet/SET_SIZE and providing it via text as
  // well would result in duplicate announcements.
  // Pass false for |is_tab_switch_button_focused|, because the button will
  // receive its own label in the case that a screen reader is listening to
  // selection events on items rather than announcements or value change events.
  node_data->SetName(AutocompleteMatchType::ToAccessibilityLabel(
      match_, match_.contents, false));

  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             model_index_ + 1);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             int{popup_contents_view_->children().size()});

  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              IsSelected());
  if (is_hovered_)
    node_data->AddState(ax::mojom::State::kHovered);
}

gfx::Size OmniboxResultView::CalculatePreferredSize() const {
  // The keyword_view_ is not added because keyword_view_ uses the same space as
  // suggestion_view_. So the 'preferred' size is just the suggestion_view_
  // size.
  return suggestion_view_->CalculatePreferredSize();
}

void OmniboxResultView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  Invalidate();
  SchedulePaint();
}

void OmniboxResultView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // Deferred unhover of the result until the context menu is closed.
  // If the mouse is still over the result when the context menu is closed, the
  // View will receive an OnMouseMoved call anyways, which sets hover to true.
  base::RepeatingClosure set_hovered_false = base::BindRepeating(
      &OmniboxResultView::SetHovered, weak_factory_.GetWeakPtr(), false);

  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      &context_menu_contents_,
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
      set_hovered_false);
  context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, source_type);

  // Opening the context menu unsets the hover state, but we still want the
  // result 'hovered' as long as the context menu is open.
  SetHovered(true);
}

// ui::SimpleMenuModel::Delegate overrides:
bool OmniboxResultView::IsCommandIdEnabled(int command_id) const {
  DCHECK_EQ(COMMAND_REMOVE_SUGGESTION, command_id);
  return match_.SupportsDeletion();
}

void OmniboxResultView::ExecuteCommand(int command_id, int event_flags) {
  DCHECK_EQ(COMMAND_REMOVE_SUGGESTION, command_id);

  // Temporarily inhibit the popup closing on blur while we open the remove
  // suggestion confirmation bubble.
  popup_contents_view_->model()->set_popup_closes_on_blur(false);

  // TODO(tommycli): We re-fetch the original match from the popup model,
  // because |match_| already has its contents and description swapped by this
  // class, and we don't want that for the bubble. We should improve this.
  AutocompleteMatch raw_match =
      popup_contents_view_->model()->result().match_at(model_index_);
  ShowRemoveSuggestion(this, raw_match,
                       base::BindOnce(&OmniboxResultView::RemoveSuggestion,
                                      weak_factory_.GetWeakPtr()));

  popup_contents_view_->model()->set_popup_closes_on_blur(true);
}

void OmniboxResultView::ProvideButtonFocusHint() {
  suggestion_tab_switch_button_->ProvideFocusHint();
}

void OmniboxResultView::RemoveSuggestion() const {
  popup_contents_view_->model()->TryDeletingLine(model_index_);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, private:

gfx::Image OmniboxResultView::GetIcon() const {
  SkColor color = GetColor(OmniboxPart::RESULTS_ICON);

  // If the blue search loop experiment is enabled, set the color of search
  // match icons to be the same as result URLs (conventionally blue).
  if (base::FeatureList::IsEnabled(
          omnibox::kUIExperimentBlueSearchLoopAndSearchQuery) &&
      AutocompleteMatch::IsSearchType(match_.type)) {
    color = GetColor(OmniboxPart::RESULTS_TEXT_URL);
  }

  return popup_contents_view_->GetMatchIcon(match_, color);
}

void OmniboxResultView::SetHovered(bool hovered) {
  if (is_hovered_ != hovered) {
    is_hovered_ = hovered;
    Invalidate();
    SchedulePaint();
  }
}

void OmniboxResultView::OpenMatch(WindowOpenDisposition disposition,
                                  base::TimeTicks match_selection_timestamp) {
  popup_contents_view_->OpenMatch(model_index_, disposition,
                                  match_selection_timestamp);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides, private:

const char* OmniboxResultView::GetClassName() const {
  return "OmniboxResultView";
}

void OmniboxResultView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  animation_->SetSlideDuration(width() / 4);
  Layout();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, gfx::AnimationProgressed overrides, private:

void OmniboxResultView::AnimationProgressed(const gfx::Animation* animation) {
  Layout();
  SchedulePaint();
}

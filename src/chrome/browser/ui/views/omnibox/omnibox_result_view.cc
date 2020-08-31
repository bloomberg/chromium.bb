// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include <limits.h>

#include <algorithm>  // NOLINT

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/browser/ui/views/omnibox/remove_suggestion_bubble.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#if defined(OS_WIN)
#include "base/win/atl.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, public:

OmniboxResultView::OmniboxResultView(
    OmniboxPopupContentsView* popup_contents_view,
    size_t model_index)
    : AnimationDelegateViews(this),
      popup_contents_view_(popup_contents_view),
      model_index_(model_index),
      animation_(new gfx::SlideAnimation(this)) {
  CHECK_GE(model_index, 0u);

  suggestion_view_ = AddChildView(std::make_unique<OmniboxMatchCellView>(this));

  suggestion_tab_switch_button_ =
      AddChildView(std::make_unique<OmniboxTabSwitchButton>(
          popup_contents_view_, this,
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_HINT),
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_SHORT_HINT),
          omnibox::kSwitchIcon));

  // This is intentionally not in the tab order by default, but should be if the
  // user has full-acessibility mode on. This is because this is a tertiary
  // priority button, which already has a Shift+Delete shortcut.
  // TODO(tommycli): Make sure we announce the Shift+Delete capability in the
  // accessibility node data for removable suggestions.
  remove_suggestion_button_ =
      AddChildView(views::CreateVectorImageButton(this));
  views::InstallCircleHighlightPathGenerator(remove_suggestion_button_);
  remove_suggestion_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_REMOVE_SUGGESTION));
  remove_suggestion_focus_ring_ =
      views::FocusRing::Install(remove_suggestion_button_);
  remove_suggestion_focus_ring_->SetHasFocusPredicate([&](View* view) {
    return view->GetVisible() && IsMatchSelected() &&
           popup_contents_view_->IsButtonSelected();
  });

  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    button_row_ = AddChildView(std::make_unique<OmniboxSuggestionButtonRowView>(
        popup_contents_view_, model_index));
    // The button row shows itself during Layout when appropriate.
    // TODO(orinj): Visibility should also be revisited when layout is reworked.
    button_row_->SetVisible(false);
  }

  keyword_view_ = AddChildView(std::make_unique<OmniboxMatchCellView>(this));
  keyword_view_->SetVisible(false);
  keyword_view_->icon()->EnableCanvasFlippingForRTLUI(true);
  keyword_view_->icon()->SizeToPreferredSize();
}

OmniboxResultView::~OmniboxResultView() {}

// static
std::unique_ptr<views::Background> OmniboxResultView::GetPopupCellBackground(
    views::View* view,
    OmniboxPartState part_state) {
  DCHECK(view);

  bool high_contrast = view->GetNativeTheme() &&
                       view->GetNativeTheme()->UsesHighContrastColors();
  // TODO(tapted): Consider using background()->SetNativeControlColor() and
  // always have a background.
  if ((part_state == OmniboxPartState::NORMAL && !high_contrast))
    return nullptr;

  return views::CreateSolidBackground(GetOmniboxColor(
      view->GetThemeProvider(), OmniboxPart::RESULTS_BACKGROUND, part_state));
}

SkColor OmniboxResultView::GetColor(OmniboxPart part) const {
  return GetOmniboxColor(GetThemeProvider(), part, GetThemeState());
}

void OmniboxResultView::SetMatch(const AutocompleteMatch& match) {
  match_ = match.GetMatchWithContentsAndDescriptionPossiblySwapped();
  animation_->Reset();

  suggestion_view_->OnMatchUpdate(this, match_);
  keyword_view_->OnMatchUpdate(this, match_);
  suggestion_tab_switch_button_->SetVisible(match.ShouldShowTabMatchButton());
  UpdateRemoveSuggestionVisibility();

  suggestion_view_->content()->SetText(match_.contents, match_.contents_class);
  if (match_.answer) {
    suggestion_view_->content()->AppendExtraText(match_.answer->first_line());
    suggestion_view_->description()->SetText(match_.answer->second_line(),
                                             true);
  } else {
    const bool deemphasize =
        match_.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
        match_.type == AutocompleteMatchType::PEDAL;
    suggestion_view_->description()->SetText(
        match_.description, match_.description_class, deemphasize);
  }

  // With button row, its keyword button is used instead of |keyword_view_|.
  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    button_row_->UpdateKeyword();
  } else {
    AutocompleteMatch* keyword_match = match_.associated_keyword.get();
    keyword_view_->SetVisible(keyword_match != nullptr);
    if (keyword_match) {
      keyword_view_->content()->SetText(keyword_match->contents,
                                        keyword_match->contents_class);
      keyword_view_->description()->SetText(keyword_match->description,
                                            keyword_match->description_class);
    }
  }

  ApplyThemeAndRefreshIcons();
  InvalidateLayout();
}

void OmniboxResultView::ShowKeyword(bool show_keyword) {
  if (show_keyword)
    animation_->Show();
  else
    animation_->Hide();
}

void OmniboxResultView::ApplyThemeAndRefreshIcons(bool force_reapply_styles) {
  SetBackground(GetPopupCellBackground(this, GetThemeState()));

  // Reapply the dim color to account for the highlight state.
  suggestion_view_->separator()->ApplyTextColor(
      OmniboxPart::RESULTS_TEXT_DIMMED);
  keyword_view_->separator()->ApplyTextColor(OmniboxPart::RESULTS_TEXT_DIMMED);
  if (suggestion_tab_switch_button_->GetVisible())
    suggestion_tab_switch_button_->UpdateBackground();
  if (remove_suggestion_button_->GetVisible())
    remove_suggestion_focus_ring_->SchedulePaint();

  // Recreate the icons in case the color needs to change.
  // Note: if this is an extension icon or favicon then this can be done in
  //       SetMatch() once (rather than repeatedly, as happens here). There may
  //       be an optimization opportunity here.
  // TODO(dschuyler): determine whether to optimize the color changes.
  suggestion_view_->icon()->SetImage(GetIcon().ToImageSkia());
  keyword_view_->icon()->SetImage(gfx::CreateVectorIcon(
      omnibox::kKeywordSearchIcon, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetColor(OmniboxPart::RESULTS_ICON)));

  bool high_contrast =
      GetNativeTheme() && GetNativeTheme()->UsesHighContrastColors();
  if (match_.answer) {
    suggestion_view_->content()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DEFAULT);
  } else if (match_.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
             match_.type == AutocompleteMatchType::PEDAL) {
    suggestion_view_->description()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DIMMED);
  } else if (high_contrast || force_reapply_styles) {
    // Normally, OmniboxTextView caches its appearance, but in high contrast,
    // selected-ness changes the text colors, so the styling of the text part of
    // the results needs to be recomputed.
    suggestion_view_->content()->ReapplyStyling();
    suggestion_view_->description()->ReapplyStyling();
  }

  if (keyword_view_->GetVisible()) {
    keyword_view_->description()->ApplyTextColor(
        OmniboxPart::RESULTS_TEXT_DIMMED);
  }

  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    button_row_->OnStyleRefresh();
  }
}

void OmniboxResultView::OnSelectionStateChanged() {
  UpdateRemoveSuggestionVisibility();
  if (IsMatchSelected()) {
    // Immediately before notifying screen readers that the selected item has
    // changed, we want to update the name of the newly-selected item so that
    // any cached values get updated prior to the selection change.
    EmitTextChangedAccessiblityEvent();

    // Send accessibility event on the popup box that its selection has changed.
    EmitSelectedChildrenChangedAccessibilityEvent();

    // The text is also accessible via text/value change events in the omnibox
    // but this selection event allows the screen reader to get more details
    // about the list and the user's position within it.
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);

    // TODO(orinj): Eventually the deep digging in this class should get
    //  replaced with a single local point of access to all selection state.
    ShowKeyword(popup_contents_view_->model()->selection().state ==
                OmniboxPopupModel::KEYWORD);
  } else {
    ShowKeyword(false);
  }
  ApplyThemeAndRefreshIcons();
}

bool OmniboxResultView::IsMatchSelected() const {
  // The header button being focused means the match itself is NOT focused.
  return popup_contents_view_->IsSelectedIndex(model_index_) &&
         popup_contents_view_->model()->selected_line_state() !=
             OmniboxPopupModel::HEADER_BUTTON_FOCUSED;
}

views::Button* OmniboxResultView::GetSecondaryButton() {
  if (suggestion_tab_switch_button_->GetVisible())
    return suggestion_tab_switch_button_;

  if (remove_suggestion_button_->GetVisible())
    return remove_suggestion_button_;

  return nullptr;
}

bool OmniboxResultView::MaybeTriggerSecondaryButton(const ui::Event& event) {
  views::Button* button = GetSecondaryButton();
  if (!button)
    return false;

  ButtonPressed(button, event);
  return true;
}

base::string16 OmniboxResultView::ToAccessibilityLabelWithSecondaryButton(
    const base::string16& match_text,
    size_t total_matches,
    int* label_prefix_length) {
  int additional_message_id = 0;
  views::Button* secondary_button = GetSecondaryButton();
  bool button_focused = IsMatchSelected() &&
                        popup_contents_view_->model()->selected_line_state() ==
                            OmniboxPopupModel::BUTTON_FOCUSED;

  // If there's a button focused, we don't want the "n of m" message announced.
  if (button_focused)
    total_matches = 0;

  // Add additional messages
  if (secondary_button == suggestion_tab_switch_button_) {
    additional_message_id = button_focused
                                ? IDS_ACC_TAB_SWITCH_BUTTON_FOCUSED_PREFIX
                                : IDS_ACC_TAB_SWITCH_SUFFIX;
  } else if (secondary_button == remove_suggestion_button_) {
    // Don't add an additional message for removable suggestions without button
    // focus, since they are relatively common.
    additional_message_id =
        button_focused ? IDS_ACC_REMOVE_SUGGESTION_FOCUSED_PREFIX : 0;
  }

  // TODO(tommycli): We re-fetch the original match from the popup model,
  // because |match_| already has its contents and description swapped by this
  // class, and we don't want that for the bubble. We should improve this.
  AutocompleteMatch raw_match =
      popup_contents_view_->model()->result().match_at(model_index_);
  return AutocompleteMatchType::ToAccessibilityLabel(
      raw_match, match_text, model_index_, total_matches, additional_message_id,
      label_prefix_length);
}

OmniboxPartState OmniboxResultView::GetThemeState() const {
  if (IsMatchSelected())
    return OmniboxPartState::SELECTED;

  // If we don't highlight the whole row when the user has the mouse over the
  // remove suggestion button, it's unclear which suggestion is being removed.
  // That does not apply to the tab switch button, which is much larger.
  bool highlight_row =
      IsMouseHovered() && !suggestion_tab_switch_button_->IsMouseHovered();
  return highlight_row ? OmniboxPartState::HOVERED : OmniboxPartState::NORMAL;
}

void OmniboxResultView::OnMatchIconUpdated() {
  // The new icon will be fetched during ApplyThemeAndRefreshIcons().
  ApplyThemeAndRefreshIcons();
}

void OmniboxResultView::SetRichSuggestionImage(const gfx::ImageSkia& image) {
  suggestion_view_->SetImage(image);
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

void OmniboxResultView::ButtonPressed(views::Button* button,
                                      const ui::Event& event) {
  if (button == suggestion_tab_switch_button_) {
    OpenMatch(WindowOpenDisposition::SWITCH_TO_TAB, event.time_stamp());
  } else if (button == remove_suggestion_button_) {
    if (!base::FeatureList::IsEnabled(
            omnibox::kConfirmOmniboxSuggestionRemovals)) {
      RemoveSuggestion();
      return;
    }

    // Temporarily inhibit the popup closing on blur while we open the remove
    // suggestion confirmation bubble.
    popup_contents_view_->model()->set_popup_closes_on_blur(false);

    // TODO(tommycli): We re-fetch the original match from the popup model,
    // because |match_| already has its contents and description swapped by this
    // class, and we don't want that for the bubble. We should improve this.
    AutocompleteMatch raw_match =
        popup_contents_view_->model()->result().match_at(model_index_);

    TemplateURLService* template_url_service = popup_contents_view_->model()
                                                   ->edit_model()
                                                   ->client()
                                                   ->GetTemplateURLService();
    ShowRemoveSuggestion(template_url_service, this, raw_match,
                         base::BindOnce(&OmniboxResultView::RemoveSuggestion,
                                        weak_factory_.GetWeakPtr()));

    popup_contents_view_->model()->set_popup_closes_on_blur(true);
  } else {
    NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides:

void OmniboxResultView::Layout() {
  views::View::Layout();
  // NOTE: While animating the keyword match, both matches may be visible.
  int suggestion_width = width();
  if (keyword_view_->GetVisible()) {
    const int max_kw_x =
        suggestion_width - OmniboxMatchCellView::GetTextIndent();
    suggestion_width = animation_->CurrentValueBetween(max_kw_x, 0);
    keyword_view_->SetBounds(suggestion_width, 0, width() - suggestion_width,
                             height());
  }
  // Add buttons from right to left, shrinking the suggestion width as we go.
  // To avoid clutter, don't show either button for matches with keyword.
  // TODO(tommycli): We should probably use a layout manager here.
  if (remove_suggestion_button_->GetVisible()) {
    const gfx::Size button_size = remove_suggestion_button_->GetPreferredSize();
    suggestion_width -=
        button_size.width() + OmniboxMatchCellView::kMarginRight;

    // Center the button vertically.
    const int vertical_margin =
        (suggestion_view_->height() - button_size.height()) / 2;
    remove_suggestion_button_->SetBounds(suggestion_width, vertical_margin,
                                         button_size.width(),
                                         button_size.height());
  }

  if (match_.ShouldShowTabMatchButton()) {
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

  const int suggestion_indent =
      (popup_contents_view_->InExplicitExperimentalKeywordMode() ||
       match_.IsSubMatch())
          ? 70
          : 0;
  const int suggestion_height = suggestion_view_->GetPreferredSize().height();
  suggestion_view_->SetBounds(suggestion_indent, 0,
                              suggestion_width - suggestion_indent,
                              suggestion_height);

  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled()) {
    // TODO(orinj): Determine and use the best way to set bounds; probably
    // GetPreferredSize() with a layout manager.
    // Put it below the suggestion view.
    button_row_->SetBounds(0, button_row_->height(),
                           suggestion_width - suggestion_indent,
                           suggestion_height);
    button_row_->Layout();
  }
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
      if (!IsMatchSelected())
        popup_contents_view_->SetSelectedLine(model_index_);
      if (suggestion_tab_switch_button_) {
        gfx::Point point_in_child_coords(event.location());
        View::ConvertPointToTarget(this, suggestion_tab_switch_button_,
                                   &point_in_child_coords);
        if (suggestion_tab_switch_button_->HitTestPoint(
                point_in_child_coords)) {
          SetMouseHandler(suggestion_tab_switch_button_);
          return false;
        }
      }
    } else {
      UpdateHoverState();
    }
    return true;
  }

  // When the drag leaves the bounds of this view, cancel the hover state and
  // pass control to the popup view.
  UpdateHoverState();
  SetMouseHandler(popup_contents_view_);
  return false;
}

void OmniboxResultView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() || event.IsOnlyLeftMouseButton()) {
    WindowOpenDisposition disposition =
        event.IsOnlyLeftMouseButton()
            ? WindowOpenDisposition::CURRENT_TAB
            : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    if (match_.IsTabSwitchSuggestion()) {
      disposition = WindowOpenDisposition::SWITCH_TO_TAB;
    }
    OpenMatch(disposition, event.time_stamp());
  }
}

void OmniboxResultView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHoverState();
}

void OmniboxResultView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHoverState();
}

void OmniboxResultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Get the label without the ", n of m" positional text appended.
  // The positional info is provided via
  // ax::mojom::IntAttribute::kPosInSet/SET_SIZE and providing it via text as
  // well would result in duplicate announcements.
  // Pass false for |is_tab_switch_button_focused|, because the button will
  // receive its own label in the case that a screen reader is listening to
  // selection events on items rather than announcements or value change events.
  node_data->SetName(
      AutocompleteMatchType::ToAccessibilityLabel(match_, match_.contents));

  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             model_index_ + 1);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             popup_contents_view_->model()->result().size());

  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              IsMatchSelected());
  if (IsMouseHovered())
    node_data->AddState(ax::mojom::State::kHovered);
}

gfx::Size OmniboxResultView::CalculatePreferredSize() const {
  gfx::Size size = suggestion_view_->GetPreferredSize();
  if (keyword_view_->GetVisible())
    size.SetToMax(keyword_view_->GetPreferredSize());
  if (OmniboxFieldTrial::IsSuggestionButtonRowEnabled() &&
      button_row_->GetVisible()) {
    // Double our height for buttons.
    size.set_height(size.height() * 2);
  }
  return size;
}

void OmniboxResultView::OnThemeChanged() {
  views::View::OnThemeChanged();
  views::SetImageFromVectorIcon(remove_suggestion_button_,
                                vector_icons::kCloseRoundedIcon,
                                GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
                                GetColor(OmniboxPart::RESULTS_ICON));
  ApplyThemeAndRefreshIcons(true);
}

void OmniboxResultView::ProvideButtonFocusHint() {
  if (suggestion_tab_switch_button_->GetVisible()) {
    suggestion_tab_switch_button_->ProvideFocusHint();
  } else if (remove_suggestion_button_->GetVisible()) {
    remove_suggestion_button_->NotifyAccessibilityEvent(
        ax::mojom::Event::kSelection, true);
  }
}

void OmniboxResultView::RemoveSuggestion() const {
  popup_contents_view_->model()->TryDeletingLine(model_index_);
}

void OmniboxResultView::EmitTextChangedAccessiblityEvent() {
  if (!popup_contents_view_->IsOpen())
    return;

  // The omnibox results list reuses the same items, but the text displayed for
  // these items is updated as the value of omnibox changes. The displayed text
  // for a given item is exposed to screen readers as the item's name/label.
  base::string16 current_name =
      AutocompleteMatchType::ToAccessibilityLabel(match_, match_.contents);
  if (accessible_name_ != current_name) {
    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
    accessible_name_ = current_name;
  }
}

void OmniboxResultView::EmitSelectedChildrenChangedAccessibilityEvent() {
  popup_contents_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kSelectedChildrenChanged, true);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, private:

gfx::Image OmniboxResultView::GetIcon() const {
  return popup_contents_view_->GetMatchIcon(
      match_, GetColor(OmniboxPart::RESULTS_ICON));
}

void OmniboxResultView::UpdateHoverState() {
  UpdateRemoveSuggestionVisibility();
  ApplyThemeAndRefreshIcons();
}

void OmniboxResultView::OpenMatch(WindowOpenDisposition disposition,
                                  base::TimeTicks match_selection_timestamp) {
  popup_contents_view_->OpenMatch(model_index_, disposition,
                                  match_selection_timestamp);
}

void OmniboxResultView::UpdateRemoveSuggestionVisibility() {
  bool old_visibility = remove_suggestion_button_->GetVisible();
  bool new_visibility = match_.SupportsDeletion() &&
                        !match_.associated_keyword &&
                        !match_.ShouldShowTabMatchButton() &&
                        base::FeatureList::IsEnabled(
                            omnibox::kOmniboxSuggestionTransparencyOptions) &&
                        (IsMatchSelected() || IsMouseHovered());

  remove_suggestion_button_->SetVisible(new_visibility);

  if (old_visibility != new_visibility)
    InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides, private:

const char* OmniboxResultView::GetClassName() const {
  return "OmniboxResultView";
}

void OmniboxResultView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(width() / 4));
  InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::AnimationDelegateViews overrides, private:

void OmniboxResultView::AnimationProgressed(const gfx::Animation* animation) {
  InvalidateLayout();
}

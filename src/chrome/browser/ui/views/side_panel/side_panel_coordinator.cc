// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include <memory>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_later_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_combobox_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_container_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_notes/user_notes_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kSidePanelContentViewId = 42;
constexpr int kSidePanelContentWrapperViewId = 43;

constexpr SidePanelEntry::Id kDefaultEntry = SidePanelEntry::Id::kReadingList;

std::unique_ptr<views::ImageButton> CreateControlButton(
    views::View* host,
    base::RepeatingClosure pressed_callback,
    const gfx::VectorIcon& icon,
    const gfx::Insets& margin_insets,
    const std::u16string& tooltip_text,
    int dip_size) {
  auto button = views::CreateVectorImageButtonWithNativeTheme(pressed_callback,
                                                              icon, dip_size);
  button->SetTooltipText(tooltip_text);
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetProperty(views::kMarginsKey, margin_insets);
  views::InstallCircleHighlightPathGenerator(button.get());

  return button;
}

class SidePanelSeparator : public views::Separator {
 public:
  METADATA_HEADER(SidePanelSeparator);

  void OnThemeChanged() override {
    views::Separator::OnThemeChanged();
    SetColor(GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_SIDE_PANEL_CONTENT_AREA_SEPARATOR));
  }
};

BEGIN_METADATA(SidePanelSeparator, views::Separator)
END_METADATA

}  // namespace

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view,
                                           SidePanelRegistry* global_registry)
    : browser_view_(browser_view), global_registry_(global_registry) {
  combobox_model_ = std::make_unique<SidePanelComboboxModel>();
  global_registry->AddObserver(this);
  browser_view_->browser()->tab_strip_model()->AddObserver(this);
  // TODO(pbos): Consider moving creation of SidePanelEntry into other functions
  // that can easily be unit tested.
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadingList,
      l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE),
      ui::ImageModel::FromVectorIcon(kReadLaterIcon, ui::kColorIcon),
      base::BindRepeating(
          [](SidePanelCoordinator* coordinator,
             Browser* browser) -> std::unique_ptr<views::View> {
            return std::make_unique<ReadLaterSidePanelWebView>(
                browser, base::BindRepeating(&SidePanelCoordinator::Close,
                                             base::Unretained(coordinator)));
          },
          this, browser_view->browser())));
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kBookmarks,
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE),
      ui::ImageModel::FromVectorIcon(omnibox::kStarIcon, ui::kColorIcon),
      base::BindRepeating(&SidePanelCoordinator::CreateBookmarksWebView,
                          base::Unretained(this), browser_view->browser())));
  if (features::IsReadAnythingEnabled()) {
    global_registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kReadAnything,
        l10n_util::GetStringUTF16(IDS_READ_ANYTHING_TITLE),
        ui::ImageModel::FromVectorIcon(kReaderModeIcon, ui::kColorIcon),
        base::BindRepeating(&SidePanelCoordinator::CreateReadAnythingWebView,
                            base::Unretained(this), browser_view->browser())));
  }

  if (user_notes::IsUserNotesEnabled()) {
    global_registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kUserNote,
        l10n_util::GetStringUTF16(IDS_USER_NOTE_TITLE),
        ui::ImageModel::FromVectorIcon(kInkHighlighterIcon, ui::kColorIcon),
        base::BindRepeating(&SidePanelCoordinator::CreateUserNoteView,
                            base::Unretained(this), browser_view->browser())));
  }
}

SidePanelCoordinator::~SidePanelCoordinator() {
  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);
}

void SidePanelCoordinator::Show(absl::optional<SidePanelEntry::Id> entry_id) {
  if (!entry_id.has_value())
    entry_id = GetLastActiveEntryId().value_or(kDefaultEntry);

  SidePanelEntry* entry = GetEntryForId(entry_id.value());
  if (!entry || (GetContentView() && GetLastActiveEntryId().has_value() &&
                 GetLastActiveEntryId().value() == entry->id())) {
    return;
  }

  if (GetContentView() == nullptr) {
    InitializeSidePanel();
    base::RecordAction(base::UserMetricsAction("SidePanel.Show"));
    // Record usage for side panel promo.
    feature_engagement::TrackerFactory::GetForBrowserContext(
        browser_view_->GetProfile())
        ->NotifyEvent("side_panel_shown");

    // Close IPH for side panel if shown.
    browser_view_->browser()->window()->CloseFeaturePromo(
        feature_engagement::kIPHReadingListInSidePanelFeature);
  }

  PopulateSidePanel(entry);
}

void SidePanelCoordinator::Close() {
  views::View* const content_view = GetContentView();
  if (!content_view)
    return;

  if (global_registry_->active_entry().has_value()) {
    last_active_global_entry_id_ =
        global_registry_->active_entry().value()->id();
  }
  // Reset active entry values for all registries since existence of an
  // active_entry for a tab in any registry will trigger the side panel to be
  // shown.
  global_registry_->ResetActiveEntry();
  if (auto* contextual_registry = GetActiveContextualRegistry())
    contextual_registry->ResetActiveEntry();
  ClearCachedEntryViews();

  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));

  browser_view_->right_aligned_side_panel()->RemoveChildViewT(content_view);
  base::RecordAction(base::UserMetricsAction("SidePanel.Hide"));
}

void SidePanelCoordinator::Toggle() {
  if (GetContentView() != nullptr) {
    Close();
  } else {
    Show();
  }
}

views::View* SidePanelCoordinator::GetContentView() {
  return browser_view_->right_aligned_side_panel()->GetViewByID(
      kSidePanelContentViewId);
}

SidePanelEntry* SidePanelCoordinator::GetEntryForId(
    SidePanelEntry::Id entry_id) {
  if (auto* entry = global_registry_->GetEntryForId(entry_id))
    return entry;
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    if (auto* entry = contextual_registry->GetEntryForId(entry_id))
      return entry;
  }
  return nullptr;
}

void SidePanelCoordinator::InitializeSidePanel() {
  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_HIDE));

  auto container = std::make_unique<views::FlexLayoutView>();
  // Align views vertically top to bottom.
  container->SetOrientation(views::LayoutOrientation::kVertical);
  container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  // Stretch views to fill horizontal bounds.
  container->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  container->SetID(kSidePanelContentViewId);

  container->AddChildView(CreateHeader());
  container->AddChildView(std::make_unique<SidePanelSeparator>());

  auto content_wrapper = std::make_unique<views::View>();
  content_wrapper->SetUseDefaultFillLayout(true);
  content_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  content_wrapper->SetID(kSidePanelContentWrapperViewId);
  container->AddChildView(std::move(content_wrapper));

  browser_view_->right_aligned_side_panel()->AddChildView(std::move(container));
}

void SidePanelCoordinator::PopulateSidePanel(SidePanelEntry* entry) {
  views::View* content_wrapper =
      GetContentView()->GetViewByID(kSidePanelContentWrapperViewId);
  DCHECK(content_wrapper);
  // |content_wrapper| should have either no child views or one child view for
  // the currently hosted SidePanelEntry.
  DCHECK(content_wrapper->children().size() <= 1);
  if (content_wrapper->children().size()) {
    DCHECK(GetLastActiveEntryId().has_value());
    SidePanelEntry* current_entry =
        GetEntryForId(GetLastActiveEntryId().value());
    DCHECK(current_entry);
    auto current_entry_view =
        content_wrapper->RemoveChildViewT(content_wrapper->children().front());
    current_entry->CacheView(std::move(current_entry_view));
  }
  content_wrapper->AddChildView(entry->GetContent());
  if (auto* contextual_registry = GetActiveContextualRegistry())
    contextual_registry->ResetActiveEntry();
  entry->OnEntryShown();
}

void SidePanelCoordinator::ClearCachedEntryViews() {
  global_registry_->ClearCachedEntryViews();
  TabStripModel* model = browser_view_->browser()->tab_strip_model();
  if (!model)
    return;
  for (int index = 0; index < model->count(); ++index) {
    auto* web_contents =
        browser_view_->browser()->tab_strip_model()->GetWebContentsAt(index);
    if (auto* registry = SidePanelRegistry::Get(web_contents))
      registry->ClearCachedEntryViews();
  }
}

absl::optional<SidePanelEntry::Id> SidePanelCoordinator::GetLastActiveEntryId()
    const {
  // If a contextual entry is active, return that. If not, return the last
  // active global entry. If neither exist, fall back to kReadingList.
  if (GetActiveContextualRegistry() &&
      GetActiveContextualRegistry()->active_entry().has_value()) {
    return GetActiveContextualRegistry()->active_entry().value()->id();
  }

  if (global_registry_->active_entry().has_value())
    return global_registry_->active_entry().value()->id();

  if (last_active_global_entry_id_.has_value())
    return last_active_global_entry_id_.value();

  return absl::nullopt;
}

SidePanelRegistry* SidePanelCoordinator::GetActiveContextualRegistry() const {
  if (auto* web_contents =
          browser_view_->browser()->tab_strip_model()->GetActiveWebContents()) {
    return SidePanelRegistry::Get(web_contents);
  }
  return nullptr;
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateHeader() {
  auto header = std::make_unique<views::FlexLayoutView>();
  // ChromeLayoutProvider for providing margins.
  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();

  // Set the interior margins of the header on the left and right sides.
  header->SetInteriorMargin(gfx::Insets::VH(
      0, chrome_layout_provider->GetDistanceMetric(
             views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  // Set alignments for horizontal (main) and vertical (cross) axes.
  header->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The minimum cross axis size should the expected height of the header.
  constexpr int kDefaultSidePanelHeaderHeight = 40;
  header->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);
  header->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorWindowBackground));

  header_combobox_ = header->AddChildView(CreateCombobox());

  // Create an empty view between branding and buttons to align branding on left
  // without hardcoding margins. This view fills up the empty space between the
  // branding and the control buttons.
  // TODO(pbos): This View seems like it should be avoidable by not having LHS
  // and RHS content stretch? This is copied from the Lens side panel, but could
  // probably by cleaned up?
  auto container = std::make_unique<views::View>();
  container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  header->AddChildView(std::move(container));

  header->AddChildView(CreateControlButton(
      header.get(),
      base::BindRepeating(&SidePanelCoordinator::Close, base::Unretained(this)),
      views::kIcCloseIcon, gfx::Insets(),
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));

  return header;
}

std::unique_ptr<views::Combobox> SidePanelCoordinator::CreateCombobox() {
  auto combobox = std::make_unique<views::Combobox>(combobox_model_.get());
  combobox->SetSelectedIndex(combobox_model_->GetIndexForId(
      GetLastActiveEntryId().value_or(kDefaultEntry)));
  // TODO(corising): Replace this with something appropriate.
  combobox->SetAccessibleName(
      combobox_model_->GetItemAt(combobox->GetSelectedIndex()));

  combobox->SetCallback(base::BindRepeating(
      &SidePanelCoordinator::OnComboboxChanged, base::Unretained(this)));
  return combobox;
}

void SidePanelCoordinator::OnComboboxChanged() {
  SidePanelEntry::Id entry_id =
      combobox_model_->GetIdAt(header_combobox_->GetSelectedIndex());
  Show(entry_id);
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateBookmarksWebView(
    Browser* browser) {
  auto bookmarks_web_view =
      std::make_unique<SidePanelWebUIViewT<BookmarksSidePanelUI>>(
          browser, base::RepeatingClosure(),
          base::BindRepeating(&SidePanelCoordinator::Close,
                              base::Unretained(this)),
          std::make_unique<BubbleContentsWrapperT<BookmarksSidePanelUI>>(
              GURL(chrome::kChromeUIBookmarksSidePanelURL), browser->profile(),
              IDS_BOOKMARK_MANAGER_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false));
  if (base::FeatureList::IsEnabled(features::kSidePanelDragAndDrop)) {
    extensions::BookmarkManagerPrivateDragEventRouter::CreateForWebContents(
        bookmarks_web_view.get()->contents_wrapper()->web_contents());
  }
  return bookmarks_web_view;
}

// The ownership of the container view moves to the caller of this function.
std::unique_ptr<views::View> SidePanelCoordinator::CreateReadAnythingWebView(
    Browser* browser) {
  read_anything_coordinator_ =
      std::make_unique<ReadAnythingCoordinator>(browser);
  return read_anything_coordinator_->GetContainerView();
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateUserNoteView(
    Browser* browser) {
  return browser_view_->user_note_ui_coordinator()->CreateUserNotesView();
}

void SidePanelCoordinator::OnEntryRegistered(SidePanelEntry* entry) {
  combobox_model_->AddItem(entry);
}

void SidePanelCoordinator::OnEntryWillDeregister(SidePanelEntry* entry) {
  combobox_model_->RemoveItem(entry->id());
  // Update the current entry to make sure we don't show an entry that is being
  // removed or close the panel if the entry being deregistered is the only one
  // that has been visible.
  if (GetContentView()) {
    if (global_registry_->active_entry().has_value()) {
      Show(GetLastActiveEntryId().value_or(kDefaultEntry));
    } else {
      Close();
    }
  }
}

void SidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kSelectionOnly ||
      !selection.active_tab_changed()) {
    return;
  }
  // Handle removing the previous tab's contextual registry if one exists and
  // update the combobox.
  if (auto* old_contextual_registry =
          SidePanelRegistry::Get(selection.old_contents)) {
    old_contextual_registry->RemoveObserver(this);
    combobox_model_->RemoveItems(old_contextual_registry->entries());
  }

  // Add the current tab's contextual registry and update the combobox.
  auto* new_contextual_registry =
      SidePanelRegistry::Get(selection.new_contents);
  if (new_contextual_registry) {
    new_contextual_registry->AddObserver(this);
    combobox_model_->AddItems(new_contextual_registry->entries());
  }

  // If an active entry is available, show it. If not, close the panel.
  if (GetContentView()) {
    if ((!new_contextual_registry ||
         !new_contextual_registry->active_entry().has_value()) &&
        !global_registry_->active_entry().has_value()) {
      Close();
    } else {
      Show(GetLastActiveEntryId().value_or(kDefaultEntry));
    }
  } else if (new_contextual_registry &&
             new_contextual_registry->active_entry().has_value()) {
    Show(new_contextual_registry->active_entry().value()->id());
  }
}

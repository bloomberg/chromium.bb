// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/site_settings_expand_button.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

using UserSiteSetting = extensions::PermissionsManager::UserSiteSetting;

// Radio buttons group id for site access settings.
constexpr int kGroupId = 1;
// Button's indexes for site access settings.
constexpr size_t kGrantAllExtensionsIndex = 0;
constexpr size_t kBlockAllExtensionsIndex = 1;
constexpr size_t kCustomizeByExtensionIndex = 2;

ExtensionsTabbedMenuView* g_extensions_dialog = nullptr;

// Adds a new tab in `tabbed_pane` at `index` with the given `contents` and
// `footer`.
void CreateTab(raw_ptr<views::TabbedPane> tabbed_pane,
               size_t index,
               int title_string_id,
               std::unique_ptr<views::View> contents,
               std::unique_ptr<views::View> footer) {
  // This is set so that the extensions menu doesn't fall outside the monitor in
  // a maximized window in 1024x768. See https://crbug.com/1096630.
  // TODO(pbos): Consider making this dynamic and handled by views. Ideally we
  // wouldn't ever pop up so that they pop outside the screen.
  constexpr int kMaxExtensionButtonsHeightDp = 448;

  auto tab_container =
      views::Builder<views::View>()
          .SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical))
          .AddChildren(
              views::Builder<views::ScrollView>()
                  .SetContents(views::Builder<views::View>(std::move(contents)))
                  .ClipHeightTo(0, kMaxExtensionButtonsHeightDp)
                  .SetDrawOverflowIndicator(false)
                  .SetHorizontalScrollBarMode(
                      views::ScrollView::ScrollBarMode::kDisabled),
              views::Builder<views::View>(std::move(footer)))
          .Build();

  tabbed_pane->AddTabAtIndex(index, l10n_util::GetStringUTF16(title_string_id),
                             std::move(tab_container));
}

// Converts a view to a InstalledExtensionsMenuItemView. This cannot
// be used to *determine* if a view is an InstalledExtensionMenuItemView (it
// should only be used when the view is known to be one). It is only used as an
// extra measure to prevent bad static casts.
InstalledExtensionMenuItemView* GetAsInstalledExtensionMenuItem(
    views::View* view) {
  DCHECK(views::IsViewClass<InstalledExtensionMenuItemView>(view));
  return views::AsViewClass<InstalledExtensionMenuItemView>(view);
}

// Converts a view to a SiteAccessMenuItemView. This cannot
// be used to *determine* if a view is an SiteAccessMenuItemView (it
// should only be used when the view is known to be one). It is only used as an
// extra measure to prevent bad static casts.
SiteAccessMenuItemView* GetAsSiteAccessMenuItem(views::View* view) {
  DCHECK(views::IsViewClass<SiteAccessMenuItemView>(view));
  return views::AsViewClass<SiteAccessMenuItemView>(view);
}

// Returns the InstalledExtensionsMenuItemView corresponding to `action_id` if
// it is a children of `parent_view`. The children of the parent view must be
// InstalledExtensionsMenuItemView, otherwise it will DCHECK.
InstalledExtensionMenuItemView* GetInstalledExtensionMenuItem(
    views::View* parent_view,
    const ToolbarActionsModel::ActionId& action_id) {
  for (auto* view : parent_view->children()) {
    auto* item_view = GetAsInstalledExtensionMenuItem(view);
    if (item_view->view_controller()->GetId() == action_id)
      return item_view;
  }
  return nullptr;
}

// Returns the SiteAccessMenuItemView corresponding to `action_id` if it is a
// children of `parent_view`. The children of the parent view must be
// SiteAccessMenuItemView, otherwise it will DCHECK.
SiteAccessMenuItemView* GetSiteAccessMenuItem(
    views::View* parent_view,
    const ToolbarActionsModel::ActionId& action_id) {
  for (auto* view : parent_view->children()) {
    auto* item_view = GetAsSiteAccessMenuItem(view);
    if (item_view->view_controller()->GetId() == action_id)
      return item_view;
  }
  return nullptr;
}

// Returns the view controller of `view`. The view must be
// InstalledExtensionMenuItemView or SiteAccessMenuItemView, since both have the
// same controller, otherwise it will return a nullptr.
ToolbarActionViewController* GetMenuItemViewController(views::View* view) {
  if (views::IsViewClass<InstalledExtensionMenuItemView>(view))
    return GetAsInstalledExtensionMenuItem(view)->view_controller();
  else if (views::IsViewClass<SiteAccessMenuItemView>(view))
    return GetAsSiteAccessMenuItem(view)->view_controller();
  NOTREACHED();
  return nullptr;
}

// Returns the current index or insert position of `extension_name` in
// `parent_view`, based on alphabetical order.
int FindIndex(views::View* parent_view, const std::u16string extension_name) {
  const auto& children = parent_view->children();
  return std::find_if(
             children.begin(), children.end(),
             [extension_name](views::View* v) {
               return base::i18n::ToLower(extension_name) <=
                      base::i18n::ToLower(
                          GetMenuItemViewController(v)->GetActionName());
             }) -
         children.begin();
}

// Returns the web content's host. This method should only be called when
// web contents are present.
std::u16string GetCurrentHost(content::WebContents* web_contents) {
  DCHECK(web_contents);
  return url_formatter::IDNToUnicode(
      url_formatter::StripWWW(web_contents->GetLastCommittedURL().host()));
}

// Sets the `label` text to `message_id` with `current_host` emphasized.
void SetLabelTextAndStyle(views::Label& label,
                          int message_id,
                          std::u16string current_host) {
  size_t offset = 0u;
  label.SetText(l10n_util::GetStringFUTF16(message_id, current_host, &offset));
  label.SetTextStyleRange(ChromeTextStyle::STYLE_EMPHASIZED,
                          gfx::Range(offset, offset + current_host.length()));
}

void SetButtonChecked(views::View* container, size_t index) {
  DCHECK_LT(index, container->children().size());
  static_cast<views::RadioButton*>(container->children().at(index))
      ->SetChecked(true);
}

}  // namespace

ExtensionsTabbedMenuView::ExtensionsTabbedMenuView(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container,
    ExtensionsToolbarButton::ButtonType button_type,
    bool allow_pinning)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      browser_(browser),
      extensions_container_(extensions_container),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())),
      allow_pinning_(allow_pinning),
      requests_access_{
          nullptr, nullptr, nullptr,
          IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_REQUESTS_ACCESS_SECTION_TITLE,
          extensions::SitePermissionsHelper::SiteInteraction::kPending},
      has_access_{nullptr, nullptr, nullptr,
                  IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_HAS_ACCESS_SECTION_TITLE,
                  extensions::SitePermissionsHelper::SiteInteraction::kActive} {
  views::Builder<ExtensionsTabbedMenuView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      .SetTitle(IDS_EXTENSIONS_MENU_TITLE)
      .set_margins(gfx::Insets(0))
      .SetButtons(ui::DIALOG_BUTTON_NONE)
      .SetShowCloseButton(true)
      // Let anchor view's MenuButtonController handle the highlight.
      .set_highlight_button_when_shown(false)
      .SetEnableArrowKeyTraversal(true)
      .BuildChildren();

  // Ensure layer masking is used for the extensions menu to ensure buttons with
  // layer effects sitting flush with the bottom of the bubble are clipped
  // appropriately.
  SetPaintClientToLayer(true);

  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());

  toolbar_model_observation_.Observe(toolbar_model_.get());
  browser_->tab_strip_model()->AddObserver(this);
  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(browser_->profile()));

  Populate();

  // Tabs left to right order is 'site access tab' | 'extensions tab'.
  tabbed_pane_->SelectTabAt(button_type ==
                            ExtensionsToolbarButton::ButtonType::kExtensions);
}

ExtensionsTabbedMenuView::~ExtensionsTabbedMenuView() {
  g_extensions_dialog = nullptr;

  // Note: No need to call TabStripModel::RemoveObserver(), because it's handled
  // directly within TabStripModelObserver::~TabStripModelObserver().
}

// static
views::Widget* ExtensionsTabbedMenuView::ShowBubble(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container_,
    ExtensionsToolbarButton::ButtonType button_type,
    bool allow_pining) {
  DCHECK(!g_extensions_dialog);
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsMenuAccessControl));
  g_extensions_dialog = new ExtensionsTabbedMenuView(
      anchor_view, browser, extensions_container_, button_type, allow_pining);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(g_extensions_dialog);
  widget->Show();
  return widget;
}

// static
bool ExtensionsTabbedMenuView::IsShowing() {
  return g_extensions_dialog != nullptr;
}

// static
void ExtensionsTabbedMenuView::Hide() {
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsMenuAccessControl));
  if (IsShowing()) {
    g_extensions_dialog->GetWidget()->Close();
    // Set the dialog to nullptr since `GetWidget->Close()` is not synchronous.
    g_extensions_dialog = nullptr;
  }
}

// static
ExtensionsTabbedMenuView*
ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting() {
  return g_extensions_dialog;
}

std::vector<InstalledExtensionMenuItemView*>
ExtensionsTabbedMenuView::GetInstalledItemsForTesting() const {
  std::vector<InstalledExtensionMenuItemView*> menu_item_views;
  if (IsShowing()) {
    for (views::View* view : installed_items_->children())
      menu_item_views.push_back(GetAsInstalledExtensionMenuItem(view));
  }
  return menu_item_views;
}

std::vector<SiteAccessMenuItemView*>
ExtensionsTabbedMenuView::GetVisibleHasAccessItemsForTesting() const {
  return GetVisibleMenuItemsOf(has_access_);
}

std::vector<SiteAccessMenuItemView*>
ExtensionsTabbedMenuView::GetVisibleRequestsAccessItemsForTesting() const {
  return GetVisibleMenuItemsOf(requests_access_);
}

views::Label* ExtensionsTabbedMenuView::GetSiteAccessMessageForTesting() const {
  return site_access_message_;
}

HoverButton* ExtensionsTabbedMenuView::GetDiscoverMoreButtonForTesting() const {
  return discover_more_button_;
}

HoverButton* ExtensionsTabbedMenuView::GetSiteSettingsButtonForTesting() const {
  return site_settings_button_;
}

views::View* ExtensionsTabbedMenuView::GetSiteSettingsForTesting() const {
  return site_settings_;
}

size_t ExtensionsTabbedMenuView::GetSelectedTabIndex() const {
  return tabbed_pane_->GetSelectedTabIndex();
}

std::u16string ExtensionsTabbedMenuView::GetAccessibleWindowTitle() const {
  // The title is already spoken via the call to SetTitle().
  return std::u16string();
}

void ExtensionsTabbedMenuView::TabChangedAt(content::WebContents* contents,
                                            int index,
                                            TabChangeType change_type) {
  Update();
}

void ExtensionsTabbedMenuView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  Update();
}

void ExtensionsTabbedMenuView::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  auto extension_name = toolbar_model_->GetExtensionName(action_id);
  auto index = FindIndex(installed_items_, extension_name);
  CreateAndInsertInstalledExtension(action_id, index);

  MaybeCreateAndInsertSiteAccessItem(action_id);
  UpdateSiteAccessTab();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  auto remove_item = [](views::View* parent_view, views::View* item_view) {
    if (item_view)
      parent_view->RemoveChildViewT(item_view);
  };

  remove_item(installed_items_,
              GetInstalledExtensionMenuItem(installed_items_, action_id));
  remove_item(requests_access_.items,
              GetSiteAccessMenuItem(requests_access_.items, action_id));
  remove_item(has_access_.items,
              GetSiteAccessMenuItem(has_access_.items, action_id));

  UpdateSiteAccessTab();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  UpdateInstalledExtensionMenuItems({action_id});
  UpdateSiteAccessMenuItems({action_id});

  UpdateSiteAccessTab();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::OnToolbarModelInitialized() {
  DCHECK(installed_items_->children().empty());
  DCHECK(requests_access_.items->children().empty());
  DCHECK(has_access_.items->children().empty());
  Populate();
}

void ExtensionsTabbedMenuView::OnToolbarPinnedActionsChanged() {
  for (views::View* view : installed_items_->children())
    GetAsInstalledExtensionMenuItem(view)->UpdatePinButton();
}

void ExtensionsTabbedMenuView::Populate() {
  // The actions for the profile haven't been initialized yet. We'll call in
  // again once they have.
  if (!toolbar_model_->actions_initialized())
    return;

  DCHECK(children().empty()) << "Populate() can only be called once!";

  tabbed_pane_ = AddChildView(std::make_unique<views::TabbedPane>());
  tabbed_pane_->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  CreateSiteAccessTab();
  CreateExtensionsTab();

  // Sort action ids based on their extension name.
  auto sort_by_name = [this](const ToolbarActionsModel::ActionId a,
                             const ToolbarActionsModel::ActionId b) {
    return base::i18n::ToLower(toolbar_model_->GetExtensionName(a)) <
           base::i18n::ToLower(toolbar_model_->GetExtensionName(b));
  };
  std::vector<std::string> sorted_ids(toolbar_model_->action_ids().begin(),
                                      toolbar_model_->action_ids().end());
  std::sort(sorted_ids.begin(), sorted_ids.end(), sort_by_name);

  for (size_t i = 0; i < sorted_ids.size(); ++i) {
    CreateAndInsertInstalledExtension(sorted_ids[i], i);
    MaybeCreateAndInsertSiteAccessItem(sorted_ids[i]);
  }

  UpdateSiteAccessTab();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::UserPermissionsSettingsChanged(
    const extensions::PermissionsManager::UserPermissionsSettings& settings) {
  UpdateSiteAccessMenuItems(toolbar_model_->action_ids());
  UpdateSiteAccessTab();

  SizeToContents();
}

void ExtensionsTabbedMenuView::Update() {
  const auto& action_ids = toolbar_model_->action_ids();
  UpdateInstalledExtensionMenuItems(action_ids);
  UpdateSiteAccessMenuItems(action_ids);

  UpdateSiteAccessTab();

  ConsistencyCheck();
}

void ExtensionsTabbedMenuView::CreateSiteAccessTab() {
  auto* web_contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;

  auto current_host = GetCurrentHost(web_contents);
  const int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
  const int vertical_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTROL_LIST_VERTICAL);

  auto create_section_builder =
      [=](ExtensionsTabbedMenuView::SiteAccessSection* section) {
        auto section_view =
            views::Builder<views::BoxLayoutView>()
                .CopyAddressTo(&section->container)
                .SetOrientation(views::BoxLayout::Orientation::kVertical)
                // Start off with the section invisible. We'll update it as we
                // add items if necessary.
                .SetVisible(false)
                .AddChildren(
                    // Empty header explaining the section. Text will be
                    // populated later since it depends on the current site.
                    views::Builder<views::Label>()
                        .CopyAddressTo(&section->header)
                        .SetTextContext(
                            ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
                        .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                        .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
                            vertical_spacing, horizontal_spacing,
                            vertical_spacing, horizontal_spacing))),
                    // Empty section for the menu items. Items
                    // will be populated later.
                    views::Builder<views::BoxLayoutView>()
                        .CopyAddressTo(&section->items)
                        .SetOrientation(
                            views::BoxLayout::Orientation::kVertical));
        return section_view;
      };

  auto site_access_content =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .AddChildren(
              // Site access sections displayed when the extension has host
              // permissions.
              create_section_builder(&requests_access_),
              create_section_builder(&has_access_),
              // Label view displayed when the extension has no host
              // permissions, or the user cannot customize them. Text and
              // visibility is set dependent on the site settings selected.
              views::Builder<views::Label>()
                  .CopyAddressTo(&site_access_message_)
                  .SetVisible(false)
                  .SetBorder(views::CreateEmptyBorder(
                      gfx::Insets::TLBR(vertical_spacing, horizontal_spacing,
                                        vertical_spacing, horizontal_spacing)))
                  .SetTextContext(
                      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL))

          .Build();

  const auto create_radio_button_builder =
      [this, current_host](UserSiteSetting site_settings, int label_id) {
        auto label = ((site_settings == UserSiteSetting::kGrantAllExtensions) ||
                      (site_settings == UserSiteSetting::kBlockAllExtensions))
                         ? l10n_util::GetStringFUTF16(label_id, current_host)
                         : l10n_util::GetStringUTF16(label_id);
        return views::Builder<views::RadioButton>(
                   std::make_unique<views::RadioButton>(label, kGroupId))
            .SetCallback(base::BindRepeating(
                &ExtensionsTabbedMenuView::OnSiteSettingSelected,
                base::Unretained(this), site_settings));
      };

  auto site_access_footer =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .AddChildren(
              // The following bind is safe because the button will be owned by
              // the parent views and therefore callback can only happen if the
              // button exists and can be clicked.
              views::Builder<SiteSettingsExpandButton>(
                  std::make_unique<
                      SiteSettingsExpandButton>(base::BindRepeating(
                      &ExtensionsTabbedMenuView::OnSiteSettingsButtonPressed,
                      base::Unretained(this))))
                  .CopyAddressTo(&site_settings_button_),
              views::Builder<views::BoxLayoutView>()
                  .CopyAddressTo(&site_settings_)
                  .SetOrientation(views::BoxLayout::Orientation::kVertical)
                  .SetVisible(show_site_settings_)
                  .AddChildAt(
                      create_radio_button_builder(
                          UserSiteSetting::kGrantAllExtensions,
                          IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_USER_SETTINGS_ALLOW_ALL_TEXT),
                      kGrantAllExtensionsIndex)
                  .AddChildAt(
                      create_radio_button_builder(
                          UserSiteSetting::kBlockAllExtensions,
                          IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_USER_SETTINGS_BLOCK_ALL_TEXT),
                      kBlockAllExtensionsIndex)
                  .AddChildAt(
                      create_radio_button_builder(
                          UserSiteSetting::kCustomizeByExtension,
                          IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_USER_SETTINGS_CUSTOMIZE_EACH_TEXT),
                      kCustomizeByExtensionIndex))
          .Build();

  CreateTab(tabbed_pane_, 0, IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_TITLE,
            std::move(site_access_content), std::move(site_access_footer));
}

void ExtensionsTabbedMenuView::CreateExtensionsTab() {
  auto installed_items =
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&installed_items_)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();

  auto webstore_icon = std::make_unique<views::ImageView>(
      ui::ImageModel::FromResourceId(IDR_WEBSTORE_ICON_16));
  auto open_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kOpenInNewIcon, ui::kColorIcon,
          webstore_icon->GetImageModel().Size().width()));

  auto installed_tab_footer =
      views::Builder<HoverButton>(
          std::make_unique<HoverButton>(
              base::BindRepeating(&chrome::ShowWebStore, browser_),
              std::move(webstore_icon),
              l10n_util::GetStringUTF16(
                  IDS_EXTENSIONS_MENU_EXTENSIONS_TAB_DISCOVER_MORE_TITLE),
              /*subtitle=*/std::u16string(), std::move(open_icon)))
          .CopyAddressTo(&discover_more_button_)
          .Build();

  CreateTab(tabbed_pane_, 1, IDS_EXTENSIONS_MENU_EXTENSIONS_TAB_TITLE,
            std::move(installed_items), std::move(installed_tab_footer));
}

void ExtensionsTabbedMenuView::CreateAndInsertInstalledExtension(
    const ToolbarActionsModel::ActionId& id,
    int index) {
  std::unique_ptr<ExtensionActionViewController> controller =
      ExtensionActionViewController::Create(id, browser_,
                                            extensions_container_);
  auto item = std::make_unique<InstalledExtensionMenuItemView>(
      browser_, std::move(controller), allow_pinning_);
  installed_items_->AddChildViewAt(std::move(item), index);
}

void ExtensionsTabbedMenuView::MaybeCreateAndInsertSiteAccessItem(
    const ToolbarActionsModel::ActionId& id) {
  std::unique_ptr<ExtensionActionViewController> controller =
      ExtensionActionViewController::Create(id, browser_,
                                            extensions_container_);

  // Extensions with no current site interaction don't belong to a site access
  // section and therefore do not need a site access item view.
  auto site_interaction = controller->GetSiteInteraction(
      browser_->tab_strip_model()->GetActiveWebContents());
  auto* section = GetSectionForSiteInteraction(site_interaction);
  if (!section)
    return;

  auto item = std::make_unique<SiteAccessMenuItemView>(
      browser_, std::move(controller), allow_pinning_);

  InsertSiteAccessItem(std::move(item), section);
}

void ExtensionsTabbedMenuView::InsertSiteAccessItem(
    std::unique_ptr<SiteAccessMenuItemView> item,
    SiteAccessSection* section) {
  DCHECK(section);

  int index =
      FindIndex(section->items, item->view_controller()->GetActionName());
  section->items->AddChildViewAt(std::move(item), index);
}

void ExtensionsTabbedMenuView::UpdateInstalledExtensionMenuItems(
    const base::flat_set<ToolbarActionsModel::ActionId>& action_ids) {
  for (const auto& action_id : action_ids) {
    auto* item = GetInstalledExtensionMenuItem(installed_items_, action_id);
    // Extensions should always have an installed extension menu entry.
    DCHECK(item);

    item->Update();
    installed_items_->ReorderChildView(
        item,
        FindIndex(installed_items_, item->view_controller()->GetActionName()));
  }
}

void ExtensionsTabbedMenuView::UpdateSiteAccessMenuItems(
    const base::flat_set<ToolbarActionsModel::ActionId>& action_ids) {
  for (const auto& action_id : action_ids) {
    // Retrieve the current section and item for the action id, if any.
    SiteAccessSection* section = nullptr;
    SiteAccessMenuItemView* item = nullptr;
    if (auto* current_item =
            GetSiteAccessMenuItem(requests_access_.items, action_id)) {
      section = &requests_access_;
      item = current_item;
    } else if (auto* current_item =
                   GetSiteAccessMenuItem(has_access_.items, action_id)) {
      section = &has_access_;
      item = current_item;
    }

    // Create item when it was not on a site access section. This can happen
    // when an extension didn't previously have or request access, and now does.
    if (!item) {
      MaybeCreateAndInsertSiteAccessItem(action_id);
      continue;
    }

    // Reorder item when it is in the same section.
    auto site_interaction = item->view_controller()->GetSiteInteraction(
        browser_->tab_strip_model()->GetActiveWebContents());
    if (site_interaction == section->site_interaction) {
      item->Update();
      int new_index =
          FindIndex(section->items, item->view_controller()->GetActionName());
      section->items->ReorderChildView(item, new_index);
      return;
    }

    // Remove item when it is in a different section or no section at all.
    std::unique_ptr<SiteAccessMenuItemView> item_to_move =
        section->items->RemoveChildViewT(item);
    auto* new_section = GetSectionForSiteInteraction(site_interaction);
    if (!new_section)
      return;

    // Re insert item to the correct section.
    item_to_move->Update();
    InsertSiteAccessItem(std::move(item_to_move), new_section);
  }
}

void ExtensionsTabbedMenuView::UpdateSiteAccessTab() {
  // Site access tab should only display content related to the current site.
  // Therefore, hide all the site access content views if this method is called
  // when there are no active web contents (e.g tab strip update is closing its
  // tabs).
  // TODO(emiliapaz): Consider adding a message instead of hiding the views.
  auto* web_contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    has_access_.container->SetVisible(false);
    requests_access_.container->SetVisible(false);
    site_access_message_->SetVisible(false);
    return;
  }

  // TODO(crbug.com/1263310): If user is on a chrome:-scheme page, show
  // respective message, and hide site settings.
  url::Origin origin = url::Origin::Create(web_contents->GetLastCommittedURL());
  extensions::PermissionsManager::UserSiteSetting site_setting =
      extensions::PermissionsManager::Get(browser_->profile())
          ->GetUserSiteSetting(origin);
  switch (site_setting) {
    case extensions::PermissionsManager::UserSiteSetting::kGrantAllExtensions:
      SetButtonChecked(site_settings_, kGrantAllExtensionsIndex);
      // TODO(crbug.com/1263310): Remove combobox from SiteAccessMenuItems.
      UpdateSiteAccessSectionsVisibility();
      // TODO(crbug.com/1263310): After finishing implementation of user
      // permission (grant user permissions with precedence over extension
      // permissions), check that "requests access" section is hidden, and
      // either "has access" section or message is visible.
      break;
    case extensions::PermissionsManager::UserSiteSetting::kBlockAllExtensions:
      SetButtonChecked(site_settings_, kBlockAllExtensionsIndex);
      site_access_message_->SetText(l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_BLOCK_ALL_EXTENSIONS_TEXT));
      site_access_message_->SetVisible(true);
      has_access_.container->SetVisible(false);
      requests_access_.container->SetVisible(false);
      break;
    case extensions::PermissionsManager::UserSiteSetting::kCustomizeByExtension:
      SetButtonChecked(site_settings_, kCustomizeByExtensionIndex);
      UpdateSiteAccessSectionsVisibility();
      break;
  }

  // Site access tab updates can happen during the menu construction, and
  // afterwards. The dialog bubble is created after constructing the menu,
  // therefore we can only resize the contents when the dialog exists.
  if (g_extensions_dialog)
    SizeToContents();
}

void ExtensionsTabbedMenuView::UpdateSiteAccessSectionsVisibility() {
  auto* web_contents = browser_->tab_strip_model()->GetActiveWebContents();
  DCHECK(web_contents);

  auto current_host = GetCurrentHost(web_contents);
  auto update_section = [current_host](SiteAccessSection* section) {
    SetLabelTextAndStyle(*section->header, section->header_string_id,
                         current_host);
    bool should_be_visible = !section->items->children().empty();
    if (section->container->GetVisible() != should_be_visible)
      section->container->SetVisible(should_be_visible);
  };

  update_section(&has_access_);
  update_section(&requests_access_);

  // Display a message when no extensions have or request access.
  if (!has_access_.container->GetVisible() &&
      !requests_access_.container->GetVisible()) {
    site_access_message_->SetVisible(true);
    SetLabelTextAndStyle(
        *site_access_message_,
        IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_NO_EXTENSIONS_HAVE_ACCESS_TEXT,
        current_host);
  } else {
    site_access_message_->SetVisible(false);
  }
}

ExtensionsTabbedMenuView::SiteAccessSection*
ExtensionsTabbedMenuView::GetSectionForSiteInteraction(
    extensions::SitePermissionsHelper::SiteInteraction status) {
  switch (status) {
    case extensions::SitePermissionsHelper::SiteInteraction::kNone:
      // Extensions with no interaction with the current site don't belong to a
      // site access section.
      return nullptr;
    case extensions::SitePermissionsHelper::SiteInteraction::kPending:
      return &requests_access_;
    case extensions::SitePermissionsHelper::SiteInteraction::kActive:
      return &has_access_;
  }
}

std::vector<SiteAccessMenuItemView*>
ExtensionsTabbedMenuView::GetVisibleMenuItemsOf(
    ExtensionsTabbedMenuView::SiteAccessSection section) const {
  std::vector<SiteAccessMenuItemView*> menu_items;
  if (IsShowing() && section.container->GetVisible()) {
    for (views::View* item : section.items->children())
      menu_items.push_back(GetAsSiteAccessMenuItem(item));
  }
  return menu_items;
}

void ExtensionsTabbedMenuView::OnSiteSettingsButtonPressed() {
  show_site_settings_ = !show_site_settings_;

  site_settings_button_->SetIcon(show_site_settings_);
  site_settings_->SetVisible(show_site_settings_);

  // Resize the menu according to the site settings visibility.
  SizeToContents();
}

void ExtensionsTabbedMenuView::OnSiteSettingSelected(
    extensions::PermissionsManager::UserSiteSetting site_settings) {
  auto current_origin = url::Origin::Create(browser_->tab_strip_model()
                                                ->GetActiveWebContents()
                                                ->GetLastCommittedURL());
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser_->profile());
  switch (site_settings) {
    case UserSiteSetting::kGrantAllExtensions:
      permissions_manager->AddUserPermittedSite(current_origin);
      break;
    case UserSiteSetting::kBlockAllExtensions:
      permissions_manager->AddUserRestrictedSite(current_origin);
      break;
    case UserSiteSetting::kCustomizeByExtension:
      permissions_manager->RemoveUserPermittedSite(current_origin);
      permissions_manager->RemoveUserRestrictedSite(current_origin);
      break;
  }
}

void ExtensionsTabbedMenuView::ConsistencyCheck() {
#if DCHECK_IS_ON()
  const base::flat_set<std::string>& action_ids = toolbar_model_->action_ids();

  auto check_items = [action_ids, this](views::View* parent_view) {
    // Check that all items are owned by the view hierarchy, and that each
    // corresponds to an item in the model.
    std::vector<std::u16string> item_names;
    for (views::View* view : parent_view->children()) {
      DCHECK(Contains(view));
      auto* view_controller = GetMenuItemViewController(view);
      DCHECK(base::Contains(action_ids, view_controller->GetId()));
      item_names.push_back(
          base::i18n::ToLower(view_controller->GetActionName()));
    }

    // Verify that all items are properly sorted.
    DCHECK(std::is_sorted(item_names.begin(), item_names.end()));
  };

  check_items(installed_items_);
  check_items(requests_access_.items);
  check_items(has_access_.items);
#endif
}

BEGIN_METADATA(ExtensionsTabbedMenuView, views::BubbleDialogDelegateView)
END_METADATA

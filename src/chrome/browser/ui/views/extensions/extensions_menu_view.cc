// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
// If true, allows more than one instance of the ExtensionsMenuView, which may
// not be the active instance in g_extensions_dialog.
bool g_allow_testing_dialogs = false;

ExtensionsMenuView* g_extensions_dialog = nullptr;

constexpr int EXTENSIONS_SETTINGS_ID = 42;

constexpr int kSettingsIconSize = 16;

bool CompareExtensionMenuItemViews(const ExtensionsMenuItemView* a,
                                   const ExtensionsMenuItemView* b) {
  return base::i18n::ToLower(a->view_controller()->GetActionName()) <
         base::i18n::ToLower(b->view_controller()->GetActionName());
}

// A helper method to convert to an ExtensionsMenuItemView. This cannot be used
// to *determine* if a view is an ExtensionsMenuItemView (it should only be used
// when the view is known to be one). It is only used as an extra measure to
// prevent bad static casts.
ExtensionsMenuItemView* GetAsMenuItemView(views::View* view) {
  DCHECK(views::IsViewClass<ExtensionsMenuItemView>(view));
  return static_cast<ExtensionsMenuItemView*>(view);
}

}  // namespace

ExtensionsMenuView::ExtensionsMenuView(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container,
    bool allow_pinning)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      browser_(browser),
      extensions_container_(extensions_container),
      allow_pinning_(allow_pinning),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())),
      cant_access_{nullptr, nullptr,
                   IDS_EXTENSIONS_MENU_CANT_ACCESS_SITE_DATA_SHORT,
                   IDS_EXTENSIONS_MENU_CANT_ACCESS_SITE_DATA,
                   ToolbarActionViewController::PageInteractionStatus::kNone},
      wants_access_{
          nullptr, nullptr, IDS_EXTENSIONS_MENU_WANTS_TO_ACCESS_SITE_DATA_SHORT,
          IDS_EXTENSIONS_MENU_WANTS_TO_ACCESS_SITE_DATA,
          ToolbarActionViewController::PageInteractionStatus::kPending},
      has_access_{nullptr, nullptr,
                  IDS_EXTENSIONS_MENU_ACCESSING_SITE_DATA_SHORT,
                  IDS_EXTENSIONS_MENU_ACCESSING_SITE_DATA,
                  ToolbarActionViewController::PageInteractionStatus::kActive} {
  // Ensure layer masking is used for the extensions menu to ensure buttons with
  // layer effects sitting flush with the bottom of the bubble are clipped
  // appropriately.
  SetPaintClientToLayer(true);

  toolbar_model_observation_.Observe(toolbar_model_.get());
  browser_->tab_strip_model()->AddObserver(this);
  set_margins(gfx::Insets(0));

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetTitle(IDS_EXTENSIONS_MENU_TITLE);
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());

  SetEnableArrowKeyTraversal(true);

  // Let anchor view's MenuButtonController handle the highlight.
  set_highlight_button_when_shown(false);

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  Populate();
}

ExtensionsMenuView::~ExtensionsMenuView() {
  if (!g_allow_testing_dialogs)
    DCHECK_EQ(g_extensions_dialog, this);
  g_extensions_dialog = nullptr;
  extensions_menu_items_.clear();

  // Note: No need to call TabStripModel::RemoveObserver(), because it's handled
  // directly within TabStripModelObserver::~TabStripModelObserver().
}

void ExtensionsMenuView::Populate() {
  // The actions for the profile haven't been initialized yet. We'll call in
  // again once they have.
  if (!toolbar_model_->actions_initialized())
    return;

  DCHECK(children().empty()) << "Populate() can only be called once!";

  auto extension_buttons = CreateExtensionButtonsContainer();

  // This is set so that the extensions menu doesn't fall outside the monitor in
  // a maximized window in 1024x768. See https://crbug.com/1096630.
  // TODO(pbos): Consider making this dynamic and handled by views. Ideally we
  // wouldn't ever pop up so that they pop outside the screen.
  constexpr int kMaxExtensionButtonsHeightDp = 448;
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->ClipHeightTo(0, kMaxExtensionButtonsHeightDp);
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetContents(std::move(extension_buttons));
  AddChildView(std::move(scroll_view));

  AddChildView(std::make_unique<views::Separator>());

  // TODO(pbos): Consider moving this a footnote view (::SetFootnoteView()).
  // If so this needs to be created before being added to a widget, constructor
  // would do.
  auto footer = CreateBubbleMenuItem(
      EXTENSIONS_SETTINGS_ID, l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS),
      base::BindRepeating(&chrome::ShowExtensions, browser_, std::string()));

  // Extension icons are larger-than-favicon as they contain internal padding
  // (space for badging). Add the same padding left and right of the icon to
  // visually align the settings icon and text with extension menu items.
  // TODO(pbos): Note that this code relies on CreateBubbleMenuItem() and
  // ExtensionsMenuItemView using the same horizontal border size and
  // image-label spacing. This dependency should probably be more explicit.
  constexpr int kSettingsIconHorizontalPadding =
      (ExtensionsMenuItemView::kIconSize.width() - kSettingsIconSize) / 2;

  footer->SetBorder(views::CreateEmptyBorder(
      footer->GetInsets() +
      gfx::Insets(0, kSettingsIconHorizontalPadding, 0, 0)));
  footer->SetImageLabelSpacing(footer->GetImageLabelSpacing() +
                               kSettingsIconHorizontalPadding);

  manage_extensions_button_ = footer.get();
  AddChildView(std::move(footer));

  // Add menu items for each extension.
  for (const auto& id : toolbar_model_->action_ids())
    CreateAndInsertNewItem(id);

  SortMenuItemsByName();
  UpdateSectionVisibility();

  SanityCheck();
}

std::unique_ptr<views::View>
ExtensionsMenuView::CreateExtensionButtonsContainer() {
  auto extension_buttons = std::make_unique<views::View>();
  extension_buttons->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto create_section =
      [&extension_buttons](Section* section) {
        auto container = std::make_unique<views::View>();
        section->container = container.get();
        container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));

        const int horizontal_spacing =
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_BUTTON_HORIZONTAL_PADDING);

        // Add an emphasized short header explaining the section.
        auto header = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(section->header_string_id),
            ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
            ChromeTextStyle::STYLE_EMPHASIZED);
        header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        header->SetBorder(views::CreateEmptyBorder(
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                DISTANCE_CONTROL_LIST_VERTICAL),
            horizontal_spacing, 0, horizontal_spacing));
        container->AddChildView(std::move(header));

        // Add longer text that explains the section in more detail.
        auto description = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(section->description_string_id),
            ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
            views::style::STYLE_PRIMARY);
        description->SetMultiLine(true);
        description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        description->SetBorder(views::CreateEmptyBorder(0, horizontal_spacing,
                                                        0, horizontal_spacing));
        container->AddChildView(std::move(description));

        // Add a (currently empty) section for the menu items of the section.
        auto menu_items = std::make_unique<views::View>();
        menu_items->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
        section->menu_items = menu_items.get();
        container->AddChildView(std::move(menu_items));

        // Start off with the section invisible. We'll update it as we add items
        // if necessary.
        container->SetVisible(false);

        extension_buttons->AddChildView(std::move(container));
      };

  create_section(&has_access_);
  create_section(&wants_access_);
  create_section(&cant_access_);

  return extension_buttons;
}

ExtensionsMenuView::Section* ExtensionsMenuView::GetSectionForStatus(
    ToolbarActionViewController::PageInteractionStatus status) {
  Section* section = nullptr;
  switch (status) {
    case ToolbarActionViewController::PageInteractionStatus::kNone:
      section = &cant_access_;
      break;
    case ToolbarActionViewController::PageInteractionStatus::kPending:
      section = &wants_access_;
      break;
    case ToolbarActionViewController::PageInteractionStatus::kActive:
      section = &has_access_;
      break;
  }
  DCHECK(section);
  return section;
}

void ExtensionsMenuView::SortMenuItemsByName() {
  auto sort_section = [](Section* section) {
    if (section->menu_items->children().empty())
      return;

    std::vector<ExtensionsMenuItemView*> menu_item_views;
    for (views::View* view : section->menu_items->children())
      menu_item_views.push_back(GetAsMenuItemView(view));

    std::sort(menu_item_views.begin(), menu_item_views.end(),
              &CompareExtensionMenuItemViews);
    for (size_t i = 0; i < menu_item_views.size(); ++i)
      section->menu_items->ReorderChildView(menu_item_views[i], i);
  };

  sort_section(&has_access_);
  sort_section(&wants_access_);
  sort_section(&cant_access_);
}

void ExtensionsMenuView::CreateAndInsertNewItem(
    const ToolbarActionsModel::ActionId& id) {
  std::unique_ptr<ExtensionActionViewController> controller =
      ExtensionActionViewController::Create(id, browser_,
                                            extensions_container_);

  // The bare `new` is safe here, because InsertMenuItem is guaranteed to
  // be added to the view hierarchy, which takes ownership.
  auto* item = new ExtensionsMenuItemView(
      ExtensionsMenuItemView::MenuItemType::kExtensions, browser_,
      std::move(controller), allow_pinning_);
  extensions_menu_items_.insert(item);
  InsertMenuItem(item);
  // Sanity check that the item was added.
  DCHECK(Contains(item));
}

void ExtensionsMenuView::InsertMenuItem(ExtensionsMenuItemView* menu_item) {
  DCHECK(!Contains(menu_item))
      << "Trying to insert a menu item that is already added in a section!";
  const ToolbarActionViewController::PageInteractionStatus status =
      menu_item->view_controller()->GetPageInteractionStatus(
          browser_->tab_strip_model()->GetActiveWebContents());

  Section* const section = GetSectionForStatus(status);
  // Add the view at the end. Note that this *doesn't* insert the item at the
  // correct spot or ensure the view is visible; it's assumed that any callers
  // will handle those separately.
  section->menu_items->AddChildView(menu_item);
}

void ExtensionsMenuView::UpdateSectionVisibility() {
  auto update_section = [](Section* section) {
    bool should_be_visible = !section->menu_items->children().empty();
    if (section->container->GetVisible() != should_be_visible)
      section->container->SetVisible(should_be_visible);
  };

  update_section(&has_access_);
  update_section(&wants_access_);
  update_section(&cant_access_);
}

void ExtensionsMenuView::Update() {
  for (ExtensionsMenuItemView* view : extensions_menu_items_)
    view->view_controller()->UpdateState();

  content::WebContents* const web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  auto move_children_between_sections_if_necessary = [this, web_contents](
                                                         Section* section) {
    // Note: Collect the views to move separately, so that we don't change the
    // children of the view during iteration.
    std::vector<ExtensionsMenuItemView*> views_to_move;
    for (views::View* view : section->menu_items->children()) {
      auto* menu_item = GetAsMenuItemView(view);
      ToolbarActionViewController::PageInteractionStatus status =
          menu_item->view_controller()->GetPageInteractionStatus(web_contents);
      if (status == section->page_status)
        continue;
      views_to_move.push_back(menu_item);
    }

    for (ExtensionsMenuItemView* menu_item : views_to_move) {
      section->menu_items->RemoveChildView(menu_item);
      InsertMenuItem(menu_item);
    }
  };

  move_children_between_sections_if_necessary(&has_access_);
  move_children_between_sections_if_necessary(&wants_access_);
  move_children_between_sections_if_necessary(&cant_access_);

  SortMenuItemsByName();
  UpdateSectionVisibility();

  SanityCheck();
}

void ExtensionsMenuView::SanityCheck() {
#if DCHECK_IS_ON()
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();

  // Sanity checks: verify that all extensions are properly sorted and in the
  // correct section.
  auto check_section = [this, web_contents](Section* section) {
    std::vector<ExtensionsMenuItemView*> menu_items;
    for (views::View* view : section->menu_items->children()) {
      auto* menu_item = GetAsMenuItemView(view);
      ToolbarActionViewController::PageInteractionStatus status =
          menu_item->view_controller()->GetPageInteractionStatus(web_contents);
      DCHECK_EQ(section, GetSectionForStatus(status));
      menu_items.push_back(menu_item);
    }
    DCHECK(std::is_sorted(menu_items.begin(), menu_items.end(),
                          CompareExtensionMenuItemViews));
  };

  check_section(&has_access_);
  check_section(&wants_access_);
  check_section(&cant_access_);

  const base::flat_set<std::string>& action_ids = toolbar_model_->action_ids();
  DCHECK_EQ(action_ids.size(), extensions_menu_items_.size());

  // Check that all items are owned by the view hierarchy, and that each
  // corresponds to an item in the model (since we already checked that the size
  // is equal for |action_ids| and |extensions_menu_items_|, this implicitly
  // guarantees that we have a view per item in |action_ids| as well).
  for (ExtensionsMenuItemView* item : extensions_menu_items_) {
    DCHECK(Contains(item));
    DCHECK(base::Contains(action_ids, item->view_controller()->GetId()));
  }
#endif
}

std::u16string ExtensionsMenuView::GetAccessibleWindowTitle() const {
  // The title is already spoken via the call to SetTitle().
  return std::u16string();
}

void ExtensionsMenuView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  if (manage_extensions_button_) {
    const SkColor background_color =
        GetColorProvider()->GetColor(ui::kColorBubbleBackground);
    SkColor icon_color = GetColorProvider()->GetColor(ui::kColorMenuIcon);
    if (background_color != SK_ColorTRANSPARENT) {
      icon_color =
          color_utils::BlendForMinContrast(icon_color, background_color).color;
    }
    manage_extensions_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(vector_icons::kSettingsIcon, kSettingsIconSize,
                              icon_color));
  }
}

void ExtensionsMenuView::TabChangedAt(content::WebContents* contents,
                                      int index,
                                      TabChangeType change_type) {
  Update();
}

void ExtensionsMenuView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  Update();
}

void ExtensionsMenuView::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& item) {
  CreateAndInsertNewItem(item);
  SortMenuItemsByName();
  UpdateSectionVisibility();

  SanityCheck();
}

void ExtensionsMenuView::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  auto iter = base::ranges::find_if(
      extensions_menu_items_, [action_id](const ExtensionsMenuItemView* item) {
        return item->view_controller()->GetId() == action_id;
      });
  DCHECK(iter != extensions_menu_items_.end());
  ExtensionsMenuItemView* const view = *iter;
  DCHECK(Contains(view));
  view->parent()->RemoveChildView(view);
  DCHECK(!Contains(view));
  extensions_menu_items_.erase(iter);

  // Removing the child view take it out of the view hierarchy, but means we
  // have to manually delete it.
  delete view;

  UpdateSectionVisibility();

  SanityCheck();
}

void ExtensionsMenuView::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Update();
}

void ExtensionsMenuView::OnToolbarModelInitialized() {
  DCHECK(extensions_menu_items_.empty());
  Populate();
}

void ExtensionsMenuView::OnToolbarPinnedActionsChanged() {
  for (auto* menu_item : extensions_menu_items_)
    menu_item->UpdatePinButton();
}

// static
base::AutoReset<bool> ExtensionsMenuView::AllowInstancesForTesting() {
  return base::AutoReset<bool>(&g_allow_testing_dialogs, true);
}

// static
views::Widget* ExtensionsMenuView::ShowBubble(
    views::View* anchor_view,
    Browser* browser,
    ExtensionsContainer* extensions_container,
    bool allow_pinning) {
  DCHECK(!g_extensions_dialog);
  // Experiment `kExtensionsMenuAccessControl` is introducing a new menu. Check
  // `ExtensionsMenuView` is only constructed when the experiment is disabled.
  DCHECK(!base::FeatureList::IsEnabled(features::kExtensionsMenuAccessControl));
  g_extensions_dialog = new ExtensionsMenuView(
      anchor_view, browser, extensions_container, allow_pinning);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(g_extensions_dialog);
  widget->Show();
  return widget;
}

// static
bool ExtensionsMenuView::IsShowing() {
  return g_extensions_dialog != nullptr;
}

// static
void ExtensionsMenuView::Hide() {
  if (IsShowing())
    g_extensions_dialog->GetWidget()->Close();
}

// static
ExtensionsMenuView* ExtensionsMenuView::GetExtensionsMenuViewForTesting() {
  return g_extensions_dialog;
}

// static
std::vector<ExtensionsMenuItemView*>
ExtensionsMenuView::GetSortedItemsForSectionForTesting(
    ToolbarActionViewController::PageInteractionStatus status) {
  const ExtensionsMenuView::Section* section =
      GetExtensionsMenuViewForTesting()->GetSectionForStatus(status);
  std::vector<ExtensionsMenuItemView*> menu_item_views;
  for (views::View* view : section->menu_items->children())
    menu_item_views.push_back(GetAsMenuItemView(view));
  return menu_item_views;
}

BEGIN_METADATA(ExtensionsMenuView, views::BubbleDialogDelegateView)
END_METADATA

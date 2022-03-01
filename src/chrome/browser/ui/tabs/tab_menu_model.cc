// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"
#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

TabMenuModel::TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                           TabMenuModelDelegate* tab_menu_model_delegate,
                           TabStripModel* tab_strip,
                           int index)
    : ui::SimpleMenuModel(delegate),
      tab_menu_model_delegate_(tab_menu_model_delegate) {
  Build(tab_strip, index);
}

TabMenuModel::~TabMenuModel() {}

void TabMenuModel::Build(TabStripModel* tab_strip, int index) {
  std::vector<int> indices;
  if (tab_strip->IsTabSelected(index)) {
    const ui::ListSelectionModel::SelectedIndices& sel =
        tab_strip->selection_model().selected_indices();
    indices = std::vector<int>(sel.begin(), sel.end());
  } else {
    indices = {index};
  }

  int num_tabs = indices.size();
  AddItemWithStringId(TabStripModel::CommandNewTabToRight,
                      base::i18n::IsRTL() ? IDS_TAB_CXMENU_NEWTABTOLEFT
                                          : IDS_TAB_CXMENU_NEWTABTORIGHT);
  if (reading_list::switches::IsReadingListEnabled() &&
      !tab_strip->profile()->IsGuestSession()) {
    AddItem(
        TabStripModel::CommandAddToReadLater,
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_READ_LATER, num_tabs));
    SetEnabledAt(GetItemCount() - 1,
                 tab_strip->IsReadLaterSupportedForAny(indices));
    if (base::FeatureList::IsEnabled(features::kReadLaterNewBadgePromo))
      SetIsNewFeatureAt(GetItemCount() - 1, true);
  }
  if (ExistingTabGroupSubMenuModel::ShouldShowSubmenu(tab_strip, index)) {
    // Create submenu with existing groups
    add_to_existing_group_submenu_ =
        std::make_unique<ExistingTabGroupSubMenuModel>(delegate(), tab_strip,
                                                       index);
    AddSubMenu(TabStripModel::CommandAddToExistingGroup,
               l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_ADD_TAB_TO_GROUP,
                                                num_tabs),
               add_to_existing_group_submenu_.get());
    if (base::FeatureList::IsEnabled(features::kTabGroupsNewBadgePromo))
      SetIsNewFeatureAt(GetItemCount() - 1, true);
  } else {
    AddItem(TabStripModel::CommandAddToNewGroup,
            l10n_util::GetPluralStringFUTF16(
                IDS_TAB_CXMENU_ADD_TAB_TO_NEW_GROUP, num_tabs));
    SetElementIdentifierAt(GetItemCount() - 1, kAddToNewGroupItemIdentifier);
    if (base::FeatureList::IsEnabled(features::kTabGroupsNewBadgePromo))
      SetIsNewFeatureAt(GetItemCount() - 1, true);
  }

  for (const auto& selection : indices) {
    if (tab_strip->GetTabGroupForTab(selection).has_value()) {
      AddItemWithStringId(TabStripModel::CommandRemoveFromGroup,
                          IDS_TAB_CXMENU_REMOVE_TAB_FROM_GROUP);
      break;
    }
  }

  if (ExistingWindowSubMenuModel::ShouldShowSubmenu(tab_strip->profile())) {
    // Create submenu with existing windows
    add_to_existing_window_submenu_ = ExistingWindowSubMenuModel::Create(
        delegate(), tab_menu_model_delegate_, tab_strip, index);
    AddSubMenu(TabStripModel::CommandMoveToExistingWindow,
               l10n_util::GetPluralStringFUTF16(
                   IDS_TAB_CXMENU_MOVETOANOTHERWINDOW, num_tabs),
               add_to_existing_window_submenu_.get());
  } else {
    AddItem(TabStripModel::CommandMoveTabsToNewWindow,
            l10n_util::GetPluralStringFUTF16(
                IDS_TAB_CXMENU_MOVE_TABS_TO_NEW_WINDOW, num_tabs));
  }

  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
  AddItemWithStringId(TabStripModel::CommandDuplicate,
                      IDS_TAB_CXMENU_DUPLICATE);
  bool will_pin = tab_strip->WillContextMenuPin(index);
  AddItemWithStringId(
      TabStripModel::CommandTogglePinned,
      will_pin ? IDS_TAB_CXMENU_PIN_TAB : IDS_TAB_CXMENU_UNPIN_TAB);
  const bool will_mute = !chrome::AreAllSitesMuted(*tab_strip, indices);
  AddItem(TabStripModel::CommandToggleSiteMuted,
          will_mute ? l10n_util::GetPluralStringFUTF16(
                          IDS_TAB_CXMENU_SOUND_MUTE_SITE, num_tabs)
                    : l10n_util::GetPluralStringFUTF16(
                          IDS_TAB_CXMENU_SOUND_UNMUTE_SITE, num_tabs));
  if (send_tab_to_self::ShouldOfferFeature(
          tab_strip->GetWebContentsAt(index))) {
    AddSeparator(ui::NORMAL_SEPARATOR);

    if (send_tab_to_self::GetValidDeviceCount(tab_strip->profile()) == 1) {
#if defined(OS_MAC)
      AddItem(TabStripModel::CommandSendTabToSelfSingleTarget,
              l10n_util::GetStringFUTF16(
                  IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
                  send_tab_to_self::GetSingleTargetDeviceName(
                      tab_strip->profile())));
#else
      AddItemWithIcon(TabStripModel::CommandSendTabToSelfSingleTarget,
                      l10n_util::GetStringFUTF16(
                          IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
                          (send_tab_to_self::GetSingleTargetDeviceName(
                              tab_strip->profile()))),
                      ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    } else {
      send_tab_to_self_sub_menu_model_ =
          std::make_unique<send_tab_to_self::SendTabToSelfSubMenuModel>(
              tab_strip->GetWebContentsAt(index),
              send_tab_to_self::SendTabToSelfMenuType::kTab);
#if defined(OS_MAC)
      AddSubMenuWithStringId(TabStripModel::CommandSendTabToSelf,
                             IDS_CONTEXT_MENU_SEND_TAB_TO_SELF,
                             send_tab_to_self_sub_menu_model_.get());
#else
      AddSubMenuWithStringIdAndIcon(
          TabStripModel::CommandSendTabToSelf,
          IDS_CONTEXT_MENU_SEND_TAB_TO_SELF,
          send_tab_to_self_sub_menu_model_.get(),
          ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    }
  }

  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(TabStripModel::CommandCloseTab, IDS_TAB_CXMENU_CLOSETAB);
  AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                      IDS_TAB_CXMENU_CLOSEOTHERTABS);
  AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                      base::i18n::IsRTL() ? IDS_TAB_CXMENU_CLOSETABSTOLEFT
                                          : IDS_TAB_CXMENU_CLOSETABSTORIGHT);
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel,
                                      kAddToNewGroupItemIdentifier);

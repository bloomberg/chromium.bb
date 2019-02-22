// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/main_menu_builder.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#include "ui/strings/grit/ui_strings.h"

namespace chrome {
namespace {

using Item = internal::MenuItemBuilder;

base::scoped_nsobject<NSMenuItem> BuildAppMenu(NSApplication* nsapp,
                                               AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_APP_MENU_PRODUCT_NAME)
          .tag(IDC_CHROME_MENU)
          .submenu({
            Item(IDS_ABOUT_MAC)
                .string_format_1(IDS_PRODUCT_NAME)
                .tag(IDC_ABOUT)
                .target(app_controller)
                .action(@selector(orderFrontStandardAboutPanel:)),
                Item().is_separator(),
                Item(IDS_PREFERENCES)
                    .tag(IDC_OPTIONS)
                    .target(app_controller)
                    .action(@selector(showPreferences:)),
                Item().is_separator(),
                Item(IDS_CLEAR_BROWSING_DATA)
                    .command_id(IDC_CLEAR_BROWSING_DATA),
                Item(IDS_IMPORT_SETTINGS_MENU_MAC)
                    .command_id(IDC_IMPORT_SETTINGS),
                Item().is_separator(),
                Item(IDS_SERVICES_MAC).tag(-1).submenu({}),
                Item(IDS_HIDE_APP_MAC)
                    .string_format_1(IDS_PRODUCT_NAME)
                    .tag(IDC_HIDE_APP)
                    .action(@selector(hide:)),
                Item(IDS_HIDE_OTHERS_MAC)
                    .action(@selector(hideOtherApplications:)),
                Item(IDS_SHOW_ALL_MAC)
                    .action(@selector(unhideAllApplications:)),
                Item().is_separator(),
                Item(IDS_CONFIRM_TO_QUIT_OPTION)
                    .target(app_controller)
                    .action(@selector(toggleConfirmToQuit:)),
                Item().is_separator(),
                // AppKit inserts "Quit and Keep Windows" as an alternate item
                // automatically by using the -terminate: action.
                Item(IDS_EXIT_MAC)
                    .string_format_1(IDS_PRODUCT_NAME)
                    .tag(IDC_EXIT)
                    .target(nsapp)
                    .action(@selector(terminate:)),
          })
          .Build();

  NSMenuItem* services_item = [[item submenu] itemWithTag:-1];
  [services_item setTag:0];

  [nsapp setServicesMenu:[services_item submenu]];

  return item;
}

base::scoped_nsobject<NSMenuItem> BuildFileMenu(NSApplication* nsapp,
                                                AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_FILE_MENU_MAC)
          .tag(IDC_FILE_MENU)
          .submenu({
            Item(IDS_NEW_TAB_MAC).command_id(IDC_NEW_TAB),
                Item(IDS_NEW_WINDOW_MAC).command_id(IDC_NEW_WINDOW),
                Item(IDS_NEW_INCOGNITO_WINDOW_MAC)
                    .command_id(IDC_NEW_INCOGNITO_WINDOW),
                Item(IDS_REOPEN_CLOSED_TABS_MAC).command_id(IDC_RESTORE_TAB),
                Item(IDS_OPEN_FILE_MAC).command_id(IDC_OPEN_FILE),
                Item(IDS_OPEN_LOCATION_MAC).command_id(IDC_FOCUS_LOCATION),
                Item().is_separator(),
                // AppKit inserts "Close All" as an alternate item automatically
                // by using the -performClose: action.
                Item(IDS_CLOSE_WINDOW_MAC)
                    .tag(IDC_CLOSE_WINDOW)
                    .action(@selector(performClose:)),
                Item(IDS_CLOSE_TAB_MAC).command_id(IDC_CLOSE_TAB),
                Item(IDS_SAVE_PAGE_MAC).command_id(IDC_SAVE_PAGE),
                Item().is_separator(),
                Item(IDS_SHARE_MAC),
                Item().is_separator(), Item(IDS_PRINT).command_id(IDC_PRINT),
                Item(IDS_PRINT_USING_SYSTEM_DIALOG_MAC)
                    .command_id(IDC_BASIC_PRINT)
                    .is_alternate(),
          })
          .Build();

  // Wire up some legacy IBOutlets.
  [app_controller setValue:[[item submenu] itemWithTag:IDC_CLOSE_TAB]
                    forKey:@"closeTabMenuItem_"];
  [app_controller setValue:[[item submenu] itemWithTag:IDC_CLOSE_WINDOW]
                    forKey:@"closeWindowMenuItem_"];

  return item;
}

base::scoped_nsobject<NSMenuItem> BuildEditMenu(NSApplication* nsapp,
                                                AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_EDIT_MENU_MAC)
          .tag(IDC_EDIT_MENU)
          .submenu({
            Item(IDS_EDIT_UNDO_MAC)
                .tag(IDC_CONTENT_CONTEXT_UNDO)
                .action(@selector(undo:)),
                Item(IDS_EDIT_REDO_MAC)
                    .tag(IDC_CONTENT_CONTEXT_REDO)
                    .action(@selector(redo:)),
                Item().is_separator(),
                Item(IDS_CUT_MAC)
                    .tag(IDC_CONTENT_CONTEXT_CUT)
                    .action(@selector(cut:)),
                Item(IDS_COPY_MAC)
                    .tag(IDC_CONTENT_CONTEXT_COPY)
                    .action(@selector(copy:)),
                Item(IDS_PASTE_MAC)
                    .tag(IDC_CONTENT_CONTEXT_PASTE)
                    .action(@selector(paste:)),
                Item(IDS_PASTE_MATCH_STYLE_MAC)
                    .tag(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE)
                    .action(@selector(pasteAndMatchStyle:)),
                Item(IDS_PASTE_MATCH_STYLE_MAC)
                    .action(@selector(pasteAndMatchStyle:))
                    .is_alternate()
                    .key_equivalent(@"V", NSEventModifierFlagCommand |
                                              NSEventModifierFlagOption),
                Item(IDS_EDIT_DELETE_MAC)
                    .tag(IDC_CONTENT_CONTEXT_DELETE)
                    .action(@selector(delete:)),
                Item(IDS_EDIT_SELECT_ALL_MAC)
                    .tag(IDC_CONTENT_CONTEXT_SELECTALL)
                    .action(@selector(selectAll:)),
                Item().is_separator(),
                Item(IDS_EDIT_FIND_SUBMENU_MAC).tag(IDC_FIND_MENU).submenu({
                  Item(IDS_EDIT_SEARCH_WEB_MAC).command_id(IDC_FOCUS_SEARCH),
                      Item().is_separator(),
                      Item(IDS_EDIT_FIND_MAC).command_id(IDC_FIND),
                      Item(IDS_EDIT_FIND_NEXT_MAC).command_id(IDC_FIND_NEXT),
                      Item(IDS_EDIT_FIND_PREVIOUS_MAC)
                          .command_id(IDC_FIND_PREVIOUS),
                      Item(IDS_EDIT_USE_SELECTION_MAC)
                          .action(@selector(copyToFindPboard:))
                          .key_equivalent(@"e", NSEventModifierFlagCommand),
                      Item(IDS_EDIT_JUMP_TO_SELECTION_MAC)
                          .action(@selector(centerSelectionInVisibleArea:))
                          .key_equivalent(@"j", NSEventModifierFlagCommand),
                }),
                Item(IDS_EDIT_SPELLING_GRAMMAR_MAC)
                    .tag(IDC_SPELLCHECK_MENU)
                    .submenu({
                      Item(IDS_EDIT_SHOW_SPELLING_GRAMMAR_MAC)
                          .action(@selector(showGuessPanel:))
                          .key_equivalent(@":", NSEventModifierFlagCommand),
                          Item(IDS_EDIT_CHECK_DOCUMENT_MAC)
                              .action(@selector(checkSpelling:))
                              .key_equivalent(@";", NSEventModifierFlagCommand),
                          Item(IDS_EDIT_CHECK_SPELLING_TYPING_MAC)
                              .action(@selector
                                      (toggleContinuousSpellChecking:)),
                          Item(IDS_EDIT_CHECK_GRAMMAR_MAC)
                              .action(@selector(toggleGrammarChecking:)),
                    }),
                Item(IDS_SPEECH_MAC).tag(50158).submenu({
                  Item(IDS_SPEECH_START_SPEAKING_MAC)
                      .action(@selector(startSpeaking:)),
                      Item(IDS_SPEECH_STOP_SPEAKING_MAC)
                          .action(@selector(stopSpeaking:)),
                }),
            // The "Start Dictation..." and "Emoji & Symbols" items are
            // inserted by AppKit.
          })
          .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildViewMenu(NSApplication* nsapp,
                                                AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_VIEW_MENU_MAC)
          .tag(IDC_VIEW_MENU)
          .submenu({
            Item(IDS_BOOKMARK_BAR_ALWAYS_SHOW_MAC)
                .command_id(IDC_SHOW_BOOKMARK_BAR),
                Item(IDS_TOGGLE_FULLSCREEN_TOOLBAR_MAC)
                    .command_id(IDC_TOGGLE_FULLSCREEN_TOOLBAR),
                Item(IDS_CUSTOMIZE_TOUCH_BAR)
                    .tag(IDC_CUSTOMIZE_TOUCH_BAR)
                    .action(@selector(toggleTouchBarCustomizationPalette:)),
                Item().is_separator(),
                Item(IDS_STOP_MENU_MAC).command_id(IDC_STOP),
                Item(IDS_RELOAD_MENU_MAC).command_id(IDC_RELOAD),
                Item(IDS_RELOAD_BYPASSING_CACHE_MENU_MAC)
                    .command_id(IDC_RELOAD_BYPASSING_CACHE)
                    .is_alternate(),
                Item().is_separator(),
                Item(IDS_ENTER_FULLSCREEN_MAC)
                    .tag(IDC_FULLSCREEN)
                    .action(@selector(toggleFullScreen:)),
                Item(IDS_TEXT_DEFAULT_MAC).command_id(IDC_ZOOM_NORMAL),
                Item(IDS_TEXT_BIGGER_MAC).command_id(IDC_ZOOM_PLUS),
                Item(IDS_TEXT_SMALLER_MAC).command_id(IDC_ZOOM_MINUS),
                Item().is_separator(),
                Item(IDS_MEDIA_ROUTER_MENU_ITEM_TITLE)
                    .command_id(IDC_ROUTE_MEDIA),
                Item().is_separator(),
                Item(IDS_DEVELOPER_MENU_MAC)
                    .tag(IDC_DEVELOPER_MENU)
                    .submenu({
                        Item(IDS_VIEW_SOURCE_MAC).command_id(IDC_VIEW_SOURCE),
                        Item(IDS_DEV_TOOLS_MAC).command_id(IDC_DEV_TOOLS),
                        Item(IDS_DEV_TOOLS_CONSOLE_MAC)
                            .command_id(IDC_DEV_TOOLS_CONSOLE),
                        Item(IDS_ALLOW_JAVASCRIPT_APPLE_EVENTS_MAC)
                            .command_id(IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS),
                    }),
          })
          .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildHistoryMenu(
    NSApplication* nsapp,
    AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_HISTORY_MENU_MAC)
          .tag(IDC_HISTORY_MENU)
          .submenu({
              Item(IDS_HISTORY_HOME_MAC).command_id(IDC_HOME),
              Item(IDS_HISTORY_BACK_MAC).command_id(IDC_BACK),
              Item(IDS_HISTORY_FORWARD_MAC).command_id(IDC_FORWARD),
              Item()
                  .tag(HistoryMenuBridge::kRecentlyClosedSeparator)
                  .is_separator(),
              Item(IDS_HISTORY_CLOSED_MAC)
                  .tag(HistoryMenuBridge::kRecentlyClosedTitle),
              Item().tag(HistoryMenuBridge::kVisitedSeparator).is_separator(),
              Item(IDS_HISTORY_VISITED_MAC)
                  .tag(HistoryMenuBridge::kVisitedTitle),
              Item().tag(HistoryMenuBridge::kShowFullSeparator).is_separator(),
              Item(IDS_HISTORY_SHOWFULLHISTORY_LINK)
                  .command_id(IDC_SHOW_HISTORY),
          })
          .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildBookmarksMenu(
    NSApplication* nsapp,
    AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_BOOKMARKS_MENU)
          .tag(IDC_BOOKMARKS_MENU)
          .submenu({
              Item(IDS_BOOKMARK_MANAGER).command_id(IDC_SHOW_BOOKMARK_MANAGER),
              Item().tag(IDC_BOOKMARK_PAGE).is_separator(),
              Item(IDS_BOOKMARK_THIS_PAGE).command_id(IDC_BOOKMARK_PAGE),
              Item(IDS_BOOKMARK_ALL_TABS_MAC).command_id(IDC_BOOKMARK_ALL_TABS),
              Item().tag(IDC_BOOKMARK_PAGE).is_separator(),
          })
          .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildPeopleMenu(
    NSApplication* nsapp,
    AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item = Item(IDS_PROFILES_OPTIONS_GROUP_NAME)
                                               .tag(IDC_PROFILE_MAIN_MENU)
                                               .submenu({})
                                               .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildWindowMenu(
    NSApplication* nsapp,
    AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_WINDOW_MENU_MAC)
          .tag(IDC_WINDOW_MENU)
          .submenu({
            Item(IDS_MINIMIZE_WINDOW_MAC)
                .tag(IDC_MINIMIZE_WINDOW)
                .action(@selector(performMiniaturize:)),
                Item(IDS_ZOOM_WINDOW_MAC)
                    .tag(IDC_MAXIMIZE_WINDOW)
                    .action(@selector(performZoom:)),
                Item().is_separator(),
                Item(IDS_NEXT_TAB_MAC).command_id(IDC_SELECT_NEXT_TAB),
                Item(IDS_PREV_TAB_MAC).command_id(IDC_SELECT_PREVIOUS_TAB),
                Item(IDS_SHOW_AS_TAB).command_id(IDC_SHOW_AS_TAB),
                Item(IDS_DUPLICATE_TAB_MAC).command_id(IDC_DUPLICATE_TAB),
                Item(IDS_MUTE_SITE_MAC).command_id(IDC_WINDOW_MUTE_SITE),
                Item(IDS_PIN_TAB_MAC).command_id(IDC_WINDOW_PIN_TAB),
                Item().is_separator(),
                Item(IDS_SHOW_DOWNLOADS_MAC).command_id(IDC_SHOW_DOWNLOADS),
                Item(IDS_SHOW_EXTENSIONS_MAC).command_id(IDC_MANAGE_EXTENSIONS),
                Item(IDS_TASK_MANAGER_MAC).command_id(IDC_TASK_MANAGER),
                Item().is_separator(),
                Item(IDS_ALL_WINDOWS_FRONT_MAC)
                    .tag(IDC_ALL_WINDOWS_FRONT)
                    .action(@selector(arrangeInFront:)),
                Item().is_separator(),
          })
          .Build();
  [nsapp setWindowsMenu:[item submenu]];
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildHelpMenu(NSApplication* nsapp,
                                                AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_HELP_MENU_MAC)
          .submenu({
#if defined(GOOGLE_CHROME_BUILD)
            Item(IDS_FEEDBACK_MAC).command_id(IDC_FEEDBACK),
#endif
                Item(IDS_HELP_MAC)
                    .string_format_1(IDS_PRODUCT_NAME)
                    .command_id(IDC_HELP_PAGE_VIA_MENU),
          })
          .Build();
  [nsapp setHelpMenu:[item submenu]];
  return item;
}

}  // namespace

void BuildMainMenu(NSApplication* nsapp, AppController* app_controller) {
  base::scoped_nsobject<NSMenu> main_menu([[NSMenu alloc] initWithTitle:@""]);

  using Builder =
      base::scoped_nsobject<NSMenuItem> (*)(NSApplication*, AppController*);
  static const Builder kBuilderFuncs[] = {
      &BuildAppMenu,    &BuildFileMenu,    &BuildEditMenu,
      &BuildViewMenu,   &BuildHistoryMenu, &BuildBookmarksMenu,
      &BuildPeopleMenu, &BuildWindowMenu,  &BuildHelpMenu,
  };
  for (auto* builder : kBuilderFuncs) {
    [main_menu addItem:builder(nsapp, app_controller)];
  }

  [nsapp setMainMenu:main_menu];
}

namespace internal {

MenuItemBuilder::MenuItemBuilder(int string_id) : string_id_(string_id) {}

MenuItemBuilder::MenuItemBuilder(const MenuItemBuilder&) = default;

MenuItemBuilder& MenuItemBuilder::operator=(const MenuItemBuilder&) = default;

MenuItemBuilder::~MenuItemBuilder() = default;

base::scoped_nsobject<NSMenuItem> MenuItemBuilder::Build() const {
  if (is_separator_) {
    base::scoped_nsobject<NSMenuItem> item([[NSMenuItem separatorItem] retain]);
    if (tag_) {
      [item setTag:tag_];
    }
    return item;
  }

  // If the item is command-dispatched, look up the relevant key equivalent
  // from the accelerator table. Otherwise, use the builder-specified key
  // equivalent.
  NSString* key_equivalent = key_equivalent_;
  NSEventModifierFlags key_equivalent_flags = key_equivalent_flags_;
  if (tag_ != 0) {
    if (const ui::Accelerator* accelerator =
            AcceleratorsCocoa::GetInstance()->GetAcceleratorForCommand(tag_)) {
      GetKeyEquivalentAndModifierMaskFromAccelerator(
          *accelerator, &key_equivalent, &key_equivalent_flags);
    }
  }

  NSString* title;
  if (string_arg1_ != 0) {
    base::string16 arg1 = l10n_util::GetStringUTF16(string_arg1_);
    title = l10n_util::GetNSStringFWithFixup(string_id_, arg1);
  } else {
    title = l10n_util::GetNSStringWithFixup(string_id_);
  }

  SEL action = !submenu_.has_value() ? action_ : nil;

  base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
      initWithTitle:title
             action:action
      keyEquivalent:key_equivalent]);
  [item setTarget:target_];
  [item setTag:tag_];
  [item setKeyEquivalentModifierMask:key_equivalent_flags];
  [item setAlternate:is_alternate_];

  if (submenu_.has_value()) {
    base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:title]);
    for (const auto& subitem : submenu_.value()) {
      [menu addItem:subitem.Build()];
    }
    [item setSubmenu:menu];
  }

  return item;
}

}  // namespace internal
}  // namespace chrome

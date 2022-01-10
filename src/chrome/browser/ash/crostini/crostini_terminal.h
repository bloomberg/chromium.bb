// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_H_

#include <vector>

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"

class Profile;

namespace crostini {

// Settings items that can be user-modified for terminal.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TerminalSetting {
  kUnknown = 0,
  kAltGrMode = 1,
  kAltBackspaceIsMetaBackspace = 2,
  kAltIsMeta = 3,
  kAltSendsWhat = 4,
  kAudibleBellSound = 5,
  kDesktopNotificationBell = 6,
  kBackgroundColor = 7,
  kBackgroundImage = 8,
  kBackgroundSize = 9,
  kBackgroundPosition = 10,
  kBackspaceSendsBackspace = 11,
  kCharacterMapOverrides = 12,
  kCloseOnExit = 13,
  kCursorBlink = 14,
  kCursorBlinkCycle = 15,
  kCursorShape = 16,
  kCursorColor = 17,
  kColorPaletteOverrides = 18,
  kCopyOnSelect = 19,
  kUseDefaultWindowCopy = 20,
  kClearSelectionAfterCopy = 21,
  kCtrlPlusMinusZeroZoom = 22,
  kCtrlCCopy = 23,
  kCtrlVPaste = 24,
  kEastAsianAmbiguousAsTwoColumn = 25,
  kEnable8BitControl = 26,
  kEnableBold = 27,
  kEnableBoldAsBright = 28,
  kEnableBlink = 29,
  kEnableClipboardNotice = 30,
  kEnableClipboardWrite = 31,
  kEnableDec12 = 32,
  kEnableCsiJ3 = 33,
  kEnvironment = 34,
  kFontFamily = 35,
  kFontSize = 36,
  kFontSmoothing = 37,
  kForegroundColor = 38,
  kEnableResizeStatus = 39,
  kHideMouseWhileTyping = 40,
  kHomeKeysScroll = 41,
  kKeybindings = 42,
  kMediaKeysAreFkeys = 43,
  kMetaSendsEscape = 44,
  kMouseRightClickPaste = 45,
  kMousePasteButton = 46,
  kWordBreakMatchLeft = 47,
  kWordBreakMatchRight = 48,
  kWordBreakMatchMiddle = 49,
  kPageKeysScroll = 50,
  kPassAltNumber = 51,
  kPassCtrlNumber = 52,
  kPassCtrlN = 53,
  kPassCtrlT = 54,
  kPassCtrlTab = 55,
  kPassCtrlW = 56,
  kPassMetaNumber = 57,
  kPassMetaV = 58,
  kPasteOnDrop = 59,
  kReceiveEncoding = 60,
  kScrollOnKeystroke = 61,
  kScrollOnOutput = 62,
  kScrollbarVisible = 63,
  kScrollWheelMaySendArrowKeys = 64,
  kScrollWheelMoveMultiplier = 65,
  kTerminalEncoding = 66,
  kShiftInsertPaste = 67,
  kUserCss = 68,
  kUserCssText = 69,
  kAllowImagesInline = 70,
  kTheme = 71,
  kThemeVariations = 72,
  kMaxValue = kThemeVariations,
};

// Launches the terminal tabbed app.
void LaunchTerminal(Profile* profile,
                    int64_t display_id = display::kInvalidDisplayId,
                    const ContainerId& container_id = ContainerId::GetDefault(),
                    const std::string& cwd = "",
                    const std::vector<std::string>& terminal_args = {});

void LaunchTerminalForSSH(Profile* profile, int64_t display_id);

void LaunchTerminalWithUrl(Profile* profile,
                           int64_t display_id,
                           const GURL& url);

void LaunchTerminalWithIntent(Profile* profile,
                              int64_t display_id,
                              apps::mojom::IntentPtr intent,
                              CrostiniSuccessCallback callback);

// Launches the terminal settings popup window.
void LaunchTerminalSettings(Profile* profile,
                            int64_t display_id = display::kInvalidDisplayId);

// Record which terminal settings have been changed by users.
void RecordTerminalSettingsChangesUMAs(Profile* profile);

// Returns terminal setting 'background-color'.
std::string GetTerminalSettingBackgroundColor(Profile* profile);

// Returns terminal setting 'pass-ctrl-w'.
bool GetTerminalSettingPassCtrlW(Profile* profile);

// Add terminal menu items (Settings, Shut down Linux).
void AddTerminalMenuItems(Profile* profile,
                          apps::mojom::MenuItemsPtr* menu_items);

// Add terminal shortcut items in menu.
void AddTerminalMenuShortcuts(
    Profile* profile,
    int next_command_id,
    apps::mojom::MenuItemsPtr menu_items,
    apps::mojom::Publisher::GetMenuModelCallback callback,
    std::vector<gfx::ImageSkia> icons = {});

// Called when user clicks on terminal menu items. Returns true if |shortcut_id|
// is recognized and handled.
bool ExecuteTerminalMenuShortcutCommand(Profile* profile,
                                        const std::string& shortcut_id,
                                        int64_t display_id);

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_

#include "base/feature_list.h"

// Feature to choose whether to use OverlayPresenter to show the new Messages
// Infobar design.  In order for it to work, kIOSInfobarUIReboot needs to also
// be enabled. Use IsInfobarOverlayUIEnabled() instead of this constant
// directly.
extern const base::Feature kInfobarOverlayUI;

// Feature to choose whether Confirm Infobars use the new Messages UI or the
// legacy one. Also, in order for it to work kIOSInfobarUIReboot needs to be
// enabled.
// Use IsConfirmInfobarMessagesUIEnabled() instead of this constant directly.
extern const base::Feature kConfirmInfobarMessagesUI;

// Feature to choose whether Confirm Infobars use the new Messages UI or the
// legacy one. Also, in order for it to work kIOSInfobarUIReboot needs to be
// enabled.
// Use IsCrashRestoreInfobarMessagesUIEnabled() instead of this constant
// directly.
extern const base::Feature kCrashRestoreInfobarMessagesUI;

// Feature to choose whether Block Popup Infobars use the new Messages UI or the
// legacy one. Also, in order for it to work kIOSInfobarUIReboot needs to be
// enabled.
// Use IsBlockPopupInfobarMessagesUIEnabled() instead of this constant
// directly.
extern const base::Feature kBlockPopupInfobarMessagesUI;

// Feature to choose whether Downloads uses the new Messages UI or the
// legacy one. Also, in order for it to work kIOSInfobarUIReboot needs to be
// enabled.
// Use IsDownloadInfobarMessagesUIEnabled() instead of this constant directly.
extern const base::Feature kDownloadInfobarMessagesUI;

// Feature to choose whether Save Card Infobar uses the new Messages UI or the
// legacy one. Also, in order for it to work kIOSInfobarUIReboot needs to be
// enabled.
// Use IsSaveCardInfobarMessagesUIEnabled() instead of this constant directly.
extern const base::Feature kSaveCardInfobarMessagesUI;

// Feature to choose whether Translate Infobar uses the new Messages UI or the
// legacy one. In order for it to work, kIOSInfobarUIReboot needs to also be
// enabled.
// Use IsTranslateInfobarMessagesUIEnabled() instead of this constant directly.
extern const base::Feature kTranslateInfobarMessagesUI;

// Feature to choose whether to use the new Messages Infobar design on iOS13, or
// the legacy one.
// Use IsInfobarUIRebootEnabled() instead of this constant directly.
extern const base::Feature kInfobarUIRebootOnlyiOS13;

// Whether the Messages Infobar UI is enabled. Prefer to use this method instead
// of kIOSInfobarUIReboot directly.
bool IsInfobarUIRebootEnabled();

// Whether the Messages Infobar UI is presented using OverlayPresenter.
bool IsInfobarOverlayUIEnabled();

// Whether the Confirm Infobar Messages UI is enabled.
bool IsConfirmInfobarMessagesUIEnabled();

// Whether the Crash Restore Infobar Messages UI is enabled.
bool IsCrashRestoreInfobarMessagesUIEnabled();

// Whether the Block Popup Infobar Messages UI is enabled.
bool IsBlockPopupInfobarMessagesUIEnabled();

// Whether the Download Infobar Messages UI is enabled.
bool IsDownloadInfobarMessagesUIEnabled();

// Whether the SaveCard Infobar Messages UI is enabled.
bool IsSaveCardInfobarMessagesUIEnabled();

// Whether the Translate Infobar Messages UI is enabled.
bool IsTranslateInfobarMessagesUIEnabled();

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_FEATURE_H_

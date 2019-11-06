// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace exclusive_access_bubble {

base::string16 GetLabelTextForType(ExclusiveAccessBubbleType type,
                                   const GURL& url,
                                   extensions::ExtensionRegistry* registry) {
  base::string16 host(base::UTF8ToUTF16(url.host()));
  if (registry) {
    const extensions::Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(url);
    if (extension) {
      host = base::UTF8ToUTF16(extension->name());
    } else if (url.SchemeIs(extensions::kExtensionScheme)) {
      // In this case, |host| is set to an extension ID.
      // We are not going to show it because it's human-unreadable.
      host.clear();
    }
  }
  if (host.empty()) {
    switch (type) {
      case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
      case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(IDS_FULLSCREEN_ENTERED_FULLSCREEN);
      case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(
            IDS_FULLSCREEN_ENTERED_FULLSCREEN_MOUSELOCK);
      case EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(IDS_FULLSCREEN_ENTERED_MOUSELOCK);
      case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(
            IDS_FULLSCREEN_USER_ENTERED_FULLSCREEN);
      case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
        return l10n_util::GetStringUTF16(
            IDS_FULLSCREEN_UNKNOWN_EXTENSION_TRIGGERED_FULLSCREEN);
      case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
        NOTREACHED();
        return base::string16();
    }
    NOTREACHED();
    return base::string16();
  }
  switch (type) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(IDS_FULLSCREEN_SITE_ENTERED_FULLSCREEN,
                                        host);
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(
          IDS_FULLSCREEN_SITE_ENTERED_FULLSCREEN_MOUSELOCK, host);
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(IDS_FULLSCREEN_SITE_ENTERED_MOUSELOCK,
                                        host);
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
      return l10n_util::GetStringUTF16(IDS_FULLSCREEN_USER_ENTERED_FULLSCREEN);
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(
          IDS_FULLSCREEN_EXTENSION_TRIGGERED_FULLSCREEN, host);
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
      NOTREACHED();
      return base::string16();
  }
  NOTREACHED();
  return base::string16();
}

base::string16 GetDenyButtonTextForType(ExclusiveAccessBubbleType type) {
  switch (type) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
      NOTREACHED();  // No button in this case.
      return base::string16();
  }
  NOTREACHED();
  return base::string16();
}

base::string16 GetAllowButtonTextForType(ExclusiveAccessBubbleType type,
                                         const GURL& url) {
  switch (type) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
      NOTREACHED();  // No button in this case.
      return base::string16();
  }
  NOTREACHED();
  return base::string16();
}

base::string16 GetInstructionTextForType(ExclusiveAccessBubbleType type,
                                         const base::string16& accelerator) {
  switch (type) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
      // Both fullscreen and fullscreen + mouselock have the same message (the
      // user does not care about mouse lock when in fullscreen mode). All ways
      // to trigger fullscreen result in the same message.
      return l10n_util::GetStringFUTF16(
          IDS_FULLSCREEN_PRESS_ESC_TO_EXIT_FULLSCREEN, accelerator);
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(
          IDS_FULLSCREEN_HOLD_ESC_TO_EXIT_FULLSCREEN, accelerator);
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
      return l10n_util::GetStringFUTF16(
          IDS_FULLSCREEN_PRESS_ESC_TO_EXIT_MOUSELOCK, accelerator);
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
      NOTREACHED();
      return base::string16();
  }
  NOTREACHED();
  return base::string16();
}

}  // namespace exclusive_access_bubble

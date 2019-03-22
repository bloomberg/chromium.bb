// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_extension_util_win.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

void GetExtensionNamesFromIds(Profile* profile,
                              const std::set<base::string16>& extension_ids,
                              std::set<base::string16>* extension_names) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  for (const base::string16& extension_id : extension_ids) {
    const extensions::Extension* extension =
        extension_registry->GetInstalledExtension(
            base::UTF16ToUTF8(extension_id));
    if (extension) {
      extension_names->insert(base::UTF8ToUTF16(extension->name()));
    } else {
      extension_names->insert(l10n_util::GetStringFUTF16(
          IDS_SETTINGS_RESET_CLEANUP_DETAILS_EXTENSION_UNKNOWN, extension_id));
    }
  }
}

}  // namespace safe_browsing

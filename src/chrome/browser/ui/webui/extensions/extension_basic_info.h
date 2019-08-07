// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_BASIC_INFO_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_BASIC_INFO_H_

namespace base {
class DictionaryValue;
}

namespace extensions {

class Extension;

// Fills the |info| dictionary with basic information about the extension.
// |enabled| is injected for easier testing.
void GetExtensionBasicInfo(const Extension* extension,
                           bool enabled,
                           base::DictionaryValue* info);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_BASIC_INFO_H_

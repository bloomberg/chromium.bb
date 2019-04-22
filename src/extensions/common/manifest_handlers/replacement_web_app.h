// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_WEB_APP_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_WEB_APP_H_

#include "base/strings/string16.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

class GURL;

namespace extensions {

// A structure to hold the replacement web app that may be specified in the
// manifest of an extension using the "replacement_web_app" key.
struct ReplacementWebAppInfo : public Extension::ManifestData {
  ReplacementWebAppInfo();
  ~ReplacementWebAppInfo() override;

  // Returns true if the specified URL has a replacement web app.
  static bool HasReplacementWebApp(const Extension* extension);

  // Returns the replacement web app for |extension|.
  static GURL GetReplacementWebApp(const Extension* extension);

  GURL replacement_web_app;
};

// Parses the "related_web_apps" manifest key.
class ReplacementWebAppHandler : public ManifestHandler {
 public:
  ReplacementWebAppHandler();
  ~ReplacementWebAppHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(ReplacementWebAppHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_REPLACEMENT_WEB_APP_H_

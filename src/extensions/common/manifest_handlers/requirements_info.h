// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_REQUIREMENTS_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_REQUIREMENTS_INFO_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// Declared requirements for the extension.
struct RequirementsInfo : public Extension::ManifestData {
  explicit RequirementsInfo(const Manifest* manifest);
  ~RequirementsInfo() override;

  bool webgl;
  bool window_shape;

  static const RequirementsInfo& GetRequirements(const Extension* extension);
};

// Parses the "requirements" manifest key.
class RequirementsHandler : public ManifestHandler {
 public:
  RequirementsHandler();
  ~RequirementsHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

  bool AlwaysParseForType(Manifest::Type type) const override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(RequirementsHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_REQUIREMENTS_INFO_H_

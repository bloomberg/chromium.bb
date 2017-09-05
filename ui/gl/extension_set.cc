// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/extension_set.h"
#include "base/strings/string_split.h"

namespace gl {

ExtensionSet MakeExtensionSet(const base::StringPiece& extensions_string) {
  return ExtensionSet(SplitStringPiece(
      extensions_string, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
}

bool HasExtension(const ExtensionSet& extension_set,
                  const base::StringPiece& extension) {
  return extension_set.find(extension) != extension_set.end();
}

}  // namespace gl

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_PARSER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_PARSER_H_

#include "base/strings/string_piece.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

struct Manifest;
struct ManifestError;
class WebURL;

class WebManifestParser {
 public:
  // Updates only Manifest if parsing did not fail.
  BLINK_EXPORT static bool ParseManifest(const base::StringPiece&,
                                         const WebURL&,
                                         const WebURL&,
                                         Manifest*,
                                         WebVector<ManifestError>*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_PARSER_H_

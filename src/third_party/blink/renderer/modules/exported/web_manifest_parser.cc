// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_manifest_parser.h"

#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

bool WebManifestParser::ParseManifest(const base::StringPiece& data,
                                      const WebURL& manifest_url,
                                      const WebURL& document_url,
                                      Manifest* manifest,
                                      WebVector<ManifestError>* errors) {
  ManifestParser parser(data, KURL(manifest_url), KURL(document_url));

  parser.Parse();
  parser.TakeErrors(errors);

  if (parser.failed())
    return false;

  *manifest = parser.manifest();
  return true;
}

}  // namespace blink

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/resource_utils.h"

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {

std::string GetResourceFromIdAsString(int resource_id) {
  base::StringPiece raw_resource =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  if (!ui::ResourceBundle::GetSharedInstance().IsGzipped(resource_id))
    return raw_resource.as_string();

  std::string uncompressed_resource;
  bool success =
      compression::GzipUncompress(raw_resource, &uncompressed_resource);
  DCHECK(success);
  return uncompressed_resource;
}

}  // namespace dom_distiller

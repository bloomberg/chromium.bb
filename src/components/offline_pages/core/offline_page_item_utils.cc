// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_item_utils.h"
#include "url/gurl.h"

namespace offline_pages {

bool EqualsIgnoringFragment(const GURL& lhs, const GURL& rhs) {
  GURL::Replacements remove_params;
  remove_params.ClearRef();

  GURL lhs_stripped = lhs.ReplaceComponents(remove_params);
  GURL rhs_stripped = rhs.ReplaceComponents(remove_params);

  return lhs_stripped == rhs_stripped;
}

}  // namespace offline_pages

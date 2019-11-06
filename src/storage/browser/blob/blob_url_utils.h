// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/gurl.h"

namespace storage {

namespace BlobUrlUtils {
// We can't use GURL directly for these hash fragment manipulations
// since it doesn't have specific knowlege of the BlobURL format. GURL
// treats BlobURLs as if they were PathURLs which don't support hash
// fragments.

// Returns true iff |url| contains a hash fragment.
bool UrlHasFragment(const GURL& url);

// Returns |url| with any potential hash fragment stripped. If |url| didn't
// contain such a fragment this returns |url| unchanged.
GURL ClearUrlFragment(const GURL& url);

}  // namespace BlobUrlUtils
}  // namespace storage

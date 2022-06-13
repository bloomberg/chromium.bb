// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHOOSER_CONTROLLER_TITLE_UTIL_H_
#define CHROME_BROWSER_CHOOSER_CONTROLLER_TITLE_UTIL_H_

#include <string>

namespace content {
class RenderFrameHost;
}

// Creates a title for a chooser. For extensions the extension name is used if
// possible. In all other cases the origin is used.
std::u16string CreateExtensionAwareChooserTitle(
    content::RenderFrameHost* render_frame_host,
    int title_string_id_origin,
    int title_string_id_extension);

#endif  // CHROME_BROWSER_CHOOSER_CONTROLLER_TITLE_UTIL_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBKIT_GLUE_H_
#define WEBKIT_GLUE_WEBKIT_GLUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/platform_file.h"
#include "base/strings/string16.h"
#include "third_party/WebKit/public/platform/WebCanvas.h"
#include "webkit/glue/webkit_glue_export.h"

class SkCanvas;

namespace WebKit {
struct WebFileInfo;
}

namespace webkit_glue {

WEBKIT_GLUE_EXPORT void SetJavaScriptFlags(const std::string& flags);

// Turn on logging for flags in the provided comma delimited list.
WEBKIT_GLUE_EXPORT void EnableWebCoreLogChannels(const std::string& channels);

#ifndef NDEBUG
// Checks various important objects to see if there are any in memory, and
// calls AppendToLog with any leaked objects. Designed to be called on
// shutdown.
WEBKIT_GLUE_EXPORT void CheckForLeaks();
#endif

// File info conversion
WEBKIT_GLUE_EXPORT void PlatformFileInfoToWebFileInfo(
    const base::PlatformFileInfo& file_info,
    WebKit::WebFileInfo* web_file_info);

// Returns a WebCanvas pointer associated with the given Skia canvas.
WEBKIT_GLUE_EXPORT WebKit::WebCanvas* ToWebCanvas(SkCanvas*);

// Returns the number of currently-active glyph pages this process is using.
// There can be many such pages (maps of 256 character -> glyph) so this is
// used to get memory usage statistics.
WEBKIT_GLUE_EXPORT int GetGlyphPageCount();

// Returns an estimate of the memory usage of the renderer process. Different
// platforms implement this function differently, and count in different
// allocations. Results are not comparable across platforms. The estimate is
// computed inside the sandbox and thus its not always accurate.
WEBKIT_GLUE_EXPORT size_t MemoryUsageKB();

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBKIT_GLUE_H_

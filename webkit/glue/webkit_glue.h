// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBKIT_GLUE_H_
#define WEBKIT_GLUE_WEBKIT_GLUE_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>
#include <vector>

#include "base/platform_file.h"
#include "base/string16.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCanvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"
#include "webkit/glue/webkit_glue_export.h"

class SkBitmap;
class SkCanvas;

namespace net {
class URLRequest;
}

namespace WebKit {
struct WebFileInfo;
class WebFrame;
}

namespace webkit_glue {

WEBKIT_GLUE_EXPORT void SetJavaScriptFlags(const std::string& flags);

// Turn on logging for flags in the provided comma delimited list.
WEBKIT_GLUE_EXPORT void EnableWebCoreLogChannels(const std::string& channels);

// Returns the text of the document element.
WEBKIT_GLUE_EXPORT base::string16 DumpDocumentText(WebKit::WebFrame* web_frame);

// Returns the text of the document element and optionally its child frames.
// If recursive is false, this is equivalent to DumpDocumentText followed by
// a newline.  If recursive is true, it recursively dumps all frames as text.
base::string16 DumpFramesAsText(WebKit::WebFrame* web_frame, bool recursive);

// Returns the renderer's description of its tree (its externalRepresentation).
WEBKIT_GLUE_EXPORT base::string16 DumpRenderer(WebKit::WebFrame* web_frame);

// Returns the number of page where the specified element will be put.
int PageNumberForElementById(WebKit::WebFrame* web_frame,
                             const std::string& id,
                             float page_width_in_pixels,
                             float page_height_in_pixels);

// Returns the number of pages to be printed.
int NumberOfPages(WebKit::WebFrame* web_frame,
                  float page_width_in_pixels,
                  float page_height_in_pixels);

// Returns a dump of the scroll position of the webframe.
base::string16 DumpFrameScrollPosition(WebKit::WebFrame* web_frame,
                                       bool recursive);

// Returns a dump of the given history state suitable for implementing the
// dumpBackForwardList command of the testRunner.
WEBKIT_GLUE_EXPORT base::string16 DumpHistoryState(
    const std::string& history_state,
    int indent,
    bool is_current);

#ifndef NDEBUG
// Checks various important objects to see if there are any in memory, and
// calls AppendToLog with any leaked objects. Designed to be called on
// shutdown.
WEBKIT_GLUE_EXPORT void CheckForLeaks();
#endif

// Decodes the image from the data in |image_data| into |image|.
// Returns false if the image could not be decoded.
WEBKIT_GLUE_EXPORT bool DecodeImage(const std::string& image_data,
                                    SkBitmap* image);

// Tells the plugin thread to terminate the process forcefully instead of
// exiting cleanly.
void SetForcefullyTerminatePluginProcess(bool value);

// Returns true if the plugin thread should terminate the process forcefully
// instead of exiting cleanly.
WEBKIT_GLUE_EXPORT bool ShouldForcefullyTerminatePluginProcess();

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

// Returns WebKit Web Inspector protocol version.
std::string GetInspectorProtocolVersion();

// Tells caller whether the given protocol version is supported by the.
WEBKIT_GLUE_EXPORT bool IsInspectorProtocolVersionSupported(
    const std::string& version);

// Configures the URLRequest according to the referrer policy.
WEBKIT_GLUE_EXPORT void ConfigureURLRequestForReferrerPolicy(
    net::URLRequest* request, WebKit::WebReferrerPolicy referrer_policy);

// Returns an estimate of the memory usage of the renderer process. Different
// platforms implement this function differently, and count in different
// allocations. Results are not comparable across platforms. The estimate is
// computed inside the sandbox and thus its not always accurate.
WEBKIT_GLUE_EXPORT size_t MemoryUsageKB();

// Converts from zoom factor (zoom percent / 100) to zoom level, where 0 means
// no zoom, positive numbers mean zoom in, negatives mean zoom out.
WEBKIT_GLUE_EXPORT double ZoomFactorToZoomLevel(double factor);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBKIT_GLUE_H_

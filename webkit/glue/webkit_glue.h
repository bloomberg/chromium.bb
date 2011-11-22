// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCanvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileError.h"

class GURL;
class SkBitmap;

namespace base {
class StringPiece;
}

namespace skia {
class PlatformCanvas;
}

namespace WebKit {
class WebFrame;
class WebString;
}

namespace webkit {
struct WebPluginInfo;
}

namespace webkit_glue {


//---- BEGIN FUNCTIONS IMPLEMENTED BY WEBKIT/GLUE -----------------------------

void SetJavaScriptFlags(const std::string& flags);

// Turn on logging for flags in the provided comma delimited list.
void EnableWebCoreLogChannels(const std::string& channels);

// Returns the text of the document element.
string16 DumpDocumentText(WebKit::WebFrame* web_frame);

// Returns the text of the document element and optionally its child frames.
// If recursive is false, this is equivalent to DumpDocumentText followed by
// a newline.  If recursive is true, it recursively dumps all frames as text.
string16 DumpFramesAsText(WebKit::WebFrame* web_frame, bool recursive);

// Returns the renderer's description of its tree (its externalRepresentation).
string16 DumpRenderer(WebKit::WebFrame* web_frame);

// Fill the value of counter in the element specified by the id into
// counter_value.  Return false when the specified id doesn't exist.
bool CounterValueForElementById(WebKit::WebFrame* web_frame,
                                const std::string& id,
                                string16* counter_value);

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
string16 DumpFrameScrollPosition(WebKit::WebFrame* web_frame, bool recursive);

// Returns a dump of the given history state suitable for implementing the
// dumpBackForwardList command of the layoutTestController.
string16 DumpHistoryState(const std::string& history_state, int indent,
                          bool is_current);

// Sets the user agent.  Pass true for overriding if this is a custom
// user agent instead of the default one (in order to turn off any browser
// sniffing workarounds). This must be called before GetUserAgent() can
// be called.
void SetUserAgent(const std::string& user_agent, bool overriding);

// Returns the user agent to use for the given URL. SetUserAgent() must
// be called prior to calling this function.
const std::string& GetUserAgent(const GURL& url);

// Creates serialized state for the specified URL. This is a variant of
// HistoryItemToString (in glue_serialize) that is used during session restore
// if the saved state is empty.
std::string CreateHistoryStateForURL(const GURL& url);

// Removes any form data state from the history state string |content_state|.
std::string RemoveFormDataFromHistoryState(const std::string& content_state);

// Removes scroll offset from the history state string |content_state|.
std::string RemoveScrollOffsetFromHistoryState(
    const std::string& content_state);

#ifndef NDEBUG
// Checks various important objects to see if there are any in memory, and
// calls AppendToLog with any leaked objects. Designed to be called on
// shutdown.
void CheckForLeaks();
#endif

// Decodes the image from the data in |image_data| into |image|.
// Returns false if the image could not be decoded.
bool DecodeImage(const std::string& image_data, SkBitmap* image);

// Tells the plugin thread to terminate the process forcefully instead of
// exiting cleanly.
void SetForcefullyTerminatePluginProcess(bool value);

// Returns true if the plugin thread should terminate the process forcefully
// instead of exiting cleanly.
bool ShouldForcefullyTerminatePluginProcess();

// File path string conversions.
FilePath::StringType WebStringToFilePathString(const WebKit::WebString& str);
WebKit::WebString FilePathStringToWebString(const FilePath::StringType& str);
FilePath WebStringToFilePath(const WebKit::WebString& str);
WebKit::WebString FilePathToWebString(const FilePath& file_path);

// File error conversion
WebKit::WebFileError PlatformFileErrorToWebFileError(
    base::PlatformFileError error_code);

// Returns a WebCanvas pointer associated with the given Skia canvas.
WebKit::WebCanvas* ToWebCanvas(skia::PlatformCanvas*);

// Returns the number of currently-active glyph pages this process is using.
// There can be many such pages (maps of 256 character -> glyph) so this is
// used to get memory usage statistics.
int GetGlyphPageCount();

// Returns WebKit Web Inspector protocol version.
std::string GetInspectorProtocolVersion();

// Tells caller whether the given protocol version is supported by the.
bool IsInspectorProtocolVersionSupported(const std::string& version);

//---- END FUNCTIONS IMPLEMENTED BY WEBKIT/GLUE -------------------------------


//---- BEGIN FUNCTIONS IMPLEMENTED BY EMBEDDER --------------------------------

// Glue to get resources from the embedder.

// Gets a localized string given a message id.  Returns an empty string if the
// message id is not found.
string16 GetLocalizedString(int message_id);

// Returns the raw data for a resource.  This resource must have been
// specified as BINDATA in the relevant .rc file.
base::StringPiece GetDataResource(int resource_id);

// Embedders implement this function to return the list of plugins to Webkit.
void GetPlugins(bool refresh,
                std::vector<webkit::WebPluginInfo>* plugins);

// ---- END FUNCTIONS IMPLEMENTED BY EMBEDDER ---------------------------------


}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBKIT_GLUE_H_

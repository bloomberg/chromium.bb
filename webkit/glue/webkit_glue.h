// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "ui/base/clipboard/clipboard.h"

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
class WebView;
}

namespace webkit {
namespace npapi {
struct WebPluginInfo;
}
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

// Returns the WebKit version (major.minor).
std::string GetWebKitVersion();

// Called to override the default user agent with a custom one.  Call this
// before anyone actually asks for the user agent in order to prevent
// inconsistent behavior.
void SetUserAgent(const std::string& new_user_agent);

// Returns the user agent to use for the given URL, which is usually the
// default user agent but may be overriden by a call to SetUserAgent() (which
// should be done at startup).
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
// calls AppendToLog with any leaked objects. Designed to be called on shutdown
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

//---- END FUNCTIONS IMPLEMENTED BY WEBKIT/GLUE -------------------------------


//---- BEGIN FUNCTIONS IMPLEMENTED BY EMBEDDER --------------------------------

// This function is called to request a prefetch of the entire URL, loading it
// into our cache for (expected) future needs.  The given URL may NOT be in
// canonical form and it will NOT be null-terminated; use the length instead.
void PrecacheUrl(const char16* url, int url_length);

// This function is called to add a line to the application's log file.
void AppendToLog(const char* filename, int line, const char* message);

// Glue to get resources from the embedder.

// Gets a localized string given a message id.  Returns an empty string if the
// message id is not found.
string16 GetLocalizedString(int message_id);

// Returns the raw data for a resource.  This resource must have been
// specified as BINDATA in the relevant .rc file.
base::StringPiece GetDataResource(int resource_id);

#if defined(OS_WIN)
// Loads and returns a cursor.
HCURSOR LoadCursor(int cursor_id);
#endif

// Glue to access the clipboard.

// Get a clipboard that can be used to construct a ScopedClipboardWriterGlue.
ui::Clipboard* ClipboardGetClipboard();

// Tests whether the clipboard contains a certain format
bool ClipboardIsFormatAvailable(const ui::Clipboard::FormatType& format,
                                ui::Clipboard::Buffer buffer);

// Reads UNICODE text from the clipboard, if available.
void ClipboardReadText(ui::Clipboard::Buffer buffer, string16* result);

// Reads ASCII text from the clipboard, if available.
void ClipboardReadAsciiText(ui::Clipboard::Buffer buffer, std::string* result);

// Reads HTML from the clipboard, if available.
void ClipboardReadHTML(ui::Clipboard::Buffer buffer, string16* markup,
                       GURL* url);

// Reads the available types from the clipboard, if available.
bool ClipboardReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                 std::vector<string16>* types,
                                 bool* contains_filenames);

// Reads one type of data from the clipboard, if available.
bool ClipboardReadData(ui::Clipboard::Buffer buffer, const string16& type,
                       string16* data, string16* metadata);

// Reads filenames from the clipboard, if available.
bool ClipboardReadFilenames(ui::Clipboard::Buffer buffer,
                            std::vector<string16>* filenames);

// Gets the directory where the application data and libraries exist.  This
// may be a versioned subdirectory, or it may be the same directory as the
// GetExeDirectory(), depending on the embedder's implementation.
// Path is an output parameter to receive the path.
// Returns true if successful, false otherwise.
bool GetApplicationDirectory(FilePath* path);

// Gets the directory where the launching executable resides on disk.
// Path is an output parameter to receive the path.
// Returns true if successful, false otherwise.
bool GetExeDirectory(FilePath* path);

// Embedders implement this function to return the list of plugins to Webkit.
void GetPlugins(bool refresh,
                std::vector<webkit::npapi::WebPluginInfo>* plugins);

// Returns true if the plugins run in the same process as the renderer, and
// false otherwise.
bool IsPluginRunningInRendererProcess();

// Returns a bool indicating if the Null plugin should be enabled or not.
bool IsDefaultPluginEnabled();

// Returns true if the protocol implemented to serve |url| supports features
// required by the media engine.
bool IsProtocolSupportedForMedia(const GURL& url);

#if defined(OS_WIN)
// Downloads the file specified by the URL. On sucess a WM_COPYDATA message
// will be sent to the caller_window.
bool DownloadUrl(const std::string& url, HWND caller_window);
#endif

// Returns the plugin finder URL.
bool GetPluginFinderURL(std::string* plugin_finder_url);

// Resolves the proxies for the url, returns true on success.
bool FindProxyForUrl(const GURL& url, std::string* proxy_list);

// Returns the locale that this instance of webkit is running as.  This is of
// the form language-country (e.g., en-US or pt-BR).
std::string GetWebKitLocale();

// Close current connections.  Used for debugging.
void CloseCurrentConnections();

// Enable or disable the disk cache.  Used for debugging.
void SetCacheMode(bool enabled);

// Clear the disk cache.  Used for debugging.
// |preserve_ssl_host_info| indicates whether disk cache entries related to
// SSL information should be purged.
void ClearCache(bool preserve_ssl_host_info);

// Returns the product version.  E.g., Chrome/4.1.333.0
std::string GetProductVersion();

// Returns true if the embedder is running in single process mode.
bool IsSingleProcess();

// Enables/Disables Spdy for requests afterwards. Used for benchmarking.
void EnableSpdy(bool enable);

// Notifies the browser that the given action has been performed.
void UserMetricsRecordAction(const std::string& action);

#if !defined(DISABLE_NACL)
// Launch NaCl's sel_ldr process.
bool LaunchSelLdr(const char* alleged_url, int socket_count, void* imc_handles,
                  void* nacl_process_handle, int* nacl_process_id);
#endif

#if defined(OS_LINUX)
// Return a read-only file descriptor to the font which best matches the given
// properties or -1 on failure.
//   charset: specifies the language(s) that the font must cover. See
// render_sandbox_host_linux.cc for more information.
int MatchFontWithFallback(const std::string& face, bool bold,
                          bool italic, int charset);

// GetFontTable loads a specified font table from an open SFNT file.
//   fd: a file descriptor to the SFNT file. The position doesn't matter.
//   table: the table in *big-endian* format, or 0 for the whole font file.
//   output: a buffer of size output_length that gets the data.  can be 0, in
//     which case output_length will be set to the required size in bytes.
//   output_length: size of output, if it's not 0.
//
//   returns: true on success.
bool GetFontTable(int fd, uint32_t table, uint8_t* output,
                  size_t* output_length);
#endif

// ---- END FUNCTIONS IMPLEMENTED BY EMBEDDER ---------------------------------


} // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBKIT_GLUE_H_

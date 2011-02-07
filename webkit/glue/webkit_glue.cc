// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include <objidl.h>
#include <mlang.h>
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "skia/ext/platform_canvas.h"
#if defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGlyphCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#if defined(OS_WIN)
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"
#endif
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/user_agent.h"
#include "v8/include/v8.h"

using WebKit::WebCanvas;
using WebKit::WebData;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebGlyphCache;
using WebKit::WebHistoryItem;
using WebKit::WebImage;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebVector;
using WebKit::WebView;

static const char kLayoutTestsPattern[] = "/LayoutTests/";
static const std::string::size_type kLayoutTestsPatternSize =
    arraysize(kLayoutTestsPattern) - 1;
static const char kFileUrlPattern[] = "file:/";
static const char kDataUrlPattern[] = "data:";
static const std::string::size_type kDataUrlPatternSize =
    arraysize(kDataUrlPattern) - 1;
static const char kFileTestPrefix[] = "(file test):";

//------------------------------------------------------------------------------
// webkit_glue impl:

namespace webkit_glue {

// Global variable used by the plugin quirk "die after unload".
bool g_forcefully_terminate_plugin_process = false;

void SetJavaScriptFlags(const std::string& str) {
#if WEBKIT_USING_V8
  v8::V8::SetFlagsFromString(str.data(), static_cast<int>(str.size()));
#endif
}

void EnableWebCoreNotImplementedLogging() {
  WebKit::enableLogChannel("NotYetImplemented");
}

string16 DumpDocumentText(WebFrame* web_frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  WebElement document_element = web_frame->document().documentElement();
  if (document_element.isNull())
    return string16();

  return document_element.innerText();
}

string16 DumpFramesAsText(WebFrame* web_frame, bool recursive) {
  string16 result;

  // Add header for all but the main frame. Skip empty frames.
  if (web_frame->parent() &&
      !web_frame->document().documentElement().isNull()) {
    result.append(ASCIIToUTF16("\n--------\nFrame: '"));
    result.append(web_frame->name());
    result.append(ASCIIToUTF16("'\n--------\n"));
  }

  result.append(DumpDocumentText(web_frame));
  result.append(ASCIIToUTF16("\n"));

  if (recursive) {
    WebFrame* child = web_frame->firstChild();
    for (; child; child = child->nextSibling())
      result.append(DumpFramesAsText(child, recursive));
  }

  return result;
}

string16 DumpRenderer(WebFrame* web_frame) {
  return web_frame->renderTreeAsText();
}

bool CounterValueForElementById(WebFrame* web_frame, const std::string& id,
                                string16* counter_value) {
  WebString result =
      web_frame->counterValueForElementById(WebString::fromUTF8(id));
  if (result.isNull())
    return false;

  *counter_value = result;
  return true;
}

int PageNumberForElementById(WebFrame* web_frame,
                             const std::string& id,
                             float page_width_in_pixels,
                             float page_height_in_pixels) {
  return web_frame->pageNumberForElementById(WebString::fromUTF8(id),
                                             page_width_in_pixels,
                                             page_height_in_pixels);
}

int NumberOfPages(WebFrame* web_frame,
                  float page_width_in_pixels,
                  float page_height_in_pixels) {
  WebSize size(static_cast<int>(page_width_in_pixels),
               static_cast<int>(page_height_in_pixels));
  int number_of_pages = web_frame->printBegin(size);
  web_frame->printEnd();
  return number_of_pages;
}

string16 DumpFrameScrollPosition(WebFrame* web_frame, bool recursive) {
  gfx::Size offset = web_frame->scrollOffset();
  std::string result_utf8;

  if (offset.width() > 0 || offset.height() > 0) {
    if (web_frame->parent()) {
      base::StringAppendF(&result_utf8, "frame '%s' ",
                          UTF16ToUTF8(web_frame->name()).c_str());
    }
    base::StringAppendF(&result_utf8, "scrolled to %d,%d\n",
                        offset.width(), offset.height());
  }

  string16 result = UTF8ToUTF16(result_utf8);

  if (recursive) {
    WebFrame* child = web_frame->firstChild();
    for (; child; child = child->nextSibling())
      result.append(DumpFrameScrollPosition(child, recursive));
  }

  return result;
}

// Returns True if item1 < item2.
static bool HistoryItemCompareLess(const WebHistoryItem& item1,
                                   const WebHistoryItem& item2) {
  string16 target1 = item1.target();
  string16 target2 = item2.target();
  std::transform(target1.begin(), target1.end(), target1.begin(), tolower);
  std::transform(target2.begin(), target2.end(), target2.begin(), tolower);
  return target1 < target2;
}

// Writes out a HistoryItem into a UTF-8 string in a readable format.
static std::string DumpHistoryItem(const WebHistoryItem& item,
                                   int indent, bool is_current) {
  std::string result;

  if (is_current) {
    result.append("curr->");
    result.append(indent - 6, ' ');  // 6 == "curr->".length()
  } else {
    result.append(indent, ' ');
  }

  std::string url = item.urlString().utf8();
  size_t pos;
  if (url.find(kFileUrlPattern) == 0 &&
      ((pos = url.find(kLayoutTestsPattern)) != std::string::npos)) {
    // adjust file URLs to match upstream results.
    url.replace(0, pos + kLayoutTestsPatternSize, kFileTestPrefix);
  } else if (url.find(kDataUrlPattern) == 0) {
    // URL-escape data URLs to match results upstream.
    std::string path = EscapePath(url.substr(kDataUrlPatternSize));
    url.replace(kDataUrlPatternSize, url.length(), path);
  }

  result.append(url);
  if (!item.target().isEmpty())
    result.append(" (in frame \"" + UTF16ToUTF8(item.target()) + "\")");
  if (item.isTargetItem())
    result.append("  **nav target**");
  result.append("\n");

  const WebVector<WebHistoryItem>& children = item.children();
  if (!children.isEmpty()) {
    // Must sort to eliminate arbitrary result ordering which defeats
    // reproducible testing.
    // TODO(darin): WebVector should probably just be a std::vector!!
    std::vector<WebHistoryItem> sorted_children;
    for (size_t i = 0; i < children.size(); ++i)
      sorted_children.push_back(children[i]);
    std::sort(sorted_children.begin(), sorted_children.end(),
              HistoryItemCompareLess);
    for (size_t i = 0; i < sorted_children.size(); i++)
      result += DumpHistoryItem(sorted_children[i], indent+4, false);
  }

  return result;
}

string16 DumpHistoryState(const std::string& history_state, int indent,
                          bool is_current) {
  return UTF8ToUTF16(
      DumpHistoryItem(HistoryItemFromString(history_state), indent,
                      is_current));
}

#ifndef NDEBUG
// The log macro was having problems due to collisions with WTF, so we just
// code here what that would have inlined.
void DumpLeakedObject(const char* file, int line, const char* object,
                      int count) {
  std::string msg = base::StringPrintf("%s LEAKED %d TIMES", object, count);
  AppendToLog(file, line, msg.c_str());
}
#endif

void CheckForLeaks() {
#ifndef NDEBUG
  int count = WebFrame::instanceCount();
  if (count)
    DumpLeakedObject(__FILE__, __LINE__, "WebFrame", count);
#endif
}

bool DecodeImage(const std::string& image_data, SkBitmap* image) {
  WebData web_data(image_data.data(), image_data.length());
  WebImage web_image(WebImage::fromData(web_data, WebSize()));
  if (web_image.isNull())
    return false;

#if defined(OS_MACOSX)
  *image = gfx::CGImageToSkBitmap(web_image.getCGImageRef());
#else
  *image = web_image.getSkBitmap();
#endif
  return true;
}

// NOTE: This pair of conversion functions are here instead of in glue_util.cc
// since that file will eventually die.  This pair of functions will need to
// remain as the concept of a file-path specific character encoding string type
// will most likely not make its way into WebKit.

FilePath::StringType WebStringToFilePathString(const WebString& str) {
#if defined(OS_POSIX)
  return base::SysWideToNativeMB(UTF16ToWideHack(str));
#elif defined(OS_WIN)
  return UTF16ToWideHack(str);
#endif
}

WebString FilePathStringToWebString(const FilePath::StringType& str) {
#if defined(OS_POSIX)
  return WideToUTF16Hack(base::SysNativeMBToWide(str));
#elif defined(OS_WIN)
  return WideToUTF16Hack(str);
#endif
}

FilePath WebStringToFilePath(const WebString& str) {
  return FilePath(WebStringToFilePathString(str));
}

WebString FilePathToWebString(const FilePath& file_path) {
  return FilePathStringToWebString(file_path.value());
}

WebKit::WebFileError PlatformFileErrorToWebFileError(
    base::PlatformFileError error_code) {
  switch (error_code) {
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return WebKit::WebFileErrorNotFound;
    case base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
    case base::PLATFORM_FILE_ERROR_EXISTS:
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
    case base::PLATFORM_FILE_ERROR_NOT_A_FILE:
    case base::PLATFORM_FILE_ERROR_NOT_EMPTY:
      return WebKit::WebFileErrorInvalidModification;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return WebKit::WebFileErrorNoModificationAllowed;
    case base::PLATFORM_FILE_ERROR_FAILED:
      return WebKit::WebFileErrorInvalidState;
    case base::PLATFORM_FILE_ERROR_ABORT:
      return WebKit::WebFileErrorAbort;
    case base::PLATFORM_FILE_ERROR_SECURITY:
      return WebKit::WebFileErrorSecurity;
    case base::PLATFORM_FILE_ERROR_NO_SPACE:
      return WebKit::WebFileErrorQuotaExceeded;
    default:
      return WebKit::WebFileErrorInvalidModification;
  }
}

namespace {

struct UserAgentState {
  UserAgentState()
      : user_agent_requested(false),
        user_agent_is_overridden(false) {
  }

  std::string user_agent;

  // The UA string when we're pretending to be Windows Chrome.
  std::string mimic_windows_user_agent;

  bool user_agent_requested;
  bool user_agent_is_overridden;
};

static base::LazyInstance<UserAgentState> g_user_agent(
    base::LINKER_INITIALIZED);

void SetUserAgentToDefault() {
  BuildUserAgent(false, &g_user_agent.Get().user_agent);
}

}  // namespace

void SetUserAgent(const std::string& new_user_agent) {
  // If you combine this with the previous line, the function only works the
  // first time.
  DCHECK(!g_user_agent.Get().user_agent_requested) <<
      "Setting the user agent after someone has "
      "already requested it can result in unexpected behavior.";
  g_user_agent.Get().user_agent_is_overridden = true;
  g_user_agent.Get().user_agent = new_user_agent;
}

const std::string& GetUserAgent(const GURL& url) {
  if (g_user_agent.Get().user_agent.empty())
    SetUserAgentToDefault();
  g_user_agent.Get().user_agent_requested = true;
  if (!g_user_agent.Get().user_agent_is_overridden) {
    // Workarounds for sites that use misguided UA sniffing.
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    if (MatchPattern(url.host(), "*.mail.yahoo.com")) {
      // mail.yahoo.com is ok with Windows Chrome but not Linux Chrome.
      // http://bugs.chromium.org/11136
      // TODO(evanm): remove this if Yahoo fixes their sniffing.
      if (g_user_agent.Get().mimic_windows_user_agent.empty())
        BuildUserAgent(true, &g_user_agent.Get().mimic_windows_user_agent);
      return g_user_agent.Get().mimic_windows_user_agent;
    }
#endif
  }
  return g_user_agent.Get().user_agent;
}

void SetForcefullyTerminatePluginProcess(bool value) {
  if (IsPluginRunningInRendererProcess()) {
    // Ignore this quirk when the plugins are not running in their own process.
    return;
  }

  g_forcefully_terminate_plugin_process = value;
}

bool ShouldForcefullyTerminatePluginProcess() {
  return g_forcefully_terminate_plugin_process;
}

WebCanvas* ToWebCanvas(skia::PlatformCanvas* canvas) {
#if WEBKIT_USING_SKIA
  return canvas;
#elif WEBKIT_USING_CG
  return canvas->getTopPlatformDevice().GetBitmapContext();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

int GetGlyphPageCount() {
  return WebGlyphCache::pageCount();
}

} // namespace webkit_glue

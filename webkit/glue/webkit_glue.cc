// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "skia/ext/platform_canvas.h"
#if defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGlyphCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#if defined(OS_WIN)
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"
#endif
#include "v8/include/v8.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/user_agent.h"

using WebKit::WebCanvas;
using WebKit::WebData;
using WebKit::WebDevToolsAgent;
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

void EnableWebCoreLogChannels(const std::string& channels) {
  if (channels.empty())
    return;
  StringTokenizer t(channels, ", ");
  while (t.GetNext()) {
    WebKit::enableLogChannel(t.token().c_str());
  }
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
    std::string path = net::EscapePath(url.substr(kDataUrlPatternSize));
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
  logging::LogMessage(file, line).stream() << msg;
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

#if defined(OS_MACOSX) && !defined(USE_SKIA)
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
    case base::PLATFORM_FILE_ERROR_NOT_EMPTY:
      return WebKit::WebFileErrorInvalidModification;
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
    case base::PLATFORM_FILE_ERROR_NOT_A_FILE:
      return WebKit::WebFileErrorTypeMismatch;
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

class UserAgentState {
 public:
  UserAgentState();
  ~UserAgentState();

  void Set(const std::string& user_agent, bool overriding);
  const std::string& Get(const GURL& url) const;

 private:
  mutable std::string user_agent_;
  // The UA string when we're pretending to be Mac Safari or Win Firefox.
  mutable std::string user_agent_for_spoofing_hack_;

  mutable bool user_agent_requested_;
  bool user_agent_is_overridden_;

  // This object can be accessed from multiple threads, so use a lock around
  // accesses to the data members.
  mutable base::Lock lock_;
};

UserAgentState::UserAgentState()
    : user_agent_requested_(false),
      user_agent_is_overridden_(false) {
}

UserAgentState::~UserAgentState() {
}

void UserAgentState::Set(const std::string& user_agent, bool overriding) {
  base::AutoLock auto_lock(lock_);
  if (user_agent == user_agent_) {
    // We allow the user agent to be set multiple times as long as it
    // is set to the same value, in order to simplify unit testing
    // given g_user_agent is a global.
    return;
  }
  DCHECK(!user_agent.empty());
  DCHECK(!user_agent_requested_) << "Setting the user agent after someone has "
      "already requested it can result in unexpected behavior.";
  user_agent_is_overridden_ = overriding;
  user_agent_ = user_agent;
}

bool IsMicrosoftSiteThatNeedsSpoofingForSilverlight(const GURL& url) {
#if defined(OS_MACOSX)
  // The landing page for updating Silverlight gives a confusing experience
  // in browsers that Silverlight doesn't officially support; spoof as
  // Safari to reduce the chance that users won't complete updates.
  // Should be removed if the sniffing is removed: http://crbug.com/88211
  if (url.host() == "www.microsoft.com" &&
      StartsWithASCII(url.path(), "/getsilverlight", false)) {
    return true;
  }
#endif
  return false;
}

bool IsYahooSiteThatNeedsSpoofingForSilverlight(const GURL& url) {
  // The following Yahoo! JAPAN pages erroneously judge that Silverlight does
  // not support Chromium. Until the pages are fixed, spoof the UA.
  // http://crbug.com/104426
  if (url.host() == "headlines.yahoo.co.jp" &&
      StartsWithASCII(url.path(), "/videonews/", true)) {
    return true;
  }
#if defined(OS_MACOSX)
  if ((url.host() == "downloads.yahoo.co.jp" &&
      StartsWithASCII(url.path(), "/docs/silverlight/", true)) ||
      url.host() == "gyao.yahoo.co.jp") {
    return true;
  }
#elif defined(OS_WIN)
  if ((url.host() == "weather.yahoo.co.jp" &&
        StartsWithASCII(url.path(), "/weather/zoomradar/", true)) ||
      url.host() == "promotion.shopping.yahoo.co.jp") {
    return true;
  }
#endif
  return false;
}

const std::string& UserAgentState::Get(const GURL& url) const {
  base::AutoLock auto_lock(lock_);
  user_agent_requested_ = true;

  DCHECK(!user_agent_.empty());

  // Workarounds for sites that use misguided UA sniffing.
  if (!user_agent_is_overridden_) {
    if (IsMicrosoftSiteThatNeedsSpoofingForSilverlight(url) ||
        IsYahooSiteThatNeedsSpoofingForSilverlight(url)) {
      if (user_agent_for_spoofing_hack_.empty()) {
#if defined(OS_MACOSX)
        user_agent_for_spoofing_hack_ =
            BuildUserAgentFromProduct("Version/5.1.1 Safari/534.51.22");
#elif defined(OS_WIN)
        // Pretend to be Firefox. Silverlight doesn't support Win Safari.
        base::StringAppendF(
            &user_agent_for_spoofing_hack_,
            "Mozilla/5.0 (%s) Gecko/20100101 Firefox/8.0",
            webkit_glue::BuildOSCpuInfo().c_str());
#endif
      }
      return user_agent_for_spoofing_hack_;
    }
  }

  return user_agent_;
}

base::LazyInstance<UserAgentState> g_user_agent = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void SetUserAgent(const std::string& user_agent, bool overriding) {
  g_user_agent.Get().Set(user_agent, overriding);
}

const std::string& GetUserAgent(const GURL& url) {
  return g_user_agent.Get().Get(url);
}

void SetForcefullyTerminatePluginProcess(bool value) {
  g_forcefully_terminate_plugin_process = value;
}

bool ShouldForcefullyTerminatePluginProcess() {
  return g_forcefully_terminate_plugin_process;
}

WebCanvas* ToWebCanvas(skia::PlatformCanvas* canvas) {
#if WEBKIT_USING_SKIA
  return canvas;
#elif WEBKIT_USING_CG
  return skia::GetBitmapContext(skia::GetTopDevice(*canvas));
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

int GetGlyphPageCount() {
  return WebGlyphCache::pageCount();
}

std::string GetInspectorProtocolVersion() {
  return WebDevToolsAgent::inspectorProtocolVersion().utf8();
}

bool IsInspectorProtocolVersionSupported(const std::string& version) {
  return WebDevToolsAgent::supportsInspectorProtocolVersion(
      WebString::fromUTF8(version));
}

} // namespace webkit_glue

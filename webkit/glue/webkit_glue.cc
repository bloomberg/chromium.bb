// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(darin): Eliminate these WebCore includes
#include "config.h"
#include "GlyphPageTreeNode.h"
#undef LOG

#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include <objidl.h>
#include <mlang.h>
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

#include "base/file_version_info.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/sys_string_conversions.h"
#include "net/base/escape.h"
#include "skia/ext/platform_canvas.h"
#if defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#if defined(OS_WIN)
#include "third_party/WebKit/WebKit/chromium/public/win/WebInputEventFactory.h"
#endif
#include "third_party/WebKit/WebKit/chromium/src/WebFrameImpl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"

#include "webkit_version.h"  // Generated

using WebKit::WebCanvas;
using WebKit::WebData;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebFrameImpl;
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
static const char kChrome1ProductString[] = "Chrome/1.0.154.53";

//------------------------------------------------------------------------------
// webkit_glue impl:

namespace webkit_glue {

// Global variable used by the plugin quirk "die after unload".
bool g_forcefully_terminate_plugin_process = false;

void SetJavaScriptFlags(const std::wstring& str) {
#if WEBKIT_USING_V8
  std::string utf8_str = WideToUTF8(str);
  v8::V8::SetFlagsFromString(
      utf8_str.data(), static_cast<int>(utf8_str.size()));
#endif
}

void EnableWebCoreNotImplementedLogging() {
  // TODO(darin): Add a WebKit API to allow this to be set.
  WebCore::LogNotYetImplemented.state = WTFLogChannelOn;
}

std::wstring DumpDocumentText(WebFrame* web_frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  WebElement document_element = web_frame->document().documentElement();
  if (document_element.isNull())
    return std::wstring();

  return UTF16ToWideHack(document_element.innerText());
}

std::wstring DumpFramesAsText(WebFrame* web_frame, bool recursive) {
  std::wstring result;

  // Add header for all but the main frame. Skip empty frames.
  if (web_frame->parent() &&
      !web_frame->document().documentElement().isNull()) {
    result.append(L"\n--------\nFrame: '");
    result.append(UTF16ToWideHack(web_frame->name()));
    result.append(L"'\n--------\n");
  }

  result.append(DumpDocumentText(web_frame));
  result.append(L"\n");

  if (recursive) {
    WebFrame* child = web_frame->firstChild();
    for (; child; child = child->nextSibling())
      result.append(DumpFramesAsText(child, recursive));
  }

  return result;
}

std::wstring DumpRenderer(WebFrame* web_frame) {
  return UTF16ToWideHack(web_frame->renderTreeAsText());
}

bool CounterValueForElementById(WebFrame* web_frame, const std::string& id,
                                std::wstring* counter_value) {
  WebString result =
      web_frame->counterValueForElementById(WebString::fromUTF8(id));
  if (result.isNull())
    return false;

  *counter_value = UTF16ToWideHack(result);
  return true;
}

std::wstring DumpFrameScrollPosition(WebFrame* web_frame, bool recursive) {
  gfx::Size offset = web_frame->scrollOffset();
  std::wstring result;

  if (offset.width() > 0 || offset.height() > 0) {
    if (web_frame->parent()) {
      StringAppendF(&result, L"frame '%ls' ", UTF16ToWide(
          web_frame->name()).c_str());
    }
    StringAppendF(&result, L"scrolled to %d,%d\n",
                  offset.width(), offset.height());
  }

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

// Writes out a HistoryItem into a string in a readable format.
static std::wstring DumpHistoryItem(const WebHistoryItem& item,
                                    int indent, bool is_current) {
  std::wstring result;

  if (is_current) {
    result.append(L"curr->");
    result.append(indent - 6, L' ');  // 6 == L"curr->".length()
  } else {
    result.append(indent, L' ');
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

  result.append(UTF8ToWide(url));
  if (!item.target().isEmpty())
    result.append(L" (in frame \"" + UTF16ToWide(item.target()) + L"\")");
  if (item.isTargetItem())
    result.append(L"  **nav target**");
  result.append(L"\n");

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

std::wstring DumpHistoryState(const std::string& history_state, int indent,
                              bool is_current) {
  return DumpHistoryItem(HistoryItemFromString(history_state), indent,
                         is_current);
}

void ResetBeforeTestRun(WebView* view) {
  WebFrame* web_frame = view->mainFrame();

  // Reset the main frame name since tests always expect it to be empty.  It
  // is normally not reset between page loads (even in IE and FF).
  if (web_frame)
    web_frame->clearName();

#if defined(OS_WIN)
  // Reset the last click information so the clicks generated from previous
  // test aren't inherited (otherwise can mistake single/double/triple clicks)
  WebKit::WebInputEventFactory::resetLastClickState();
#endif
}

#ifndef NDEBUG
// The log macro was having problems due to collisions with WTF, so we just
// code here what that would have inlined.
void DumpLeakedObject(const char* file, int line, const char* object, int count) {
  std::string msg = StringPrintf("%s LEAKED %d TIMES", object, count);
  AppendToLog(file, line, msg.c_str());
}
#endif

void CheckForLeaks() {
#ifndef NDEBUG
  int count = WebFrameImpl::liveObjectCount();
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

std::string GetWebKitVersion() {
  return StringPrintf("%d.%d", WEBKIT_VERSION_MAJOR, WEBKIT_VERSION_MINOR);
}

namespace {

struct UserAgentState {
  UserAgentState()
      : user_agent_requested(false),
        user_agent_is_overridden(false) {
  }

  std::string user_agent;

  // The UA string when we're pretending to be Chrome 1.
  std::string mimic_chrome1_user_agent;

  // The UA string when we're pretending to be Windows Chrome.
  std::string mimic_windows_user_agent;

  bool user_agent_requested;
  bool user_agent_is_overridden;
};

Singleton<UserAgentState> g_user_agent;

std::string BuildOSCpuInfo() {
  std::string os_cpu;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  std::string cputype;
  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32)) {
    cputype.assign("i686 (x86_64)");
  } else {
    cputype.assign(unixinfo.machine);
  }
#endif

  StringAppendF(
      &os_cpu,
#if defined(OS_WIN)
      "Windows NT %d.%d",
      os_major_version,
      os_minor_version
#elif defined(OS_MACOSX)
      "Intel Mac OS X %d_%d_%d",
      os_major_version,
      os_minor_version,
      os_bugfix_version
#elif defined(OS_CHROMEOS)
      "CrOS %s %d.%d.%d",
      cputype.c_str(),  // e.g. i686
      os_major_version,
      os_minor_version,
      os_bugfix_version
#else
      "%s %s",
      unixinfo.sysname, // e.g. Linux
      cputype.c_str()   // e.g. i686
#endif
  );

  return os_cpu;
}

// Construct the User-Agent header, filling in |result|.
// The other parameters are workarounds for broken websites:
// - If mimic_chrome1 is true, produce a fake Chrome 1 string.
// - If mimic_windows is true, produce a fake Windows Chrome string.
void BuildUserAgent(bool mimic_chrome1, bool mimic_windows,
                    std::string* result) {
  const char kUserAgentPlatform[] =
#if defined(OS_WIN)
      "Windows";
#elif defined(OS_MACOSX)
      "Macintosh";
#elif defined(USE_X11)
      "X11";              // strange, but that's what Firefox uses
#else
      "?";
#endif

  const char kUserAgentSecurity = 'U'; // "US" strength encryption

  // TODO(port): figure out correct locale
  const char kUserAgentLocale[] = "en-US";

  // Get the product name and version, and replace Safari's Version/X string
  // with it.  This is done to expose our product name in a manner that is
  // maximally compatible with Safari, we hope!!
  std::string product;

  if (mimic_chrome1) {
    product = kChrome1ProductString;
  } else {
    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfoForCurrentModule());
    if (version_info.get()) {
      product = "Chrome/" + WideToASCII(version_info->product_version());
    } else {
      DLOG(WARNING) << "Unknown product version";
      product = "Chrome/0.0.0.0";
    }
  }

  // Derived from Safari's UA string.
  StringAppendF(
      result,
      "Mozilla/5.0 (%s; %c; %s; %s) AppleWebKit/%d.%d"
      " (KHTML, like Gecko) %s Safari/%d.%d",
      mimic_windows ? "Windows" : kUserAgentPlatform,
      kUserAgentSecurity,
      ((mimic_windows ? "Windows " : "") + BuildOSCpuInfo()).c_str(),
      kUserAgentLocale,
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR,
      product.c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR
      );
}

void SetUserAgentToDefault() {
  BuildUserAgent(false, false, &g_user_agent->user_agent);
}

}  // namespace

void SetUserAgent(const std::string& new_user_agent) {
  // If you combine this with the previous line, the function only works the
  // first time.
  DCHECK(!g_user_agent->user_agent_requested) <<
      "Setting the user agent after someone has "
      "already requested it can result in unexpected behavior.";
  g_user_agent->user_agent_is_overridden = true;
  g_user_agent->user_agent = new_user_agent;
}

const std::string& GetUserAgent(const GURL& url) {
  if (g_user_agent->user_agent.empty())
    SetUserAgentToDefault();
  g_user_agent->user_agent_requested = true;
  if (!g_user_agent->user_agent_is_overridden) {
    // Workarounds for sites that use misguided UA sniffing.
    if (MatchPatternASCII(url.host(), "*.pointroll.com")) {
      // For cnn.com, which uses pointroll.com to serve their front door promo,
      // we must spoof Chrome 1.0 in order to avoid a blank page.
      // http://crbug.com/25934
      // TODO(dglazkov): Remove this once CNN's front door promo is
      // over or when pointroll fixes their sniffing.
      if (g_user_agent->mimic_chrome1_user_agent.empty())
        BuildUserAgent(true, false, &g_user_agent->mimic_chrome1_user_agent);
      return g_user_agent->mimic_chrome1_user_agent;
    }
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    else if (MatchPatternASCII(url.host(), "*.mail.yahoo.com")) {
      // mail.yahoo.com is ok with Windows Chrome but not Linux Chrome.
      // http://bugs.chromium.org/11136
      // TODO(evanm): remove this if Yahoo fixes their sniffing.
      if (g_user_agent->mimic_windows_user_agent.empty())
        BuildUserAgent(false, true, &g_user_agent->mimic_windows_user_agent);
      return g_user_agent->mimic_windows_user_agent;
    }
#endif
  }
  return g_user_agent->user_agent;
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
  // TODO(darin): Add a WebKit API to expose this counter.
  return WebCore::GlyphPageTreeNode::treeGlyphPageCount();
}

bool g_enable_media_cache = false;

bool IsMediaCacheEnabled() {
  return g_enable_media_cache;
}

void SetMediaCacheEnabled(bool enabled) {
  g_enable_media_cache = enabled;
}

} // namespace webkit_glue

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include <objidl.h>
#include <mlang.h>
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

#if defined(OS_LINUX)
#include <malloc.h>
#endif

#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "skia/ext/platform_canvas.h"
#if defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGlyphCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPrintParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#if defined(OS_WIN)
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"
#endif
#include "v8/include/v8.h"
#include "webkit/glue/glue_serialize.h"

using WebKit::WebCanvas;
using WebKit::WebData;
using WebKit::WebDevToolsAgent;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebGlyphCache;
using WebKit::WebHistoryItem;
using WebKit::WebImage;
using WebKit::WebPrintParams;
using WebKit::WebRect;
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
    result.append(web_frame->uniqueName());
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

int NumberOfPages(WebFrame* web_frame,
                  float page_width_in_pixels,
                  float page_height_in_pixels) {
  WebSize size(static_cast<int>(page_width_in_pixels),
               static_cast<int>(page_height_in_pixels));

  WebPrintParams print_params;
  print_params.paperSize = size;
  print_params.printContentArea = WebRect(0, 0, size.width, size.height);
  print_params.printableArea = WebRect(0, 0, size.width, size.height);

  int number_of_pages = web_frame->printBegin(print_params);
  web_frame->printEnd();
  return number_of_pages;
}

string16 DumpFrameScrollPosition(WebFrame* web_frame, bool recursive) {
  gfx::Size offset = web_frame->scrollOffset();
  std::string result_utf8;

  if (offset.width() > 0 || offset.height() > 0) {
    if (web_frame->parent()) {
      base::StringAppendF(&result_utf8, "frame '%s' ",
                          UTF16ToUTF8(web_frame->uniqueName()).c_str());
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

void PlatformFileInfoToWebFileInfo(
    const base::PlatformFileInfo& file_info,
    WebKit::WebFileInfo* web_file_info) {
  DCHECK(web_file_info);
  // WebKit now expects NaN as uninitialized/null Date.
  if (file_info.last_modified.is_null())
    web_file_info->modificationTime = std::numeric_limits<double>::quiet_NaN();
  else
    web_file_info->modificationTime = file_info.last_modified.ToDoubleT();
  web_file_info->length = file_info.size;
  if (file_info.is_directory)
    web_file_info->type = WebKit::WebFileInfo::TypeDirectory;
  else
    web_file_info->type = WebKit::WebFileInfo::TypeFile;
}

void SetForcefullyTerminatePluginProcess(bool value) {
  g_forcefully_terminate_plugin_process = value;
}

bool ShouldForcefullyTerminatePluginProcess() {
  return g_forcefully_terminate_plugin_process;
}

WebCanvas* ToWebCanvas(skia::PlatformCanvas* canvas) {
  return canvas;
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

void ConfigureURLRequestForReferrerPolicy(
    net::URLRequest* request, WebKit::WebReferrerPolicy referrer_policy) {
  net::URLRequest::ReferrerPolicy net_referrer_policy =
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  switch (referrer_policy) {
    case WebKit::WebReferrerPolicyDefault:
      net_referrer_policy =
          net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      break;

    case WebKit::WebReferrerPolicyAlways:
    case WebKit::WebReferrerPolicyNever:
    case WebKit::WebReferrerPolicyOrigin:
      net_referrer_policy = net::URLRequest::NEVER_CLEAR_REFERRER;
      break;
  }
  request->set_referrer_policy(net_referrer_policy);
}

COMPILE_ASSERT(std::numeric_limits<double>::has_quiet_NaN, has_quiet_NaN);

#if defined(OS_LINUX) || defined(OS_ANDROID)
size_t MemoryUsageKB() {
  struct mallinfo minfo = mallinfo();
  uint64_t mem_usage =
#if defined(USE_TCMALLOC)
      minfo.uordblks
#else
      (minfo.hblkhd + minfo.arena)
#endif
      >> 10;

  v8::HeapStatistics stat;
  v8::V8::GetHeapStatistics(&stat);
  return mem_usage + (static_cast<uint64_t>(stat.total_heap_size()) >> 10);
}
#elif defined(OS_MACOSX)
size_t MemoryUsageKB() {
  scoped_ptr<base::ProcessMetrics> process_metrics(
      // The default port provider is sufficient to get data for the current
      // process.
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle(), NULL));
  return process_metrics->GetWorkingSetSize() >> 10;
}
#else
size_t MemoryUsageKB() {
  scoped_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));
  return process_metrics->GetPagefileUsage() >> 10;
}
#endif

} // namespace webkit_glue

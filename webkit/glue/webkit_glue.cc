// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include <mlang.h>
#include <objidl.h>
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
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "skia/ext/platform_canvas.h"
#if defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebGlyphCache.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebView.h"
#if defined(OS_WIN)
#include "third_party/WebKit/public/web/win/WebInputEventFactory.h"
#endif
#include "third_party/skia/include/core/SkBitmap.h"
#include "v8/include/v8.h"

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

void SetJavaScriptFlags(const std::string& str) {
  v8::V8::SetFlagsFromString(str.data(), static_cast<int>(str.size()));
}

void EnableWebCoreLogChannels(const std::string& channels) {
  if (channels.empty())
    return;
  base::StringTokenizer t(channels, ", ");
  while (t.GetNext()) {
    WebKit::enableLogChannel(t.token().c_str());
  }
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

  *image = web_image.getSkBitmap();
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
  // TODO(svenpanne) The call below doesn't take web workers into account, this
  // has to be done manually by iterating over all Isolates involved.
  v8::Isolate::GetCurrent()->GetHeapStatistics(&stat);
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

double ZoomFactorToZoomLevel(double factor) {
  return WebView::zoomFactorToZoomLevel(factor);
}

} // namespace webkit_glue

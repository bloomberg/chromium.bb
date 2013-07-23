// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#if defined(OS_LINUX)
#include <malloc.h>
#endif

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/escape.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebGlyphCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "v8/include/v8.h"

using WebKit::WebCanvas;
using WebKit::WebFrame;
using WebKit::WebGlyphCache;

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

COMPILE_ASSERT(std::numeric_limits<double>::has_quiet_NaN, has_quiet_NaN);

} // namespace webkit_glue

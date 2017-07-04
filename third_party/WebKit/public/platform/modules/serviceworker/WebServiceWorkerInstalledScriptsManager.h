// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerInstalledScriptsManager_h
#define WebServiceWorkerInstalledScriptsManager_h

#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"

#if INSIDE_BLINK
#include <memory>
#include "platform/network/HTTPHeaderMap.h"
#endif  // INSIDE_BLINK

namespace blink {

// WebServiceWorkerInstalledScriptsManager provides installed main script and
// imported scripts to the service worker thread. This is used by multiple
// threads, so implementation needs to be thread safe.
class WebServiceWorkerInstalledScriptsManager {
 public:
  using BytesChunk = WebVector<char>;
  // Container of a script. All the fields of this class need to be
  // cross-thread-transfer-safe.
  class BLINK_PLATFORM_EXPORT RawScriptData {
   public:
    RawScriptData(WebString encoding,
                  WebVector<BytesChunk> script_text,
                  WebVector<BytesChunk> meta_data);
    void AddHeader(const WebString& key, const WebString& value);

#if INSIDE_BLINK
    // The encoding of the script text.
    const WebString& Encoding() const { return encoding_; }
    // An array of raw byte chunks of the script text.
    const WebVector<BytesChunk>& ScriptTextChunks() const {
      return script_text_;
    }
    // An array of raw byte chunks of the scripts's meta data from the script's
    // V8 code cache.
    const WebVector<BytesChunk>& MetaDataChunks() const { return meta_data_; }
    // The HTTP headers of the script.
    std::unique_ptr<CrossThreadHTTPHeaderMapData> TakeHeaders() {
      return std::move(headers_);
    }

   private:
    WebString encoding_;
    WebVector<BytesChunk> script_text_;
    WebVector<BytesChunk> meta_data_;
    std::unique_ptr<CrossThreadHTTPHeaderMapData> headers_;
#endif  // INSIDE_BLINK
  };

  virtual ~WebServiceWorkerInstalledScriptsManager() = default;

  // Called on the main or worker thread.
  virtual bool IsScriptInstalled(const blink::WebURL& script_url) const = 0;
  // Called on the worker thread.
  virtual std::unique_ptr<RawScriptData> GetRawScriptData(
      const WebURL& script_url) = 0;
};

}  // namespace blink

#endif  // WebServiceWorkerInstalledScriptsManager_h

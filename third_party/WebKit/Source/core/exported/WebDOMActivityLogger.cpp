/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebDOMActivityLogger.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DOMActivityLoggerContainer : public V8DOMActivityLogger {
 public:
  explicit DOMActivityLoggerContainer(
      std::unique_ptr<WebDOMActivityLogger> logger)
      : dom_activity_logger_(std::move(logger)) {}

  void LogGetter(const String& api_name) override {
    dom_activity_logger_->LogGetter(WebString(api_name), GetURL(), GetTitle());
  }

  void LogSetter(const String& api_name,
                 const v8::Local<v8::Value>& new_value) override {
    dom_activity_logger_->LogSetter(WebString(api_name), new_value, GetURL(),
                                    GetTitle());
  }

  void LogMethod(const String& api_name,
                 int argc,
                 const v8::Local<v8::Value>* argv) override {
    dom_activity_logger_->LogMethod(WebString(api_name), argc, argv, GetURL(),
                                    GetTitle());
  }

  void LogEvent(const String& event_name,
                int argc,
                const String* argv) override {
    Vector<WebString> web_string_argv;
    for (int i = 0; i < argc; i++)
      web_string_argv.push_back(argv[i]);
    dom_activity_logger_->LogEvent(WebString(event_name), argc,
                                   web_string_argv.data(), GetURL(),
                                   GetTitle());
  }

 private:
  WebURL GetURL() {
    if (Document* document =
            CurrentDOMWindow(v8::Isolate::GetCurrent())->document())
      return WebURL(document->Url());
    return WebURL();
  }

  WebString GetTitle() {
    if (Document* document =
            CurrentDOMWindow(v8::Isolate::GetCurrent())->document())
      return WebString(document->title());
    return WebString();
  }

  std::unique_ptr<WebDOMActivityLogger> dom_activity_logger_;
};

bool HasDOMActivityLogger(int world_id, const WebString& extension_id) {
  return V8DOMActivityLogger::ActivityLogger(world_id, extension_id);
}

void SetDOMActivityLogger(int world_id,
                          const WebString& extension_id,
                          WebDOMActivityLogger* logger) {
  DCHECK(logger);
  V8DOMActivityLogger::SetActivityLogger(
      world_id, extension_id,
      WTF::WrapUnique(new DOMActivityLoggerContainer(WTF::WrapUnique(logger))));
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ApplicationCacheErrorEvent_h
#define ApplicationCacheErrorEvent_h

#include "core/dom/events/Event.h"
#include "core/events/ApplicationCacheErrorEventInit.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "public/platform/WebApplicationCacheHostClient.h"

namespace blink {

class ApplicationCacheErrorEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~ApplicationCacheErrorEvent() override;

  static ApplicationCacheErrorEvent* Create(
      WebApplicationCacheHost::ErrorReason reason,
      const String& url,
      int status,
      const String& message) {
    return new ApplicationCacheErrorEvent(reason, url, status, message);
  }

  static ApplicationCacheErrorEvent* Create(
      const AtomicString& event_type,
      const ApplicationCacheErrorEventInit& initializer) {
    return new ApplicationCacheErrorEvent(event_type, initializer);
  }

  const String& reason() const { return reason_; }
  const String& url() const { return url_; }
  int status() const { return status_; }
  const String& message() const { return message_; }

  const AtomicString& InterfaceName() const override {
    return EventNames::ApplicationCacheErrorEvent;
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  ApplicationCacheErrorEvent(WebApplicationCacheHost::ErrorReason,
                             const String& url,
                             int status,
                             const String& message);
  ApplicationCacheErrorEvent(const AtomicString& event_type,
                             const ApplicationCacheErrorEventInit& initializer);

  String reason_;
  String url_;
  int status_;
  String message_;
};

}  // namespace blink

#endif  // ApplicationCacheErrorEvent_h

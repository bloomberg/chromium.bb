// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClientHintsPreferences_h
#define ClientHintsPreferences_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebClientHintsType.h"

namespace blink {

class ResourceResponse;

// TODO (tbansal): Remove PLATFORM_EXPORT, and pass WebClientHintsType
// everywhere.
class PLATFORM_EXPORT ClientHintsPreferences {
  DISALLOW_NEW();

 public:
  class Context {
   public:
    virtual void CountClientHints(WebClientHintsType) = 0;
    virtual void CountPersistentClientHintHeaders() = 0;

   protected:
    virtual ~Context() {}
  };

  ClientHintsPreferences();

  void UpdateFrom(const ClientHintsPreferences&);

  // Parses the client hints headers, and populates |this| with the client hint
  // preferences. |context| may be null.
  void UpdateFromAcceptClientHintsHeader(const String& header_value, Context*);

  bool ShouldSend(WebClientHintsType type) const {
    return enabled_hints_.IsEnabled(type);
  }
  void SetShouldSendForTesting(WebClientHintsType type) {
    enabled_hints_.SetIsEnabled(type, true);
  }

  // Parses the client hints headers, and populates |enabled_hints| with the
  // client hint preferences that should be persisted for |persist_duration|.
  // |persist_duration| should be non-null.
  // If there are no client hints that need to be persisted,
  // |persist_duration| is not set, otherwise it is set to the duration for
  // which the client hint preferences should be persisted.
  // UpdatePersistentHintsFromHeaders may be called for all responses
  // received (including subresources). |context| may be null.
  static void UpdatePersistentHintsFromHeaders(
      const ResourceResponse&,
      Context*,
      WebEnabledClientHints& enabled_hints,
      TimeDelta* persist_duration);

 private:
  WebEnabledClientHints enabled_hints_;
};

}  // namespace blink

#endif

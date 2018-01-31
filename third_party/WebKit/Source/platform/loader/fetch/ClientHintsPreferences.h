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

class KURL;
class ResourceResponse;

// TODO (tbansal): Remove PLATFORM_EXPORT, and pass WebClientHintsType
// everywhere.
class PLATFORM_EXPORT ClientHintsPreferences {
  DISALLOW_NEW();

 public:
  class Context {
   public:
    virtual void CountClientHints(mojom::WebClientHintsType) = 0;
    virtual void CountPersistentClientHintHeaders() = 0;

   protected:
    virtual ~Context() = default;
  };

  ClientHintsPreferences();

  void UpdateFrom(const ClientHintsPreferences&);

  // Parses the client hints headers, and populates |this| with the client hint
  // preferences. |url| is the URL of the resource whose response included the
  // |header_value|. |context| may be null. If client hints are not allowed for
  // |url|, then |this| would not be updated.
  void UpdateFromAcceptClientHintsHeader(const String& header_value,
                                         const KURL&,
                                         Context*);

  bool ShouldSend(mojom::WebClientHintsType type) const {
    return enabled_hints_.IsEnabled(type);
  }
  void SetShouldSendForTesting(mojom::WebClientHintsType type) {
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

  // Returns true if client hints are allowed for the provided KURL. Client
  // hints are allowed only on HTTP URLs that belong to secure contexts.
  static bool IsClientHintsAllowed(const KURL&);

 private:
  WebEnabledClientHints enabled_hints_;
};

}  // namespace blink

#endif

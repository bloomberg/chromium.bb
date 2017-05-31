// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Deprecation_h
#define Deprecation_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "platform/wtf/BitVector.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT Deprecation {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(Deprecation);

 public:
  Deprecation();
  ~Deprecation();

  static void WarnOnDeprecatedProperties(const LocalFrame*,
                                         CSSPropertyID unresolved_property);
  void ClearSuppression();

  void MuteForInspector();
  void UnmuteForInspector();

  // "countDeprecation" sets the bit for this feature to 1, and sends a
  // deprecation warning to the console. Repeated calls are ignored.
  //
  // Be considerate to developers' consoles: features should only send
  // deprecation warnings when we're actively interested in removing them from
  // the platform.
  //
  // For shared workers and service workers, the ExecutionContext* overload
  // doesn't count the usage but only sends a console warning.
  // TODO(lunalu): Deprecate UseCounter::Feature by WebFeature
  static void CountDeprecation(const LocalFrame*, UseCounter::Feature);
  static void CountDeprecation(ExecutionContext*, UseCounter::Feature);
  static void CountDeprecation(const Document&, UseCounter::Feature);
  static void CountDeprecation(const LocalFrame* frame, WebFeature feature) {
    return CountDeprecation(frame, static_cast<UseCounter::Feature>(feature));
  }
  static void CountDeprecation(ExecutionContext* exec_context,
                               WebFeature feature) {
    return CountDeprecation(exec_context,
                            static_cast<UseCounter::Feature>(feature));
  }
  static void CountDeprecation(const Document& document, WebFeature feature) {
    return CountDeprecation(document,
                            static_cast<UseCounter::Feature>(feature));
  }

  // Count only features if they're being used in an iframe which does not
  // have script access into the top level document.
  static void CountDeprecationCrossOriginIframe(const LocalFrame*,
                                                UseCounter::Feature);
  static void CountDeprecationCrossOriginIframe(const Document&,
                                                UseCounter::Feature);
  static String DeprecationMessage(UseCounter::Feature);
  static void CountDeprecationCrossOriginIframe(const LocalFrame* frame,
                                                WebFeature feature) {
    return CountDeprecationCrossOriginIframe(
        frame, static_cast<UseCounter::Feature>(feature));
  }
  static void CountDeprecationCrossOriginIframe(const Document& document,
                                                WebFeature feature) {
    return CountDeprecationCrossOriginIframe(
        document, static_cast<UseCounter::Feature>(feature));
  }
  static String DeprecationMessage(WebFeature feature) {
    return DeprecationMessage(static_cast<UseCounter::Feature>(feature));
  }

  // Note: this is only public for tests.
  bool IsSuppressed(CSSPropertyID unresolved_property);

 protected:
  void Suppress(CSSPropertyID unresolved_property);
  // CSSPropertyIDs that aren't deprecated return an empty string.
  static String DeprecationMessage(CSSPropertyID unresolved_property);

  BitVector css_property_deprecation_bits_;
  unsigned mute_count_;
};

}  // namespace blink

#endif  // Deprecation_h

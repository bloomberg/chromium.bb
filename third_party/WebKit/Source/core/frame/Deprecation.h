// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Deprecation_h
#define Deprecation_h

#include "base/macros.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "platform/wtf/BitVector.h"

namespace blink {

class LocalFrame;
enum class FeaturePolicyFeature;

class CORE_EXPORT Deprecation {
  DISALLOW_NEW();

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
  static void CountDeprecation(const LocalFrame*, WebFeature);
  static void CountDeprecation(ExecutionContext*, WebFeature);
  static void CountDeprecation(const Document&, WebFeature);

  // Count only features if they're being used in an iframe which does not
  // have script access into the top level document.
  static void CountDeprecationCrossOriginIframe(const LocalFrame*, WebFeature);
  static void CountDeprecationCrossOriginIframe(const Document&, WebFeature);

  static void CountDeprecationFeaturePolicy(const Document&,
                                            FeaturePolicyFeature);

  static String DeprecationMessage(WebFeature);

  // Note: this is only public for tests.
  bool IsSuppressed(CSSPropertyID unresolved_property);

 protected:
  void Suppress(CSSPropertyID unresolved_property);
  // CSSPropertyIDs that aren't deprecated return an empty string.
  static String DeprecationMessage(CSSPropertyID unresolved_property);

  // Generates a deprecation report, to be routed to the Reporting API and any
  // ReportingObservers. Also sends the deprecation message to the console.
  static void GenerateReport(const LocalFrame*, WebFeature);

  BitVector css_property_deprecation_bits_;
  unsigned mute_count_;

  DISALLOW_COPY_AND_ASSIGN(Deprecation);
};

}  // namespace blink

#endif  // Deprecation_h

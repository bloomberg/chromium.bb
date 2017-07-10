/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UseCounter_h
#define UseCounter_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/parser/CSSParserMode.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/BitVector.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/web_feature.mojom-blink.h"
#include "v8/include/v8.h"

namespace blink {

class CSSStyleSheet;
class Document;
class EnumerationHistogram;
class ExecutionContext;
class LocalFrame;
class StyleSheetContents;
// Definition for UseCounter features can be found in:
// third_party/WebKit/public/platform/web_feature.mojom
using WebFeature = mojom::WebFeature;

// UseCounter is used for counting the number of times features of
// Blink are used on real web pages and help us know commonly
// features are used and thus when it's safe to remove or change them.
//
// The Chromium Content layer controls what is done with this data.
//
// For instance, in Google Chrome, these counts are submitted anonymously
// through the UMA histogram recording system in Chrome for users who have the
// "Automatically send usage statistics and crash reports to Google" setting
// enabled:
// http://www.google.com/chrome/intl/en/privacy.html
//
// Changes on UseCounter are observable by UseCounter::Observer.
class CORE_EXPORT UseCounter {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(UseCounter);

 public:
  enum Context {
    kDefaultContext,
    // Counters for SVGImages (lifetime independent from other pages).
    kSVGImageContext,
    // Counters for extensions.
    kExtensionContext,
    // Context when counters should be disabled (eg, internal pages such as
    // about, chrome-devtools, etc).
    kDisabledContext
  };

  UseCounter(Context = kDefaultContext);

  // An interface to observe UseCounter changes. Note that this is never
  // notified when the counter is disabled by |m_muteCount| or when |m_context|
  // is kDisabledContext.
  class Observer : public GarbageCollected<Observer> {
   public:
    // Notified when a feature is counted for the first time. This should return
    // true if it no longer needs to observe changes so that the counter can
    // remove a reference to the observer and stop notifications.
    virtual bool OnCountFeature(WebFeature) = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };

  // "count" sets the bit for this feature to 1. Repeated calls are ignored.
  static void Count(const LocalFrame*, WebFeature);
  static void Count(const Document&, WebFeature);
  static void Count(ExecutionContext*, WebFeature);

  void Count(CSSParserMode, CSSPropertyID);
  void Count(WebFeature, const LocalFrame*);

  static void CountAnimatedCSS(const Document&, CSSPropertyID);
  void CountAnimatedCSS(CSSPropertyID);

  // Count only features if they're being used in an iframe which does not
  // have script access into the top level document.
  static void CountCrossOriginIframe(const Document&, WebFeature);

  // Return whether the Feature was previously counted for this document.
  // NOTE: only for use in testing.
  static bool IsCounted(Document&, WebFeature);
  // Return whether the CSSPropertyID was previously counted for this document.
  // NOTE: only for use in testing.
  static bool IsCounted(Document&, const String&);
  bool IsCounted(CSSPropertyID unresolved_property);

  // Return whether the CSSPropertyID was previously counted for this document.
  // NOTE: only for use in testing.
  static bool IsCountedAnimatedCSS(Document&, const String&);
  bool IsCountedAnimatedCSS(CSSPropertyID unresolved_property);

  // Retains a reference to the observer to notify of UseCounter changes.
  void AddObserver(Observer*);

  // Invoked when a new document is loaded into the main frame of the page.
  void DidCommitLoad(const KURL&);

  static int MapCSSPropertyIdToCSSSampleIdForHistogram(CSSPropertyID);

  // When muted, all calls to "count" functions are ignoed.  May be nested.
  void MuteForInspector();
  void UnmuteForInspector();

  void RecordMeasurement(WebFeature);

  // Return whether the feature has been seen since the last page load
  // (except when muted).  Does include features seen in documents which have
  // reporting disabled.
  bool HasRecordedMeasurement(WebFeature) const;

  DECLARE_TRACE();

 private:
  // Notifies that a feature is newly counted to |m_observers|. This shouldn't
  // be called when the counter is disabled by |m_muteCount| or when |m_context|
  // if kDisabledContext.
  void NotifyFeatureCounted(WebFeature);

  EnumerationHistogram& FeaturesHistogram() const;
  EnumerationHistogram& CssHistogram() const;
  EnumerationHistogram& AnimatedCSSHistogram() const;

  // If non-zero, ignore all 'count' calls completely.
  unsigned mute_count_;

  // The scope represented by this UseCounter instance, which must be fixed for
  // the duration of a page but can change when a new page is loaded.
  Context context_;

  // Track what features/properties have been reported to the (non-legacy)
  // histograms.
  BitVector features_recorded_;
  BitVector css_recorded_;
  BitVector animated_css_recorded_;

  HeapHashSet<Member<Observer>> observers_;

  // Encapsulates the work to preserve the old "FeatureObserver" histogram with
  // original semantics
  // TODO(rbyers): remove this - http://crbug.com/676837
  class CORE_EXPORT LegacyCounter {
   public:
    LegacyCounter();
    ~LegacyCounter();
    void CountFeature(WebFeature);
    void CountCSS(CSSPropertyID);
    void UpdateMeasurements();

   private:
    // Tracks what features/properties need to be reported to the legacy
    // histograms.
    BitVector feature_bits_;
    BitVector css_bits_;
  } legacy_counter_;
};

}  // namespace blink

#endif  // UseCounter_h

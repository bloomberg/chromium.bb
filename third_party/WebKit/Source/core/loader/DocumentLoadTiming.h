/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DocumentLoadTiming_h
#define DocumentLoadTiming_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

class DocumentLoader;
class KURL;
class LocalFrame;

class CORE_EXPORT DocumentLoadTiming final {
  DISALLOW_NEW();

 public:
  explicit DocumentLoadTiming(DocumentLoader&);

  double MonotonicTimeToZeroBasedDocumentTime(double) const;
  double MonotonicTimeToPseudoWallTime(double) const;
  double PseudoWallTimeToMonotonicTime(double) const;

  void MarkNavigationStart();
  void SetNavigationStart(double);

  void AddRedirect(const KURL& redirecting_url, const KURL& redirected_url);
  void SetRedirectStart(double);
  void SetRedirectEnd(double);
  void SetRedirectCount(short value) { redirect_count_ = value; }
  void SetHasCrossOriginRedirect(bool value) {
    has_cross_origin_redirect_ = value;
  }

  void MarkUnloadEventStart(double);
  void MarkUnloadEventEnd(double);

  void MarkFetchStart();
  void SetFetchStart(double);

  void SetResponseEnd(double);

  void MarkLoadEventStart();
  void MarkLoadEventEnd();

  void SetHasSameOriginAsPreviousDocument(bool value) {
    has_same_origin_as_previous_document_ = value;
  }

  double NavigationStart() const { return navigation_start_; }
  double UnloadEventStart() const { return unload_event_start_; }
  double UnloadEventEnd() const { return unload_event_end_; }
  double RedirectStart() const { return redirect_start_; }
  double RedirectEnd() const { return redirect_end_; }
  short RedirectCount() const { return redirect_count_; }
  double FetchStart() const { return fetch_start_; }
  double ResponseEnd() const { return response_end_; }
  double LoadEventStart() const { return load_event_start_; }
  double LoadEventEnd() const { return load_event_end_; }
  bool HasCrossOriginRedirect() const { return has_cross_origin_redirect_; }
  bool HasSameOriginAsPreviousDocument() const {
    return has_same_origin_as_previous_document_;
  }

  double ReferenceMonotonicTime() const { return reference_monotonic_time_; }

  void Trace(blink::Visitor*);

 private:
  void MarkRedirectEnd();
  void NotifyDocumentTimingChanged();
  void EnsureReferenceTimesSet();
  LocalFrame* GetFrame() const;

  double reference_monotonic_time_;
  double reference_wall_time_;
  double navigation_start_;
  double unload_event_start_;
  double unload_event_end_;
  double redirect_start_;
  double redirect_end_;
  short redirect_count_;
  double fetch_start_;
  double response_end_;
  double load_event_start_;
  double load_event_end_;
  bool has_cross_origin_redirect_;
  bool has_same_origin_as_previous_document_;

  Member<DocumentLoader> document_loader_;
};

}  // namespace blink

#endif

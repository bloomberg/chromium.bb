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

#ifndef DocumentTiming_h
#define DocumentTiming_h

#include "platform/heap/Handle.h"

namespace blink {

class Document;
class LocalFrame;

class DocumentTiming final {
  DISALLOW_NEW();

 public:
  explicit DocumentTiming(Document&);

  void MarkDomLoading();
  void MarkDomInteractive();
  void MarkDomContentLoadedEventStart();
  void MarkDomContentLoadedEventEnd();
  void MarkDomComplete();
  void MarkFirstLayout();

  // These return monotonically-increasing seconds.
  double DomLoading() const { return dom_loading_; }
  double DomInteractive() const { return dom_interactive_; }
  double DomContentLoadedEventStart() const {
    return dom_content_loaded_event_start_;
  }
  double DomContentLoadedEventEnd() const {
    return dom_content_loaded_event_end_;
  }
  double DomComplete() const { return dom_complete_; }
  double FirstLayout() const { return first_layout_; }

  void Trace(blink::Visitor*);

 private:
  LocalFrame* GetFrame() const;
  void NotifyDocumentTimingChanged();

  double dom_loading_ = 0.0;
  double dom_interactive_ = 0.0;
  double dom_content_loaded_event_start_ = 0.0;
  double dom_content_loaded_event_end_ = 0.0;
  double dom_complete_ = 0.0;
  double first_layout_ = 0.0;

  Member<Document> document_;
};

}  // namespace blink

#endif

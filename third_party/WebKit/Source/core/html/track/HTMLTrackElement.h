/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef HTMLTrackElement_h
#define HTMLTrackElement_h

#include "core/html/HTMLElement.h"
#include "core/html/track/TextTrack.h"
#include "core/loader/TextTrackLoader.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLMediaElement;
class LoadableTextTrack;

class HTMLTrackElement final : public HTMLElement,
                               private TextTrackLoaderClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLTrackElement);

 public:
  DECLARE_NODE_FACTORY(HTMLTrackElement);

  const AtomicString& kind();
  void setKind(const AtomicString&);

  enum ReadyState { kNone = 0, kLoading = 1, kLoaded = 2, kError = 3 };
  ReadyState getReadyState();
  void ScheduleLoad();

  TextTrack* track();

  virtual void Trace(blink::Visitor*);

 private:
  explicit HTMLTrackElement(Document&);
  ~HTMLTrackElement() override;

  void ParseAttribute(const AttributeModificationParams&) override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;

  void RemovedFrom(ContainerNode*) override;
  bool IsURLAttribute(const Attribute&) const override;

  // TextTrackLoaderClient
  void NewCuesAvailable(TextTrackLoader*) override;
  void CueLoadingCompleted(TextTrackLoader*, bool loading_failed) override;

  void SetReadyState(ReadyState);

  const AtomicString& MediaElementCrossOriginAttribute() const;
  bool CanLoadUrl(const KURL&);
  void LoadTimerFired(TimerBase*);

  enum LoadStatus { kFailure, kSuccess };
  void DidCompleteLoad(LoadStatus);

  HTMLMediaElement* MediaElement() const;

  LoadableTextTrack* EnsureTrack();

  Member<LoadableTextTrack> track_;
  Member<TextTrackLoader> loader_;
  TaskRunnerTimer<HTMLTrackElement> load_timer_;
  KURL url_;
};

}  // namespace blink

#endif  // HTMLTrackElement_h

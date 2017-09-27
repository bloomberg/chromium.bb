/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef SpellCheckRequester_h
#define SpellCheckRequester_h

#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/Forward.h"
#include "platform/Timer.h"
#include "platform/text/TextChecking.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LocalFrame;
class SpellCheckRequester;
class TextCheckerClient;

class CORE_EXPORT SpellCheckRequest final : public TextCheckingRequest {
 public:
  static SpellCheckRequest* Create(const EphemeralRange& checking_range,
                                   int request_number);

  ~SpellCheckRequest() override;
  void Dispose();

  Range* CheckingRange() const { return checking_range_; }
  Element* RootEditableElement() const { return root_editable_element_; }

  void SetCheckerAndSequence(SpellCheckRequester*, int sequence);

  const TextCheckingRequestData& Data() const override;
  bool IsValid() const;
  void DidSucceed(const Vector<TextCheckingResult>&) override;
  void DidCancel() override;

  int RequestNumber() const { return request_number_; }

  DECLARE_VIRTUAL_TRACE();

 private:
  SpellCheckRequest(Range* checking_range, const String&, int request_number);

  Member<SpellCheckRequester> requester_;
  Member<Range> checking_range_;
  Member<Element> root_editable_element_;
  TextCheckingRequestData request_data_;
  int request_number_;
};

class SpellCheckRequester final
    : public GarbageCollectedFinalized<SpellCheckRequester> {
  WTF_MAKE_NONCOPYABLE(SpellCheckRequester);

 public:
  static SpellCheckRequester* Create(LocalFrame& frame) {
    return new SpellCheckRequester(frame);
  }

  ~SpellCheckRequester();
  DECLARE_TRACE();

  void RequestCheckingFor(const EphemeralRange&);
  void RequestCheckingFor(const EphemeralRange&, int request_num);
  void CancelCheck();

  int LastRequestSequence() const { return last_request_sequence_; }

  int LastProcessedSequence() const { return last_processed_sequence_; }

  // Exposed for leak detector only, see comment for corresponding
  // SpellChecker method.
  void PrepareForLeakDetection();

 private:
  friend class SpellCheckRequest;

  explicit SpellCheckRequester(LocalFrame&);

  TextCheckerClient& Client() const;
  void TimerFiredToProcessQueuedRequest(TimerBase*);
  void InvokeRequest(SpellCheckRequest*);
  void EnqueueRequest(SpellCheckRequest*);
  bool EnsureValidRequestQueueFor(int sequence);
  void DidCheckSucceed(int sequence, const Vector<TextCheckingResult>&);
  void DidCheckCancel(int sequence);
  void DidCheck(int sequence);

  void ClearProcessingRequest();

  Member<LocalFrame> frame_;
  LocalFrame& GetFrame() const {
    DCHECK(frame_);
    return *frame_;
  }

  int last_request_sequence_;
  int last_processed_sequence_;
  double last_request_time_;

  TaskRunnerTimer<SpellCheckRequester> timer_to_process_queued_request_;

  Member<SpellCheckRequest> processing_request_;

  typedef HeapDeque<Member<SpellCheckRequest>> RequestQueue;
  RequestQueue request_queue_;
};

}  // namespace blink

#endif  // SpellCheckRequester_h

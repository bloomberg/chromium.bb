/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SourceBufferList_h
#define SourceBufferList_h

#include "core/dom/ExecutionContext.h"
#include "modules/EventTargetModules.h"
#include "platform/heap/Handle.h"

namespace blink {

class SourceBuffer;
class MediaElementEventQueue;

class SourceBufferList final : public EventTargetWithInlineData,
                               public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SourceBufferList);

 public:
  static SourceBufferList* Create(ExecutionContext* context,
                                  MediaElementEventQueue* async_event_queue) {
    return new SourceBufferList(context, async_event_queue);
  }
  ~SourceBufferList() override;

  unsigned length() const { return list_.size(); }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(addsourcebuffer);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(removesourcebuffer);

  SourceBuffer* item(unsigned index) const {
    return (index < list_.size()) ? list_[index].Get() : 0;
  }

  void Add(SourceBuffer*);
  void insert(size_t position, SourceBuffer*);
  void Remove(SourceBuffer*);
  size_t Find(SourceBuffer* buffer) { return list_.Find(buffer); }
  bool Contains(SourceBuffer* buffer) {
    return list_.Find(buffer) != kNotFound;
  }
  void Clear();

  // EventTarget interface
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ContextClient::GetExecutionContext();
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  SourceBufferList(ExecutionContext*, MediaElementEventQueue*);

  void ScheduleEvent(const AtomicString&);

  Member<MediaElementEventQueue> async_event_queue_;

  HeapVector<Member<SourceBuffer>> list_;
};

}  // namespace blink

#endif  // SourceBufferList_h

/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#ifndef MockResourceClient_h
#define MockResourceClient_h

#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceClient.h"

namespace blink {

class MockResourceClient : public GarbageCollectedFinalized<MockResourceClient>,
                           public ResourceClient {
  USING_PRE_FINALIZER(MockResourceClient, Dispose);
  USING_GARBAGE_COLLECTED_MIXIN(MockResourceClient);

 public:
  explicit MockResourceClient(Resource*);
  ~MockResourceClient() override;

  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "MockResourceClient"; }
  virtual bool NotifyFinishedCalled() const { return notify_finished_called_; }

  size_t EncodedSizeOnNotifyFinished() const {
    return encoded_size_on_notify_finished_;
  }

  virtual void RemoveAsClient();
  virtual void Dispose();

  void Trace(blink::Visitor*);

 protected:
  Member<Resource> resource_;
  bool notify_finished_called_;
  size_t encoded_size_on_notify_finished_;
};

}  // namespace blink

#endif  // MockResourceClient_h

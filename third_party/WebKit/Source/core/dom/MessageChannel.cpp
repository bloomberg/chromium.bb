/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#include "core/dom/MessageChannel.h"

#include "core/dom/MessagePort.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMessagePortChannel.h"

namespace blink {

static void CreateChannel(MessagePort* port1, MessagePort* port2) {
  std::unique_ptr<WebMessagePortChannel> channel1;
  std::unique_ptr<WebMessagePortChannel> channel2;
  Platform::Current()->CreateMessageChannel(&channel1, &channel2);
  DCHECK(channel1);
  DCHECK(channel2);

  // Now entangle the proxies with the appropriate local ports.
  port1->Entangle(std::move(channel2));
  port2->Entangle(std::move(channel1));
}

MessageChannel::MessageChannel(ExecutionContext* context)
    : port1_(MessagePort::Create(*context)),
      port2_(MessagePort::Create(*context)) {
  CreateChannel(port1_.Get(), port2_.Get());
}

DEFINE_TRACE(MessageChannel) {
  visitor->Trace(port1_);
  visitor->Trace(port2_);
}

}  // namespace blink

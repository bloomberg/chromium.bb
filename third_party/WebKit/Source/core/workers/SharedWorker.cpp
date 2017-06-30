/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "core/workers/SharedWorker.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessageChannel.h"
#include "core/dom/MessagePort.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/UseCounter.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/SharedWorkerRepositoryClient.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

inline SharedWorker::SharedWorker(ExecutionContext* context)
    : AbstractWorker(context), is_being_connected_(false) {}

SharedWorker* SharedWorker::Create(ExecutionContext* context,
                                   const String& url,
                                   const String& name,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  SECURITY_DCHECK(context->IsDocument());

  UseCounter::Count(context, WebFeature::kSharedWorkerStart);

  SharedWorker* worker = new SharedWorker(context);

  MessageChannel* channel = MessageChannel::Create(context);
  worker->port_ = channel->port1();
  std::unique_ptr<WebMessagePortChannel> remote_port =
      channel->port2()->Disentangle();
  DCHECK(remote_port);

  // We don't currently support nested workers, so workers can only be created
  // from documents.
  Document* document = ToDocument(context);
  if (!document->GetSecurityOrigin()->CanAccessSharedWorkers()) {
    exception_state.ThrowSecurityError(
        "Access to shared workers is denied to origin '" +
        document->GetSecurityOrigin()->ToString() + "'.");
    return nullptr;
  }

  KURL script_url = worker->ResolveURL(
      url, exception_state, WebURLRequest::kRequestContextSharedWorker);
  if (script_url.IsEmpty())
    return nullptr;

  if (document->GetFrame()->Client()->GetSharedWorkerRepositoryClient()) {
    document->GetFrame()->Client()->GetSharedWorkerRepositoryClient()->Connect(
        worker, std::move(remote_port), script_url, name);
  }

  return worker;
}

SharedWorker::~SharedWorker() {}

const AtomicString& SharedWorker::InterfaceName() const {
  return EventTargetNames::SharedWorker;
}

bool SharedWorker::HasPendingActivity() const {
  return is_being_connected_;
}

DEFINE_TRACE(SharedWorker) {
  visitor->Trace(port_);
  AbstractWorker::Trace(visitor);
  Supplementable<SharedWorker>::Trace(visitor);
}

}  // namespace blink

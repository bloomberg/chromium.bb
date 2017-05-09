/*
 * Copyright (c) 2014, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/timing/SharedWorkerPerformance.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/workers/SharedWorker.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

SharedWorkerPerformance::SharedWorkerPerformance(SharedWorker& shared_worker)
    : Supplement<SharedWorker>(shared_worker),
      time_origin_(MonotonicallyIncreasingTime()) {}

const char* SharedWorkerPerformance::SupplementName() {
  return "SharedWorkerPerformance";
}

SharedWorkerPerformance& SharedWorkerPerformance::From(
    SharedWorker& shared_worker) {
  SharedWorkerPerformance* supplement = static_cast<SharedWorkerPerformance*>(
      Supplement<SharedWorker>::From(shared_worker, SupplementName()));
  if (!supplement) {
    supplement = new SharedWorkerPerformance(shared_worker);
    ProvideTo(shared_worker, SupplementName(), supplement);
  }
  return *supplement;
}

double SharedWorkerPerformance::workerStart(ScriptState* script_state,
                                            SharedWorker& shared_worker) {
  return SharedWorkerPerformance::From(shared_worker)
      .GetWorkerStart(ExecutionContext::From(script_state), shared_worker);
}

double SharedWorkerPerformance::GetWorkerStart(ExecutionContext* context,
                                               SharedWorker&) const {
  DCHECK(context);
  DCHECK(context->IsDocument());
  Document* document = ToDocument(context);
  if (!document->Loader())
    return 0;

  double navigation_start = document->Loader()->GetTiming().NavigationStart();
  return time_origin_ - navigation_start;
}

}  // namespace blink

/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#include "InternalsSpeechSynthesis.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/testing/Internals.h"
#include "modules/speech/DOMWindowSpeechSynthesis.h"
#include "modules/speech/SpeechSynthesis.h"
#include "modules/speech/testing/PlatformSpeechSynthesizerMock.h"

namespace blink {

void InternalsSpeechSynthesis::enableMockSpeechSynthesizer(
    ScriptState* scriptState,
    Internals&,
    DOMWindow* window) {
  // TODO(dcheng): Performing a local/remote check is an anti-pattern. However,
  // it is necessary here since |window| is an argument passed from Javascript,
  // and the Window interface is accessible cross origin. The long-term fix is
  // to make the Internals object per-context, so |window| doesn't need to
  // passed as an argument.
  if (!window->isLocalDOMWindow())
    return;
  SpeechSynthesis* synthesis = DOMWindowSpeechSynthesis::speechSynthesis(
      scriptState, toLocalDOMWindow(*window));
  if (!synthesis)
    return;

  synthesis->setPlatformSynthesizer(
      PlatformSpeechSynthesizerMock::create(synthesis));
}

}  // namespace blink

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

#include "modules/webmidi/NavigatorWebMIDI.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIOptions.h"
#include "third_party/WebKit/public/mojom/feature_policy/feature_policy.mojom-blink.h"

namespace blink {
namespace {

const char kFeaturePolicyErrorMessage[] =
    "Midi has been disabled in this document by Feature Policy.";
const char kFeaturePolicyConsoleWarning[] =
    "Midi access has been blocked because of a Feature Policy applied to the "
    "current document. See https://goo.gl/EuHzyv for more details.";

}  // namespace

NavigatorWebMIDI::NavigatorWebMIDI(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

void NavigatorWebMIDI::Trace(blink::Visitor* visitor) {
  Supplement<Navigator>::Trace(visitor);
}

const char NavigatorWebMIDI::kSupplementName[] = "NavigatorWebMIDI";

NavigatorWebMIDI& NavigatorWebMIDI::From(Navigator& navigator) {
  NavigatorWebMIDI* supplement =
      Supplement<Navigator>::From<NavigatorWebMIDI>(navigator);
  if (!supplement) {
    supplement = new NavigatorWebMIDI(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

ScriptPromise NavigatorWebMIDI::requestMIDIAccess(ScriptState* script_state,
                                                  Navigator& navigator,
                                                  const MIDIOptions& options) {
  return NavigatorWebMIDI::From(navigator).requestMIDIAccess(script_state,
                                                             options);
}

ScriptPromise NavigatorWebMIDI::requestMIDIAccess(ScriptState* script_state,
                                                  const MIDIOptions& options) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kAbortError, "The frame is not working."));
  }

  Document& document = *ToDocument(ExecutionContext::From(script_state));
  if (options.hasSysex() && options.sysex()) {
    UseCounter::Count(
        document,
        WebFeature::kRequestMIDIAccessWithSysExOption_ObscuredByFootprinting);
    UseCounter::CountCrossOriginIframe(
        document,
        WebFeature::
            kRequestMIDIAccessIframeWithSysExOption_ObscuredByFootprinting);
  }
  UseCounter::CountCrossOriginIframe(
      document, WebFeature::kRequestMIDIAccessIframe_ObscuredByFootprinting);

  if (RuntimeEnabledFeatures::FeaturePolicyForPermissionsEnabled()) {
    if (!document.GetFrame()->IsFeatureEnabled(
            mojom::FeaturePolicyFeature::kMidiFeature)) {
      UseCounter::Count(document, WebFeature::kMidiDisabledByFeaturePolicy);
      document.AddConsoleMessage(
          ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                                 kFeaturePolicyConsoleWarning));
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kSecurityError, kFeaturePolicyErrorMessage));
    }
  } else {
    Deprecation::CountDeprecationFeaturePolicy(
        document, mojom::FeaturePolicyFeature::kMidiFeature);
  }

  return MIDIAccessInitializer::Start(script_state, options);
}

}  // namespace blink

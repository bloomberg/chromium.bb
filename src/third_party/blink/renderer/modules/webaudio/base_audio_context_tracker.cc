// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/base_audio_context_tracker.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_web_audio_agent.h"

namespace blink {

const char BaseAudioContextTracker::kSupplementName[] =
    "BaseAudioContextTracker";

void BaseAudioContextTracker::ProvideToPage(Page& page) {
  page.ProvideSupplement(MakeGarbageCollected<BaseAudioContextTracker>());
}

BaseAudioContextTracker::BaseAudioContextTracker()
    : inspector_agent_(nullptr) {}

void BaseAudioContextTracker::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspector_agent_);
  visitor->Trace(contexts_);
  Supplement<Page>::Trace(visitor);
}

void BaseAudioContextTracker::SetInspectorAgent(InspectorWebAudioAgent* agent) {
  inspector_agent_ = agent;
  if (!inspector_agent_)
    return;
  for (const auto& context : contexts_)
    inspector_agent_->DidCreateBaseAudioContext(context);
}

void BaseAudioContextTracker::DidCreateBaseAudioContext(
    BaseAudioContext* context) {
  DCHECK(!contexts_.Contains(context));

  contexts_.insert(context);
  if (inspector_agent_)
    inspector_agent_->DidCreateBaseAudioContext(context);
}

void BaseAudioContextTracker::DidDestroyBaseAudioContext(
    BaseAudioContext* context) {
  DCHECK(contexts_.Contains(context));

  contexts_.erase(context);
  if (inspector_agent_)
    inspector_agent_->DidDestroyBaseAudioContext(context);
}

void BaseAudioContextTracker::DidChangeBaseAudioContext(
    BaseAudioContext* context) {
  DCHECK(contexts_.Contains(context));

  if (inspector_agent_)
    inspector_agent_->DidChangeBaseAudioContext(context);
}

BaseAudioContext* BaseAudioContextTracker::GetContextById(String contextId) {
  for (const auto& context : contexts_) {
    if (context->Uuid() == contextId)
      return context;
  }

  return nullptr;
}

BaseAudioContextTracker* BaseAudioContextTracker::FromPage(Page* page) {
  return Supplement<Page>::From<BaseAudioContextTracker>(page);
}

BaseAudioContextTracker* BaseAudioContextTracker::FromDocument(
    const Document& document) {
  return BaseAudioContextTracker::FromPage(document.GetPage());
}

}  // namespace blink

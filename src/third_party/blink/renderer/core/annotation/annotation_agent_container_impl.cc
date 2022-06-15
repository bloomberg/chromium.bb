// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"

#include "base/callback.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

// static
const char AnnotationAgentContainerImpl::kSupplementName[] =
    "AnnotationAgentContainerImpl";

// static
AnnotationAgentContainerImpl* AnnotationAgentContainerImpl::From(
    Document& document) {
  if (!document.IsActive())
    return nullptr;

  AnnotationAgentContainerImpl* container =
      Supplement<Document>::From<AnnotationAgentContainerImpl>(document);
  if (!container) {
    container =
        MakeGarbageCollected<AnnotationAgentContainerImpl>(document, PassKey());
    Supplement<Document>::ProvideTo(document, container);
  }

  return container;
}

// static
void AnnotationAgentContainerImpl::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::AnnotationAgentContainer> receiver) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());
  Document& document = *frame->GetDocument();

  auto* container = AnnotationAgentContainerImpl::From(document);
  if (!container)
    return;

  container->Bind(std::move(receiver));
}

AnnotationAgentContainerImpl::AnnotationAgentContainerImpl(Document& document,
                                                           PassKey)
    : Supplement<Document>(document),
      receivers_(this, document.GetExecutionContext()) {}

void AnnotationAgentContainerImpl::Bind(
    mojo::PendingReceiver<mojom::blink::AnnotationAgentContainer> receiver) {
  receivers_.Add(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kInternalDefault));
}

void AnnotationAgentContainerImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receivers_);
  visitor->Trace(agents_);
  Supplement<Document>::Trace(visitor);
}

AnnotationAgentImpl* AnnotationAgentContainerImpl::CreateUnboundAgent(
    mojom::blink::AnnotationType type,
    AnnotationSelector& selector) {
  auto* agent_impl = MakeGarbageCollected<AnnotationAgentImpl>(
      *this, type, selector, PassKey());
  agents_.insert(agent_impl);

  // TODO(bokan): This is a stepping stone in refactoring the
  // TextFragmentHandler. When we replace it with a browser-side manager it may
  // make for a better API to have components register a handler for an
  // annotation type with AnnotationAgentContainer.
  // https://crbug.com/1303887.
  if (type == mojom::blink::AnnotationType::kSharedHighlight) {
    TextFragmentHandler::DidCreateTextFragment(*agent_impl,
                                               *GetSupplementable());
  }

  return agent_impl;
}

void AnnotationAgentContainerImpl::RemoveAgent(AnnotationAgentImpl& agent,
                                               AnnotationAgentImpl::PassKey) {
  DCHECK(!agent.IsAttached());
  auto itr = agents_.find(&agent);
  DCHECK_NE(itr, agents_.end());
  agents_.erase(itr);
}

HeapHashSet<Member<AnnotationAgentImpl>>
AnnotationAgentContainerImpl::GetAgentsOfType(
    mojom::blink::AnnotationType type) {
  HeapHashSet<Member<AnnotationAgentImpl>> agents_of_type;
  for (auto& agent : agents_) {
    if (agent->GetType() == type)
      agents_of_type.insert(agent);
  }

  return agents_of_type;
}

void AnnotationAgentContainerImpl::CreateAgent(
    mojo::PendingRemote<mojom::blink::AnnotationAgentHost> host_remote,
    mojo::PendingReceiver<mojom::blink::AnnotationAgent> agent_receiver,
    mojom::blink::AnnotationType type,
    const String& serialized_selector) {
  DCHECK(GetSupplementable());

  AnnotationSelector* selector =
      AnnotationSelector::Deserialize(serialized_selector);

  // If the selector was invalid, we should drop the bindings which the host
  // will see as a disconnect.
  // TODO(bokan): We could support more graceful fallback/error reporting by
  // calling an error method on the host.
  if (!selector)
    return;

  auto* agent_impl = MakeGarbageCollected<AnnotationAgentImpl>(
      *this, type, *selector, PassKey());
  agents_.insert(agent_impl);
  agent_impl->Bind(std::move(host_remote), std::move(agent_receiver));
  agent_impl->Attach();
}

}  // namespace blink

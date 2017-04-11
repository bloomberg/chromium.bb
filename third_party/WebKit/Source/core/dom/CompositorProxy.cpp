// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CompositorProxy.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/CompositorWorkerProxyClient.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include <algorithm>

namespace blink {

static const struct {
  const char* name;
  uint32_t property;
} kAllowedProperties[] = {
    {"opacity", CompositorMutableProperty::kOpacity},
    {"scrollleft", CompositorMutableProperty::kScrollLeft},
    {"scrolltop", CompositorMutableProperty::kScrollTop},
    {"transform", CompositorMutableProperty::kTransform},
};

static uint32_t CompositorMutablePropertyForName(const String& attribute_name) {
  for (const auto& mapping : kAllowedProperties) {
    if (DeprecatedEqualIgnoringCase(mapping.name, attribute_name))
      return mapping.property;
  }
  return CompositorMutableProperty::kNone;
}

static bool IsControlThread() {
  return !IsMainThread();
}

static bool IsCallingCompositorFrameCallback() {
  // TODO(sad): Check that the requestCompositorFrame callbacks are currently
  // being called.
  return true;
}

static void DecrementCompositorProxiedPropertiesForElement(
    uint64_t element_id,
    uint32_t compositor_mutable_properties) {
  DCHECK(IsMainThread());
  Node* node = DOMNodeIds::NodeForId(element_id);
  if (!node)
    return;
  Element* element = ToElement(node);
  element->DecrementCompositorProxiedProperties(compositor_mutable_properties);
}

static void IncrementCompositorProxiedPropertiesForElement(
    uint64_t element_id,
    uint32_t compositor_mutable_properties) {
  DCHECK(IsMainThread());
  Node* node = DOMNodeIds::NodeForId(element_id);
  if (!node)
    return;
  Element* element = ToElement(node);
  element->IncrementCompositorProxiedProperties(compositor_mutable_properties);
}

static bool RaiseExceptionIfMutationNotAllowed(
    ExceptionState& exception_state) {
  if (!IsControlThread()) {
    exception_state.ThrowDOMException(
        kNoModificationAllowedError,
        "Cannot mutate a proxy attribute from the main page.");
    return true;
  }
  if (!IsCallingCompositorFrameCallback()) {
    exception_state.ThrowDOMException(kNoModificationAllowedError,
                                      "Cannot mutate a proxy attribute outside "
                                      "of a requestCompositorFrame callback.");
    return true;
  }
  return false;
}

static uint32_t CompositorMutablePropertiesFromNames(
    const Vector<String>& attribute_array) {
  uint32_t properties = 0;
  for (const auto& attribute : attribute_array) {
    properties |= CompositorMutablePropertyForName(attribute);
  }
  return properties;
}

#if DCHECK_IS_ON()
static bool SanityCheckMutableProperties(uint32_t properties) {
  // Ensures that we only have bits set for valid mutable properties.
  uint32_t sanity_check_properties = properties;
  for (const auto& property : kAllowedProperties)
    sanity_check_properties &= ~static_cast<uint32_t>(property.property);
  return !sanity_check_properties;
}
#endif

CompositorProxy* CompositorProxy::Create(ExecutionContext* context,
                                         Element* element,
                                         const Vector<String>& attribute_array,
                                         ExceptionState& exception_state) {
  if (!context->IsDocument()) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConstruct(
        "CompositorProxy", "Can only be created from the main context."));
    return nullptr;
  }

  return new CompositorProxy(*element, attribute_array);
}

CompositorProxy* CompositorProxy::Create(
    ExecutionContext* context,
    uint64_t element_id,
    uint32_t compositor_mutable_properties) {
  if (context->IsCompositorWorkerGlobalScope()) {
    WorkerClients* clients = ToWorkerGlobalScope(context)->Clients();
    DCHECK(clients);
    CompositorWorkerProxyClient* client =
        CompositorWorkerProxyClient::From(clients);
    return new CompositorProxy(element_id, compositor_mutable_properties,
                               client->GetCompositorProxyClient());
  }

  return new CompositorProxy(element_id, compositor_mutable_properties);
}

CompositorProxy::CompositorProxy(uint64_t element_id,
                                 uint32_t compositor_mutable_properties)
    : element_id_(element_id),
      compositor_mutable_properties_(compositor_mutable_properties),
      client_(nullptr) {
  DCHECK(compositor_mutable_properties_);
#if DCHECK_IS_ON()
  DCHECK(SanityCheckMutableProperties(compositor_mutable_properties_));
#endif

  if (IsMainThread()) {
    IncrementCompositorProxiedPropertiesForElement(
        element_id_, compositor_mutable_properties_);
  } else {
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&IncrementCompositorProxiedPropertiesForElement,
                        element_id_, compositor_mutable_properties_));
  }
}

CompositorProxy::CompositorProxy(Element& element,
                                 const Vector<String>& attribute_array)
    : CompositorProxy(DOMNodeIds::IdForNode(&element),
                      CompositorMutablePropertiesFromNames(attribute_array)) {
  DCHECK(IsMainThread());
}

CompositorProxy::CompositorProxy(uint64_t element_id,
                                 uint32_t compositor_mutable_properties,
                                 CompositorProxyClient* client)
    : CompositorProxy(element_id, compositor_mutable_properties) {
  client_ = client;
  DCHECK(client_);
  DCHECK(IsControlThread());
  client_->RegisterCompositorProxy(this);
}

CompositorProxy::~CompositorProxy() {
  // We do not explicitly unregister from client here. The client has a weak
  // reference to us which gets collected on its own. This way we avoid using
  // a pre-finalizer.
  DisconnectInternal();
  DCHECK(!connected_);
}

DEFINE_TRACE(CompositorProxy) {
  visitor->Trace(client_);
}

bool CompositorProxy::supports(const String& attribute_name) const {
  return compositor_mutable_properties_ &
         CompositorMutablePropertyForName(attribute_name);
}

double CompositorProxy::opacity(ExceptionState& exception_state) const {
  if (RaiseExceptionIfMutationNotAllowed(exception_state))
    return 0.0;
  if (RaiseExceptionIfNotMutable(CompositorMutableProperty::kOpacity,
                                 exception_state))
    return 0.0;
  return state_->Opacity();
}

double CompositorProxy::scrollLeft(ExceptionState& exception_state) const {
  if (RaiseExceptionIfMutationNotAllowed(exception_state))
    return 0.0;
  if (RaiseExceptionIfNotMutable(CompositorMutableProperty::kScrollLeft,
                                 exception_state))
    return 0.0;
  return state_->ScrollLeft();
}

double CompositorProxy::scrollTop(ExceptionState& exception_state) const {
  if (RaiseExceptionIfMutationNotAllowed(exception_state))
    return 0.0;
  if (RaiseExceptionIfNotMutable(CompositorMutableProperty::kScrollTop,
                                 exception_state))
    return 0.0;
  return state_->ScrollTop();
}

DOMMatrix* CompositorProxy::transform(ExceptionState& exception_state) const {
  if (RaiseExceptionIfMutationNotAllowed(exception_state))
    return nullptr;
  if (RaiseExceptionIfNotMutable(CompositorMutableProperty::kTransform,
                                 exception_state))
    return nullptr;
  return DOMMatrix::Create(state_->Transform(), exception_state);
}

void CompositorProxy::setOpacity(double opacity,
                                 ExceptionState& exception_state) {
  if (RaiseExceptionIfMutationNotAllowed(exception_state))
    return;
  if (RaiseExceptionIfNotMutable(CompositorMutableProperty::kOpacity,
                                 exception_state))
    return;
  state_->SetOpacity(std::min(1., std::max(0., opacity)));
}

void CompositorProxy::setScrollLeft(double scroll_left,
                                    ExceptionState& exception_state) {
  if (RaiseExceptionIfMutationNotAllowed(exception_state))
    return;
  if (RaiseExceptionIfNotMutable(CompositorMutableProperty::kScrollLeft,
                                 exception_state))
    return;
  state_->SetScrollLeft(scroll_left);
}

void CompositorProxy::setScrollTop(double scroll_top,
                                   ExceptionState& exception_state) {
  if (RaiseExceptionIfMutationNotAllowed(exception_state))
    return;
  if (RaiseExceptionIfNotMutable(CompositorMutableProperty::kScrollTop,
                                 exception_state))
    return;
  state_->SetScrollTop(scroll_top);
}

void CompositorProxy::setTransform(DOMMatrix* transform,
                                   ExceptionState& exception_state) {
  if (RaiseExceptionIfMutationNotAllowed(exception_state))
    return;
  if (RaiseExceptionIfNotMutable(CompositorMutableProperty::kTransform,
                                 exception_state))
    return;
  state_->SetTransform(TransformationMatrix::ToSkMatrix44(transform->Matrix()));
}

void CompositorProxy::TakeCompositorMutableState(
    std::unique_ptr<CompositorMutableState> state) {
  state_ = std::move(state);
}

bool CompositorProxy::RaiseExceptionIfNotMutable(
    uint32_t property,
    ExceptionState& exception_state) const {
  if (!connected_)
    exception_state.ThrowDOMException(
        kNoModificationAllowedError,
        "Attempted to mutate attribute on a disconnected proxy.");
  else if (!(compositor_mutable_properties_ & property))
    exception_state.ThrowDOMException(
        kNoModificationAllowedError,
        "Attempted to mutate non-mutable attribute.");
  else if (!state_)
    exception_state.ThrowDOMException(
        kNoModificationAllowedError,
        "Attempted to mutate attribute on an uninitialized proxy.");

  return exception_state.HadException();
}

void CompositorProxy::disconnect() {
  DisconnectInternal();
  if (client_)
    client_->UnregisterCompositorProxy(this);
}

void CompositorProxy::DisconnectInternal() {
  if (!connected_)
    return;
  connected_ = false;
  if (IsMainThread()) {
    DecrementCompositorProxiedPropertiesForElement(
        element_id_, compositor_mutable_properties_);
  } else {
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&DecrementCompositorProxiedPropertiesForElement,
                        element_id_, compositor_mutable_properties_));
  }
}

}  // namespace blink

/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/inspector/InspectorDOMDebuggerAgent.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/public/V8EventListenerInfo.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"

namespace {

enum DOMBreakpointType {
    SubtreeModified = 0,
    AttributeModified,
    NodeRemoved,
    DOMBreakpointTypesCount
};

static const char listenerEventCategoryType[] = "listener:";
static const char instrumentationEventCategoryType[] = "instrumentation:";

const uint32_t inheritableDOMBreakpointTypesMask = (1 << SubtreeModified);
const int domBreakpointDerivedTypeShift = 16;

} // namespace

namespace blink {

static const char webglErrorFiredEventName[] = "webglErrorFired";
static const char webglWarningFiredEventName[] = "webglWarningFired";
static const char webglErrorNameProperty[] = "webglErrorName";

namespace DOMDebuggerAgentState {
static const char eventListenerBreakpoints[] = "eventListenerBreakpoints";
static const char eventTargetAny[] = "*";
static const char pauseOnAllXHRs[] = "pauseOnAllXHRs";
static const char xhrBreakpoints[] = "xhrBreakpoints";
static const char enabled[] = "enabled";
}

void InspectorDOMDebuggerAgent::eventListenersInfoForTarget(v8::Isolate* isolate, v8::Local<v8::Value> value, V8EventListenerInfoList& eventInformation)
{
    EventTarget* target = V8EventTarget::toImplWithTypeCheck(isolate, value);
    // We need to handle LocalDOMWindow specially, because LocalDOMWindow wrapper exists on prototype chain.
    if (!target)
        target = toDOMWindow(isolate, value);
    if (!target || !target->getExecutionContext())
        return;

    ExecutionContext* executionContext = target->getExecutionContext();

    // Nodes and their Listeners for the concerned event types (order is top to bottom)
    Vector<AtomicString> eventTypes = target->eventTypes();
    for (size_t j = 0; j < eventTypes.size(); ++j) {
        AtomicString& type = eventTypes[j];
        EventListenerVector* listeners = target->getEventListeners(type);
        if (!listeners)
            continue;
        for (size_t k = 0; k < listeners->size(); ++k) {
            EventListener* eventListener = listeners->at(k).listener;
            if (eventListener->type() != EventListener::JSEventListenerType)
                continue;
            V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(eventListener);
            v8::Local<v8::Context> context = toV8Context(executionContext, v8Listener->world());
            // Hide listeners from other contexts.
            if (context != isolate->GetCurrentContext())
                continue;
            // getListenerObject() may cause JS in the event attribute to get
            // compiled, potentially unsuccessfully.  In that case, the function
            // returns the empty handle without an exception.
            v8::Local<v8::Object> handler = v8Listener->getListenerObject(executionContext);
            if (handler.IsEmpty())
                continue;
            eventInformation.append(V8EventListenerInfo(type, listeners->at(k).useCapture, listeners->at(k).passive, handler));
        }
    }
}

InspectorDOMDebuggerAgent::InspectorDOMDebuggerAgent(v8::Isolate* isolate, InspectorDOMAgent* domAgent, V8InspectorSession* v8Session)
    : InspectorBaseAgent<InspectorDOMDebuggerAgent, protocol::Frontend::DOMDebugger>("DOMDebugger")
    , m_isolate(isolate)
    , m_domAgent(domAgent)
    , m_v8Session(v8Session)
{
}

InspectorDOMDebuggerAgent::~InspectorDOMDebuggerAgent()
{
}

DEFINE_TRACE(InspectorDOMDebuggerAgent)
{
    visitor->trace(m_domAgent);
    visitor->trace(m_domBreakpoints);
    InspectorBaseAgent::trace(visitor);
}

void InspectorDOMDebuggerAgent::disable(ErrorString*)
{
    setEnabled(false);
    m_domBreakpoints.clear();
    m_state->remove(DOMDebuggerAgentState::eventListenerBreakpoints);
    m_state->remove(DOMDebuggerAgentState::xhrBreakpoints);
    m_state->remove(DOMDebuggerAgentState::pauseOnAllXHRs);
}

void InspectorDOMDebuggerAgent::restore()
{
    if (m_state->booleanProperty(DOMDebuggerAgentState::enabled, false))
        m_instrumentingAgents->addInspectorDOMDebuggerAgent(this);
}

void InspectorDOMDebuggerAgent::setEventListenerBreakpoint(ErrorString* error, const String& eventName, const Maybe<String>& targetName)
{
    setBreakpoint(error, String(listenerEventCategoryType) + eventName, targetName.fromMaybe(String()));
}

void InspectorDOMDebuggerAgent::setInstrumentationBreakpoint(ErrorString* error, const String& eventName)
{
    setBreakpoint(error, String(instrumentationEventCategoryType) + eventName, String());
}

static protocol::DictionaryValue* ensurePropertyObject(protocol::DictionaryValue* object, const String& propertyName)
{
    protocol::Value* value = object->get(propertyName);
    if (value)
        return protocol::DictionaryValue::cast(value);

    OwnPtr<protocol::DictionaryValue> newResult = protocol::DictionaryValue::create();
    protocol::DictionaryValue* result = newResult.get();
    object->setObject(propertyName, newResult.release());
    return result;
}

protocol::DictionaryValue* InspectorDOMDebuggerAgent::eventListenerBreakpoints()
{
    protocol::DictionaryValue* breakpoints = m_state->getObject(DOMDebuggerAgentState::eventListenerBreakpoints);
    if (!breakpoints) {
        OwnPtr<protocol::DictionaryValue> newBreakpoints = protocol::DictionaryValue::create();
        breakpoints = newBreakpoints.get();
        m_state->setObject(DOMDebuggerAgentState::eventListenerBreakpoints, newBreakpoints.release());
    }
    return breakpoints;
}

protocol::DictionaryValue* InspectorDOMDebuggerAgent::xhrBreakpoints()
{
    protocol::DictionaryValue* breakpoints = m_state->getObject(DOMDebuggerAgentState::xhrBreakpoints);
    if (!breakpoints) {
        OwnPtr<protocol::DictionaryValue> newBreakpoints = protocol::DictionaryValue::create();
        breakpoints = newBreakpoints.get();
        m_state->setObject(DOMDebuggerAgentState::xhrBreakpoints, newBreakpoints.release());
    }
    return breakpoints;
}

void InspectorDOMDebuggerAgent::setBreakpoint(ErrorString* error, const String& eventName, const String& targetName)
{
    if (eventName.isEmpty()) {
        *error = "Event name is empty";
        return;
    }

    protocol::DictionaryValue* breakpointsByTarget = ensurePropertyObject(eventListenerBreakpoints(), eventName);
    if (targetName.isEmpty())
        breakpointsByTarget->setBoolean(DOMDebuggerAgentState::eventTargetAny, true);
    else
        breakpointsByTarget->setBoolean(targetName.lower(), true);
    didAddBreakpoint();
}

void InspectorDOMDebuggerAgent::removeEventListenerBreakpoint(ErrorString* error, const String& eventName, const Maybe<String>& targetName)
{
    removeBreakpoint(error, String(listenerEventCategoryType) + eventName, targetName.fromMaybe(String()));
}

void InspectorDOMDebuggerAgent::removeInstrumentationBreakpoint(ErrorString* error, const String& eventName)
{
    removeBreakpoint(error, String(instrumentationEventCategoryType) + eventName, String());
}

void InspectorDOMDebuggerAgent::removeBreakpoint(ErrorString* error, const String& eventName, const String& targetName)
{
    if (eventName.isEmpty()) {
        *error = "Event name is empty";
        return;
    }

    protocol::DictionaryValue* breakpointsByTarget = ensurePropertyObject(eventListenerBreakpoints(), eventName);
    if (targetName.isEmpty())
        breakpointsByTarget->remove(DOMDebuggerAgentState::eventTargetAny);
    else
        breakpointsByTarget->remove(targetName.lower());
    didRemoveBreakpoint();
}

void InspectorDOMDebuggerAgent::didInvalidateStyleAttr(Node* node)
{
    if (hasBreakpoint(node, AttributeModified)) {
        OwnPtr<protocol::DictionaryValue> eventData = protocol::DictionaryValue::create();
        descriptionForDOMEvent(node, AttributeModified, false, eventData.get());
        m_v8Session->breakProgram(protocol::Debugger::Paused::ReasonEnum::DOM, eventData.release());
    }
}

void InspectorDOMDebuggerAgent::didInsertDOMNode(Node* node)
{
    if (m_domBreakpoints.size()) {
        uint32_t mask = m_domBreakpoints.get(InspectorDOMAgent::innerParentNode(node));
        uint32_t inheritableTypesMask = (mask | (mask >> domBreakpointDerivedTypeShift)) & inheritableDOMBreakpointTypesMask;
        if (inheritableTypesMask)
            updateSubtreeBreakpoints(node, inheritableTypesMask, true);
    }
}

void InspectorDOMDebuggerAgent::didRemoveDOMNode(Node* node)
{
    if (m_domBreakpoints.size()) {
        // Remove subtree breakpoints.
        m_domBreakpoints.remove(node);
        HeapVector<Member<Node>> stack(1, InspectorDOMAgent::innerFirstChild(node));
        do {
            Node* node = stack.last();
            stack.removeLast();
            if (!node)
                continue;
            m_domBreakpoints.remove(node);
            stack.append(InspectorDOMAgent::innerFirstChild(node));
            stack.append(InspectorDOMAgent::innerNextSibling(node));
        } while (!stack.isEmpty());
    }
}

static int domTypeForName(ErrorString* errorString, const String& typeString)
{
    if (typeString == "subtree-modified")
        return SubtreeModified;
    if (typeString == "attribute-modified")
        return AttributeModified;
    if (typeString == "node-removed")
        return NodeRemoved;
    *errorString = String("Unknown DOM breakpoint type: " + typeString);
    return -1;
}

static String domTypeName(int type)
{
    switch (type) {
    case SubtreeModified: return "subtree-modified";
    case AttributeModified: return "attribute-modified";
    case NodeRemoved: return "node-removed";
    default: break;
    }
    return "";
}

void InspectorDOMDebuggerAgent::setDOMBreakpoint(ErrorString* errorString, int nodeId, const String& typeString)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    int type = domTypeForName(errorString, typeString);
    if (type == -1)
        return;

    uint32_t rootBit = 1 << type;
    m_domBreakpoints.set(node, m_domBreakpoints.get(node) | rootBit);
    if (rootBit & inheritableDOMBreakpointTypesMask) {
        for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
            updateSubtreeBreakpoints(child, rootBit, true);
    }
    didAddBreakpoint();
}

void InspectorDOMDebuggerAgent::removeDOMBreakpoint(ErrorString* errorString, int nodeId, const String& typeString)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;
    int type = domTypeForName(errorString, typeString);
    if (type == -1)
        return;

    uint32_t rootBit = 1 << type;
    uint32_t mask = m_domBreakpoints.get(node) & ~rootBit;
    if (mask)
        m_domBreakpoints.set(node, mask);
    else
        m_domBreakpoints.remove(node);

    if ((rootBit & inheritableDOMBreakpointTypesMask) && !(mask & (rootBit << domBreakpointDerivedTypeShift))) {
        for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
            updateSubtreeBreakpoints(child, rootBit, false);
    }
    didRemoveBreakpoint();
}

void InspectorDOMDebuggerAgent::getEventListeners(ErrorString* errorString, const String16& objectId, OwnPtr<protocol::Array<protocol::DOMDebugger::EventListener>>* listenersArray)
{
    v8::HandleScope handles(m_isolate);

    v8::Local<v8::Context> context;
    String16 objectGroup;
    v8::Local<v8::Value> value = m_v8Session->findObject(errorString, objectId, &context, &objectGroup);
    if (value.IsEmpty())
        return;
    v8::Context::Scope scope(context);
    *listenersArray = protocol::Array<protocol::DOMDebugger::EventListener>::create();
    eventListeners(context, value, objectGroup, listenersArray->get());
}

void InspectorDOMDebuggerAgent::eventListeners(v8::Local<v8::Context> context, v8::Local<v8::Value> object, const String16& objectGroup, protocol::Array<protocol::DOMDebugger::EventListener>* listenersArray)
{
    V8EventListenerInfoList eventInformation;
    InspectorDOMDebuggerAgent::eventListenersInfoForTarget(context->GetIsolate(), object, eventInformation);
    for (const auto& info : eventInformation) {
        if (!info.useCapture)
            continue;
        OwnPtr<protocol::DOMDebugger::EventListener> listenerObject = buildObjectForEventListener(context, info, objectGroup);
        if (listenerObject)
            listenersArray->addItem(listenerObject.release());
    }
    for (const auto& info : eventInformation) {
        if (info.useCapture)
            continue;
        OwnPtr<protocol::DOMDebugger::EventListener> listenerObject = buildObjectForEventListener(context, info, objectGroup);
        if (listenerObject)
            listenersArray->addItem(listenerObject.release());
    }
}

PassOwnPtr<protocol::DOMDebugger::EventListener> InspectorDOMDebuggerAgent::buildObjectForEventListener(v8::Local<v8::Context> context, const V8EventListenerInfo& info, const String16& objectGroupId)
{
    if (info.handler.IsEmpty())
        return nullptr;

    v8::Isolate* isolate = context->GetIsolate();
    v8::Local<v8::Function> function = eventListenerEffectiveFunction(isolate, info.handler);
    if (function.IsEmpty())
        return nullptr;

    String scriptId;
    int lineNumber;
    int columnNumber;
    getFunctionLocation(function, scriptId, lineNumber, columnNumber);

    OwnPtr<protocol::Debugger::Location> location = protocol::Debugger::Location::create()
        .setScriptId(scriptId)
        .setLineNumber(lineNumber).build();
    location->setColumnNumber(columnNumber);
    OwnPtr<protocol::DOMDebugger::EventListener> value = protocol::DOMDebugger::EventListener::create()
        .setType(info.eventType)
        .setUseCapture(info.useCapture)
        .setPassive(info.passive)
        .setLocation(location.release()).build();
    if (!objectGroupId.isEmpty()) {
        value->setHandler(m_v8Session->wrapObject(context, function, objectGroupId));
        value->setOriginalHandler(m_v8Session->wrapObject(context, info.handler, objectGroupId));
    }
    return value.release();
}

void InspectorDOMDebuggerAgent::allowNativeBreakpoint(const String& breakpointName, const String* targetName, bool sync)
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(breakpointName, targetName), sync);
}

void InspectorDOMDebuggerAgent::willInsertDOMNode(Node* parent)
{
    if (hasBreakpoint(parent, SubtreeModified)) {
        OwnPtr<protocol::DictionaryValue> eventData = protocol::DictionaryValue::create();
        descriptionForDOMEvent(parent, SubtreeModified, true, eventData.get());
        m_v8Session->breakProgram(protocol::Debugger::Paused::ReasonEnum::DOM, eventData.release());
    }
}

void InspectorDOMDebuggerAgent::willRemoveDOMNode(Node* node)
{
    Node* parentNode = InspectorDOMAgent::innerParentNode(node);
    if (hasBreakpoint(node, NodeRemoved)) {
        OwnPtr<protocol::DictionaryValue> eventData = protocol::DictionaryValue::create();
        descriptionForDOMEvent(node, NodeRemoved, false, eventData.get());
        m_v8Session->breakProgram(protocol::Debugger::Paused::ReasonEnum::DOM, eventData.release());
    } else if (parentNode && hasBreakpoint(parentNode, SubtreeModified)) {
        OwnPtr<protocol::DictionaryValue> eventData = protocol::DictionaryValue::create();
        descriptionForDOMEvent(node, SubtreeModified, false, eventData.get());
        m_v8Session->breakProgram(protocol::Debugger::Paused::ReasonEnum::DOM, eventData.release());
    }
    didRemoveDOMNode(node);
}

void InspectorDOMDebuggerAgent::willModifyDOMAttr(Element* element, const AtomicString&, const AtomicString&)
{
    if (hasBreakpoint(element, AttributeModified)) {
        OwnPtr<protocol::DictionaryValue> eventData = protocol::DictionaryValue::create();
        descriptionForDOMEvent(element, AttributeModified, false, eventData.get());
        m_v8Session->breakProgram(protocol::Debugger::Paused::ReasonEnum::DOM, eventData.release());
    }
}

void InspectorDOMDebuggerAgent::descriptionForDOMEvent(Node* target, int breakpointType, bool insertion, protocol::DictionaryValue* description)
{
    ASSERT(hasBreakpoint(target, breakpointType));

    Node* breakpointOwner = target;
    if ((1 << breakpointType) & inheritableDOMBreakpointTypesMask) {
        // For inheritable breakpoint types, target node isn't always the same as the node that owns a breakpoint.
        // Target node may be unknown to frontend, so we need to push it first.
        OwnPtr<protocol::Runtime::RemoteObject> targetNodeObject = m_domAgent->resolveNode(target, V8InspectorSession::backtraceObjectGroup);
        description->setValue("targetNode", targetNodeObject->serialize());

        // Find breakpoint owner node.
        if (!insertion)
            breakpointOwner = InspectorDOMAgent::innerParentNode(target);
        ASSERT(breakpointOwner);
        while (!(m_domBreakpoints.get(breakpointOwner) & (1 << breakpointType))) {
            Node* parentNode = InspectorDOMAgent::innerParentNode(breakpointOwner);
            if (!parentNode)
                break;
            breakpointOwner = parentNode;
        }

        if (breakpointType == SubtreeModified)
            description->setBoolean("insertion", insertion);
    }

    int breakpointOwnerNodeId = m_domAgent->boundNodeId(breakpointOwner);
    ASSERT(breakpointOwnerNodeId);
    description->setNumber("nodeId", breakpointOwnerNodeId);
    description->setString("type", domTypeName(breakpointType));
}

bool InspectorDOMDebuggerAgent::hasBreakpoint(Node* node, int type)
{
    if (!m_domAgent->enabled())
        return false;
    uint32_t rootBit = 1 << type;
    uint32_t derivedBit = rootBit << domBreakpointDerivedTypeShift;
    return m_domBreakpoints.get(node) & (rootBit | derivedBit);
}

void InspectorDOMDebuggerAgent::updateSubtreeBreakpoints(Node* node, uint32_t rootMask, bool set)
{
    uint32_t oldMask = m_domBreakpoints.get(node);
    uint32_t derivedMask = rootMask << domBreakpointDerivedTypeShift;
    uint32_t newMask = set ? oldMask | derivedMask : oldMask & ~derivedMask;
    if (newMask)
        m_domBreakpoints.set(node, newMask);
    else
        m_domBreakpoints.remove(node);

    uint32_t newRootMask = rootMask & ~newMask;
    if (!newRootMask)
        return;

    for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
        updateSubtreeBreakpoints(child, newRootMask, set);
}

void InspectorDOMDebuggerAgent::pauseOnNativeEventIfNeeded(PassOwnPtr<protocol::DictionaryValue> eventData, bool synchronous)
{
    if (!eventData)
        return;
    if (synchronous)
        m_v8Session->breakProgram(protocol::Debugger::Paused::ReasonEnum::EventListener, std::move(eventData));
    else
        m_v8Session->schedulePauseOnNextStatement(protocol::Debugger::Paused::ReasonEnum::EventListener, std::move(eventData));
}

PassOwnPtr<protocol::DictionaryValue> InspectorDOMDebuggerAgent::preparePauseOnNativeEventData(const String& eventName, const String* targetName)
{
    String fullEventName = (targetName ? listenerEventCategoryType : instrumentationEventCategoryType) + eventName;
    protocol::DictionaryValue* breakpoints = eventListenerBreakpoints();
    protocol::Value* value = breakpoints->get(fullEventName);
    if (!value)
        return nullptr;
    bool match = false;
    protocol::DictionaryValue* breakpointsByTarget = protocol::DictionaryValue::cast(value);
    breakpointsByTarget->getBoolean(DOMDebuggerAgentState::eventTargetAny, &match);
    if (!match && targetName)
        breakpointsByTarget->getBoolean(targetName->lower(), &match);
    if (!match)
        return nullptr;

    OwnPtr<protocol::DictionaryValue> eventData = protocol::DictionaryValue::create();
    eventData->setString("eventName", fullEventName);
    if (targetName)
        eventData->setString("targetName", *targetName);
    return eventData.release();
}

void InspectorDOMDebuggerAgent::didFireWebGLError(const String& errorName)
{
    OwnPtr<protocol::DictionaryValue> eventData = preparePauseOnNativeEventData(webglErrorFiredEventName, 0);
    if (!eventData)
        return;
    if (!errorName.isEmpty())
        eventData->setString(webglErrorNameProperty, errorName);
    pauseOnNativeEventIfNeeded(eventData.release(), false);
}

void InspectorDOMDebuggerAgent::didFireWebGLWarning()
{
    pauseOnNativeEventIfNeeded(preparePauseOnNativeEventData(webglWarningFiredEventName, 0), false);
}

void InspectorDOMDebuggerAgent::didFireWebGLErrorOrWarning(const String& message)
{
    if (message.findIgnoringCase("error") != WTF::kNotFound)
        didFireWebGLError(String());
    else
        didFireWebGLWarning();
}

void InspectorDOMDebuggerAgent::cancelNativeBreakpoint()
{
    m_v8Session->cancelPauseOnNextStatement();
}

void InspectorDOMDebuggerAgent::setXHRBreakpoint(ErrorString* errorString, const String& url)
{
    if (url.isEmpty())
        m_state->setBoolean(DOMDebuggerAgentState::pauseOnAllXHRs, true);
    else
        xhrBreakpoints()->setBoolean(url, true);
    didAddBreakpoint();
}

void InspectorDOMDebuggerAgent::removeXHRBreakpoint(ErrorString* errorString, const String& url)
{
    if (url.isEmpty())
        m_state->setBoolean(DOMDebuggerAgentState::pauseOnAllXHRs, false);
    else
        xhrBreakpoints()->remove(url);
    didRemoveBreakpoint();
}

void InspectorDOMDebuggerAgent::willSendXMLHttpRequest(const String& url)
{
    String breakpointURL;
    if (m_state->booleanProperty(DOMDebuggerAgentState::pauseOnAllXHRs, false))
        breakpointURL = "";
    else {
        protocol::DictionaryValue* breakpoints = xhrBreakpoints();
        for (size_t i = 0; i < breakpoints->size(); ++i) {
            auto breakpoint = breakpoints->at(i);
            if (url.contains(breakpoint.first)) {
                breakpointURL = breakpoint.first;
                break;
            }
        }
    }

    if (breakpointURL.isNull())
        return;

    OwnPtr<protocol::DictionaryValue> eventData = protocol::DictionaryValue::create();
    eventData->setString("breakpointURL", breakpointURL);
    eventData->setString("url", url);
    m_v8Session->breakProgram(protocol::Debugger::Paused::ReasonEnum::XHR, eventData.release());
}

void InspectorDOMDebuggerAgent::didAddBreakpoint()
{
    if (m_state->booleanProperty(DOMDebuggerAgentState::enabled, false))
        return;
    setEnabled(true);
}

void InspectorDOMDebuggerAgent::didRemoveBreakpoint()
{
    if (!m_domBreakpoints.isEmpty())
        return;
    if (eventListenerBreakpoints()->size())
        return;
    if (xhrBreakpoints()->size())
        return;
    if (m_state->booleanProperty(DOMDebuggerAgentState::pauseOnAllXHRs, false))
        return;
    setEnabled(false);
}

void InspectorDOMDebuggerAgent::setEnabled(bool enabled)
{
    if (enabled) {
        m_instrumentingAgents->addInspectorDOMDebuggerAgent(this);
        m_state->setBoolean(DOMDebuggerAgentState::enabled, true);
    } else {
        m_state->remove(DOMDebuggerAgentState::enabled);
        m_instrumentingAgents->removeInspectorDOMDebuggerAgent(this);
    }
}

void InspectorDOMDebuggerAgent::didCommitLoadForLocalFrame(LocalFrame*)
{
    m_domBreakpoints.clear();
}

} // namespace blink

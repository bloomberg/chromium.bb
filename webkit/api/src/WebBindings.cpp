/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "WebBindings.h"

#include "npruntime_priv.h"
#include "webkit/api/public/WebDragData.h"

#if USE(V8_BINDING)
#include "ChromiumDataObject.h"
#include "ClipboardChromium.h"
#include "EventNames.h"
#include "MouseEvent.h"
#include "NPV8Object.h"  // for PrivateIdentifier
#include "V8Helpers.h"
#include "V8Proxy.h"
#elif USE(JAVASCRIPTCORE_BINDINGS)
#include "bridge/c/c_utility.h"
#endif

#if USE(JAVASCRIPTCORE_BINDINGS)
using JSC::Bindings::PrivateIdentifier;
#endif

using namespace WebCore;

namespace WebKit {

void WebBindings::extractIdentifierData(const NPIdentifier& identifier, const NPUTF8*& string, int32_t& number, bool& isString)
{
    PrivateIdentifier* priv = static_cast<PrivateIdentifier*>(identifier);
    if (!priv) {
        isString = false;
        number = 0;
        return;
    }

    isString = priv->isString;
    if (isString)
        string = priv->value.string;
    else
        number = priv->value.number;
}

NPIdentifier WebBindings::getStringIdentifier(const NPUTF8* string)
{
    return NPN_GetStringIdentifier(string);
}

NPIdentifier WebBindings::getIntIdentifier(int32_t number)
{
    return NPN_GetIntIdentifier(number);
}

#if USE(V8)

static v8::Local<v8::Value> getEvent(const v8::Handle<v8::Context>& context)
{
    static v8::Persistent<v8::String> event(v8::Persistent<v8::String>::New(v8::String::NewSymbol("event")));
    return context->Global()->GetHiddenValue(event);
}

static bool getDragDataImpl(NPObject* npobj, int* eventId, WebDragData* data)
{
    if (npobj == NULL)
        return false;
    if (npobj->_class != npScriptObjectClass)
        return false;

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = v8::Context::GetEntered();
    if (context.IsEmpty())
        return false;

    // Get the current WebCore event.
    v8::Handle<v8::Value> currentEvent(getEvent(context));
    Event* event = V8DOMWrapper::convertToNativeEvent(currentEvent);
    if (event == NULL)
        return false;

    // Check that the given npobj is that event.
    V8NPObject* object = reinterpret_cast<V8NPObject*>(npobj);
    Event* given = V8DOMWrapper::convertToNativeEvent(object->v8Object);
    if (given != event)
        return false;

    // Check the execution frames are same origin.
    V8Proxy* current = V8Proxy::retrieve(V8Proxy::retrieveFrameForCurrentContext());
    Frame* frame = V8Proxy::retrieveFrame(context);
    if (!current || !current->canAccessFrame(frame, false))
        return false;

    const EventNames& names(eventNames());
    const AtomicString& eventType(event->type());

    enum DragTargetMouseEventId {
        DragEnterId = 1, DragOverId = 2, DragLeaveId = 3, DropId = 4
    };

    // The event type should be a drag event.
    if (eventType == names.dragenterEvent)
        *eventId = DragEnterId;
    else if (eventType == names.dragoverEvent)
        *eventId = DragOverId;
    else if (eventType == names.dragleaveEvent)
        *eventId = DragLeaveId;
    else if (eventType == names.dropEvent)
        *eventId = DropId;
    else
        return false;

    // Drag events are mouse events and should have a clipboard.
    MouseEvent* me = reinterpret_cast<MouseEvent*>(event);
    Clipboard* clipboard = me->clipboard();
    if (!clipboard)
        return false;

    // And that clipboard should be accessible by WebKit policy.
    ClipboardChromium* chrome = reinterpret_cast<ClipboardChromium*>(clipboard);
    HashSet<String> accessible(chrome->types());
    if (accessible.isEmpty())
        return false;

    RefPtr<ChromiumDataObject> dataObject(chrome->dataObject());
    if (dataObject && data)
        *data = WebDragData(dataObject);

    return dataObject != NULL;
}

#endif

bool WebBindings::getDragData(NPObject* event, int* eventId, WebDragData* data)
{
#if USE(V8)
    return getDragDataImpl(event, eventId, data);
#else
    // Not supported on other ports (JSC, etc).
    return false;
#endif
}

bool WebBindings::isDragEvent(NPObject* event)
{
    int eventId;
    return getDragData(event, &eventId, NULL);
}

} // namespace WebKit

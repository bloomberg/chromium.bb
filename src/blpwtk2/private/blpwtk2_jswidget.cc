/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_jswidget.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <third_party/blink/public/web/web_document.h>
#include <third_party/blink/public/web/web_dom_event.h>
#include <third_party/blink/public/web/web_local_frame.h>
#include <third_party/blink/public/web/web_plugin_container.h>
#include <third_party/blink/public/web/web_serialized_script_value.h>
#include <third_party/blink/renderer/platform/bindings/script_forbidden_scope.h>
#include <v8/include/v8.h>

namespace blpwtk2 {

static v8::Handle<v8::Object> ToV8(v8::Isolate* isolate, const blink::WebRect& rc)
{
    // TODO: make a template for this
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Maybe<bool> maybe = result->Set(context, v8::String::NewFromUtf8(isolate, "x").ToLocalChecked(), v8::Integer::New(isolate, rc.x));
    maybe = result->Set(context, v8::String::NewFromUtf8(isolate, "y").ToLocalChecked(), v8::Integer::New(isolate, rc.y));
    maybe = result->Set(context, v8::String::NewFromUtf8(isolate, "width").ToLocalChecked(), v8::Integer::New(isolate, rc.width));
    maybe = result->Set(context, v8::String::NewFromUtf8(isolate, "height").ToLocalChecked(), v8::Integer::New(isolate, rc.height));

    if (!maybe.IsJust() || !maybe.FromJust())
      LOG(WARNING) << "Failed to set x, y, width, height in jswidget ToV8().";
    return result;
}

JsWidget::JsWidget(blink::WebLocalFrame* frame)
: d_container(nullptr)
, d_frame(frame)
, d_hasParent(false)
, d_pendingVisible(false)
{
}

JsWidget::~JsWidget()
{
}

void JsWidget::DispatchEvent(const blink::WebDOMEvent& event)
{
    d_container->EnqueueEvent(event);
}

// blink::WebPlugin overrides

bool JsWidget::Initialize(blink::WebPluginContainer* container)
{
    d_container = container;
    v8::HandleScope handleScope(v8::Isolate::GetCurrent());
    v8::Context::Scope contextScope(d_frame->MainWorldScriptContext());
    blink::WebDOMEvent event = blink::WebDOMEvent::CreateCustomEvent(
        d_frame->ScriptIsolate(), "bbOnInitialize", false, false, v8::Null(v8::Isolate::GetCurrent()));
    DispatchEvent(event);
    return true;
}

void JsWidget::Destroy()
{
    if (d_container) {
        base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);

        d_container = nullptr;
    }
}

blink::WebPluginContainer* JsWidget::Container() const
{
    return d_container;
}

void JsWidget::UpdateGeometry(
    const blink::WebRect& windowRect, const blink::WebRect& clipRect,
    const blink::WebRect& unobscuredRect, bool isVisible)
{
    if (!d_hasParent) {
        return;
    }

    v8::Isolate* isolate = d_frame->ScriptIsolate();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context = d_frame->MainWorldScriptContext();
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::Object> detailObj = v8::Object::New(isolate);
    {
      // https://chromium.googlesource.com/chromium/src/+/45236fd563e9df53dc45579be1f3d0b4784885a2%5E%21/#F1
      // added ScriptForbiddenScope. It would block running scripts when
      // invoking JsWidget from Document::UpdateStyleAndLayout. we'd use
      // blink::ScriptForbiddenScope::AllowUserAgentScript to allow script.
      blink::ScriptForbiddenScope::AllowUserAgentScript alllow_script;
      v8::Maybe<bool> maybe = detailObj->Set(context, v8::String::NewFromUtf8(isolate, "windowRect").ToLocalChecked(),
                     ToV8(isolate, windowRect));
      maybe = detailObj->Set(context, v8::String::NewFromUtf8(isolate, "clipRect").ToLocalChecked(),
                     ToV8(isolate, clipRect));
      maybe = detailObj->Set(context, v8::String::NewFromUtf8(isolate, "unobscuredRect").ToLocalChecked(),
                     ToV8(isolate, unobscuredRect));
      maybe = detailObj->Set(context, v8::String::NewFromUtf8(isolate, "isVisible").ToLocalChecked(),
                     v8::Boolean::New(isolate, isVisible));
      maybe = detailObj->Set(context, v8::String::NewFromUtf8(isolate, "frameRect").ToLocalChecked(),
                     ToV8(isolate, windowRect));
      if (!maybe.IsJust() || !maybe.FromJust())
          LOG(WARNING) << "Failed to set geometry value for jswidget.";
    }

    blink::WebDOMEvent event = blink::WebDOMEvent::CreateCustomEvent(isolate, "bbOnUpdateGeometry", false, false,
                                                                     detailObj);
    DispatchEvent(event);
}

void JsWidget::UpdateVisibility(bool isVisible)
{
    if (!d_hasParent) {
        d_pendingVisible = isVisible;
        return;
    }

    v8::Isolate* isolate = d_frame->ScriptIsolate();

    v8::HandleScope handleScope(isolate);

    v8::Handle<v8::Context> context = d_frame->MainWorldScriptContext();
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::Object> detailObj = v8::Object::New(isolate);
    {
      // See the explanation of AllowUserAgentScript above
      blink::ScriptForbiddenScope::AllowUserAgentScript alllow_script;
      v8::Maybe<bool> maybe = detailObj->Set(context, v8::String::NewFromUtf8(isolate, "isVisible").ToLocalChecked(),
                     v8::Boolean::New(isolate, isVisible));
      if (!maybe.IsJust() || !maybe.FromJust())
          LOG(WARNING) << "Failed to update visibility for jswidget.";
    }

    blink::WebDOMEvent event = blink::WebDOMEvent::CreateCustomEvent(isolate, "bbOnUpdateVisibility", false, false,
                                                                     detailObj);
    DispatchEvent(event);
}

blink::WebInputEventResult JsWidget::HandleInputEvent(
    const blink::WebCoalescedInputEvent&, blink::WebCursorInfo&)
{
    return blink::WebInputEventResult::kNotHandled;
}

void JsWidget::AttachToLayout()
{
    d_hasParent = true;
    if (d_pendingVisible) {
        UpdateVisibility(d_pendingVisible);
        d_pendingVisible = false;
    }
}

void JsWidget::DetachFromLayout()
{
    d_hasParent = false;
}

blink::WebRect JsWidget::LocalToRootFrameRect(const blink::WebRect& localRect) const
{
    blink::WebPoint rect_corner0 = d_container->LocalToRootFramePoint(blink::WebPoint(localRect.x, localRect.y));
    blink::WebPoint rect_corner1 = d_container->LocalToRootFramePoint(blink::WebPoint(localRect.x + localRect.width, localRect.y + localRect.height));
    int x = rect_corner0.x <= rect_corner1.x ? rect_corner0.x : rect_corner1.x;
    int width = rect_corner0.x <= rect_corner1.x ? (rect_corner1.x - rect_corner0.x) : (rect_corner0.x - rect_corner1.x);
     int y = rect_corner0.y <= rect_corner1.y ? rect_corner0.y : rect_corner1.y;
    int height = rect_corner0.y <= rect_corner1.y ? (rect_corner1.y - rect_corner0.y) : (rect_corner0.y - rect_corner1.y);
    return blink::WebRect(x, y, width, height);
}


}  // close namespace blpwtk2

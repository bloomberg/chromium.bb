/*
 * Copyright (C) 2017 Bloomberg Finance L.P.
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

#include "third_party/blink/public/web/web_script_bindings.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"

#include <v8.h>

namespace blink {

v8::Local<v8::Context> WebScriptBindings::CreateWebScriptContext(
    const WebSecurityOrigin& security_origin)
{
    // The tasks carried out by this function are similar to those
    // implemented in 'blink::LocalWindowProxy::CreateContext()' and
    // 'blink::LocalWindowProxy::SetupWindowPrototypeChain()'.
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope hs(isolate);

    scoped_refptr<SecurityOrigin> blink_security_origin =
    SecurityOrigin::CreateFromString(security_origin.ToString());
    blink_security_origin->GrantUniversalAccess();

    LocalDOMWindow *window = LocalDOMWindow::Create();
    DocumentInit init = DocumentInit::Create().WithTypeFrom("text/html")
                                              .WithOriginToCommit(blink_security_origin);

    Document *document = window->InstallNewUnintializedDocument(init);

    DOMWrapperWorld& world = DOMWrapperWorld::MainWorld();

    v8::Local<v8::Context> context =
        V8ContextSnapshot::CreateContextFromSnapshot(
            isolate, world, nullptr, v8::Local<v8::Object>(), document, window);

    MakeGarbageCollected<ScriptState>(context, std::move(&world));

    const WrapperTypeInfo* wrapper_type_info = window->GetWrapperTypeInfo();

    v8::Local<v8::Object> global = context->Global();

    V8DOMWrapper::SetNativeInfo(
        isolate, global, wrapper_type_info, window);

    // The global object, aka window wrapper object.
    v8::Local<v8::Object> window_wrapper =
        global->GetPrototype().As<v8::Object>();

    if (world.DomDataStore().Set(
        isolate, window, wrapper_type_info, window_wrapper)) {
        V8DOMWrapper::SetNativeInfo(
            isolate, window_wrapper, wrapper_type_info, window);
    }
    SECURITY_CHECK(ToScriptWrappable(window_wrapper) == window);

    // The prototype object of Window interface.
    v8::Local<v8::Object> window_prototype =
        window_wrapper->GetPrototype().As<v8::Object>();
    CHECK(!window_prototype.IsEmpty());
    V8DOMWrapper::SetNativeInfo(
        isolate, window_prototype, wrapper_type_info, window);

    // The named properties object of Window interface.
    v8::Local<v8::Object> window_properties =
    window_prototype->GetPrototype().As<v8::Object>();
    CHECK(!window_properties.IsEmpty());
    V8DOMWrapper::SetNativeInfo(
        isolate, window_properties, wrapper_type_info, window);

    return hs.Escape(context);
}

void WebScriptBindings::DisposeWebScriptContext(v8::Local<v8::Context> context)
{
	ScriptState *scriptState = ScriptState::From(context);

	if (!scriptState) {
		return;
	}

    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    v8::Local<v8::Object> global = context->Global();

    V8DOMWrapper::ClearNativeInfo(isolate, global);
    scriptState->DetachGlobalObject();

	scriptState->DisposePerContextData();
}

} // namespace blink


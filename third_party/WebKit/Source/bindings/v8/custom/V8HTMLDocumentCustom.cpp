/*
 * Copyright (C) 2007, 2008, 2009 Google Inc. All rights reserved.
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
#include "V8HTMLDocument.h"

#include "HTMLNames.h"
#include "V8HTMLAllCollection.h"
#include "V8HTMLCollection.h"
#include "V8Node.h"
#include "V8Window.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8WindowShell.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/page/Frame.h"
#include "wtf/OwnArrayPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

// HTMLDocument ----------------------------------------------------------------

// Concatenates "args" to a string. If args is empty, returns empty string.
// Firefox/Safari/IE support non-standard arguments to document.write, ex:
//   document.write("a", "b", "c") --> document.write("abc")
//   document.write() --> document.write("")
static String writeHelperGetString(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    StringBuilder builder;
    for (int i = 0; i < args.Length(); ++i) {
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringArgument, args[i], String());
        builder.append(stringArgument);
    }
    return builder.toString();
}

void V8HTMLDocument::writeMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    HTMLDocument* htmlDocument = V8HTMLDocument::toNative(args.Holder());
    htmlDocument->write(writeHelperGetString(args), activeDOMWindow()->document());
}

void V8HTMLDocument::writelnMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    HTMLDocument* htmlDocument = V8HTMLDocument::toNative(args.Holder());
    htmlDocument->writeln(writeHelperGetString(args), activeDOMWindow()->document());
}

void V8HTMLDocument::openMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    HTMLDocument* htmlDocument = V8HTMLDocument::toNative(args.Holder());

    if (args.Length() > 2) {
        if (RefPtr<Frame> frame = htmlDocument->frame()) {
            // Fetch the global object for the frame.
            v8::Local<v8::Context> context = frame->script()->currentWorldContext();
            // Bail out if we cannot get the context.
            if (context.IsEmpty())
                return;
            v8::Local<v8::Object> global = context->Global();
            // Get the open property of the global object.
            v8::Local<v8::Value> function = global->Get(v8::String::NewSymbol("open"));
            // If the open property is not a function throw a type error.
            if (!function->IsFunction()) {
                throwTypeError("open is not a function", args.GetIsolate());
                return;
            }
            // Wrap up the arguments and call the function.
            OwnArrayPtr<v8::Local<v8::Value> > params = adoptArrayPtr(new v8::Local<v8::Value>[args.Length()]);
            for (int i = 0; i < args.Length(); i++)
                params[i] = args[i];

            v8SetReturnValue(args, frame->script()->callFunction(v8::Local<v8::Function>::Cast(function), global, args.Length(), params.get()));
            return;
        }
    }

    htmlDocument->open(activeDOMWindow()->document());
    v8SetReturnValue(args, args.Holder());
}

v8::Handle<v8::Object> wrap(HTMLDocument* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    v8::Handle<v8::Object> wrapper = V8HTMLDocument::createWrapper(impl, creationContext, isolate);
    if (wrapper.IsEmpty())
        return wrapper;
    if (!isolatedWorldForEnteredContext()) {
        if (Frame* frame = impl->frame())
            frame->script()->windowShell(mainThreadNormalWorld())->updateDocumentWrapper(wrapper);
    }
    return wrapper;
}

} // namespace WebCore

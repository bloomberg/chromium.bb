/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "V8Clipboard.h"

#include "HTMLNames.h"
#include "V8Node.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/Clipboard.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/html/HTMLImageElement.h"
#include "core/platform/graphics/IntPoint.h"

namespace WebCore {

void V8Clipboard::clearDataMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    Clipboard* clipboard = V8Clipboard::toNative(args.Holder());

    if (!args.Length()) {
        clipboard->clearAllData();
        return;
    }

    if (args.Length() != 1) {
        throwError(v8SyntaxError, "clearData: Invalid number of arguments", args.GetIsolate());
        return;
    }

    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, type, args[0]);
    clipboard->clearData(type);
}

void V8Clipboard::setDragImageMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    Clipboard* clipboard = V8Clipboard::toNative(args.Holder());

    if (!clipboard->isForDragAndDrop())
        return;

    if (args.Length() != 3) {
        throwError(v8SyntaxError, "setDragImage: Invalid number of arguments", args.GetIsolate());
        return;
    }

    int x = toInt32(args[1]);
    int y = toInt32(args[2]);

    Node* node = 0;
    if (V8Node::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())))
        node = V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0]));

    if (!node || !node->isElementNode()) {
        throwTypeError("setDragImageFromElement: Invalid first argument", args.GetIsolate());
        return;
    }

    if (toElement(node)->hasTagName(HTMLNames::imgTag) && !node->inDocument())
        clipboard->setDragImage(toHTMLImageElement(node)->cachedImage(), IntPoint(x, y));
    else
        clipboard->setDragImageElement(node, IntPoint(x, y));
}

} // namespace WebCore

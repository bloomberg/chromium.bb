/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2006 James G. Speth (speth@end.com)
 *  Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 *  Copyright (C) 2007, 2008, 2009 Google Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "V8Document.h"

#include "V8Location.h"
#include "bindings/v8/V8Binding.h"
#include "core/page/DOMWindow.h"
#include "core/page/Location.h"

namespace WebCore {

void V8Document::locationAttributeGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    Document* document = V8Document::toNative(info.Holder());
    if (!document->frame()) {
        v8SetReturnValueNull(info);
        return;
    }

    DOMWindow* window = document->domWindow();
    v8SetReturnValueFast(info, window->location(), document);
}

void V8Document::locationAttributeSetterCustom(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    Document* document = V8Document::toNative(info.Holder());
    if (!document->frame())
        return;

    DOMWindow* active = activeDOMWindow();
    if (!active)
        return;

    DOMWindow* first = firstDOMWindow();
    if (!first)
        return;

    DOMWindow* window = document->domWindow();
    if (Location* location = window->location())
        location->setHref(active, first, toWebCoreString(value));
}

} // namespace WebCore

/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/css/MediaQueryListListener.h"

#include "V8MediaQueryList.h"
#include "bindings/v8/ScriptFunctionCall.h"

namespace WebCore {

void MediaQueryListListener::queryChanged(ScriptState* state, MediaQueryList* query)
{
    ScriptCallback callback(state, m_value);
    v8::HandleScope handleScope(state->isolate());

    v8::Handle<v8::Context> context = state->context();
    if (context.IsEmpty())
        return; // JS may not be enabled.

    v8::Context::Scope scope(context);
    callback.appendArgument(ScriptValue(toV8(query, v8::Handle<v8::Object>(), context->GetIsolate()), context->GetIsolate()));
    callback.call();
}

}

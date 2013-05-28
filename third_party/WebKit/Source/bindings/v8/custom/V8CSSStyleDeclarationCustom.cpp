/*
 * Copyright (C) 2007-2011 Google Inc. All rights reserved.
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
#include "V8CSSStyleDeclaration.h"

#include "CSSPropertyNames.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/CSSValue.h"
#include "core/dom/EventTarget.h"
#include "core/page/RuntimeCSSEnabled.h"

#include "bindings/v8/V8Binding.h"

#include "wtf/ASCIICType.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringConcatenate.h"

using namespace WTF;
using namespace std;

namespace WebCore {



v8::Handle<v8::Array> V8CSSStyleDeclaration::namedPropertyEnumerator(const v8::AccessorInfo& info)
{
    typedef Vector<String, numCSSProperties - 1> PreAllocatedPropertyVector;
    DEFINE_STATIC_LOCAL(PreAllocatedPropertyVector, propertyNames, ());
    static unsigned propertyNamesLength = 0;

    if (propertyNames.isEmpty()) {
        for (int id = firstCSSProperty; id <= lastCSSProperty; ++id) {
            CSSPropertyID propertyId = static_cast<CSSPropertyID>(id);
            if (RuntimeCSSEnabled::isCSSPropertyEnabled(propertyId))
                propertyNames.append(getJSPropertyName(propertyId));
        }
        sort(propertyNames.begin(), propertyNames.end(), codePointCompareLessThan);
        propertyNamesLength = propertyNames.size();
    }

    v8::Handle<v8::Array> properties = v8::Array::New(propertyNamesLength);
    for (unsigned i = 0; i < propertyNamesLength; ++i) {
        String key = propertyNames.at(i);
        ASSERT(!key.isNull());
        properties->Set(v8Integer(i, info.GetIsolate()), v8String(key, info.GetIsolate()));
    }

    return properties;
}

v8::Handle<v8::Integer> V8CSSStyleDeclaration::namedPropertyQuery(v8::Local<v8::String> v8Name, const v8::AccessorInfo& info)
{
    // NOTE: cssPropertyInfo lookups incur several mallocs.
    // Successful lookups have the same cost the first time, but are cached.
    String propertyName = toWebCoreString(v8Name);
    if (CSSStyleDeclaration::cssPropertyInfo(propertyName))
        return v8Integer(0, info.GetIsolate());

    return v8::Handle<v8::Integer>();
}

v8::Handle<v8::Value> V8CSSStyleDeclaration::namedPropertySetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    CSSStyleDeclaration* imp = V8CSSStyleDeclaration::toNative(info.Holder());
    String propertyName = toWebCoreString(name);
    CSSPropertyInfo* propInfo = CSSStyleDeclaration::cssPropertyInfo(propertyName);
    if (!propInfo)
        return v8Undefined();

    String propertyValue = toWebCoreStringWithNullCheck(value);
    if (propInfo->hadPixelOrPosPrefix)
        propertyValue.append("px");

    ExceptionCode ec = 0;
    imp->setPropertyInternal(static_cast<CSSPropertyID>(propInfo->propID), propertyValue, false, ec);

    if (ec)
        setDOMException(ec, info.GetIsolate());

    return value;
}

} // namespace WebCore

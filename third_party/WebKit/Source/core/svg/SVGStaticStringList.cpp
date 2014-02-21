/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "core/svg/SVGStaticStringList.h"

namespace WebCore {

SVGStaticStringList::SVGStaticStringList(SVGElement* contextElement, const QualifiedName& attributeName)
    : NewSVGAnimatedPropertyBase(AnimatedStringList, contextElement, attributeName)
    , m_value(SVGStringList::create())
{
    ASSERT(contextElement);
}

SVGStaticStringList::~SVGStaticStringList()
{
}

NewSVGPropertyBase* SVGStaticStringList::currentValueBase()
{
    return m_value.get();
}

void SVGStaticStringList::animationStarted()
{
    ASSERT_NOT_REACHED();
}

PassRefPtr<NewSVGPropertyBase> SVGStaticStringList::createAnimatedValue()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void SVGStaticStringList::setAnimatedValue(PassRefPtr<NewSVGPropertyBase>)
{
    ASSERT_NOT_REACHED();
}

void SVGStaticStringList::animationEnded()
{
    ASSERT_NOT_REACHED();
}

void SVGStaticStringList::animValWillChange()
{
    ASSERT_NOT_REACHED();
}

void SVGStaticStringList::animValDidChange()
{
    ASSERT_NOT_REACHED();
}

bool SVGStaticStringList::needsSynchronizeAttribute()
{
    return m_tearOff;
}

SVGStringListTearOff* SVGStaticStringList::tearOff()
{
    if (!m_tearOff)
        m_tearOff = SVGStringListTearOff::create(m_value, contextElement(), PropertyIsNotAnimVal, attributeName());

    return m_tearOff.get();
}

void SVGStaticStringList::setBaseValueAsString(const String& value, SVGParsingError& parseError)
{
    TrackExceptionState es;

    m_value->setValueAsString(value, es);

    if (es.hadException())
        parseError = ParsingAttributeFailedError;
}

}

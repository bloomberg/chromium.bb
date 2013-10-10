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
#include "platform/LocalizedStrings.h"

#include "platform/NotImplemented.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "wtf/text/WTFString.h"

using WebKit::WebLocalizedString;
using WebKit::WebString;

namespace WebCore {

static String query(WebLocalizedString::Name name)
{
    return WebKit::Platform::current()->queryLocalizedString(name);
}

static String query(WebLocalizedString::Name name, const WebString& parameter)
{
    return WebKit::Platform::current()->queryLocalizedString(name, parameter);
}

static String query(WebLocalizedString::Name name, const WebString& parameter1, const WebString& parameter2)
{
    return WebKit::Platform::current()->queryLocalizedString(name, parameter1, parameter2);
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
String AXAMPMFieldText()
{
    return query(WebLocalizedString::AXAMPMFieldText);
}

String AXDayOfMonthFieldText()
{
    return query(WebLocalizedString::AXDayOfMonthFieldText);
}

String AXDateTimeFieldEmptyValueText()
{
    return query(WebLocalizedString::AXDateTimeFieldEmptyValueText);
}

String AXHourFieldText()
{
    return query(WebLocalizedString::AXHourFieldText);
}

String AXMillisecondFieldText()
{
    return query(WebLocalizedString::AXMillisecondFieldText);
}

String AXMinuteFieldText()
{
    return query(WebLocalizedString::AXMinuteFieldText);
}

String AXMonthFieldText()
{
    return query(WebLocalizedString::AXMonthFieldText);
}

String AXSecondFieldText()
{
    return query(WebLocalizedString::AXSecondFieldText);
}

String AXWeekOfYearFieldText()
{
    return query(WebLocalizedString::AXWeekOfYearFieldText);
}

String AXYearFieldText()
{
    return query(WebLocalizedString::AXYearFieldText);
}
#endif

String missingPluginText()
{
    return query(WebLocalizedString::MissingPluginText);
}

String blockedPluginByContentSecurityPolicyText()
{
    return query(WebLocalizedString::BlockedPluginText);
}

} // namespace WebCore

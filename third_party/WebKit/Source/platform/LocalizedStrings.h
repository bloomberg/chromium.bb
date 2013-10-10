/*
 * Copyright (C) 2003, 2006, 2009, 2011, 2012, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2010 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LocalizedStrings_h
#define LocalizedStrings_h

#include "platform/PlatformExport.h"
#include "public/platform/WebLocalizedString.h"
#include "wtf/Forward.h"

namespace WebCore {

// FIXME: Use Locale::queryString instead of the following functions.

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
PLATFORM_EXPORT String AXAMPMFieldText();
PLATFORM_EXPORT String AXDayOfMonthFieldText();
PLATFORM_EXPORT String AXDateTimeFieldEmptyValueText();
PLATFORM_EXPORT String AXHourFieldText();
PLATFORM_EXPORT String AXMillisecondFieldText();
PLATFORM_EXPORT String AXMinuteFieldText();
PLATFORM_EXPORT String AXMonthFieldText();
PLATFORM_EXPORT String AXSecondFieldText();
PLATFORM_EXPORT String AXWeekOfYearFieldText();
PLATFORM_EXPORT String AXYearFieldText();
#endif

PLATFORM_EXPORT String missingPluginText();
PLATFORM_EXPORT String blockedPluginByContentSecurityPolicyText();

} // namespace WebCore

#endif // LocalizedStrings_h

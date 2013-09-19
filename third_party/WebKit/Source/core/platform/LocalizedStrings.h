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

#include "public/platform/WebLocalizedString.h"
#include "wtf/Forward.h"

namespace WebCore {

    class IntSize;

    // FIXME: Use Locale::queryString instead of the following functions.

    String inputElementAltText();
    String resetButtonDefaultLabel();
    String searchableIndexIntroduction();
    String submitButtonDefaultLabel();
    String fileButtonChooseFileLabel();
    String fileButtonChooseMultipleFilesLabel();
    String fileButtonNoFileSelectedLabel();
    String fileButtonNoFilesSelectedLabel();
    String defaultDetailsSummaryText();

    String searchMenuNoRecentSearchesText();
    String searchMenuRecentSearchesText();
    String searchMenuClearRecentSearchesText();

    String AXWebAreaText();
    String AXLinkText();
    String AXListMarkerText();
    String AXImageMapText();
    String AXHeadingText();
    String AXFileUploadButtonText();
    String AXButtonActionVerb();
    String AXRadioButtonActionVerb();
    String AXTextFieldActionVerb();
    String AXCheckedCheckBoxActionVerb();
    String AXUncheckedCheckBoxActionVerb();
    String AXMenuListActionVerb();
    String AXMenuListPopupActionVerb();
    String AXLinkActionVerb();

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    String AXAMPMFieldText();
    String AXDayOfMonthFieldText();
    String AXDateTimeFieldEmptyValueText();
    String AXHourFieldText();
    String AXMillisecondFieldText();
    String AXMinuteFieldText();
    String AXMonthFieldText();
    String AXSecondFieldText();
    String AXWeekOfYearFieldText();
    String AXYearFieldText();

    // placeholderForDayOfMonthField() returns localized placeholder text, e.g.
    // "dd", for date field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead "--".
    String placeholderForDayOfMonthField();

    // placeholderForfMonthField() returns localized placeholder text, e.g.
    // "mm", for month field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead "--".
    String placeholderForMonthField();

    // placeholderForYearField() returns localized placeholder text, e.g.
    // "yyyy", for year field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead "----".
    String placeholderForYearField();
#endif
    // weekFormatInLDML() returns week and year format in LDML, Unicode
    // technical standard 35, Locale Data Markup Language, e.g. "'Week' ww, yyyy"
    String weekFormatInLDML();

    String missingPluginText();
    String blockedPluginByContentSecurityPolicyText();

    String multipleFileUploadText(unsigned numberOfFiles);

    String imageTitle(const String& filename, const IntSize& size);

    String localizedMediaControlElementString(const String&);
    String localizedMediaControlElementHelpText(const String&);
    String localizedMediaTimeDescription(float);

    String validationMessageValueMissingText();
    String validationMessageValueMissingForCheckboxText();
    String validationMessageValueMissingForFileText();
    String validationMessageValueMissingForMultipleFileText();
    String validationMessageValueMissingForRadioText();
    String validationMessageValueMissingForSelectText();
    String validationMessageTypeMismatchText();
    String validationMessageTypeMismatchForURLText();
    String validationMessagePatternMismatchText();
    String validationMessageTooLongText(int valueLength, int maxLength);
    String validationMessageRangeUnderflowText(const String& minimum);
    String validationMessageRangeOverflowText(const String& maximum);
    String validationMessageBadInputForNumberText();
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    String validationMessageBadInputForDateTimeText();
#endif

    String clickToExitFullScreenText();

    String textTrackSubtitlesText();
    String textTrackOffText();
    String textTrackNoLabelText();

} // namespace WebCore

#endif // LocalizedStrings_h

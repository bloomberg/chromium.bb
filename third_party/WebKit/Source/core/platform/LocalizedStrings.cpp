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
#include "core/platform/LocalizedStrings.h"

#include "core/platform/NotImplemented.h"
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

String searchableIndexIntroduction()
{
    return query(WebLocalizedString::SearchableIndexIntroduction);
}

String defaultDetailsSummaryText()
{
    return query(WebLocalizedString::DetailsLabel);
}

String searchMenuNoRecentSearchesText()
{
    return query(WebLocalizedString::SearchMenuNoRecentSearchesText);
}
String searchMenuRecentSearchesText()
{
    return query(WebLocalizedString::SearchMenuRecentSearchesText);
}

String searchMenuClearRecentSearchesText()
{
    return query(WebLocalizedString::SearchMenuClearRecentSearchesText);
}

String AXWebAreaText()
{
    return query(WebLocalizedString::AXWebAreaText);
}

String AXLinkText()
{
    return query(WebLocalizedString::AXLinkText);
}

String AXListMarkerText()
{
    return query(WebLocalizedString::AXListMarkerText);
}

String AXImageMapText()
{
    return query(WebLocalizedString::AXImageMapText);
}

String AXHeadingText()
{
    return query(WebLocalizedString::AXHeadingText);
}

String AXButtonActionVerb()
{
    return query(WebLocalizedString::AXButtonActionVerb);
}

String AXRadioButtonActionVerb()
{
    return query(WebLocalizedString::AXRadioButtonActionVerb);
}

String AXTextFieldActionVerb()
{
    return query(WebLocalizedString::AXTextFieldActionVerb);
}

String AXCheckedCheckBoxActionVerb()
{
    return query(WebLocalizedString::AXCheckedCheckBoxActionVerb);
}

String AXUncheckedCheckBoxActionVerb()
{
    return query(WebLocalizedString::AXUncheckedCheckBoxActionVerb);
}

String AXLinkActionVerb()
{
    return query(WebLocalizedString::AXLinkActionVerb);
}

String AXMenuListPopupActionVerb()
{
    return String();
}

String AXMenuListActionVerb()
{
    return String();
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
    notImplemented();
    return String();
}

String textTrackClosedCaptionsText() { return String(); }
String textTrackSubtitlesText() { return String(); }
String textTrackOffText() { return String(); }
String textTrackNoLabelText() { return String(); }

String localizedMediaControlElementString(const String& name)
{
    if (name == "AudioElement")
        return query(WebLocalizedString::AXMediaAudioElement);
    if (name == "VideoElement")
        return query(WebLocalizedString::AXMediaVideoElement);
    if (name == "MuteButton")
        return query(WebLocalizedString::AXMediaMuteButton);
    if (name == "UnMuteButton")
        return query(WebLocalizedString::AXMediaUnMuteButton);
    if (name == "PlayButton")
        return query(WebLocalizedString::AXMediaPlayButton);
    if (name == "PauseButton")
        return query(WebLocalizedString::AXMediaPauseButton);
    if (name == "Slider")
        return query(WebLocalizedString::AXMediaSlider);
    if (name == "SliderThumb")
        return query(WebLocalizedString::AXMediaSliderThumb);
    if (name == "RewindButton")
        return query(WebLocalizedString::AXMediaRewindButton);
    if (name == "ReturnToRealtimeButton")
        return query(WebLocalizedString::AXMediaReturnToRealTime);
    if (name == "CurrentTimeDisplay")
        return query(WebLocalizedString::AXMediaCurrentTimeDisplay);
    if (name == "TimeRemainingDisplay")
        return query(WebLocalizedString::AXMediaTimeRemainingDisplay);
    if (name == "StatusDisplay")
        return query(WebLocalizedString::AXMediaStatusDisplay);
    if (name == "EnterFullscreenButton")
        return query(WebLocalizedString::AXMediaEnterFullscreenButton);
    if (name == "ExitFullscreenButton")
        return query(WebLocalizedString::AXMediaExitFullscreenButton);
    if (name == "SeekForwardButton")
        return query(WebLocalizedString::AXMediaSeekForwardButton);
    if (name == "SeekBackButton")
        return query(WebLocalizedString::AXMediaSeekBackButton);
    if (name == "ShowClosedCaptionsButton")
        return query(WebLocalizedString::AXMediaShowClosedCaptionsButton);
    if (name == "HideClosedCaptionsButton")
        return query(WebLocalizedString::AXMediaHideClosedCaptionsButton);

    // FIXME: the ControlsPanel container should never be visible in the accessibility hierarchy.
    if (name == "ControlsPanel")
        return query(WebLocalizedString::AXMediaDefault);

    ASSERT_NOT_REACHED();
    return query(WebLocalizedString::AXMediaDefault);
}

String localizedMediaControlElementHelpText(const String& name)
{
    if (name == "AudioElement")
        return query(WebLocalizedString::AXMediaAudioElementHelp);
    if (name == "VideoElement")
        return query(WebLocalizedString::AXMediaVideoElementHelp);
    if (name == "MuteButton")
        return query(WebLocalizedString::AXMediaMuteButtonHelp);
    if (name == "UnMuteButton")
        return query(WebLocalizedString::AXMediaUnMuteButtonHelp);
    if (name == "PlayButton")
        return query(WebLocalizedString::AXMediaPlayButtonHelp);
    if (name == "PauseButton")
        return query(WebLocalizedString::AXMediaPauseButtonHelp);
    if (name == "Slider")
        return query(WebLocalizedString::AXMediaSliderHelp);
    if (name == "SliderThumb")
        return query(WebLocalizedString::AXMediaSliderThumbHelp);
    if (name == "RewindButton")
        return query(WebLocalizedString::AXMediaRewindButtonHelp);
    if (name == "ReturnToRealtimeButton")
        return query(WebLocalizedString::AXMediaReturnToRealTimeHelp);
    if (name == "CurrentTimeDisplay")
        return query(WebLocalizedString::AXMediaCurrentTimeDisplayHelp);
    if (name == "TimeRemainingDisplay")
        return query(WebLocalizedString::AXMediaTimeRemainingDisplayHelp);
    if (name == "StatusDisplay")
        return query(WebLocalizedString::AXMediaStatusDisplayHelp);
    if (name == "EnterFullscreenButton")
        return query(WebLocalizedString::AXMediaEnterFullscreenButtonHelp);
    if (name == "ExitFullscreenButton")
        return query(WebLocalizedString::AXMediaExitFullscreenButtonHelp);
    if (name == "SeekForwardButton")
        return query(WebLocalizedString::AXMediaSeekForwardButtonHelp);
    if (name == "SeekBackButton")
        return query(WebLocalizedString::AXMediaSeekBackButtonHelp);
    if (name == "ShowClosedCaptionsButton")
        return query(WebLocalizedString::AXMediaShowClosedCaptionsButtonHelp);
    if (name == "HideClosedCaptionsButton")
        return query(WebLocalizedString::AXMediaHideClosedCaptionsButtonHelp);

    ASSERT_NOT_REACHED();
    return query(WebLocalizedString::AXMediaDefault);
}

String localizedMediaTimeDescription(float /*time*/)
{
    // FIXME: to be fixed.
    return String();
}

} // namespace WebCore

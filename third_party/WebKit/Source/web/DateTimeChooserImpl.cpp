/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "web/DateTimeChooserImpl.h"

#include "core/InputTypeNames.h"
#include "core/frame/FrameView.h"
#include "core/rendering/RenderTheme.h"
#include "platform/DateComponents.h"
#include "platform/DateTimeChooserClient.h"
#include "platform/Language.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/Platform.h"
#include "web/ChromeClientImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

DateTimeChooserImpl::DateTimeChooserImpl(ChromeClientImpl* chromeClient, DateTimeChooserClient* client, const DateTimeChooserParameters& parameters)
    : m_chromeClient(chromeClient)
    , m_client(client)
    , m_popup(0)
    , m_parameters(parameters)
    , m_locale(Locale::create(parameters.locale))
{
    ASSERT(m_chromeClient);
    ASSERT(m_client);
    m_popup = m_chromeClient->openPagePopup(this, m_parameters.anchorRectInRootView);
}

PassRefPtr<DateTimeChooserImpl> DateTimeChooserImpl::create(ChromeClientImpl* chromeClient, DateTimeChooserClient* client, const DateTimeChooserParameters& parameters)
{
    return adoptRef(new DateTimeChooserImpl(chromeClient, client, parameters));
}

DateTimeChooserImpl::~DateTimeChooserImpl()
{
}

void DateTimeChooserImpl::endChooser()
{
    if (!m_popup)
        return;
    m_chromeClient->closePagePopup(m_popup);
}

IntSize DateTimeChooserImpl::contentSize()
{
    return IntSize(0, 0);
}

static String valueToDateTimeString(double value, AtomicString type)
{
    DateComponents components;
    if (type == InputTypeNames::date)
        components.setMillisecondsSinceEpochForDate(value);
    else if (type == InputTypeNames::datetime_local)
        components.setMillisecondsSinceEpochForDateTimeLocal(value);
    else if (type == InputTypeNames::month)
        components.setMonthsSinceEpoch(value);
    else if (type == InputTypeNames::time)
        components.setMillisecondsSinceMidnight(value);
    else if (type == InputTypeNames::week)
        components.setMillisecondsSinceEpochForWeek(value);
    else
        ASSERT_NOT_REACHED();
    return components.type() == DateComponents::Invalid ? String() : components.toString();
}

void DateTimeChooserImpl::writeDocument(SharedBuffer* data)
{
    String stepString = String::number(m_parameters.step);
    String stepBaseString = String::number(m_parameters.stepBase, 11, WTF::TruncateTrailingZeros);
    IntRect anchorRectInScreen = m_chromeClient->rootViewToScreen(m_parameters.anchorRectInRootView);
    String todayLabelString;
    String otherDateLabelString;
    if (m_parameters.type == InputTypeNames::month) {
        todayLabelString = locale().queryString(WebLocalizedString::ThisMonthButtonLabel);
        otherDateLabelString = locale().queryString(WebLocalizedString::OtherMonthLabel);
    } else if (m_parameters.type == InputTypeNames::week) {
        todayLabelString = locale().queryString(WebLocalizedString::ThisWeekButtonLabel);
        otherDateLabelString = locale().queryString(WebLocalizedString::OtherWeekLabel);
    } else {
        todayLabelString = locale().queryString(WebLocalizedString::CalendarToday);
        otherDateLabelString = locale().queryString(WebLocalizedString::OtherDateLabel);
    }

    addString("<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", data);
    data->append(Platform::current()->loadResource("pickerCommon.css"));
    data->append(Platform::current()->loadResource("pickerButton.css"));
    data->append(Platform::current()->loadResource("suggestionPicker.css"));
    data->append(Platform::current()->loadResource("calendarPicker.css"));
    addString("</style></head><body><div id=main>Loading...</div><script>\n"
        "window.dialogArguments = {\n", data);
    addProperty("anchorRectInScreen", anchorRectInScreen, data);
    addProperty("min", valueToDateTimeString(m_parameters.minimum, m_parameters.type), data);
    addProperty("max", valueToDateTimeString(m_parameters.maximum, m_parameters.type), data);
    addProperty("step", stepString, data);
    addProperty("stepBase", stepBaseString, data);
    addProperty("required", m_parameters.required, data);
    addProperty("currentValue", valueToDateTimeString(m_parameters.doubleValue, m_parameters.type), data);
    addProperty("locale", m_parameters.locale.string(), data);
    addProperty("todayLabel", todayLabelString, data);
    addProperty("clearLabel", locale().queryString(WebLocalizedString::CalendarClear), data);
    addProperty("weekLabel", locale().queryString(WebLocalizedString::WeekNumberLabel), data);
    addProperty("weekStartDay", m_locale->firstDayOfWeek(), data);
    addProperty("shortMonthLabels", m_locale->shortMonthLabels(), data);
    addProperty("dayLabels", m_locale->weekDayShortLabels(), data);
    addProperty("isLocaleRTL", m_locale->isRTL(), data);
    addProperty("isRTL", m_parameters.isAnchorElementRTL, data);
    addProperty("mode", m_parameters.type.string(), data);
    if (m_parameters.suggestions.size()) {
        Vector<String> suggestionValues;
        Vector<String> localizedSuggestionValues;
        Vector<String> suggestionLabels;
        for (unsigned i = 0; i < m_parameters.suggestions.size(); i++) {
            suggestionValues.append(valueToDateTimeString(m_parameters.suggestions[i].value, m_parameters.type));
            localizedSuggestionValues.append(m_parameters.suggestions[i].localizedValue);
            suggestionLabels.append(m_parameters.suggestions[i].label);
        }
        addProperty("suggestionValues", suggestionValues, data);
        addProperty("localizedSuggestionValues", localizedSuggestionValues, data);
        addProperty("suggestionLabels", suggestionLabels, data);
        addProperty("inputWidth", static_cast<unsigned>(m_parameters.anchorRectInRootView.width()), data);
        addProperty("showOtherDateEntry", RenderTheme::theme().supportsCalendarPicker(m_parameters.type), data);
        addProperty("otherDateLabel", otherDateLabelString, data);
        addProperty("suggestionHighlightColor", RenderTheme::theme().activeListBoxSelectionBackgroundColor().serialized(), data);
        addProperty("suggestionHighlightTextColor", RenderTheme::theme().activeListBoxSelectionForegroundColor().serialized(), data);
    }
    addString("}\n", data);

    data->append(Platform::current()->loadResource("pickerCommon.js"));
    data->append(Platform::current()->loadResource("suggestionPicker.js"));
    data->append(Platform::current()->loadResource("calendarPicker.js"));
    addString("</script></body>\n", data);
}

Locale& DateTimeChooserImpl::locale()
{
    return *m_locale;
}

void DateTimeChooserImpl::setValueAndClosePopup(int numValue, const String& stringValue)
{
    RefPtr<DateTimeChooserImpl> protector(this);
    if (numValue >= 0)
        setValue(stringValue);
    endChooser();
}

void DateTimeChooserImpl::setValue(const String& value)
{
    m_client->didChooseValue(value);
}

void DateTimeChooserImpl::closePopup()
{
    endChooser();
}

void DateTimeChooserImpl::didClosePopup()
{
    ASSERT(m_client);
    m_popup = 0;
    m_client->didEndChooser();
}

} // namespace blink

#endif // ENABLE(INPUT_MULTIPLE_FIELDS_UI)

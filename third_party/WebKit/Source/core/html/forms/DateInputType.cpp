/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/html/forms/DateInputType.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/DateTimeFieldsState.h"
#include "platform/DateComponents.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using blink::WebLocalizedString;
using namespace HTMLNames;

static const int kDateDefaultStep = 1;
static const int kDateDefaultStepBase = 0;
static const int kDateStepScaleFactor = 86400000;

inline DateInputType::DateInputType(HTMLInputElement& element)
    : BaseTemporalInputType(element) {}

InputType* DateInputType::Create(HTMLInputElement& element) {
  return new DateInputType(element);
}

void DateInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeDate);
}

const AtomicString& DateInputType::FormControlType() const {
  return InputTypeNames::date;
}

StepRange DateInputType::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  DEFINE_STATIC_LOCAL(
      const StepRange::StepDescription, step_description,
      (kDateDefaultStep, kDateDefaultStepBase, kDateStepScaleFactor,
       StepRange::kParsedStepValueShouldBeInteger));

  return InputType::CreateStepRange(
      any_step_handling, kDateDefaultStepBase,
      Decimal::FromDouble(DateComponents::MinimumDate()),
      Decimal::FromDouble(DateComponents::MaximumDate()), step_description);
}

bool DateInputType::ParseToDateComponentsInternal(const String& string,
                                                  DateComponents* out) const {
  DCHECK(out);
  unsigned end;
  return out->ParseDate(string, 0, end) && end == string.length();
}

bool DateInputType::SetMillisecondToDateComponents(double value,
                                                   DateComponents* date) const {
  DCHECK(date);
  return date->SetMillisecondsSinceEpochForDate(value);
}

void DateInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value != GetElement().SanitizeValue(value))
    AddWarningToConsole(
        "The specified value %s does not conform to the required format, "
        "\"yyyy-MM-dd\".",
        value);
}

String DateInputType::FormatDateTimeFieldsState(
    const DateTimeFieldsState& date_time_fields_state) const {
  if (!date_time_fields_state.HasDayOfMonth() ||
      !date_time_fields_state.HasMonth() || !date_time_fields_state.HasYear())
    return g_empty_string;

  return String::Format("%04u-%02u-%02u", date_time_fields_state.Year(),
                        date_time_fields_state.Month(),
                        date_time_fields_state.DayOfMonth());
}

void DateInputType::SetupLayoutParameters(
    DateTimeEditElement::LayoutParameters& layout_parameters,
    const DateComponents& date) const {
  layout_parameters.date_time_format = layout_parameters.locale.DateFormat();
  layout_parameters.fallback_date_time_format = "yyyy-MM-dd";
  if (!ParseToDateComponents(GetElement().FastGetAttribute(minAttr),
                             &layout_parameters.minimum))
    layout_parameters.minimum = DateComponents();
  if (!ParseToDateComponents(GetElement().FastGetAttribute(maxAttr),
                             &layout_parameters.maximum))
    layout_parameters.maximum = DateComponents();
  layout_parameters.placeholder_for_day = GetLocale().QueryString(
      WebLocalizedString::kPlaceholderForDayOfMonthField);
  layout_parameters.placeholder_for_month =
      GetLocale().QueryString(WebLocalizedString::kPlaceholderForMonthField);
  layout_parameters.placeholder_for_year =
      GetLocale().QueryString(WebLocalizedString::kPlaceholderForYearField);
}

bool DateInputType::IsValidFormat(bool has_year,
                                  bool has_month,
                                  bool has_week,
                                  bool has_day,
                                  bool has_ampm,
                                  bool has_hour,
                                  bool has_minute,
                                  bool has_second) const {
  return has_year && has_month && has_day;
}

}  // namespace blink

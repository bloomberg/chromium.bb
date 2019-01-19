// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_time_display_element.h"

#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace {

// These constants are used to estimate the size of time display element
// when the time display is hidden.
constexpr int kDefaultTimeDisplayDigitWidth = 8;
constexpr int kDefaultTimeDisplayColonWidth = 3;
constexpr int kDefaultTimeDisplayHeight = 48;

}  // namespace

namespace blink {

MediaControlTimeDisplayElement::MediaControlTimeDisplayElement(
    MediaControlsImpl& media_controls,
    blink::WebLocalizedString::Name localized_label)
    : MediaControlDivElement(media_controls, kMediaIgnore),
      localized_label_(localized_label) {
  SetAriaLabel();
}

void MediaControlTimeDisplayElement::SetCurrentValue(double time) {
  current_value_ = time;
  SetAriaLabel();
  setInnerText(FormatTime(), ASSERT_NO_EXCEPTION);
}

double MediaControlTimeDisplayElement::CurrentValue() const {
  return current_value_;
}

WebSize MediaControlTimeDisplayElement::GetSizeOrDefault() const {
  return MediaControlElementsHelper::GetSizeOrDefault(
      *this, WebSize(EstimateElementWidth(), kDefaultTimeDisplayHeight));
}

int MediaControlTimeDisplayElement::EstimateElementWidth() const {
  String formatted_time = MediaControlTimeDisplayElement::FormatTime();
  int colons = formatted_time.length() > 5 ? 2 : 1;
  return kDefaultTimeDisplayColonWidth * colons +
         kDefaultTimeDisplayDigitWidth * (formatted_time.length() - colons);
}

String MediaControlTimeDisplayElement::FormatTime() const {
  double time = std::isfinite(current_value_) ? current_value_ : 0;

  int seconds = static_cast<int>(fabs(time));
  int minutes = seconds / 60;
  int hours = minutes / 60;

  seconds %= 60;
  minutes %= 60;

  const char* negative_sign = (time < 0 ? "-" : "");

  // [0-10) minutes duration is m:ss
  // [10-60) minutes duration is mm:ss
  // [1-10) hours duration is h:mm:ss
  // [10-100) hours duration is hh:mm:ss
  // [100-1000) hours duration is hhh:mm:ss
  // etc.

  if (hours > 0) {
    return String::Format("%s%d:%02d:%02d", negative_sign, hours, minutes,
                          seconds);
  }

  return String::Format("%s%d:%02d", negative_sign, minutes, seconds);
}

void MediaControlTimeDisplayElement::SetAriaLabel() {
  String aria_label = GetLocale().QueryString(localized_label_, FormatTime());
  setAttribute(html_names::kAriaLabelAttr, AtomicString(aria_label));
}

}  // namespace blink

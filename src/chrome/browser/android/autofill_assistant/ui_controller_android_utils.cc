// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantColor_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDateTime_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDialogButton_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDimension_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantInfoPopup_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantValue_jni.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

namespace ui_controller_android_utils {

base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const std::string& color_string) {
  if (!Java_AssistantColor_isValidColorString(
          env, base::android::ConvertUTF8ToJavaString(env, color_string))) {
    if (!color_string.empty()) {
      VLOG(1) << "Encountered invalid color string: " << color_string;
    }
    return nullptr;
  }

  return Java_AssistantColor_createFromString(
      env, base::android::ConvertUTF8ToJavaString(env, color_string));
}

base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ColorProto& proto) {
  switch (proto.color_case()) {
    case ColorProto::kResourceIdentifier:
      if (!Java_AssistantColor_isValidResourceIdentifier(
              env, jcontext,
              base::android::ConvertUTF8ToJavaString(
                  env, proto.resource_identifier()))) {
        VLOG(1) << "Encountered invalid color resource identifier: "
                << proto.resource_identifier();
        return nullptr;
      }
      return Java_AssistantColor_createFromResource(
          env, jcontext,
          base::android::ConvertUTF8ToJavaString(env,
                                                 proto.resource_identifier()));
    case ColorProto::kParseableColor:
      return GetJavaColor(env, proto.parseable_color());
    case ColorProto::COLOR_NOT_SET:
      return nullptr;
  }
}

base::Optional<int> GetPixelSize(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ClientDimensionProto& proto) {
  switch (proto.size_case()) {
    case ClientDimensionProto::kDp:
      return Java_AssistantDimension_getPixelSizeDp(env, jcontext, proto.dp());
      break;
    case ClientDimensionProto::kWidthFactor:
      return Java_AssistantDimension_getPixelSizeWidthFactor(
          env, jcontext, proto.width_factor());
      break;
    case ClientDimensionProto::kHeightFactor:
      return Java_AssistantDimension_getPixelSizeHeightFactor(
          env, jcontext, proto.height_factor());
      break;
    case ClientDimensionProto::SIZE_NOT_SET:
      return base::nullopt;
  }
}

int GetPixelSizeOrDefault(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ClientDimensionProto& proto,
    int default_value) {
  auto size = GetPixelSize(env, jcontext, proto);
  if (size) {
    return *size;
  }
  return default_value;
}

base::android::ScopedJavaLocalRef<jobject> ToJavaValue(
    JNIEnv* env,
    const ValueProto& proto) {
  switch (proto.kind_case()) {
    case ValueProto::kStrings: {
      std::vector<std::string> strings;
      strings.reserve(proto.strings().values_size());
      for (const auto& string : proto.strings().values()) {
        strings.push_back(string);
      }
      return Java_AssistantValue_createForStrings(
          env, base::android::ToJavaArrayOfStrings(env, strings));
    }
    case ValueProto::kBooleans: {
      auto booleans = std::make_unique<bool[]>(proto.booleans().values_size());
      for (int i = 0; i < proto.booleans().values_size(); ++i) {
        booleans[i] = proto.booleans().values()[i];
      }
      return Java_AssistantValue_createForBooleans(
          env, base::android::ToJavaBooleanArray(
                   env, booleans.get(), proto.booleans().values_size()));
    }
    case ValueProto::kInts: {
      auto ints = std::make_unique<int[]>(proto.ints().values_size());
      for (int i = 0; i < proto.ints().values_size(); ++i) {
        ints[i] = proto.ints().values()[i];
      }
      return Java_AssistantValue_createForIntegers(
          env, base::android::ToJavaIntArray(env, ints.get(),
                                             proto.ints().values_size()));
    }
    case ValueProto::kCreditCards:
    case ValueProto::kProfiles:
    case ValueProto::kLoginOptions:
    case ValueProto::kCreditCardResponse:
    case ValueProto::kLoginOptionResponse:
    case ValueProto::kUserActions:
      // Unused.
      NOTREACHED();
      return nullptr;
    case ValueProto::kDates: {
      auto jlist = Java_AssistantValue_createDateTimeList(env);
      for (const auto& value : proto.dates().values()) {
        Java_AssistantValue_addDateTimeToList(
            env, jlist,
            Java_AssistantDateTime_Constructor(
                env, static_cast<int>(value.year()), value.month(), value.day(),
                0, 0, 0));
      }
      return Java_AssistantValue_createForDateTimes(env, jlist);
    }
    case ValueProto::KIND_NOT_SET:
      return Java_AssistantValue_create(env);
  }
}

ValueProto ToNativeValue(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jvalue) {
  ValueProto proto;
  if (!jvalue) {
    return proto;
  }
  auto jints = Java_AssistantValue_getIntegers(env, jvalue);
  if (jints) {
    auto* mutable_ints = proto.mutable_ints();
    std::vector<int> ints;
    base::android::JavaIntArrayToIntVector(env, jints, &ints);
    for (int i : ints) {
      mutable_ints->add_values(i);
    }
    return proto;
  }

  auto jbooleans = Java_AssistantValue_getBooleans(env, jvalue);
  if (jbooleans) {
    auto* mutable_booleans = proto.mutable_booleans();
    std::vector<bool> booleans;
    base::android::JavaBooleanArrayToBoolVector(env, jbooleans, &booleans);
    for (auto b : booleans) {
      mutable_booleans->add_values(b);
    }
    return proto;
  }

  auto jstrings = Java_AssistantValue_getStrings(env, jvalue);
  if (jstrings) {
    auto* mutable_strings = proto.mutable_strings();
    std::vector<std::string> strings;
    base::android::AppendJavaStringArrayToStringVector(env, jstrings, &strings);
    for (const auto& string : strings) {
      mutable_strings->add_values(string);
    }
    return proto;
  }

  auto jdatetimes = Java_AssistantValue_getDateTimes(env, jvalue);
  if (jdatetimes) {
    auto* mutable_dates = proto.mutable_dates();
    for (int i = 0; i < Java_AssistantValue_getListSize(env, jdatetimes); ++i) {
      auto jvalue = Java_AssistantValue_getListAt(env, jdatetimes, i);
      DateProto date;
      date.set_year(Java_AssistantDateTime_getYear(env, jvalue));
      date.set_month(Java_AssistantDateTime_getMonth(env, jvalue));
      date.set_day(Java_AssistantDateTime_getDay(env, jvalue));
      *mutable_dates->add_values() = date;
    }
    return proto;
  }

  return proto;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaDialogButton(
    JNIEnv* env,
    const InfoPopupProto_DialogButton& button_proto) {
  base::android::ScopedJavaLocalRef<jstring> jurl = nullptr;

  switch (button_proto.click_action_case()) {
    case InfoPopupProto::DialogButton::kOpenUrlInCct:
      jurl = base::android::ConvertUTF8ToJavaString(
          env, button_proto.open_url_in_cct().url());
      break;
    case InfoPopupProto::DialogButton::kCloseDialog:
      break;
    case InfoPopupProto::DialogButton::CLICK_ACTION_NOT_SET:
      NOTREACHED();
      break;
  }
  return Java_AssistantDialogButton_Constructor(
      env, base::android::ConvertUTF8ToJavaString(env, button_proto.label()),
      jurl);
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaInfoPopup(
    JNIEnv* env,
    const InfoPopupProto& info_popup_proto) {
  base::android::ScopedJavaLocalRef<jobject> jpositive_button = nullptr;
  base::android::ScopedJavaLocalRef<jobject> jnegative_button = nullptr;
  base::android::ScopedJavaLocalRef<jobject> jneutral_button = nullptr;

  if (info_popup_proto.has_positive_button() ||
      info_popup_proto.has_negative_button() ||
      info_popup_proto.has_neutral_button()) {
    if (info_popup_proto.has_positive_button()) {
      jpositive_button =
          CreateJavaDialogButton(env, info_popup_proto.positive_button());
    }
    if (info_popup_proto.has_negative_button()) {
      jnegative_button =
          CreateJavaDialogButton(env, info_popup_proto.negative_button());
    }
    if (info_popup_proto.has_neutral_button()) {
      jneutral_button =
          CreateJavaDialogButton(env, info_popup_proto.neutral_button());
    }
  } else {
    // If no button is set in the proto, we add a Close button
    jpositive_button = Java_AssistantDialogButton_Constructor(
        env,
        base::android::ConvertUTF8ToJavaString(
            env, l10n_util::GetStringUTF8(IDS_CLOSE)),
        nullptr);
  }

  return Java_AssistantInfoPopup_Constructor(
      env,
      base::android::ConvertUTF8ToJavaString(env, info_popup_proto.title()),
      base::android::ConvertUTF8ToJavaString(env, info_popup_proto.text()),
      jpositive_button, jnegative_button, jneutral_button);
}

void ShowJavaInfoPopup(JNIEnv* env,
                       base::android::ScopedJavaLocalRef<jobject> jinfo_popup,
                       base::android::ScopedJavaLocalRef<jobject> jcontext) {
  Java_AssistantInfoPopup_show(env, jinfo_popup, jcontext);
}

std::string SafeConvertJavaStringToNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jstring) {
  std::string native_string;
  if (jstring) {
    base::android::ConvertJavaStringToUTF8(env, jstring, &native_string);
  }
  return native_string;
}

}  // namespace ui_controller_android_utils

}  // namespace autofill_assistant

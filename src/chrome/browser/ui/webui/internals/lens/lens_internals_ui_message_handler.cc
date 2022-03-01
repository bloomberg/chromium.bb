// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/lens/lens_internals_ui_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/values.h"
#include "chrome/android/chrome_jni_headers/LensDebugBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

LensInternalsUIMessageHandler::LensInternalsUIMessageHandler(Profile* profile) {
}

LensInternalsUIMessageHandler::~LensInternalsUIMessageHandler() = default;

void LensInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "startDebugMode",
      base::BindRepeating(&LensInternalsUIMessageHandler::HandleStartDebugMode,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "refreshDebugData",
      base::BindRepeating(
          &LensInternalsUIMessageHandler::HandleRefreshDebugData,
          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "stopDebugMode",
      base::BindRepeating(&LensInternalsUIMessageHandler::HandleStopDebugMode,
                          base::Unretained(this)));
}

void LensInternalsUIMessageHandler::HandleStartDebugMode(
    const base::ListValue* args) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_LensDebugBridge_startProactiveDebugMode(env);

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, base::Value());
}

void LensInternalsUIMessageHandler::HandleRefreshDebugData(
    const base::ListValue* args) {
  // Only needs to be called once.
  AllowJavascript();
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobjectArray> j_debug_data =
      Java_LensDebugBridge_refreshDebugData(env);
  std::vector<std::vector<std::u16string>> debug_data;
  base::android::Java2dStringArrayTo2dStringVector(env, j_debug_data,
                                                   &debug_data);

  std::vector<base::Value> debug_data_as_vector_of_values;
  for (auto const& row : debug_data) {
    std::vector<base::Value> row_as_list_storage;
    // Convert to base::Value array.
    for (std::u16string const& cell_string : row) {
      row_as_list_storage.emplace_back(base::Value(cell_string));
    }
    debug_data_as_vector_of_values.emplace_back(
        base::Value(row_as_list_storage));
  }

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id,
                            base::Value(debug_data_as_vector_of_values));
}

void LensInternalsUIMessageHandler::HandleStopDebugMode(
    const base::ListValue* args) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_LensDebugBridge_stopProactiveDebugMode(env);

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, base::Value());
}

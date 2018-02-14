// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notification_helper/notification_activator.h"

namespace notification_helper {

NotificationActivator::~NotificationActivator() = default;

// |invoked_args| contains the template ID string encoded by Chrome via
// NotificationPlatformBridgeWin::EncodeTemplateId.
// Chrome adds it to the launch argument and gets it back via invoked_args here.
// Chrome needs the data to be able to look up the notification on its end.
//
// |invoked_args| string constains the following structure:
// "notification_type|profile_id|incognito|origin_url|notification_id"
//
// An example of input parameter value. The toast from which the activation
// comes is generated from https://tests.peter.sh/notification-generator/ with
// the default setting.
// {
//    app_user_model_id = "Chromium.KX56J2SJSCJTWGPH2RNH3MHAM4"
//    invoked_args =
//         "0|Default|0|https://tests.peter.sh/|p#https://tests.peter.sh/#01"
//    data = nullptr
//    count = 0
// }
//
// Then invoked_args we will be decoded into:
//    |notification_type| = 0
//           |profile_id| = Default
//            |incognito| = 0
//           |origin_url| = https://tests.peter.sh/
//      |notification_id| = p#https://tests.peter.sh/#01
HRESULT NotificationActivator::Activate(
    LPCWSTR app_user_model_id,
    LPCWSTR invoked_args,
    const NOTIFICATION_USER_INPUT_DATA* data,
    ULONG count) {
  // When Chrome is running, Windows will call this API and
  // NotificationPlatformBridgeWinImpl::OnActivated serially.

  // TODO(chengx): Investigate the correct activate behavior (mainly when Chrome
  // is not running) and implement it. For example, decode the useful data from
  // the function input and launch Chrome with proper args.

  return S_OK;
}

}  // namespace notification_helper

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_TEST_METRO_REGISTRATION_HELPER_H_
#define WIN8_TEST_METRO_REGISTRATION_HELPER_H_

#include "base/string16.h"

namespace win8 {

// Registers a viewer process as a potential Win8 default browser. Intended to
// be used by a test binary in the build output directory and assumes the
// presence of test_registrar.exe, a viewer process and all needed DLLs in the
// same directory as the calling module. The viewer process is then subsequently
// set as THE default Win8 browser, is activatable using the AppUserModelId in
// |app_user_model_id| and will show up in the default browser selection dialog
// as |viewer_process_name|.
//
// See also open_with_dialog_controller.h for a mechanism for setting THE
// default Win8 browser.
bool RegisterTestDefaultBrowser(const string16& app_user_model_id,
                                const string16& viewer_process_name);

}  // namespace win8

#endif  // WIN8_TEST_METRO_REGISTRATION_HELPER_H_

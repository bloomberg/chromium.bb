// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_MESSAGING_H_
#define CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_MESSAGING_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"

namespace extensions {
class NativeMessageHost;
}  // namespace extensions

namespace chromeos {

extern const char kWilcoDtcSupportdUiMessageHost[];

extern const char kWilcoDtcSupportdUiMessageTooBigExtensionsError[];
extern const char kWilcoDtcSupportdUiExtraMessagesExtensionsError[];

extern const int kWilcoDtcSupportdUiMessageMaxSize;

// Creates an extensions native message host that talks to the
// diagnostics_processor daemon. This should be used when the communication is
// initiated by the extension (i.e., not the daemon).
std::unique_ptr<extensions::NativeMessageHost>
CreateExtensionOwnedWilcoDtcSupportdMessageHost();

// Delivers the UI message |json_message| from the diagnostics_processor daemon
// to the extensions that are allowed to receive it. The delivery is done via
// creating extensions native message hosts. |send_response_callback| will be
// called with the response from the extension (the first non-empty one in case
// of multiple extensions providing some responses).
void DeliverWilcoDtcSupportdUiMessageToExtensions(
    const std::string& json_message,
    base::OnceCallback<void(const std::string& response)>
        send_response_callback);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_MESSAGING_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_
#define CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace network_handler {

COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDBusFailedError[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDBusFailedErrorMessage[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kErrorName[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kErrorDetail[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDbusErrorName[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDbusErrorMessage[];

// On success, |result| contains the result. On failure, |result| is nullopt.
using ResultCallback =
    base::OnceCallback<void(const std::string& service_path,
                            absl::optional<base::Value> result)>;

// On success, |properties| contains the resulting properties and |error| is
// nullopt. On failure, |result| is nullopt and |error| may contain an error
// identifier.
using PropertiesCallback =
    base::OnceCallback<void(const std::string& service_path,
                            absl::optional<base::Value> properties,
                            absl::optional<std::string> error)>;

// An error callback used by both the configuration handler and the state
// handler to receive error results from the API.
using ErrorCallback =
    base::OnceCallback<void(const std::string& error_name,
                            std::unique_ptr<base::DictionaryValue> error_data)>;

using ServiceResultCallback =
    base::OnceCallback<void(const std::string& service_path,
                            const std::string& guid)>;

// Create a DictionaryValue for passing to ErrorCallback.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::DictionaryValue* CreateErrorData(const std::string& path,
                                       const std::string& error_name,
                                       const std::string& error_detail);

// If not NULL, runs |error_callback| with an ErrorData dictionary created from
// the other arguments.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void RunErrorCallback(ErrorCallback error_callback,
                      const std::string& path,
                      const std::string& error_name,
                      const std::string& error_detail);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::unique_ptr<base::DictionaryValue> CreateDBusErrorData(
    const std::string& path,
    const std::string& error_name,
    const std::string& error_detail,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message);

// Callback for Shill errors.
// |error_name| is the error name passed to |error_callback|.
// |path| is the associated object path or blank if not relevant.
// |dbus_error_name| and |dbus_error_message| are provided by the DBus handler.
// Logs an error and calls |error_callback| if not null.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void ShillErrorCallbackFunction(const std::string& error_name,
                                const std::string& path,
                                ErrorCallback error_callback,
                                const std::string& dbus_error_name,
                                const std::string& dbus_error_message);

}  // namespace network_handler
}  // namespace chromeos

namespace ash {
// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace network_handler = ::chromeos::network_handler;
}

#endif  // CHROMEOS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_

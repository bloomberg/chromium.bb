// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/adb/adb_device_provider.h"

#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/device/adb/adb_client_socket.h"

namespace {

const char kHostDevicesCommand[] = "host:devices";
const char kHostTransportCommand[] = "host:transport:%s|%s";
const char kLocalAbstractCommand[] = "localabstract:%s";

const int kAdbPort = 5037;

static void RunCommand(const std::string& serial,
                       const std::string& command,
                       const AdbDeviceProvider::CommandCallback& callback) {
  std::string query = base::StringPrintf(
      kHostTransportCommand, serial.c_str(), command.c_str());
  AdbClientSocket::AdbQuery(kAdbPort, query, callback);
}

static void ReceivedAdbDevices(
    const AdbDeviceProvider::SerialsCallback& callback,
    int result_code,
    const std::string& response) {
  std::vector<std::string> result;
  if (result_code < 0) {
    callback.Run(result);
    return;
  }
  for (const base::StringPiece& line :
       base::SplitStringPiece(response, "\n", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<base::StringPiece> tokens =
        base::SplitStringPiece(line, "\t ", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    result.push_back(tokens[0].as_string());
  }
  callback.Run(result);
}

} // namespace

void AdbDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand, base::Bind(&ReceivedAdbDevices, callback));
}

void AdbDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                        const DeviceInfoCallback& callback) {
  AndroidDeviceManager::QueryDeviceInfo(base::Bind(&RunCommand, serial),
                                        callback);
}

void AdbDeviceProvider::OpenSocket(const std::string& serial,
                                   const std::string& socket_name,
                                   const SocketCallback& callback) {
  std::string request =
      base::StringPrintf(kLocalAbstractCommand, socket_name.c_str());
  AdbClientSocket::TransportQuery(kAdbPort, serial, request, callback);
}

AdbDeviceProvider::~AdbDeviceProvider() {
}

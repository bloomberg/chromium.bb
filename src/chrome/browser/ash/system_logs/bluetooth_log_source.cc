// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/bluetooth_log_source.h"

#include "base/json/json_writer.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/floss/floss_features.h"

namespace system_logs {

namespace {

constexpr char kBluetoothLogEntry[] = "CHROMEOS_BLUETOOTH";
constexpr char kIsFloss[] = "Floss";

}  // namespace

BluetoothLogSource::BluetoothLogSource() : SystemLogsSource("BluetoothLog") {}

BluetoothLogSource::~BluetoothLogSource() {}

void BluetoothLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto bluetooth_log = base::Value(base::Value::Type::DICTIONARY);
  auto response = std::make_unique<SystemLogsResponse>();
  std::string json;

  bluetooth_log.SetBoolKey(
      kIsFloss, base::FeatureList::IsEnabled(floss::features::kFlossEnabled));

  base::JSONWriter::WriteWithOptions(
      bluetooth_log, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);

  (*response)[kBluetoothLogEntry] = std::move(json);
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs

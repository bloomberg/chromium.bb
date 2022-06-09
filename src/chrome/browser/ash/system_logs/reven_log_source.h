// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_REVEN_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_REVEN_LOG_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace system_logs {

// Collect Hardware data for cloudready devices via cros_healthd calls
// Currently, reven is the only board name for cloudready. The board name won't
// change but cloudready will be replaced by a new official name. This is why
// the new log source is named.
class RevenLogSource : public SystemLogsSource {
 public:
  RevenLogSource();
  RevenLogSource(const RevenLogSource&) = delete;
  RevenLogSource& operator=(const RevenLogSource&) = delete;
  ~RevenLogSource() override;

  // SystemLogsSource
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  void OnTelemetryInfoProbeResponse(
      SysLogsSourceCallback callback,
      chromeos::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  mojo::Remote<chromeos::cros_healthd::mojom::CrosHealthdProbeService>
      probe_service_;

  base::WeakPtrFactory<RevenLogSource> weak_ptr_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_REVEN_LOG_SOURCE_H_

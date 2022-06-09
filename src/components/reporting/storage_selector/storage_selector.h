// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_SELECTOR_STORAGE_SELECTOR_H_
#define COMPONENTS_REPORTING_STORAGE_SELECTOR_STORAGE_SELECTOR_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// This static class facilitates ReportingClient ability to select underlying
// storage for encrypted reporting pipeline report client.  It is built into
// Chrome and configured differently depending on whether Chrome is intended for
// ChromeOS/LaCros or not and whether it is Ash Chrome: it can store event
// locally or in Missive Daemon. It can also be built into other daemons; in
// that case it always connects to Missive Daemon.
class StorageSelector {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Features to select specific backends.
  // By default storage is local (as opposed to missive daemon use)
  // and upload is enabled.
  static const char kUseMissiveDaemon[];
  static const char kProvideUploader[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  static bool is_use_missive();
  static bool is_uploader_required();

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  static void CreateMissiveStorageModule(
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
          cb);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_SELECTOR_STORAGE_SELECTOR_H_

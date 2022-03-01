// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater_scope.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_WIN)
#include "chrome/updater/tag.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/win_util.h"
#endif

namespace updater {
namespace {

bool IsSystemProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSystemSwitch);
}

}  // namespace

UpdaterScope GetUpdaterScope() {
#if defined(OS_WIN)
  if (IsSystemProcess()) {
    return UpdaterScope::kSystem;
  }

  // TODO(crbug.com/1128631): support bundles. For now, assume one app.
  const absl::optional<tagging::TagArgs> tag_args = GetTagArgs();
  if (tag_args && !tag_args->apps.empty() &&
      tag_args->apps.front().needs_admin) {
    DCHECK_EQ(tag_args->apps.size(), size_t{1});
    switch (*tag_args->apps.front().needs_admin) {
      case tagging::AppArgs::NeedsAdmin::kYes:
      case tagging::AppArgs::NeedsAdmin::kPrefers:
        return UpdaterScope::kSystem;
      case tagging::AppArgs::NeedsAdmin::kNo:
        return UpdaterScope::kUser;
    }
  }

  // crbug.com(1214058): consider handling the elevation case by
  // calling IsUserAdmin().
  return UpdaterScope::kUser;
#else
  return IsSystemProcess() ? UpdaterScope::kSystem : UpdaterScope::kUser;
#endif
}

}  // namespace updater

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_apps_page/mojom/app_type_mojom_traits.h"

namespace mojo {

Readiness EnumTraits<Readiness, apps::Readiness>::ToMojom(
    apps::Readiness input) {
  switch (input) {
    case apps::Readiness::kUnknown:
      return Readiness::kUnknown;
    case apps::Readiness::kReady:
      return Readiness::kReady;
    case apps::Readiness::kDisabledByBlocklist:
      return Readiness::kDisabledByBlocklist;
    case apps::Readiness::kDisabledByPolicy:
      return Readiness::kDisabledByPolicy;
    case apps::Readiness::kDisabledByUser:
      return Readiness::kDisabledByUser;
    case apps::Readiness::kTerminated:
      return Readiness::kTerminated;
    case apps::Readiness::kUninstalledByUser:
      return Readiness::kUninstalledByUser;
    case apps::Readiness::kRemoved:
      return Readiness::kRemoved;
    case apps::Readiness::kUninstalledByMigration:
      return Readiness::kUninstalledByMigration;
  }
}

bool EnumTraits<Readiness, apps::Readiness>::FromMojom(
    Readiness input,
    apps::Readiness* output) {
  switch (input) {
    case Readiness::kUnknown:
      *output = apps::Readiness::kUnknown;
      return true;
    case Readiness::kReady:
      *output = apps::Readiness::kReady;
      return true;
    case Readiness::kDisabledByBlocklist:
      *output = apps::Readiness::kDisabledByBlocklist;
      return true;
    case Readiness::kDisabledByPolicy:
      *output = apps::Readiness::kDisabledByPolicy;
      return true;
    case Readiness::kDisabledByUser:
      *output = apps::Readiness::kDisabledByUser;
      return true;
    case Readiness::kTerminated:
      *output = apps::Readiness::kTerminated;
      return true;
    case Readiness::kUninstalledByUser:
      *output = apps::Readiness::kUninstalledByUser;
      return true;
    case Readiness::kRemoved:
      *output = apps::Readiness::kRemoved;
      return true;
    case Readiness::kUninstalledByMigration:
      *output = apps::Readiness::kUninstalledByMigration;
      return true;
  }
}

}  // namespace mojo

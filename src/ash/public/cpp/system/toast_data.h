// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_
#define ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/system/toast_catalog.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ToastData {
  // A `ToastData` with a `kInfiniteDuration` duration will be displayed until
  // the dismiss button on the toast is clicked.
  static constexpr base::TimeDelta kInfiniteDuration = base::TimeDelta::Max();

  // The default duration that a toast will be shown before it is automatically
  // dismissed.
  static constexpr base::TimeDelta kDefaultToastDuration = base::Seconds(6);

  // Minimum duration for a toast to be visible before it is automatically
  // dismissed.
  static constexpr base::TimeDelta kMinimumDuration = base::Milliseconds(200);

  // Creates a `ToastData` which is used to configure how a toast behaves when
  // shown. The toast `duration` is how long the toast will be shown before it
  // is automatically dismissed. The `duration` will be set to
  // `kMinimumDuration` for any value provided that is smaller than
  // `kMinimumDuration`. To disable automatically dismissing the toast, set the
  // `duration` to `kInfiniteDuration`.
  ToastData(std::string id,
            ToastCatalogName catalog_name,
            const std::u16string& text,
            base::TimeDelta duration = kDefaultToastDuration,
            bool visible_on_lock_screen = false,
            const absl::optional<std::u16string>& dismiss_text = absl::nullopt);

  ToastData(const ToastData& other);
  ~ToastData();

  std::string id;
  ToastCatalogName catalog_name;
  std::u16string text;
  base::TimeDelta duration;
  bool visible_on_lock_screen;
  absl::optional<std::u16string> dismiss_text;
  bool is_managed = false;
  base::RepeatingClosure dismiss_callback;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_

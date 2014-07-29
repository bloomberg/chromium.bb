// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_INFO_H_
#define UI_CHROMEOS_NETWORK_NETWORK_INFO_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class ImageSkia;
}

namespace ui {

// Includes information necessary about a network for displaying the appropriate
// UI to the user.
struct UI_CHROMEOS_EXPORT NetworkInfo {
  NetworkInfo();
  NetworkInfo(const std::string& path);
  ~NetworkInfo();

  std::string service_path;
  base::string16 label;
  gfx::ImageSkia image;
  bool disable;
  bool highlight;
};

}  // namespace ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_INFO_H_

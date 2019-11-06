// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DOCK_TILE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DOCK_TILE_H_

#include "base/macros.h"
#include "ui/gfx/image/image.h"

class DevToolsDockTile {
 public:
  static void Update(const std::string& label, gfx::Image image);

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsDockTile);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DOCK_TILE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_TEST_MOCK_SURFACE_GENERATOR_H_
#define UI_OZONE_PLATFORM_DRI_TEST_MOCK_SURFACE_GENERATOR_H_

#include <vector>

#include "ui/ozone/platform/dri/dri_surface.h"

namespace gfx {
class Size;
}

namespace ui {

class DriWrapper;

class MockSurfaceGenerator : public ScanoutSurfaceGenerator {
 public:
  MockSurfaceGenerator(DriWrapper* dri);
  virtual ~MockSurfaceGenerator();

  std::vector<DriSurface*> surfaces() const { return surfaces_; }

  // ScanoutSurfaceGenerator:
  virtual ScanoutSurface* Create(const gfx::Size& size) OVERRIDE;

 private:
  DriWrapper* dri_;  // Not owned.

  std::vector<DriSurface*> surfaces_;

  DISALLOW_COPY_AND_ASSIGN(MockSurfaceGenerator);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_TEST_MOCK_SURFACE_GENERATOR_H_

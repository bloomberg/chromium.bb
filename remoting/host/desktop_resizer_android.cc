// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "remoting/host/desktop_resizer.h"

namespace remoting {

namespace {

class DesktopResizerAndroid : public DesktopResizer {
 public:
  DesktopResizerAndroid() {}

  // DesktopResizer:
  ScreenResolution GetCurrentResolution() override {
    NOTIMPLEMENTED();
    return ScreenResolution();
  }

  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) override {
    NOTIMPLEMENTED();
    return std::list<ScreenResolution>();
  }

  void SetResolution(const ScreenResolution& resolution) override {
    NOTIMPLEMENTED();
  }

  void RestoreResolution(const ScreenResolution& original) override {
    NOTIMPLEMENTED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopResizerAndroid);
};

}  // namespace

// static
std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return base::WrapUnique(new DesktopResizerAndroid);
}

}  // namespace remoting

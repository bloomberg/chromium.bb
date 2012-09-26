// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include "base/logging.h"

namespace remoting {

namespace {
class DesktopResizerMac : public DesktopResizer {
 public:
  DesktopResizerMac() {
  }

  virtual SkISize GetCurrentSize() OVERRIDE {
    NOTIMPLEMENTED();
    return SkISize::Make(0, 0);
  }

  virtual std::list<SkISize> GetSupportedSizes(
      const SkISize& preferred) OVERRIDE {
    NOTIMPLEMENTED();
    return std::list<SkISize>();
  }

  virtual void SetSize(const SkISize& size) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void RestoreSize(const SkISize& original) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopResizerMac);
};
}  // namespace

scoped_ptr<DesktopResizer> DesktopResizer::Create() {
  return scoped_ptr<DesktopResizer>(new DesktopResizerMac);
}

}  // namespace remoting

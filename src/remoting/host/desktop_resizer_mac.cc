// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <Carbon/Carbon.h>
#include <stdint.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "remoting/base/logging.h"

namespace {
// TODO(jamiewalch): Use the correct DPI for the mode: http://crbug.com/172405.
const int kDefaultDPI = 96;
}  // namespace

namespace remoting {

class DesktopResizerMac : public DesktopResizer {
 public:
  DesktopResizerMac();

  // DesktopResizer interface
  ScreenResolution GetCurrentResolution() override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) override;
  void SetResolution(const ScreenResolution& resolution) override;
  void RestoreResolution(const ScreenResolution& original) override;

 private:
  // If there is a single display, get its id and return true, otherwise return
  // false. We don't currently support resize-to-client on multi-monitor Macs.
  bool GetSoleDisplayId(CGDirectDisplayID* display);

  void GetSupportedModesAndResolutions(
      base::ScopedCFTypeRef<CFMutableArrayRef>* modes,
      std::list<ScreenResolution>* resolutions);

  DISALLOW_COPY_AND_ASSIGN(DesktopResizerMac);
};

DesktopResizerMac::DesktopResizerMac() {}

ScreenResolution DesktopResizerMac::GetCurrentResolution() {
  CGDirectDisplayID display;
  if (GetSoleDisplayId(&display)) {
    CGRect rect = CGDisplayBounds(display);
    return ScreenResolution(
        webrtc::DesktopSize(rect.size.width, rect.size.height),
        webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
  }
  return ScreenResolution();
}

std::list<ScreenResolution> DesktopResizerMac::GetSupportedResolutions(
    const ScreenResolution& preferred) {
  base::ScopedCFTypeRef<CFMutableArrayRef> modes;
  std::list<ScreenResolution> resolutions;
  GetSupportedModesAndResolutions(&modes, &resolutions);
  return resolutions;
}

void DesktopResizerMac::SetResolution(const ScreenResolution& resolution) {
  CGDirectDisplayID display;
  if (!GetSoleDisplayId(&display)) {
    return;
  }

  base::ScopedCFTypeRef<CFMutableArrayRef> modes;
  std::list<ScreenResolution> resolutions;
  GetSupportedModesAndResolutions(&modes, &resolutions);
  // There may be many modes with the requested resolution. Pick the one with
  // the highest color depth.
  int index = 0, best_depth = 0;
  CGDisplayModeRef best_mode = nullptr;
  for (std::list<ScreenResolution>::const_iterator i = resolutions.begin();
       i != resolutions.end(); ++i, ++index) {
    if (i->Equals(resolution)) {
      CGDisplayModeRef mode = const_cast<CGDisplayModeRef>(
          static_cast<const CGDisplayMode*>(
              CFArrayGetValueAtIndex(modes, index)));
      int depth = 0;
      base::ScopedCFTypeRef<CFStringRef> encoding(
          CGDisplayModeCopyPixelEncoding(mode));
      if (CFStringCompare(encoding, CFSTR(IO32BitDirectPixels),
                          kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        depth = 32;
      } else if (CFStringCompare(
          encoding, CFSTR(IO16BitDirectPixels),
          kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        depth = 16;
      } else if(CFStringCompare(
          encoding, CFSTR(IO8BitIndexedPixels),
          kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        depth = 8;
      }
      if (depth > best_depth) {
        best_depth = depth;
        best_mode = mode;
      }
    }
  }
  if (best_mode) {
    HOST_LOG << "Changing mode to " << best_mode << " ("
              << resolution.dimensions().width() << "x"
              << "x" << resolution.dimensions().height() << "x"
              << best_depth << " @ "
              << resolution.dpi().x() << "x" << resolution.dpi().y() << " dpi)";
    CGDisplaySetDisplayMode(display, best_mode, nullptr);
  }
}

void DesktopResizerMac::RestoreResolution(const ScreenResolution& original) {
  SetResolution(original);
}

void DesktopResizerMac::GetSupportedModesAndResolutions(
    base::ScopedCFTypeRef<CFMutableArrayRef>* modes,
    std::list<ScreenResolution>* resolutions) {
  CGDirectDisplayID display;
  if (!GetSoleDisplayId(&display)) {
    return;
  }

  base::ScopedCFTypeRef<CFArrayRef> all_modes(
      CGDisplayCopyAllDisplayModes(display, nullptr));
  if (!all_modes) {
    return;
  }

  modes->reset(CFArrayCreateMutableCopy(nullptr, 0, all_modes));
  CFIndex count = CFArrayGetCount(*modes);
  for (CFIndex i = 0; i < count; ++i) {
    CGDisplayModeRef mode = const_cast<CGDisplayModeRef>(
        static_cast<const CGDisplayMode*>(
            CFArrayGetValueAtIndex(*modes, i)));
    if (CGDisplayModeIsUsableForDesktopGUI(mode)) {
      // TODO(jamiewalch): Get the correct DPI: http://crbug.com/172405.
      ScreenResolution resolution(
          webrtc::DesktopSize(CGDisplayModeGetWidth(mode),
                              CGDisplayModeGetHeight(mode)),
          webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
      resolutions->push_back(resolution);
    } else {
      CFArrayRemoveValueAtIndex(*modes, i);
      --count;
      --i;
    }
  }
}

bool DesktopResizerMac::GetSoleDisplayId(CGDirectDisplayID* display) {
  // This code only supports a single display, but allocates space for two
  // to allow the multi-monitor case to be detected.
  CGDirectDisplayID displays[2];
  uint32_t num_displays;
  CGError err = CGGetActiveDisplayList(arraysize(displays),
                                       displays, &num_displays);
  if (err != kCGErrorSuccess || num_displays != 1) {
    return false;
  }
  *display = displays[0];
  return true;
}

std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return base::WrapUnique(new DesktopResizerMac);
}

}  // namespace remoting

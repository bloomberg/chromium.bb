// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <Carbon/Carbon.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"

namespace remoting {

class DesktopResizerMac : public DesktopResizer {
 public:
  DesktopResizerMac();

  // DesktopResizer interface
  virtual SkISize GetCurrentSize() OVERRIDE;
  virtual std::list<SkISize> GetSupportedSizes(
      const SkISize& preferred) OVERRIDE;
  virtual void SetSize(const SkISize& size) OVERRIDE;
  virtual void RestoreSize(const SkISize& original) OVERRIDE;

 private:
  // If there is a single display, get its id and return true, otherwise return
  // false. We don't currently support resize-to-client on multi-monitor Macs.
  bool GetSoleDisplayId(CGDirectDisplayID* display);

  void GetSupportedModesAndSizes(
      base::mac::ScopedCFTypeRef<CFMutableArrayRef>* modes,
      std::list<SkISize>* sizes);

  DISALLOW_COPY_AND_ASSIGN(DesktopResizerMac);
};

DesktopResizerMac::DesktopResizerMac() {}

SkISize DesktopResizerMac::GetCurrentSize() {
  CGDirectDisplayID display;
  if (!base::mac::IsOSSnowLeopard() && GetSoleDisplayId(&display)) {
    CGRect rect = CGDisplayBounds(display);
    return SkISize::Make(rect.size.width, rect.size.height);
  }
  return SkISize::Make(0, 0);
}

std::list<SkISize> DesktopResizerMac::GetSupportedSizes(
    const SkISize& preferred) {
  base::mac::ScopedCFTypeRef<CFMutableArrayRef> modes;
  std::list<SkISize> sizes;
  GetSupportedModesAndSizes(&modes, &sizes);
  return sizes;
}

void DesktopResizerMac::SetSize(const SkISize& size) {
  CGDirectDisplayID display;
  if (base::mac::IsOSSnowLeopard() || !GetSoleDisplayId(&display)) {
    return;
  }

  base::mac::ScopedCFTypeRef<CFMutableArrayRef> modes;
  std::list<SkISize> sizes;
  GetSupportedModesAndSizes(&modes, &sizes);
  // There may be many modes with the requested size. Pick the one with the
  // highest color depth.
  int index = 0, best_depth = 0;
  CGDisplayModeRef best_mode = NULL;
  for (std::list<SkISize>::const_iterator i = sizes.begin(); i != sizes.end();
       ++i, ++index) {
    if (*i == size) {
      CGDisplayModeRef mode = const_cast<CGDisplayModeRef>(
          static_cast<const CGDisplayMode*>(
              CFArrayGetValueAtIndex(modes, index)));
      int depth = 0;
      base::mac::ScopedCFTypeRef<CFStringRef> encoding(
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
    LOG(INFO) << "Changing mode to " << best_mode << " (" << size.width()
              << "x" << size.height() << "x" << best_depth << ")";
    CGDisplaySetDisplayMode(display, best_mode, NULL);
  }
}

void DesktopResizerMac::RestoreSize(const SkISize& original) {
  SetSize(original);
}

void DesktopResizerMac::GetSupportedModesAndSizes(
    base::mac::ScopedCFTypeRef<CFMutableArrayRef>* modes,
    std::list<SkISize>* sizes) {
  CGDirectDisplayID display;
  if (GetSoleDisplayId(&display)) {
    base::mac::ScopedCFTypeRef<CFArrayRef>
        all_modes(CGDisplayCopyAllDisplayModes(display, NULL));
    modes->reset(CFArrayCreateMutableCopy(NULL, 0, all_modes));
    CFIndex count = CFArrayGetCount(*modes);
    for (CFIndex i = 0; i < count; ++i) {
      CGDisplayModeRef mode = const_cast<CGDisplayModeRef>(
          static_cast<const CGDisplayMode*>(
              CFArrayGetValueAtIndex(*modes, i)));
      if (CGDisplayModeIsUsableForDesktopGUI(mode)) {
        SkISize size = SkISize::Make(CGDisplayModeGetWidth(mode),
                                     CGDisplayModeGetHeight(mode));
        sizes->push_back(size);
      } else {
        CFArrayRemoveValueAtIndex(*modes, i);
        --count;
        --i;
      }
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

scoped_ptr<DesktopResizer> DesktopResizer::Create() {
  return scoped_ptr<DesktopResizer>(new DesktopResizerMac);
}

}  // namespace remoting

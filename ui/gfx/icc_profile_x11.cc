// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

extern "C" {
#include <X11/Xatom.h>
#include <X11/Xlib.h>
}

#include "base/command_line.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

namespace gfx {

// static
ICCProfile ICCProfile::FromBestMonitor() {
  ICCProfile icc_profile;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless))
    return icc_profile;
  Atom property = GetAtom("_ICC_PROFILE");
  if (property != None) {
    Atom prop_type = None;
    int prop_format = 0;
    unsigned long nitems = 0;
    unsigned long nbytes = 0;
    char* property_data = NULL;
    if (XGetWindowProperty(
            GetXDisplay(), DefaultRootWindow(GetXDisplay()), property, 0,
            0x1FFFFFFF /* MAXINT32 / 4 */, False, AnyPropertyType, &prop_type,
            &prop_format, &nitems, &nbytes,
            reinterpret_cast<unsigned char**>(&property_data)) == Success) {
      icc_profile = FromData(property_data, nitems);
      XFree(property_data);
    }
  }
  return icc_profile;
}

}  // namespace gfx

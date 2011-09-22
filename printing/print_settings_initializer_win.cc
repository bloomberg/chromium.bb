// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer_win.h"

#include <windows.h>

#include "printing/print_settings.h"

namespace printing {

// static
void PrintSettingsInitializerWin::InitPrintSettings(
    HDC hdc,
    const DEVMODE& dev_mode,
    const PageRanges& new_ranges,
    const std::wstring& new_device_name,
    bool print_selection_only,
    PrintSettings* print_settings) {
  DCHECK(hdc);
  DCHECK(print_settings);

  print_settings->set_printer_name(dev_mode.dmDeviceName);
  print_settings->set_device_name(new_device_name);
  print_settings->ranges = const_cast<PageRanges&>(new_ranges);
  print_settings->set_landscape(dev_mode.dmOrientation == DMORIENT_LANDSCAPE);
  print_settings->selection_only = print_selection_only;

  int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
  print_settings->set_dpi(dpi);
  const int kAlphaCaps = SB_CONST_ALPHA | SB_PIXEL_ALPHA;
  print_settings->set_supports_alpha_blend(
    (GetDeviceCaps(hdc, SHADEBLENDCAPS) & kAlphaCaps) == kAlphaCaps);
  // No printer device is known to advertise different dpi in X and Y axis; even
  // the fax device using the 200x100 dpi setting. It's ought to break so many
  // applications that it's not even needed to care about. WebKit doesn't
  // support different dpi settings in X and Y axis.
  DCHECK_EQ(dpi, GetDeviceCaps(hdc, LOGPIXELSY));

  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORX), 0);
  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORY), 0);

  // Initialize page_setup_device_units_.
  gfx::Size physical_size_device_units(GetDeviceCaps(hdc, PHYSICALWIDTH),
                                       GetDeviceCaps(hdc, PHYSICALHEIGHT));
  gfx::Rect printable_area_device_units(GetDeviceCaps(hdc, PHYSICALOFFSETX),
                                        GetDeviceCaps(hdc, PHYSICALOFFSETY),
                                        GetDeviceCaps(hdc, HORZRES),
                                        GetDeviceCaps(hdc, VERTRES));

  print_settings->SetPrinterPrintableArea(physical_size_device_units,
                                          printable_area_device_units,
                                          dpi);
}

}  // namespace printing

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer_win.h"

#include <windows.h>

#include "printing/print_settings.h"

namespace printing {

namespace {

bool HasEscapeSupprt(HDC hdc, DWORD escape) {
  const char* ptr = reinterpret_cast<const char*>(&escape);
  return ExtEscape(hdc, QUERYESCSUPPORT, sizeof(escape), ptr, 0, nullptr) > 0;
}

bool IsTechnology(HDC hdc, const char* technology) {
  if (::GetDeviceCaps(hdc, TECHNOLOGY) != DT_RASPRINTER)
    return false;

  if (!HasEscapeSupprt(hdc, GETTECHNOLOGY))
    return false;

  char buf[256];
  memset(buf, 0, sizeof(buf));
  if (ExtEscape(hdc, GETTECHNOLOGY, 0, nullptr, sizeof(buf) - 1, buf) <= 0)
    return false;
  return strcmp(buf, technology) == 0;
}

bool IsPrinterPostScript(HDC hdc, int* level) {
  static constexpr char kPostScriptDriver[] = "PostScript";
  if (!IsTechnology(hdc, kPostScriptDriver)) {
    return false;
  }

  // Query the PS Level if possible. Many PS printers do not implement this.
  if (HasEscapeSupprt(hdc, GET_PS_FEATURESETTING)) {
    constexpr int param = FEATURESETTING_PSLEVEL;
    const char* param_char_ptr = reinterpret_cast<const char*>(&param);
    int param_out = -1;
    char* param_out_char_ptr = reinterpret_cast<char*>(&param_out);
    if (ExtEscape(hdc, GET_PS_FEATURESETTING, sizeof(param), param_char_ptr,
                  sizeof(param_out), param_out_char_ptr) > 0) {
      if (param_out < 2 || param_out > 3)
        return false;

      *level = param_out;
      return true;
    }
  }

  // If it looks like a PS printer.
  if (HasEscapeSupprt(hdc, POSTSCRIPT_PASSTHROUGH) &&
      HasEscapeSupprt(hdc, POSTSCRIPT_DATA)) {
    *level = 2;
    return true;
  }

  return false;
}

bool IsPrinterXPS(HDC hdc) {
  static constexpr char kXPSDriver[] =
      "http://schemas.microsoft.com/xps/2005/06";
  return IsTechnology(hdc, kXPSDriver);
}

}  // namespace

// static
void PrintSettingsInitializerWin::InitPrintSettings(
    HDC hdc,
    const DEVMODE& dev_mode,
    PrintSettings* print_settings) {
  DCHECK(hdc);
  DCHECK(print_settings);

  print_settings->SetOrientation(dev_mode.dmOrientation == DMORIENT_LANDSCAPE);

  int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
  print_settings->set_dpi(dpi);

  const int kAlphaCaps = SB_CONST_ALPHA | SB_PIXEL_ALPHA;
  print_settings->set_supports_alpha_blend(
    (GetDeviceCaps(hdc, SHADEBLENDCAPS) & kAlphaCaps) == kAlphaCaps);

  // No printer device is known to advertise different dpi in X and Y axis; even
  // the fax device using the 200x100 dpi setting. It's ought to break so many
  // applications that it's not even needed to care about. Blink doesn't support
  // different dpi settings in X and Y axis.
  DCHECK_EQ(dpi, GetDeviceCaps(hdc, LOGPIXELSY));

  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORX), 0);
  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORY), 0);

  // Initialize |page_setup_device_units_|.
  gfx::Size physical_size_device_units(GetDeviceCaps(hdc, PHYSICALWIDTH),
                                       GetDeviceCaps(hdc, PHYSICALHEIGHT));
  gfx::Rect printable_area_device_units(GetDeviceCaps(hdc, PHYSICALOFFSETX),
                                        GetDeviceCaps(hdc, PHYSICALOFFSETY),
                                        GetDeviceCaps(hdc, HORZRES),
                                        GetDeviceCaps(hdc, VERTRES));

  // Sanity check the printable_area: we've seen crashes caused by a printable
  // area rect of 0, 0, 0, 0, so it seems some drivers don't set it.
  if (printable_area_device_units.IsEmpty() ||
      !gfx::Rect(physical_size_device_units).Contains(
          printable_area_device_units)) {
    printable_area_device_units = gfx::Rect(physical_size_device_units);
  }
  DCHECK_EQ(print_settings->device_units_per_inch(), dpi);
  print_settings->SetPrinterPrintableArea(physical_size_device_units,
                                          printable_area_device_units,
                                          false);
  if (IsPrinterXPS(hdc)) {
    print_settings->set_printer_type(PrintSettings::PrinterType::TYPE_XPS);
    return;
  }
  int level;
  if (IsPrinterPostScript(hdc, &level)) {
    if (level == 2) {
      print_settings->set_printer_type(
          PrintSettings::PrinterType::TYPE_POSTSCRIPT_LEVEL2);
      return;
    }
    DCHECK_EQ(3, level);
    print_settings->set_printer_type(
        PrintSettings::PrinterType::TYPE_POSTSCRIPT_LEVEL3);
    return;
  }
  print_settings->set_printer_type(PrintSettings::PrinterType::TYPE_NONE);
}

}  // namespace printing

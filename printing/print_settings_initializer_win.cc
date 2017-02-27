// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer_win.h"

#include <windows.h>

#include "printing/print_settings.h"

namespace printing {

namespace {

bool HasEscapeSupport(HDC hdc, DWORD escape) {
  const char* ptr = reinterpret_cast<const char*>(&escape);
  return ExtEscape(hdc, QUERYESCSUPPORT, sizeof(escape), ptr, 0, nullptr) > 0;
}

bool IsTechnology(HDC hdc, const char* technology) {
  if (::GetDeviceCaps(hdc, TECHNOLOGY) != DT_RASPRINTER)
    return false;

  if (!HasEscapeSupport(hdc, GETTECHNOLOGY))
    return false;

  char buf[256];
  memset(buf, 0, sizeof(buf));
  if (ExtEscape(hdc, GETTECHNOLOGY, 0, nullptr, sizeof(buf) - 1, buf) <= 0)
    return false;
  return strcmp(buf, technology) == 0;
}

void SetPrinterToGdiMode(HDC hdc) {
  // Try to set to GDI centric mode
  DWORD mode = PSIDENT_GDICENTRIC;
  const char* ptr = reinterpret_cast<const char*>(&mode);
  ExtEscape(hdc, POSTSCRIPT_IDENTIFY, sizeof(DWORD), ptr, 0, nullptr);
}

int GetPrinterPostScriptLevel(HDC hdc) {
  constexpr int param = FEATURESETTING_PSLEVEL;
  const char* param_char_ptr = reinterpret_cast<const char*>(&param);
  int param_out = 0;
  char* param_out_char_ptr = reinterpret_cast<char*>(&param_out);
  if (ExtEscape(hdc, GET_PS_FEATURESETTING, sizeof(param), param_char_ptr,
                sizeof(param_out), param_out_char_ptr) > 0) {
    return param_out;
  }
  return 0;
}

bool IsPrinterPostScript(HDC hdc, int* level) {
  static constexpr char kPostScriptDriver[] = "PostScript";

  // If printer does not support POSTSCRIPT_IDENTIFY, it cannot be set to GDI
  // mode to check the language level supported. See if it looks like a
  // postscript printer and supports the postscript functions that are
  // supported in compatability mode. If so set to level 2 postscript.
  if (!HasEscapeSupport(hdc, POSTSCRIPT_IDENTIFY)) {
    if (!IsTechnology(hdc, kPostScriptDriver))
      return false;
    if (!HasEscapeSupport(hdc, POSTSCRIPT_PASSTHROUGH) ||
        !HasEscapeSupport(hdc, POSTSCRIPT_DATA)) {
      return false;
    }
    *level = 2;
    return true;
  }

  // Printer supports POSTSCRIPT_IDENTIFY so we can assume it has a postscript
  // driver. Set the printer to GDI mode in order to query the postscript
  // level. Use GDI mode instead of PostScript mode so that if level detection
  // fails or returns language level < 2 we can fall back to normal printing.
  // Note: This escape must be called before other escapes.
  SetPrinterToGdiMode(hdc);
  if (!HasEscapeSupport(hdc, GET_PS_FEATURESETTING)) {
    // Can't query the level, use level 2 to be safe
    *level = 2;
    return true;
  }

  // Get the language level. If invalid or < 2, return false to set printer to
  // normal printing mode.
  *level = GetPrinterPostScriptLevel(hdc);
  return *level == 2 || *level == 3;
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
  // Check for postscript first so that we can change the mode with the
  // first command.
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
  if (IsPrinterXPS(hdc)) {
    print_settings->set_printer_type(PrintSettings::PrinterType::TYPE_XPS);
    return;
  }
  print_settings->set_printer_type(PrintSettings::PrinterType::TYPE_NONE);
}

}  // namespace printing

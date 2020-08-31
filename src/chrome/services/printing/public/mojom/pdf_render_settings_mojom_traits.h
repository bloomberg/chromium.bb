// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_RENDER_SETTINGS_MOJOM_TRAITS_H_
#define CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_RENDER_SETTINGS_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "chrome/services/printing/public/mojom/pdf_render_settings.mojom.h"
#include "printing/pdf_render_settings.h"

namespace mojo {

template <>
struct EnumTraits<printing::mojom::PdfRenderSettings::Mode,
                  printing::PdfRenderSettings::Mode> {
  static printing::mojom::PdfRenderSettings::Mode ToMojom(
      printing::PdfRenderSettings::Mode mode) {
    using MojomMode = printing::mojom::PdfRenderSettings::Mode;
    using PrintMode = printing::PdfRenderSettings::Mode;
    switch (mode) {
      case PrintMode::NORMAL:
        return MojomMode::NORMAL;
#if defined(OS_WIN)
      case PrintMode::TEXTONLY:
        return MojomMode::TEXTONLY;
      case PrintMode::GDI_TEXT:
        return MojomMode::GDI_TEXT;
      case PrintMode::POSTSCRIPT_LEVEL2:
        return MojomMode::POSTSCRIPT_LEVEL2;
      case PrintMode::POSTSCRIPT_LEVEL3:
        return MojomMode::POSTSCRIPT_LEVEL3;
      case PrintMode::EMF_WITH_REDUCED_RASTERIZATION:
        return MojomMode::EMF_WITH_REDUCED_RASTERIZATION;
      case PrintMode::EMF_WITH_REDUCED_RASTERIZATION_AND_GDI_TEXT:
        return MojomMode::EMF_WITH_REDUCED_RASTERIZATION_AND_GDI_TEXT;
#endif
    }
    NOTREACHED() << "Unknown mode " << static_cast<int>(mode);
    return printing::mojom::PdfRenderSettings::Mode::NORMAL;
  }

  static bool FromMojom(printing::mojom::PdfRenderSettings::Mode input,
                        printing::PdfRenderSettings::Mode* output) {
    using MojomMode = printing::mojom::PdfRenderSettings::Mode;
    using PrintMode = printing::PdfRenderSettings::Mode;
    switch (input) {
      case MojomMode::NORMAL:
        *output = PrintMode::NORMAL;
        return true;
#if defined(OS_WIN)
      case MojomMode::TEXTONLY:
        *output = PrintMode::TEXTONLY;
        return true;
      case MojomMode::GDI_TEXT:
        *output = PrintMode::GDI_TEXT;
        return true;
      case MojomMode::POSTSCRIPT_LEVEL2:
        *output = PrintMode::POSTSCRIPT_LEVEL2;
        return true;
      case MojomMode::POSTSCRIPT_LEVEL3:
        *output = PrintMode::POSTSCRIPT_LEVEL3;
        return true;
      case MojomMode::EMF_WITH_REDUCED_RASTERIZATION:
        *output = PrintMode::EMF_WITH_REDUCED_RASTERIZATION;
        return true;
      case MojomMode::EMF_WITH_REDUCED_RASTERIZATION_AND_GDI_TEXT:
        *output = PrintMode::EMF_WITH_REDUCED_RASTERIZATION_AND_GDI_TEXT;
        return true;
#endif
    }
    NOTREACHED() << "Unknown mode " << static_cast<int>(input);
    return false;
  }
};

template <>
class StructTraits<printing::mojom::PdfRenderSettingsDataView,
                   printing::PdfRenderSettings> {
 public:
  static gfx::Rect area(const printing::PdfRenderSettings& settings) {
    return settings.area;
  }
  static gfx::Point offsets(const printing::PdfRenderSettings& settings) {
    return settings.offsets;
  }
  static gfx::Size dpi(const printing::PdfRenderSettings& settings) {
    return settings.dpi;
  }
  static bool autorotate(const printing::PdfRenderSettings& settings) {
    return settings.autorotate;
  }
  static bool use_color(const printing::PdfRenderSettings& settings) {
    return settings.use_color;
  }
  static printing::PdfRenderSettings::Mode mode(
      const printing::PdfRenderSettings& settings) {
    return settings.mode;
  }

  static bool Read(printing::mojom::PdfRenderSettingsDataView data,
                   printing::PdfRenderSettings* out_settings);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_RENDER_SETTINGS_MOJOM_TRAITS_H_

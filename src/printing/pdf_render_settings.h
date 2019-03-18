// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_RENDER_SETTINGS_H_
#define PRINTING_PDF_RENDER_SETTINGS_H_

#include "build/build_config.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

namespace printing {

// Defining PDF rendering settings.
struct PdfRenderSettings {
  enum Mode {
    NORMAL = 0,
#if defined(OS_WIN)
    TEXTONLY,
    GDI_TEXT,
    POSTSCRIPT_LEVEL2,
    POSTSCRIPT_LEVEL3,
    LAST = POSTSCRIPT_LEVEL3,
#else
    LAST = NORMAL,
#endif
  };

  PdfRenderSettings()
      : autorotate(false), use_color(true), mode(Mode::NORMAL) {}
  PdfRenderSettings(const gfx::Rect& area,
                    const gfx::Point& offsets,
                    const gfx::Size& dpi,
                    bool autorotate,
                    bool use_color,
                    Mode mode)
      : area(area),
        offsets(offsets),
        dpi(dpi),
        autorotate(autorotate),
        use_color(use_color),
        mode(mode) {}
  ~PdfRenderSettings() {}

  gfx::Rect area;
  gfx::Point offsets;
  gfx::Size dpi;
  bool autorotate;
  bool use_color;
  Mode mode;
};

}  // namespace printing

namespace IPC {
template <>
struct ParamTraits<printing::PdfRenderSettings> {
  typedef printing::PdfRenderSettings param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.area);
    WriteParam(m, p.offsets);
    WriteParam(m, p.dpi);
    WriteParam(m, p.autorotate);
    WriteParam(m, (int) p.mode);
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int mode_int = 0;
    bool ret = ReadParam(m, iter, &r->area) &&
        ReadParam(m, iter, &r->offsets) &&
        ReadParam(m, iter, &r->dpi) &&
        ReadParam(m, iter, &r->autorotate) &&
        ReadParam(m, iter, &mode_int);
        r->mode = static_cast<printing::PdfRenderSettings::Mode>(mode_int);
    return ret;
  }

  static void Log(const param_type& p, std::string* l) {
    LogParam(p.area, l);
    l->append(", ");
    LogParam(p.offsets, l);
    l->append(", ");
    LogParam(p.dpi, l);
    l->append(", ");
    LogParam(p.autorotate, l);
    l->append(", ");
    LogParam((int)p.mode, l);
  }
};

}  // namespace IPC

#endif  // PRINTING_PDF_RENDER_SETTINGS_H_

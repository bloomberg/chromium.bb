// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/color/gfx_param_traits.h"

#include "ui/gfx/color_space.h"
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"

namespace IPC {

void ParamTraits<gfx::ColorSpace>::Write(base::Pickle* m,
                                         const gfx::ColorSpace& p) {
  WriteParam(m, p.primaries_);
  WriteParam(m, p.transfer_);
  WriteParam(m, p.matrix_);
  WriteParam(m, p.range_);
  WriteParam(m, p.custom_primary_matrix_);
  WriteParam(m, p.transfer_params_);
}

bool ParamTraits<gfx::ColorSpace>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        gfx::ColorSpace* r) {
  if (!ReadParam(m, iter, &r->primaries_))
    return false;
  if (!ReadParam(m, iter, &r->transfer_))
    return false;
  if (!ReadParam(m, iter, &r->matrix_))
    return false;
  if (!ReadParam(m, iter, &r->range_))
    return false;
  if (!ReadParam(m, iter, &r->custom_primary_matrix_))
    return false;
  if (!ReadParam(m, iter, &r->transfer_params_))
    return false;
  return true;
}

void ParamTraits<gfx::ColorSpace>::Log(const gfx::ColorSpace& p,
                                       std::string* l) {
  l->append("<gfx::ColorSpace>");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC

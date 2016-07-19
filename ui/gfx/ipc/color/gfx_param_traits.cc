// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/color/gfx_param_traits.h"

#include "ui/gfx/color_space.h"
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"

namespace IPC {

void ParamTraits<gfx::ColorSpace>::GetSize(base::PickleSizer* s,
                                           const gfx::ColorSpace& p) {
  GetParamSize(s, p.type_);
  switch (p.type_) {
    case gfx::ColorSpace::Type::UNDEFINED:
      break;
    case gfx::ColorSpace::Type::ICC_PROFILE: {
      GetParamSize(s, p.GetICCProfile());
      break;
    }
  }
}

void ParamTraits<gfx::ColorSpace>::Write(base::Pickle* m,
                                         const gfx::ColorSpace& p) {
  std::vector<char> icc_profile = p.GetICCProfile();
  WriteParam(m, p.type_);
  switch (p.type_) {
    case gfx::ColorSpace::Type::UNDEFINED:
      break;
    case gfx::ColorSpace::Type::ICC_PROFILE: {
      WriteParam(m, p.GetICCProfile());
      break;
    }
  }
}

bool ParamTraits<gfx::ColorSpace>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        gfx::ColorSpace* r) {
  if (!ReadParam(m, iter, &r->type_))
    return false;
  switch (r->type_) {
    case gfx::ColorSpace::Type::UNDEFINED:
      return true;
    case gfx::ColorSpace::Type::ICC_PROFILE: {
      std::vector<char> icc_profile;
      if (!ReadParam(m, iter, &icc_profile))
        return false;
      *r = gfx::ColorSpace::FromICCProfile(icc_profile);
      return true;
    }
  }
  return false;
}

void ParamTraits<gfx::ColorSpace>::Log(const gfx::ColorSpace& p,
                                       std::string* l) {
  l->append("<gfx::ColorSpace>");
}

}  // namespace IPC

// Generate param traits size methods.
#include "ipc/param_traits_size_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}

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

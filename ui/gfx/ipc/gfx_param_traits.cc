// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/gfx_param_traits.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "ui/gfx/range/range.h"

#if defined(OS_MACOSX)
#include "ipc/mach_port_mac.h"
#endif

namespace IPC {

void ParamTraits<gfx::Range>::Write(base::Pickle* m, const gfx::Range& r) {
  m->WriteUInt32(r.start());
  m->WriteUInt32(r.end());
}

bool ParamTraits<gfx::Range>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   gfx::Range* r) {
  uint32_t start, end;
  if (!iter->ReadUInt32(&start) || !iter->ReadUInt32(&end))
    return false;
  r->set_start(start);
  r->set_end(end);
  return true;
}

void ParamTraits<gfx::Range>::Log(const gfx::Range& r, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", r.start(), r.end()));
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void ParamTraits<gfx::ScopedRefCountedIOSurfaceMachPort>::Write(
    base::Pickle* m,
    const param_type p) {
  MachPortMac mach_port_mac(p.get());
  ParamTraits<MachPortMac>::Write(m, mach_port_mac);
}

bool ParamTraits<gfx::ScopedRefCountedIOSurfaceMachPort>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  MachPortMac mach_port_mac;
  if (!ParamTraits<MachPortMac>::Read(m, iter, &mach_port_mac))
    return false;
  r->reset(mach_port_mac.get_mach_port());
  return true;
}

void ParamTraits<gfx::ScopedRefCountedIOSurfaceMachPort>::Log(
    const param_type& p,
    std::string* l) {
  l->append("IOSurface Mach send right: ");
  LogParam(p.get(), l);
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/gfx_param_traits_macros.h"
}  // namespace IPC

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "ppapi/proxy/pepper_file_messages.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#include "ppapi/proxy/pepper_file_messages.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#include "ppapi/proxy/pepper_file_messages.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "ppapi/proxy/pepper_file_messages.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "ppapi/proxy/pepper_file_messages.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "ppapi/proxy/pepper_file_messages.h"
}  // namespace IPC

namespace IPC {

void ParamTraits<ppapi::PepperFilePath>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, static_cast<unsigned>(p.domain()));
  WriteParam(m, p.path());
}

bool ParamTraits<ppapi::PepperFilePath>::Read(const Message* m,
                                              PickleIterator* iter,
                                              param_type* p) {
  unsigned domain;
  FilePath path;
  if (!ReadParam(m, iter, &domain) || !ReadParam(m, iter, &path))
    return false;
  if (domain > ppapi::PepperFilePath::DOMAIN_MAX_VALID)
    return false;

  *p = ppapi::PepperFilePath(
      static_cast<ppapi::PepperFilePath::Domain>(domain), path);
  return true;
}

void ParamTraits<ppapi::PepperFilePath>::Log(const param_type& p,
                                             std::string* l) {
  l->append("(");
  LogParam(static_cast<unsigned>(p.domain()), l);
  l->append(", ");
  LogParam(p.path(), l);
  l->append(")");
}

}  // namespace IPC

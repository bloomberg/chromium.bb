// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_INFO_IPC_LATENCY_INFO_PARAM_TRAITS_H_
#define UI_LATENCY_INFO_IPC_LATENCY_INFO_PARAM_TRAITS_H_

#include "ui/latency_info/ipc/latency_info_ipc_export.h"
#include "ui/latency_info/latency_info.h"

namespace IPC {
template <>
struct LATENCY_INFO_IPC_EXPORT ParamTraits<ui::LatencyInfo> {
  typedef ui::LatencyInfo param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};
}  // namespace IPC

#endif  // UI_LATENCY_INFO_IPC_LATENCY_INFO_PARAM_TRAITS_H_

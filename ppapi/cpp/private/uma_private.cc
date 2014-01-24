// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/uma_private.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_UMA_Private_0_2>() {
  return PPB_UMA_PRIVATE_INTERFACE_0_2;
}

}  // namespace

UMAPrivate::UMAPrivate() {
}

UMAPrivate::UMAPrivate(
    const InstanceHandle& instance) : instance_(instance.pp_instance()) {
}

UMAPrivate::~UMAPrivate() {
}

bool UMAPrivate::IsAvailable() {
  return has_interface<PPB_UMA_Private_0_2>();
}

void UMAPrivate::HistogramCustomTimes(const std::string& name,
                                      int64_t sample,
                                      int64_t min,
                                      int64_t max,
                                      uint32_t bucket_count) {
  if (!IsAvailable())
    return;
  get_interface<PPB_UMA_Private_0_2>()->
      HistogramCustomTimes(instance_, pp::Var(name).pp_var(),
                           sample, min, max, bucket_count);
}

void UMAPrivate::HistogramCustomCounts(const std::string& name,
                                       int32_t sample,
                                       int32_t min,
                                       int32_t max,
                                       uint32_t bucket_count) {
  if (!IsAvailable())
    return;
  get_interface<PPB_UMA_Private_0_2>()->
      HistogramCustomCounts(instance_, pp::Var(name).pp_var(),
                            sample, min, max, bucket_count);
}

void UMAPrivate::HistogramEnumeration(const std::string& name,
                                      int32_t sample,
                                      int32_t boundary_value) {
  if (!IsAvailable())
    return;
  get_interface<PPB_UMA_Private_0_2>()->
      HistogramEnumeration(instance_, pp::Var(name).pp_var(),
                           sample, boundary_value);
}

}  // namespace pp

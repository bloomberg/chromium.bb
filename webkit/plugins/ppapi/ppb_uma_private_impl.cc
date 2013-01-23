// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_uma_private_impl.h"

#include "base/metrics/histogram.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/shared_impl/var.h"
#include "webkit/glue/webkit_glue.h"

using ppapi::StringVar;

namespace webkit {
namespace ppapi {

namespace {

#define RETURN_IF_BAD_ARGS(_name, _sample, _min, _max, _bucket_count) \
  do { \
    if (_name.type != PP_VARTYPE_STRING || _name.value.as_id == 0) \
      return; \
    if (_min >= _max) \
      return; \
    if (_bucket_count <= 1) \
      return; \
  } while (0)

void HistogramCustomTimes(PP_Var name,
                          int64_t sample,
                          int64_t min, int64_t max,
                          uint32_t bucket_count) {
  RETURN_IF_BAD_ARGS(name, sample, min, max, bucket_count);

  StringVar* name_string = StringVar::FromPPVar(name);
  if (name_string == NULL)
    return;
  base::HistogramBase* counter =
      base::Histogram::FactoryTimeGet(
          name_string->value(),
          base::TimeDelta::FromMilliseconds(min),
          base::TimeDelta::FromMilliseconds(max),
          bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddTime(base::TimeDelta::FromMilliseconds(sample));
}

void HistogramCustomCounts(PP_Var name,
                           int32_t sample,
                           int32_t min, int32_t max,
                           uint32_t bucket_count) {
  RETURN_IF_BAD_ARGS(name, sample, min, max, bucket_count);

  StringVar* name_string = StringVar::FromPPVar(name);
  if (name_string == NULL)
    return;
  base::HistogramBase* counter =
      base::Histogram::FactoryGet(
          name_string->value(),
          min,
          max,
          bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void HistogramEnumeration(PP_Var name,
                          int32_t sample,
                          int32_t boundary_value) {
  RETURN_IF_BAD_ARGS(name, sample, 1, boundary_value, boundary_value + 1);

  StringVar* name_string = StringVar::FromPPVar(name);
  if (name_string == NULL)
    return;
  base::HistogramBase* counter =
      base::LinearHistogram::FactoryGet(
          name_string->value(),
          1,
          boundary_value,
          boundary_value + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

}  // namespace

const PPB_UMA_Private ppb_uma = {
  &HistogramCustomTimes,
  &HistogramCustomCounts,
  &HistogramEnumeration,
};

// static
const PPB_UMA_Private* PPB_UMA_Private_Impl::GetInterface() {
  return &ppb_uma;
}

}  // namespace ppapi
}  // namespace webkit

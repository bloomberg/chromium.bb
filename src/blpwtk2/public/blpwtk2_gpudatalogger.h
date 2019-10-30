// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDED_BLPWTK2_GPUDATALOGGER_H
#define INCLUDED_BLPWTK2_GPUDATALOGGER_H

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include <string>

namespace blpwtk2 {

class GpuDataLogger : public base::RefCountedThreadSafe<GpuDataLogger>, public content::GpuDataManagerObserver {
 public:

  static scoped_refptr<GpuDataLogger> Create();

 protected:
  friend class base::RefCountedThreadSafe<GpuDataLogger>;

  ~GpuDataLogger() override;
};

}  // namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_GPUDATALOGGER_H

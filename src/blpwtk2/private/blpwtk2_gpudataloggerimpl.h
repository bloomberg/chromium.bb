/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_GPUDATALOGGER_IMPL_H
#define INCLUDED_BLPWTK2_GPUDATALOGGER_IMPL_H

#include <blpwtk2_config.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "blpwtk2/public/blpwtk2_gpudatalogger.h"
#include <string>

namespace blpwtk2 {

class GpuDataLoggerImpl : public GpuDataLogger {
 public:
  GpuDataLoggerImpl();

  // Called for any observers whenever there is a log message added to the GPU data.
  void OnAddLogMessage(int level, const std::string& header, const std::string& message) override;


 private:
  ~GpuDataLoggerImpl() override;

  DISALLOW_COPY_AND_ASSIGN(GpuDataLoggerImpl);
};

}  // namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_GPUDATALOGGER_IMPL_H

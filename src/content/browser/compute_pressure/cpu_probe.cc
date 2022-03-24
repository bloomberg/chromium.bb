// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/cpu_probe.h"

#include <memory>

#include "base/sequence_checker.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/browser/compute_pressure/cpu_probe_linux.h"

namespace content {

constexpr ComputePressureSample CpuProbe::kUnsupportedValue;

CpuProbe::CpuProbe() = default;
CpuProbe::~CpuProbe() = default;

class NullCpuProbe : public CpuProbe {
 public:
  NullCpuProbe() { DETACH_FROM_SEQUENCE(sequence_checker_); }
  ~NullCpuProbe() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // CpuProbe implementation.
  void Update() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Checks that
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
  }
  ComputePressureSample LastSample() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return CpuProbe::kUnsupportedValue;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

// Default implementation for platforms that don't have one.

// static
std::unique_ptr<CpuProbe> CpuProbe::Create() {
#if defined(OS_ANDROID)
  return nullptr;
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  return CpuProbeLinux::Create();
#else
  return std::make_unique<NullCpuProbe>();
#endif  // defined(OS_ANDROID)
}

}  // namespace content

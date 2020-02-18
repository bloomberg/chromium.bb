// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

#include "base/system/sys_info.h"

#if defined(OS_CHROMEOS)
#include "chromeos/system/cpu_temperature_reader.h"
#endif  // defined(OS_CHROMEOS)

namespace extensions {

using api::system_cpu::CpuInfo;

// Static member intialization.
base::LazyInstance<scoped_refptr<CpuInfoProvider>>::DestructorAtExit
    CpuInfoProvider::provider_ = LAZY_INSTANCE_INITIALIZER;

CpuInfoProvider::CpuInfoProvider() {
}

CpuInfoProvider::~CpuInfoProvider() {
}

void CpuInfoProvider::InitializeForTesting(
    scoped_refptr<CpuInfoProvider> provider) {
  DCHECK(provider.get() != NULL);
  provider_.Get() = provider;
}

bool CpuInfoProvider::QueryInfo() {
  info_.num_of_processors = base::SysInfo::NumberOfProcessors();
  info_.arch_name = base::SysInfo::OperatingSystemArchitecture();
  info_.model_name = base::SysInfo::CPUModelName();
  info_.features = GetFeatures();

  info_.processors.clear();
  // Fill in the correct number of uninitialized ProcessorInfos.
  for (int i = 0; i < info_.num_of_processors; ++i)
    info_.processors.push_back(api::system_cpu::ProcessorInfo());
  // Initialize the ProcessorInfos, or return an empty array if that fails.
  if (!QueryCpuTimePerProcessor(&info_.processors))
    info_.processors.clear();

#if defined(OS_CHROMEOS)
  using CPUTemperatureInfo =
      chromeos::system::CPUTemperatureReader::CPUTemperatureInfo;
  std::vector<CPUTemperatureInfo> cpu_temp_info =
      chromeos::system::CPUTemperatureReader().GetCPUTemperatures();
  info_.temperatures.clear();
  info_.temperatures.reserve(cpu_temp_info.size());
  for (const CPUTemperatureInfo& info : cpu_temp_info) {
    info_.temperatures.push_back(info.temp_celsius);
  }
#endif  // defined(OS_CHROMEOS)

  return true;
}

std::vector<std::string> CpuInfoProvider::GetFeatures() const {
  std::vector<std::string> features;
  // These are the feature codes used by /proc/cpuinfo on Linux.
  if (cpu_.has_mmx())
    features.push_back("mmx");
  if (cpu_.has_sse())
    features.push_back("sse");
  if (cpu_.has_sse2())
    features.push_back("sse2");
  if (cpu_.has_sse3())
    features.push_back("sse3");
  if (cpu_.has_ssse3())
    features.push_back("ssse3");
  if (cpu_.has_sse41())
    features.push_back("sse4_1");
  if (cpu_.has_sse42())
    features.push_back("sse4_2");
  if (cpu_.has_avx())
    features.push_back("avx");
  return features;
}

// static
CpuInfoProvider* CpuInfoProvider::Get() {
  if (provider_.Get().get() == NULL)
    provider_.Get() = new CpuInfoProvider();
  return provider_.Get().get();
}

}  // namespace extensions

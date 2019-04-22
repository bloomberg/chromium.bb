// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/nvml_info.h"

#include <windows.h>

#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/version.h"
#include "third_party/nvml/nvml.h"

namespace {

const unsigned int kDriverVersionCapacity = 80u;

}  // anonymous namespace

typedef decltype(&nvmlInit) INITPROC;
typedef decltype(&nvmlShutdown) SHUTDOWNPROC;
typedef decltype(&nvmlSystemGetDriverVersion) GETDRIVERVERSIONPROC;
typedef decltype(&nvmlDeviceGetCount) DEVICEGETCOUNTPROC;
typedef decltype(&nvmlDeviceGetHandleByIndex) DEVICEGETHANDLEBYINDEXPROC;
typedef decltype(&nvmlDeviceGetPciInfo) DEVICEGETPCIINFOPROC;
typedef decltype(
    &nvmlDeviceGetCudaComputeCapability) DEVICEGETCUDACOMPUTECAPABILITYPROC;

bool GetNvmlDeviceInfo(uint32_t pci_device_id,
                       std::string* driver_version,
                       int* major_cuda_compute_capability,
                       int* minor_cuda_compute_capability) {
  DCHECK(major_cuda_compute_capability);
  DCHECK(minor_cuda_compute_capability);
  *major_cuda_compute_capability = 0;
  *minor_cuda_compute_capability = 0;

  base::FilePath dll_path;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILES6432, &dll_path)) {
    return false;
  }
  dll_path = dll_path.Append(L"NVIDIA Corporation\\NVSMI\\nvml.dll");

  std::unique_ptr<FileVersionInfoWin> file_version_info =
      FileVersionInfoWin::CreateFileVersionInfoWin(dll_path);
  if (!file_version_info) {
    return false;
  }

  std::vector<uint32_t> components = {
      {HIWORD(file_version_info->fixed_file_info()->dwFileVersionMS),
       LOWORD(file_version_info->fixed_file_info()->dwFileVersionMS),
       HIWORD(file_version_info->fixed_file_info()->dwFileVersionLS),
       LOWORD(file_version_info->fixed_file_info()->dwFileVersionLS)}};
  base::Version dll_version(components);

  // There was a bug that could cause crashes in 64-bit applications in earlier
  // versions of NVML, so we only use it if a new enough DLL is detected.
  // http://crbug.com/873095
  if (dll_version < base::Version("8.17.13.4800")) {
    return false;
  }

  HINSTANCE hinstNVML = LoadLibrary(dll_path.value().data());

  if (!hinstNVML) {
    return false;
  }

  INITPROC fnNvmlInit = (INITPROC)GetProcAddress(hinstNVML, "nvmlInit");
  SHUTDOWNPROC fnNvmlShutdown =
      (SHUTDOWNPROC)GetProcAddress(hinstNVML, "nvmlShutdown");
  GETDRIVERVERSIONPROC fnNvmlSystemGetDriverVersion =
      (GETDRIVERVERSIONPROC)GetProcAddress(hinstNVML,
                                           "nvmlSystemGetDriverVersion");
  DEVICEGETCOUNTPROC fnNvmlDeviceGetCount =
      (DEVICEGETCOUNTPROC)GetProcAddress(hinstNVML, "nvmlDeviceGetCount");
  DEVICEGETHANDLEBYINDEXPROC fnNvmlDeviceGetHandleByIndex =
      (DEVICEGETHANDLEBYINDEXPROC)GetProcAddress(hinstNVML,
                                                 "nvmlDeviceGetHandleByIndex");
  DEVICEGETPCIINFOPROC fnNvmlDeviceGetPciInfo =
      (DEVICEGETPCIINFOPROC)GetProcAddress(hinstNVML, "nvmlDeviceGetPciInfo");
  DEVICEGETCUDACOMPUTECAPABILITYPROC fnNvmlDeviceGetCudaComputeCapability =
      (DEVICEGETCUDACOMPUTECAPABILITYPROC)GetProcAddress(
          hinstNVML, "nvmlDeviceGetCudaComputeCapability");

  if (!fnNvmlInit || !fnNvmlShutdown || !fnNvmlSystemGetDriverVersion) {
    return false;
  }

  nvmlReturn_t result = fnNvmlInit();
  if (NVML_SUCCESS != result) {
    return false;
  }

  char driver_version_temp[kDriverVersionCapacity];

  result =
      fnNvmlSystemGetDriverVersion(driver_version_temp, kDriverVersionCapacity);
  if (NVML_SUCCESS != result) {
    fnNvmlShutdown();
    return false;
  }

  *driver_version = driver_version_temp;

  if (fnNvmlDeviceGetCount && fnNvmlDeviceGetHandleByIndex &&
      fnNvmlDeviceGetPciInfo && fnNvmlDeviceGetCudaComputeCapability) {
    unsigned int device_count;
    result = fnNvmlDeviceGetCount(&device_count);
    if (NVML_SUCCESS == result) {
      for (unsigned int i = 0; i < device_count; i++) {
        nvmlDevice_t device;
        nvmlPciInfo_t pci;

        result = fnNvmlDeviceGetHandleByIndex(i, &device);
        if (NVML_SUCCESS != result) {
          continue;
        }

        result = fnNvmlDeviceGetPciInfo(device, &pci);
        if (NVML_SUCCESS != result) {
          continue;
        }

        // We're looking for a particular PCI device.
        if (pci.pciDeviceId != ((pci_device_id << 16) | 0x10deu)) {
          continue;
        }

        result = fnNvmlDeviceGetCudaComputeCapability(
            device, major_cuda_compute_capability,
            minor_cuda_compute_capability);
        if (NVML_SUCCESS != result) {
          *major_cuda_compute_capability = 0;
          *minor_cuda_compute_capability = 0;
        }
      }
    }
  }

  result = fnNvmlShutdown();
  if (NVML_SUCCESS != result) {
    return false;
  }
  return true;
}

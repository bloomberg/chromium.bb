// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/processor_metrics.h"

#include <objbase.h>
#include <wbemidl.h>
#include <wrl/client.h>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/wmi.h"

using base::win::ScopedBstr;
using base::win::ScopedVariant;
using Microsoft::WRL::ComPtr;

namespace {

HRESULT GetClassObject(ComPtr<IWbemClassObject> class_object,
                       const wchar_t* const name,
                       ScopedVariant* variant) {
  return class_object->Get(name, 0, variant->Receive(), 0, 0);
}

void RecordHypervStatusFromWMI(const ComPtr<IWbemServices>& services) {
  static constexpr wchar_t kHypervPresent[] = L"HypervisorPresent";
  static constexpr wchar_t kQueryProcessor[] =
      L"SELECT HypervisorPresent FROM Win32_ComputerSystem";

  ComPtr<IEnumWbemClassObject> enumerator_computer_system;
  HRESULT hr =
      services->ExecQuery(ScopedBstr(L"WQL").Get(), ScopedBstr(kQueryProcessor).Get(),
                          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                          nullptr, &enumerator_computer_system);
  if (FAILED(hr) || !enumerator_computer_system.Get())
    return;

  ComPtr<IWbemClassObject> class_object;
  ULONG items_returned = 0;
  hr = enumerator_computer_system->Next(WBEM_INFINITE, 1, &class_object,
                                        &items_returned);
  if (FAILED(hr) || !items_returned)
    return;

  ScopedVariant hyperv_present;
  hr = GetClassObject(class_object, kHypervPresent, &hyperv_present);
  if (SUCCEEDED(hr) && hyperv_present.type() == VT_BOOL) {
    base::UmaHistogramBoolean("Windows.HypervPresent",
                              V_BOOL(hyperv_present.ptr()));
  }
}

void RecordProcessorMetricsFromWMI(const ComPtr<IWbemServices>& services) {
  static constexpr wchar_t kProcessorFamily[] = L"Family";
  static constexpr wchar_t kProcessorVirtualizationFirmwareEnabled[] =
      L"VirtualizationFirmwareEnabled";
  static constexpr wchar_t kQueryProcessor[] =
      L"SELECT Family,VirtualizationFirmwareEnabled FROM Win32_Processor";

  ComPtr<IEnumWbemClassObject> enumerator_processor;
  HRESULT hr =
      services->ExecQuery(ScopedBstr(L"WQL").Get(), ScopedBstr(kQueryProcessor).Get(),
                          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                          nullptr, &enumerator_processor);
  if (FAILED(hr) || !enumerator_processor.Get())
    return;

  ComPtr<IWbemClassObject> class_object;
  ULONG items_returned = 0;
  hr = enumerator_processor->Next(WBEM_INFINITE, 1, &class_object,
                                  &items_returned);
  if (FAILED(hr) || !items_returned)
    return;

  ScopedVariant processor_family;
  hr = GetClassObject(class_object, kProcessorFamily, &processor_family);
  if (SUCCEEDED(hr) && processor_family.type() == VT_I4) {
    base::UmaHistogramSparse("Windows.ProcessorFamily",
                             V_I4(processor_family.ptr()));
  }

  ScopedVariant enabled;
  hr = GetClassObject(class_object, kProcessorVirtualizationFirmwareEnabled,
                      &enabled);
  if (SUCCEEDED(hr) && enabled.type() == VT_BOOL) {
    base::UmaHistogramBoolean("Windows.ProcessorVirtualizationFirmwareEnabled",
                              V_BOOL(enabled.ptr()));
  }
}

}  // namespace

void RecordProcessorMetrics() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ComPtr<IWbemServices> wmi_services;
  if (!base::win::CreateLocalWmiConnection(true, &wmi_services))
    return;
  RecordProcessorMetricsFromWMI(wmi_services);
  RecordHypervStatusFromWMI(wmi_services);
}

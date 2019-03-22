// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_scoped_service_handle.h"

#include <windows.h>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/test/test_executables.h"
#include "chrome/chrome_cleaner/test/test_strings.h"

namespace chrome_cleaner {

TestScopedServiceHandle::~TestScopedServiceHandle() {
  StopAndDelete();
}

bool TestScopedServiceHandle::InstallService() {
  // Construct the full-path of the test service.
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_EXE, &module_path)) {
    PLOG(ERROR) << "Cannot retrieve module name.";
    return false;
  }
  module_path = module_path.Append(kTestServiceExecutableName);

  return InstallCustomService(kServiceName, module_path);
}

bool TestScopedServiceHandle::InstallCustomService(
    const base::string16& service_name,
    const base::FilePath& module_path) {
  DCHECK(!service_name.empty());
  DCHECK(!service_manager_.IsValid());
  DCHECK(!service_.IsValid());
  DCHECK(service_name_.empty());

  // Make sure a service with this name doesn't already exist.
  StopAndDeleteService(service_name);

  // Get a handle to the service manager.
  service_manager_.Set(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!service_manager_.IsValid()) {
    PLOG(ERROR) << "Cannot open service manager.";
    return false;
  }

  const base::string16 service_desc =
      base::StrCat({service_name, L" - ", kServiceDescription});

  service_.Set(::CreateServiceW(
      service_manager_.Get(), service_name.c_str(), service_desc.c_str(),
      SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
      SERVICE_ERROR_NORMAL, module_path.value().c_str(), nullptr, nullptr,
      nullptr, nullptr, nullptr));
  if (!service_.IsValid()) {
    // Unable to create the service.
    PLOG(ERROR) << "Cannot create service '" << service_name << "'.";
    return false;
  }
  service_name_ = service_name;
  return true;
}

bool TestScopedServiceHandle::StartService() {
  if (!::StartService(service_.Get(), 0, nullptr))
    return false;
  SERVICE_STATUS service_status = {};
  for (int iteration = 0; iteration < kServiceQueryRetry; ++iteration) {
    if (!::QueryServiceStatus(service_.Get(), &service_status))
      return false;
    if (service_status.dwCurrentState == SERVICE_RUNNING)
      return true;
    ::Sleep(kServiceQueryWaitTimeMs);
  }
  return false;
}

bool TestScopedServiceHandle::StopAndDelete() {
  Close();
  if (service_name_.empty())
    return false;
  bool result = StopAndDeleteService(service_name_.c_str());
  service_name_.clear();
  return result;
}

void TestScopedServiceHandle::Close() {
  service_.Close();
  service_manager_.Close();
}

bool TestScopedServiceHandle::StopAndDeleteService(
    const base::string16& service_name) {
  return StopService(service_name.c_str()) &&
         DeleteService(service_name.c_str()) &&
         WaitForServiceDeleted(service_name.c_str());
}

}  // namespace chrome_cleaner

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/elevated_controller.h"

#include "base/file_version_info.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/branding.h"
#include "remoting/host/host_config.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/verify_config_window_win.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting {

namespace {

// The maximum size of the configuration file. "1MB ought to be enough" for any
// reasonable configuration we will ever need. 1MB is low enough to make
// the probability of out of memory situation fairly low. OOM is still possible
// and we will crash if it occurs.
const size_t kMaxConfigFileSize = 1024 * 1024;

// The host configuration file name.
const base::FilePath::CharType kConfigFileName[] = FILE_PATH_LITERAL("host.json");

// The unprivileged configuration file name.
const base::FilePath::CharType kUnprivilegedConfigFileName[] =
    FILE_PATH_LITERAL("host_unprivileged.json");

// The extension for the temporary file.
const base::FilePath::CharType kTempFileExtension[] = FILE_PATH_LITERAL("json~");

// The host configuration file security descriptor that enables full access to
// Local System and built-in administrators only.
const char kConfigFileSecurityDescriptor[] =
    "O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)";

const char kUnprivilegedConfigFileSecurityDescriptor[] =
    "O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AU)";

// Configuration keys.

// The configuration keys that cannot be specified in UpdateConfig().
const char* const kReadonlyKeys[] = {
  kHostIdConfigPath, kHostOwnerConfigPath, kHostOwnerEmailConfigPath,
  kXmppLoginConfigPath };

// The configuration keys whose values may be read by GetConfig().
const char* const kUnprivilegedConfigKeys[] = {
  kHostIdConfigPath, kXmppLoginConfigPath };

// Determines if the client runs in the security context that allows performing
// administrative tasks (i.e. the user belongs to the adminstrators group and
// the client runs elevated).
bool IsClientAdmin() {
  HRESULT hr = CoImpersonateClient();
  if (FAILED(hr)) {
    return false;
  }

  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  PSID administrators_group = NULL;
  BOOL result = AllocateAndInitializeSid(&nt_authority,
                                         2,
                                         SECURITY_BUILTIN_DOMAIN_RID,
                                         DOMAIN_ALIAS_RID_ADMINS,
                                         0, 0, 0, 0, 0, 0,
                                         &administrators_group);
  if (result) {
    if (!CheckTokenMembership(NULL, administrators_group, &result)) {
      result = false;
    }
    FreeSid(administrators_group);
  }

  hr = CoRevertToSelf();
  CHECK(SUCCEEDED(hr));

  return !!result;
}

// Reads and parses the configuration file up to |kMaxConfigFileSize| in
// size.
HRESULT ReadConfig(const base::FilePath& filename,
                   scoped_ptr<base::DictionaryValue>* config_out) {

  // Read raw data from the configuration file.
  base::win::ScopedHandle file(
      CreateFileW(filename.value().c_str(),
                  GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL,
                  OPEN_EXISTING,
                  FILE_FLAG_SEQUENTIAL_SCAN,
                  NULL));

  if (!file.IsValid()) {
    DWORD error = GetLastError();
    PLOG(ERROR) << "Failed to open '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  scoped_ptr<char[]> buffer(new char[kMaxConfigFileSize]);
  DWORD size = kMaxConfigFileSize;
  if (!::ReadFile(file.Get(), &buffer[0], size, &size, NULL)) {
    DWORD error = GetLastError();
    PLOG(ERROR) << "Failed to read '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  // Parse the JSON configuration, expecting it to contain a dictionary.
  std::string file_content(buffer.get(), size);
  scoped_ptr<base::Value> value(
      base::JSONReader::Read(file_content, base::JSON_ALLOW_TRAILING_COMMAS));

  base::DictionaryValue* dictionary;
  if (value.get() == NULL || !value->GetAsDictionary(&dictionary)) {
    LOG(ERROR) << "Failed to read '" << filename.value() << "'.";
    return E_FAIL;
  }

  value.release();
  config_out->reset(dictionary);
  return S_OK;
}

base::FilePath GetTempLocationFor(const base::FilePath& filename) {
  return filename.ReplaceExtension(kTempFileExtension);
}

// Writes a config file to a temporary location.
HRESULT WriteConfigFileToTemp(const base::FilePath& filename,
                              const char* security_descriptor,
                              const char* content,
                              size_t length) {
  // Create the security descriptor for the configuration file.
  ScopedSd sd = ConvertSddlToSd(security_descriptor);
  if (!sd) {
    DWORD error = GetLastError();
    PLOG(ERROR)
        << "Failed to create a security descriptor for the configuration file";
    return HRESULT_FROM_WIN32(error);
  }

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = sd.get();
  security_attributes.bInheritHandle = FALSE;

  // Create a temporary file and write configuration to it.
  base::FilePath tempname = GetTempLocationFor(filename);
  base::win::ScopedHandle file(
      CreateFileW(tempname.value().c_str(),
                  GENERIC_WRITE,
                  0,
                  &security_attributes,
                  CREATE_ALWAYS,
                  FILE_FLAG_SEQUENTIAL_SCAN,
                  NULL));

  if (!file.IsValid()) {
    DWORD error = GetLastError();
    PLOG(ERROR) << "Failed to create '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  DWORD written;
  if (!WriteFile(file.Get(), content, static_cast<DWORD>(length), &written,
                 NULL)) {
    DWORD error = GetLastError();
    PLOG(ERROR) << "Failed to write to '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

// Moves a config file from its temporary location to its permanent location.
HRESULT MoveConfigFileFromTemp(const base::FilePath& filename) {
  // Now that the configuration is stored successfully replace the actual
  // configuration file.
  base::FilePath tempname = GetTempLocationFor(filename);
  if (!MoveFileExW(tempname.value().c_str(),
                   filename.value().c_str(),
                   MOVEFILE_REPLACE_EXISTING)) {
      DWORD error = GetLastError();
      PLOG(ERROR) << "Failed to rename '" << tempname.value() << "' to '"
                  << filename.value() << "'";
      return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

// Writes the configuration file up to |kMaxConfigFileSize| in size.
HRESULT WriteConfig(const char* content, size_t length, HWND owner_window) {
  if (length > kMaxConfigFileSize) {
      return E_FAIL;
  }

  // Extract the configuration data that the user will verify.
  scoped_ptr<base::Value> config_value(base::JSONReader::Read(content));
  if (!config_value.get()) {
    return E_FAIL;
  }
  base::DictionaryValue* config_dict = NULL;
  if (!config_value->GetAsDictionary(&config_dict)) {
    return E_FAIL;
  }
  std::string email;
  if (!config_dict->GetString(kHostOwnerEmailConfigPath, &email)) {
    if (!config_dict->GetString(kHostOwnerConfigPath, &email)) {
      if (!config_dict->GetString(kXmppLoginConfigPath, &email)) {
        return E_FAIL;
      }
    }
  }
  std::string host_id, host_secret_hash;
  if (!config_dict->GetString(kHostIdConfigPath, &host_id) ||
      !config_dict->GetString(kHostSecretHashConfigPath, &host_secret_hash)) {
    return E_FAIL;
  }

  // Ask the user to verify the configuration (unless the client is admin
  // already).
  if (!IsClientAdmin()) {
    remoting::VerifyConfigWindowWin verify_win(email, host_id,
                                               host_secret_hash);
    DWORD error = verify_win.DoModal(owner_window);
    if (error != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(error);
    }
  }

  // Extract the unprivileged fields from the configuration.
  base::DictionaryValue unprivileged_config_dict;
  for (int i = 0; i < arraysize(kUnprivilegedConfigKeys); ++i) {
    const char* key = kUnprivilegedConfigKeys[i];
    base::string16 value;
    if (config_dict->GetString(key, &value)) {
      unprivileged_config_dict.SetString(key, value);
    }
  }
  std::string unprivileged_config_str;
  base::JSONWriter::Write(&unprivileged_config_dict, &unprivileged_config_str);

  // Write the full configuration file to a temporary location.
  base::FilePath full_config_file_path =
      remoting::GetConfigDir().Append(kConfigFileName);
  HRESULT hr = WriteConfigFileToTemp(full_config_file_path,
                                     kConfigFileSecurityDescriptor,
                                     content,
                                     length);
  if (FAILED(hr)) {
    return hr;
  }

  // Write the unprivileged configuration file to a temporary location.
  base::FilePath unprivileged_config_file_path =
      remoting::GetConfigDir().Append(kUnprivilegedConfigFileName);
  hr = WriteConfigFileToTemp(unprivileged_config_file_path,
                             kUnprivilegedConfigFileSecurityDescriptor,
                             unprivileged_config_str.data(),
                             unprivileged_config_str.size());
  if (FAILED(hr)) {
    return hr;
  }

  // Move the full configuration file to its permanent location.
  hr = MoveConfigFileFromTemp(full_config_file_path);
  if (FAILED(hr)) {
    return hr;
  }

  // Move the unprivileged configuration file to its permanent location.
  hr = MoveConfigFileFromTemp(unprivileged_config_file_path);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

} // namespace

ElevatedController::ElevatedController() : owner_window_(NULL) {
}

HRESULT ElevatedController::FinalConstruct() {
  return S_OK;
}

void ElevatedController::FinalRelease() {
}

STDMETHODIMP ElevatedController::GetConfig(BSTR* config_out) {
  base::FilePath config_dir = remoting::GetConfigDir();

  // Read the unprivileged part of host configuration.
  scoped_ptr<base::DictionaryValue> config;
  HRESULT hr = ReadConfig(config_dir.Append(kUnprivilegedConfigFileName),
                          &config);
  if (FAILED(hr)) {
    return hr;
  }

  // Convert the config back to a string and return it to the caller.
  std::string file_content;
  base::JSONWriter::Write(config.get(), &file_content);

  *config_out = ::SysAllocString(base::UTF8ToUTF16(file_content).c_str());
  if (config_out == NULL) {
    return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP ElevatedController::GetVersion(BSTR* version_out) {
  // Report the product version number of the daemon controller binary as
  // the host version.
  HMODULE binary = base::GetModuleFromAddress(
      reinterpret_cast<void*>(&ReadConfig));
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForModule(binary));

  base::string16 version;
  if (version_info.get()) {
    version = version_info->product_version();
  }

  *version_out = ::SysAllocString(version.c_str());
  if (version_out == NULL) {
    return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP ElevatedController::SetConfig(BSTR config) {
  // Determine the config directory path and create it if necessary.
  base::FilePath config_dir = remoting::GetConfigDir();
  if (!base::CreateDirectory(config_dir)) {
    return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
  }

  std::string file_content = base::UTF16ToUTF8(
    base::string16(static_cast<base::char16*>(config), ::SysStringLen(config)));

  return WriteConfig(file_content.c_str(), file_content.size(), owner_window_);
}

STDMETHODIMP ElevatedController::SetOwnerWindow(LONG_PTR window_handle) {
  owner_window_ = reinterpret_cast<HWND>(window_handle);
  return S_OK;
}

STDMETHODIMP ElevatedController::StartDaemon() {
  ScopedScHandle service;
  HRESULT hr = OpenService(&service);
  if (FAILED(hr)) {
    return hr;
  }

  // Change the service start type to 'auto'.
  if (!::ChangeServiceConfigW(service.Get(),
                              SERVICE_NO_CHANGE,
                              SERVICE_AUTO_START,
                              SERVICE_NO_CHANGE,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL)) {
    DWORD error = GetLastError();
    PLOG(ERROR) << "Failed to change the '" << kWindowsServiceName
                << "'service start type to 'auto'";
    return HRESULT_FROM_WIN32(error);
  }

  // Start the service.
  if (!StartService(service.Get(), 0, NULL)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_ALREADY_RUNNING) {
      PLOG(ERROR) << "Failed to start the '" << kWindowsServiceName
                  << "'service";

      return HRESULT_FROM_WIN32(error);
    }
  }

  return S_OK;
}

STDMETHODIMP ElevatedController::StopDaemon() {
  ScopedScHandle service;
  HRESULT hr = OpenService(&service);
  if (FAILED(hr)) {
    return hr;
  }

  // Change the service start type to 'manual'.
  if (!::ChangeServiceConfigW(service.Get(),
                              SERVICE_NO_CHANGE,
                              SERVICE_DEMAND_START,
                              SERVICE_NO_CHANGE,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL)) {
    DWORD error = GetLastError();
    PLOG(ERROR) << "Failed to change the '" << kWindowsServiceName
                << "'service start type to 'manual'";
    return HRESULT_FROM_WIN32(error);
  }

  // Stop the service.
  SERVICE_STATUS status;
  if (!ControlService(service.Get(), SERVICE_CONTROL_STOP, &status)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_NOT_ACTIVE) {
      PLOG(ERROR) << "Failed to stop the '" << kWindowsServiceName
                  << "'service";
      return HRESULT_FROM_WIN32(error);
    }
  }

  return S_OK;
}

STDMETHODIMP ElevatedController::UpdateConfig(BSTR config) {
  // Parse the config.
  std::string config_str = base::UTF16ToUTF8(
    base::string16(static_cast<base::char16*>(config), ::SysStringLen(config)));
  scoped_ptr<base::Value> config_value(base::JSONReader::Read(config_str));
  if (!config_value.get()) {
    return E_FAIL;
  }
  base::DictionaryValue* config_dict = NULL;
  if (!config_value->GetAsDictionary(&config_dict)) {
    return E_FAIL;
  }
  // Check for bad keys.
  for (int i = 0; i < arraysize(kReadonlyKeys); ++i) {
    if (config_dict->HasKey(kReadonlyKeys[i])) {
      return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    }
  }
  // Get the old config.
  base::FilePath config_dir = remoting::GetConfigDir();
  scoped_ptr<base::DictionaryValue> config_old;
  HRESULT hr = ReadConfig(config_dir.Append(kConfigFileName), &config_old);
  if (FAILED(hr)) {
    return hr;
  }
  // Merge items from the given config into the old config.
  config_old->MergeDictionary(config_dict);
  // Write the updated config.
  std::string config_updated_str;
  base::JSONWriter::Write(config_old.get(), &config_updated_str);
  return WriteConfig(config_updated_str.c_str(), config_updated_str.size(),
                     owner_window_);
}

STDMETHODIMP ElevatedController::GetUsageStatsConsent(BOOL* allowed,
                                                         BOOL* set_by_policy) {
  bool local_allowed;
  bool local_set_by_policy;
  if (remoting::GetUsageStatsConsent(&local_allowed, &local_set_by_policy)) {
    *allowed = local_allowed;
    *set_by_policy = local_set_by_policy;
    return S_OK;
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP ElevatedController::SetUsageStatsConsent(BOOL allowed) {
  if (remoting::SetUsageStatsConsent(!!allowed)) {
    return S_OK;
  } else {
    return E_FAIL;
  }
}

HRESULT ElevatedController::OpenService(ScopedScHandle* service_out) {
  DWORD error;

  ScopedScHandle scmanager(
      ::OpenSCManagerW(NULL, SERVICES_ACTIVE_DATABASE,
                       SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
  if (!scmanager.IsValid()) {
    error = GetLastError();
    PLOG(ERROR) << "Failed to connect to the service control manager";

    return HRESULT_FROM_WIN32(error);
  }

  DWORD desired_access = SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS |
                         SERVICE_START | SERVICE_STOP;
  ScopedScHandle service(
      ::OpenServiceW(scmanager.Get(), kWindowsServiceName, desired_access));
  if (!service.IsValid()) {
    error = GetLastError();
    PLOG(ERROR) << "Failed to open to the '" << kWindowsServiceName
                << "' service";

    return HRESULT_FROM_WIN32(error);
  }

  service_out->Set(service.Take());
  return S_OK;
}

} // namespace remoting

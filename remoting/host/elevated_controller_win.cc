// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/elevated_controller_win.h"

#include <sddl.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stringize_macros.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/branding.h"
#include "remoting/host/elevated_controller_resource.h"
#include "remoting/host/verify_config_window_win.h"

namespace {

// The maximum size of the configuration file. "1MB ought to be enough" for any
// reasonable configuration we will ever need. 1MB is low enough to make
// the probability of out of memory situation fairly low. OOM is still possible
// and we will crash if it occurs.
const size_t kMaxConfigFileSize = 1024 * 1024;

// The host configuration file name.
const FilePath::CharType kConfigFileName[] = FILE_PATH_LITERAL("host.json");

// The unprivileged configuration file name.
const FilePath::CharType kUnprivilegedConfigFileName[] =
    FILE_PATH_LITERAL("host_unprivileged.json");

// The extension for the temporary file.
const FilePath::CharType kTempFileExtension[] = FILE_PATH_LITERAL("json~");

// The host configuration file security descriptor that enables full access to
// Local System and built-in administrators only.
const char16 kConfigFileSecurityDescriptor[] =
    TO_L_STRING("O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)");

const char16 kUnprivilegedConfigFileSecurityDescriptor[] =
    TO_L_STRING("O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AU)");

// Configuration keys.
const char kHostId[] = "host_id";
const char kXmppLogin[] = "xmpp_login";
const char kHostSecretHash[] = "host_secret_hash";

// The configuration keys that cannot be specified in UpdateConfig().
const char* const kReadonlyKeys[] = { kHostId, kXmppLogin };

// The configuration keys whose values may be read by GetConfig().
const char* const kUnprivilegedConfigKeys[] = { kHostId, kXmppLogin };

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
HRESULT ReadConfig(const FilePath& filename,
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
    LOG_GETLASTERROR(ERROR)
        << "Failed to open '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  scoped_array<char> buffer(new char[kMaxConfigFileSize]);
  DWORD size = kMaxConfigFileSize;
  if (!::ReadFile(file, &buffer[0], size, &size, NULL)) {
    DWORD error = GetLastError();
    LOG_GETLASTERROR(ERROR)
        << "Failed to read '" << filename.value() << "'";
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

FilePath GetTempLocationFor(const FilePath& filename) {
  return filename.ReplaceExtension(kTempFileExtension);
}

// Writes a config file to a temporary location.
HRESULT WriteConfigFileToTemp(const FilePath& filename,
                              const char16* security_descriptor,
                              const char* content,
                              size_t length) {
  // Create a security descriptor for the configuration file.
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = FALSE;

  ULONG security_descriptor_length = 0;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
           security_descriptor,
           SDDL_REVISION_1,
           reinterpret_cast<PSECURITY_DESCRIPTOR*>(
               &security_attributes.lpSecurityDescriptor),
           &security_descriptor_length)) {
    DWORD error = GetLastError();
    LOG_GETLASTERROR(ERROR) <<
        "Failed to create a security descriptor for the configuration file";
    return HRESULT_FROM_WIN32(error);
  }

  // Create a temporary file and write configuration to it.
  FilePath tempname = GetTempLocationFor(filename);
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
    LOG_GETLASTERROR(ERROR)
        << "Failed to create '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  DWORD written;
  if (!WriteFile(file, content, static_cast<DWORD>(length), &written, NULL)) {
    DWORD error = GetLastError();
    LOG_GETLASTERROR(ERROR)
        << "Failed to write to '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

// Moves a config file from its temporary location to its permanent location.
HRESULT MoveConfigFileFromTemp(const FilePath& filename) {
  // Now that the configuration is stored successfully replace the actual
  // configuration file.
  FilePath tempname = GetTempLocationFor(filename);
  if (!MoveFileExW(tempname.value().c_str(),
                   filename.value().c_str(),
                   MOVEFILE_REPLACE_EXISTING)) {
      DWORD error = GetLastError();
      LOG_GETLASTERROR(ERROR)
          << "Failed to rename '" << tempname.value() << "' to '"
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
  std::string email, host_id, host_secret_hash;
  if (!config_dict->GetString(kXmppLogin, &email) ||
      !config_dict->GetString(kHostId, &host_id) ||
      !config_dict->GetString(kHostSecretHash, &host_secret_hash)) {
    return E_FAIL;
  }

  // Ask the user to verify the configuration (unless the client is admin
  // already).
  if (!IsClientAdmin()) {
    remoting::VerifyConfigWindowWin verify_win(email, host_id,
                                               host_secret_hash);
    if (verify_win.DoModal(owner_window) != IDOK) {
      return E_FAIL;
    }
  }

  // Extract the unprivileged fields from the configuration.
  base::DictionaryValue unprivileged_config_dict;
  for (int i = 0; i < arraysize(kUnprivilegedConfigKeys); ++i) {
    const char* key = kUnprivilegedConfigKeys[i];
    string16 value;
    if (config_dict->GetString(key, &value)) {
      unprivileged_config_dict.SetString(key, value);
    }
  }
  std::string unprivileged_config_str;
  base::JSONWriter::Write(&unprivileged_config_dict, &unprivileged_config_str);

  // Write the full configuration file to a temporary location.
  FilePath full_config_file_path =
      remoting::GetConfigDir().Append(kConfigFileName);
  HRESULT hr = WriteConfigFileToTemp(full_config_file_path,
                                     kConfigFileSecurityDescriptor,
                                     content,
                                     length);
  if (FAILED(hr)) {
    return hr;
  }

  // Write the unprivileged configuration file to a temporary location.
  FilePath unprivileged_config_file_path =
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

namespace remoting {

ElevatedControllerWin::ElevatedControllerWin() : owner_window_(NULL) {
}

HRESULT ElevatedControllerWin::FinalConstruct() {
  return S_OK;
}

void ElevatedControllerWin::FinalRelease() {
}

STDMETHODIMP ElevatedControllerWin::GetConfig(BSTR* config_out) {
  FilePath config_dir = remoting::GetConfigDir();

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

  *config_out = ::SysAllocString(UTF8ToUTF16(file_content).c_str());
  if (config_out == NULL) {
    return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP ElevatedControllerWin::SetConfig(BSTR config) {
  // Determine the config directory path and create it if necessary.
  FilePath config_dir = remoting::GetConfigDir();
  if (!file_util::CreateDirectory(config_dir)) {
    return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
  }

  std::string file_content = UTF16ToUTF8(
    string16(static_cast<char16*>(config), ::SysStringLen(config)));

  return WriteConfig(file_content.c_str(), file_content.size(), owner_window_);
}

STDMETHODIMP ElevatedControllerWin::StartDaemon() {
  ScopedScHandle service;
  HRESULT hr = OpenService(&service);
  if (FAILED(hr)) {
    return hr;
  }

  // Change the service start type to 'auto'.
  if (!::ChangeServiceConfigW(service,
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
    LOG_GETLASTERROR(ERROR)
        << "Failed to change the '" << kWindowsServiceName
        << "'service start type to 'auto'";
    return HRESULT_FROM_WIN32(error);
  }

  // Start the service.
  if (!StartService(service, 0, NULL)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_ALREADY_RUNNING) {
      LOG_GETLASTERROR(ERROR)
          << "Failed to start the '" << kWindowsServiceName << "'service";

      return HRESULT_FROM_WIN32(error);
    }
  }

  return S_OK;
}

STDMETHODIMP ElevatedControllerWin::StopDaemon() {
  ScopedScHandle service;
  HRESULT hr = OpenService(&service);
  if (FAILED(hr)) {
    return hr;
  }

  // Change the service start type to 'manual'.
  if (!::ChangeServiceConfigW(service,
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
    LOG_GETLASTERROR(ERROR)
        << "Failed to change the '" << kWindowsServiceName
        << "'service start type to 'manual'";
    return HRESULT_FROM_WIN32(error);
  }

  // Stop the service.
  SERVICE_STATUS status;
  if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_NOT_ACTIVE) {
      LOG_GETLASTERROR(ERROR)
          << "Failed to stop the '" << kWindowsServiceName << "'service";
      return HRESULT_FROM_WIN32(error);
    }
  }

  return S_OK;
}

STDMETHODIMP ElevatedControllerWin::UpdateConfig(BSTR config) {
  // Parse the config.
  std::string config_str = UTF16ToUTF8(
    string16(static_cast<char16*>(config), ::SysStringLen(config)));
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
  FilePath config_dir = remoting::GetConfigDir();
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

STDMETHODIMP ElevatedControllerWin::SetOwnerWindow(LONG_PTR window_handle) {
  owner_window_ = reinterpret_cast<HWND>(window_handle);
  return S_OK;
}

HRESULT ElevatedControllerWin::OpenService(ScopedScHandle* service_out) {
  DWORD error;

  ScopedScHandle scmanager(
      ::OpenSCManagerW(NULL, SERVICES_ACTIVE_DATABASE,
                       SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
  if (!scmanager.IsValid()) {
    error = GetLastError();
    LOG_GETLASTERROR(ERROR)
        << "Failed to connect to the service control manager";

    return HRESULT_FROM_WIN32(error);
  }

  DWORD desired_access = SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS |
                         SERVICE_START | SERVICE_STOP;
  ScopedScHandle service(
      ::OpenServiceW(scmanager, kWindowsServiceName, desired_access));
  if (!service.IsValid()) {
    error = GetLastError();
    LOG_GETLASTERROR(ERROR)
        << "Failed to open to the '" << kWindowsServiceName << "' service";

    return HRESULT_FROM_WIN32(error);
  }

  service_out->Set(service.Take());
  return S_OK;
}

} // namespace remoting

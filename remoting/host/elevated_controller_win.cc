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
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/branding.h"

namespace {

// The host configuration file name.
const FilePath::CharType kConfigFileName[] = FILE_PATH_LITERAL("host.json");

// The extension for the temporary file.
const FilePath::CharType kTempFileExtension[] = FILE_PATH_LITERAL("json~");

// The host configuration file security descriptor that enables full access to
// Local System and built-in administrators only.
const char kConfigFileSecurityDescriptor[] =
    "O:BA" "G:BA" "D:(A;;GA;;;SY)(A;;GA;;;BA)";

// The maximum size of the configuration file. "1MB ought to be enough" for any
// reasonable configuration we will ever need. 1MB is low enough to make
// the probability of out of memory situation fairly low. OOM is still possible
// and we will crash if it occurs.
const size_t kMaxConfigFileSize = 1024 * 1024;

// ReadConfig() filters the configuration file stripping all variables except of
// the following two.
const char kHostId[] = "host_id";
const char kXmppLogin[] = "xmpp_login";

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

// Writes the configuration file up to |kMaxConfigFileSize| in size.
HRESULT WriteConfig(const FilePath& filename,
                    const char* content,
                    size_t length) {
  if (length > kMaxConfigFileSize) {
      return E_FAIL;
  }

  // Create a security descriptor for the configuration file.
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = FALSE;

  ULONG security_descriptor_length = 0;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
           kConfigFileSecurityDescriptor,
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
  FilePath tempname = filename.ReplaceExtension(kTempFileExtension);
  {
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
  }

  // Now that the configuration is stored successfully replace the actual
  // configuration file.
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


} // namespace

namespace remoting {

ElevatedControllerWin::ElevatedControllerWin() {
}

HRESULT ElevatedControllerWin::FinalConstruct() {
  return S_OK;
}

void ElevatedControllerWin::FinalRelease() {
}

STDMETHODIMP ElevatedControllerWin::GetConfig(BSTR* config_out) {
  FilePath config_dir = remoting::GetConfigDir();

  // Read the host configuration.
  scoped_ptr<base::DictionaryValue> config;
  HRESULT hr = ReadConfig(config_dir.Append(kConfigFileName), &config);
  if (FAILED(hr)) {
    return hr;
  }

  // Build the filtered config.
  scoped_ptr<base::DictionaryValue> filtered_config(
      new base::DictionaryValue());

  std::string value;
  if (config->GetString(kHostId, &value)) {
    filtered_config->SetString(kHostId, value);
  }
  if (config->GetString(kXmppLogin, &value)) {
    filtered_config->SetString(kXmppLogin, value);
  }

  // Convert the filtered config back to a string and return it to the caller.
  std::string file_content;
  base::JSONWriter::Write(filtered_config.get(), &file_content);

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

  return WriteConfig(config_dir.Append(kConfigFileName),
                     file_content.c_str(),
                     file_content.size());
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
  return E_NOTIMPL;
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
      ::OpenServiceW(scmanager, UTF8ToUTF16(kWindowsServiceName).c_str(),
                     desired_access));
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

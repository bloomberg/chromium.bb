// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/elevated_controller_win.h"

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

// MIDL-generated definitions.
#include "elevated_controller_i.c"

namespace {

// ReadConfig() filters the configuration file stripping all variables except of
// the following two.
const char kHostId[] = "host_id";
const char kXmppLogin[] = "xmpp_login";

// Names of the configuration files.
const FilePath::CharType kAuthConfigFilename[] = FILE_PATH_LITERAL("auth.json");
const FilePath::CharType kHostConfigFilename[] = FILE_PATH_LITERAL("host.json");

// TODO(alexeypa): Remove the hardcoded undocumented paths and store
// the configuration in the registry.
#ifdef OFFICIAL_BUILD
const FilePath::CharType kConfigDir[] = FILE_PATH_LITERAL(
    "config\\systemprofile\\AppData\\Local\\Google\\Chrome Remote Desktop");
#else
const FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL("config\\systemprofile\\AppData\\Local\\Chromoting");
#endif

// Reads and parses a JSON configuration file.
HRESULT ReadConfig(const FilePath& filename,
                   scoped_ptr<base::DictionaryValue>* config_out) {
  // TODO(alexeypa): Remove 64KB limitation.
  const size_t kMaxConfigFileSize = 64 * 1024;

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
        << "Failed to read '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  std::vector<char> buffer(kMaxConfigFileSize);
  DWORD size = static_cast<DWORD>(buffer.size());
  if (!::ReadFile(file, &buffer[0], size, &size, NULL)) {
    DWORD error = GetLastError();
    LOG_GETLASTERROR(ERROR)
        << "Failed to read '" << filename.value() << "'";
    return HRESULT_FROM_WIN32(error);
  }

  // Parse the JSON configuration, expecting it to contain a dictionary.
  std::string file_content(&buffer[0], size);
  scoped_ptr<base::Value> value(base::JSONReader::Read(file_content, true));

  base::DictionaryValue* dictionary;
  if (value.get() == NULL || !value->GetAsDictionary(&dictionary)) {
    LOG(ERROR) << "Failed to read '" << filename.value() << "'.";
    return E_FAIL;
  }

  value.release();
  config_out->reset(dictionary);
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
  FilePath system_profile;
  PathService::Get(base::DIR_SYSTEM, &system_profile);

  // Read the host configuration.
  scoped_ptr<base::DictionaryValue> config;
  HRESULT hr = ReadConfig(
      system_profile.Append(kConfigDir).Append(kHostConfigFilename),
      &config);
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
  // N.B. The configuration files are stored in LocalSystems's profile which is
  // not readable by non administrators.
  FilePath system_profile;
  PathService::Get(base::DIR_SYSTEM, &system_profile);
  FilePath config_dir = system_profile.Append(kConfigDir);
  if (!file_util::CreateDirectory(config_dir)) {
    return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
  }

  std::string file_content = UTF16ToUTF8(
    string16(static_cast<char16*>(config), ::SysStringLen(config)));

  int written = file_util::WriteFile(
      config_dir.Append(kAuthConfigFilename),
      file_content.c_str(),
      file_content.size());
  if (written != static_cast<int>(file_content.size())) {
    return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
  }

  // TODO(alexeypa): Store the authentication and host configurations in a
  // single file.
  written = file_util::WriteFile(
      config_dir.Append(kHostConfigFilename),
      file_content.c_str(),
      file_content.size());
  if (written != static_cast<int>(file_content.size())) {
    return E_FAIL;
  }

  return S_OK;
}

STDMETHODIMP ElevatedControllerWin::StartDaemon() {
  ScopedScHandle service;
  HRESULT hr = OpenService(&service);
  if (FAILED(hr)) {
    return hr;
  }

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

  ScopedScHandle service(
      ::OpenServiceW(scmanager, UTF8ToUTF16(kWindowsServiceName).c_str(),
                     SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP));
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

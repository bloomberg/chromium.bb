// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_WIN_UTIL_H_
#define CHROME_UPDATER_WIN_WIN_UTIL_H_

#include <winerror.h>

#include <stdint.h>

#include <string>

#include "base/process/process_iterator.h"
#include "base/win/atl.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class FilePath;
}

namespace updater {

class ProcessFilterName : public base::ProcessFilter {
 public:
  explicit ProcessFilterName(const std::wstring& process_name);
  ~ProcessFilterName() override = default;

  // Overrides for base::ProcessFilter.
  bool Includes(const base::ProcessEntry& entry) const override;

 private:
  // Case-insensive name of the program image to look for, not including the
  // path. The name is not localized, therefore the function must be used
  // to look up only processes whose names are known to be ASCII.
  std::wstring process_name_;
};

// Returns the last error as an HRESULT or E_FAIL if last error is NO_ERROR.
// This is not a drop in replacement for the HRESULT_FROM_WIN32 macro.
// The macro maps a NO_ERROR to S_OK, whereas the HRESULTFromLastError maps a
// NO_ERROR to E_FAIL.
HRESULT HRESULTFromLastError();

// Returns an HRESULT with a custom facility code representing an updater error.
// The updater error should be a small positive or a small negative 16-bit
// integral value.
template <typename Error>
HRESULT HRESULTFromUpdaterError(Error error) {
  constexpr ULONG kSeverityError = 0x80000000;
  constexpr ULONG kCustomerBit = 0x20000000;
  constexpr ULONG kFacilityOmaha = 67;
  return static_cast<HRESULT>(kSeverityError | kCustomerBit |
                              (kFacilityOmaha << 16) |
                              static_cast<ULONG>(error));
}

// Checks whether a process is running with the image |executable|. Returns true
// if a process is found.
bool IsProcessRunning(const wchar_t* executable);

// Waits until every running instance of |executable| is stopped.
// Returns true if every running processes are stopped.
bool WaitForProcessesStopped(const wchar_t* executable);

bool InitializeCOMSecurity();

// Gets the handle to the module containing the given executing address.
HMODULE GetModuleHandleFromAddress(void* address);

// Gets the handle to the currently executing module.
HMODULE GetCurrentModuleHandle();

// Creates a unique event name and stores it in the specified environment var.
HRESULT CreateUniqueEventInEnvironment(const std::wstring& var_name,
                                       UpdaterScope scope,
                                       HANDLE* unique_event);

// Obtains a unique event name from specified environment var and opens it.
HRESULT OpenUniqueEventFromEnvironment(const std::wstring& var_name,
                                       UpdaterScope scope,
                                       HANDLE* unique_event);

struct NamedObjectAttributes {
  NamedObjectAttributes();
  ~NamedObjectAttributes();
  std::wstring name;
  CSecurityAttributes sa;
};

// For machine and local system, the prefix would be "Global\G{obj_name}".
// For user, the prefix would be "Global\G{user_sid}{obj_name}".
// For machine objects, returns a security attributes that gives permissions to
// both Admins and SYSTEM. This allows for cases where SYSTEM creates the named
// object first. The default DACL for SYSTEM will not allow Admins access.
void GetNamedObjectAttributes(const wchar_t* base_name,
                              UpdaterScope scope,
                              NamedObjectAttributes* attr);

// Creates an event based on the provided attributes.
HRESULT CreateEvent(NamedObjectAttributes* event_attr, HANDLE* event_handle);

// Gets the security descriptor with the default DACL for the current process
// user. The owner is the current user, the group is the current primary group.
// Returns true and populates sec_attr on success, false on failure.
bool GetCurrentUserDefaultSecurityAttributes(CSecurityAttributes* sec_attr);

// Get security attributes containing a DACL that grant the ACCESS_MASK access
// to admins and system.
void GetAdminDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                    ACCESS_MASK accessmask);

// Get security descriptor containing a DACL that grants the ACCESS_MASK access
// to admins and system.
void GetAdminDaclSecurityDescriptor(CSecurityDesc* sd, ACCESS_MASK accessmask);

// Returns the registry path for the Updater app id under the |Clients| subkey.
// The path does not include the registry root hive prefix.
std::wstring GetRegistryKeyClientsUpdater();

// Returns the registry path for the Updater app id under the |ClientState|
// subkey. The path does not include the registry root hive prefix.
std::wstring GetRegistryKeyClientStateUpdater();

// Returns a value in the [0, 100] range or -1 if the progress could not
// be computed.
int GetDownloadProgress(int64_t downloaded_bytes, int64_t total_bytes);

// Returns a logged on user token handle from the current session.
base::win::ScopedHandle GetUserTokenFromCurrentSessionId();

// Sets `is_user_admin` to true if the user is running as an elevated
// administrator.
HRESULT IsUserAdmin(bool& is_user_admin);

// Sets `is_user_non_elevated_admin` to true if the user is running as a
// non-elevated administrator.
HRESULT IsUserNonElevatedAdmin(bool& is_user_non_elevated_admin);

// Sets `is_uac_on` to true if the UAC is enabled.
HRESULT IsUACOn(bool& is_uac_on);

// Sets `is_elevated_with_uac_on` to true if running at high integrity with
// UAC on.
HRESULT IsElevatedWithUACOn(bool& is_elevated_with_uac_on);

// Returns a string representing the UAC settings and elevation state for the
// caller. The value can be used for logging purposes.
std::string GetUACState();

// Returns the versioned service name in the following format:
// "{ProductName}{InternalService/Service}{UpdaterVersion}".
// For instance: "ChromiumUpdaterInternalService92.0.0.1".
std::wstring GetServiceName(bool is_internal_service);

// Returns the versioned service name in the following format:
// "{ProductName} {InternalService/Service} {UpdaterVersion}".
// For instance: "ChromiumUpdater InternalService 92.0.0.1".
std::wstring GetServiceDisplayName(bool is_internal_service);

// Returns the versioned task name in the following format:
// "{ProductName}Task{System/User}{UpdaterVersion}".
// For instance: "ChromiumUpdaterTaskSystem92.0.0.1".
std::wstring GetTaskName(UpdaterScope scope);

// Returns the versioned task display name in the following format:
// "{ProductName} Task {System/User} {UpdaterVersion}".
// For instance: "ChromiumUpdater Task System 92.0.0.1".
std::wstring GetTaskDisplayName(UpdaterScope scope);

// Returns `KEY_WOW64_32KEY | access`. All registry access under the Updater key
// should use `Wow6432(access)` as the `REGSAM`.
REGSAM Wow6432(REGSAM access);

// Starts a new elevated process. `file_path` specifies the program to be run.
// `parameters` can be an empty string.
// The function waits until the spawned process has completed. The exit code of
// the process is returned in `exit_code`.
HRESULT RunElevated(const base::FilePath& file_path,
                    const std::wstring& parameters,
                    DWORD* exit_code);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_WIN_UTIL_H_

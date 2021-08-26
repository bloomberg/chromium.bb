// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/win_util.h"

#include <aclapi.h>
#include <shlobj.h>
#include <windows.h>
#include <wtsapi32.h>

#include <cstdlib>
#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/process/process_iterator.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/win/user_info.h"
#include "chrome/updater/win/win_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

// The number of iterations to poll if a process is stopped correctly.
const unsigned int kMaxProcessQueryIterations = 50;

// The sleep time in ms between each poll.
const unsigned int kProcessQueryWaitTimeMs = 100;

class ProcessFilterExplorer : public base::ProcessFilter {
 public:
  ~ProcessFilterExplorer() override = default;

  // Overrides for base::ProcessFilter.
  bool Includes(const base::ProcessEntry& entry) const override {
    return base::EqualsCaseInsensitiveASCII(entry.exe_file(), L"EXPLORER.EXE");
  }
};

HRESULT IsUserRunningSplitToken(bool& is_split_token) {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return HRESULTFromLastError();
  base::win::ScopedHandle token_holder(token);
  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(token_holder.Get(), TokenElevationType,
                             &elevation_type, sizeof(elevation_type),
                             &size_returned)) {
    return HRESULTFromLastError();
  }
  is_split_token = elevation_type == TokenElevationTypeFull ||
                   elevation_type == TokenElevationTypeLimited;
  DCHECK(is_split_token || elevation_type == TokenElevationTypeDefault);
  return S_OK;
}

HRESULT GetSidIntegrityLevel(PSID sid, MANDATORY_LEVEL* level) {
  if (!::IsValidSid(sid))
    return E_FAIL;
  SID_IDENTIFIER_AUTHORITY* authority = ::GetSidIdentifierAuthority(sid);
  if (!authority)
    return E_FAIL;
  static const SID_IDENTIFIER_AUTHORITY kMandatoryLabelAuth =
      SECURITY_MANDATORY_LABEL_AUTHORITY;
  if (std::memcmp(authority, &kMandatoryLabelAuth,
                  sizeof(SID_IDENTIFIER_AUTHORITY))) {
    return E_FAIL;
  }
  PUCHAR count = ::GetSidSubAuthorityCount(sid);
  if (!count || *count != 1)
    return E_FAIL;
  DWORD* rid = ::GetSidSubAuthority(sid, 0);
  if (!rid)
    return E_FAIL;
  if ((*rid & 0xFFF) != 0 || *rid > SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
    return E_FAIL;
  *level = static_cast<MANDATORY_LEVEL>(*rid >> 12);
  return S_OK;
}

// Gets the mandatory integrity level of a process.
// TODO(crbug.com/1233748): consider reusing
// base::GetCurrentProcessIntegrityLevel().
HRESULT GetProcessIntegrityLevel(DWORD process_id, MANDATORY_LEVEL* level) {
  HANDLE process = ::OpenProcess(PROCESS_QUERY_INFORMATION, false, process_id);
  if (!process)
    return HRESULTFromLastError();
  base::win::ScopedHandle process_holder(process);
  HANDLE token = NULL;
  if (!::OpenProcessToken(process_holder.Get(),
                          TOKEN_QUERY | TOKEN_QUERY_SOURCE, &token)) {
    return HRESULTFromLastError();
  }
  base::win::ScopedHandle token_holder(token);
  DWORD label_size = 0;
  if (::GetTokenInformation(token_holder.Get(), TokenIntegrityLevel, nullptr, 0,
                            &label_size) ||
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return E_FAIL;
  }
  std::unique_ptr<TOKEN_MANDATORY_LABEL, base::FreeDeleter> label(
      static_cast<TOKEN_MANDATORY_LABEL*>(std::malloc(label_size)));
  if (!::GetTokenInformation(token_holder.Get(), TokenIntegrityLevel,
                             label.get(), label_size, &label_size)) {
    return HRESULTFromLastError();
  }
  return GetSidIntegrityLevel(label->Label.Sid, level);
}

bool IsExplorerRunningAtMediumOrLower() {
  ProcessFilterExplorer filter;
  base::ProcessIterator iter(&filter);
  while (const base::ProcessEntry* process_entry = iter.NextProcessEntry()) {
    MANDATORY_LEVEL level = MandatoryLevelUntrusted;
    if (SUCCEEDED(GetProcessIntegrityLevel(process_entry->pid(), &level)) &&
        level <= MandatoryLevelMedium) {
      return true;
    }
  }
  return false;
}

}  // namespace

NamedObjectAttributes::NamedObjectAttributes() = default;
NamedObjectAttributes::~NamedObjectAttributes() = default;

HRESULT HRESULTFromLastError() {
  const auto error_code = ::GetLastError();
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

bool IsProcessRunning(const wchar_t* executable) {
  base::NamedProcessIterator iter(executable, nullptr);
  const base::ProcessEntry* entry = iter.NextProcessEntry();
  return entry != nullptr;
}

bool WaitForProcessesStopped(const wchar_t* executable) {
  DCHECK(executable);
  VLOG(1) << "Wait for processes '" << executable << "'.";

  // Wait until the process is completely stopped.
  for (unsigned int iteration = 0; iteration < kMaxProcessQueryIterations;
       ++iteration) {
    if (!IsProcessRunning(executable))
      return true;
    ::Sleep(kProcessQueryWaitTimeMs);
  }

  // The process didn't terminate.
  LOG(ERROR) << "Cannot stop process '" << executable << "', timeout.";
  return false;
}

// This sets up COM security to allow NetworkService, LocalService, and System
// to call back into the process. It is largely inspired by
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa378987.aspx
// static
bool InitializeCOMSecurity() {
  // Create the security descriptor explicitly as follows because
  // CoInitializeSecurity() will not accept the relative security descriptors
  // returned by ConvertStringSecurityDescriptorToSecurityDescriptor().
  const size_t kSidCount = 5;
  uint64_t* sids[kSidCount][(SECURITY_MAX_SID_SIZE + sizeof(uint64_t) - 1) /
                            sizeof(uint64_t)] = {
      {}, {}, {}, {}, {},
  };

  // These are ordered by most interesting ones to try first.
  WELL_KNOWN_SID_TYPE sid_types[kSidCount] = {
      WinBuiltinAdministratorsSid,  // administrator group security identifier
      WinLocalServiceSid,           // local service security identifier
      WinNetworkServiceSid,         // network service security identifier
      WinSelfSid,                   // personal account security identifier
      WinLocalSystemSid,            // local system security identifier
  };

  // This creates a security descriptor that is equivalent to the following
  // security descriptor definition language (SDDL) string:
  //   O:BAG:BAD:(A;;0x1;;;LS)(A;;0x1;;;NS)(A;;0x1;;;PS)
  //   (A;;0x1;;;SY)(A;;0x1;;;BA)

  // Initialize the security descriptor.
  SECURITY_DESCRIPTOR security_desc = {};
  if (!::InitializeSecurityDescriptor(&security_desc,
                                      SECURITY_DESCRIPTOR_REVISION))
    return false;

  DCHECK_EQ(kSidCount, base::size(sids));
  DCHECK_EQ(kSidCount, base::size(sid_types));
  for (size_t i = 0; i < kSidCount; ++i) {
    DWORD sid_bytes = sizeof(sids[i]);
    if (!::CreateWellKnownSid(sid_types[i], nullptr, sids[i], &sid_bytes))
      return false;
  }

  // Setup the access control entries (ACE) for COM. You may need to modify
  // the access permissions for your application. COM_RIGHTS_EXECUTE and
  // COM_RIGHTS_EXECUTE_LOCAL are the minimum access rights required.
  EXPLICIT_ACCESS explicit_access[kSidCount] = {};
  DCHECK_EQ(kSidCount, base::size(sids));
  DCHECK_EQ(kSidCount, base::size(explicit_access));
  for (size_t i = 0; i < kSidCount; ++i) {
    explicit_access[i].grfAccessPermissions =
        COM_RIGHTS_EXECUTE | COM_RIGHTS_EXECUTE_LOCAL;
    explicit_access[i].grfAccessMode = SET_ACCESS;
    explicit_access[i].grfInheritance = NO_INHERITANCE;
    explicit_access[i].Trustee.pMultipleTrustee = nullptr;
    explicit_access[i].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    explicit_access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    explicit_access[i].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    explicit_access[i].Trustee.ptstrName = reinterpret_cast<LPTSTR>(sids[i]);
  }

  // Create an access control list (ACL) using this ACE list, if this succeeds
  // make sure to ::LocalFree(acl).
  ACL* acl = nullptr;
  DWORD acl_result = ::SetEntriesInAcl(base::size(explicit_access),
                                       explicit_access, nullptr, &acl);
  if (acl_result != ERROR_SUCCESS || acl == nullptr)
    return false;

  HRESULT hr = E_FAIL;

  // Set the security descriptor owner and group to Administrators and set the
  // discretionary access control list (DACL) to the ACL.
  if (::SetSecurityDescriptorOwner(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorGroup(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorDacl(&security_desc, TRUE, acl, FALSE)) {
    // Initialize COM. You may need to modify the parameters of
    // CoInitializeSecurity() for your application. Note that an
    // explicit security descriptor is being passed down.
    hr = ::CoInitializeSecurity(
        &security_desc, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IDENTIFY, nullptr,
        EOAC_DISABLE_AAA | EOAC_NO_CUSTOM_MARSHAL, nullptr);
  }

  ::LocalFree(acl);
  return SUCCEEDED(hr);
}

HMODULE GetModuleHandleFromAddress(void* address) {
  MEMORY_BASIC_INFORMATION mbi = {0};
  size_t result = ::VirtualQuery(address, &mbi, sizeof(mbi));
  DCHECK_EQ(result, sizeof(mbi));
  return static_cast<HMODULE>(mbi.AllocationBase);
}

HMODULE GetCurrentModuleHandle() {
  return GetModuleHandleFromAddress(
      reinterpret_cast<void*>(&GetCurrentModuleHandle));
}

// The event name saved to the environment variable does not contain the
// decoration added by GetNamedObjectAttributes.
HRESULT CreateUniqueEventInEnvironment(const std::wstring& var_name,
                                       UpdaterScope scope,
                                       HANDLE* unique_event) {
  DCHECK(unique_event);

  const std::wstring event_name = base::ASCIIToWide(base::GenerateGUID());
  NamedObjectAttributes attr;
  GetNamedObjectAttributes(event_name.c_str(), scope, &attr);

  HRESULT hr = CreateEvent(&attr, unique_event);
  if (FAILED(hr))
    return hr;

  if (!::SetEnvironmentVariable(var_name.c_str(), event_name.c_str()))
    return HRESULTFromLastError();

  return S_OK;
}

HRESULT OpenUniqueEventFromEnvironment(const std::wstring& var_name,
                                       UpdaterScope scope,
                                       HANDLE* unique_event) {
  DCHECK(unique_event);

  wchar_t event_name[MAX_PATH] = {0};
  if (!::GetEnvironmentVariable(var_name.c_str(), event_name,
                                base::size(event_name))) {
    return HRESULTFromLastError();
  }

  NamedObjectAttributes attr;
  GetNamedObjectAttributes(event_name, scope, &attr);
  *unique_event = ::OpenEvent(EVENT_ALL_ACCESS, false, attr.name.c_str());

  if (!*unique_event)
    return HRESULTFromLastError();

  return S_OK;
}

HRESULT CreateEvent(NamedObjectAttributes* event_attr, HANDLE* event_handle) {
  DCHECK(event_handle);
  DCHECK(event_attr);
  DCHECK(!event_attr->name.empty());
  *event_handle = ::CreateEvent(&event_attr->sa,
                                true,   // manual reset
                                false,  // not signaled
                                event_attr->name.c_str());

  if (!*event_handle)
    return HRESULTFromLastError();

  return S_OK;
}

void GetNamedObjectAttributes(const wchar_t* base_name,
                              UpdaterScope scope,
                              NamedObjectAttributes* attr) {
  DCHECK(base_name);
  DCHECK(attr);

  attr->name = kGlobalPrefix;

  switch (scope) {
    case UpdaterScope::kUser: {
      std::wstring user_sid;
      GetProcessUser(nullptr, nullptr, &user_sid);
      attr->name += user_sid;
      GetCurrentUserDefaultSecurityAttributes(&attr->sa);
      break;
    }
    case UpdaterScope::kSystem:
      // Grant access to administrators and system.
      GetAdminDaclSecurityAttributes(&attr->sa, GENERIC_ALL);
      break;
  }

  attr->name += base_name;
}

bool GetCurrentUserDefaultSecurityAttributes(CSecurityAttributes* sec_attr) {
  DCHECK(sec_attr);

  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY))
    return false;

  CSecurityDesc security_desc;
  CSid sid_owner;
  if (!token.GetOwner(&sid_owner))
    return false;

  security_desc.SetOwner(sid_owner);
  CSid sid_group;
  if (!token.GetPrimaryGroup(&sid_group))
    return false;

  security_desc.SetGroup(sid_group);

  CDacl dacl;
  if (!token.GetDefaultDacl(&dacl))
    return false;

  CSid sid_user;
  if (!token.GetUser(&sid_user))
    return false;
  if (!dacl.AddAllowedAce(sid_user, GENERIC_ALL))
    return false;

  security_desc.SetDacl(dacl);
  sec_attr->Set(security_desc);

  return true;
}

void GetAdminDaclSecurityDescriptor(CSecurityDesc* sd, ACCESS_MASK accessmask) {
  DCHECK(sd);

  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), accessmask);
  dacl.AddAllowedAce(Sids::Admins(), accessmask);

  sd->SetOwner(Sids::Admins());
  sd->SetGroup(Sids::Admins());
  sd->SetDacl(dacl);
  sd->MakeAbsolute();
}

void GetAdminDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                    ACCESS_MASK accessmask) {
  DCHECK(sec_attr);
  CSecurityDesc sd;
  GetAdminDaclSecurityDescriptor(&sd, accessmask);
  sec_attr->Set(sd);
}

std::wstring GetRegistryKeyClientsUpdater() {
  return base::ASCIIToWide(base::StrCat({CLIENTS_KEY, kUpdaterAppId}));
}

std::wstring GetRegistryKeyClientStateUpdater() {
  return base::ASCIIToWide(base::StrCat({CLIENT_STATE_KEY, kUpdaterAppId}));
}

int GetDownloadProgress(int64_t downloaded_bytes, int64_t total_bytes) {
  if (downloaded_bytes == -1 || total_bytes == -1 || total_bytes == 0)
    return -1;
  DCHECK_LE(downloaded_bytes, total_bytes);
  return 100 * base::clamp(static_cast<double>(downloaded_bytes) / total_bytes,
                           0.0, 1.0);
}

base::win::ScopedHandle GetUserTokenFromCurrentSessionId() {
  base::win::ScopedHandle token_handle;

  DWORD bytes_returned = 0;
  DWORD* session_id_ptr = nullptr;
  if (!::WTSQuerySessionInformation(
          WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionId,
          reinterpret_cast<LPTSTR*>(&session_id_ptr), &bytes_returned)) {
    PLOG(ERROR) << "WTSQuerySessionInformation failed.";
    return token_handle;
  }

  DCHECK_EQ(bytes_returned, sizeof(*session_id_ptr));
  DWORD session_id = *session_id_ptr;
  ::WTSFreeMemory(session_id_ptr);
  DVLOG(1) << "::WTSQuerySessionInformation session id: " << session_id;

  HANDLE token_handle_raw = nullptr;
  if (!::WTSQueryUserToken(session_id, &token_handle_raw)) {
    PLOG(ERROR) << "WTSQueryUserToken failed";
    return token_handle;
  }

  token_handle.Set(token_handle_raw);
  return token_handle;
}

bool PathOwnedByUser(const base::FilePath& path) {
  // TODO(crbug.com/1147094): Implement for Win.
  return true;
}

// TODO(crbug.com/1212187): maybe handle filtered tokens.
HRESULT IsUserAdmin(bool& is_user_admin) {
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  PSID administrators_group = nullptr;
  if (!::AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &administrators_group)) {
    return HRESULTFromLastError();
  }
  base::ScopedClosureRunner free_sid(
      base::BindOnce([](PSID sid) { ::FreeSid(sid); }, administrators_group));
  BOOL is_member = false;
  if (!::CheckTokenMembership(NULL, administrators_group, &is_member))
    return HRESULTFromLastError();
  is_user_admin = is_member;
  return S_OK;
}

HRESULT IsUserNonElevatedAdmin(bool& is_user_non_elevated_admin) {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_READ, &token))
    return HRESULTFromLastError();
  is_user_non_elevated_admin = false;
  base::win::ScopedHandle token_holder(token);
  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (::GetTokenInformation(token_holder.Get(), TokenElevationType,
                            &elevation_type, sizeof(elevation_type),
                            &size_returned)) {
    if (elevation_type == TokenElevationTypeLimited) {
      is_user_non_elevated_admin = true;
    }
  }
  return S_OK;
}

HRESULT IsUACOn(bool& is_uac_on) {
  // The presence of a split token definitively indicates that UAC is on. But
  // the absence of the token does not necessarily indicate that UAC is off.
  bool is_split_token = false;
  if (SUCCEEDED(IsUserRunningSplitToken(is_split_token)) && is_split_token) {
    is_uac_on = true;
    return S_OK;
  }

  is_uac_on = IsExplorerRunningAtMediumOrLower();
  return S_OK;
}

HRESULT IsElevatedWithUACOn(bool& is_elevated_with_uac_on) {
  bool is_user_admin = false;
  if (SUCCEEDED(IsUserAdmin(is_user_admin)) && !is_user_admin) {
    is_elevated_with_uac_on = false;
    return S_OK;
  }
  return IsUACOn(is_elevated_with_uac_on);
}

std::string GetUACState() {
  std::string s;

  bool is_user_admin = false;
  if (SUCCEEDED(IsUserAdmin(is_user_admin)))
    base::StringAppendF(&s, "IsUserAdmin: %d, ", is_user_admin);

  bool is_user_non_elevated_admin = false;
  if (SUCCEEDED(IsUserNonElevatedAdmin(is_user_non_elevated_admin))) {
    base::StringAppendF(&s, "IsUserNonElevatedAdmin: %d, ",
                        is_user_non_elevated_admin);
  }

  bool is_uac_on = false;
  if (SUCCEEDED(IsUACOn(is_uac_on)))
    base::StringAppendF(&s, "IsUACOn: %d, ", is_uac_on);

  bool is_elevated_with_uac_on = false;
  if (SUCCEEDED(IsElevatedWithUACOn(is_elevated_with_uac_on))) {
    base::StringAppendF(&s, "IsElevatedWithUACOn: %d", is_elevated_with_uac_on);
  }

  return s;
}

std::wstring GetServiceName(bool is_internal_service) {
  std::wstring service_name = GetServiceDisplayName(is_internal_service);
  service_name.erase(
      std::remove_if(service_name.begin(), service_name.end(), isspace),
      service_name.end());
  return service_name;
}

std::wstring GetServiceDisplayName(bool is_internal_service) {
  return base::StrCat(
      {base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" ",
       is_internal_service ? kWindowsInternalServiceName : kWindowsServiceName,
       L" ", kUpdaterVersionUtf16});
}

}  // namespace updater

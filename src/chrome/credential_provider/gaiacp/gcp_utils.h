// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/gaiacp/scoped_handle.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "url/gurl.h"

// These define are documented in
// https://msdn.microsoft.com/en-us/library/bb470234(v=vs.85).aspx not available
// in the user mode headers.
#define DIRECTORY_QUERY 0x00000001
#define DIRECTORY_TRAVERSE 0x00000002
#define DIRECTORY_CREATE_OBJECT 0x00000004
#define DIRECTORY_CREATE_SUBDIRECTORY 0x00000008
#define DIRECTORY_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0xF)

namespace base {

class CommandLine;
class FilePath;

}  // namespace base

namespace credential_provider {

// Windows supports a maximum of 20 characters plus null in username.
constexpr int kWindowsUsernameBufferLength = 21;

// Maximum domain length is 256 characters including null.
// https://support.microsoft.com/en-ca/help/909264/naming-conventions-in-active-directory-for-computers-domains-sites-and
constexpr int kWindowsDomainBufferLength = 256;

// According to:
// https://stackoverflow.com/questions/1140528/what-is-the-maximum-length-of-a-sid-in-sddl-format
constexpr int kWindowsSidBufferLength = 184;

// Max number of attempts to find a new username when a user already exists
// with the same username.
constexpr int kMaxUsernameAttempts = 10;

// First index to append to a username when another user with the same name
// already exists.
constexpr int kInitialDuplicateUsernameIndex = 2;

// Default extension used as a fallback if the picture_url returned from gaia
// does not have a file extension.
extern const wchar_t kDefaultProfilePictureFileExtension[];

// Required extension for the picture that will be shown by the credential on
// the login screen. Windows only supports .bmp files for the images shown by
// credentials.
extern const wchar_t kCredentialLogoPictureFileExtension[];

constexpr int kLargestProfilePictureSize = 448;

// Because of some strange dependency problems with windows header files,
// define STATUS_SUCCESS here instead of including ntstatus.h or SubAuth.h
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

// A bitfield indicating which standard handles are to be created.
using StdHandlesToCreate = uint32_t;

enum : uint32_t {
  kStdOutput = 1 << 0,
  kStdInput = 1 << 1,
  kStdError = 1 << 2,
  kAllStdHandles = kStdOutput | kStdInput | kStdError
};

// Filled in by InitializeStdHandles to return the parent side of stdin/stdout/
// stderr pipes of the login UI process.
struct StdParentHandles {
  StdParentHandles();
  ~StdParentHandles();

  base::win::ScopedHandle hstdin_write;
  base::win::ScopedHandle hstdout_read;
  base::win::ScopedHandle hstderr_read;
};

// Process startup options that allows customization of stdin/stdout/stderr
// handles.
class ScopedStartupInfo {
 public:
  ScopedStartupInfo();
  explicit ScopedStartupInfo(const wchar_t* desktop);
  ~ScopedStartupInfo();

  // This function takes ownership of the handles.
  HRESULT SetStdHandles(base::win::ScopedHandle* hstdin,
                        base::win::ScopedHandle* hstdout,
                        base::win::ScopedHandle* hstderr);

  LPSTARTUPINFOW GetInfo() { return &info_; }

  // Releases all resources held by this info.
  void Shutdown();

 private:
  STARTUPINFOW info_;
  base::string16 desktop_;
};

// Gets the brand specific path in which to install GCPW.
base::FilePath::StringType GetInstallParentDirectoryName();

// Gets the directory where the GCP is installed
base::FilePath GetInstallDirectory();

// Deletes versions of GCP found under |gcp_path| except for version
// |product_version|.
void DeleteVersionsExcept(const base::FilePath& gcp_path,
                          const base::string16& product_version);

// Waits for the process specified by |procinfo| to terminate.  The handles
// in |read_handles| can be used to read stdout/err from the process.  Upon
// return, |exit_code| contains one of the UIEC_xxx constants listed above,
// and |stdout_buffer| and |stderr_buffer| contain the output, if any.
// Both buffers must be at least |buffer_size| characters long.
HRESULT WaitForProcess(base::win::ScopedHandle::Handle process_handle,
                       const StdParentHandles& parent_handles,
                       DWORD* exit_code,
                       char* output_buffer,
                       int buffer_size);

// Creates a restricted, batch or interactive login token for the given user.
HRESULT CreateLogonToken(const wchar_t* domain,
                         const wchar_t* username,
                         const wchar_t* password,
                         bool interactive,
                         base::win::ScopedHandle* token);

HRESULT CreateJobForSignin(base::win::ScopedHandle* job);

// Creates a pipe that can be used by a parent process to communicate with a
// child process.  If |child_reads| is false, then it is expected that the
// parent process will read from |reading| anything the child process writes
// to |writing|.  For example, this is used to read stdout/stderr of child.
//
// If |child_reads| is true, then it is expected that the child process will
// read from |reading| anything the parent process writes to |writing|.  For
// example, this is used to write to stdin of child.
//
// If |use_nul| is true, then the parent's handle is not used (can be passed
// as nullptr).  The child reads from or writes to the null device.
HRESULT CreatePipeForChildProcess(bool child_reads,
                                  bool use_nul,
                                  base::win::ScopedHandle* reading,
                                  base::win::ScopedHandle* writing);

// Initializes 3 pipes for communicating with a child process.  On return,
// |startupinfo| will be set with the handles needed by the child.  This is
// used when creating the child process.  |parent_handles| contains the
// corresponding handles to be used by the parent process.
//
// Communication direction is used to optimize handle creation.  If
// communication occurs in only one direction then some pipes will be directed
// to the nul device.
enum class CommDirection {
  kParentToChildOnly,
  kChildToParentOnly,
  kBidirectional,
};
HRESULT InitializeStdHandles(CommDirection direction,
                             StdHandlesToCreate to_create,
                             ScopedStartupInfo* startupinfo,
                             StdParentHandles* parent_handles);

// Fills |path_to_dll| with the short path to the dll referenced by
// |dll_handle|. The short path is needed to correctly call rundll32.exe in
// cases where there might be quotes or spaces in the path.
HRESULT GetPathToDllFromHandle(HINSTANCE dll_handle,
                               base::FilePath* path_to_dll);

// This function gets a correctly formatted entry point argument to pass to
// rundll32.exe for a dll referenced by the handle |dll_handle| and an entry
// point function with the name |entrypoint|. |entrypoint_arg| will be filled
// with the argument value.
HRESULT GetEntryPointArgumentForRunDll(HINSTANCE dll_handle,
                                       const wchar_t* entrypoint,
                                       base::string16* entrypoint_arg);

// This function is used to build the command line for rundll32 to call an
// exported entrypoint from the DLL given by |dll_handle|.
// Returns S_FALSE if a command line can successfully be built but if the
// path to the "dll" actually points to a non ".dll" file. This allows
// detection of calls to this function via a unit test which will be
// running under an ".exe" module.
HRESULT GetCommandLineForEntrypoint(HINSTANCE dll_handle,
                                    const wchar_t* entrypoint,
                                    base::CommandLine* command_line);

// Handles the writing and deletion of a startup sentinel file used to ensure
// that the GCPW does not crash continuously on startup and render the
// winlogon process unusable.
bool VerifyStartupSentinel();
void DeleteStartupSentinel();

// Gets a string resource from the DLL with the given id.
base::string16 GetStringResource(int base_message_id);

// Fills |base_path| with the path where user profile pictures are stored for
// user with |sid|. This function can fail if the known folder
// FOLDERID_PublicUserTiles cannot be found.
HRESULT GetUserAccountPicturePath(const base::string16& sid,
                                  base::FilePath* base_path);

// Returns the full path to a user profile picture of a specific |size| and
// |picture_extension|. |account_picture_path| is path filled in by a call to
// GetUserAccountPicturePath.
base::FilePath GetUserSizedAccountPictureFilePath(
    const base::FilePath& account_picture_path,
    int size,
    const base::string16& picture_extension);

// Gets the language selected by the base::win::i18n::LanguageSelector.
base::string16 GetSelectedLanguage();

// Helpers to get strings from DictionaryValues.
base::string16 GetDictString(const base::DictionaryValue* dict,
                             const char* name);
base::string16 GetDictString(const std::unique_ptr<base::DictionaryValue>& dict,
                             const char* name);
std::string GetDictStringUTF8(const base::DictionaryValue* dict,
                              const char* name);
std::string GetDictStringUTF8(
    const std::unique_ptr<base::DictionaryValue>& dict,
    const char* name);

// Returns the major build version of Windows by reading the registry.
// See:
// https://stackoverflow.com/questions/31072543/reliable-way-to-get-windows-version-from-registry
base::string16 GetWindowsVersion();

class OSUserManager;
class OSProcessManager;

// This structure is used in tests to set fake objects in the credential
// provider dll.  See the function SetFakesForTesting() for details.
struct FakesForTesting {
  FakesForTesting();
  ~FakesForTesting();

  ScopedLsaPolicy::CreatorCallback scoped_lsa_policy_creator;
  OSUserManager* os_user_manager_for_testing = nullptr;
  OSProcessManager* os_process_manager_for_testing = nullptr;
};

// DLL entrypoint signature for settings testing fakes.  This is used by
// the setup tests to install fakes into the dynamically loaded gaia1_0 DLL
// static data.  This way the production DLL does not need to include binary
// code used only for testing.
typedef void CALLBACK (*SetFakesForTestingFn)(const FakesForTesting* fakes);

// Initializes the members of a Windows STRING struct (UNICODE_STRING or
// LSA_STRING) to point to the string pointed to by |string|.
template <class WindowsStringT,
          class WindowsStringCharT = decltype(WindowsStringT().Buffer[0])>
void InitWindowsStringWithString(const WindowsStringCharT* string,
                                 WindowsStringT* windows_string) {
  constexpr size_t buffer_char_size = sizeof(WindowsStringCharT);
  windows_string->Buffer = const_cast<WindowsStringCharT*>(string);
  windows_string->Length = static_cast<USHORT>(
      std::char_traits<WindowsStringCharT>::length((windows_string->Buffer)) *
      buffer_char_size);
  windows_string->MaximumLength = windows_string->Length + buffer_char_size;
}

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_UTILS_H_

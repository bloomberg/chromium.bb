// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/scoped_handle.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/chrome_availability_checker.h"
#include "chrome/credential_provider/gaiacp/event_logging_api_manager.h"
#include "chrome/credential_provider/gaiacp/event_logs_upload_manager.h"
#include "chrome/credential_provider/gaiacp/gem_device_details_manager.h"
#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"
#include "chrome/credential_provider/gaiacp/os_process_manager.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace base {
class WaitableEvent;
}

namespace credential_provider {

enum class FAILEDOPERATIONS {
  ADD_USER,
  CHANGE_PASSWORD,
  GET_USER_FULLNAME,
  SET_USER_FULLNAME
};

void InitializeRegistryOverrideForTesting(
    registry_util::RegistryOverrideManager* registry_override);

///////////////////////////////////////////////////////////////////////////////

class FakeOSProcessManager : public OSProcessManager {
 public:
  FakeOSProcessManager();
  ~FakeOSProcessManager() override;

  // OSProcessManager
  HRESULT GetTokenLogonSID(const base::win::ScopedHandle& token,
                           PSID* sid) override;
  HRESULT SetupPermissionsForLogonSid(PSID sid) override;
  HRESULT CreateProcessWithToken(
      const base::win::ScopedHandle& logon_token,
      const base::CommandLine& command_line,
      _STARTUPINFOW* startupinfo,
      base::win::ScopedProcessInformation* procinfo) override;

 private:
  OSProcessManager* original_manager_;
  DWORD next_rid_ = 0;
};

///////////////////////////////////////////////////////////////////////////////

class FakeOSUserManager : public OSUserManager {
 public:
  FakeOSUserManager();
  ~FakeOSUserManager() override;

  // OSUserManager
  HRESULT GenerateRandomPassword(wchar_t* password, int length) override;
  HRESULT AddUser(const wchar_t* username,
                  const wchar_t* password,
                  const wchar_t* fullname,
                  const wchar_t* comment,
                  bool add_to_users_group,
                  BSTR* sid,
                  DWORD* error) override;
  // Add a user to the OS with domain associated with it.
  HRESULT AddUser(const wchar_t* username,
                  const wchar_t* password,
                  const wchar_t* fullname,
                  const wchar_t* comment,
                  bool add_to_users_group,
                  const wchar_t* domain,
                  BSTR* sid,
                  DWORD* error);
  HRESULT ChangeUserPassword(const wchar_t* domain,
                             const wchar_t* username,
                             const wchar_t* password,
                             const wchar_t* old_password) override;
  HRESULT SetUserPassword(const wchar_t* domain,
                          const wchar_t* username,
                          const wchar_t* password) override;
  HRESULT SetUserFullname(const wchar_t* domain,
                          const wchar_t* username,
                          const wchar_t* full_name) override;
  HRESULT IsWindowsPasswordValid(const wchar_t* domain,
                                 const wchar_t* username,
                                 const wchar_t* password) override;

  HRESULT CreateLogonToken(const wchar_t* domain,
                           const wchar_t* username,
                           const wchar_t* password,
                           bool interactive,
                           base::win::ScopedHandle* token) override;
  HRESULT GetUserSID(const wchar_t* domain,
                     const wchar_t* username,
                     PSID* sid) override;
  HRESULT FindUserBySID(const wchar_t* sid,
                        wchar_t* username,
                        DWORD username_size,
                        wchar_t* domain,
                        DWORD domain_size) override;
  HRESULT RemoveUser(const wchar_t* username, const wchar_t* password) override;

  HRESULT GetUserFullname(const wchar_t* domain,
                          const wchar_t* username,
                          base::string16* fullname) override;

  HRESULT ModifyUserAccessWithLogonHours(const wchar_t* domain,
                                         const wchar_t* username,
                                         bool allow) override;

  bool IsDeviceDomainJoined() override;

  void SetIsDeviceDomainJoined(bool is_device_domain_joined) {
    is_device_domain_joined_ = is_device_domain_joined;
  }

  struct UserInfo {
    UserInfo(const wchar_t* domain,
             const wchar_t* password,
             const wchar_t* fullname,
             const wchar_t* comment,
             const wchar_t* sid);
    UserInfo();
    UserInfo(const UserInfo& other);
    ~UserInfo();

    bool operator==(const UserInfo& other) const;

    base::string16 domain;
    base::string16 password;
    base::string16 fullname;
    base::string16 comment;
    base::string16 sid;
  };
  const UserInfo GetUserInfo(const wchar_t* username);

  // Creates a new unique sid.  Free returned sid with FreeSid().
  HRESULT CreateNewSID(PSID* sid);

  // Creates a fake user with the given |username|, |password|, |fullname|,
  // |comment|. If |gaia_id| is non-empty, also associates the user with
  // the given gaia id. If |email| is non-empty, sets the email to use for
  // reauth to be this one.
  // |sid| is allocated and filled with the SID of the new user.
  HRESULT CreateTestOSUser(const base::string16& username,
                           const base::string16& password,
                           const base::string16& fullname,
                           const base::string16& comment,
                           const base::string16& gaia_id,
                           const base::string16& email,
                           BSTR* sid);

  // Creates a fake user with the given |username|, |password|, |fullname|,
  // |comment| and |domain|. If |gaia_id| is non-empty, also associates the
  // user with the given gaia id. If |email| is non-empty, sets the email to
  // use for reauth to be this one.
  // |sid| is allocated and filled with the SID of the new user.
  HRESULT CreateTestOSUser(const base::string16& username,
                           const base::string16& password,
                           const base::string16& fullname,
                           const base::string16& comment,
                           const base::string16& gaia_id,
                           const base::string16& email,
                           const base::string16& domain,
                           BSTR* sid);

  size_t GetUserCount() const { return username_to_info_.size(); }
  std::vector<std::pair<base::string16, base::string16>> GetUsers() const;

  void SetFailureReason(FAILEDOPERATIONS failed_operaetion,
                        HRESULT failure_reason) {
    failure_reasons_[failed_operaetion] = failure_reason;
  }

  bool DoesOperationFail(FAILEDOPERATIONS op) {
    return failure_reasons_.find(op) != failure_reasons_.end();
  }

  void RestoreOperation(FAILEDOPERATIONS op) { failure_reasons_.erase(op); }

 private:
  OSUserManager* original_manager_;
  DWORD next_rid_ = 0;
  std::map<base::string16, UserInfo> username_to_info_;
  bool is_device_domain_joined_ = false;
  std::map<FAILEDOPERATIONS, HRESULT> failure_reasons_;
};

///////////////////////////////////////////////////////////////////////////////

class FakeScopedLsaPolicyFactory {
 public:
  FakeScopedLsaPolicyFactory();
  virtual ~FakeScopedLsaPolicyFactory();

  ScopedLsaPolicy::CreatorCallback GetCreatorCallback();

  // PrivateDataMap is a string-to-string key/value store that maps private
  // names to their corresponding data strings.  The term "private" here is
  // used to reflect the name of the underlying OS calls.  This data is meant
  // to be shared by all ScopedLsaPolicy instances created by this factory.
  using PrivateDataMap = std::map<base::string16, base::string16>;
  PrivateDataMap& private_data() { return private_data_; }

 private:
  std::unique_ptr<ScopedLsaPolicy> Create(ACCESS_MASK mask);

  ScopedLsaPolicy::CreatorCallback original_creator_;
  PrivateDataMap private_data_;
};

class FakeScopedLsaPolicy : public ScopedLsaPolicy {
 public:
  ~FakeScopedLsaPolicy() override;

  // ScopedLsaPolicy
  HRESULT StorePrivateData(const wchar_t* key, const wchar_t* value) override;
  HRESULT RemovePrivateData(const wchar_t* key) override;
  HRESULT RetrievePrivateData(const wchar_t* key,
                              wchar_t* value,
                              size_t length) override;
  bool PrivateDataExists(const wchar_t* key) override;
  HRESULT AddAccountRights(PSID sid,
                           const std::vector<base::string16>& rights) override;
  HRESULT RemoveAccountRights(
      PSID sid,
      const std::vector<base::string16>& rights) override;
  HRESULT RemoveAccount(PSID sid) override;

 private:
  friend class FakeScopedLsaPolicyFactory;

  explicit FakeScopedLsaPolicy(FakeScopedLsaPolicyFactory* factory);

  FakeScopedLsaPolicyFactory::PrivateDataMap& private_data() {
    return factory_->private_data();
  }

  FakeScopedLsaPolicyFactory* factory_;
};

///////////////////////////////////////////////////////////////////////////////

// A scoped FakeScopedUserProfile factory.  Installs itself when constructed
// and removes itself when deleted.
class FakeScopedUserProfileFactory {
 public:
  FakeScopedUserProfileFactory();
  virtual ~FakeScopedUserProfileFactory();

 private:
  std::unique_ptr<ScopedUserProfile> Create(const base::string16& sid,
                                            const base::string16& domain,
                                            const base::string16& username,
                                            const base::string16& password);

  ScopedUserProfile::CreatorCallback original_creator_;
};

class FakeScopedUserProfile : public ScopedUserProfile {
 public:
  HRESULT SaveAccountInfo(const base::Value& properties) override;

 private:
  friend class FakeScopedUserProfileFactory;

  FakeScopedUserProfile(const base::string16& sid,
                        const base::string16& domain,
                        const base::string16& username,
                        const base::string16& password);
  ~FakeScopedUserProfile() override;

  bool is_valid_ = false;
};

///////////////////////////////////////////////////////////////////////////////

// A scoped FakeWinHttpUrlFetcher factory.  Installs itself when constructed
// and removes itself when deleted.
class FakeWinHttpUrlFetcherFactory {
 public:
  FakeWinHttpUrlFetcherFactory();
  ~FakeWinHttpUrlFetcherFactory();

  // Sets the given |response| for any number of HTTP requests made for |url|.
  void SetFakeResponse(
      const GURL& url,
      const WinHttpUrlFetcher::Headers& headers,
      const std::string& response,
      HANDLE send_response_event_handle = INVALID_HANDLE_VALUE);

  // Queues the given |response| for the specified |num_requests| number of HTTP
  // requests made for |url|. Different responses for the URL can be set by
  // calling this function multiple times with different responses.
  void SetFakeResponseForSpecifiedNumRequests(
      const GURL& url,
      const WinHttpUrlFetcher::Headers& headers,
      const std::string& response,
      unsigned int num_requests,
      HANDLE send_response_event_handle = INVALID_HANDLE_VALUE);

  // Sets the response as a failed http attempt. The return result
  // from http_url_fetcher.Fetch() would be set as the input HRESULT
  // to this method.
  void SetFakeFailedResponse(const GURL& url, HRESULT failed_hr);

  // Sets the option to collect request data for each URL fetcher created.
  void SetCollectRequestData(bool value) { collect_request_data_ = value; }

  // Data used to make each HTTP request by the fetcher.
  struct RequestData {
    RequestData();
    RequestData(const RequestData& rhs);
    ~RequestData();
    WinHttpUrlFetcher::Headers headers;
    std::string body;
    int timeout_in_millis;
  };

  // Returns the request data for the request identified by |request_index|.
  RequestData GetRequestData(size_t request_index) const;

  // Returns the number of requests created.
  size_t requests_created() const { return requests_created_; }

 private:
  std::unique_ptr<WinHttpUrlFetcher> Create(const GURL& url);

  WinHttpUrlFetcher::CreatorCallback original_creator_;

  struct Response {
    Response();
    Response(const Response& rhs);
    Response(const WinHttpUrlFetcher::Headers& new_headers,
             const std::string& new_response,
             HANDLE new_send_response_event_handle);
    ~Response();
    WinHttpUrlFetcher::Headers headers;
    std::string response;
    HANDLE send_response_event_handle;
  };

  std::map<GURL, std::deque<Response>> fake_responses_;
  std::map<GURL, HRESULT> failed_http_fetch_hr_;
  size_t requests_created_ = 0;
  bool collect_request_data_ = false;
  bool remove_fake_response_when_created_ = false;
  std::vector<RequestData> requests_data_;
};

class FakeWinHttpUrlFetcher : public WinHttpUrlFetcher {
 public:
  explicit FakeWinHttpUrlFetcher(const GURL& url);
  ~FakeWinHttpUrlFetcher() override;

  using WinHttpUrlFetcher::Headers;

  const Headers& response_headers() const { return response_headers_; }

  // WinHttpUrlFetcher
  bool IsValid() const override;
  HRESULT SetRequestHeader(const char* name, const char* value) override;
  HRESULT SetRequestBody(const char* body) override;
  HRESULT SetHttpRequestTimeout(const int timeout_in_millis) override;
  HRESULT Fetch(std::vector<char>* response) override;
  HRESULT Close() override;

 private:
  friend FakeWinHttpUrlFetcherFactory;
  typedef FakeWinHttpUrlFetcherFactory::RequestData RequestData;

  Headers response_headers_;
  std::string response_;
  HANDLE send_response_event_handle_;
  HRESULT response_hr_ = S_OK;
  RequestData* request_data_ = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

class FakeAssociatedUserValidator : public AssociatedUserValidator {
 public:
  FakeAssociatedUserValidator();
  explicit FakeAssociatedUserValidator(base::TimeDelta validation_timeout);
  ~FakeAssociatedUserValidator() override;

  using AssociatedUserValidator::ForceRefreshTokenHandlesForTesting;
  using AssociatedUserValidator::IsUserAccessBlockedForTesting;

 private:
  AssociatedUserValidator* original_validator_ = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

class FakeChromeAvailabilityChecker : public ChromeAvailabilityChecker {
 public:
  enum HasSupportedChromeCheckType {
    kChromeForceYes,
    kChromeForceNo,
    kChromeDontForce  // Uses the original checker to get result.
  };

  FakeChromeAvailabilityChecker(
      HasSupportedChromeCheckType has_supported_chrome = kChromeForceYes);
  ~FakeChromeAvailabilityChecker() override;

  bool HasSupportedChromeVersion() override;
  void SetHasSupportedChrome(HasSupportedChromeCheckType has_supported_chrome);

 private:
  ChromeAvailabilityChecker* original_checker_ = nullptr;

  // Used during tests to force the credential provider to believe if a
  // supported Chrome version is installed or not. In production a real
  // check is performed at runtime.
  HasSupportedChromeCheckType has_supported_chrome_ = kChromeForceYes;
};

///////////////////////////////////////////////////////////////////////////////

class FakeInternetAvailabilityChecker : public InternetAvailabilityChecker {
 public:
  enum HasInternetConnectionCheckType { kHicForceYes, kHicForceNo };

  FakeInternetAvailabilityChecker(
      HasInternetConnectionCheckType has_internet_connection = kHicForceYes);
  ~FakeInternetAvailabilityChecker() override;

  bool HasInternetConnection() override;
  void SetHasInternetConnection(
      HasInternetConnectionCheckType has_internet_connection);

 private:
  InternetAvailabilityChecker* original_checker_ = nullptr;

  // Used during tests to force the credential provider to believe if an
  // internet connection is possible or not.  In production the value is
  // always set to HIC_CHECK_ALWAYS to perform a real check at runtime.
  HasInternetConnectionCheckType has_internet_connection_ = kHicForceYes;
};

///////////////////////////////////////////////////////////////////////////////

class FakePasswordRecoveryManager : public PasswordRecoveryManager {
 public:
  FakePasswordRecoveryManager();
  explicit FakePasswordRecoveryManager(
      base::TimeDelta encryption_key_request_timeout,
      base::TimeDelta decryption_key_request_timeout);
  ~FakePasswordRecoveryManager() override;

  using PasswordRecoveryManager::MakeGenerateKeyPairResponseForTesting;
  using PasswordRecoveryManager::MakeGetPrivateKeyResponseForTesting;
  using PasswordRecoveryManager::SetRequestTimeoutForTesting;

 private:
  PasswordRecoveryManager* original_validator_ = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

class FakeGemDeviceDetailsManager : public GemDeviceDetailsManager {
 public:
  FakeGemDeviceDetailsManager();
  explicit FakeGemDeviceDetailsManager(
      base::TimeDelta upload_device_details_request_timeout);
  ~FakeGemDeviceDetailsManager() override;

  using GemDeviceDetailsManager::GetRequestDictForTesting;
  using GemDeviceDetailsManager::GetUploadStatusForTesting;
  using GemDeviceDetailsManager::SetRequestTimeoutForTesting;

 private:
  GemDeviceDetailsManager* original_manager_ = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

class FakeEventLoggingApiManager : public EventLoggingApiManager {
 public:
  typedef EventLogsUploadManager::EventLogEntry EventLogEntry;

  EVT_HANDLE EvtQuery(EVT_HANDLE session,
                      LPCWSTR path,
                      LPCWSTR query,
                      DWORD flags) override;

  EVT_HANDLE EvtOpenPublisherMetadata(EVT_HANDLE session,
                                      LPCWSTR publisher_id,
                                      LPCWSTR log_file_path,
                                      LCID locale,
                                      DWORD flags) override;

  EVT_HANDLE EvtCreateRenderContext(DWORD value_paths_count,
                                    LPCWSTR* value_paths,
                                    DWORD flags) override;

  BOOL EvtNext(EVT_HANDLE result_set,
               DWORD events_size,
               PEVT_HANDLE events,
               DWORD timeout,
               DWORD flags,
               PDWORD num_returned) override;

  BOOL EvtGetQueryInfo(EVT_HANDLE query,
                       EVT_QUERY_PROPERTY_ID property_id,
                       DWORD value_buffer_size,
                       PEVT_VARIANT value_buffer,
                       PDWORD value_buffer_used) override;

  BOOL EvtRender(EVT_HANDLE context,
                 EVT_HANDLE evt_handle,
                 DWORD flags,
                 DWORD buffer_size,
                 PVOID buffer,
                 PDWORD buffer_used,
                 PDWORD property_count) override;

  BOOL EvtFormatMessage(EVT_HANDLE publisher_metadata,
                        EVT_HANDLE event,
                        DWORD message_id,
                        DWORD value_count,
                        PEVT_VARIANT values,
                        DWORD flags,
                        DWORD buffer_size,
                        LPWSTR buffer,
                        PDWORD buffer_used) override;

  BOOL EvtClose(EVT_HANDLE handle) override;

  DWORD GetLastError() override;

  explicit FakeEventLoggingApiManager(const std::vector<EventLogEntry>& logs);

  ~FakeEventLoggingApiManager() override;

 private:
  EventLoggingApiManager* original_manager_ = nullptr;

  const std::vector<EventLogEntry>& logs_;
  EVT_HANDLE query_handle_, publisher_metadata_, render_context_;
  DWORD last_error_;
  size_t next_event_idx_;
  std::vector<EVT_HANDLE> event_handles_;
  std::unordered_map<EVT_HANDLE, size_t> handle_to_index_map_;
};

class FakeEventLogsUploadManager : public EventLogsUploadManager {
 public:
  typedef EventLogsUploadManager::EventLogEntry EventLogEntry;

  // Construct with the logs that should be present in the fake event log.
  explicit FakeEventLogsUploadManager(const std::vector<EventLogEntry>& logs);
  ~FakeEventLogsUploadManager() override;

  // Get the last upload status of the call to UploadEventViewerLogs.
  HRESULT GetUploadStatus();

  // Get the number of successfully uploaded event logs.
  uint64_t GetNumLogsUploaded();

 private:
  EventLogsUploadManager* original_manager_ = nullptr;
  FakeEventLoggingApiManager api_manager_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/session_manager/fake_session_manager_client.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/login_manager/policy_descriptor.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/sha2.h"

namespace chromeos {

using RetrievePolicyCallback = FakeSessionManagerClient::RetrievePolicyCallback;
using RetrievePolicyResponseType =
    FakeSessionManagerClient::RetrievePolicyResponseType;

namespace {

constexpr char kStubDevicePolicyFileNamePrefix[] = "stub_device_policy";
constexpr char kStubPerAccountPolicyFileNamePrefix[] = "stub_policy";
constexpr char kStubStateKeysFileName[] = "stub_state_keys";
constexpr char kStubExtensionPolicyFileNameFragment[] = "_extension_";
constexpr char kStubSigninExtensionPolicyFileNameFragment[] =
    "_signin_extension_";
constexpr char kStubPerAccountPolicyKeyFileName[] = "policy.pub";
constexpr char kEmptyAccountId[] = "";

// Global flag weather the g_instance in SessionManagerClient is a
// FakeSessionManagerClient or not.
bool g_is_fake = false;

// Helper to asynchronously retrieve a file's content.
std::string GetFileContent(const base::FilePath& path) {
  std::string result;
  if (!path.empty())
    base::ReadFileToString(path, &result);
  return result;
}

// Helper to write files in a background thread.
void StoreFiles(std::map<base::FilePath, std::string> paths_and_data) {
  for (const auto& kv : paths_and_data) {
    const base::FilePath& path = kv.first;
    if (path.empty() || !base::CreateDirectory(path.DirName())) {
      LOG(WARNING) << "Failed to write to " << path.value();
      continue;
    }
    const std::string& data = kv.second;
    int result = base::WriteFile(path, data.data(), data.size());
    if (result == -1 || static_cast<size_t>(result) != data.size())
      LOG(WARNING) << "Failed to write to " << path.value();
  }
}

// Creates a PolicyDescriptor object to store/retrieve Chrome policy.
login_manager::PolicyDescriptor MakeChromePolicyDescriptor(
    login_manager::PolicyAccountType account_type,
    const std::string& account_id) {
  login_manager::PolicyDescriptor descriptor;
  descriptor.set_account_type(account_type);
  descriptor.set_account_id(account_id);
  descriptor.set_domain(login_manager::POLICY_DOMAIN_CHROME);
  return descriptor;
}

// Returns true if the policy descriptor points to Chrome device policy.
bool IsChromeDevicePolicy(const login_manager::PolicyDescriptor& descriptor) {
  DCHECK(descriptor.has_account_type());
  DCHECK(descriptor.has_domain());
  return descriptor.account_type() == login_manager::ACCOUNT_TYPE_DEVICE &&
         descriptor.domain() == login_manager::POLICY_DOMAIN_CHROME;
}

// Helper to asynchronously read (or if missing create) state key stubs.
std::vector<std::string> ReadCreateStateKeysStub(const base::FilePath& path) {
  std::string contents;
  if (base::PathExists(path)) {
    contents = GetFileContent(path);
  } else {
    // Create stub state keys on the fly.
    for (int i = 0; i < 5; ++i) {
      contents += crypto::SHA256HashString(
          base::NumberToString(i) +
          base::NumberToString(base::Time::Now().ToJavaTime()));
    }
    StoreFiles({{path, contents}});
  }

  std::vector<std::string> state_keys;
  for (size_t i = 0; i < contents.size() / 32; ++i)
    state_keys.push_back(contents.substr(i * 32, 32));
  return state_keys;
}

// Gets the postfix of the stub policy filename, which is based the
// |descriptor|'s domain and component id. Returns an empty string if the domain
// doesn't use a component id (e.g. normal Chrome user/device policy).
std::string GetStubPolicyFilenamePostfix(
    const login_manager::PolicyDescriptor& descriptor) {
  DCHECK(descriptor.has_domain());
  switch (descriptor.domain()) {
    case login_manager::POLICY_DOMAIN_CHROME:
      return std::string();
    case login_manager::POLICY_DOMAIN_EXTENSIONS:
      DCHECK(descriptor.has_component_id());
      return kStubExtensionPolicyFileNameFragment + descriptor.component_id();
    case login_manager::POLICY_DOMAIN_SIGNIN_EXTENSIONS:
      DCHECK(descriptor.has_component_id());
      return kStubSigninExtensionPolicyFileNameFragment +
             descriptor.component_id();
  }
  NOTREACHED();
  return std::string();
}

// Returns the last part of the stub policy file path consisting of the filename
// for device accounts and <cryptohome_id>/filename for user and device local
// accounts, e.g.
//   "stub_device_policy" for Chrome device policy or
//   "<cryptohome_id>/stub_policy_extension_<id>" for extension policy.
// This path is also used as key in the in-memory policy map |policy_|.
base::FilePath GetStubRelativePolicyPath(
    const login_manager::PolicyDescriptor& descriptor) {
  DCHECK(descriptor.has_account_type());
  std::string postfix = GetStubPolicyFilenamePostfix(descriptor);
  switch (descriptor.account_type()) {
    case login_manager::ACCOUNT_TYPE_DEVICE:
      return base::FilePath(kStubDevicePolicyFileNamePrefix + postfix);

    case login_manager::ACCOUNT_TYPE_USER:
    case login_manager::ACCOUNT_TYPE_SESSIONLESS_USER:
    case login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT: {
      DCHECK(descriptor.has_account_id());
      cryptohome::AccountIdentifier cryptohome_id;
      cryptohome_id.set_account_id(descriptor.account_id());
      const std::string sanitized_id =
          CryptohomeClient::GetStubSanitizedUsername(cryptohome_id);
      return base::FilePath(sanitized_id)
          .AppendASCII(kStubPerAccountPolicyFileNamePrefix + postfix);
    }
  }
  NOTREACHED();
  return base::FilePath();
}

// Gets the stub file paths of the policy blob and optionally the policy key
// (|key_path|) for the given |descriptor|. |key_path| can be nullptr.
base::FilePath GetStubPolicyFilePath(
    const login_manager::PolicyDescriptor& descriptor,
    base::FilePath* key_path) {
  if (key_path)
    key_path->clear();

  base::FilePath relative_policy_path = GetStubRelativePolicyPath(descriptor);
  DCHECK(descriptor.has_account_type());
  switch (descriptor.account_type()) {
    case login_manager::ACCOUNT_TYPE_DEVICE: {
      base::FilePath owner_key_path;
      CHECK(
          base::PathService::Get(dbus_paths::FILE_OWNER_KEY, &owner_key_path));
      if (key_path)
        *key_path = owner_key_path;
      return owner_key_path.DirName().Append(relative_policy_path);
    }

    case login_manager::ACCOUNT_TYPE_USER:
    case login_manager::ACCOUNT_TYPE_SESSIONLESS_USER:
    case login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT: {
      base::FilePath base_path;
      CHECK(
          base::PathService::Get(dbus_paths::DIR_USER_POLICY_KEYS, &base_path));
      if (key_path) {
        *key_path = base_path.Append(relative_policy_path.DirName())
                        .AppendASCII(kStubPerAccountPolicyKeyFileName);
      }
      return base_path.Append(relative_policy_path);
    }
  }
  NOTREACHED();
  return base::FilePath();
}

// Returns a key that's used for storing policy in memory.
std::string GetMemoryStorageKey(
    const login_manager::PolicyDescriptor& descriptor) {
  base::FilePath relative_policy_path = GetStubRelativePolicyPath(descriptor);
  DCHECK(!relative_policy_path.empty());
  return relative_policy_path.value();
}

// Posts a task to call callback(response).
template <typename CallbackType, typename ResponseType>
void PostReply(const base::Location& from_here,
               CallbackType callback,
               ResponseType response) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      from_here, base::BindOnce(std::move(callback), std::move(response)));
}

}  // namespace

FakeSessionManagerClient::FakeSessionManagerClient()
    : FakeSessionManagerClient(PolicyStorageType::kInMemory) {}

// This constructor will implicitly create a global static variable by
// SessionManagerClient::SessionManagerClient() that can be retrieved
// via FakeSessionManagerClient::Get() down casted. With the global
// flag g_is_fake we make sure that either the SessionManagerClient or
// the FakeSessionManagerClient constructor is called but not both.
FakeSessionManagerClient::FakeSessionManagerClient(
    PolicyStorageType policy_storage)
    : policy_storage_(policy_storage) {
  DCHECK(!g_is_fake);
  g_is_fake = true;
}

FakeSessionManagerClient::~FakeSessionManagerClient() {
  g_is_fake = false;
}

// static
// Returns the static instance of FakeSessionManagerClient if the
// g_instance in SessionManagerClientis a FakeSessionManagerClient otherwise it
// will return nullptr.
FakeSessionManagerClient* FakeSessionManagerClient::Get() {
  SessionManagerClient* client = SessionManagerClient::Get();
  if (g_is_fake)
    return static_cast<FakeSessionManagerClient*>(client);
  else
    return nullptr;
}

void FakeSessionManagerClient::SetStubDelegate(StubDelegate* delegate) {
  delegate_ = delegate;
}

void FakeSessionManagerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSessionManagerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeSessionManagerClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakeSessionManagerClient::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

bool FakeSessionManagerClient::IsScreenLocked() const {
  return screen_is_locked_;
}

void FakeSessionManagerClient::EmitLoginPromptVisible() {
  for (auto& observer : observers_)
    observer.EmitLoginPromptVisibleCalled();
}

void FakeSessionManagerClient::EmitAshInitialized() {}

void FakeSessionManagerClient::RestartJob(int socket_fd,
                                          const std::vector<std::string>& argv,
                                          VoidDBusMethodCallback callback) {
  DCHECK(supports_browser_restart_);

  restart_job_argv_ = argv;
  if (restart_job_callback_)
    std::move(restart_job_callback_).Run();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeSessionManagerClient::SaveLoginPassword(const std::string& password) {
  login_password_ = password;
}

void FakeSessionManagerClient::LoginScreenStorageStore(
    const std::string& key,
    const login_manager::LoginScreenStorageMetadata& metadata,
    const std::string& data,
    LoginScreenStorageStoreCallback callback) {
  PostReply(FROM_HERE, std::move(callback), base::nullopt /* error */);
}

void FakeSessionManagerClient::LoginScreenStorageRetrieve(
    const std::string& key,
    LoginScreenStorageRetrieveCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), "Test" /* data */,
                                base::nullopt /* error */));
}

void FakeSessionManagerClient::LoginScreenStorageListKeys(
    LoginScreenStorageListKeysCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::vector<std::string>() /* keys */,
                     base::nullopt /* error */));
}

void FakeSessionManagerClient::LoginScreenStorageDelete(
    const std::string& key) {}

void FakeSessionManagerClient::StartSession(
    const cryptohome::AccountIdentifier& cryptohome_id) {
  DCHECK_EQ(0UL, user_sessions_.count(cryptohome_id.account_id()));
  std::string user_id_hash =
      CryptohomeClient::GetStubSanitizedUsername(cryptohome_id);
  user_sessions_[cryptohome_id.account_id()] = user_id_hash;
}

void FakeSessionManagerClient::StopSession(
    login_manager::SessionStopReason reason) {
  session_stopped_ = true;
}

void FakeSessionManagerClient::StartDeviceWipe() {
  start_device_wipe_call_count_++;
  if (!on_start_device_wipe_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(on_start_device_wipe_callback_));
  }
}

void FakeSessionManagerClient::StartRemoteDeviceWipe(
    const enterprise_management::SignedData& signed_command) {
  start_device_wipe_call_count_++;
  if (!on_start_device_wipe_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(on_start_device_wipe_callback_));
  }
}

void FakeSessionManagerClient::ClearForcedReEnrollmentVpd(
    VoidDBusMethodCallback callback) {
  clear_forced_re_enrollment_vpd_call_count_++;
  PostReply(FROM_HERE, std::move(callback), true);
}

void FakeSessionManagerClient::StartTPMFirmwareUpdate(
    const std::string& update_mode) {
  last_tpm_firmware_update_mode_ = update_mode;
  start_tpm_firmware_update_call_count_++;
}

void FakeSessionManagerClient::RequestLockScreen() {
  request_lock_screen_call_count_++;
  if (delegate_)
    delegate_->LockScreenForStub();
}

void FakeSessionManagerClient::NotifyLockScreenShown() {
  notify_lock_screen_shown_call_count_++;
  screen_is_locked_ = true;
}

void FakeSessionManagerClient::NotifyLockScreenDismissed() {
  notify_lock_screen_dismissed_call_count_++;
  screen_is_locked_ = false;
}

void FakeSessionManagerClient::RetrieveActiveSessions(
    ActiveSessionsCallback callback) {
  PostReply(FROM_HERE, std::move(callback), user_sessions_);
}

void FakeSessionManagerClient::RetrieveDevicePolicy(
    RetrievePolicyCallback callback) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
  RetrievePolicy(descriptor, std::move(callback));
}

RetrievePolicyResponseType
FakeSessionManagerClient::BlockingRetrieveDevicePolicy(
    std::string* policy_out) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
  return BlockingRetrievePolicy(descriptor, policy_out);
}

void FakeSessionManagerClient::RetrievePolicyForUser(
    const cryptohome::AccountIdentifier& cryptohome_id,
    RetrievePolicyCallback callback) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_USER, cryptohome_id.account_id());
  RetrievePolicy(descriptor, std::move(callback));
}

RetrievePolicyResponseType
FakeSessionManagerClient::BlockingRetrievePolicyForUser(
    const cryptohome::AccountIdentifier& cryptohome_id,
    std::string* policy_out) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_USER, cryptohome_id.account_id());
  return BlockingRetrievePolicy(descriptor, policy_out);
}

void FakeSessionManagerClient::RetrievePolicyForUserWithoutSession(
    const cryptohome::AccountIdentifier& cryptohome_id,
    RetrievePolicyCallback callback) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_SESSIONLESS_USER, cryptohome_id.account_id());
  RetrievePolicy(descriptor, std::move(callback));
}

void FakeSessionManagerClient::RetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    RetrievePolicyCallback callback) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_id);
  RetrievePolicy(descriptor, std::move(callback));
}

RetrievePolicyResponseType
FakeSessionManagerClient::BlockingRetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    std::string* policy_out) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_id);
  return BlockingRetrievePolicy(descriptor, policy_out);
}

void FakeSessionManagerClient::RetrievePolicy(
    const login_manager::PolicyDescriptor& descriptor,
    RetrievePolicyCallback callback) {
  // Simulate load error.
  if (force_retrieve_policy_load_error_) {
    enterprise_management::PolicyFetchResponse empty;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), RetrievePolicyResponseType::SUCCESS,
                       empty.SerializeAsString()));
    return;
  }

  if (policy_storage_ == PolicyStorageType::kOnDisk) {
    base::FilePath policy_path =
        GetStubPolicyFilePath(descriptor, nullptr /* key_path */);
    DCHECK(!policy_path.empty());

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&GetFileContent, policy_path),
        base::BindOnce(std::move(callback),
                       RetrievePolicyResponseType::SUCCESS));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), RetrievePolicyResponseType::SUCCESS,
                       policy_[GetMemoryStorageKey(descriptor)]));
  }
}

RetrievePolicyResponseType FakeSessionManagerClient::BlockingRetrievePolicy(
    const login_manager::PolicyDescriptor& descriptor,
    std::string* policy_out) {
  if (policy_storage_ == PolicyStorageType::kOnDisk) {
    base::FilePath policy_path =
        GetStubPolicyFilePath(descriptor, nullptr /* key_path */);
    DCHECK(!policy_path.empty());
    *policy_out = GetFileContent(policy_path);
  } else {
    *policy_out = policy_[GetMemoryStorageKey(descriptor)];
  }
  return RetrievePolicyResponseType::SUCCESS;
}

void FakeSessionManagerClient::StoreDevicePolicy(
    const std::string& policy_blob,
    VoidDBusMethodCallback callback) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
  StorePolicy(descriptor, policy_blob, std::move(callback));
}

void FakeSessionManagerClient::StorePolicyForUser(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& policy_blob,
    VoidDBusMethodCallback callback) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_USER, cryptohome_id.account_id());
  StorePolicy(descriptor, policy_blob, std::move(callback));
}

void FakeSessionManagerClient::StoreDeviceLocalAccountPolicy(
    const std::string& account_id,
    const std::string& policy_blob,
    VoidDBusMethodCallback callback) {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_id);
  StorePolicy(descriptor, policy_blob, std::move(callback));
}

void FakeSessionManagerClient::StorePolicy(
    const login_manager::PolicyDescriptor& descriptor,
    const std::string& policy_blob,
    VoidDBusMethodCallback callback) {
  // Decode the blob to get the new key.
  enterprise_management::PolicyFetchResponse response;
  if (!response.ParseFromString(policy_blob)) {
    PostReply(FROM_HERE, std::move(callback), false /* success */);
    return;
  }

  // Simulate failure.
  if (force_store_policy_failure_) {
    PostReply(FROM_HERE, std::move(callback), false /* success */);
    return;
  }

  if (policy_storage_ == PolicyStorageType::kOnDisk) {
    // Store policy and maybe key in files (background threads) and call
    // callback in main thread.
    base::FilePath key_path;
    base::FilePath policy_path = GetStubPolicyFilePath(descriptor, &key_path);
    DCHECK(!policy_path.empty());
    DCHECK(!key_path.empty());

    std::map<base::FilePath, std::string> files_to_store;
    files_to_store[policy_path] = policy_blob;
    if (response.has_new_public_key())
      files_to_store[key_path] = response.new_public_key();

    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(StoreFiles, std::move(files_to_store)),
        base::BindOnce(std::move(callback), true /* success */));
  } else {
    policy_[GetMemoryStorageKey(descriptor)] = policy_blob;

    if (IsChromeDevicePolicy(descriptor)) {
      if (response.has_new_public_key()) {
        base::FilePath key_path;
        GetStubPolicyFilePath(descriptor, &key_path);
        DCHECK(!key_path.empty());

        base::ThreadPool::PostTaskAndReply(
            FROM_HERE,
            {base::MayBlock(),
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
            base::BindOnce(StoreFiles,
                           std::map<base::FilePath, std::string>{
                               {key_path, response.new_public_key()}}),
            base::BindOnce(
                &FakeSessionManagerClient::HandleOwnerKeySet,
                weak_ptr_factory_.GetWeakPtr(),
                base::BindOnce(std::move(callback), true /*success*/)));
      }
      for (auto& observer : observers_)
        observer.PropertyChangeComplete(true /* success */);
    }

    // Run the callback if it hasn't been passed to
    // PostTaskAndReply(), in which case it will be run after the
    // owner key file was stored to disk.
    if (callback) {
      PostReply(FROM_HERE, std::move(callback), true /* success */);
    }
  }
}

bool FakeSessionManagerClient::SupportsBrowserRestart() const {
  return supports_browser_restart_;
}

void FakeSessionManagerClient::SetFlagsForUser(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::vector<std::string>& flags) {
  flags_for_user_[cryptohome_id] = flags;
}

void FakeSessionManagerClient::GetServerBackedStateKeys(
    StateKeysCallback callback) {
  if (force_state_keys_missing_) {
    PostReply(FROM_HERE, std::move(callback), std::vector<std::string>());
    return;
  }

  if (policy_storage_ == PolicyStorageType::kOnDisk) {
    base::FilePath owner_key_path;
    CHECK(base::PathService::Get(dbus_paths::FILE_OWNER_KEY, &owner_key_path));
    const base::FilePath state_keys_path =
        owner_key_path.DirName().AppendASCII(kStubStateKeysFileName);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&ReadCreateStateKeysStub, state_keys_path),
        std::move(callback));
  } else {
    PostReply(FROM_HERE, std::move(callback), server_backed_state_keys_);
  }
}

void FakeSessionManagerClient::StartArcMiniContainer(
    const login_manager::StartArcMiniContainerRequest& request,
    VoidDBusMethodCallback callback) {
  last_start_arc_mini_container_request_ = request;

  if (!arc_available_) {
    PostReply(FROM_HERE, std::move(callback), false);
    return;
  }
  // This is starting a new container.
  container_running_ = true;
  PostReply(FROM_HERE, std::move(callback), true);
}

void FakeSessionManagerClient::UpgradeArcContainer(
    const login_manager::UpgradeArcContainerRequest& request,
    VoidDBusMethodCallback callback) {
  last_upgrade_arc_request_ = request;

  PostReply(FROM_HERE, std::move(callback), !force_upgrade_failure_);
  if (force_upgrade_failure_) {
    // Emulate ArcInstanceStopped signal propagation.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSessionManagerClient::NotifyArcInstanceStopped,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void FakeSessionManagerClient::StopArcInstance(
    const std::string& account_id,
    bool should_backup_log,
    VoidDBusMethodCallback callback) {
  if (!arc_available_ || !container_running_) {
    PostReply(FROM_HERE, std::move(callback), false /* result */);
    return;
  }

  PostReply(FROM_HERE, std::move(callback), true /* result */);
  // Emulate ArcInstanceStopped signal propagation.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSessionManagerClient::NotifyArcInstanceStopped,
                     weak_ptr_factory_.GetWeakPtr()));

  container_running_ = false;
}

void FakeSessionManagerClient::SetArcCpuRestriction(
    login_manager::ContainerCpuRestrictionState restriction_state,
    VoidDBusMethodCallback callback) {
  PostReply(FROM_HERE, std::move(callback), arc_available_);
}

void FakeSessionManagerClient::EmitArcBooted(
    const cryptohome::AccountIdentifier& cryptohome_id,
    VoidDBusMethodCallback callback) {
  PostReply(FROM_HERE, std::move(callback), arc_available_);
}

void FakeSessionManagerClient::GetArcStartTime(
    DBusMethodCallback<base::TimeTicks> callback) {
  PostReply(
      FROM_HERE, std::move(callback),
      arc_available_ ? base::make_optional(arc_start_time_) : base::nullopt);
}

void FakeSessionManagerClient::EnableAdbSideload(
    EnableAdbSideloadCallback callback) {}

void FakeSessionManagerClient::QueryAdbSideload(
    QueryAdbSideloadCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     adb_sideload_enabled_ ? AdbSideloadResponseCode::SUCCESS
                                           : AdbSideloadResponseCode::FAILED,
                     adb_sideload_enabled_));
}

void FakeSessionManagerClient::NotifyArcInstanceStopped() {
  for (auto& observer : observers_)
    observer.ArcInstanceStopped();
}

bool FakeSessionManagerClient::GetFlagsForUser(
    const cryptohome::AccountIdentifier& cryptohome_id,
    std::vector<std::string>* out_flags_for_user) const {
  auto iter = flags_for_user_.find(cryptohome_id);
  if (iter == flags_for_user_.end())
    return false;

  *out_flags_for_user = iter->second;
  return true;
}

const std::string& FakeSessionManagerClient::device_policy() const {
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
  DCHECK(policy_storage_ == PolicyStorageType::kInMemory);
  auto it = policy_.find(GetMemoryStorageKey(descriptor));
  return it != policy_.end() ? it->second : base::EmptyString();
}

void FakeSessionManagerClient::set_device_policy(
    const std::string& policy_blob) {
  DCHECK(policy_storage_ == PolicyStorageType::kInMemory);
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
  policy_[GetMemoryStorageKey(descriptor)] = policy_blob;
}

const std::string& FakeSessionManagerClient::user_policy(
    const cryptohome::AccountIdentifier& cryptohome_id) const {
  DCHECK(policy_storage_ == PolicyStorageType::kInMemory);
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_USER, cryptohome_id.account_id());
  auto it = policy_.find(GetMemoryStorageKey(descriptor));
  return it != policy_.end() ? it->second : base::EmptyString();
}

void FakeSessionManagerClient::set_user_policy(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& policy_blob) {
  DCHECK(policy_storage_ == PolicyStorageType::kInMemory);
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_USER, cryptohome_id.account_id());
  policy_[GetMemoryStorageKey(descriptor)] = policy_blob;
}

void FakeSessionManagerClient::set_user_policy_without_session(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& policy_blob) {
  DCHECK(policy_storage_ == PolicyStorageType::kInMemory);
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_SESSIONLESS_USER, cryptohome_id.account_id());
  policy_[GetMemoryStorageKey(descriptor)] = policy_blob;
}

const std::string& FakeSessionManagerClient::device_local_account_policy(
    const std::string& account_id) const {
  DCHECK(policy_storage_ == PolicyStorageType::kInMemory);
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_id);
  auto it = policy_.find(GetMemoryStorageKey(descriptor));
  return it != policy_.end() ? it->second : base::EmptyString();
}

void FakeSessionManagerClient::set_device_local_account_policy(
    const std::string& account_id,
    const std::string& policy_blob) {
  DCHECK(policy_storage_ == PolicyStorageType::kInMemory);
  login_manager::PolicyDescriptor descriptor = MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_id);
  policy_[GetMemoryStorageKey(descriptor)] = policy_blob;
}

void FakeSessionManagerClient::OnPropertyChangeComplete(bool success) {
  for (auto& observer : observers_)
    observer.PropertyChangeComplete(success);
}

void FakeSessionManagerClient::HandleOwnerKeySet(
    base::OnceClosure callback_to_run) {
  for (auto& observer : observers_)
    observer.OwnerKeySet(true /* success */);

  std::move(callback_to_run).Run();
}

void FakeSessionManagerClient::set_on_start_device_wipe_callback(
    base::OnceClosure callback) {
  on_start_device_wipe_callback_ = std::move(callback);
}

}  // namespace chromeos

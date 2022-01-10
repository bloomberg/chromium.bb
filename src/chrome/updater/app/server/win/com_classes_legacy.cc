// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes_legacy.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/updater/app/server/win/server.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"

namespace {

HRESULT OpenCallerProcessHandle(DWORD proc_id,
                                base::win::ScopedHandle& proc_handle) {
  proc_handle.Set(::OpenProcess(PROCESS_DUP_HANDLE, false, proc_id));
  return proc_handle.IsValid() ? S_OK : updater::HRESULTFromLastError();
}

std::wstring GetCommandToLaunch(const WCHAR* app_guid, const WCHAR* cmd_id) {
  if (!app_guid || !cmd_id)
    return std::wstring();

  base::win::RegKey key(HKEY_LOCAL_MACHINE, CLIENTS_KEY,
                        updater::Wow6432(KEY_READ));
  if (key.OpenKey(app_guid, updater::Wow6432(KEY_READ)) != ERROR_SUCCESS)
    return std::wstring();

  std::wstring cmd_line;
  key.ReadValue(cmd_id, &cmd_line);
  return cmd_line;
}

HRESULT LaunchCmd(const std::wstring& cmd,
                  const base::win::ScopedHandle& caller_proc_handle,
                  ULONG_PTR* proc_handle) {
  if (cmd.empty() || !caller_proc_handle.IsValid() || !proc_handle)
    return E_INVALIDARG;

  *proc_handle = NULL;

  STARTUPINFOW startup_info = {sizeof(startup_info)};
  PROCESS_INFORMATION process_information = {0};
  std::wstring cmd_line(cmd);
  if (!::CreateProcess(nullptr, &cmd_line[0], nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup_info,
                       &process_information)) {
    return updater::HRESULTFromLastError();
  }

  base::win::ScopedProcessInformation pi(process_information);
  DCHECK(pi.IsValid());

  HANDLE duplicate_proc_handle = NULL;

  bool res = ::DuplicateHandle(
                 ::GetCurrentProcess(),     // Current process.
                 pi.process_handle(),       // Process handle to duplicate.
                 caller_proc_handle.Get(),  // Process receiving the handle.
                 &duplicate_proc_handle,    // Duplicated handle.
                 PROCESS_QUERY_INFORMATION |
                     SYNCHRONIZE,  // Access requested for the new handle.
                 FALSE,            // Don't inherit the new handle.
                 0) != 0;          // Flags.
  if (!res) {
    HRESULT hr = updater::HRESULTFromLastError();
    VLOG(1) << "Failed to duplicate the handle " << hr;
    return hr;
  }

  // The caller must close this handle.
  *proc_handle = reinterpret_cast<ULONG_PTR>(duplicate_proc_handle);
  return S_OK;
}

}  // namespace

namespace updater {

LegacyOnDemandImpl::LegacyOnDemandImpl()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives()})) {}

LegacyOnDemandImpl::~LegacyOnDemandImpl() = default;

STDMETHODIMP LegacyOnDemandImpl::createAppBundleWeb(
    IDispatch** app_bundle_web) {
  DCHECK(app_bundle_web);
  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle(this);
  *app_bundle_web = app_bundle.Detach();
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::createApp(BSTR app_id,
                                           BSTR brand_code,
                                           BSTR language,
                                           BSTR ap) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::createInstalledApp(BSTR app_id) {
  set_app_id(base::WideToASCII(app_id));
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::createAllInstalledApps() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_displayLanguage(BSTR* language) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::put_displayLanguage(BSTR language) {
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::put_parentHWND(ULONG_PTR hwnd) {
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_length(int* number) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_appWeb(int index, IDispatch** app_web) {
  DCHECK_EQ(index, 0);
  DCHECK(app_web);

  Microsoft::WRL::ComPtr<IAppWeb> app(this);
  *app_web = app.Detach();
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::initialize() {
  return S_OK;
}

// Invokes the in-process update service on the main sequence. Forwards the
// callbacks to a sequenced task runner. |obj| is bound to this object.
STDMETHODIMP LegacyOnDemandImpl::checkForUpdate() {
  using LegacyOnDemandImplPtr = Microsoft::WRL::ComPtr<LegacyOnDemandImpl>;
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             LegacyOnDemandImplPtr obj) {
            update_service->Update(
                obj->app_id(), UpdateService::Priority::kForeground,
                UpdateService::PolicySameVersionUpdate::kNotAllowed,
                base::BindRepeating(
                    [](LegacyOnDemandImplPtr obj,
                       const UpdateService::UpdateState& state_update) {
                      obj->task_runner_->PostTask(
                          FROM_HERE,
                          base::BindOnce(
                              &LegacyOnDemandImpl::UpdateStateCallback, obj,
                              state_update));
                    },
                    obj),
                base::BindOnce(
                    [](LegacyOnDemandImplPtr obj,
                       UpdateService::Result result) {
                      obj->task_runner_->PostTask(
                          FROM_HERE,
                          base::BindOnce(
                              &LegacyOnDemandImpl::UpdateResultCallback, obj,
                              result));
                    },
                    obj));
          },
          com_server->update_service(), LegacyOnDemandImplPtr(this)));
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::download() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::install() {
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::pause() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::resume() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::cancel() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::downloadPackage(BSTR app_id,
                                                 BSTR package_name) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_currentState(VARIANT* current_state) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_appId(BSTR* app_id) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_currentVersionWeb(IDispatch** current) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_nextVersionWeb(IDispatch** next) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_command(BSTR command_id,
                                             IDispatch** command) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_currentState(IDispatch** current_state) {
  DCHECK(current_state);
  Microsoft::WRL::ComPtr<ICurrentState> state(this);
  *current_state = state.Detach();
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::launch() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::uninstall() {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_serverInstallDataIndex(BSTR* language) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::put_serverInstallDataIndex(BSTR language) {
  return E_NOTIMPL;
}

// Returns the state of update as seen by the on-demand client:
// - if the repeading callback has been received: returns the specific state.
// - if the completion callback has been received, but no repeating callback,
//   then it returns STATE_ERROR. This is an error state and it indicates that
//   update is not going to be further handled and repeating callbacks posted.
// - if no callback has been received at all: returns STATE_INIT.
STDMETHODIMP LegacyOnDemandImpl::get_stateValue(LONG* state_value) {
  DCHECK(state_value);
  base::AutoLock lock{lock_};
  if (state_update_) {
    switch (state_update_.value().state) {
      case UpdateService::UpdateState::State::kUnknown:  // Fall through.
      case UpdateService::UpdateState::State::kNotStarted:
        *state_value = STATE_INIT;
        break;
      case UpdateService::UpdateState::State::kCheckingForUpdates:
        *state_value = STATE_CHECKING_FOR_UPDATE;
        break;
      case UpdateService::UpdateState::State::kUpdateAvailable:
        *state_value = STATE_UPDATE_AVAILABLE;
        break;
      case UpdateService::UpdateState::State::kDownloading:
        *state_value = STATE_DOWNLOADING;
        break;
      case UpdateService::UpdateState::State::kInstalling:
        *state_value = STATE_INSTALLING;
        break;
      case UpdateService::UpdateState::State::kUpdated:
        *state_value = STATE_INSTALL_COMPLETE;
        break;
      case UpdateService::UpdateState::State::kNoUpdate:
        *state_value = STATE_NO_UPDATE;
        break;
      case UpdateService::UpdateState::State::kUpdateError:
        *state_value = STATE_ERROR;
        break;
    }
  } else if (result_) {
    DCHECK_NE(result_.value(), UpdateService::Result::kSuccess);
    *state_value = STATE_ERROR;
  } else {
    *state_value = STATE_INIT;
  }

  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_availableVersion(BSTR* available_version) {
  base::AutoLock lock{lock_};
  if (state_update_) {
    *available_version =
        base::win::ScopedBstr(
            base::UTF8ToWide(state_update_->next_version.GetString()))
            .Release();
  }
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_bytesDownloaded(ULONG* bytes_downloaded) {
  DCHECK(bytes_downloaded);
  base::AutoLock lock{lock_};
  if (!state_update_ || state_update_->downloaded_bytes == -1)
    return E_FAIL;
  *bytes_downloaded = state_update_->downloaded_bytes;
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_totalBytesToDownload(
    ULONG* total_bytes_to_download) {
  DCHECK(total_bytes_to_download);
  base::AutoLock lock{lock_};
  if (!state_update_ || state_update_->total_bytes == -1)
    return E_FAIL;
  *total_bytes_to_download = state_update_->total_bytes;
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_downloadTimeRemainingMs(
    LONG* download_time_remaining_ms) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_nextRetryTime(ULONGLONG* next_retry_time) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_installProgress(
    LONG* install_progress_percentage) {
  DCHECK(install_progress_percentage);
  base::AutoLock lock{lock_};
  if (!state_update_ || state_update_->install_progress == -1)
    return E_FAIL;
  *install_progress_percentage = state_update_->install_progress;
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_installTimeRemainingMs(
    LONG* install_time_remaining_ms) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_isCanceled(VARIANT_BOOL* is_canceled) {
  return E_NOTIMPL;
}

// In the error case, if an installer error occurred, it remaps the installer
// error to the legacy installer error value, for backward compatibility.
STDMETHODIMP LegacyOnDemandImpl::get_errorCode(LONG* error_code) {
  DCHECK(error_code);
  base::AutoLock lock{lock_};
  if (state_update_ &&
      state_update_->state == UpdateService::UpdateState::State::kUpdateError) {
    *error_code = state_update_->error_code == kErrorApplicationInstallerFailed
                      ? GOOPDATEINSTALL_E_INSTALLER_FAILED
                      : state_update_->error_code;
  } else if (result_) {
    *error_code = (result_.value() == UpdateService::Result::kSuccess) ? 0 : -1;
  } else {
    *error_code = 0;
  }
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_extraCode1(LONG* extra_code1) {
  DCHECK(extra_code1);
  base::AutoLock lock{lock_};
  if (state_update_ &&
      state_update_->state == UpdateService::UpdateState::State::kUpdateError) {
    *extra_code1 = state_update_->extra_code1;
  } else {
    *extra_code1 = 0;
  }
  return S_OK;
}

// Returns an installer error completion message.
STDMETHODIMP LegacyOnDemandImpl::get_completionMessage(
    BSTR* completion_message) {
  DCHECK(completion_message);
  base::AutoLock lock{lock_};
  if (state_update_ &&
      state_update_->error_code == kErrorApplicationInstallerFailed) {
    // TODO(1095133): this string needs localization.
    *completion_message = base::win::ScopedBstr(L"Installer failed.").Release();
  } else {
    completion_message = nullptr;
  }
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_installerResultCode(
    LONG* installer_result_code) {
  DCHECK(installer_result_code);
  base::AutoLock lock{lock_};
  if (state_update_ &&
      state_update_->error_code == kErrorApplicationInstallerFailed) {
    *installer_result_code = state_update_->extra_code1;
  } else {
    *installer_result_code = 0;
  }
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::get_installerResultExtraCode1(
    LONG* installer_result_extra_code1) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_postInstallLaunchCommandLine(
    BSTR* post_install_launch_command_line) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_postInstallUrl(BSTR* post_install_url) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_postInstallAction(
    LONG* post_install_action) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::GetTypeInfoCount(UINT*) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::GetTypeInfo(UINT, LCID, ITypeInfo**) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::GetIDsOfNames(REFIID,
                                               LPOLESTR*,
                                               UINT,
                                               LCID,
                                               DISPID*) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::Invoke(DISPID,
                                        REFIID,
                                        LCID,
                                        WORD,
                                        DISPPARAMS*,
                                        VARIANT*,
                                        EXCEPINFO*,
                                        UINT*) {
  return E_NOTIMPL;
}

void LegacyOnDemandImpl::UpdateStateCallback(
    UpdateService::UpdateState state_update) {
  base::AutoLock lock{lock_};
  state_update_ = state_update;
}

void LegacyOnDemandImpl::UpdateResultCallback(UpdateService::Result result) {
  base::AutoLock lock{lock_};
  result_ = result;
}

LegacyProcessLauncherImpl::LegacyProcessLauncherImpl() = default;
LegacyProcessLauncherImpl::~LegacyProcessLauncherImpl() = default;

STDMETHODIMP LegacyProcessLauncherImpl::LaunchCmdLine(const WCHAR* cmd_line) {
  return LaunchCmdLineEx(cmd_line, nullptr, nullptr, nullptr);
}

STDMETHODIMP LegacyProcessLauncherImpl::LaunchBrowser(DWORD browser_type,
                                                      const WCHAR* url) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyProcessLauncherImpl::LaunchCmdElevated(
    const WCHAR* app_guid,
    const WCHAR* cmd_id,
    DWORD caller_proc_id,
    ULONG_PTR* proc_handle) {
  VLOG(2) << "LegacyProcessLauncherImpl::LaunchCmdElevated: app " << app_guid
          << ", cmd_id " << cmd_id << ", pid " << caller_proc_id;

  if (!cmd_id || !wcslen(cmd_id) || !proc_handle) {
    VLOG(1) << "Invalid arguments";
    return E_INVALIDARG;
  }

  base::win::ScopedHandle caller_proc_handle;
  HRESULT hr = OpenCallerProcessHandle(caller_proc_id, caller_proc_handle);
  if (FAILED(hr)) {
    VLOG(1) << "failed to open caller's handle " << hr;
    return hr;
  }

  std::wstring cmd = GetCommandToLaunch(app_guid, cmd_id);
  if (cmd.empty()) {
    VLOG(1) << "cmd not found";
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  VLOG(2) << "[LegacyProcessLauncherImpl::LaunchCmdElevated][cmd " << cmd
          << "]";
  return LaunchCmd(cmd, caller_proc_handle, proc_handle);
}

STDMETHODIMP LegacyProcessLauncherImpl::LaunchCmdLineEx(
    const WCHAR* cmd_line,
    DWORD* server_proc_id,
    ULONG_PTR* proc_handle,
    ULONG_PTR* stdout_handle) {
  return E_NOTIMPL;
}

}  // namespace updater

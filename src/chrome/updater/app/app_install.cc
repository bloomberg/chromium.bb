// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

#if !BUILDFLAG(IS_WIN)
namespace {

class SplashScreenImpl : public SplashScreen {
 public:
  // Overrides for SplashScreen.
  void Show() override {}
  void Dismiss(base::OnceClosure callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
  }
};

class AppInstallControllerImpl : public AppInstallController {
 public:
  explicit AppInstallControllerImpl(
      scoped_refptr<UpdateService> /*update_service*/) {}
  // Override for AppInstallController.
  void InstallApp(const std::string& app_id,
                  base::OnceCallback<void(int)> callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0));
  }

 protected:
  ~AppInstallControllerImpl() override = default;
};

}  // namespace

scoped_refptr<App> MakeAppInstall() {
  return base::MakeRefCounted<AppInstall>(
      base::BindRepeating([]() -> std::unique_ptr<SplashScreen> {
        return std::make_unique<SplashScreenImpl>();
      }),
      base::BindRepeating([](scoped_refptr<UpdateService> update_service)
                              -> scoped_refptr<AppInstallController> {
        return base::MakeRefCounted<AppInstallControllerImpl>(update_service);
      }));
}
#endif  // !BUILDFLAG(IS_WIN)

AppInstall::AppInstall(SplashScreen::Maker splash_screen_maker,
                       AppInstallController::Maker app_install_controller_maker)
    : splash_screen_maker_(std::move(splash_screen_maker)),
      app_install_controller_maker_(app_install_controller_maker) {
  DCHECK(splash_screen_maker_);
  DCHECK(app_install_controller_maker_);
}

AppInstall::~AppInstall() = default;

void AppInstall::Initialize() {
  base::i18n::InitializeICU();
}

void AppInstall::Uninitialize() {
  if (update_service_) {
    update_service_->Uninitialize();
  }
}

void AppInstall::FirstTaskRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  splash_screen_ = splash_screen_maker_.Run();
  splash_screen_->Show();

  // Creating instances of `UpdateServiceProxy` is possible only after task
  // scheduling has been initialized.
  update_service_ = CreateUpdateServiceProxy(updater_scope());
  update_service_->GetVersion(
      base::BindOnce(&AppInstall::GetVersionDone, this));
}

void AppInstall::GetVersionDone(const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG_IF(1, (version.IsValid()))
      << "Found active version: " << version.GetString();
  if (version.IsValid() && version >= base::Version(kUpdaterVersion)) {
    splash_screen_->Dismiss(base::BindOnce(&AppInstall::MaybeInstallApp, this));
    return;
  }

  InstallCandidate(
      updater_scope(),
      base::BindOnce(
          [](SplashScreen* splash_screen, base::OnceCallback<void(int)> done,
             int result) {
            splash_screen->Dismiss(base::BindOnce(std::move(done), result));
          },
          splash_screen_.get(),
          base::BindOnce(&AppInstall::InstallCandidateDone, this,
                         version.IsValid())));
}

void AppInstall::InstallCandidateDone(bool valid_version, int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != 0) {
    Shutdown(result);
    return;
  }

  if (valid_version) {
    WakeCandidateDone();
    return;
  }

  WakeCandidate();
}

void AppInstall::WakeCandidate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invoke UpdateServiceInternal::InitializeUpdateService to wake this version
  // of the updater, qualify, and possibly promote this version as a result. The
  // |UpdateServiceInternal| instance has sequence affinity. Bind it in the
  // closure to ensure it is released in this sequence.
  scoped_refptr<UpdateServiceInternal> update_service_internal =
      CreateUpdateServiceInternalProxy(updater_scope());
  update_service_internal->InitializeUpdateService(base::BindOnce(
      [](scoped_refptr<UpdateServiceInternal> /*update_service_internal*/,
         scoped_refptr<AppInstall> app_install) {
        app_install->WakeCandidateDone();
      },
      update_service_internal, base::WrapRefCounted(this)));
}

#if BUILDFLAG(IS_LINUX)
// TODO(crbug.com/1276114) - implement.
void AppInstall::WakeCandidateDone() {
  NOTIMPLEMENTED();
}
#endif  // BUILDFLAG(IS_LINUX)

void AppInstall::RegisterUpdater() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/1297163) - encapsulate the reinitialization of the
  // proxy server instance to avoid this special case.
  update_service_ = CreateUpdateServiceProxy(updater_scope());
#endif

  RegistrationRequest request;
  request.app_id = kUpdaterAppId;
  request.version = base::Version(kUpdaterVersion);
  update_service_->RegisterApp(
      request,
      base::BindOnce(
          [](scoped_refptr<AppInstall> app_install,
             const RegistrationResponse& registration_response) {
            if (registration_response.status_code != kRegistrationSuccess &&
                registration_response.status_code !=
                    kRegistrationAlreadyRegistered) {
              VLOG(2) << "Updater registration failed: "
                      << registration_response.status_code;
              app_install->Shutdown(kErrorRegistrationFailed);
              return;
            }
            app_install->MaybeInstallApp();
          },
          base::WrapRefCounted(this)));
}

void AppInstall::MaybeInstallApp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string app_id = []() {
    absl::optional<tagging::TagArgs> tag_args = GetTagArgs();
    if (tag_args && !tag_args->apps.empty()) {
      // TODO(crbug.com/1128631): support bundles. For now, assume one app.
      DCHECK_EQ(tag_args->apps.size(), size_t{1});
      const std::string& app_id = tag_args->apps.front().app_id;
      if (!app_id.empty()) {
        return app_id;
      }
    }
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        kAppIdSwitch);
  }();

  if (app_id.empty()) {
    Shutdown(kErrorOk);
    return;
  }
  app_install_controller_ = app_install_controller_maker_.Run(update_service_);
  app_install_controller_->InstallApp(
      app_id, base::BindOnce(&AppInstall::Shutdown, this));
}

}  // namespace updater

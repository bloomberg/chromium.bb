// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {
namespace diagnostics {

namespace {

DiagnosticsLogController* g_instance = nullptr;

// Default path for storing logs.
const char kDiaganosticsTmpDir[] = "/tmp/diagnostics";
const char kDiaganosticsDirName[] = "diagnostics";

// Session log headers and fallback content.
const char kRoutineLogSubsectionHeader[] = "--- Test Routines --- \n";
const char kSystemLogSectionHeader[] = "=== System === \n";
const char kNetworkingLogSectionHeader[] = "=== Networking === \n";
const char kNoRoutinesRun[] =
    "No routines of this type were run in the session.\n";

std::string GetRoutineResultsString(const std::string& results) {
  const std::string section_header =
      std::string(kRoutineLogSubsectionHeader) + "\n";
  if (results.empty()) {
    return section_header + kNoRoutinesRun;
  }

  return section_header + results;
}
// Determines if profile should be accessed with current session state.  If at
// sign-in screen, guest user, kiosk app, or before the profile has
// successfully loaded temporary path should be used for storing logs.
bool ShouldUseActiveUserProfileDir(session_manager::SessionState state,
                                   LoginStatus status) {
  return state == session_manager::SessionState::ACTIVE &&
         status == ash::LoginStatus::USER;
}

}  // namespace

DiagnosticsLogController::DiagnosticsLogController()
    : log_base_path_(kDiaganosticsTmpDir) {
  DCHECK_EQ(nullptr, g_instance);
  ash::Shell::Get()->session_controller()->AddObserver(this);
  g_instance = this;
}

DiagnosticsLogController::~DiagnosticsLogController() {
  DCHECK_EQ(this, g_instance);
  ash::Shell::Get()->session_controller()->RemoveObserver(this);
  g_instance = nullptr;
}

// static
DiagnosticsLogController* DiagnosticsLogController::Get() {
  return g_instance;
}

// static
bool DiagnosticsLogController::IsInitialized() {
  return g_instance && g_instance->delegate_;
}

// static
void DiagnosticsLogController::Initialize(
    std::unique_ptr<DiagnosticsBrowserDelegate> delegate) {
  DCHECK(g_instance);
  DCHECK_CALLED_ON_VALID_SEQUENCE(g_instance->sequence_checker_);
  g_instance->delegate_ = std::move(delegate);
  g_instance->ResetAndInitializeLogWriters();

  // Schedule removal of log directory.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&DiagnosticsLogController::RemoveDirectory,
                     g_instance->weak_ptr_factory_.GetWeakPtr(),
                     g_instance->log_base_path_));
}

bool DiagnosticsLogController::GenerateSessionLogOnBlockingPool(
    const base::FilePath& save_file_path) {
  DCHECK(!save_file_path.empty());
  std::vector<std::string> log_pieces;

  // Fetch system data from TelemetryLog.
  const std::string system_log_contents = telemetry_log_->GetContents();
  log_pieces.push_back(kSystemLogSectionHeader);
  if (!system_log_contents.empty()) {
    log_pieces.push_back(system_log_contents);
  }

  // Fetch system routines from RoutineLog.
  const std::string system_routines = routine_log_->GetContentsForCategory(
      RoutineLog::RoutineCategory::kSystem);
  // Add the routine section for the system category.
  log_pieces.push_back(GetRoutineResultsString(system_routines));

  if (features::IsNetworkingInDiagnosticsAppEnabled()) {
    // Add networking category.
    log_pieces.push_back(kNetworkingLogSectionHeader);

    // Add the network info section.
    log_pieces.push_back(networking_log_->GetNetworkInfo());

    // Add the routine section for the network category.
    const std::string network_routines = routine_log_->GetContentsForCategory(
        RoutineLog::RoutineCategory::kNetwork);
    log_pieces.push_back(GetRoutineResultsString(network_routines));

    // Add the network events section.
    log_pieces.push_back(networking_log_->GetNetworkEvents());
  }

  return base::WriteFile(save_file_path, base::JoinString(log_pieces, "\n"));
}

void DiagnosticsLogController::ResetAndInitializeLogWriters() {
  if (!DiagnosticsLogController::IsInitialized()) {
    return;
  }

  ResetLogBasePath();

  networking_log_ = std::make_unique<NetworkingLog>(log_base_path_);
  routine_log_ = std::make_unique<RoutineLog>(log_base_path_);
  telemetry_log_ = std::make_unique<TelemetryLog>();
}

NetworkingLog* DiagnosticsLogController::GetNetworkingLog() {
  return networking_log_.get();
}

RoutineLog* DiagnosticsLogController::GetRoutineLog() {
  return routine_log_.get();
}

TelemetryLog* DiagnosticsLogController::GetTelemetryLog() {
  return telemetry_log_.get();
}

void DiagnosticsLogController::ResetLogBasePath() {
  const session_manager::SessionState state =
      ash::Shell::Get()->session_controller()->GetSessionState();
  const LoginStatus status =
      ash::Shell::Get()->session_controller()->login_status();

  // Check if there is an active user and profile is ready based on session and
  // login state.
  if (ShouldUseActiveUserProfileDir(state, status)) {
    base::FilePath user_dir = g_instance->delegate_->GetActiveUserProfileDir();

    // Update |log_base_path_| when path is non-empty. Otherwise fallback to
    // |kDiaganosticsTmpDir|.
    if (!user_dir.empty()) {
      g_instance->log_base_path_ = user_dir.Append(kDiaganosticsDirName);
      return;
    }
  }

  // Use diagnostics temporary path for Guest, KioskApp, and no user states.
  g_instance->log_base_path_ = base::FilePath(kDiaganosticsTmpDir);
}

void DiagnosticsLogController::OnLoginStatusChanged(LoginStatus login_status) {
  if (!DiagnosticsLogController::IsInitialized()) {
    return;
  }

  g_instance->ResetAndInitializeLogWriters();
}

void DiagnosticsLogController::RemoveDirectory(const base::FilePath& path) {
  DCHECK(!path.empty());

  if (base::PathExists(path)) {
    base::DeletePathRecursively(path);
  }
}

}  // namespace diagnostics
}  // namespace ash

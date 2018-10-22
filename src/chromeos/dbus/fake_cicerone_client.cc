// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cicerone_client.h"

#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeCiceroneClient::FakeCiceroneClient() {
  launch_container_application_response_.Clear();
  launch_container_application_response_.set_success(true);

  container_app_icon_response_.Clear();

  install_linux_package_response_.Clear();
  install_linux_package_response_.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
}
FakeCiceroneClient::~FakeCiceroneClient() = default;

void FakeCiceroneClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCiceroneClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool FakeCiceroneClient::IsContainerStartedSignalConnected() {
  return is_container_started_signal_connected_;
}

bool FakeCiceroneClient::IsContainerShutdownSignalConnected() {
  return is_container_shutdown_signal_connected_;
}

bool FakeCiceroneClient::IsLxdContainerCreatedSignalConnected() {
  return is_lxd_container_created_signal_connected_;
}

bool FakeCiceroneClient::IsLxdContainerDownloadingSignalConnected() {
  return is_lxd_container_downloading_signal_connected_;
}

bool FakeCiceroneClient::IsTremplinStartedSignalConnected() {
  return is_tremplin_started_signal_connected_;
}

bool FakeCiceroneClient::IsInstallLinuxPackageProgressSignalConnected() {
  return is_install_linux_package_progress_signal_connected_;
}

void FakeCiceroneClient::LaunchContainerApplication(
    const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
    DBusMethodCallback<vm_tools::cicerone::LaunchContainerApplicationResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                launch_container_application_response_));
}

void FakeCiceroneClient::GetContainerAppIcons(
    const vm_tools::cicerone::ContainerAppIconRequest& request,
    DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), container_app_icon_response_));
}

void FakeCiceroneClient::InstallLinuxPackage(
    const vm_tools::cicerone::InstallLinuxPackageRequest& request,
    DBusMethodCallback<vm_tools::cicerone::InstallLinuxPackageResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), install_linux_package_response_));
}

void FakeCiceroneClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCiceroneClient::CreateLxdContainer(
    const vm_tools::cicerone::CreateLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::CreateLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), create_lxd_container_response_));
}

void FakeCiceroneClient::StartLxdContainer(
    const vm_tools::cicerone::StartLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::StartLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), start_lxd_container_response_));
}

void FakeCiceroneClient::GetLxdContainerUsername(
    const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
    DBusMethodCallback<vm_tools::cicerone::GetLxdContainerUsernameResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                get_lxd_container_username_response_));
}

void FakeCiceroneClient::SetUpLxdContainerUser(
    const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
    DBusMethodCallback<vm_tools::cicerone::SetUpLxdContainerUserResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), setup_lxd_container_user_response_));
}

}  // namespace chromeos

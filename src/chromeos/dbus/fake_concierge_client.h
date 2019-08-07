// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/concierge_client.h"

namespace chromeos {

// FakeConciergeClient is a light mock of ConciergeClient used for testing.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeConciergeClient
    : public ConciergeClient {
 public:
  FakeConciergeClient();
  ~FakeConciergeClient() override;

  // Adds an observer.
  void AddObserver(Observer* observer) override;

  // Removes an observer if added.
  void RemoveObserver(Observer* observer) override;

  // IsContainerStartupFailedSignalConnected must return true before
  // StartLxdContainer is called.
  bool IsContainerStartupFailedSignalConnected() override;

  // Fake version of the method that creates a disk image for the Termina VM.
  // Sets create_disk_image_called_. |callback| is called after the method
  // call finishes.
  void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback)
      override;

  // Fake version of the method that destroys a Termina VM and removes its disk
  // image. Sets destroy_disk_image_called_. |callback| is called after the
  // method call finishes.
  void DestroyDiskImage(
      const vm_tools::concierge::DestroyDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::DestroyDiskImageResponse>
          callback) override;

  // Fake version of the method that lists Termina VM disks. Sets
  // list_vm_disks_called_. |callback| is called after the method call
  // finishes.
  void ListVmDisks(const vm_tools::concierge::ListVmDisksRequest& request,
                   DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse>
                       callback) override;

  // Fake version of the method that starts a Termina VM. Sets
  // start_termina_vm_called_. |callback| is called after the method call
  // finishes.
  void StartTerminaVm(const vm_tools::concierge::StartVmRequest& request,
                      DBusMethodCallback<vm_tools::concierge::StartVmResponse>
                          callback) override;

  // Fake version of the method that stops the named Termina VM if it is
  // running. Sets stop_vm_called_. |callback| is called after the method
  // call finishes.
  void StopVm(const vm_tools::concierge::StopVmRequest& request,
              DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback)
      override;

  // Fake version of the method that waits for the Concierge service to be
  // availble.  |callback| is called after the method call finishes.
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Fake version of the method that fetches ssh key information.
  // |callback| is called after the method call finishes.
  void GetContainerSshKeys(
      const vm_tools::concierge::ContainerSshKeysRequest& request,
      DBusMethodCallback<vm_tools::concierge::ContainerSshKeysResponse>
          callback) override;

  // Fake version of the method that attaches a USB device to a VM
  // |callback| is called once the method call has finished
  void AttachUsbDevice(base::ScopedFD fd,
      const vm_tools::concierge::AttachUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::AttachUsbDeviceResponse> callback)
      override;

  // Fake version of the method that removes a USB device from a VM it's been
  // attached to
  // |callback| is called once the method call has finished
  void DetachUsbDevice(
      const vm_tools::concierge::DetachUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::DetachUsbDeviceResponse> callback)
      override;

  // Fake version of the method that lists all the USB devices currently
  // attached to a given VM
  // |callback| is called once the method call has finished
  void ListUsbDevices(
      const vm_tools::concierge::ListUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::ListUsbDeviceResponse> callback)
      override;

  // Indicates whether CreateDiskImage has been called
  bool create_disk_image_called() const { return create_disk_image_called_; }
  // Indicates whether DestroyDiskImage has been called
  bool destroy_disk_image_called() const { return destroy_disk_image_called_; }
  // Indicates whether ListVmDisks has been called
  bool list_vm_disks_called() const { return list_vm_disks_called_; }
  // Indicates whether StartTerminaVm has been called
  bool start_termina_vm_called() const { return start_termina_vm_called_; }
  // Indicates whether StopVm has been called
  bool stop_vm_called() const { return stop_vm_called_; }
  // Indicates whether GetContainerSshKeys has been called
  bool get_container_ssh_keys_called() const {
    return get_container_ssh_keys_called_;
  }
  // Indicates whether AttachUsbDevice has been called
  bool attach_usb_device_called() const { return attach_usb_device_called_; }
  // Indicates whether DetachUsbDevice has been called
  bool detach_usb_device_called() const { return detach_usb_device_called_; }
  // Indicates whether ListUsbDevices has been called
  bool list_usb_devices_called() const { return list_usb_devices_called_; }
  // Set ContainerStartupFailedSignalConnected state
  void set_container_startup_failed_signal_connected(bool connected) {
    is_container_startup_failed_signal_connected_ = connected;
  }

  void set_create_disk_image_response(
      const vm_tools::concierge::CreateDiskImageResponse&
          create_disk_image_response) {
    create_disk_image_response_ = create_disk_image_response;
  }
  void set_destroy_disk_image_response(
      const vm_tools::concierge::DestroyDiskImageResponse&
          destroy_disk_image_response) {
    destroy_disk_image_response_ = destroy_disk_image_response;
  }
  void set_list_vm_disks_response(
      const vm_tools::concierge::ListVmDisksResponse& list_vm_disks_response) {
    list_vm_disks_response_ = list_vm_disks_response;
  }
  void set_start_vm_response(
      const vm_tools::concierge::StartVmResponse& start_vm_response) {
    start_vm_response_ = start_vm_response;
  }
  void set_stop_vm_response(
      const vm_tools::concierge::StopVmResponse& stop_vm_response) {
    stop_vm_response_ = stop_vm_response;
  }
  void set_container_ssh_keys_response(
      const vm_tools::concierge::ContainerSshKeysResponse&
          container_ssh_keys_response) {
    container_ssh_keys_response_ = container_ssh_keys_response;
  }
  void set_attach_usb_device_response(
      const vm_tools::concierge::AttachUsbDeviceResponse&
          attach_usb_device_response) {
    attach_usb_device_response_ = attach_usb_device_response;
  }
  void set_detach_usb_device_response(
      const vm_tools::concierge::DetachUsbDeviceResponse&
      detach_usb_device_response) {
    detach_usb_device_response_ = detach_usb_device_response;
  }
  void set_list_usb_devices_response(
      const vm_tools::concierge::ListUsbDeviceResponse&
      list_usb_devices_response) {
    list_usb_devices_response_ = list_usb_devices_response;
  }

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  void InitializeProtoResponses();

  void NotifyTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal);

  bool create_disk_image_called_ = false;
  bool destroy_disk_image_called_ = false;
  bool list_vm_disks_called_ = false;
  bool start_termina_vm_called_ = false;
  bool stop_vm_called_ = false;
  bool get_container_ssh_keys_called_ = false;
  bool attach_usb_device_called_ = false;
  bool detach_usb_device_called_ = false;
  bool list_usb_devices_called_ = false;
  bool is_container_startup_failed_signal_connected_ = true;

  vm_tools::concierge::CreateDiskImageResponse create_disk_image_response_;
  vm_tools::concierge::DestroyDiskImageResponse destroy_disk_image_response_;
  vm_tools::concierge::ListVmDisksResponse list_vm_disks_response_;
  vm_tools::concierge::StartVmResponse start_vm_response_;
  vm_tools::concierge::StopVmResponse stop_vm_response_;
  vm_tools::concierge::ContainerSshKeysResponse container_ssh_keys_response_;
  vm_tools::concierge::AttachUsbDeviceResponse attach_usb_device_response_;
  vm_tools::concierge::DetachUsbDeviceResponse detach_usb_device_response_;
  vm_tools::concierge::ListUsbDeviceResponse list_usb_devices_response_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeConciergeClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeConciergeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_

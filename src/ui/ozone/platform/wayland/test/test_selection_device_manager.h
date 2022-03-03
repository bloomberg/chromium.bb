// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace base {
class SequencedTaskRunner;
}

namespace wl {

class TestSelectionSource;
class TestSelectionDevice;

// Base classes for data device implementations. Protocol specific derived
// classes must bind request handlers and factory methods for data device and
// source instances. E.g: Standard data device (wl_data_*), as well as zwp and
// gtk primary selection protocols.

class TestSelectionDeviceManager : public GlobalObject {
 public:
  struct Delegate {
    virtual TestSelectionDevice* CreateDevice(wl_client* client,
                                              uint32_t id) = 0;
    virtual TestSelectionSource* CreateSource(wl_client* client,
                                              uint32_t id) = 0;
    virtual void OnDestroying() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  struct InterfaceInfo {
    const struct wl_interface* interface;
    const void* implementation;
    uint32_t version;
  };

  TestSelectionDeviceManager(const InterfaceInfo& info, Delegate* delegate);
  ~TestSelectionDeviceManager() override;

  TestSelectionDeviceManager(const TestSelectionDeviceManager&) = delete;
  TestSelectionDeviceManager& operator=(const TestSelectionDeviceManager&) =
      delete;

  TestSelectionDevice* device() { return device_; }
  TestSelectionSource* source() { return source_; }

  // Protocol object requests:
  static void CreateSource(wl_client* client,
                           wl_resource* manager_resource,
                           uint32_t id);
  static void GetDevice(wl_client* client,
                        wl_resource* manager_resource,
                        uint32_t id,
                        wl_resource* seat_resource);

 private:
  Delegate* const delegate_;

  TestSelectionDevice* device_ = nullptr;
  TestSelectionSource* source_ = nullptr;
};

class TestSelectionOffer : public ServerObject {
 public:
  struct Delegate {
    virtual void SendOffer(const std::string& mime_type) = 0;
    virtual void OnDestroying() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  TestSelectionOffer(wl_resource* resource, Delegate* delegate);
  ~TestSelectionOffer() override;

  TestSelectionOffer(const TestSelectionOffer&) = delete;
  TestSelectionOffer& operator=(const TestSelectionOffer&) = delete;

  void OnOffer(const std::string& mime_type, ui::PlatformClipboard::Data data);

  // Protocol object requests:
  static void Receive(wl_client* client,
                      wl_resource* resource,
                      const char* mime_type,
                      int fd);

 private:
  Delegate* const delegate_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ui::PlatformClipboard::DataMap data_to_offer_;
};

class TestSelectionSource : public ServerObject {
 public:
  struct Delegate {
    virtual void SendSend(const std::string& mime_type,
                          base::ScopedFD write_fd) = 0;
    virtual void SendCancelled() = 0;
    virtual void OnDestroying() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  TestSelectionSource(wl_resource* resource, Delegate* delegate);
  ~TestSelectionSource() override;

  using ReadDataCallback = base::OnceCallback<void(std::vector<uint8_t>&&)>;
  void ReadData(const std::string& mime_type, ReadDataCallback callback);

  void OnCancelled();

  const std::vector<std::string>& mime_types() const { return mime_types_; }

  // Protocol object requests:
  static void Offer(struct wl_client* client,
                    struct wl_resource* resource,
                    const char* mime_type);

 private:
  Delegate* const delegate_;

  std::vector<std::string> mime_types_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

class TestSelectionDevice : public ServerObject {
 public:
  struct Delegate {
    virtual TestSelectionOffer* CreateAndSendOffer() = 0;
    virtual void SendSelection(TestSelectionOffer* offer) = 0;
    virtual void HandleSetSelection(TestSelectionSource* source,
                                    uint32_t serial) = 0;
    virtual void OnDestroying() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  TestSelectionDevice(wl_resource* resource, Delegate* delegate);
  ~TestSelectionDevice() override;

  TestSelectionDevice(const TestSelectionDevice&) = delete;
  TestSelectionDevice& operator=(const TestSelectionDevice&) = delete;

  TestSelectionOffer* OnDataOffer();
  void OnSelection(TestSelectionOffer* offer);

  // Protocol object requests:
  static void SetSelection(struct wl_client* client,
                           struct wl_resource* resource,
                           struct wl_resource* source,
                           uint32_t serial);

 private:
  Delegate* const delegate_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_

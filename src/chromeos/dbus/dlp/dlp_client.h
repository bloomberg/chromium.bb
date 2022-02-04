// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLP_DLP_CLIENT_H_
#define CHROMEOS_DBUS_DLP_DLP_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// DlpClient is used to communicate with the org.chromium.Dlp
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(DLP) DlpClient {
 public:
  using SetDlpFilesPolicyCallback =
      base::OnceCallback<void(const dlp::SetDlpFilesPolicyResponse response)>;
  using AddFileCallback =
      base::OnceCallback<void(const dlp::AddFileResponse response)>;
  using GetFilesSourcesCallback =
      base::OnceCallback<void(const dlp::GetFilesSourcesResponse response)>;

  // Interface with testing functionality. Accessed through GetTestInterface(),
  // only implemented in the fake implementation.
  class TestInterface {
   public:
    // Returns how many times |SetDlpFilesPolicyCount| was called.
    virtual int GetSetDlpFilesPolicyCount() const = 0;

   protected:
    virtual ~TestInterface() {}
  };

  DlpClient(const DlpClient&) = delete;
  DlpClient& operator=(const DlpClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static DlpClient* Get();

  // Dlp daemon D-Bus method calls. See org.chromium.Dlp.xml and
  // dlp_service.proto in Chromium OS code for the documentation of the methods
  // and request/response messages.
  virtual void SetDlpFilesPolicy(const dlp::SetDlpFilesPolicyRequest request,
                                 SetDlpFilesPolicyCallback callback) = 0;
  virtual void AddFile(const dlp::AddFileRequest request,
                       AddFileCallback callback) = 0;
  virtual void GetFilesSources(const dlp::GetFilesSourcesRequest request,
                               GetFilesSourcesCallback callback) const = 0;

  virtual bool IsAlive() const = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  DlpClient();
  virtual ~DlpClient();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::DlpClient;
}  // namespace ash

#endif  // CHROMEOS_DBUS_DLP_DLP_CLIENT_H_

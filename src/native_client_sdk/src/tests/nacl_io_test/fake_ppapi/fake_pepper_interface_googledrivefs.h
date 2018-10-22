// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_GOOGLEDRIVEFS_H_
#define LIBRARIES_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_GOOGLEDRIVEFS_H_

#include <string>

#include "nacl_io/pepper_interface_dummy.h"
#include "sdk_util/macros.h"

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_file_io_interface.h"
#include "fake_ppapi/fake_file_ref_interface.h"
#include "fake_ppapi/fake_pepper_interface_url_loader.h"
#include "fake_ppapi/fake_var_interface.h"
#include "fake_ppapi/fake_var_manager.h"

struct FakeGoogleDriveServerResponse {
  FakeGoogleDriveServerResponse() : status_code(0) {}

  int status_code;
  std::string body;
};

class FakeGoogleDriveServer {
 public:
  FakeGoogleDriveServer();
  void Respond(const std::string& url,
               const std::string& headers,
               const std::string& method,
               const std::string& body,
               FakeGoogleDriveServerResponse* out_response);
};

class FakeDriveURLLoaderInterface : public FakeURLLoaderInterface {
 public:
  explicit FakeDriveURLLoaderInterface(FakeCoreInterface* core_interface);

  virtual PP_Resource Create(PP_Instance instance);
  virtual int32_t Open(PP_Resource loader,
                       PP_Resource request_info,
                       PP_CompletionCallback callback);
  virtual PP_Resource GetResponseInfo(PP_Resource loader);
  virtual int32_t FinishStreamingToFile(PP_Resource loader,
                                        PP_CompletionCallback callback);
  virtual void Close(PP_Resource loader);

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDriveURLLoaderInterface);
};

class FakeDriveURLRequestInfoInterface : public FakeURLRequestInfoInterface {
 public:
  FakeDriveURLRequestInfoInterface(FakeCoreInterface* core_interface,
                                   FakeVarInterface* var_interface);

  virtual PP_Resource Create(PP_Instance instance);

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDriveURLRequestInfoInterface);
};

class FakeDriveURLResponseInfoInterface : public FakeURLResponseInfoInterface {
 public:
  FakeDriveURLResponseInfoInterface(FakeCoreInterface* core_interface,
                                    FakeVarInterface* var_interface,
                                    FakeFileRefInterface* file_ref_interface);
  ~FakeDriveURLResponseInfoInterface();

  virtual PP_Var GetProperty(PP_Resource response,
                             PP_URLResponseProperty property);
  virtual PP_Resource GetBodyAsFileRef(PP_Resource response);

 private:
  FakeFileRefInterface* file_ref_interface_;
  PP_Resource filesystem_resource_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveURLResponseInfoInterface);
};

// This class is a fake implementation of the interfaces necessary to access
// the GOOGLEDRIVEFS Filesystem from NaCl.
//
// Example:
//   FakePepperInterfaceGoogleDriveFs ppapi_googledrivefs;
//   ...
//   PP_Resource ref_resource =
//   ppapi_googledrivefs.GetURLLoaderInterface()->Create(
//       ppapi_googledrivefs.GetInstance());
//   ...
//
// NOTE: This pepper interface creates an instance resource that can only be
// used with FakePepperInterfaceGoogleDriveFs, not other fake pepper
// implementations.
class FakePepperInterfaceGoogleDriveFs : public nacl_io::PepperInterfaceDummy {
 public:
  FakePepperInterfaceGoogleDriveFs();
  ~FakePepperInterfaceGoogleDriveFs();

  virtual PP_Instance GetInstance() { return instance_; }
  virtual nacl_io::CoreInterface* GetCoreInterface();
  virtual nacl_io::FileIoInterface* GetFileIoInterface();
  virtual nacl_io::FileRefInterface* GetFileRefInterface();
  virtual nacl_io::VarInterface* GetVarInterface();
  virtual nacl_io::URLLoaderInterface* GetURLLoaderInterface();
  virtual nacl_io::URLRequestInfoInterface* GetURLRequestInfoInterface();
  virtual nacl_io::URLResponseInfoInterface* GetURLResponseInfoInterface();

  FakeGoogleDriveServer* server_template() {
    return &google_drive_server_template_;
  }

 private:
  void Init();

  FakeResourceManager resource_manager_;
  FakeCoreInterface core_interface_;
  FakeVarInterface var_interface_;
  FakeVarManager var_manager_;
  FakeFileIoInterface file_io_interface_;
  FakeFileRefInterface file_ref_interface_;
  FakeGoogleDriveServer google_drive_server_template_;
  FakeDriveURLLoaderInterface drive_url_loader_interface_;
  FakeDriveURLRequestInfoInterface drive_url_request_info_interface_;
  FakeDriveURLResponseInfoInterface drive_url_response_info_interface_;
  PP_Instance instance_;

  DISALLOW_COPY_AND_ASSIGN(FakePepperInterfaceGoogleDriveFs);
};

#endif  // LIBRARIES_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_GOOGLEDRIVEFS_H_

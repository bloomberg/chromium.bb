// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_H_
#define PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_H_

#include <string>

#include "ppapi/c/ppb_core.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/tests/test_case.h"

namespace pp {

class TCPSocketPrivate;

}  // namespace pp

class TestHostResolverPrivate : public TestCase {
 public:
  explicit TestHostResolverPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string SyncConnect(pp::TCPSocketPrivate* socket,
                          const std::string& host,
                          uint16_t port);
  std::string SyncConnect(pp::TCPSocketPrivate* socket,
                          const PP_NetAddress_Private& address);
  std::string SyncRead(pp::TCPSocketPrivate* socket,
                       char* buffer,
                       int32_t num_bytes,
                       int32_t* bytes_read);
  std::string SyncWrite(pp::TCPSocketPrivate* socket,
                        const char* buffer,
                        int32_t num_bytes,
                        int32_t* bytes_written);
  std::string CheckHTTPResponse(pp::TCPSocketPrivate* socket,
                                const std::string& request,
                                const std::string& response);
  std::string SyncResolve(PP_Resource host_resolver,
                          const std::string& host,
                          uint16_t port,
                          const PP_HostResolver_Private_Hint& hint);
  std::string ParametrizedTestResolve(const PP_HostResolver_Private_Hint& hint);

  std::string TestCreate();
  std::string TestResolve();
  std::string TestResolveIPv4();

  const PPB_Core* core_interface_;
  const PPB_HostResolver_Private* host_resolver_private_interface_;

  std::string host_;
  uint16_t port_;
};

#endif  // PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_H_

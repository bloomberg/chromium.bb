// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_host_resolver_private_disallowed.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(HostResolverPrivateDisallowed);

TestHostResolverPrivateDisallowed::TestHostResolverPrivateDisallowed(
    TestingInstance* instance)
    : TestCase(instance) {
}

bool TestHostResolverPrivateDisallowed::Init() {
  bool host_resolver_private_is_available =
      pp::HostResolverPrivate::IsAvailable();
  if (!host_resolver_private_is_available)
    instance_->AppendError("PPB_HostResolver_Private interface not available");

  bool init_host_port =
      GetLocalHostPort(instance_->pp_instance(), &host_, &port_);
  if (!init_host_port)
    instance_->AppendError("Can't init host and port");

  return host_resolver_private_is_available &&
      init_host_port &&
      EnsureRunningOverHTTP();
}

void TestHostResolverPrivateDisallowed::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Resolve, filter);
}

std::string TestHostResolverPrivateDisallowed::TestResolve() {
  pp::HostResolverPrivate host_resolver(instance_);
  PP_HostResolver_Private_Hint hint;
  hint.family = PP_NETADDRESSFAMILY_UNSPECIFIED;
  hint.flags = PP_HOST_RESOLVER_FLAGS_CANONNAME;
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = host_resolver.Resolve(host_, port_, hint,
                                     callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_HostResolver_Private::Resolve force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_ERROR_FAILED)
    return "PPB_HostResolver_Private can resolve without allowing switch";
  PASS();
}

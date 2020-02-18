// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_HOST_RESOLVER_H_
#define CONTENT_PUBLIC_TEST_TEST_HOST_RESOLVER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace net {
class HostResolverProc;
class RuleBasedHostResolverProc;
class ScopedDefaultHostResolverProc;
}

namespace content {
class TestHostResolver {
 public:
  TestHostResolver();
  ~TestHostResolver();

  net::RuleBasedHostResolverProc* host_resolver() {
    return rule_based_resolver_.get();
  }

 private:
  // Host resolver used during tests.
  scoped_refptr<net::HostResolverProc> local_resolver_;
  scoped_refptr<net::RuleBasedHostResolverProc> rule_based_resolver_;
  std::unique_ptr<net::ScopedDefaultHostResolverProc>
      scoped_local_host_resolver_proc_;

  DISALLOW_COPY_AND_ASSIGN(TestHostResolver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_HOST_RESOLVER_H_

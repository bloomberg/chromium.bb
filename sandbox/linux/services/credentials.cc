// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/credentials.h"

#include <stdio.h>
#include <sys/capability.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace {

struct CapFreeDeleter {
  inline void operator()(cap_t cap) const {
    int ret = cap_free(cap);
    CHECK_EQ(0, ret);
  }
};

// Wrapper to manage libcap2's cap_t type.
typedef scoped_ptr<typeof(*((cap_t)0)), CapFreeDeleter> ScopedCap;

struct CapTextFreeDeleter {
  inline void operator()(char* cap_text) const {
    int ret = cap_free(cap_text);
    CHECK_EQ(0, ret);
  }
};

// Wrapper to manage the result from libcap2's cap_from_text().
typedef scoped_ptr<char, CapTextFreeDeleter> ScopedCapText;

}  // namespace.

namespace sandbox {

Credentials::Credentials() {
}

Credentials::~Credentials() {
}

void Credentials::DropAllCapabilities() {
  ScopedCap cap(cap_init());
  CHECK(cap);
  PCHECK(0 == cap_set_proc(cap.get()));
}

bool Credentials::HasAnyCapability() {
  ScopedCap current_cap(cap_get_proc());
  CHECK(current_cap);
  ScopedCap empty_cap(cap_init());
  CHECK(empty_cap);
  return cap_compare(current_cap.get(), empty_cap.get()) != 0;
}

scoped_ptr<std::string> Credentials::GetCurrentCapString() {
  ScopedCap current_cap(cap_get_proc());
  CHECK(current_cap);
  ScopedCapText cap_text(cap_to_text(current_cap.get(), NULL));
  CHECK(cap_text);
  return scoped_ptr<std::string> (new std::string(cap_text.get()));
}

}  // namespace sandbox.

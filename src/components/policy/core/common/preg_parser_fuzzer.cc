// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <base/strings/utf_string_conversions.h>
#include "base/strings/string16.h"

#include "components/policy/core/common/policy_load_status.h"
#include "components/policy/core/common/preg_parser.h"
#include "components/policy/core/common/registry_dict.h"

namespace {

const char kRegistryChromePolicyKey[] = "SOFTWARE\\Policies\\Chromium";

}  // namespace

namespace policy {
namespace preg_parser {

// Disable logging.
struct Environment {
  Environment() : root(base::ASCIIToUTF16(kRegistryChromePolicyKey)) {
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }

  const base::string16 root;
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Note: Don't use PolicyLoadStatusUmaReporter here, it leaks!
  PolicyLoadStatusSampler status;
  RegistryDict dict;
  ReadDataInternal(data, size, env->root, &dict, &status, "data");
  return 0;
}

}  // namespace preg_parser
}  // namespace policy

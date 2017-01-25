// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace service_manager {
namespace test {

// This symbol must be defined by any target linking against the
// ":run_all_service_tests" target in this directory. Use the service_test
// GN template defined in
// src/services/service_manager/public/tools/test/service_test.gni to
// autogenerate and link against a definition of the symbol dervied from the
// contents of a generated service catalog. See the service_test.gni
// documentation for more details.
extern const char kServiceTestCatalog[];

}  // namespace test
}  // namespace service_manager

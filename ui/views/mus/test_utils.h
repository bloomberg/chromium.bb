// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_TEST_UTILS_H_
#define UI_VIEWS_MUS_TEST_UTILS_H_

#include "base/memory/ptr_util.h"
#include "ui/views/mus/mus_client.h"

namespace views {
namespace test {

class MusClientTestApi {
 public:
  static std::unique_ptr<MusClient> Create(
      service_manager::Connector* connector,
      const service_manager::Identity& identity) {
    return base::WrapUnique(new MusClient(connector, identity));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MusClientTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_MUS_TEST_UTILS_H_

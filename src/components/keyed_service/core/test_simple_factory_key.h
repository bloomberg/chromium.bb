// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_TEST_SIMPLE_FACTORY_KEY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_TEST_SIMPLE_FACTORY_KEY_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/core/simple_factory_key.h"

// A fake SimpleFactoryKey for testing.
class KEYED_SERVICE_EXPORT TestSimpleFactoryKey : public SimpleFactoryKey {
 public:
  TestSimpleFactoryKey(const base::FilePath& path,
                       bool is_off_the_record = false);
  ~TestSimpleFactoryKey() override;

  // SimpleFactoryKey implementation.
  bool IsOffTheRecord() const override;

 private:
  bool is_off_the_record_;

  DISALLOW_COPY_AND_ASSIGN(TestSimpleFactoryKey);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_TEST_SIMPLE_FACTORY_KEY_H_

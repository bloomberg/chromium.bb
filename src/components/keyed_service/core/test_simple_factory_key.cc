// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/test_simple_factory_key.h"

TestSimpleFactoryKey::TestSimpleFactoryKey(const base::FilePath& path,
                                           bool is_off_the_record)
    : SimpleFactoryKey(path), is_off_the_record_(is_off_the_record) {}

TestSimpleFactoryKey::~TestSimpleFactoryKey() = default;

bool TestSimpleFactoryKey::IsOffTheRecord() const {
  return is_off_the_record_;
}

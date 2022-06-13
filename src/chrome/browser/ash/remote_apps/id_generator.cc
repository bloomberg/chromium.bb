// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/remote_apps/id_generator.h"

#include "base/guid.h"

namespace ash {

std::string GuidIdGenerator::GenerateId() {
  return base::GenerateGUID();
}

FakeIdGenerator::FakeIdGenerator(const std::vector<std::string>& ids)
    : ids_(ids) {}

FakeIdGenerator::~FakeIdGenerator() = default;

std::string FakeIdGenerator::GenerateId() {
  std::string id = ids_[index_];
  ++index_;
  return id;
}

}  // namespace ash

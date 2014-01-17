// Copyright (C) 2014 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fallback_data_store.h"

#include <string>

#include <gtest/gtest.h>

#include "util/json.h"

namespace i18n {
namespace addressinput {

namespace {

TEST(FallbackDataStore, Parsability) {
  std::string data;
  ASSERT_TRUE(FallbackDataStore::Get("data/US", &data));

  scoped_ptr<Json> json(Json::Build());
  EXPECT_TRUE(json->ParseObject(data));
}

}  // namespace

}  // namespace addressinput
}  // namespace i18n

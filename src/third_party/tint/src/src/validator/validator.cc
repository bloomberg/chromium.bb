// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/validator/validator.h"

#include "src/validator/validator_impl.h"

namespace tint {

Validator::Validator() : impl_(std::make_unique<tint::ValidatorImpl>()) {}

Validator::~Validator() = default;

bool Validator::Validate(const ast::Module* module) {
  bool ret = impl_->Validate(module);

  if (impl_->has_error())
    set_error(impl_->error());

  return ret;
}

}  // namespace tint

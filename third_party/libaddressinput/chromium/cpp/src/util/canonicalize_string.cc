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

#include "canonicalize_string.h"

#include <libaddressinput/util/scoped_ptr.h>

#include <string>

namespace i18n {
namespace addressinput {

namespace {

class SimpleStringCanonicalizer : public StringCanonicalizer {
 public:
  SimpleStringCanonicalizer() {}

  virtual ~SimpleStringCanonicalizer() {}

  // StringCanonicalizer implementation.
  virtual std::string CanonicalizeString(const std::string& original) {
    // The best we can do without ICU.
    return original;
  }
};

}  // namespace

// static
scoped_ptr<StringCanonicalizer> StringCanonicalizer::Build() {
  return scoped_ptr<StringCanonicalizer>(new SimpleStringCanonicalizer);
}

}  // namespace addressinput
}  // namespace i18n

/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include <string>

#include "tensorflow/lite/delegates/utils/dummy_delegate/dummy_delegate.h"
#include "tensorflow/lite/tools/delegates/delegate_provider.h"

namespace tflite {
namespace tools {

class DummyDelegateProvider : public DelegateProvider {
 public:
  DummyDelegateProvider() {
    default_params_.AddParam("use_dummy_delegate",
                             ToolParam::Create<bool>(false));
  }

  std::vector<Flag> CreateFlags(ToolParams* params) const final;

  void LogParams(const ToolParams& params) const final;

  TfLiteDelegatePtr CreateTfLiteDelegate(const ToolParams& params) const final;

  std::string GetName() const final { return "DummyDelegate"; }
};
REGISTER_DELEGATE_PROVIDER(DummyDelegateProvider);

std::vector<Flag> DummyDelegateProvider::CreateFlags(ToolParams* params) const {
  std::vector<Flag> flags = {CreateFlag<bool>("use_dummy_delegate", params,
                                              "use the dummy delegate.")};
  return flags;
}

void DummyDelegateProvider::LogParams(const ToolParams& params) const {
  TFLITE_LOG(INFO) << "Use dummy test delegate : ["
                   << params.Get<bool>("use_dummy_delegate") << "]";
}

TfLiteDelegatePtr DummyDelegateProvider::CreateTfLiteDelegate(
    const ToolParams& params) const {
  if (params.Get<bool>("use_dummy_delegate")) {
    auto default_options = TfLiteDummyDelegateOptionsDefault();
    return TfLiteDummyDelegateCreateUnique(&default_options);
  }
  return TfLiteDelegatePtr(nullptr, [](TfLiteDelegate*) {});
}

}  // namespace tools
}  // namespace tflite

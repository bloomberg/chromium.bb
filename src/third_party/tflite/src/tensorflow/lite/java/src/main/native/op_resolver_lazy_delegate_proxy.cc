/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#if !TFLITE_DISABLE_SELECT_JAVA_APIS
#include <dlfcn.h>
#endif

#include <functional>
#include <memory>

#include "tensorflow/lite/core/api/op_resolver_internal.h"
#include "tensorflow/lite/core/shims/c/common.h"
#include "tensorflow/lite/core/shims/cc/model_builder.h"
#if TFLITE_DISABLE_SELECT_JAVA_APIS
#include "tensorflow/lite/core/shims/c/experimental/acceleration/configuration/delegate_plugin.h"
#include "tensorflow/lite/core/shims/c/experimental/acceleration/configuration/xnnpack_plugin.h"
#include "tensorflow/lite/experimental/acceleration/configuration/configuration_generated.h"
#else
#include "tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"
#endif
#include "tensorflow/lite/java/src/main/native/op_resolver_lazy_delegate_proxy.h"

namespace tflite {
namespace jni {

const TfLiteRegistration* OpResolverLazyDelegateProxy::FindOp(
    tflite::BuiltinOperator op, int version) const {
  return op_resolver_->FindOp(op, version);
}

const TfLiteRegistration* OpResolverLazyDelegateProxy::FindOp(
    const char* op, int version) const {
  return op_resolver_->FindOp(op, version);
}

OpResolver::TfLiteDelegateCreators
OpResolverLazyDelegateProxy::GetDelegateCreators() const {
  // Early exit if not using the XNNPack delegate by default
  if (!use_xnnpack_) {
    return OpResolver::TfLiteDelegateCreators();
  }

  return OpResolver::TfLiteDelegateCreators{
      {&OpResolverLazyDelegateProxy::createXNNPackDelegate}};
}

bool OpResolverLazyDelegateProxy::MayContainUserDefinedOps() const {
  return OpResolverInternal::MayContainUserDefinedOps(*op_resolver_);
}

std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)>
OpResolverLazyDelegateProxy::createXNNPackDelegate(int num_threads) {
  TfLiteDelegate* delegate = nullptr;
  void (*delegate_deleter)(TfLiteDelegate*) = nullptr;
#if TFLITE_DISABLE_SELECT_JAVA_APIS
  // Construct a FlatBuffer containing
  //   TFLiteSettings {
  //     delegate: Delegate.XNNPack
  //     XNNPackSettings {
  //       num_threads: <num_threads>
  //     }
  //   }
  flatbuffers::FlatBufferBuilder flatbuffer_builder;
  tflite::XNNPackSettingsBuilder xnnpack_settings_builder(flatbuffer_builder);
  if (num_threads >= 0) {
    xnnpack_settings_builder.add_num_threads(num_threads);
  }
  flatbuffers::Offset<tflite::XNNPackSettings> xnnpack_settings =
      xnnpack_settings_builder.Finish();
  tflite::TFLiteSettingsBuilder tflite_settings_builder(flatbuffer_builder);
  tflite_settings_builder.add_xnnpack_settings(xnnpack_settings);
  tflite_settings_builder.add_delegate(tflite::Delegate_XNNPACK);
  flatbuffers::Offset<tflite::TFLiteSettings> tflite_settings =
      tflite_settings_builder.Finish();
  flatbuffer_builder.Finish(tflite_settings);
  const tflite::TFLiteSettings* tflite_settings_flatbuffer =
      flatbuffers::GetRoot<tflite::TFLiteSettings>(
          flatbuffer_builder.GetBufferPointer());
  // Create an XNNPack delegate plugin using the settings from the flatbuffer.
  const TfLiteOpaqueDelegatePlugin* delegate_plugin =
      TfLiteXnnpackDelegatePluginCApi();
  delegate = reinterpret_cast<TfLiteDelegate*>(
      delegate_plugin->create(tflite_settings_flatbuffer));
  delegate_deleter =
      reinterpret_cast<void (*)(TfLiteDelegate*)>(delegate_plugin->destroy);
#else
  // We use dynamic loading to avoid taking a hard dependency on XNNPack.
  // This allows clients that use trimmed builds to save on binary size.
  auto xnnpack_options_default =
      reinterpret_cast<decltype(TfLiteXNNPackDelegateOptionsDefault)*>(
          dlsym(RTLD_DEFAULT, "TfLiteXNNPackDelegateOptionsDefault"));
  auto xnnpack_create =
      reinterpret_cast<decltype(TfLiteXNNPackDelegateCreate)*>(
          dlsym(RTLD_DEFAULT, "TfLiteXNNPackDelegateCreate"));
  auto xnnpack_delete =
      reinterpret_cast<decltype(TfLiteXNNPackDelegateDelete)*>(
          dlsym(RTLD_DEFAULT, "TfLiteXNNPackDelegateDelete"));

  if (xnnpack_options_default && xnnpack_create && xnnpack_delete) {
    TfLiteXNNPackDelegateOptions options = xnnpack_options_default();
    if (num_threads > 0) {
      options.num_threads = num_threads;
    }
    delegate = xnnpack_create(&options);
    delegate_deleter = xnnpack_delete;
  }
#endif
  return std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)>(
      delegate, delegate_deleter);
}

}  // namespace jni
}  // namespace tflite

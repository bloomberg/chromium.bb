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

// This file wraps rocsolver API calls with dso loader so that we don't need to
// have explicit linking to librocsolver. All TF hipsarse API usage should route
// through this wrapper.

#ifndef TENSORFLOW_STREAM_EXECUTOR_ROCM_ROCSOLVER_WRAPPER_H_
#define TENSORFLOW_STREAM_EXECUTOR_ROCM_ROCSOLVER_WRAPPER_H_

#include "rocm/include/rocsolver.h"
#include "tensorflow/stream_executor/lib/env.h"
#include "tensorflow/stream_executor/platform/dso_loader.h"
#include "tensorflow/stream_executor/platform/port.h"

namespace tensorflow {
namespace wrap {

#ifdef PLATFORM_GOOGLE

#define ROCSOLVER_API_WRAPPER(api_name)                        \
  template <typename... Args>                                  \
  auto api_name(Args... args)->decltype(::api_name(args...)) { \
    return ::api_name(args...);                                \
  }

#else

#define TO_STR_(x) #x
#define TO_STR(x) TO_STR_(x)

#define ROCSOLVER_API_WRAPPER(api_name)                                       \
  template <typename... Args>                                                 \
  auto api_name(Args... args)->decltype(::api_name(args...)) {                \
    using FuncPtrT = std::add_pointer<decltype(::api_name)>::type;            \
    static FuncPtrT loaded = []() -> FuncPtrT {                               \
      static const char* kName = TO_STR(api_name);                            \
      void* f;                                                                \
      auto s = stream_executor::port::Env::Default()->GetSymbolFromLibrary(   \
          stream_executor::internal::CachedDsoLoader::GetRocsolverDsoHandle() \
              .ValueOrDie(),                                                  \
          kName, &f);                                                         \
      CHECK(s.ok()) << "could not find " << kName                             \
                    << " in rocsolver lib; dlerror: " << s.error_message();   \
      return reinterpret_cast<FuncPtrT>(f);                                   \
    }();                                                                      \
    return loaded(args...);                                                   \
  }

#endif

// clang-format off
#define FOREACH_ROCSOLVER_API(__macro) \
  __macro(rocsolver_spotrf)            \
  __macro(rocsolver_dpotrf)            \
  __macro(rocsolver_cpotrf)            \
  __macro(rocsolver_zpotrf)

// clang-format on

FOREACH_ROCSOLVER_API(ROCSOLVER_API_WRAPPER)

#undef TO_STR_
#undef TO_STR
#undef FOREACH_ROCSOLVER_API
#undef ROCSOLVER_API_WRAPPER

}  // namespace wrap
}  // namespace tensorflow

#endif  // TENSORFLOW_STREAM_EXECUTOR_ROCM_ROCSOLVER_WRAPPER_H_

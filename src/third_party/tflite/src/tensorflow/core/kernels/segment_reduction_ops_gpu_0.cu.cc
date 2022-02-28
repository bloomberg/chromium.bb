/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/kernels/segment_reduction_ops_gpu.cu.h"

namespace tensorflow {

bool UseDeterministicSegmentReductions() {
  // See comment below regarding CI build error on Windows.
#if !defined(PLATFORM_WINDOWS)
  static bool cached_result = [] {
    bool result = false;
    TF_CHECK_OK(tensorflow::ReadBoolFromEnvVar(
        "TF_USE_DETERMINISTIC_SEGMENT_REDUCTIONS",
        /*default_val=*/false, &result));
    return result;
  }();
  return cached_result;
#else
  return false;
#endif
}

bool DisableSegmentReductionOpDeterminismExceptions() {
  static bool cached_disable = [] {
    bool disable = false;
    TF_CHECK_OK(tensorflow::ReadBoolFromEnvVar(
        "TF_DISABLE_SEGMENT_REDUCTION_OP_DETERMINISM_EXCEPTIONS",
        /*default_val=*/false, &disable));
    return disable;
  }();
  return cached_disable;
}

namespace functor {

#define DEFINE_SORTED_GPU_SPECS_INDEX(T, Index)               \
  template struct SegmentReductionFunctor<                    \
      T, Index, /*InitialValueF=*/functor::Zero<T>,           \
      /*EmptySegmentValueF=*/functor::Zero<T>, functor::Sum>; \
  template struct SegmentReductionFunctor<                    \
      T, Index, /*InitialValueF=*/functor::One<T>,            \
      /*EmptySegmentValueF=*/functor::One<T>, functor::Prod>; \
  template struct SegmentReductionFunctor<                    \
      T, Index, /*InitialValueF=*/functor::Highest<T>,        \
      /*EmptySegmentValueF=*/functor::Zero<T>, functor::Min>; \
  template struct SegmentReductionFunctor<                    \
      T, Index, /*InitialValueF=*/functor::Lowest<T>,         \
      /*EmptySegmentValueF=*/functor::Zero<T>, functor::Max>;

#define DEFINE_SORTED_GPU_SPECS(T) DEFINE_SORTED_GPU_SPECS_INDEX(T, int32);

TF_CALL_GPU_NUMBER_TYPES(DEFINE_SORTED_GPU_SPECS);

#define DEFINE_REAL_UNSORTED_GPU_SPECS_INDEX(T, Index)                         \
  template struct UnsortedSegmentFunctor<GPUDevice, T, Index,                  \
                                         functor::Lowest<T>, functor::Max>;    \
  template struct UnsortedSegmentFunctor<GPUDevice, T, Index,                  \
                                         functor::Highest<T>, functor::Min>;   \
  template struct UnsortedSegmentFunctor<GPUDevice, T, Index, functor::One<T>, \
                                         functor::Prod>;

// Sum is the only op that supports all input types currently.
#define DEFINE_SUM_UNSORTED_GPU_SPECS_INDEX(T, Index)         \
  template struct UnsortedSegmentFunctor<GPUDevice, T, Index, \
                                         functor::Zero<T>, functor::Sum>;

#define DEFINE_REAL_GPU_SPECS(T) DEFINE_REAL_UNSORTED_GPU_SPECS_INDEX(T, int32);

#define DEFINE_SUM_GPU_SPECS(T) DEFINE_SUM_UNSORTED_GPU_SPECS_INDEX(T, int32);

TF_CALL_GPU_NUMBER_TYPES(DEFINE_REAL_GPU_SPECS);
TF_CALL_GPU_NUMBER_TYPES(DEFINE_SUM_GPU_SPECS);

#undef DEFINE_SORTED_GPU_SPECS_INDEX
#undef DEFINE_SORTED_GPU_SPECS
#undef DEFINE_REAL_UNSORTED_GPU_SPECS_INDEX
#undef DEFINE_SUM_UNSORTED_GPU_SPECS_INDEX
#undef DEFINE_REAL_GPU_SPECS
#undef DEFINE_SUM_GPU_SPECS

// TODO(benbarsdell): These kernels are disabled on Windows as a workaround for
// a CI build error: "formal parameter with requested alignment of 128 won't be
// aligned". The root cause is suspected to be an aligned type (AlignedVector)
// being passed to a function by value, possibly inside the CUB library
// somewhere, but I have not yet been able to reproduce it in isolation outside
// of the GitHub CI.
#if !defined(PLATFORM_WINDOWS)

#define DEFINE_SPARSE_SEGMENT_REDUCTION_FUNCTOR(T)                \
  template struct SparseSegmentReductionFunctor<T, int32, int32>; \
  template struct SparseSegmentReductionFunctor<T, int32, int64_t>;
TF_CALL_GPU_NUMBER_TYPES(DEFINE_SPARSE_SEGMENT_REDUCTION_FUNCTOR);
#undef DEFINE_SPARSE_SEGMENT_REDUCTION_FUNCTOR

#define DEFINE_SPARSE_SEGMENT_GRAD_FUNCTOR(T)                           \
  template struct SparseSegmentGradFunctor<GPUDevice, T, int32, int32>; \
  template struct SparseSegmentGradFunctor<GPUDevice, T, int32, int64_t>;
TF_CALL_GPU_NUMBER_TYPES(DEFINE_SPARSE_SEGMENT_GRAD_FUNCTOR);
#undef DEFINE_SPARSE_SEGMENT_GRAD_FUNCTOR

#endif  // !defined(PLATFORM_WINDOWS)

}  // namespace functor
}  // namespace tensorflow

#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

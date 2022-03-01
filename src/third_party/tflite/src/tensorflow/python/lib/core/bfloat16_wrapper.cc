/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#include "pybind11/pybind11.h"
#include "tensorflow/python/lib/core/bfloat16.h"

PYBIND11_MODULE(_pywrap_bfloat16, m) {
  tensorflow::RegisterNumpyBfloat16();

  m.def("TF_bfloat16_type",
        [] { return pybind11::handle(tensorflow::Bfloat16Dtype()); });
}

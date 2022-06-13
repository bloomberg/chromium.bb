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

#ifndef TENSORFLOW_PYTHON_LIB_CORE_PYBIND11_STATUS_H_
#define TENSORFLOW_PYTHON_LIB_CORE_PYBIND11_STATUS_H_

#include <Python.h>

#include "pybind11/pybind11.h"
#include "tensorflow/c/tf_status_internal.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"
#include "tensorflow/python/lib/core/py_exception_registry.h"

namespace tensorflow {

namespace internal {

inline PyObject* CodeToPyExc(const int code) {
  switch (code) {
    case error::Code::INVALID_ARGUMENT:
      return PyExc_ValueError;
    case error::Code::OUT_OF_RANGE:
      return PyExc_IndexError;
    case error::Code::UNIMPLEMENTED:
      return PyExc_NotImplementedError;
    default:
      return PyExc_RuntimeError;
  }
}

inline PyObject* StatusToPyExc(const Status& status) {
  return CodeToPyExc(status.code());
}

inline PyObject* TFStatusToPyExc(const TF_Status* status) {
  return CodeToPyExc(TF_GetCode(status));
}

inline pybind11::dict StatusPayloadToDict(const Status& status) {
  pybind11::dict dict;
  const auto& payloads = errors::GetPayloads(status);
  for (auto& pair : payloads) {
    dict[PyBytes_FromString(pair.first.c_str())] =
        PyBytes_FromString(pair.second.c_str());
  }
  return dict;
}

inline pybind11::dict TFStatusPayloadToDict(TF_Status* status) {
  return StatusPayloadToDict(status->status);
}

}  // namespace internal

inline void MaybeRaiseFromStatus(const Status& status) {
  if (!status.ok()) {
    PyErr_SetString(internal::StatusToPyExc(status),
                    status.error_message().c_str());
    throw pybind11::error_already_set();
  }
}

inline void SetRegisteredErrFromStatus(const tensorflow::Status& status) {
  PyErr_SetObject(PyExceptionRegistry::Lookup(status.code()),
                  pybind11::make_tuple(pybind11::none(), pybind11::none(),
                                       status.error_message(),
                                       internal::StatusPayloadToDict(status))
                      .ptr());
}

inline void SetRegisteredErrFromTFStatus(TF_Status* status) {
  PyErr_SetObject(PyExceptionRegistry::Lookup(TF_GetCode(status)),
                  pybind11::make_tuple(pybind11::none(), pybind11::none(),
                                       TF_Message(status),
                                       internal::TFStatusPayloadToDict(status))
                      .ptr());
}

inline void MaybeRaiseRegisteredFromStatus(const tensorflow::Status& status) {
  if (!status.ok()) {
    SetRegisteredErrFromStatus(status);
    throw pybind11::error_already_set();
  }
}

inline void MaybeRaiseRegisteredFromStatusWithGIL(
    const tensorflow::Status& status) {
  if (!status.ok()) {
    // Acquire GIL for throwing exception.
    pybind11::gil_scoped_acquire acquire;
    SetRegisteredErrFromStatus(status);
    throw pybind11::error_already_set();
  }
}

inline void MaybeRaiseFromTFStatus(TF_Status* status) {
  TF_Code code = TF_GetCode(status);
  if (code != TF_OK) {
    PyErr_SetString(internal::TFStatusToPyExc(status), TF_Message(status));
    throw pybind11::error_already_set();
  }
}

inline void MaybeRaiseRegisteredFromTFStatus(TF_Status* status) {
  TF_Code code = TF_GetCode(status);
  if (code != TF_OK) {
    SetRegisteredErrFromTFStatus(status);
    throw pybind11::error_already_set();
  }
}

inline void MaybeRaiseRegisteredFromTFStatusWithGIL(TF_Status* status) {
  TF_Code code = TF_GetCode(status);
  if (code != TF_OK) {
    // Acquire GIL for throwing exception.
    pybind11::gil_scoped_acquire acquire;
    SetRegisteredErrFromTFStatus(status);
    throw pybind11::error_already_set();
  }
}

}  // namespace tensorflow

namespace pybind11 {
namespace detail {

// Raise an exception if a given status is not OK, otherwise return None.
//
// The correspondence between status codes and exception classes is given
// by PyExceptionRegistry. Note that the registry should be initialized
// in order to be used, see PyExceptionRegistry::Init.
template <>
struct type_caster<tensorflow::Status> {
 public:
  PYBIND11_TYPE_CASTER(tensorflow::Status, _("Status"));
  static handle cast(tensorflow::Status status, return_value_policy, handle) {
    tensorflow::MaybeRaiseFromStatus(status);
    return none().inc_ref();
  }
};

}  // namespace detail
}  // namespace pybind11

#endif  // TENSORFLOW_PYTHON_LIB_CORE_PYBIND11_STATUS_H_

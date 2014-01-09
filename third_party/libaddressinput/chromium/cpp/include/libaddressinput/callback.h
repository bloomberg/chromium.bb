// Copyright (C) 2013 Google Inc.
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

#ifndef I18N_ADDRESSINPUT_CALLBACK_H_
#define I18N_ADDRESSINPUT_CALLBACK_H_

#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <string>

namespace i18n {
namespace addressinput {

// Stores a pointer to a method in an object. Sample usage:
//    class MyClass {
//     public:
//      typedef Callback<std::string, std::string> MyConstRefCallback;
//      typedef ScopdedPtrCallback<std::string, MyDataType> MyScopedPtrCallback;
//
//      void GetStringAsynchronously() {
//        scoped_ptr<MyCallback> callback(BuildCallback(
//            this, &MyClass::OnStringReady));
//        bool success = ...
//        std::string key = ...
//        std::string data = ...
//        (*callback)(success, key, data);
//      }
//
//      void GetDataAsynchronously() {
//        scoped_ptr<MyScopedPtrCallback> callback(BuildScopedPtrCallback(
//            this, &MyClass::OnDataReady));
//        bool success = ...
//        std::string key = ...
//        scoped_ptr<MyDataType> data = ...
//        (*callback)(success, key, data.Pass());
//      }
//
//      void OnStringReady(bool success,
//                         const std::string& key,
//                         const std::string& data) {
//        ...
//      }
//
//      void OnDataReady(bool success,
//                       const std::string& key,
//                       scoped_ptr<MyDataType> data) {
//        ...
//      }
//    };
template <typename RequestType, typename ResponseType>
class Callback {
 public:
  virtual ~Callback() {}

  virtual void operator()(bool success,
                          const RequestType& request,
                          const ResponseType& response) const = 0;
};

template <typename RequestType, typename ResponseType>
class ScopedPtrCallback {
 public:
  virtual ~ScopedPtrCallback() {}

  virtual void operator()(bool success,
                          const RequestType& request,
                          scoped_ptr<ResponseType> response) const = 0;
};

namespace {

template <typename BaseType, typename RequestType, typename ResponseType>
class CallbackImpl : public Callback<RequestType, ResponseType> {
 public:
  typedef void (BaseType::*Method)(
      bool, const RequestType&, const ResponseType&);

  CallbackImpl(BaseType* instance, Method method)
      : instance_(instance),
        method_(method) {
    assert(instance_ != NULL);
    assert(method_ != NULL);
  }

  virtual ~CallbackImpl() {}

  // Callback implementation.
  virtual void operator()(bool success,
                          const RequestType& request,
                          const ResponseType& response) const {
    (instance_->*method_)(success, request, response);
  }

 private:
  BaseType* instance_;
  Method method_;
};

template <typename BaseType, typename RequestType, typename ResponseType>
class ScopedPtrCallbackImpl :
    public ScopedPtrCallback<RequestType, ResponseType> {
 public:
  typedef void (BaseType::*Method)(
      bool, const RequestType&, scoped_ptr<ResponseType>);

  ScopedPtrCallbackImpl(BaseType* instance, Method method)
      : instance_(instance),
        method_(method) {
    assert(instance_ != NULL);
    assert(method_ != NULL);
  }

  virtual ~ScopedPtrCallbackImpl() {}

  // ScopedPtrCallback implementation.
  virtual void operator()(bool success,
                          const RequestType& request,
                          scoped_ptr<ResponseType> response) const {
    (instance_->*method_)(success, request, response.Pass());
  }

 private:
  BaseType* instance_;
  Method method_;
};

}  // namespace

// Returns a callback to |instance->method| with constant reference to data.
template <typename BaseType, typename RequestType, typename ResponseType>
scoped_ptr<Callback<RequestType, ResponseType> > BuildCallback(
    BaseType* instance,
    void (BaseType::*method)(bool, const RequestType&, const ResponseType&)) {
  return scoped_ptr<Callback<RequestType, ResponseType> >(
      new CallbackImpl<BaseType, RequestType, ResponseType>(instance, method));
}

// Returns a callback to |instance->method| with scoped pointer to data.
template <typename BaseType, typename RequestType, typename ResponseType>
scoped_ptr<ScopedPtrCallback<RequestType, ResponseType> >
BuildScopedPtrCallback(
    BaseType* instance,
    void (BaseType::*method)(
        bool, const RequestType&, scoped_ptr<ResponseType>)) {
  return scoped_ptr<ScopedPtrCallback<RequestType, ResponseType> >(
      new ScopedPtrCallbackImpl<BaseType, RequestType, ResponseType>(
          instance, method));
}

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_CALLBACK_H_

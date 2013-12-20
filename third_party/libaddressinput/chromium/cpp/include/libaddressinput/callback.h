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

namespace i18n {
namespace addressinput {

// Stores a pointer to a method in an object. Sample usage:
//    class MyClass {
//     public:
//      typedef Callback<MyKeyType, MyDataType> MyCallback;
//
//      void GetDataAsynchronously() {
//        scoped_ptr<MyCallback> callback(BuildCallback(
//            this, &MyClass::OnDataReady));
//        bool success = ...
//        MyKeyType key = ...
//        MyDataType data = ...
//        (*callback)(success, key, data);
//      }
//
//      void OnDataReady(bool success,
//                       const MyKeyType& key,
//                       const MyDataType& data) {
//        ...
//      }
//    };
template <typename Param1, typename Param2>
class Callback {
 public:
  virtual ~Callback() {}

  virtual void operator()(bool success,
                          const Param1& param1,
                          const Param2& param2) const = 0;
};

namespace {

template <typename BaseType, typename Param1, typename Param2>
class CallbackImpl : public Callback<Param1, Param2> {
 public:
  typedef void (BaseType::*Method)(bool, const Param1&, const Param2&);

  CallbackImpl(BaseType* instance, Method method)
      : instance_(instance),
        method_(method) {
    assert(instance_ != NULL);
    assert(method_ != NULL);
  }

  virtual ~CallbackImpl() {}

  virtual void operator()(bool success,
                          const Param1& param1,
                          const Param2& param2) const {
    (instance_->*method_)(success, param1, param2);
  }

 private:
  BaseType* instance_;
  Method method_;
};

}  // namespace

// Returns a callback to |instance->method|.
template <typename BaseType, typename Param1, typename Param2>
scoped_ptr<Callback<Param1, Param2> > BuildCallback(
    BaseType* instance,
    void (BaseType::*method)(bool, const Param1&, const Param2&)) {
  return scoped_ptr<Callback<Param1, Param2> >(
      new CallbackImpl<BaseType, Param1, Param2>(instance, method));
}

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_CALLBACK_H_

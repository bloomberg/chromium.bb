// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// There is deliberately no header guard here. This file is included multiple
// times, once for each dispatcher specialiation arity. Do not include this
// file directly. Include np_dispatcher.h instead.

template <typename NPObjectType PARAM_TYPENAMES>
class NPDispatcher<NPObjectType, void(PARAM_TYPES)>
    : public BaseNPDispatcher {
  typedef void (NPObjectType::*FunctionType)(PARAM_TYPES);
 public:
  NPDispatcher(BaseNPDispatcher* next,
               const NPUTF8* name,
               FunctionType function)
      : BaseNPDispatcher(next, name),
        function_(function) {
  }

  virtual bool Invoke(NPObject* object,
                      const NPVariant* args,
                      uint32_t num_args,
                      NPVariant* result) {
    VOID_TO_NPVARIANT(*result);

    if (num_args != NUM_PARAMS)
      return false;

    PARAM_TO_NVPARIANT_CONVERSIONS

    (static_cast<NPObjectType*>(object)->*function_)(PARAM_NAMES);
    return true;
  }

  virtual int num_args() const {
    return NUM_PARAMS;
  }

 private:
  FunctionType function_;
};

template <typename NPObjectType, typename R PARAM_TYPENAMES>
class NPDispatcher<NPObjectType, R(PARAM_TYPES)>
    : public BaseNPDispatcher {
  typedef R (NPObjectType::*FunctionType)(PARAM_TYPES);
 public:
  NPDispatcher(BaseNPDispatcher* next,
               const NPUTF8* name,
               FunctionType function)
      : BaseNPDispatcher(next, name),
        function_(function) {
  }

  virtual bool Invoke(NPObject* object,
                      const NPVariant* args,
                      uint32_t num_args,
                      NPVariant* result) {
    VOID_TO_NPVARIANT(*result);

    if (num_args != NUM_PARAMS)
      return false;

    PARAM_TO_NVPARIANT_CONVERSIONS

    ValueToNPVariant(
        (static_cast<NPObjectType*>(object)->*function_)(PARAM_NAMES), result);
    return true;
  }

  virtual int num_args() const {
    return NUM_PARAMS;
  }

 private:
  FunctionType function_;
};

#undef NUM_PARAMS
#undef PARAM_TYPENAMES
#undef PARAM_TYPES
#undef PARAM_NAMES
#undef PARAM_DECLS
#undef PARAM_TO_NVPARIANT_CONVERSIONS

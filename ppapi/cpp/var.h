// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VAR_H_
#define PPAPI_CPP_VAR_H_

#include <string>
#include <vector>

#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"

namespace pp {

class Instance;

#ifndef PPAPI_VAR_REMOVE_SCRIPTING
namespace deprecated {
class ScriptableObject;
}
#endif

class Var {
 public:
  struct Null {};  // Special value passed to constructor to make NULL.

  Var();  // PP_Var of type Undefined.
  Var(Null);  // PP_Var of type Null.
  Var(bool b);
  Var(int32_t i);
  Var(double d);
  Var(const char* utf8_str);  // Must be encoded in UTF-8.
  Var(const std::string& utf8_str);  // Must be encoded in UTF-8.

  // This magic constructor is used when we've gotten a PP_Var as a return
  // value that has already been addref'ed for us.
  struct PassRef {};
  Var(PassRef, PP_Var var) {
    var_ = var;
    needs_release_ = true;
  }

  // TODO(brettw): remove DontManage when this bug is fixed
  //               http://code.google.com/p/chromium/issues/detail?id=52105
  // This magic constructor is used when we've given a PP_Var as an input
  // argument from somewhere and that reference is managing the reference
  // count for us. The object will not be AddRef'ed or Release'd by this
  // class instance..
  struct DontManage {};
  Var(DontManage, PP_Var var) {
    var_ = var;
    needs_release_ = false;
  }

  Var(const Var& other);

  virtual ~Var();

  Var& operator=(const Var& other);

  // For objects, dictionaries, and arrays, this operator compares object
  // identity rather than value identity.
  bool operator==(const Var& other) const;

  bool is_undefined() const { return var_.type == PP_VARTYPE_UNDEFINED; }
  bool is_null() const { return var_.type == PP_VARTYPE_NULL; }
  bool is_bool() const { return var_.type == PP_VARTYPE_BOOL; }
  bool is_string() const { return var_.type == PP_VARTYPE_STRING; }
  bool is_object() const { return var_.type == PP_VARTYPE_OBJECT; }

  // IsInt and IsDouble return the internal representation. The JavaScript
  // runtime may convert between the two as needed, so the distinction may
  // not be relevant in all cases (int is really an optimization inside the
  // runtime). So most of the time, you will want to check IsNumber.
  bool is_int() const { return var_.type == PP_VARTYPE_INT32; }
  bool is_double() const { return var_.type == PP_VARTYPE_DOUBLE; }
  bool is_number() const {
    return var_.type == PP_VARTYPE_INT32 ||
           var_.type == PP_VARTYPE_DOUBLE;
  }

  // Assumes the internal representation IsBool. If it's not, it will assert
  // in debug mode, and return false.
  bool AsBool() const;

  // AsInt and AsDouble implicitly convert between ints and doubles. This is
  // because JavaScript doesn't have a concept of ints and doubles, only
  // numbers. The distinction between the two is an optimization inside the
  // compiler. Since converting from a double to an int may be lossy, if you
  // care about the distinction, either always work in doubles, or check
  // !IsDouble() before calling AsInt().
  //
  // These functions will assert in debug mode and return 0 if the internal
  // representation is not IsNumber().
  int32_t AsInt() const;
  double AsDouble() const;

  // This assumes the object is of type string. If it's not, it will assert
  // in debug mode, and return an empty string.
  std::string AsString() const;

#ifndef PPAPI_VAR_REMOVE_SCRIPTING
  // Takes ownership of the given pointer.
  Var(Instance* instance, deprecated::ScriptableObject* object);

  // This assumes the object is of type object. If it's not, it will assert in
  // debug mode. If it is not an object or not a ScriptableObject type, returns
  // NULL.
  deprecated::ScriptableObject* AsScriptableObject() const;

  bool HasProperty(const Var& name, Var* exception = NULL) const;
  bool HasMethod(const Var& name, Var* exception = NULL) const;
  Var GetProperty(const Var& name, Var* exception = NULL) const;
  void GetAllPropertyNames(std::vector<Var>* properties,
                           Var* exception = NULL) const;
  void SetProperty(const Var& name, const Var& value, Var* exception = NULL);
  void RemoveProperty(const Var& name, Var* exception = NULL);
  Var Call(const Var& method_name, uint32_t argc, Var* argv,
           Var* exception = NULL);
  Var Construct(uint32_t argc, Var* argv, Var* exception = NULL) const;

  // Convenience functions for calling functions with small # of args.
  Var Call(const Var& method_name, Var* exception = NULL);
  Var Call(const Var& method_name, const Var& arg1, Var* exception = NULL);
  Var Call(const Var& method_name, const Var& arg1, const Var& arg2,
           Var* exception = NULL);
  Var Call(const Var& method_name, const Var& arg1, const Var& arg2,
           const Var& arg3, Var* exception = NULL);
  Var Call(const Var& method_name, const Var& arg1, const Var& arg2,
           const Var& arg3, const Var& arg4, Var* exception = NULL);
#endif  // ifndef PPAPI_VAR_REMOVE_SCRIPTING

  // Returns a const reference to the PP_Var managed by this Var object.
  const PP_Var& pp_var() const {
    return var_;
  }

  // Detaches from the internal PP_Var of this object, keeping the reference
  // count the same. This is used when returning a PP_Var from an API function
  // where the caller expects the return value to be AddRef'ed for it.
  PP_Var Detach() {
    PP_Var ret = var_;
    var_ = PP_MakeUndefined();
    needs_release_ = false;
    return ret;
  }

  // Prints a short description "Var<X>" that can be used for logging, where
  // "X" is the underlying scalar or "UNDEFINED" or "OBJ" as it does not call
  // into the browser to get the object description.
  std::string DebugString() const;

  // For use when calling the raw C PPAPI when using the C++ Var as a possibly
  // NULL exception. This will handle getting the address of the internal value
  // out if it's non-NULL and fixing up the reference count.
  //
  // Danger: this will only work for things with exception semantics, i.e. that
  // the value will not be changed if it's a non-undefined exception. Otherwise,
  // this class will mess up the refcounting.
  //
  // This is a bit subtle:
  // - If NULL is passed, we return NULL from get() and do nothing.
  //
  // - If a undefined value is passed, we return the address of a undefined var
  //   from get and have the output value take ownership of that var.
  //
  // - If a non-undefined value is passed, we return the address of that var
  //   from get, and nothing else should change.
  //
  // Example:
  //   void FooBar(a, b, Var* exception = NULL) {
  //     foo_interface->Bar(a, b, Var::OutException(exception).get());
  //   }
  class OutException {
   public:
    OutException(Var* v)
        : output_(v),
          originally_had_exception_(v && v->is_null()) {
      if (output_) {
        temp_ = output_->var_;
      } else {
        temp_.padding = 0;
        temp_.type = PP_VARTYPE_UNDEFINED;
      }
    }
    ~OutException() {
      if (output_ && !originally_had_exception_)
        *output_ = Var(PassRef(), temp_);
    }

    PP_Var* get() {
      if (output_)
        return &temp_;
      return NULL;
    }

   private:
    Var* output_;
    bool originally_had_exception_;
    PP_Var temp_;
  };

 protected:
  PP_Var var_;
  bool needs_release_;

 private:
  // Prevent an arbitrary pointer argument from being implicitly converted to
  // a bool at Var construction. If somebody makes such a mistake, (s)he will
  // get a compilation error.
  Var(void* non_scriptable_object_pointer);

};

}  // namespace pp

#endif  // PPAPI_CPP_VAR_H_

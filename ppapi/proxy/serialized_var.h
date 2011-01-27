// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_SERIALIZED_VAR_H_
#define PPAPI_PROXY_SERIALIZED_VAR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "ppapi/c/pp_var.h"

namespace IPC {
class Message;
}

namespace pp {
namespace proxy {

class Dispatcher;
class VarSerializationRules;

// This class encapsulates a var so that we can serialize and deserialize it
// The problem is that for strings, serialization and deserialization requires
// knowledge from outside about how to get at or create a string. So this
// object groups the var with a dispatcher so that string values can be set or
// gotten.
//
// Declare IPC messages as using this type, but don't use it directly (it has
// no useful public methods). Instead, instantiate one of the helper classes
// below which are conveniently named for each use case to prevent screwups.
//
// Design background
// -----------------
// This is sadly super complicated. The IPC system needs a consistent type to
// use for sending and receiving vars (this is a SerializedVar). But there are
// different combinations of reference counting for sending and receiving
// objects and for dealing with strings
//
// This makes SerializedVar complicated and easy to mess up. To make it
// reasonable to use all functions are protected and there are a use-specific
// classes that encapsulate exactly one type of use in a way that typically
// won't compile if you do the wrong thing.
//
// The IPC system is designed to pass things around and will make copies in
// some cases, so our system must be designed so that this stuff will work.
// This is challenging when the SerializedVar must to some cleanup after the
// message is sent. To work around this, we create an inner class using a
// linked_ptr so all copies of a SerializedVar can share and we can guarantee
// that the actual data will get cleaned up on shutdown.
//
// Constness
// ---------
// SerializedVar basically doesn't support const. Everything is mutable and
// most functions are declared const. This unfortunateness is because of the
// way the IPC system works. When deserializing, it will have a const
// SerializedVar in a Tuple and this will be given to the function. We kind of
// want to modify that to convert strings and do refcounting.
//
// The helper classes used for accessing the SerializedVar have more reasonable
// behavior and will enforce that you don't do stupid things.
class SerializedVar {
 public:
  SerializedVar();
  ~SerializedVar();

  // Backend implementation for IPC::ParamTraits<SerializedVar>.
  void WriteToMessage(IPC::Message* m) const {
    inner_->WriteToMessage(m);
  }
  bool ReadFromMessage(const IPC::Message* m, void** iter) {
    return inner_->ReadFromMessage(m, iter);
  }

 protected:
  friend class SerializedVarReceiveInput;
  friend class SerializedVarReturnValue;
  friend class SerializedVarOutParam;
  friend class SerializedVarSendInput;
  friend class SerializedVarTestConstructor;
  friend class SerializedVarVectorReceiveInput;

  class Inner : public base::RefCounted<Inner> {
   public:
    Inner();
    Inner(VarSerializationRules* serialization_rules);
    Inner(VarSerializationRules* serialization_rules, const PP_Var& var);
    ~Inner();

    VarSerializationRules* serialization_rules() {
      return serialization_rules_;
    }
    void set_serialization_rules(VarSerializationRules* serialization_rules) {
      serialization_rules_ = serialization_rules;
    }

    // See outer class's declarations above.
    PP_Var GetVar() const;
    PP_Var GetIncompleteVar() const;
    void SetVar(PP_Var var);
    const std::string& GetString() const;
    std::string* GetStringPtr();

    void WriteToMessage(IPC::Message* m) const;
    bool ReadFromMessage(const IPC::Message* m, void** iter);

    // Sets the cleanup mode. See the CleanupMode enum below. These functions
    // are not just a simple setter in order to require that the appropriate
    // data is set along with the corresponding mode.
    void SetCleanupModeToEndSendPassRef(Dispatcher* dispatcher);
    void SetCleanupModeToEndReceiveCallerOwned();

   private:
    enum CleanupMode {
      // The serialized var won't do anything special in the destructor
      // (default).
      CLEANUP_NONE,

      // The serialized var will call EndSendPassRef in the destructor.
      END_SEND_PASS_REF,

      // The serialized var will call EndReceiveCallerOwned in the destructor.
      END_RECEIVE_CALLER_OWNED
    };

    // Rules for serializing and deserializing vars for this process type.
    // This may be NULL, but must be set before trying to serialize to IPC when
    // sending, or before converting back to a PP_Var when receiving.
    VarSerializationRules* serialization_rules_;

    // If this is set to VARTYPE_STRING and the 'value.id' is 0, then the
    // string_value_ contains the string. This means that the caller hasn't
    // called Deserialize with a valid Dispatcher yet, which is how we can
    // convert the serialized string value to a PP_Var string ID.
    //
    // This var may not be complete until the serialization rules are set when
    // reading from IPC since we'll need that to convert the string_value to
    // a string ID. Before this, the as_id will be 0 for VARTYPE_STRING.
    PP_Var var_;

    // Holds the literal string value to/from IPC. This will be valid of the
    // var_ is VARTYPE_STRING.
    std::string string_value_;

    CleanupMode cleanup_mode_;

    // The dispatcher saved for the call to EndSendPassRef for the cleanup.
    // This is only valid when cleanup_mode == END_SEND_PASS_REF.
    Dispatcher* dispatcher_for_end_send_pass_ref_;

#ifndef NDEBUG
    // When being sent or received over IPC, we should only be serialized or
    // deserialized once. These flags help us assert this is true.
    mutable bool has_been_serialized_;
    mutable bool has_been_deserialized_;
#endif

    DISALLOW_COPY_AND_ASSIGN(Inner);
  };

  SerializedVar(VarSerializationRules* serialization_rules);
  SerializedVar(VarSerializationRules* serialization, const PP_Var& var);

  mutable scoped_refptr<Inner> inner_;
};

// Helpers for message sending side --------------------------------------------

// For sending a value to the remote side.
//
// Example for API:
//   void MyFunction(PP_Var)
// IPC message:
//   IPC_MESSAGE_ROUTED1(MyFunction, SerializedVar);
// Sender would be:
//   void MyFunctionProxy(PP_Var param) {
//     Send(new MyFunctionMsg(SerializedVarSendInput(param));
//   }
class SerializedVarSendInput : public SerializedVar {
 public:
  SerializedVarSendInput(Dispatcher* dispatcher, const PP_Var& var);

  // Helper function for serializing a vector of input vars for serialization.
  static void ConvertVector(Dispatcher* dispatcher,
                            const PP_Var* input,
                            size_t input_count,
                            std::vector<SerializedVar>* output);

 private:
  // Disallow the empty constructor, but keep the default copy constructor
  // which is required to send the object to the IPC system.
  SerializedVarSendInput();
};

// For the calling side of a function returning a var. The sending side uses
// SerializedVarReturnValue.
//
// Example for API:
//   PP_Var MyFunction()
// IPC message:
//   IPC_SYNC_MESSAGE_ROUTED0_1(MyFunction, SerializedVar);
// Message handler would be:
//   PP_Var MyFunctionProxy() {
//     ReceiveSerializedVarReturnValue result;
//     Send(new MyFunctionMsg(&result));
//     return result.Return(dispatcher());
//   }
class ReceiveSerializedVarReturnValue : public SerializedVar {
 public:
  // Note that we can't set the dispatcher in the constructor because the
  // data will be overridden when the return value is set.
  ReceiveSerializedVarReturnValue();

  PP_Var Return(Dispatcher* dispatcher);

 private:
  DISALLOW_COPY_AND_ASSIGN(ReceiveSerializedVarReturnValue);
};

// Example for API:
//   "void MyFunction(PP_Var* exception);"
// IPC message:
//   IPC_SYNC_MESSAGE_ROUTED0_1(MyFunction, SerializedVar);
// Message handler would be:
//   void OnMsgMyFunction(PP_Var* exception) {
//     ReceiveSerializedException se(dispatcher(), exception)
//     Send(new PpapiHostMsg_Foo(&se));
//   }
class ReceiveSerializedException : public SerializedVar {
 public:
  ReceiveSerializedException(Dispatcher* dispatcher, PP_Var* exception);
  ~ReceiveSerializedException();

  // Returns true if the exception passed in the constructor is set. Check
  // this before actually issuing the IPC.
  bool IsThrown() const;

 private:
  Dispatcher* dispatcher_;

  // The input/output exception we're wrapping. May be NULL.
  PP_Var* exception_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReceiveSerializedException);
};

// Helper class for when we're returning a vector of Vars. When it goes out
// of scope it will automatically convert the vector filled by the IPC layer
// into the array specified by the constructor params.
//
// Example for API:
//   "void MyFunction(uint32_t* count, PP_Var** vars);"
// IPC message:
//   IPC_SYNC_MESSAGE_ROUTED0_1(MyFunction, std::vector<SerializedVar>);
// Proxy function:
//   void MyFunction(uint32_t* count, PP_Var** vars) {
//     ReceiveSerializedVarVectorOutParam vect(dispatcher, count, vars);
//     Send(new MyMsg(vect.OutParam()));
//   }
class ReceiveSerializedVarVectorOutParam {
 public:
  ReceiveSerializedVarVectorOutParam(Dispatcher* dispatcher,
                                     uint32_t* output_count,
                                     PP_Var** output);
  ~ReceiveSerializedVarVectorOutParam();

  std::vector<SerializedVar>* OutParam();

 private:
  Dispatcher* dispatcher_;
  uint32_t* output_count_;
  PP_Var** output_;

  std::vector<SerializedVar> vector_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReceiveSerializedVarVectorOutParam);
};

// Helpers for message receiving side ------------------------------------------

// For receiving a value from the remote side.
//
// Example for API:
//   void MyFunction(PP_Var)
// IPC message:
//   IPC_MESSAGE_ROUTED1(MyFunction, SerializedVar);
// Message handler would be:
//   void OnMsgMyFunction(SerializedVarReceiveInput param) {
//     MyFunction(param.Get());
//   }
class SerializedVarReceiveInput {
 public:
  // We rely on the implicit constructor here since the IPC layer will call
  // us with a SerializedVar. Pass this object by value, the copy constructor
  // will pass along the pointer (as cheap as passing a pointer arg).
  SerializedVarReceiveInput(const SerializedVar& serialized);
  ~SerializedVarReceiveInput();

  PP_Var Get(Dispatcher* dispatcher);

 private:
  const SerializedVar& serialized_;

  // Since the SerializedVar is const, we can't set its dispatcher (which is
  // OK since we don't need to). But since we need it for our own uses, we
  // track it here. Will be NULL before Get() is called.
  Dispatcher* dispatcher_;
  PP_Var var_;
};

// For receiving an input vector of vars from the remote side.
//
// Example:
//   OnMsgMyFunction(SerializedVarVectorReceiveInput vector) {
//     uint32_t size;
//     PP_Var* array = vector.Get(dispatcher, &size);
//     MyFunction(size, array);
//   }
class SerializedVarVectorReceiveInput {
 public:
  SerializedVarVectorReceiveInput(const std::vector<SerializedVar>& serialized);
  ~SerializedVarVectorReceiveInput();

  // Only call Get() once. It will return a pointer to the converted array and
  // place the array size in the out param. Will return NULL when the array is
  // empty.
  PP_Var* Get(Dispatcher* dispatcher, uint32_t* array_size);

 private:
  const std::vector<SerializedVar>& serialized_;

  // Filled by Get().
  std::vector<PP_Var> deserialized_;
};

// For the receiving side of a function returning a var. The calling side uses
// ReceiveSerializedVarReturnValue.
//
// Example for API:
//   PP_Var MyFunction()
// IPC message:
//   IPC_SYNC_MESSAGE_ROUTED0_1(MyFunction, SerializedVar);
// Message handler would be:
//   void OnMsgMyFunction(SerializedVarReturnValue result) {
//     result.Return(dispatcher(), MyFunction());
//   }
class SerializedVarReturnValue {
 public:
  // We rely on the implicit constructor here since the IPC layer will call
  // us with a SerializedVar*. Pass this object by value, the copy constructor
  // will pass along the pointer (as cheap as passing a pointer arg).
  SerializedVarReturnValue(SerializedVar* serialized);

  void Return(Dispatcher* dispatcher, const PP_Var& var);

 private:
  SerializedVar* serialized_;
};

// For writing an out param to the remote side.
//
// Example for API:
//   "void MyFunction(PP_Var* out);"
// IPC message:
//   IPC_SYNC_MESSAGE_ROUTED0_1(MyFunction, SerializedVar);
// Message handler would be:
//   void OnMsgMyFunction(SerializedVarOutParam out_param) {
//     MyFunction(out_param.OutParam(dispatcher()));
//   }
class SerializedVarOutParam {
 public:
  // We rely on the implicit constructor here since the IPC layer will call
  // us with a SerializedVar*. Pass this object by value, the copy constructor
  // will pass along the pointer (as cheap as passing a pointer arg).
  SerializedVarOutParam(SerializedVar* serialized);
  ~SerializedVarOutParam();

  // Call this function only once. The caller should write its result to the
  // returned var pointer before this class goes out of scope. The var's
  // initial value will be VARTYPE_UNDEFINED.
  PP_Var* OutParam(Dispatcher* dispatcher);

 private:
  SerializedVar* serialized_;

  // This is the value actually written by the code and returned by OutParam.
  // We'll write this into serialized_ in our destructor.
  PP_Var writable_var_;

  Dispatcher* dispatcher_;
};

// For returning an array of PP_Vars to the other side and transferring
// ownership.
//
class SerializedVarVectorOutParam {
 public:
  SerializedVarVectorOutParam(std::vector<SerializedVar>* serialized);
  ~SerializedVarVectorOutParam();

  uint32_t* CountOutParam() { return &count_; }
  PP_Var** ArrayOutParam(Dispatcher* dispatcher);

 private:
  Dispatcher* dispatcher_;
  std::vector<SerializedVar>* serialized_;

  uint32_t count_;
  PP_Var* array_;
};

// For tests that just want to construct a SerializedVar for giving it to one
// of the other classes.
class SerializedVarTestConstructor : public SerializedVar {
 public:
  // For POD-types and objects.
  explicit SerializedVarTestConstructor(const PP_Var& pod_var);

  // For strings.
  explicit SerializedVarTestConstructor(const std::string& str);
};

// For tests that want to read what's in a SerializedVar.
class SerializedVarTestReader : public SerializedVar {
 public:
  explicit SerializedVarTestReader(const SerializedVar& var);

  // The "incomplete" var is the one sent over the wire. Strings and object
  // IDs have not yet been converted, so this is the thing that tests will
  // actually want to check.
  PP_Var GetIncompleteVar() const { return inner_->GetIncompleteVar(); }

  const std::string& GetString() const { return inner_->GetString(); }
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_SERIALIZED_VAR_H_


// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/serialized_var.h"

#include "base/logging.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_param_traits.h"
#include "ppapi/proxy/var_serialization_rules.h"

namespace pp {
namespace proxy {

// SerializedVar::Inner --------------------------------------------------------

SerializedVar::Inner::Inner()
    : serialization_rules_(NULL),
      var_(PP_MakeUndefined()),
      cleanup_mode_(CLEANUP_NONE),
      dispatcher_for_end_send_pass_ref_(NULL) {
#ifndef NDEBUG
  has_been_serialized_ = false;
  has_been_deserialized_ = false;
#endif
}

SerializedVar::Inner::Inner(VarSerializationRules* serialization_rules)
    : serialization_rules_(serialization_rules),
      var_(PP_MakeUndefined()),
      cleanup_mode_(CLEANUP_NONE),
      dispatcher_for_end_send_pass_ref_(NULL) {
#ifndef NDEBUG
  has_been_serialized_ = false;
  has_been_deserialized_ = false;
#endif
}

SerializedVar::Inner::Inner(VarSerializationRules* serialization_rules,
                            const PP_Var& var)
    : serialization_rules_(serialization_rules),
      var_(var),
      cleanup_mode_(CLEANUP_NONE),
      dispatcher_for_end_send_pass_ref_(NULL) {
#ifndef NDEBUG
  has_been_serialized_ = false;
  has_been_deserialized_ = false;
#endif
}

SerializedVar::Inner::~Inner() {
  switch (cleanup_mode_) {
    case END_SEND_PASS_REF:
      DCHECK(dispatcher_for_end_send_pass_ref_);
      serialization_rules_->EndSendPassRef(var_,
                                           dispatcher_for_end_send_pass_ref_);
      break;
    case END_RECEIVE_CALLER_OWNED:
      serialization_rules_->EndReceiveCallerOwned(var_);
      break;
    default:
      break;
  }
}

PP_Var SerializedVar::Inner::GetVar() const {
  DCHECK(serialization_rules_);

  // If we're a string var, we should have already converted the string value
  // to a var ID.
  DCHECK(var_.type != PP_VARTYPE_STRING || var_.value.as_id != 0);
  return var_;
}

PP_Var SerializedVar::Inner::GetIncompleteVar() const {
  DCHECK(serialization_rules_);
  return var_;
}

void SerializedVar::Inner::SetVar(PP_Var var) {
  // Sanity check, when updating the var we should have received a
  // serialization rules pointer already.
  DCHECK(serialization_rules_);
  var_ = var;
}

const std::string& SerializedVar::Inner::GetString() const {
  DCHECK(serialization_rules_);
  return string_value_;
}

std::string* SerializedVar::Inner::GetStringPtr() {
  DCHECK(serialization_rules_);
  return &string_value_;
}

void SerializedVar::Inner::WriteToMessage(IPC::Message* m) const {
  // When writing to the IPC messages, a serization rules handler should
  // always have been set.
  //
  // When sending a message, it should be difficult to trigger this if you're
  // using the SerializedVarSendInput class and giving a non-NULL dispatcher.
  // Make sure you're using the proper "Send" helper class.
  //
  // It should be more common to see this when handling an incoming message
  // that returns a var. This means the message handler didn't write to the
  // output parameter, or possibly you used the wrong helper class
  // (normally SerializedVarReturnValue).
  DCHECK(serialization_rules_);

#ifndef NDEBUG
  // We should only be serializing something once.
  DCHECK(!has_been_serialized_);
  has_been_serialized_ = true;
#endif

  // If the var is not a string type, we should not have ended up with any
  // string data.
  DCHECK(var_.type == PP_VARTYPE_STRING || string_value_.empty());

  m->WriteInt(static_cast<int>(var_.type));
  switch (var_.type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
      // These don't need any data associated with them other than the type we
      // just serialized.
      break;
    case PP_VARTYPE_BOOL:
      m->WriteBool(PPBoolToBool(var_.value.as_bool));
      break;
    case PP_VARTYPE_INT32:
      m->WriteInt(var_.value.as_int);
      break;
    case PP_VARTYPE_DOUBLE:
      IPC::ParamTraits<double>::Write(m, var_.value.as_double);
      break;
    case PP_VARTYPE_STRING:
      // TODO(brettw) in the case of an invalid string ID, it would be nice
      // to send something to the other side such that a 0 ID would be
      // generated there. Then the function implementing the interface can
      // handle the invalid string as if it was in process rather than seeing
      // what looks like a valid empty string.
      m->WriteString(string_value_);
      break;
    case PP_VARTYPE_OBJECT:
      m->WriteInt64(var_.value.as_id);
      break;
  }
}

bool SerializedVar::Inner::ReadFromMessage(const IPC::Message* m, void** iter) {
#ifndef NDEBUG
  // We should only deserialize something once or will end up with leaked
  // references.
  //
  // One place this has happened in the past is using
  // std::vector<SerializedVar>.resize(). If you're doing this manually instead
  // of using the helper classes for handling in/out vectors of vars, be
  // sure you use the same pattern as the SerializedVarVector classes.
  DCHECK(!has_been_deserialized_);
  has_been_deserialized_ = true;
#endif

  // When reading, the dispatcher should be set when we get a Deserialize
  // call (which will supply a dispatcher).
  int type;
  if (!m->ReadInt(iter, &type))
    return false;

  bool success = false;
  switch (type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
      // These don't have any data associated with them other than the type we
      // just serialized.
      success = true;
      break;
    case PP_VARTYPE_BOOL: {
      bool bool_value;
      success = m->ReadBool(iter, &bool_value);
      var_.value.as_bool = BoolToPPBool(bool_value);
      break;
    }
    case PP_VARTYPE_INT32:
      success = m->ReadInt(iter, &var_.value.as_int);
      break;
    case PP_VARTYPE_DOUBLE:
      success = IPC::ParamTraits<double>::Read(m, iter, &var_.value.as_double);
      break;
    case PP_VARTYPE_STRING:
      success = m->ReadString(iter, &string_value_);
      var_.value.as_id = 0;
      break;
    case PP_VARTYPE_OBJECT:
      success = m->ReadInt64(iter, &var_.value.as_id);
      break;
    default:
      // Leave success as false.
      break;
  }

  // All success cases get here. We avoid writing the type above so that the
  // output param is untouched (defaults to VARTYPE_UNDEFINED) even in the
  // failure case.
  if (success)
    var_.type = static_cast<PP_VarType>(type);
  return success;
}

void SerializedVar::Inner::SetCleanupModeToEndSendPassRef(
    Dispatcher* dispatcher) {
  DCHECK(dispatcher);
  DCHECK(!dispatcher_for_end_send_pass_ref_);
  dispatcher_for_end_send_pass_ref_ = dispatcher;
  cleanup_mode_ = END_SEND_PASS_REF;
}

void SerializedVar::Inner::SetCleanupModeToEndReceiveCallerOwned() {
  cleanup_mode_ = END_RECEIVE_CALLER_OWNED;
}

// SerializedVar ---------------------------------------------------------------

SerializedVar::SerializedVar() : inner_(new Inner) {
}

SerializedVar::SerializedVar(VarSerializationRules* serialization_rules)
    : inner_(new Inner(serialization_rules)) {
}

SerializedVar::SerializedVar(VarSerializationRules* serialization_rules,
                             const PP_Var& var)
    : inner_(new Inner(serialization_rules, var)) {
}

SerializedVar::~SerializedVar() {
}

// SerializedVarSendInput ------------------------------------------------------

SerializedVarSendInput::SerializedVarSendInput(Dispatcher* dispatcher,
                                               const PP_Var& var)
    : SerializedVar(dispatcher->serialization_rules()) {
  inner_->SetVar(dispatcher->serialization_rules()->SendCallerOwned(
      var, inner_->GetStringPtr()));
}

// static
void SerializedVarSendInput::ConvertVector(Dispatcher* dispatcher,
                                           const PP_Var* input,
                                           size_t input_count,
                                           std::vector<SerializedVar>* output) {
  output->resize(input_count);
  for (size_t i = 0; i < input_count; i++) {
    SerializedVar& cur = (*output)[i];
    cur = SerializedVar(dispatcher->serialization_rules());
    cur.inner_->SetVar(dispatcher->serialization_rules()->SendCallerOwned(
        input[i], cur.inner_->GetStringPtr()));
  }
}

// ReceiveSerializedVarReturnValue ---------------------------------------------

ReceiveSerializedVarReturnValue::ReceiveSerializedVarReturnValue() {
}

ReceiveSerializedVarReturnValue::ReceiveSerializedVarReturnValue(
    const SerializedVar& serialized)
    : SerializedVar(serialized) {
}

PP_Var ReceiveSerializedVarReturnValue::Return(Dispatcher* dispatcher) {
  inner_->set_serialization_rules(dispatcher->serialization_rules());
  inner_->SetVar(inner_->serialization_rules()->ReceivePassRef(
      inner_->GetIncompleteVar(), inner_->GetString(), dispatcher));
  return inner_->GetVar();
}

// ReceiveSerializedException --------------------------------------------------

ReceiveSerializedException::ReceiveSerializedException(Dispatcher* dispatcher,
                                                       PP_Var* exception)
    : SerializedVar(dispatcher->serialization_rules()),
      dispatcher_(dispatcher),
      exception_(exception) {
}

ReceiveSerializedException::~ReceiveSerializedException() {
  if (exception_) {
    // When an output exception is specified, it will take ownership of the
    // reference.
    inner_->SetVar(inner_->serialization_rules()->ReceivePassRef(
        inner_->GetIncompleteVar(), inner_->GetString(), dispatcher_));
    *exception_ = inner_->GetVar();
  } else {
    // When no output exception is specified, the browser thinks we have a ref
    // to an object that we don't want (this will happen only in the plugin
    // since the browser will always specify an out exception for the plugin to
    // write into).
    //
    // Strings don't need this handling since we can just avoid creating a
    // Var from the std::string in the first place.
    if (inner_->GetVar().type == PP_VARTYPE_OBJECT)
      inner_->serialization_rules()->ReleaseObjectRef(inner_->GetVar());
  }
}

bool ReceiveSerializedException::IsThrown() const {
  return exception_ && exception_->type != PP_VARTYPE_UNDEFINED;
}

// ReceiveSerializedVarVectorOutParam ------------------------------------------

ReceiveSerializedVarVectorOutParam::ReceiveSerializedVarVectorOutParam(
    Dispatcher* dispatcher,
    uint32_t* output_count,
    PP_Var** output)
    : dispatcher_(dispatcher),
      output_count_(output_count),
      output_(output) {
}

ReceiveSerializedVarVectorOutParam::~ReceiveSerializedVarVectorOutParam() {
  *output_count_ = static_cast<uint32_t>(vector_.size());
  if (!vector_.size()) {
    *output_ = NULL;
    return;
  }

  *output_ = static_cast<PP_Var*>(malloc(vector_.size() * sizeof(PP_Var)));
  for (size_t i = 0; i < vector_.size(); i++) {
    // Here we just mimic what happens when returning a value.
    ReceiveSerializedVarReturnValue converted;
    SerializedVar* serialized = &converted;
    *serialized = vector_[i];
    (*output_)[i] = converted.Return(dispatcher_);
  }
}

std::vector<SerializedVar>* ReceiveSerializedVarVectorOutParam::OutParam() {
  return &vector_;
}

// SerializedVarReceiveInput ---------------------------------------------------

SerializedVarReceiveInput::SerializedVarReceiveInput(
    const SerializedVar& serialized)
    : serialized_(serialized),
      dispatcher_(NULL),
      var_(PP_MakeUndefined()) {
}

SerializedVarReceiveInput::~SerializedVarReceiveInput() {
}

PP_Var SerializedVarReceiveInput::Get(Dispatcher* dispatcher) {
  serialized_.inner_->set_serialization_rules(
      dispatcher->serialization_rules());

  // Ensure that when the serialized var goes out of scope it cleans up the
  // stuff we're making in BeginReceiveCallerOwned.
  serialized_.inner_->SetCleanupModeToEndReceiveCallerOwned();

  serialized_.inner_->SetVar(
      serialized_.inner_->serialization_rules()->BeginReceiveCallerOwned(
          serialized_.inner_->GetIncompleteVar(),
          serialized_.inner_->GetStringPtr(),
          dispatcher));
  return serialized_.inner_->GetVar();
}

// SerializedVarVectorReceiveInput ---------------------------------------------

SerializedVarVectorReceiveInput::SerializedVarVectorReceiveInput(
    const std::vector<SerializedVar>& serialized)
    : serialized_(serialized) {
}

SerializedVarVectorReceiveInput::~SerializedVarVectorReceiveInput() {
  for (size_t i = 0; i < deserialized_.size(); i++) {
    serialized_[i].inner_->serialization_rules()->EndReceiveCallerOwned(
        deserialized_[i]);
  }
}

PP_Var* SerializedVarVectorReceiveInput::Get(Dispatcher* dispatcher,
                                             uint32_t* array_size) {
  deserialized_.resize(serialized_.size());
  for (size_t i = 0; i < serialized_.size(); i++) {
    // The vector must be able to clean themselves up after this call is
    // torn down.
    serialized_[i].inner_->set_serialization_rules(
        dispatcher->serialization_rules());

    serialized_[i].inner_->SetVar(
        serialized_[i].inner_->serialization_rules()->BeginReceiveCallerOwned(
            serialized_[i].inner_->GetIncompleteVar(),
            serialized_[i].inner_->GetStringPtr(),
            dispatcher));
    deserialized_[i] = serialized_[i].inner_->GetVar();
  }

  *array_size = static_cast<uint32_t>(serialized_.size());
  return deserialized_.empty() ? NULL : &deserialized_[0];
}

// SerializedVarReturnValue ----------------------------------------------------

SerializedVarReturnValue::SerializedVarReturnValue(SerializedVar* serialized)
    : serialized_(serialized) {
}

void SerializedVarReturnValue::Return(Dispatcher* dispatcher,
                                      const PP_Var& var) {
  serialized_->inner_->set_serialization_rules(
      dispatcher->serialization_rules());

  // Var must clean up after our BeginSendPassRef call.
  serialized_->inner_->SetCleanupModeToEndSendPassRef(dispatcher);

  serialized_->inner_->SetVar(
      dispatcher->serialization_rules()->BeginSendPassRef(
          var,
          serialized_->inner_->GetStringPtr()));
}

// static
SerializedVar SerializedVarReturnValue::Convert(Dispatcher* dispatcher,
                                                const PP_Var& var) {
  // Mimic what happens in the normal case.
  SerializedVar result;
  SerializedVarReturnValue retvalue(&result);
  retvalue.Return(dispatcher, var);
  return result;
}

// SerializedVarOutParam -------------------------------------------------------

SerializedVarOutParam::SerializedVarOutParam(SerializedVar* serialized)
    : serialized_(serialized),
      writable_var_(PP_MakeUndefined()),
      dispatcher_(NULL) {
}

SerializedVarOutParam::~SerializedVarOutParam() {
  if (serialized_->inner_->serialization_rules()) {
    // When unset, OutParam wasn't called. We'll just leave the var untouched
    // in that case.
    serialized_->inner_->SetVar(
        serialized_->inner_->serialization_rules()->BeginSendPassRef(
            writable_var_, serialized_->inner_->GetStringPtr()));

    // Normally the current object will be created on the stack to wrap a
    // SerializedVar and won't have a scope around the actual IPC send. So we
    // need to tell the SerializedVar to do the begin/end send pass ref calls.
    serialized_->inner_->SetCleanupModeToEndSendPassRef(dispatcher_);
  }
}

PP_Var* SerializedVarOutParam::OutParam(Dispatcher* dispatcher) {
  dispatcher_ = dispatcher;
  serialized_->inner_->set_serialization_rules(
      dispatcher->serialization_rules());
  return &writable_var_;
}

// SerializedVarVectorOutParam -------------------------------------------------

SerializedVarVectorOutParam::SerializedVarVectorOutParam(
    std::vector<SerializedVar>* serialized)
    : dispatcher_(NULL),
      serialized_(serialized),
      count_(0),
      array_(NULL) {
}

SerializedVarVectorOutParam::~SerializedVarVectorOutParam() {
  DCHECK(dispatcher_);

  // Convert the array written by the pepper code to the serialized structure.
  // Note we can't use resize here, we have to allocate a new SerializedVar
  // for each serialized item. See ParamTraits<vector<SerializedVar>>::Read.
  serialized_->reserve(count_);
  for (uint32_t i = 0; i < count_; i++) {
    // Just mimic what we do for regular OutParams.
    SerializedVar var;
    SerializedVarOutParam out(&var);
    *out.OutParam(dispatcher_) = array_[i];
    serialized_->push_back(var);
  }

  // When returning arrays, the pepper code expects the caller to take
  // ownership of the array.
  free(array_);
}

PP_Var** SerializedVarVectorOutParam::ArrayOutParam(Dispatcher* dispatcher) {
  DCHECK(!dispatcher_);  // Should only be called once.
  dispatcher_ = dispatcher;
  return &array_;
}

SerializedVarTestConstructor::SerializedVarTestConstructor(
    const PP_Var& pod_var) {
  DCHECK(pod_var.type != PP_VARTYPE_STRING);
  inner_->SetVar(pod_var);
}

SerializedVarTestConstructor::SerializedVarTestConstructor(
    const std::string& str) {
  PP_Var string_var;
  string_var.type = PP_VARTYPE_STRING;
  string_var.value.as_id = 0;
  inner_->SetVar(string_var);
  *inner_->GetStringPtr() = str;
}

SerializedVarTestReader::SerializedVarTestReader(const SerializedVar& var)
    : SerializedVar(var) {
}

}  // namespace proxy
}  // namespace pp


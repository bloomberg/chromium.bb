// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/var_value_conversions.h"

#include <limits>
#include <set>
#include <stack>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

namespace {

// In CreateValueFromVar(), a stack is used to keep track of conversion progress
// of array and dictionary vars. VarNode represents elements of that stack.
struct VarNode {
  VarNode(const PP_Var& in_var, base::Value* in_value)
      : var(in_var),
        value(in_value),
        sentinel(false) {
  }

  // This object doesn't hold a reference to it.
  PP_Var var;
  // It is not owned by this object.
  base::Value* value;
  // When this is set to true for a node in the stack, it means that we have
  // finished processing the node itself. However, we keep it in the stack as
  // a sentinel. When it becomes the top element of the stack again, we know
  // that we have processed all the descendants of this node.
  bool sentinel;
};

// In CreateVarFromValue(), a stack is used to keep track of conversion progress
// of list and dictionary values. ValueNode represents elements of that stack.
struct ValueNode {
  ValueNode(const PP_Var& in_var, const base::Value* in_value)
      : var(in_var),
        value(in_value) {
  }

  // This object doesn't hold a reference to it.
  PP_Var var;
  // It is not owned by this object.
  const base::Value* value;
};

// Helper function for CreateValueFromVar(). It only looks at |var| but not its
// descendants. The conversion result is stored in |value|. If |var| is array or
// dictionary, a new node is pushed onto |state|.
//
// Returns false on failure.
bool CreateValueFromVarHelper(const std::set<int64_t>& parent_ids,
                              const PP_Var& var,
                              scoped_ptr<base::Value>* value,
                              std::stack<VarNode>* state) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL: {
      value->reset(base::Value::CreateNullValue());
      return true;
    }
    case PP_VARTYPE_BOOL: {
      value->reset(new base::FundamentalValue(PP_ToBool(var.value.as_bool)));
      return true;
    }
    case PP_VARTYPE_INT32: {
      value->reset(new base::FundamentalValue(var.value.as_int));
      return true;
    }
    case PP_VARTYPE_DOUBLE: {
      value->reset(new base::FundamentalValue(var.value.as_double));
      return true;
    }
    case PP_VARTYPE_STRING: {
      StringVar* string_var = StringVar::FromPPVar(var);
      if (!string_var)
        return false;

      value->reset(new base::StringValue(string_var->value()));
      return true;
    }
    case PP_VARTYPE_OBJECT: {
      return false;
    }
    case PP_VARTYPE_ARRAY: {
      if (ContainsKey(parent_ids, var.value.as_id)) {
        // A circular reference is found.
        return false;
      }

      value->reset(new base::ListValue());
      state->push(VarNode(var, value->get()));
      return true;
    }
    case PP_VARTYPE_DICTIONARY: {
      if (ContainsKey(parent_ids, var.value.as_id)) {
        // A circular reference is found.
        return false;
      }

      value->reset(new base::DictionaryValue());
      state->push(VarNode(var, value->get()));
      return true;
    }
    case PP_VARTYPE_ARRAY_BUFFER: {
      ArrayBufferVar* array_buffer = ArrayBufferVar::FromPPVar(var);
      if (!array_buffer)
        return false;

      base::BinaryValue* binary_value =
          base::BinaryValue::CreateWithCopiedBuffer(
              static_cast<const char*>(array_buffer->Map()),
              array_buffer->ByteLength());
      array_buffer->Unmap();
      value->reset(binary_value);
      return true;
    }
  }
  NOTREACHED();
  return false;
}

// Helper function for CreateVarFromValue(). It only looks at |value| but not
// its descendants. The conversion result is stored in |var|. If |value| is list
// or dictionary, a new node is pushed onto |state|.
//
// Returns false on failure.
bool CreateVarFromValueHelper(const base::Value& value,
                              ScopedPPVar* var,
                              std::stack<ValueNode>* state) {
  switch (value.GetType()) {
    case base::Value::TYPE_NULL: {
      *var = PP_MakeNull();
      return true;
    }
    case base::Value::TYPE_BOOLEAN: {
      bool result = false;
      if (value.GetAsBoolean(&result)) {
        *var = PP_MakeBool(PP_FromBool(result));
        return true;
      }
      return false;
    }
    case base::Value::TYPE_INTEGER: {
      int result = 0;
      if (value.GetAsInteger(&result)) {
        *var = PP_MakeInt32(result);
        return true;
      }
      return false;
    }
    case base::Value::TYPE_DOUBLE: {
      double result = 0;
      if (value.GetAsDouble(&result)) {
        *var = PP_MakeDouble(result);
        return true;
      }
      return false;
    }
    case base::Value::TYPE_STRING: {
      std::string result;
      if (value.GetAsString(&result)) {
        *var = ScopedPPVar(ScopedPPVar::PassRef(),
                           StringVar::StringToPPVar(result));
        return true;
      }
      return false;
    }
    case base::Value::TYPE_BINARY: {
      const base::BinaryValue& binary_value =
          static_cast<const base::BinaryValue&>(value);

      size_t size = binary_value.GetSize();
      if (size > std::numeric_limits<uint32>::max())
        return false;

      ScopedPPVar temp(
          ScopedPPVar::PassRef(),
          PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
              static_cast<uint32>(size), binary_value.GetBuffer()));
      if (temp.get().type == PP_VARTYPE_ARRAY_BUFFER) {
        *var = temp;
        return true;
      }
      return false;
    }
    case base::Value::TYPE_DICTIONARY: {
      scoped_refptr<DictionaryVar> dict_var(new DictionaryVar());
      *var = ScopedPPVar(ScopedPPVar::PassRef(), dict_var->GetPPVar());
      state->push(ValueNode(var->get(), &value));
      return true;
    }
    case base::Value::TYPE_LIST: {
      scoped_refptr<ArrayVar> array_var(new ArrayVar());
      *var = ScopedPPVar(ScopedPPVar::PassRef(), array_var->GetPPVar());
      state->push(ValueNode(var->get(), &value));
      return true;
    }
  }
  NOTREACHED();
  return false;
}

}  // namespace

base::Value* CreateValueFromVar(const PP_Var& var) {
  // Used to detect circular references.
  std::set<int64_t> parent_ids;
  std::stack<VarNode> state;
  scoped_ptr<base::Value> root_value;

  if (!CreateValueFromVarHelper(parent_ids, var, &root_value, &state))
    return NULL;

  while (!state.empty()) {
    VarNode& top = state.top();
    if (top.sentinel) {
      parent_ids.erase(top.var.value.as_id);
      state.pop();
    } else if (top.var.type == PP_VARTYPE_DICTIONARY) {
      parent_ids.insert(top.var.value.as_id);
      top.sentinel = true;

      DictionaryVar* dict_var = DictionaryVar::FromPPVar(top.var);
      if (!dict_var)
        return NULL;

      DCHECK(top.value->GetType() == base::Value::TYPE_DICTIONARY);
      base::DictionaryValue* dict_value =
          static_cast<base::DictionaryValue*>(top.value);

      for (DictionaryVar::KeyValueMap::const_iterator iter =
               dict_var->key_value_map().begin();
           iter != dict_var->key_value_map().end();
           ++iter) {
        // Skip the key-value pair if the value is undefined or null.
        if (iter->second.get().type == PP_VARTYPE_UNDEFINED ||
            iter->second.get().type == PP_VARTYPE_NULL) {
          continue;
        }

        scoped_ptr<base::Value> child_value;
        if (!CreateValueFromVarHelper(parent_ids, iter->second.get(),
                                      &child_value, &state)) {
          return NULL;
        }

        dict_value->SetWithoutPathExpansion(iter->first, child_value.release());
      }
    } else if (top.var.type == PP_VARTYPE_ARRAY) {
      parent_ids.insert(top.var.value.as_id);
      top.sentinel = true;

      ArrayVar* array_var = ArrayVar::FromPPVar(top.var);
      if (!array_var)
        return NULL;

      DCHECK(top.value->GetType() == base::Value::TYPE_LIST);
      base::ListValue* list_value = static_cast<base::ListValue*>(top.value);

      for (ArrayVar::ElementVector::const_iterator iter =
               array_var->elements().begin();
           iter != array_var->elements().end();
           ++iter) {
        scoped_ptr<base::Value> child_value;
        if (!CreateValueFromVarHelper(parent_ids, iter->get(), &child_value,
                                      &state)) {
          return NULL;
        }

        list_value->Append(child_value.release());
      }
    } else {
      NOTREACHED();
      return NULL;
    }
  }
  DCHECK(parent_ids.empty());
  return root_value.release();
}

PP_Var CreateVarFromValue(const base::Value& value) {
  std::stack<ValueNode> state;
  ScopedPPVar root_var;

  if (!CreateVarFromValueHelper(value, &root_var, &state))
    return PP_MakeUndefined();

  while (!state.empty()) {
    ValueNode top = state.top();
    state.pop();

    if (top.value->GetType() == base::Value::TYPE_DICTIONARY) {
      const base::DictionaryValue* dict_value =
          static_cast<const base::DictionaryValue*>(top.value);
      DictionaryVar* dict_var = DictionaryVar::FromPPVar(top.var);
      DCHECK(dict_var);
      for (base::DictionaryValue::Iterator iter(*dict_value);
           !iter.IsAtEnd();
           iter.Advance()) {
        ScopedPPVar child_var;
        if (!CreateVarFromValueHelper(iter.value(), &child_var, &state) ||
            !dict_var->SetWithStringKey(iter.key(), child_var.get())) {
          return PP_MakeUndefined();
        }
      }
    } else if (top.value->GetType() == base::Value::TYPE_LIST) {
      const base::ListValue* list_value =
          static_cast<const base::ListValue*>(top.value);
      ArrayVar* array_var = ArrayVar::FromPPVar(top.var);
      DCHECK(array_var);
      for (base::ListValue::const_iterator iter = list_value->begin();
           iter != list_value->end();
           ++iter) {
        ScopedPPVar child_var;
        if (!CreateVarFromValueHelper(**iter, &child_var, &state))
          return PP_MakeUndefined();

        array_var->elements().push_back(child_var);
      }
    } else {
      NOTREACHED();
      return PP_MakeUndefined();
    }
  }

  return root_var.Release();
}

base::ListValue* CreateListValueFromVarVector(
    const std::vector<PP_Var>& vars) {
  scoped_ptr<base::ListValue> list_value(new base::ListValue());

  for (std::vector<PP_Var>::const_iterator iter = vars.begin();
       iter != vars.end();
       ++iter) {
    base::Value* value = CreateValueFromVar(*iter);
    if (!value)
      return NULL;
    list_value->Append(value);
  }
  return list_value.release();
}

bool CreateVarVectorFromListValue(const base::ListValue& list_value,
                                  std::vector<PP_Var>* vars) {
  if (!vars)
    return false;

  std::vector<ScopedPPVar> result;
  result.reserve(list_value.GetSize());
  for (base::ListValue::const_iterator iter = list_value.begin();
       iter != list_value.end();
       ++iter) {
    ScopedPPVar child_var(ScopedPPVar::PassRef(),
                          CreateVarFromValue(**iter));
    if (child_var.get().type == PP_VARTYPE_UNDEFINED)
      return false;

    result.push_back(child_var);
  }

  vars->clear();
  vars->reserve(result.size());
  for (std::vector<ScopedPPVar>::iterator iter = result.begin();
       iter != result.end();
       ++iter) {
    vars->push_back(iter->Release());
  }

  return true;
}

}  // namespace ppapi


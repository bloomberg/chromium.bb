// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/var_value_conversions.h"

#include <cmath>
#include <cstring>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/test_globals.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {
namespace {

bool Equals(const base::Value& value, const PP_Var& var) {
  switch (value.GetType()) {
    case base::Value::TYPE_NULL: {
      return var.type == PP_VARTYPE_NULL || var.type == PP_VARTYPE_UNDEFINED;
    }
    case base::Value::TYPE_BOOLEAN: {
      bool result = false;
      return var.type == PP_VARTYPE_BOOL &&
             value.GetAsBoolean(&result) &&
             result == PP_ToBool(var.value.as_bool);
    }
    case base::Value::TYPE_INTEGER: {
      int result = 0;
      return var.type == PP_VARTYPE_INT32 &&
             value.GetAsInteger(&result) &&
             result == var.value.as_int;
    }
    case base::Value::TYPE_DOUBLE: {
      double result = 0;
      return var.type == PP_VARTYPE_DOUBLE &&
             value.GetAsDouble(&result) &&
             fabs(result - var.value.as_double) < 1.0e-4;
    }
    case base::Value::TYPE_STRING: {
      std::string result;
      StringVar* string_var = StringVar::FromPPVar(var);
      return string_var &&
             value.GetAsString(&result) &&
             result == string_var->value();
    }
    case base::Value::TYPE_BINARY: {
      const base::BinaryValue& binary_value =
          static_cast<const base::BinaryValue&>(value);
      ArrayBufferVar* array_buffer_var = ArrayBufferVar::FromPPVar(var);
      if (!array_buffer_var ||
          binary_value.GetSize() != array_buffer_var->ByteLength()) {
        return false;
      }

      bool result = !memcmp(binary_value.GetBuffer(), array_buffer_var->Map(),
                            binary_value.GetSize());
      array_buffer_var->Unmap();
      return result;
    }
    case base::Value::TYPE_DICTIONARY: {
      const base::DictionaryValue& dict_value =
          static_cast<const base::DictionaryValue&>(value);
      DictionaryVar* dict_var = DictionaryVar::FromPPVar(var);
      if (!dict_var)
        return false;

      size_t non_undefined_count = 0;
      for (DictionaryVar::KeyValueMap::const_iterator iter =
               dict_var->key_value_map().begin();
           iter != dict_var->key_value_map().end();
           ++iter) {
        if (iter->second.get().type == PP_VARTYPE_UNDEFINED)
          continue;

        ++non_undefined_count;
        const base::Value* sub_value = NULL;
        if (!dict_value.GetWithoutPathExpansion(iter->first, &sub_value) ||
            !Equals(*sub_value, iter->second.get())) {
          return false;
        }
      }
      return non_undefined_count == dict_value.size();
    }
    case base::Value::TYPE_LIST: {
      const base::ListValue& list_value =
          static_cast<const base::ListValue&>(value);
      ArrayVar* array_var = ArrayVar::FromPPVar(var);
      if (!array_var || list_value.GetSize() != array_var->elements().size())
        return false;

      base::ListValue::const_iterator value_iter = list_value.begin();
      ArrayVar::ElementVector::const_iterator var_iter =
          array_var->elements().begin();
      for (; value_iter != list_value.end() &&
                 var_iter != array_var->elements().end();
           ++value_iter, ++var_iter) {
        if (!Equals(**value_iter, var_iter->get()))
          return false;
      }
      return true;
    }
  }
  NOTREACHED();
  return false;
}

class VarValueConversionsTest : public testing::Test {
 public:
  VarValueConversionsTest() {
  }
  virtual ~VarValueConversionsTest() {
  }

  // testing::Test implementation.
  virtual void SetUp() {
    ProxyLock::Acquire();
  }
  virtual void TearDown() {
    ASSERT_TRUE(PpapiGlobals::Get()->GetVarTracker()->GetLiveVars().empty());
    ProxyLock::Release();
  }

 private:
  TestGlobals globals_;
};

}  // namespace

TEST_F(VarValueConversionsTest, CreateValueFromVar) {
  {
    // Var holding a ref to itself is not a valid input.
    scoped_refptr<DictionaryVar> dict_var(new DictionaryVar());
    ScopedPPVar var_1(ScopedPPVar::PassRef(), dict_var->GetPPVar());
    scoped_refptr<ArrayVar> array_var(new ArrayVar());
    ScopedPPVar var_2(ScopedPPVar::PassRef(), array_var->GetPPVar());

    ASSERT_TRUE(dict_var->SetWithStringKey("key_1", var_2.get()));
    scoped_ptr<base::Value> value(CreateValueFromVar(var_1.get()));
    ASSERT_TRUE(value.get());

    ASSERT_TRUE(array_var->Set(0, var_1.get()));
    value.reset(CreateValueFromVar(var_1.get()));
    ASSERT_EQ(NULL, value.get());

    // Make sure |var_1| doesn't indirectly hold a ref to itself, otherwise it
    // is leaked.
    dict_var->DeleteWithStringKey("key_1");
  }

  // Vars of null or undefined type are converted to null values.
  {
    scoped_ptr<base::Value> value(CreateValueFromVar(PP_MakeNull()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, PP_MakeNull()));

    value.reset(CreateValueFromVar(PP_MakeUndefined()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, PP_MakeUndefined()));
  }

  {
    // Test empty dictionary.
    scoped_refptr<DictionaryVar> dict_var(new DictionaryVar());
    ScopedPPVar var(ScopedPPVar::PassRef(), dict_var->GetPPVar());

    scoped_ptr<base::Value> value(CreateValueFromVar(var.get()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, var.get()));
  }

  {
    // Key-value pairs whose value is undefined are ignored.
    scoped_refptr<DictionaryVar> dict_var(new DictionaryVar());
    ASSERT_TRUE(dict_var->SetWithStringKey("key_1", PP_MakeUndefined()));
    ASSERT_TRUE(dict_var->SetWithStringKey("key_2", PP_MakeInt32(1)));
    ScopedPPVar var(ScopedPPVar::PassRef(), dict_var->GetPPVar());

    scoped_ptr<base::Value> value(CreateValueFromVar(var.get()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, var.get()));
  }

  {
    // The same PP_Var is allowed to appear multiple times.
    scoped_refptr<DictionaryVar> dict_var_1(new DictionaryVar());
    ScopedPPVar dict_pp_var_1(ScopedPPVar::PassRef(), dict_var_1->GetPPVar());
    scoped_refptr<DictionaryVar> dict_var_2(new DictionaryVar());
    ScopedPPVar dict_pp_var_2(ScopedPPVar::PassRef(), dict_var_2->GetPPVar());
    scoped_refptr<StringVar> string_var(new StringVar("string_value"));
    ScopedPPVar string_pp_var(ScopedPPVar::PassRef(), string_var->GetPPVar());

    ASSERT_TRUE(dict_var_1->SetWithStringKey("key_1", dict_pp_var_2.get()));
    ASSERT_TRUE(dict_var_1->SetWithStringKey("key_2", dict_pp_var_2.get()));
    ASSERT_TRUE(dict_var_1->SetWithStringKey("key_3", string_pp_var.get()));
    ASSERT_TRUE(dict_var_2->SetWithStringKey("key_4", string_pp_var.get()));

    scoped_ptr<base::Value> value(CreateValueFromVar(dict_pp_var_1.get()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, dict_pp_var_1.get()));
  }

  {
    // Test basic cases for array.
    scoped_refptr<ArrayVar> array_var(new ArrayVar());
    ScopedPPVar var(ScopedPPVar::PassRef(), array_var->GetPPVar());

    scoped_ptr<base::Value> value(CreateValueFromVar(var.get()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, var.get()));

    ASSERT_TRUE(array_var->Set(0, PP_MakeDouble(1)));
    value.reset(CreateValueFromVar(var.get()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, var.get()));
  }

  {
    // Test more complex inputs.
    scoped_refptr<DictionaryVar> dict_var_1(new DictionaryVar());
    ScopedPPVar dict_pp_var_1(ScopedPPVar::PassRef(), dict_var_1->GetPPVar());
    scoped_refptr<DictionaryVar> dict_var_2(new DictionaryVar());
    ScopedPPVar dict_pp_var_2(ScopedPPVar::PassRef(), dict_var_2->GetPPVar());
    scoped_refptr<ArrayVar> array_var(new ArrayVar());
    ScopedPPVar array_pp_var(ScopedPPVar::PassRef(), array_var->GetPPVar());
    scoped_refptr<StringVar> string_var(new StringVar("string_value"));
    ScopedPPVar string_pp_var(ScopedPPVar::PassRef(), string_var->GetPPVar());

    ASSERT_TRUE(dict_var_1->SetWithStringKey("null_key", PP_MakeNull()));
    ASSERT_TRUE(dict_var_1->SetWithStringKey("string_key",
                                             string_pp_var.get()));
    ASSERT_TRUE(dict_var_1->SetWithStringKey("dict_key", dict_pp_var_2.get()));

    ASSERT_TRUE(dict_var_2->SetWithStringKey("undefined_key",
                                             PP_MakeUndefined()));
    ASSERT_TRUE(dict_var_2->SetWithStringKey("double_key", PP_MakeDouble(1)));
    ASSERT_TRUE(dict_var_2->SetWithStringKey("array_key", array_pp_var.get()));

    ASSERT_TRUE(array_var->Set(0, PP_MakeInt32(2)));
    ASSERT_TRUE(array_var->Set(1, PP_MakeBool(PP_TRUE)));
    ASSERT_TRUE(array_var->SetLength(4));

    scoped_ptr<base::Value> value(CreateValueFromVar(dict_pp_var_1.get()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, dict_pp_var_1.get()));
  }

  {
    // Test that dictionary keys containing '.' are handled correctly.
    scoped_refptr<DictionaryVar> dict_var(new DictionaryVar());
    ScopedPPVar dict_pp_var(ScopedPPVar::PassRef(), dict_var->GetPPVar());

    ASSERT_TRUE(dict_var->SetWithStringKey("double.key", PP_MakeDouble(1)));
    ASSERT_TRUE(dict_var->SetWithStringKey("int.key..name", PP_MakeInt32(2)));

    scoped_ptr<base::Value> value(CreateValueFromVar(dict_pp_var.get()));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(Equals(*value, dict_pp_var.get()));
  }
}

TEST_F(VarValueConversionsTest, CreateVarFromValue) {
  {
    // Test basic cases for dictionary.
    base::DictionaryValue dict_value;
    ScopedPPVar var(ScopedPPVar::PassRef(), CreateVarFromValue(dict_value));
    ASSERT_TRUE(Equals(dict_value, var.get()));

    dict_value.SetInteger("int_key", 1);
    var = ScopedPPVar(ScopedPPVar::PassRef(), CreateVarFromValue(dict_value));
    ASSERT_TRUE(Equals(dict_value, var.get()));
  }

  {
    // Test basic cases for array.
    base::ListValue list_value;
    ScopedPPVar var(ScopedPPVar::PassRef(), CreateVarFromValue(list_value));
    ASSERT_TRUE(Equals(list_value, var.get()));

    list_value.AppendInteger(1);
    var = ScopedPPVar(ScopedPPVar::PassRef(), CreateVarFromValue(list_value));
    ASSERT_TRUE(Equals(list_value, var.get()));
  }

  {
    // Test more complex inputs.
    base::DictionaryValue dict_value;
    dict_value.Set("null_key", base::Value::CreateNullValue());
    dict_value.SetString("string_key", "string_value");
    dict_value.SetDouble("dict_key.double_key", 1);

    scoped_ptr<base::ListValue> list_value(new base::ListValue());
    list_value->AppendInteger(2);
    list_value->AppendBoolean(true);
    list_value->Append(base::Value::CreateNullValue());

    dict_value.Set("dict_key.array_key", list_value.release());

    ScopedPPVar var(ScopedPPVar::PassRef(), CreateVarFromValue(dict_value));
    ASSERT_TRUE(Equals(dict_value, var.get()));
  }
}

}  // namespace ppapi

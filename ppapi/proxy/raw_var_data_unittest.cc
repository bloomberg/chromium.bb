// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/raw_var_data.h"

#include <cmath>

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
namespace proxy {

namespace {

class RawVarDataTest : public testing::Test {
 public:
  RawVarDataTest() {}
  ~RawVarDataTest() {}

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

// Compares two vars for equality. When two vars are found to be equal, an entry
// is inserted into |visited_map| with (expected id, actual id). When comparing
// two PP_Vars that have a graph of references, this avoids following reference
// cycles. It also ensures that a var with ID x in the graph is always equal
// to a var with ID y. This guarantees that the topology of the two graphs
// being compared is identical.
bool Equals(const PP_Var& expected,
            const PP_Var& actual,
            base::hash_map<int64_t, int64_t>* visited_map) {
  if (expected.type != actual.type) {
    LOG(ERROR) << "expected type: " << expected.type <<
        " actual type: " << actual.type;
    return false;
  }
  if (VarTracker::IsVarTypeRefcounted(expected.type)) {
    base::hash_map<int64_t, int64_t>::iterator it =
        visited_map->find(expected.value.as_id);
    if (it != visited_map->end()) {
      if (it->second != actual.value.as_id) {
        LOG(ERROR) << "expected id: " << it->second << " actual id: " <<
            actual.value.as_id;
        return false;
      } else {
        return true;
      }
    } else {
      (*visited_map)[expected.value.as_id] = actual.value.as_id;
    }
  }
  switch (expected.type) {
    case PP_VARTYPE_UNDEFINED:
      return true;
    case PP_VARTYPE_NULL:
      return true;
    case PP_VARTYPE_BOOL:
      if (expected.value.as_bool != actual.value.as_bool) {
        LOG(ERROR) << "expected: " << expected.value.as_bool << " actual: " <<
            actual.value.as_bool;
        return false;
      }
      return true;
    case PP_VARTYPE_INT32:
      if (expected.value.as_int != actual.value.as_int) {
        LOG(ERROR) << "expected: " << expected.value.as_int << " actual: " <<
            actual.value.as_int;
        return false;
      }
      return true;
    case PP_VARTYPE_DOUBLE:
      if (fabs(expected.value.as_double - actual.value.as_double) > 1.0e-4) {
        LOG(ERROR) << "expected: " << expected.value.as_double <<
            " actual: " << actual.value.as_double;
        return false;
      }
      return true;
    case PP_VARTYPE_OBJECT:
      if (expected.value.as_id != actual.value.as_id) {
        LOG(ERROR) << "expected: " << expected.value.as_id << " actual: " <<
            actual.value.as_id;
        return false;
      }
      return true;
    case PP_VARTYPE_STRING: {
      StringVar* expected_var = StringVar::FromPPVar(expected);
      StringVar* actual_var = StringVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->value() != actual_var->value()) {
        LOG(ERROR) << "expected: " << expected_var->value() << " actual: " <<
            actual_var->value();
        return false;
      }
      return true;
    }
    case PP_VARTYPE_ARRAY_BUFFER: {
      ArrayBufferVar* expected_var = ArrayBufferVar::FromPPVar(expected);
      ArrayBufferVar* actual_var = ArrayBufferVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->ByteLength() != actual_var->ByteLength()) {
        LOG(ERROR) << "expected: " << expected_var->ByteLength() <<
            " actual: " << actual_var->ByteLength();
        return false;
      }
      if (memcmp(expected_var->Map(), actual_var->Map(),
                 expected_var->ByteLength()) != 0) {
        LOG(ERROR) << "expected array buffer does not match actual.";
        return false;
      }
      return true;
    }
    case PP_VARTYPE_ARRAY: {
      ArrayVar* expected_var = ArrayVar::FromPPVar(expected);
      ArrayVar* actual_var = ArrayVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->elements().size() != actual_var->elements().size()) {
        LOG(ERROR) << "expected: " << expected_var->elements().size() <<
            " actual: " << actual_var->elements().size();
        return false;
      }
      for (size_t i = 0; i < expected_var->elements().size(); ++i) {
        if (!Equals(expected_var->elements()[i].get(),
                    actual_var->elements()[i].get(),
                    visited_map)) {
          return false;
        }
      }
      return true;
    }
    case PP_VARTYPE_DICTIONARY: {
      DictionaryVar* expected_var = DictionaryVar::FromPPVar(expected);
      DictionaryVar* actual_var = DictionaryVar::FromPPVar(actual);
      DCHECK(expected_var && actual_var);
      if (expected_var->key_value_map().size() !=
          actual_var->key_value_map().size()) {
        LOG(ERROR) << "expected: " << expected_var->key_value_map().size() <<
            " actual: " << actual_var->key_value_map().size();
        return false;
      }
      DictionaryVar::KeyValueMap::const_iterator expected_iter =
          expected_var->key_value_map().begin();
      DictionaryVar::KeyValueMap::const_iterator actual_iter =
          actual_var->key_value_map().begin();
      for ( ; expected_iter != expected_var->key_value_map().end();
           ++expected_iter, ++actual_iter) {
        if (expected_iter->first != actual_iter->first) {
          LOG(ERROR) << "expected: " << expected_iter->first <<
              " actual: " << actual_iter->first;
          return false;
        }
        if (!Equals(expected_iter->second.get(),
                    actual_iter->second.get(),
                    visited_map)) {
          return false;
        }
      }
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool Equals(const PP_Var& expected,
            const PP_Var& actual) {
  base::hash_map<int64_t, int64_t> visited_map;
  return Equals(expected, actual, &visited_map);
}

PP_Var WriteAndRead(const PP_Var& var) {
  PP_Instance dummy_instance = 1234;
  scoped_ptr<RawVarDataGraph> expected_data(RawVarDataGraph::Create(
      var, dummy_instance));
  IPC::Message m;
  expected_data->Write(&m);
  PickleIterator iter(m);
  scoped_ptr<RawVarDataGraph> actual_data(RawVarDataGraph::Read(&m, &iter));
  return actual_data->CreatePPVar(dummy_instance);
}

// Assumes a ref for var.
bool WriteReadAndCompare(const PP_Var& var) {
  ScopedPPVar expected(ScopedPPVar::PassRef(), var);
  ScopedPPVar actual(ScopedPPVar::PassRef(), WriteAndRead(expected.get()));
  return Equals(expected.get(), actual.get());
}

}  // namespace

TEST_F(RawVarDataTest, SimpleTest) {
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeUndefined()));
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeNull()));
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeInt32(100)));
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeBool(PP_TRUE)));
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeDouble(53.75)));
  PP_Var object;
  object.type = PP_VARTYPE_OBJECT;
  object.value.as_id = 10;
  EXPECT_TRUE(WriteReadAndCompare(object));
}

TEST_F(RawVarDataTest, StringTest) {
  EXPECT_TRUE(WriteReadAndCompare(StringVar::StringToPPVar("")));
  EXPECT_TRUE(WriteReadAndCompare(StringVar::StringToPPVar("hello world!")));
}

TEST_F(RawVarDataTest, ArrayBufferTest) {
  std::string data = "hello world!";
  PP_Var var = PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
      data.size(), data.data());
  EXPECT_TRUE(WriteReadAndCompare(var));
  var = PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
      0, static_cast<void*>(NULL));
  EXPECT_TRUE(WriteReadAndCompare(var));
  // TODO(raymes): add tests for shmem type array buffers.
}

TEST_F(RawVarDataTest, DictionaryArrayTest) {
  // Empty array.
  scoped_refptr<ArrayVar> array(new ArrayVar);
  ScopedPPVar release_array(ScopedPPVar::PassRef(), array->GetPPVar());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  size_t index = 0;

  // Array with primitives.
  array->Set(index++, PP_MakeUndefined());
  array->Set(index++, PP_MakeNull());
  array->Set(index++, PP_MakeInt32(100));
  array->Set(index++, PP_MakeBool(PP_FALSE));
  array->Set(index++, PP_MakeDouble(0.123));
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Array with 2 references to the same string.
  ScopedPPVar release_string(
      ScopedPPVar::PassRef(), StringVar::StringToPPVar("abc"));
  array->Set(index++, release_string.get());
  array->Set(index++, release_string.get());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Array with nested array that references the same string.
  scoped_refptr<ArrayVar> array2(new ArrayVar);
  ScopedPPVar release_array2(ScopedPPVar::PassRef(), array2->GetPPVar());
  array2->Set(0, release_string.get());
  array->Set(index++, release_array2.get());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Empty dictionary.
  scoped_refptr<DictionaryVar> dictionary(new DictionaryVar);
  ScopedPPVar release_dictionary(ScopedPPVar::PassRef(),
                                 dictionary->GetPPVar());
  EXPECT_TRUE(WriteReadAndCompare(dictionary->GetPPVar()));

  // Dictionary with primitives.
  dictionary->SetWithStringKey("1", PP_MakeUndefined());
  dictionary->SetWithStringKey("2", PP_MakeNull());
  dictionary->SetWithStringKey("3", PP_MakeInt32(-100));
  dictionary->SetWithStringKey("4", PP_MakeBool(PP_TRUE));
  dictionary->SetWithStringKey("5", PP_MakeDouble(-103.52));
  EXPECT_TRUE(WriteReadAndCompare(dictionary->GetPPVar()));

  // Dictionary with 2 references to the same string.
  dictionary->SetWithStringKey("6", release_string.get());
  dictionary->SetWithStringKey("7", release_string.get());
  EXPECT_TRUE(WriteReadAndCompare(dictionary->GetPPVar()));

  // Dictionary with nested dictionary that references the same string.
  scoped_refptr<DictionaryVar> dictionary2(new DictionaryVar);
  ScopedPPVar release_dictionary2(ScopedPPVar::PassRef(),
                                  dictionary2->GetPPVar());
  dictionary2->SetWithStringKey("abc", release_string.get());
  dictionary->SetWithStringKey("8", release_dictionary2.get());
  EXPECT_TRUE(WriteReadAndCompare(dictionary->GetPPVar()));

  // Array with dictionary.
  array->Set(index++, release_dictionary.get());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Array with dictionary with array.
  array2->Set(0, PP_MakeInt32(100));
  dictionary->SetWithStringKey("9", release_array2.get());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Array <-> dictionary cycle.
  dictionary->SetWithStringKey("10", release_array.get());
  ScopedPPVar result = ScopedPPVar(ScopedPPVar::PassRef(),
                                   WriteAndRead(release_dictionary.get()));
  EXPECT_TRUE(Equals(release_dictionary.get(), result.get()));
  // Break the cycle.
  // TODO(raymes): We need some better machinery for releasing vars with
  // cycles. Remove the code below once we have that.
  dictionary->DeleteWithStringKey("10");
  DictionaryVar* result_dictionary = DictionaryVar::FromPPVar(result.get());
  result_dictionary->DeleteWithStringKey("10");

  // Array with self references.
  array->Set(index, release_array.get());
  result = ScopedPPVar(ScopedPPVar::PassRef(),
                       WriteAndRead(release_array.get()));
  EXPECT_TRUE(Equals(release_array.get(), result.get()));
  // Break the self reference.
  array->Set(index, PP_MakeUndefined());
  ArrayVar* result_array = ArrayVar::FromPPVar(result.get());
  result_array->Set(index, PP_MakeUndefined());
}

}  // namespace proxy
}  // namespace ppapi

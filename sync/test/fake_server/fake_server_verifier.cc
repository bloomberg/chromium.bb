// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server_verifier.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/test/fake_server/fake_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

namespace {

AssertionResult DictionaryCreationAssertionFailure() {
  return AssertionFailure() << "FakeServer failed to create an entities "
                            << "dictionary.";
}

AssertionResult VerificationCountAssertionFailure(size_t actual_count,
                                                  size_t expected_count) {
  return AssertionFailure() << "Actual count: " << actual_count << "; "
                            << "Expected count: " << expected_count;
}

AssertionResult UnknownTypeAssertionFailure(const string& model_type) {
  return AssertionFailure() << "Verification not attempted. Unknown ModelType: "
                            << model_type;
}

}  // namespace

namespace fake_server {

FakeServerVerifier::FakeServerVerifier(FakeServer* fake_server)
    : fake_server_(fake_server) { }

FakeServerVerifier::~FakeServerVerifier() {}

AssertionResult FakeServerVerifier::VerifyEntityCountByType(
    size_t expected_count,
    syncer::ModelType model_type) const {
  scoped_ptr<base::DictionaryValue> entities =
      fake_server_->GetEntitiesAsDictionaryValue();
  if (!entities.get()) {
    return DictionaryCreationAssertionFailure();
  }

  string model_type_string = ModelTypeToString(model_type);
  base::ListValue* entity_list = NULL;
  if (!entities->GetList(model_type_string, &entity_list)) {
    return UnknownTypeAssertionFailure(model_type_string);
  } else if  (expected_count != entity_list->GetSize()) {
    return VerificationCountAssertionFailure(entity_list->GetSize(),
                                             expected_count);
  }

  return AssertionSuccess();
}

AssertionResult FakeServerVerifier::VerifyEntityCountByTypeAndName(
    size_t expected_count,
    syncer::ModelType model_type,
    const string& name) const {
  scoped_ptr<base::DictionaryValue> entities =
      fake_server_->GetEntitiesAsDictionaryValue();
  if (!entities.get()) {
    return DictionaryCreationAssertionFailure();
  }

  string model_type_string = ModelTypeToString(model_type);
  base::ListValue* entity_list = NULL;
  size_t actual_count = 0;
  if (entities->GetList(model_type_string, &entity_list)) {
    scoped_ptr<base::Value> name_value(new base::StringValue(name));
    for (base::ListValue::const_iterator it = entity_list->begin();
         it != entity_list->end(); ++it) {
      if (name_value->Equals(*it)) {
        actual_count++;
      }
    }
  }

  if (!entity_list) {
    return UnknownTypeAssertionFailure(model_type_string);
  } else if (actual_count != expected_count) {
    return VerificationCountAssertionFailure(actual_count, expected_count)
        << "; Name: " << name;
  }

  return AssertionSuccess();
}

}  // namespace fake_server

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "google/cacheinvalidation/types.pb.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/invalidator_registrar.h"
#include "sync/notifier/invalidator_test_template.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// We test InvalidatorRegistrar by wrapping it in an Invalidator and
// running the usual Invalidator tests.

// Thin Invalidator wrapper around InvalidatorRegistrar.
class RegistrarInvalidator : public Invalidator {
 public:
  RegistrarInvalidator() {}
  virtual ~RegistrarInvalidator() {}

  InvalidatorRegistrar* GetRegistrar() {
    return &registrar_;
  }

  // Invalidator implementation.
  virtual void RegisterHandler(InvalidationHandler* handler) OVERRIDE {
    registrar_.RegisterHandler(handler);
  }

  virtual void UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids) OVERRIDE {
    registrar_.UpdateRegisteredIds(handler, ids);
  }

  virtual void UnregisterHandler(InvalidationHandler* handler) OVERRIDE {
    registrar_.UnregisterHandler(handler);
  }

  virtual InvalidatorState GetInvalidatorState() const OVERRIDE {
    return registrar_.GetInvalidatorState();
  }

  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE {
    // Do nothing.
  }

  virtual void SetStateDeprecated(const std::string& state) OVERRIDE {
    // Do nothing.
  }

  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE {
    // Do nothing.
  }

  virtual void SendInvalidation(const ObjectIdStateMap& id_state_map) OVERRIDE {
    // Do nothing.
  }

 private:
  InvalidatorRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RegistrarInvalidator);
};

class RegistrarInvalidatorTestDelegate {
 public:
  RegistrarInvalidatorTestDelegate() {}

  ~RegistrarInvalidatorTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& initial_state,
      const base::WeakPtr<InvalidationStateTracker>&
          invalidation_state_tracker) {
    DCHECK(!invalidator_.get());
    invalidator_.reset(new RegistrarInvalidator());
  }

  RegistrarInvalidator* GetInvalidator() {
    return invalidator_.get();
  }

  void DestroyInvalidator() {
    invalidator_.reset();
  }

  void WaitForInvalidator() {
    // Do nothing.
  }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->GetRegistrar()->UpdateInvalidatorState(state);
  }

  void TriggerOnIncomingInvalidation(const ObjectIdStateMap& id_state_map,
                                     IncomingInvalidationSource source) {
    invalidator_->GetRegistrar()->DispatchInvalidationsToHandlers(
        id_state_map, source);
  }

  static bool InvalidatorHandlesDeprecatedState() {
    return false;
  }

 private:
  scoped_ptr<RegistrarInvalidator> invalidator_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    RegistrarInvalidatorTest, InvalidatorTest,
    RegistrarInvalidatorTestDelegate);

class InvalidatorRegistrarTest : public testing::Test {};

// Multiple registrations by different handlers on the same object ID should
// cause a CHECK.
//
// Technically this can be part of InvalidatorTest, but we want to keep the
// number of death tests down.
TEST_F(InvalidatorRegistrarTest, MultipleRegistration) {
  const invalidation::ObjectId id1(ipc::invalidation::ObjectSource::TEST, "a");
  const invalidation::ObjectId id2(ipc::invalidation::ObjectSource::TEST, "a");

  InvalidatorRegistrar registrar;

  FakeInvalidationHandler handler1;
  registrar.RegisterHandler(&handler1);

  FakeInvalidationHandler handler2;
  registrar.RegisterHandler(&handler2);

  ObjectIdSet ids;
  ids.insert(id1);
  ids.insert(id2);
  registrar.UpdateRegisteredIds(&handler1, ids);

  registrar.DetachFromThreadForTest();
  // We expect a death via CHECK(). We can't match against the CHECK() message
  // though since they are removed in official builds.
  EXPECT_DEATH({ registrar.UpdateRegisteredIds(&handler2, ids); }, "");
}

}  // namespace

}  // namespace syncer

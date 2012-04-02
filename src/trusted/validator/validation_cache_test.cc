/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gtest/gtest.h"

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/ncvalidate.h"
#include "native_client/src/trusted/validator/validation_cache.h"

#define CONTEXT_MARKER 31
#define QUERY_MARKER 37

#define CODE_SIZE 32

// ret
const char ret[CODE_SIZE + 1] =
    "\xc3\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";

// pblendw $0xc0,%xmm0,%xmm2
const char sse41[CODE_SIZE + 1] =
    "\x66\x0f\x3a\x0e\xd0\xc0\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";

struct MockContext {
  int marker; /* Sanity check that we're getting the right object. */
  int query_result;
  bool set_validates_expected;
  bool query_destroyed;
};

enum MockQueryState {
  QUERY_CREATED,
  QUERY_GET_CALLED,
  QUERY_SET_CALLED,
  QUERY_DESTROYED
};

struct MockQuery {
  /* Sanity check that we're getting the right object. */
  int marker;
  MockQueryState state;
  int add_count;
  MockContext *context;
};

void *MockCreateQuery(void *handle) {
  MockContext *mcontext = (MockContext *) handle;
  MockQuery *mquery = (MockQuery *) malloc(sizeof(MockQuery));
  EXPECT_EQ(CONTEXT_MARKER, mcontext->marker);
  mquery->marker = QUERY_MARKER;
  mquery->state = QUERY_CREATED;
  mquery->add_count = 0;
  mquery->context = mcontext;
  return mquery;
}

void MockAddData(void *query, const unsigned char *data, size_t length) {
  UNREFERENCED_PARAMETER(data);
  MockQuery *mquery = (MockQuery *) query;
  ASSERT_EQ(QUERY_MARKER, mquery->marker);
  EXPECT_EQ(QUERY_CREATED, mquery->state);
  /* Small data is suspicious. */
  EXPECT_LE((size_t) 2, length);
  mquery->add_count += 1;
}

int MockQueryCodeValidates(void *query) {
  MockQuery *mquery = (MockQuery *) query;
  EXPECT_EQ(QUERY_MARKER, mquery->marker);
  EXPECT_EQ(QUERY_CREATED, mquery->state);
  /* Less than two pieces of data is suspicious. */
  EXPECT_LE(2, mquery->add_count);
  mquery->state = QUERY_GET_CALLED;
  return mquery->context->query_result;
}

void MockSetCodeValidates(void *query) {
  MockQuery *mquery = (MockQuery *) query;
  ASSERT_EQ(QUERY_MARKER, mquery->marker);
  EXPECT_EQ(QUERY_GET_CALLED, mquery->state);
  EXPECT_EQ(true, mquery->context->set_validates_expected);
  mquery->state = QUERY_SET_CALLED;
}

void MockDestroyQuery(void *query) {
  MockQuery *mquery = (MockQuery *) query;
  ASSERT_EQ(QUERY_MARKER, mquery->marker);
  if (mquery->context->set_validates_expected) {
    EXPECT_EQ(QUERY_SET_CALLED, mquery->state);
  } else {
    EXPECT_EQ(QUERY_GET_CALLED, mquery->state);
  }
  mquery->state = QUERY_DESTROYED;
  mquery->context->query_destroyed = true;
  free(mquery);
}

class ValidationCachingInterfaceTests : public ::testing::Test {
 protected:
  MockContext context;
  NaClValidationCache cache;
  NaClCPUFeatures cpu_features;
  int bundle_size;

  unsigned char code_buffer[CODE_SIZE];

  void SetUp() {
    context.marker = CONTEXT_MARKER;
    context.query_result = 1;
    context.set_validates_expected = false;
    context.query_destroyed = false;

    cache.handle = &context;
    cache.CreateQuery = MockCreateQuery;
    cache.AddData = MockAddData;
    cache.QueryKnownToValidate = MockQueryCodeValidates;
    cache.SetKnownToValidate = MockSetCodeValidates;
    cache.DestroyQuery = MockDestroyQuery;

    NaClSetAllCPUFeatures(&cpu_features);

    bundle_size = 32;

    memset(code_buffer, 0x90, sizeof(code_buffer));
  }

  NaClValidationStatus Validate() {
    return NACL_SUBARCH_NAME(ApplyValidator,
                             NACL_TARGET_ARCH,
                             NACL_TARGET_SUBARCH)(
                                 NACL_SB_DEFAULT,
                                 NaClApplyCodeValidation,
                                 0, code_buffer, 32,
                                 bundle_size, FALSE, &cpu_features,
                                 &cache);
  }
};

TEST_F(ValidationCachingInterfaceTests, Sanity) {
  void *query = cache.CreateQuery(cache.handle);
  cache.AddData(query, NULL, 6);
  cache.AddData(query, NULL, 128);
  EXPECT_EQ(1, cache.QueryKnownToValidate(query));
  cache.DestroyQuery(query);
  EXPECT_EQ(true, context.query_destroyed);
}

TEST_F(ValidationCachingInterfaceTests, NoCache) {
  NaClValidationStatus status =
      NACL_SUBARCH_NAME(ApplyValidator,
                        NACL_TARGET_ARCH,
                        NACL_TARGET_SUBARCH)(
                            NACL_SB_DEFAULT,
                            NaClApplyCodeValidation,
                            0, code_buffer, CODE_SIZE,
                            bundle_size, FALSE, &cpu_features,
                            NULL);
  EXPECT_EQ(NaClValidationSucceeded, status);
}

TEST_F(ValidationCachingInterfaceTests, CacheHit) {
  NaClValidationStatus status = Validate();
  EXPECT_EQ(NaClValidationSucceeded, status);
  EXPECT_EQ(true, context.query_destroyed);
}

TEST_F(ValidationCachingInterfaceTests, CacheMiss) {
  context.query_result = 0;
  context.set_validates_expected = true;
  NaClValidationStatus status = Validate();
  EXPECT_EQ(NaClValidationSucceeded, status);
  EXPECT_EQ(true, context.query_destroyed);
}

TEST_F(ValidationCachingInterfaceTests, SSE4Allowed) {
  memcpy(code_buffer, sse41, CODE_SIZE);
  context.query_result = 0;
  context.set_validates_expected = true;
  NaClValidationStatus status = Validate();
  EXPECT_EQ(NaClValidationSucceeded, status);
  EXPECT_EQ(true, context.query_destroyed);
}

TEST_F(ValidationCachingInterfaceTests, SSE4Stubout) {
  memcpy(code_buffer, sse41, CODE_SIZE);
  context.query_result = 0;
  NaClSetCPUFeature(&cpu_features, NaClCPUFeature_SSE41, 0);
  NaClValidationStatus status = Validate();
  EXPECT_EQ(NaClValidationSucceeded, status);
  EXPECT_EQ(true, context.query_destroyed);
}

TEST_F(ValidationCachingInterfaceTests, IllegalInst) {
  memcpy(code_buffer, ret, CODE_SIZE);
  context.query_result = 0;
  NaClValidationStatus status = Validate();
  EXPECT_EQ(NaClValidationFailed, status);
  EXPECT_EQ(true, context.query_destroyed);
}

TEST_F(ValidationCachingInterfaceTests, IllegalCacheHit) {
  memcpy(code_buffer, ret, CODE_SIZE);
  NaClValidationStatus status = Validate();
  // Success proves the cache shortcircuted validation.
  EXPECT_EQ(NaClValidationSucceeded, status);
  EXPECT_EQ(true, context.query_destroyed);
}

// Test driver function.
int main(int argc, char *argv[]) {
  // The IllegalInst test touches the log mutex deep inside the validator.
  // This causes an SEH exception to be thrown on Windows if the mutex is not
  // initialized.
  // http://code.google.com/p/nativeclient/issues/detail?id=1696
  NaClLogModuleInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

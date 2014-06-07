/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gtest/gtest.h"

#include <fcntl.h>

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/validator/ncvalidate.h"
#include "native_client/src/trusted/validator/validation_cache.h"
#include "native_client/src/trusted/validator/validation_cache_internal.h"
#include "native_client/src/trusted/cpu_features/arch/x86/cpu_x86.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/validator/rich_file_info.h"
#include "native_client/src/trusted/validator/validation_metadata.h"

#define CONTEXT_MARKER 31
#define QUERY_MARKER 37

#define CODE_SIZE 32

#if defined(__mips__)
# define NOP 0x00
#else  // x86
# define NOP 0x90
#endif


#if defined(__mips__)
// jr ra
const char ret[CODE_SIZE + 1] =
    "\x08\x00\xe0\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
#else  // x86
// ret
const char ret[CODE_SIZE + 1] =
    "\xc3\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";
#endif

// pblendw $0xc0,%xmm0,%xmm2
const char sse41[CODE_SIZE + 1] =
    "\x66\x0f\x3a\x0e\xd0\xc0\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";

struct MockContext {
  int marker; /* Sanity check that we're getting the right object. */
  int query_result;
  int add_count_expected;
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
  EXPECT_EQ(mquery->context->add_count_expected, mquery->add_count);
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

/* Hint that the validation should use the (fake) cache. */
int MockCachingIsInexpensive(const struct NaClValidationMetadata *metadata) {
  UNREFERENCED_PARAMETER(metadata);
  return 1;
}

class ValidationCachingInterfaceTests : public ::testing::Test {
 protected:
  MockContext context;
  NaClValidationMetadata *metadata_ptr;
  NaClValidationCache cache;
  const struct NaClValidatorInterface *validator;
  NaClCPUFeatures *cpu_features;

  unsigned char code_buffer[CODE_SIZE];

  void SetUp() {
    context.marker = CONTEXT_MARKER;
    context.query_result = 1;
    context.add_count_expected = 4;
    context.set_validates_expected = false;
    context.query_destroyed = false;

    metadata_ptr = NULL;

    cache.handle = &context;
    cache.CreateQuery = MockCreateQuery;
    cache.AddData = MockAddData;
    cache.QueryKnownToValidate = MockQueryCodeValidates;
    cache.SetKnownToValidate = MockSetCodeValidates;
    cache.DestroyQuery = MockDestroyQuery;
    cache.CachingIsInexpensive = MockCachingIsInexpensive;

    validator = NaClCreateValidator();
    cpu_features = (NaClCPUFeatures *) malloc(validator->CPUFeatureSize);
    EXPECT_NE(cpu_features, (NaClCPUFeatures *) NULL);
    validator->SetAllCPUFeatures(cpu_features);

    memset(code_buffer, NOP, sizeof(code_buffer));
  }

  NaClValidationStatus Validate() {
    return validator->Validate(0, code_buffer, 32,
                               FALSE,  /* stubout_mode */
                               FALSE,  /* readonly_test */
                               cpu_features,
                               metadata_ptr,
                               &cache);
  }

  void TearDown() {
    free(cpu_features);
  }
};

TEST_F(ValidationCachingInterfaceTests, Sanity) {
  void *query = cache.CreateQuery(cache.handle);
  context.add_count_expected = 2;
  cache.AddData(query, NULL, 6);
  cache.AddData(query, NULL, 128);
  EXPECT_EQ(1, cache.QueryKnownToValidate(query));
  cache.DestroyQuery(query);
  EXPECT_EQ(true, context.query_destroyed);
}

TEST_F(ValidationCachingInterfaceTests, NoCache) {
  const struct NaClValidatorInterface *validator = NaClCreateValidator();
  NaClValidationStatus status = validator->Validate(
      0, code_buffer, CODE_SIZE,
      FALSE, /* stubout_mode */
      FALSE, /* readonly_test */
      cpu_features,
      NULL, /* metadata */
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

#if defined(__x86_64__) || defined(__i386__)
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
  /* TODO(jfb) Use a safe cast here, this test should only run for x86. */
  NaClSetCPUFeatureX86((NaClCPUFeaturesX86 *) cpu_features,
                       NaClCPUFeatureX86_SSE41, 0);
  NaClValidationStatus status = Validate();
  EXPECT_EQ(NaClValidationSucceeded, status);
  EXPECT_EQ(true, context.query_destroyed);
}
#endif

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

TEST_F(ValidationCachingInterfaceTests, Metadata) {
  NaClValidationMetadata metadata;
  memset(&metadata, 0, sizeof(metadata));
  metadata.identity_type = NaClCodeIdentityFile;
  metadata.file_name = (char *) "foobar";
  metadata.file_name_length = strlen(metadata.file_name);
  metadata.file_size = CODE_SIZE;
  metadata.mtime = 100;
  metadata_ptr = &metadata;
  context.add_count_expected = 12;
  NaClValidationStatus status = Validate();
  EXPECT_EQ(NaClValidationSucceeded, status);
  EXPECT_EQ(true, context.query_destroyed);
}

class ValidationCachingSerializationTests : public ::testing::Test {
 protected:
  struct NaClRichFileInfo info;
  uint8_t *buffer;
  uint32_t buffer_length;
  struct NaClRichFileInfo inp;
  struct NaClRichFileInfo outp;

  void SetUp() {
    buffer = 0;
    buffer_length = 0;
    NaClRichFileInfoCtor(&inp);
    NaClRichFileInfoCtor(&outp);
  }

  void TearDown() {
    free(buffer);
    // Don't free the inp structure, it does not contain malloced memory.
    NaClRichFileInfoDtor(&outp);
  }
};

TEST_F(ValidationCachingSerializationTests, NormalOperationSimple) {
  inp.known_file = 0;
  inp.file_path = "foo";
  inp.file_path_length = 3;
  EXPECT_EQ(0, NaClSerializeNaClDescMetadata(&inp, &buffer, &buffer_length));
  EXPECT_EQ(0, NaClDeserializeNaClDescMetadata(buffer, buffer_length, &outp));

  EXPECT_EQ((uint8_t) 0, outp.known_file);
  EXPECT_EQ((uint32_t) 0, outp.file_path_length);
  EXPECT_EQ(NULL, outp.file_path);
}

TEST_F(ValidationCachingSerializationTests, NormalOperationFull) {
  inp.known_file = 1;
  inp.file_path = "foo";
  inp.file_path_length = 3;

  EXPECT_EQ(0, NaClSerializeNaClDescMetadata(&inp, &buffer, &buffer_length));
  EXPECT_EQ(0, NaClDeserializeNaClDescMetadata(buffer, buffer_length, &outp));

  EXPECT_EQ((uint8_t) 1, outp.known_file);
  EXPECT_EQ((uint32_t) 3, outp.file_path_length);
  EXPECT_EQ(0, memcmp("foo", outp.file_path, outp.file_path_length));
}

TEST_F(ValidationCachingSerializationTests, BadSizeSimple) {
  inp.known_file = 0;
  EXPECT_EQ(0, NaClSerializeNaClDescMetadata(&inp, &buffer, &buffer_length));
  for (uint32_t i = -1; i <= buffer_length + 4; i++) {
    /* The only case that is OK. */
    if (i == buffer_length)
      continue;

    /* Wrong number of bytes, fail. */
    EXPECT_EQ(1, NaClDeserializeNaClDescMetadata(buffer, i, &outp));
  }
}

TEST_F(ValidationCachingSerializationTests, BadSizeFull) {
  inp.known_file = 1;
  inp.file_path = "foo";
  inp.file_path_length = 3;
  EXPECT_EQ(0, NaClSerializeNaClDescMetadata(&inp, &buffer, &buffer_length));
  for (uint32_t i = -1; i <= buffer_length + 4; i++) {
    /* The only case that is OK. */
    if (i == buffer_length)
      continue;

    /* Wrong number of bytes, fail. */
    EXPECT_EQ(1, NaClDeserializeNaClDescMetadata(buffer, i, &outp));
    /* Paranoia. */
    EXPECT_EQ(0, outp.known_file);
    /* Make sure we don't leak on failure. */
    EXPECT_EQ(NULL, outp.file_path);
  }
}

static char *AN_ARBITRARY_FILE_PATH = NULL;

class ValidationCachingFileOriginTests : public ::testing::Test {
 protected:
  struct NaClDesc *desc;

  struct NaClRichFileInfo inp;
  struct NaClRichFileInfo outp;

  void SetUp() {
    struct NaClHostDesc *host_desc = NULL;
    int fd = open(AN_ARBITRARY_FILE_PATH, O_RDONLY);

    desc = NULL;
    NaClRichFileInfoCtor(&inp);
    NaClRichFileInfoCtor(&outp);

    ASSERT_NE(-1, fd);
    host_desc = NaClHostDescPosixMake(fd, NACL_ABI_O_RDONLY);
    desc = (struct NaClDesc *) NaClDescIoDescMake(host_desc);
    ASSERT_NE((struct NaClDesc *) NULL, desc);
  }

  void TearDown() {
    // Don't free the inp structure, it does not contain malloced memory.
    NaClRichFileInfoDtor(&outp);
    NaClDescSafeUnref(desc);
  }
};

TEST_F(ValidationCachingFileOriginTests, None) {
  EXPECT_EQ(1, NaClGetFileOriginInfo(desc, &outp));
}

TEST_F(ValidationCachingFileOriginTests, Simple) {
  inp.known_file = 0;
  inp.file_path = "foobar";
  inp.file_path_length = 6;
  EXPECT_EQ(0, NaClSetFileOriginInfo(desc, &inp));
  EXPECT_EQ(0, NaClGetFileOriginInfo(desc, &outp));

  EXPECT_EQ(0, outp.known_file);
  EXPECT_EQ((uint32_t) 0, outp.file_path_length);
  EXPECT_EQ(NULL, outp.file_path);
}


TEST_F(ValidationCachingFileOriginTests, Full) {
  inp.known_file = 1;
  inp.file_path = "foobar";
  inp.file_path_length = 6;
  EXPECT_EQ(0, NaClSetFileOriginInfo(desc, &inp));
  EXPECT_EQ(0, NaClGetFileOriginInfo(desc, &outp));

  EXPECT_EQ(1, outp.known_file);
  EXPECT_EQ((uint32_t) 6, outp.file_path_length);
  EXPECT_EQ(0, memcmp("foobar", outp.file_path, outp.file_path_length));
}

// Test driver function.
int main(int argc, char *argv[]) {
  // One file we know must exist is this executable.
  AN_ARBITRARY_FILE_PATH = argv[0];
  // The IllegalInst test touches the log mutex deep inside the validator.
  // This causes an SEH exception to be thrown on Windows if the mutex is not
  // initialized.
  // http://code.google.com/p/nativeclient/issues/detail?id=1696
  NaClLogModuleInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

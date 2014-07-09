// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_mapping.h"

#include <string.h>

#include <limits>
#include <string>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_mapping.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"

REGISTER_TEST_CASE(FileMapping);

namespace {

// TODO(dmichael): Move these to test_utils so we can share them?
int32_t ReadEntireFile(PP_Instance instance,
                       pp::FileIO* file_io,
                       int32_t offset,
                       std::string* data,
                       CallbackType callback_type) {
  TestCompletionCallback callback(instance, callback_type);
  char buf[256];
  int32_t read_offset = offset;

  for (;;) {
    callback.WaitForResult(
        file_io->Read(read_offset, buf, sizeof(buf), callback.GetCallback()));
    if (callback.result() < 0)
      return callback.result();
    if (callback.result() == 0)
      break;
    read_offset += callback.result();
    data->append(buf, callback.result());
  }

  return PP_OK;
}

int32_t WriteEntireBuffer(PP_Instance instance,
                          pp::FileIO* file_io,
                          int32_t offset,
                          const std::string& data,
                          CallbackType callback_type) {
  TestCompletionCallback callback(instance, callback_type);
  int32_t write_offset = offset;
  const char* buf = data.c_str();
  int32_t size = data.size();

  while (write_offset < offset + size) {
    callback.WaitForResult(file_io->Write(write_offset,
                                          &buf[write_offset - offset],
                                          size - write_offset + offset,
                                          callback.GetCallback()));
    if (callback.result() < 0)
      return callback.result();
    if (callback.result() == 0)
      return PP_ERROR_FAILED;
    write_offset += callback.result();
  }
  callback.WaitForResult(file_io->Flush(callback.GetCallback()));
  return callback.result();
}

}  // namespace

std::string TestFileMapping::MapAndCheckResults(uint32_t prot,
                                                uint32_t flags) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/mapped_file");

  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  const int64_t page_size =
      file_mapping_if_->GetMapPageSize(instance_->pp_instance());
  const int64_t kNumPages = 4;
  // Make a string that's big enough that it spans all of the first |n-1| pages,
  // plus a little bit of the |nth| page.
  std::string file_contents((page_size * (kNumPages - 1)) + 128, 'a');

  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_EQ(PP_OK, WriteEntireBuffer(instance_->pp_instance(),
                                     &file_io,
                                     0,
                                     file_contents,
                                     callback_type()));

  // TODO(dmichael): Use C++ interface.
  void* address = NULL;
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io.pp_resource(),
          kNumPages * page_size,
          prot,
          flags,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_NE(NULL, address);

  if (prot & PP_FILEMAPPROTECTION_READ) {
    // Make sure we can read.
    std::string mapped_data(static_cast<char*>(address), file_contents.size());
    // The initial data should match.
    ASSERT_EQ(file_contents, mapped_data);

    // Now write some data and flush it.
    const std::string file_contents2(file_contents.size(), 'x');
    ASSERT_EQ(PP_OK, WriteEntireBuffer(instance_->pp_instance(),
                                       &file_io,
                                       0,
                                       file_contents2,
                                       callback_type()));
    // If the region was mapped SHARED, it should get updated.
    std::string mapped_data2(static_cast<char*>(address), file_contents.size());
    if (flags & PP_FILEMAPFLAG_SHARED)
      ASSERT_EQ(file_contents2, mapped_data2);
    // In POSIX, it is unspecified in the PRIVATE case whether changes to the
    // file are visible to the mapped region. So we can't really test anything
    // here in that case.
    // TODO(dmichael): Make sure our Pepper documentation reflects this.
  }
  if (prot & PP_FILEMAPPROTECTION_WRITE) {
    std::string old_file_contents;
    ASSERT_EQ(PP_OK, ReadEntireFile(instance_->pp_instance(),
                                    &file_io,
                                    0,
                                    &old_file_contents,
                                    callback_type()));
    // Write something else to the mapped region, then unmap, and see if it
    // gets written to the file. (Note we have to Unmap to make sure that the
    // write is committed).
    memset(address, 'y', file_contents.size());
    // Note, we might not have read access to the mapped region here, so we
    // make a string with the same contents without actually reading.
    std::string mapped_data3(file_contents.size(), 'y');
    callback.WaitForResult(
        file_mapping_if_->Unmap(
            instance_->pp_instance(), address, file_contents.size(),
            callback.GetCallback().pp_completion_callback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());
    std::string new_file_contents;
    ASSERT_EQ(PP_OK, ReadEntireFile(instance_->pp_instance(),
                                    &file_io,
                                    0,
                                    &new_file_contents,
                                    callback_type()));

    // Sanity-check that the data we wrote isn't the same as what was already
    // there, otherwise our test is invalid.
    ASSERT_NE(mapped_data3, old_file_contents);
    // If it's SHARED, the file should match what we wrote to the mapped region.
    // Otherwise, it should not have changed.
    if (flags & PP_FILEMAPFLAG_SHARED)
      ASSERT_EQ(mapped_data3, new_file_contents);
    else
      ASSERT_EQ(old_file_contents, new_file_contents);
  } else {
    // We didn't do the "WRITE" test, but we still want to Unmap.
    callback.WaitForResult(
        file_mapping_if_->Unmap(
            instance_->pp_instance(), address, file_contents.size(),
            callback.GetCallback().pp_completion_callback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());
  }
  PASS();
}

bool TestFileMapping::Init() {
  // TODO(dmichael): Use unversioned string when this goes to stable?
  file_mapping_if_ = static_cast<const PPB_FileMapping_0_1*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEMAPPING_INTERFACE_0_1));
  return !!file_mapping_if_ && CheckTestingInterface() &&
         EnsureRunningOverHTTP();
}

void TestFileMapping::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestFileMapping, BadParameters, filter);
  RUN_CALLBACK_TEST(TestFileMapping, Map, filter);
  RUN_CALLBACK_TEST(TestFileMapping, PartialRegions, filter);
}

std::string TestFileMapping::TestBadParameters() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/mapped_file");

  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  const int64_t page_size =
      file_mapping_if_->GetMapPageSize(instance_->pp_instance());
  // const int64_t kNumPages = 4;
  // Make a string that's big enough that it spans 3 pages, plus a little extra.
  //std::string file_contents((page_size * (kNumPages - 1)) + 128, 'a');
  std::string file_contents(page_size, 'a');

  pp::FileIO file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_EQ(PP_OK, WriteEntireBuffer(instance_->pp_instance(),
                                     &file_io,
                                     0,
                                     file_contents,
                                     callback_type()));

  // Bad instance.
  void* address = NULL;
  callback.WaitForResult(
      file_mapping_if_->Map(
          PP_Instance(0xbadbad),
          file_io.pp_resource(),
          page_size,
          PP_FILEMAPPROTECTION_READ,
          PP_FILEMAPFLAG_PRIVATE,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_EQ(NULL, address);

  // Bad resource.
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          PP_Resource(0xbadbad),
          page_size,
          PP_FILEMAPPROTECTION_READ,
          PP_FILEMAPFLAG_PRIVATE,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_EQ(NULL, address);

  // Length too big.
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io.pp_resource(),
          std::numeric_limits<int64_t>::max(),
          PP_FILEMAPPROTECTION_READ,
          PP_FILEMAPFLAG_PRIVATE,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_NOMEMORY, callback.result());
  ASSERT_EQ(NULL, address);

  // Length too small.
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io.pp_resource(),
          -1,
          PP_FILEMAPPROTECTION_READ,
          PP_FILEMAPFLAG_PRIVATE,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_EQ(NULL, address);
  // TODO(dmichael): Check & test length that is not a multiple of page size???

  // Bad protection.
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io.pp_resource(),
          page_size,
          ~static_cast<uint32_t>(PP_FILEMAPPROTECTION_READ),
          PP_FILEMAPFLAG_PRIVATE,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_EQ(NULL, address);

  // No flags.
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io.pp_resource(),
          page_size,
          PP_FILEMAPPROTECTION_READ,
          0,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_EQ(NULL, address);

  // Both flags.
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io.pp_resource(),
          page_size,
          PP_FILEMAPPROTECTION_READ,
          PP_FILEMAPFLAG_SHARED | PP_FILEMAPFLAG_PRIVATE,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_EQ(NULL, address);

  // Bad flags.
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io.pp_resource(),
          page_size,
          PP_FILEMAPPROTECTION_READ,
          ~static_cast<uint32_t>(PP_FILEMAPFLAG_SHARED),
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_EQ(NULL, address);

  // Bad offset; not a multiple of page size.
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io.pp_resource(),
          page_size,
          PP_FILEMAPPROTECTION_READ,
          ~static_cast<uint32_t>(PP_FILEMAPFLAG_SHARED),
          1,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_EQ(NULL, address);

  // Unmap NULL.
  callback.WaitForResult(
      file_mapping_if_->Unmap(
          instance_->pp_instance(),
          NULL,
          page_size,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());

  // Unmap bad address.
  callback.WaitForResult(
      file_mapping_if_->Unmap(
          instance_->pp_instance(),
          reinterpret_cast<const void*>(0xdeadbeef),
          page_size,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());

  PASS();
}

std::string TestFileMapping::TestMap() {
  ASSERT_SUBTEST_SUCCESS(MapAndCheckResults(PP_FILEMAPPROTECTION_READ,
                                            PP_FILEMAPFLAG_SHARED));
  ASSERT_SUBTEST_SUCCESS(MapAndCheckResults(PP_FILEMAPPROTECTION_WRITE,
                                            PP_FILEMAPFLAG_SHARED));
  ASSERT_SUBTEST_SUCCESS(
      MapAndCheckResults(PP_FILEMAPPROTECTION_WRITE | PP_FILEMAPPROTECTION_READ,
                         PP_FILEMAPFLAG_SHARED));
  ASSERT_SUBTEST_SUCCESS(MapAndCheckResults(PP_FILEMAPPROTECTION_READ,
                                            PP_FILEMAPFLAG_PRIVATE));
  ASSERT_SUBTEST_SUCCESS(MapAndCheckResults(PP_FILEMAPPROTECTION_WRITE,
                                            PP_FILEMAPFLAG_PRIVATE));
  ASSERT_SUBTEST_SUCCESS(
      MapAndCheckResults(PP_FILEMAPPROTECTION_WRITE | PP_FILEMAPPROTECTION_READ,
                         PP_FILEMAPFLAG_PRIVATE));
  PASS();
}

std::string TestFileMapping::TestPartialRegions() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref1(file_system, "/mapped_file1");
  pp::FileRef file_ref2(file_system, "/mapped_file2");

  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  const int64_t page_size =
      file_mapping_if_->GetMapPageSize(instance_->pp_instance());
  const int64_t kNumPages = 3;
  std::string file_contents1(kNumPages * page_size, 'a');

  pp::FileIO file_io1(instance_);
  callback.WaitForResult(file_io1.Open(file_ref1,
                                       PP_FILEOPENFLAG_CREATE |
                                       PP_FILEOPENFLAG_TRUNCATE |
                                       PP_FILEOPENFLAG_READ |
                                       PP_FILEOPENFLAG_WRITE,
                                       callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_EQ(PP_OK, WriteEntireBuffer(instance_->pp_instance(),
                                     &file_io1,
                                     0,
                                     file_contents1,
                                     callback_type()));

  // TODO(dmichael): Use C++ interface.
  void* address = NULL;
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io1.pp_resource(),
          kNumPages * page_size,
          PP_FILEMAPPROTECTION_WRITE | PP_FILEMAPPROTECTION_READ,
          PP_FILEMAPFLAG_SHARED,
          0,
          &address,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_NE(NULL, address);

  // Unmap only the middle page.
  void* address_of_middle_page =
      reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address) + page_size);
  callback.WaitForResult(
      file_mapping_if_->Unmap(
          instance_->pp_instance(),
          address_of_middle_page,
          page_size,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Write another file, map it in to the middle hole that was left above.
  pp::FileIO file_io2(instance_);
  callback.WaitForResult(file_io2.Open(file_ref2,
                                       PP_FILEOPENFLAG_CREATE |
                                       PP_FILEOPENFLAG_TRUNCATE |
                                       PP_FILEOPENFLAG_READ |
                                       PP_FILEOPENFLAG_WRITE,
                                       callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());
  // This second file will have 1 page worth of data.
  std::string file_contents2(page_size, 'b');
  ASSERT_EQ(PP_OK, WriteEntireBuffer(instance_->pp_instance(),
                                     &file_io2,
                                     0,
                                     file_contents2,
                                     callback_type()));
  callback.WaitForResult(
      file_mapping_if_->Map(
          instance_->pp_instance(),
          file_io2.pp_resource(),
          page_size,
          PP_FILEMAPPROTECTION_WRITE | PP_FILEMAPPROTECTION_READ,
          PP_FILEMAPFLAG_SHARED | PP_FILEMAPFLAG_FIXED,
          0,
          &address_of_middle_page,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Write something else to the mapped region, then unmap, and see if it
  // gets written to both files. (Note we have to Unmap to make sure that the
  // write is committed).
  memset(address, 'c', kNumPages * page_size);
  callback.WaitForResult(
      file_mapping_if_->Unmap(
          instance_->pp_instance(), address, kNumPages * page_size,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  // The first and third page should have been written with 'c', but the
  // second page should be untouched.
  std::string expected_file_contents1 = std::string(page_size, 'c') +
                                        std::string(page_size, 'a') +
                                        std::string(page_size, 'c');
  std::string new_file_contents1;
  ASSERT_EQ(PP_OK, ReadEntireFile(instance_->pp_instance(),
                                  &file_io1,
                                  0,
                                  &new_file_contents1,
                                  callback_type()));
  ASSERT_EQ(expected_file_contents1, new_file_contents1);

  // The second file should have been entirely over-written.
  std::string expected_file_contents2 = std::string(page_size, 'c');
  std::string new_file_contents2;
  ASSERT_EQ(PP_OK, ReadEntireFile(instance_->pp_instance(),
                                  &file_io2,
                                  0,
                                  &new_file_contents2,
                                  callback_type()));
  ASSERT_EQ(expected_file_contents2, new_file_contents2);

  // TODO(dmichael): Test non-zero offset

  PASS();
}


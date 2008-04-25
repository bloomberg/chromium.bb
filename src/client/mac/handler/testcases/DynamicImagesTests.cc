/*
 *  DynamicImagesTests.cpp
 *  minidump_test
 *
 *  Created by Neal Sidhwaney on 4/17/08.
 *  Copyright 2008 Google Inc. All rights reserved.
 *
 */

#include "DynamicImagesTests.h"
#include "dynamic_images.h"

DynamicImagesTests test2(TEST_INVOCATION(DynamicImagesTests, ReadTaskMemoryTest));

DynamicImagesTests::DynamicImagesTests(TestInvocation *invocation)
    : TestCase(invocation)
{
}


DynamicImagesTests::~DynamicImagesTests()
{
}

void DynamicImagesTests::ReadTaskMemoryTest()
{
  kern_return_t kr;

  // pick test2 as a symbol we know to be valid to read
  // anything will work, really
  void *addr = (void*)&test2;

  void *buf;
  fprintf(stderr, "reading 0x%p\n",addr);
  buf = google_breakpad::ReadTaskMemory(mach_task_self(),
                                        addr,
                                        getpagesize(),
                                        &kr);

  CPTAssert(kr == KERN_SUCCESS);

  CPTAssert(buf != NULL);

  CPTAssert(0 == memcmp(buf,(const void*)addr,getpagesize()));

  free(buf);

}

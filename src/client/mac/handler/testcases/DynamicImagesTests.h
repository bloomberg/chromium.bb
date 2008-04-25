/*
 *  DynamicImagesTests.h
 *  minidump_test
 *
 *  Created by Neal Sidhwaney on 4/17/08.
 *  Copyright 2008 Google Inc. All rights reserved.
 *
 */

#include <CPlusTest/CPlusTest.h>


class DynamicImagesTests : public TestCase {
public:
    DynamicImagesTests(TestInvocation* invocation);
    virtual ~DynamicImagesTests();

    void ReadTaskMemoryTest();
};

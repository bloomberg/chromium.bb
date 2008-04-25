/*
 *  breakpad_nlist_test.h
 *  minidump_test
 *
 *  Created by Neal Sidhwaney on 4/13/08.
 *  Copyright 2008 Google Inc. All rights reserved.
 *
 */

#include <CPlusTest/CPlusTest.h>

class BreakpadNlistTest : public TestCase {
private:

  // nm dumps multiple addresses for the same symbol in
  // /usr/lib/dyld. So we track those so we don't report failures
  // in mismatches between what our nlist returns and what nm has
  // for the duplicate symbols.
  bool IsSymbolMoreThanOnceInDyld(const char *symbolName);

public:
  BreakpadNlistTest(TestInvocation* invocation);
  virtual ~BreakpadNlistTest();


  /* This test case runs nm on /usr/lib/dyld and then compares the
     output of every symbol to what our nlist implementation returns */
  void CompareToNM();
};

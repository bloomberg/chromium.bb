/*
 *  BreakpadNlistTest.h
 *  minidump_test
 *
 *  Created by Neal Sidhwaney on 4/13/08.
 *  Copyright 2008 Google Inc. All rights reserved.
 *
 */

#include <CPlusTest/CPlusTest.h>

/*
__Z41__static_initialization_and_destruction_0ii
__Z41__static_initialization_and_destruction_0ii
__Z41__static_initialization_and_destruction_0ii
___tcf_0
___tcf_0
___tcf_0
___tcf_1
_read_encoded_value_with_base
_read_sleb128
_read_uleb128

*/

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

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/key_input.h"
#import "remoting/ios/key_map_us.h"

#include <vector>

#import "base/compiler_specific.h"
#import "testing/gtest_mac.h"

@interface KeyInputDelegateTester : NSObject<KeyInputDelegate> {
 @private
  std::vector<uint32_t> _keyList;
}

@property(nonatomic, assign) int numKeysDown;
@property(nonatomic, assign) BOOL wasDismissed;

- (std::vector<uint32_t>&)getKeyList;

@end

@implementation KeyInputDelegateTester

- (std::vector<uint32_t>&)getKeyList {
  return _keyList;
}

- (void)keyboardDismissed {
  // This can not be tested, because we can not set |keyInput_| as
  // FirstResponder in this test harness
  _wasDismissed = true;
}

- (void)keyboardActionKeyCode:(uint32_t)keyPressed isKeyDown:(BOOL)keyDown {
  if (keyDown) {
    _keyList.push_back(keyPressed);
    _numKeysDown++;
  } else {
    _numKeysDown--;
  }
}

@end

namespace remoting {

class KeyInputTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    keyInput_ = [[KeyInput allocWithZone:nil] init];
    delegateTester_ = [[KeyInputDelegateTester alloc] init];
    keyInput_.delegate = delegateTester_;
  }

  KeyInput* keyInput_;
  KeyInputDelegateTester* delegateTester_;
};

TEST_F(KeyInputTest, SendKey) {
  // Empty
  [keyInput_ insertText:@""];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(0, [delegateTester_ getKeyList].size());

  // Value is out of bounds
  [keyInput_ insertText:@"ó"];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(0, [delegateTester_ getKeyList].size());

  // Lower case
  [keyInput_ insertText:@"a"];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(1, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeUS['a'], [delegateTester_ getKeyList][0]);
  // Upper Case
  [delegateTester_ getKeyList].clear();
  [keyInput_ insertText:@"A"];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(2, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeUS[kShiftIndex], [delegateTester_ getKeyList][0]);
  ASSERT_EQ(kKeyCodeUS['A'], [delegateTester_ getKeyList][1]);

  // Multiple characters and mixed case
  [delegateTester_ getKeyList].clear();
  [keyInput_ insertText:@"ABCabc"];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(9, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeUS[kShiftIndex], [delegateTester_ getKeyList][0]);
  ASSERT_EQ(kKeyCodeUS['A'], [delegateTester_ getKeyList][1]);
  ASSERT_EQ(kKeyCodeUS[kShiftIndex], [delegateTester_ getKeyList][2]);
  ASSERT_EQ(kKeyCodeUS['B'], [delegateTester_ getKeyList][3]);
  ASSERT_EQ(kKeyCodeUS[kShiftIndex], [delegateTester_ getKeyList][4]);
  ASSERT_EQ(kKeyCodeUS['C'], [delegateTester_ getKeyList][5]);
  ASSERT_EQ(kKeyCodeUS['a'], [delegateTester_ getKeyList][6]);
  ASSERT_EQ(kKeyCodeUS['b'], [delegateTester_ getKeyList][7]);
  ASSERT_EQ(kKeyCodeUS['c'], [delegateTester_ getKeyList][8]);
}

TEST_F(KeyInputTest, CtrlAltDel) {
  [keyInput_ ctrlAltDel];

  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(3, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeUS[kCtrlIndex], [delegateTester_ getKeyList][0]);
  ASSERT_EQ(kKeyCodeUS[kAltIndex], [delegateTester_ getKeyList][1]);
  ASSERT_EQ(kKeyCodeUS[kDelIndex], [delegateTester_ getKeyList][2]);
}

TEST_F(KeyInputTest, Backspace) {
  [keyInput_ deleteBackward];

  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(1, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeUS[kBackspaceIndex], [delegateTester_ getKeyList][0]);
}

}  // namespace remoting
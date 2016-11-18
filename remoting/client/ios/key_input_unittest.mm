// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/key_input.h"
#import "remoting/client/ios/key_map_us.h"

#include <vector>

#import "base/compiler_specific.h"
#import "testing/gtest_mac.h"

@interface KeyInputDelegateTester : NSObject<KeyInputDelegate> {
 @private
  std::vector<uint32_t> _keyList;
}

@property(nonatomic, assign) int numKeysDown;
@property(nonatomic, assign) BOOL wasDismissed;
@property(nonatomic, assign) BOOL wasShown;

- (std::vector<uint32_t>&)getKeyList;

@end

@implementation KeyInputDelegateTester

@synthesize numKeysDown = _numKeysDown;
@synthesize wasDismissed = _wasDismissed;
@synthesize wasShown = _wasShown;

- (std::vector<uint32_t>&)getKeyList {
  return _keyList;
}

- (void)keyboardShown {
  _wasShown = true;
}

- (void)keyboardDismissed {
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
  virtual void SetUp() override {
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
  ASSERT_EQ(0UL, [delegateTester_ getKeyList].size());

  // Value is out of bounds
  [keyInput_ insertText:@"ó"];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(0UL, [delegateTester_ getKeyList].size());

  // Lower case
  [keyInput_ insertText:@"a"];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(1UL, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeMetaUS[(int)'a'].code, [delegateTester_ getKeyList][0]);
  // Upper Case
  [delegateTester_ getKeyList].clear();
  [keyInput_ insertText:@"A"];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(2UL, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeMetaUS[kShiftIndex].code, [delegateTester_ getKeyList][0]);
  ASSERT_EQ(kKeyCodeMetaUS[(int)'A'].code, [delegateTester_ getKeyList][1]);

  // Multiple characters and mixed case
  [delegateTester_ getKeyList].clear();
  [keyInput_ insertText:@"ABCabc"];
  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(9UL, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeMetaUS[kShiftIndex].code, [delegateTester_ getKeyList][0]);
  ASSERT_EQ(kKeyCodeMetaUS[(int)'A'].code, [delegateTester_ getKeyList][1]);
  ASSERT_EQ(kKeyCodeMetaUS[kShiftIndex].code, [delegateTester_ getKeyList][2]);
  ASSERT_EQ(kKeyCodeMetaUS[(int)'B'].code, [delegateTester_ getKeyList][3]);
  ASSERT_EQ(kKeyCodeMetaUS[kShiftIndex].code, [delegateTester_ getKeyList][4]);
  ASSERT_EQ(kKeyCodeMetaUS[(int)'C'].code, [delegateTester_ getKeyList][5]);
  ASSERT_EQ(kKeyCodeMetaUS[(int)'a'].code, [delegateTester_ getKeyList][6]);
  ASSERT_EQ(kKeyCodeMetaUS[(int)'b'].code, [delegateTester_ getKeyList][7]);
  ASSERT_EQ(kKeyCodeMetaUS[(int)'c'].code, [delegateTester_ getKeyList][8]);
}

TEST_F(KeyInputTest, CtrlAltDel) {
  [keyInput_ ctrlAltDel];

  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(3UL, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeMetaUS[kCtrlIndex].code, [delegateTester_ getKeyList][0]);
  ASSERT_EQ(kKeyCodeMetaUS[kAltIndex].code, [delegateTester_ getKeyList][1]);
  ASSERT_EQ(kKeyCodeMetaUS[kDelIndex].code, [delegateTester_ getKeyList][2]);
}

TEST_F(KeyInputTest, Backspace) {
  [keyInput_ deleteBackward];

  ASSERT_EQ(0, delegateTester_.numKeysDown);
  ASSERT_EQ(1UL, [delegateTester_ getKeyList].size());
  ASSERT_EQ(kKeyCodeMetaUS[kBackspaceIndex].code,
            [delegateTester_ getKeyList][0]);
}

TEST_F(KeyInputTest, KeyboardShown) {
  ASSERT_FALSE(delegateTester_.wasShown);

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:UIKeyboardDidShowNotification object:nil];

  ASSERT_TRUE(delegateTester_.wasShown);
}

TEST_F(KeyInputTest, KeyboardDismissed) {
  ASSERT_FALSE(delegateTester_.wasDismissed);

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:UIKeyboardWillHideNotification object:nil];

  ASSERT_TRUE(delegateTester_.wasDismissed);
}

}  // namespace remoting

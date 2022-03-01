// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_TEST_FAKE_SCENE_STATE_H_
#define IOS_CHROME_BROWSER_UI_MAIN_TEST_FAKE_SCENE_STATE_H_

#import "ios/chrome/browser/ui/main/scene_state.h"
#import "url/gurl.h"

// Test double for SceneState, created with appropriate interface objects backed
// by a browser. No incognito interface is created by default.
// Any test using objects of this class must include a TaskEnvironment member
// because of the embedded test browser state.
@interface FakeSceneState : SceneState

// Creates an array of |count| instances, without any associated AppState.
+ (NSArray<FakeSceneState*>*)sceneArrayWithCount:(int)count;

// Append a suitable web state test double to the receiver's main interface.
- (void)appendWebStateWithURL:(const GURL)URL;
// Append |count| web states, all with |url| as the current URL, to the
- (void)appendWebStatesWithURL:(const GURL)URL count:(int)count;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_TEST_FAKE_SCENE_STATE_H_

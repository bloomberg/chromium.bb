// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_EXAMPLE_VIEW_CONTROLLER_H_
#define REMOTING_CLIENT_IOS_EXAMPLE_VIEW_CONTROLLER_H_

#import <GLKit/GLKit.h>

@interface ExampleViewController : GLKViewController {
 @private

  // The GLES2 context being drawn too.
  EAGLContext* _context;

}

@end

#endif  // REMOTING_CLIENT_IOS_EXAMPLE_VIEW_CONTROLLER_H_

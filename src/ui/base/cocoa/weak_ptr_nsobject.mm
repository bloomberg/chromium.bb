// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/weak_ptr_nsobject.h"

#import <Foundation/Foundation.h>

@interface WeakPtrNSObject : NSObject {
 @public
  void* weak_ptr;
}
@end
@implementation WeakPtrNSObject
@end

namespace ui {
namespace internal {

WeakPtrNSObject* WeakPtrNSObjectFactoryBase::Create(void* owner) {
  WeakPtrNSObject* object = [[WeakPtrNSObject alloc] init];
  object->weak_ptr = owner;
  return object;
}

void* WeakPtrNSObjectFactoryBase::UnWrap(WeakPtrNSObject* handle) {
  return handle->weak_ptr;
}

void WeakPtrNSObjectFactoryBase::InvalidateAndRelease(WeakPtrNSObject* handle) {
  handle->weak_ptr = nullptr;
  [handle release];
}

}  // namespace internal
}  // namespace ui

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/show_signin_command.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShowSigninCommand

@synthesize operation = _operation;
@synthesize identity = _identity;
@synthesize accessPoint = _accessPoint;
@synthesize promoAction = _promoAction;
@synthesize callback = _callback;

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                         identity:(ChromeIdentity*)identity
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
                         callback:
                             (ShowSigninCommandCompletionCallback)callback {
  if ((self = [super init])) {
    DCHECK(operation == AUTHENTICATION_OPERATION_SIGNIN || identity == nil);
    _operation = operation;
    _identity = identity;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
    _callback = [callback copy];
  }
  return self;
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction {
  return [self initWithOperation:operation
                        identity:nil
                     accessPoint:accessPoint
                     promoAction:promoAction
                        callback:nil];
}

- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint {
  return [self initWithOperation:operation
                        identity:nil
                     accessPoint:accessPoint
                     promoAction:signin_metrics::PromoAction::
                                     PROMO_ACTION_NO_SIGNIN_PROMO
                        callback:nil];
}

@end

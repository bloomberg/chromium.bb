// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_SPECIAL_USER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_SPECIAL_USER_PROVIDER_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"

// SpecialUserProvider allows embedders to provide functionality related
// to special users.
class SpecialUserProvider {
 public:
  SpecialUserProvider();
  virtual ~SpecialUserProvider();
  // Returns whether the user needs special handling.
  virtual bool IsSpecialUser();
  // Records UMA metrics about user type.
  virtual void RecordUserTypeMetrics();

 private:
  DISALLOW_COPY_AND_ASSIGN(SpecialUserProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_SPECIAL_USER_PROVIDER_H_

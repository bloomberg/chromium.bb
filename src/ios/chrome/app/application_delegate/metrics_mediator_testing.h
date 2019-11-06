// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_TESTING_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_TESTING_H_

#include "net/base/network_change_notifier.h"

@interface MetricsMediator (TestingAddition)
- (void)processCrashReportsPresentAtStartup;
- (void)connectionTypeChanged:(net::NetworkChangeNotifier::ConnectionType)type;
- (void)setBreakpadUploadingEnabled:(BOOL)enableUploading;
- (void)setReporting:(BOOL)enableReporting;
- (BOOL)isMetricsReportingEnabledWifiOnly;
+ (void)recordNumTabAtStartup:(int)numTabs;
+ (void)recordNumTabAtResume:(int)numTabs;
@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_TESTING_H_

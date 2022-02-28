// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_BRIDGE_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_BRIDGE_H_

#import <WebKit/WebKit.h>

@class DownloadNativeTaskBridge;

@protocol DownloadNativeTaskBridgeReadyDelegate <NSObject>

// Used to set response url, content length, mimetype and http response headers
// in CRWWkNavigationHandler so method can interact with WKWebView.
- (void)onDownloadNativeTaskBridgeReadyForDownload:
    (DownloadNativeTaskBridge*)bridge API_AVAILABLE(ios(15));

@end

// Class used to create a download task object that handles downloads through
// WKDownload. |progressionHandler| and |completionHandler| are instantiated
// as private instance variables in the implementation file in ios/web/download
@interface DownloadNativeTaskBridge : NSObject <WKDownloadDelegate>

// Default initializer. |download| and |readyDelegate| must be non-nil.
- (instancetype)initWithDownload:(WKDownload*)download
           downloadReadyDelegate:
               (id<DownloadNativeTaskBridgeReadyDelegate>)readyDelegate
    NS_DESIGNATED_INITIALIZER API_AVAILABLE(ios(15));

- (instancetype)init NS_UNAVAILABLE;

// Cancels download
- (void)cancel;

// Starts download and sets |progressionHandler| and |completionHandler|
- (void)startDownload:(NSURL*)url
    progressionHandler:(void (^)())progressionHander
     completionHandler:(void (^)(int error_code))completionHandler;

@property(nonatomic, readonly) WKDownload* download API_AVAILABLE(ios(15));
@property(nonatomic, readonly) NSURLResponse* response;
@property(nonatomic, readonly) NSString* suggestedFilename;
@property(nonatomic, readonly) NSProgress* progress;
@property(nonatomic, readonly) NSURL* urlForDownload;

@end

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_NATIVE_TASK_BRIDGE_H_

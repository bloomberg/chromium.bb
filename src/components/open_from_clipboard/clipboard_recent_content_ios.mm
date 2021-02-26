// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/open_from_clipboard/clipboard_recent_content_ios.h"

#import <CommonCrypto/CommonDigest.h>
#include <stddef.h>
#include <stdint.h>
#import <UIKit/UIKit.h>

#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Schemes accepted by the ClipboardRecentContentIOS.
const char* kAuthorizedSchemes[] = {
    url::kHttpScheme, url::kHttpsScheme, url::kDataScheme, url::kAboutScheme,
};

// Get the list of authorized schemes.
NSSet<NSString*>* getAuthorizedSchemeList(
    const std::string& application_scheme) {
  NSMutableSet<NSString*>* schemes = [NSMutableSet set];
  for (size_t i = 0; i < base::size(kAuthorizedSchemes); ++i) {
    [schemes addObject:base::SysUTF8ToNSString(kAuthorizedSchemes[i])];
  }
  if (!application_scheme.empty()) {
    [schemes addObject:base::SysUTF8ToNSString(application_scheme)];
  }

  return [schemes copy];
}

ContentType ContentTypeFromClipboardContentType(ClipboardContentType type) {
  switch (type) {
    case ClipboardContentType::URL:
      return ContentTypeURL;
    case ClipboardContentType::Text:
      return ContentTypeText;
    case ClipboardContentType::Image:
      return ContentTypeImage;
  }
}

ClipboardContentType ClipboardContentTypeFromContentType(ContentType type) {
  if ([type isEqualToString:ContentTypeURL]) {
    return ClipboardContentType::URL;
  } else if ([type isEqualToString:ContentTypeText]) {
    return ClipboardContentType::Text;
  } else if ([type isEqualToString:ContentTypeImage]) {
    return ClipboardContentType::Image;
  }
  NOTREACHED();
  return ClipboardContentType::Text;
}

}  // namespace

@interface ClipboardRecentContentDelegateImpl
    : NSObject<ClipboardRecentContentDelegate>
@end

@implementation ClipboardRecentContentDelegateImpl

- (void)onClipboardChanged {
  base::RecordAction(base::UserMetricsAction("MobileOmniboxClipboardChanged"));
}

@end

ClipboardRecentContentIOS::ClipboardRecentContentIOS(
    const std::string& application_scheme,
    NSUserDefaults* group_user_defaults)
    : ClipboardRecentContentIOS([[ClipboardRecentContentImplIOS alloc]
             initWithMaxAge:MaximumAgeOfClipboard().InSecondsF()
          authorizedSchemes:getAuthorizedSchemeList(application_scheme)
               userDefaults:group_user_defaults
                   delegate:[[ClipboardRecentContentDelegateImpl alloc]
                                init]]) {}

ClipboardRecentContentIOS::ClipboardRecentContentIOS(
    ClipboardRecentContentImplIOS* implementation) {
  implementation_ = implementation;
}

base::Optional<GURL> ClipboardRecentContentIOS::GetRecentURLFromClipboard() {
  NSURL* url_from_pasteboard = [implementation_ recentURLFromClipboard];
  GURL converted_url = net::GURLWithNSURL(url_from_pasteboard);
  if (!converted_url.is_valid()) {
    return base::nullopt;
  }

  return converted_url;
}

base::Optional<base::string16>
ClipboardRecentContentIOS::GetRecentTextFromClipboard() {
  NSString* text_from_pasteboard = [implementation_ recentTextFromClipboard];
  if (!text_from_pasteboard) {
    return base::nullopt;
  }

  return base::SysNSStringToUTF16(text_from_pasteboard);
}

bool ClipboardRecentContentIOS::HasRecentImageFromClipboard() {
  return GetRecentImageFromClipboardInternal().has_value();
}

void ClipboardRecentContentIOS::HasRecentContentFromClipboard(
    std::set<ClipboardContentType> types,
    HasDataCallback callback) {
  __block HasDataCallback callback_for_block = std::move(callback);
  NSMutableSet<ContentType>* ios_types = [NSMutableSet set];
  for (ClipboardContentType type : types) {
    [ios_types addObject:ContentTypeFromClipboardContentType(type)];
  }
  // The iOS methods for checking clipboard content call their callbacks on an
  // arbitrary thread. As Objective-C doesn't have very good thread-management
  // techniques, make sure this method calls its callback on the same thread
  // that it was called on.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunnerHandle::Get();
  [implementation_
      hasContentMatchingTypes:ios_types
            completionHandler:^(NSSet<ContentType>* results) {
              std::set<ClipboardContentType> matching_types;
              for (ContentType type in results) {
                matching_types.insert(
                    ClipboardContentTypeFromContentType(type));
              }
              task_runner->PostTask(
                  FROM_HERE, base::BindOnce(^{
                    std::move(callback_for_block).Run(matching_types);
                  }));
            }];
}

void ClipboardRecentContentIOS::GetRecentURLFromClipboard(
    GetRecentURLCallback callback) {
  __block GetRecentURLCallback callback_for_block = std::move(callback);
  // The iOS methods for checking clipboard content call their callbacks on an
  // arbitrary thread. As Objective-C doesn't have very good thread-management
  // techniques, make sure this method calls its callback on the same thread
  // that it was called on.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunnerHandle::Get();
  [implementation_ recentURLFromClipboardAsync:^(NSURL* url) {
    GURL converted_url = net::GURLWithNSURL(url);
    if (!converted_url.is_valid()) {
      task_runner->PostTask(FROM_HERE, base::BindOnce(^{
                              std::move(callback_for_block).Run(base::nullopt);
                            }));
      return;
    }
    task_runner->PostTask(FROM_HERE, base::BindOnce(^{
                            std::move(callback_for_block).Run(converted_url);
                          }));
  }];
}

void ClipboardRecentContentIOS::GetRecentTextFromClipboard(
    GetRecentTextCallback callback) {
  __block GetRecentTextCallback callback_for_block = std::move(callback);
  // The iOS methods for checking clipboard content call their callbacks on an
  // arbitrary thread. As Objective-C doesn't have very good thread-management
  // techniques, make sure this method calls its callback on the same thread
  // that it was called on.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunnerHandle::Get();
  [implementation_ recentTextFromClipboardAsync:^(NSString* text) {
    if (!text) {
      task_runner->PostTask(FROM_HERE, base::BindOnce(^{
                              std::move(callback_for_block).Run(base::nullopt);
                            }));
      return;
    }
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(^{
          std::move(callback_for_block).Run(base::SysNSStringToUTF16(text));
        }));
  }];
}

void ClipboardRecentContentIOS::GetRecentImageFromClipboard(
    GetRecentImageCallback callback) {
  __block GetRecentImageCallback callback_for_block = std::move(callback);
  // The iOS methods for checking clipboard content call their callbacks on an
  // arbitrary thread. As Objective-C doesn't have very good thread-management
  // techniques, make sure this method calls its callback on the same thread
  // that it was called on.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunnerHandle::Get();
  [implementation_ recentImageFromClipboardAsync:^(UIImage* image) {
    if (!image) {
      task_runner->PostTask(FROM_HERE, base::BindOnce(^{
                              std::move(callback_for_block).Run(base::nullopt);
                            }));
      return;
    }
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(^{
          std::move(callback_for_block).Run(gfx::Image(image));
        }));
  }];
}

ClipboardRecentContentIOS::~ClipboardRecentContentIOS() {}

base::TimeDelta ClipboardRecentContentIOS::GetClipboardContentAge() const {
  return base::TimeDelta::FromSeconds(
      static_cast<int64_t>([implementation_ clipboardContentAge]));
}

void ClipboardRecentContentIOS::SuppressClipboardContent() {
  [implementation_ suppressClipboardContent];
}

void ClipboardRecentContentIOS::ClearClipboardContent() {
  NOTIMPLEMENTED();
  return;
}

base::Optional<gfx::Image>
ClipboardRecentContentIOS::GetRecentImageFromClipboardInternal() {
  UIImage* image_from_pasteboard = [implementation_ recentImageFromClipboard];
  if (!image_from_pasteboard) {
    return base::nullopt;
  }

  return gfx::Image(image_from_pasteboard);
}

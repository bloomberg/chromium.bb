// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/wk_web_view_util.h"

#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

bool RequiresContentFilterBlockingWorkaround() {
  // This is fixed in iOS13 beta 7.
  if (@available(iOS 13, *))
    return false;

  if (@available(iOS 12.2, *))
    return true;

  return false;
}

bool RequiresProvisionalNavigationFailureWorkaround() {
  if (@available(iOS 12.2, *))
    return true;
  return false;
}

void CreateFullPagePdf(WKWebView* web_view,
                       base::OnceCallback<void(NSData*)> callback) {

  if (!web_view) {
    std::move(callback).Run(nil);
    return;
  }

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    __block base::OnceCallback<void(NSData*)> callback_for_block =
        std::move(callback);
    WKPDFConfiguration* pdf_configuration = [[WKPDFConfiguration alloc] init];
    [web_view createPDFWithConfiguration:pdf_configuration
                       completionHandler:^(NSData* pdf_document_data,
                                           NSError* error) {
                         std::move(callback_for_block).Run(pdf_document_data);
                       }];
    return;
  }
#endif

  // PDF generation is only supported on iOS 14+.
  std::move(callback).Run(nil);
}
}  // namespace web

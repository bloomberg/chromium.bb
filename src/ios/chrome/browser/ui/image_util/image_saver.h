// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_SAVER_H_
#define IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_SAVER_H_

#import <UIKit/UIKit.h>

#include "components/image_fetcher/core/request_metadata.h"

class GURL;
namespace web {
class WebState;
struct Referrer;
}

// Object saving images to the system's album.
@interface ImageSaver : NSObject

// Init the ImageSaver with a |baseViewController| used to display alerts.
- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController;

// Fetches and saves the image at |url| to the system's album. |web_state| is
// used for fetching image data by JavaScript and must not be nullptr.
// |referrer| is used for download.
- (void)saveImageAtURL:(const GURL&)url
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_SAVER_H_

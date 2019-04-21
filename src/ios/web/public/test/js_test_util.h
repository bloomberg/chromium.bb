// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/compiler_specific.h"

@class CRWJSInjectionManager;
@class CRWJSInjectionReceiver;

namespace web {
namespace test {

// These functions synchronously execute JavaScript and return result as id.
// id will be backed up by different classes depending on resulting JS type:
// NSString (string), NSNumber (number or boolean), NSDictionary (object),
// NSArray (array), NSNull (null), NSDate (Date), nil (undefined or execution
// exception).

// Executes JavaScript on the |manager| and returns the result as an id.
id ExecuteJavaScript(CRWJSInjectionManager* manager, NSString* script);

// Executes JavaScript on the |receiver| and returns the result as an id.
id ExecuteJavaScript(CRWJSInjectionReceiver* receiver, NSString* script);

// Executes JavaScript on |web_view| and returns the result as an id.
// |error| can be null and will be updated only if script execution fails.
id ExecuteJavaScript(WKWebView* web_view,
                     NSString* script,
                     NSError* __autoreleasing* error);

// Executes JavaScript on |web_view| and returns the result as an id.
// Fails if there was an error during script execution.
id ExecuteJavaScript(WKWebView* web_view, NSString* script);

// Synchronously loads |html| into |web_view|. Returns true is successful or
// false if the |web_view| never finishes loading.
bool LoadHtml(WKWebView* web_view,
              NSString* html,
              NSURL* base_url) WARN_UNUSED_RESULT;

// Waits until custom javascript is injected into __gCrWeb.
bool WaitForInjectedScripts(WKWebView* web_view) WARN_UNUSED_RESULT;

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_

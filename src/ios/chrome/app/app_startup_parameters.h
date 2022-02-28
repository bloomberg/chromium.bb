// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APP_STARTUP_PARAMETERS_H_
#define IOS_CHROME_APP_APP_STARTUP_PARAMETERS_H_

#import <Foundation/Foundation.h>

#include <map>
#include <string>
#include <vector>

enum class ApplicationModeForTabOpening { NORMAL, INCOGNITO, CURRENT };

enum NTPTabOpeningPostOpeningAction {
  // No action should be done
  NO_ACTION = 0,
  START_VOICE_SEARCH,
  START_QR_CODE_SCANNER,
  FOCUS_OMNIBOX,
  NTP_TAB_OPENING_POST_OPENING_ACTION_COUNT,
};

class GURL;

// This class stores all the parameters relevant to the app startup in case
// of launch from another app.
@interface AppStartupParameters : NSObject

// The URL that should be opened. This may not always be the same URL as the one
// that was received. The reason for this is in the case of Universal Link
// navigation where we may want to open up a fallback URL e.g., the New Tab Page
// instead of the actual universal link.
@property(nonatomic, readonly, assign) const GURL& externalURL;

// Original URL that should be opened. May or may not be the same as
// |externalURL|.
@property(nonatomic, readonly, assign) const GURL& completeURL;

// The list of URLs to open. First URL in the vector is the same
// as |externalURL|.
@property(nonatomic, readonly, assign) const std::vector<GURL>& URLs;

// The URL query string parameters in the case that the app was launched as a
// result of Universal Link navigation. The map associates query string
// parameters with their corresponding value.
@property(nonatomic, assign) std::map<std::string, std::string>
    externalURLParams;

// Boolean to track if the app should launch in incognito mode.
@property(nonatomic, readwrite, assign) BOOL launchInIncognito;
// The mode in which the tab must be opened.
@property(nonatomic, readonly) ApplicationModeForTabOpening applicationMode;
// Action to be taken after opening the initial NTP.
@property(nonatomic, readwrite, assign)
    NTPTabOpeningPostOpeningAction postOpeningAction;
// Boolean to track if a Payment Request response is requested at startup.
@property(nonatomic, readwrite, assign) BOOL completePaymentRequest;
// Text query that should be executed on startup.
@property(nonatomic, readwrite, copy) NSString* textQuery;
// Data for UIImage for image query that should be executed on startup.
@property(nonatomic, readwrite, strong) NSData* imageSearchData;
// Boolean to track if the app is open in an user unexpected mode.
// When a certain enterprise policy has been set, it's possible that one browser
// mode is disabled. When the user intends to open an unavailable mode of
// Chrome, the browser won't proceed in that disabled mode, and it will signal
// to the user that a different mode is opened.
@property(nonatomic, readwrite, getter=isUnexpectedMode) BOOL unexpectedMode;
// Boolean to track whether the app was opened via a custom scheme from another
// first-party app.
@property(nonatomic, readwrite, assign) BOOL openedViaFirstPartyScheme;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                        completeURL:(const GURL&)completeURL
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithURLs:(const std::vector<GURL>&)URLs;

@end

#endif  // IOS_CHROME_APP_APP_STARTUP_PARAMETERS_H_

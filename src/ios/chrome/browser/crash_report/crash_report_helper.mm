// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_report_helper.h"

#import <Foundation/Foundation.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/upload_list/crash_upload_list.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/public/web_thread.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TabModelObserver that allows loaded urls to be sent to the crash server.
@interface CrashReporterURLObserver
    : NSObject <TabModelObserver, CRWWebStateObserver> {
 @private
  // Map associating the tab id to the breakpad key used to keep track of the
  // loaded URL.
  NSMutableDictionary* breakpadKeyByTabId_;
  // List of keys to use for recording URLs. This list is sorted such that a new
  // tab must use the first key in this list to record its URLs.
  NSMutableArray* breakpadKeys_;
  // The WebStateObserverBridge used to register self as a WebStateObserver
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Forwards observer methods for all WebStates in the WebStateList monitored
  // by the CrashReporterURLObserver.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;
}
+ (CrashReporterURLObserver*)uniqueInstance;
// Removes the URL for the tab with the given id from the URLs sent to the crash
// server.
- (void)removeTabId:(NSString*)tabId;
// Records the given URL associated to the given id to the list of URLs to send
// to the crash server. If |pending| is true, the URL is one that is
// expected to start loading, but hasn't actually been seen yet.
- (void)recordURL:(NSString*)url
         forTabId:(NSString*)tabId
          pending:(BOOL)pending;
// Observes |webState| by this instance of the CrashReporterURLObserver.
- (void)observeWebState:(web::WebState*)webState;
// Stop Observing |webState| by this instance of the CrashReporterURLObserver.
- (void)stopObservingWebState:(web::WebState*)webState;
// Observes |tabModel| by this instance of the CrashReporterURLObserver.
- (void)observeTabModel:(TabModel*)tabModel;
// Stop Observing |tabModel| by this instance of the CrashReporterURLObserver.
- (void)stopObservingTabModel:(TabModel*)tabModel;

@end

// TabModelObserver that some tabs stats to be sent to the crash server.
@interface CrashReporterTabStateObserver : NSObject<TabModelObserver> {
 @private
  // Map associating the tab id to an object describing the current state of the
  // tab.
  NSMutableDictionary* tabCurrentStateByTabId_;
}
+ (CrashReporterURLObserver*)uniqueInstance;
// Removes the stats for the tab tabId
- (void)removeTabId:(NSString*)tabId;
// Callback for the kTabClosingCurrentDocumentNotificationForCrashReporting
// notification. Removes document related information from
// tabCurrentStateByTabId_ by calling closingDocumentInTab:tabId.
- (void)closingDocument:(NSNotification*)notification;
// Removes document related information from tabCurrentStateByTabId_.
- (void)closingDocumentInTab:(NSString*)tabId;
// Callback for the  kTabIsShowingExportableNotificationForCrashReporting
// notification. Sets the mimeType in tabCurrentStateByTabId_.
- (void)showingExportableDocument:(NSNotification*)notification;

// Sets a tab |tabId| specific information with key |key| and value |value| in
// tabCurrentStateByTabId_.
- (void)setTabInfo:(NSString*)key
         withValue:(NSString*)value
            forTab:(NSString*)tabId;
// Retrieves the |key| information for tab |tabId|.
- (id)getTabInfo:(NSString*)key forTab:(NSString*)tabId;
// Removes the |key| information for tab |tabId|
- (void)removeTabInfo:(NSString*)key forTab:(NSString*)tabId;
@end

namespace {

// Returns the breakpad key to use for a pending URL corresponding to the
// same tab that is using |key|.
NSString* PendingURLKeyForKey(NSString* key) {
  return [key stringByAppendingString:@"-pending"];
}

// Max number of urls to send. This must be kept low for privacy issue as well
// as because breakpad does limit the total number of parameters to 64.
const int kNumberOfURLsToSend = 1;
}

@implementation CrashReporterURLObserver

+ (CrashReporterURLObserver*)uniqueInstance {
  static CrashReporterURLObserver* instance =
      [[CrashReporterURLObserver alloc] init];
  return instance;
}

- (id)init {
  if ((self = [super init])) {
    breakpadKeyByTabId_ =
        [[NSMutableDictionary alloc] initWithCapacity:kNumberOfURLsToSend];
    breakpadKeys_ =
        [[NSMutableArray alloc] initWithCapacity:kNumberOfURLsToSend];
    for (int i = 0; i < kNumberOfURLsToSend; ++i)
      [breakpadKeys_ addObject:[NSString stringWithFormat:@"url%d", i]];
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

- (void)removeTabId:(NSString*)tabId {
  NSString* key = [breakpadKeyByTabId_ objectForKey:tabId];
  if (!key)
    return;
  breakpad_helper::RemoveReportParameter(key);
  breakpad_helper::RemoveReportParameter(PendingURLKeyForKey(key));
  [breakpadKeyByTabId_ removeObjectForKey:tabId];
  [breakpadKeys_ removeObject:key];
  [breakpadKeys_ insertObject:key atIndex:0];
}

- (void)recordURL:(NSString*)url
         forTabId:(NSString*)tabId
          pending:(BOOL)pending {
  NSString* breakpadKey = [breakpadKeyByTabId_ objectForKey:tabId];
  BOOL reusingKey = NO;
  if (!breakpadKey) {
    // Get the first breakpad key and push it back at the end of the keys.
    breakpadKey = [breakpadKeys_ objectAtIndex:0];
    [breakpadKeys_ removeObject:breakpadKey];
    [breakpadKeys_ addObject:breakpadKey];
    // Remove the current mapping to the breakpad key.
    for (NSString* tabId in
         [breakpadKeyByTabId_ allKeysForObject:breakpadKey]) {
      reusingKey = YES;
      [breakpadKeyByTabId_ removeObjectForKey:tabId];
    }
    // Associate the breakpad key to the tab id.
    [breakpadKeyByTabId_ setObject:breakpadKey forKey:tabId];
  }
  NSString* pendingKey = PendingURLKeyForKey(breakpadKey);
  if (pending) {
    if (reusingKey)
      breakpad_helper::RemoveReportParameter(breakpadKey);
    breakpad_helper::AddReportParameter(pendingKey, url, true);
  } else {
    breakpad_helper::AddReportParameter(breakpadKey, url, true);
    breakpad_helper::RemoveReportParameter(pendingKey);
  }
}

- (void)observeWebState:(web::WebState*)webState {
  webState->AddObserver(_webStateObserver.get());
}

- (void)stopObservingWebState:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserver.get());
}

- (void)observeTabModel:(TabModel*)tabModel {
  [tabModel addObserver:self];
  DCHECK(!_allWebStateObservationForwarder);
  // Observe all webStates of this tabModel, so that URLs are saved in cases
  // of crashing.
  _allWebStateObservationForwarder =
      std::make_unique<AllWebStateObservationForwarder>(
          tabModel.webStateList, _webStateObserver.get());
}

- (void)stopObservingTabModel:(TabModel*)tabModel {
  _allWebStateObservationForwarder.reset(nullptr);
  [tabModel removeObserver:self];
}

- (void)tabModel:(TabModel*)model
    didRemoveTab:(Tab*)tab
         atIndex:(NSUInteger)index {
  [self removeTabId:TabIdTabHelper::FromWebState(tab.webState)->tab_id()];
}

- (void)tabModel:(TabModel*)model
    didReplaceTab:(Tab*)oldTab
          withTab:(Tab*)newTab
          atIndex:(NSUInteger)index {
  [self removeTabId:TabIdTabHelper::FromWebState(oldTab.webState)->tab_id()];
}

- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)modelIndex {
  web::NavigationItem* pendingItem =
      newTab.webState->GetNavigationManager()->GetPendingItem();
  const GURL& URL = pendingItem ? pendingItem->GetURL()
                                : newTab.webState->GetLastCommittedURL();
  [self recordURL:base::SysUTF8ToNSString(URL.spec())
         forTabId:TabIdTabHelper::FromWebState(newTab.webState)->tab_id()
          pending:pendingItem ? YES : NO];
}

#pragma mark - CRWWebStateObserver protocol

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  NSString* urlString = base::SysUTF8ToNSString(navigation->GetUrl().spec());
  if (!urlString.length || webState->GetBrowserState()->IsOffTheRecord())
    return;
  [self recordURL:urlString
         forTabId:TabIdTabHelper::FromWebState(webState)->tab_id()
          pending:YES];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  NSString* urlString =
      base::SysUTF8ToNSString(webState->GetLastCommittedURL().spec());
  if (!urlString.length || webState->GetBrowserState()->IsOffTheRecord())
    return;
  [self recordURL:urlString
         forTabId:TabIdTabHelper::FromWebState(webState)->tab_id()
          pending:NO];
}

// Empty method left in place in case jailbreakers are swizzling this.
- (void)detectJailbrokenDevice {
  // This method has been intentionally left blank.
}

@end

@implementation CrashReporterTabStateObserver

+ (CrashReporterTabStateObserver*)uniqueInstance {
  static CrashReporterTabStateObserver* instance =
      [[CrashReporterTabStateObserver alloc] init];
  return instance;
}

- (id)init {
  if ((self = [super init])) {
    tabCurrentStateByTabId_ = [[NSMutableDictionary alloc] init];
    // Register for url changed notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(closingDocument:)
               name:kTabClosingCurrentDocumentNotificationForCrashReporting
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(showingExportableDocument:)
               name:kTabIsShowingExportableNotificationForCrashReporting
             object:nil];
  }
  return self;
}

- (void)closingDocument:(NSNotification*)notification {
  Tab* tab = notification.object;
  NSString* tabID = nil;
  if (tab.webState)
    tabID = TabIdTabHelper::FromWebState(tab.webState)->tab_id();
  [self closingDocumentInTab:tabID];
}

- (void)closingDocumentInTab:(NSString*)tabId {
  NSString* mime = (NSString*)[self getTabInfo:@"mime" forTab:tabId];
  if ([mime isEqualToString:@"application/pdf"])
    breakpad_helper::SetCurrentTabIsPDF(false);
  [self removeTabInfo:@"mime" forTab:tabId];
}

- (void)setTabInfo:(NSString*)key
         withValue:(NSString*)value
            forTab:(NSString*)tabId {
  NSMutableDictionary* tabCurrentState =
      [tabCurrentStateByTabId_ objectForKey:tabId];
  if (tabCurrentState == nil) {
    NSMutableDictionary* currentStateOfNewTab =
        [[NSMutableDictionary alloc] init];
    [tabCurrentStateByTabId_ setObject:currentStateOfNewTab forKey:tabId];
    tabCurrentState = [tabCurrentStateByTabId_ objectForKey:tabId];
  }
  [tabCurrentState setObject:value forKey:key];
}

- (id)getTabInfo:(NSString*)key forTab:(NSString*)tabId {
  NSMutableDictionary* tabValues = [tabCurrentStateByTabId_ objectForKey:tabId];
  return [tabValues objectForKey:key];
}

- (void)removeTabInfo:(NSString*)key forTab:(NSString*)tabId {
  [[tabCurrentStateByTabId_ objectForKey:tabId] removeObjectForKey:key];
}

- (void)showingExportableDocument:(NSNotification*)notification {
  Tab* tab = notification.object;
  NSString* tabID = nil;
  if (tab.webState)
    tabID = TabIdTabHelper::FromWebState(tab.webState)->tab_id();
  NSString* oldMime = (NSString*)[self getTabInfo:@"mime" forTab:tabID];
  if ([oldMime isEqualToString:@"application/pdf"])
    return;

  std::string mime = [tab webState]->GetContentsMimeType();
  NSString* nsMime = base::SysUTF8ToNSString(mime);
  [self setTabInfo:@"mime" withValue:nsMime forTab:tabID];
  breakpad_helper::SetCurrentTabIsPDF(true);
}

- (void)removeTabId:(NSString*)tabId {
  [self closingDocumentInTab:tabId];
  [tabCurrentStateByTabId_ removeObjectForKey:tabId];
}

- (void)tabModel:(TabModel*)model
    didRemoveTab:(Tab*)tab
         atIndex:(NSUInteger)index {
  [self removeTabId:TabIdTabHelper::FromWebState(tab.webState)->tab_id()];
}

- (void)tabModel:(TabModel*)model
    didReplaceTab:(Tab*)oldTab
          withTab:(Tab*)newTab
          atIndex:(NSUInteger)index {
  [self removeTabId:TabIdTabHelper::FromWebState(oldTab.webState)->tab_id()];
}

@end

namespace breakpad {

void MonitorURLsForWebState(web::WebState* web_state) {
  [[CrashReporterURLObserver uniqueInstance] observeWebState:web_state];
}

void StopMonitoringURLsForWebState(web::WebState* web_state) {
  [[CrashReporterURLObserver uniqueInstance] stopObservingWebState:web_state];
}

void MonitorURLsForTabModel(TabModel* tab_model) {
  DCHECK(!tab_model.isOffTheRecord);
  [[CrashReporterURLObserver uniqueInstance] observeTabModel:tab_model];
}

void StopMonitoringURLsForTabModel(TabModel* tab_model) {
  [[CrashReporterURLObserver uniqueInstance] stopObservingTabModel:tab_model];
}

void MonitorTabStateForTabModel(TabModel* tab_model) {
  [tab_model addObserver:[CrashReporterTabStateObserver uniqueInstance]];
}

void StopMonitoringTabStateForTabModel(TabModel* tab_model) {
  [tab_model removeObserver:[CrashReporterTabStateObserver uniqueInstance]];
}

void ClearStateForTabModel(TabModel* tab_model) {
  CrashReporterURLObserver* observer =
      [CrashReporterURLObserver uniqueInstance];

  WebStateList* web_state_list = tab_model.webStateList;
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    [observer removeTabId:TabIdTabHelper::FromWebState(web_state)->tab_id()];
  }
}

}  // namespace breakpad

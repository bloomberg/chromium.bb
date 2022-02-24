// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_mac.h"

#import <Foundation/Foundation.h>

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

@interface CRUUpdateClientOnDemandImpl () {
  base::scoped_nsobject<NSXPCConnection> _xpcConnection;
}
@end

namespace {

NSString* GetAppIdForUpdaterAsNSString() {
  return base::SysUTF8ToNSString(base::mac::BaseBundleID());
}

}  // namespace

@implementation CRUUpdateClientOnDemandImpl

- (instancetype)init {
  return [self initWithScope:ShouldUseSystemLevelUpdater()
                                 ? updater::UpdaterScope::kSystem
                                 : updater::UpdaterScope::kUser];
}

- (instancetype)initWithScope:(updater::UpdaterScope)scope {
  // If the system-level updater exists, and the browser is registered to the
  // system-level updater, then connect using NSXPCConnectionPrivileged.
  NSXPCConnectionOptions options = 0;
  if (scope == updater::UpdaterScope::kSystem) {
    options = NSXPCConnectionPrivileged;
  }
  return [self initWithConnectionOptions:options withScope:scope];
}

- (instancetype)initWithConnectionOptions:(NSXPCConnectionOptions)options
                                withScope:(updater::UpdaterScope)scope {
  if (self = [super init]) {
    _xpcConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:updater::GetUpdateServiceMachName(scope)
                        options:options]);

    _xpcConnection.get().remoteObjectInterface =
        updater::GetXPCUpdateServicingInterface();

    _xpcConnection.get().interruptionHandler = ^{
      LOG(WARNING)
          << "CRUUpdateClientOnDemandImpl: XPC connection interrupted.";
    };

    _xpcConnection.get().invalidationHandler = ^{
      LOG(WARNING)
          << "CRUUpdateClientOnDemandImpl: XPC connection invalidated.";
    };

    [_xpcConnection resume];
  }

  return self;
}

- (void)getVersionWithReply:
    (void (^_Nonnull)(NSString* _Nullable version))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC Connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(nil);
  };

  [[_xpcConnection remoteObjectProxyWithErrorHandler:errorHandler]
      getVersionWithReply:reply];
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                          brandPath:(NSString* _Nullable)brandPath
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC Connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(
        static_cast<int>(updater::UpdateService::Result::kIPCConnectionFailed));
  };

  [[_xpcConnection remoteObjectProxyWithErrorHandler:errorHandler]
      registerForUpdatesWithAppId:appId
                        brandCode:brandCode
                        brandPath:brandPath
                              tag:tag
                          version:version
             existenceCheckerPath:existenceCheckerPath
                            reply:reply];
}

// Checks for updates and returns the result in the reply block.
- (void)checkForUpdatesWithUpdateState:
            (CRUUpdateStateObserver* _Nonnull)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  // The method stub needs to be implemented as it is part of the
  // CRUUpdateChecking protocol. However, this method does not need to be
  // implemented because checkForUpdatesWithAppId:priority:updateState:reply:
  // gives us all the functionality we need for on-demand updates.
  NOTIMPLEMENTED();
}

// Checks for update of a given app, with specified priority. Sends repeated
// updates of progress and returns the result in the reply block.
- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
        policySameVersionUpdate:
            (CRUPolicySameVersionUpdateWrapper* _Nonnull)policySameVersionUpdate
                    updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                          reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC Connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(
        static_cast<int>(updater::UpdateService::Result::kIPCConnectionFailed));
  };

  [[_xpcConnection remoteObjectProxyWithErrorHandler:errorHandler]
      checkForUpdateWithAppID:appID
                     priority:priority
      policySameVersionUpdate:policySameVersionUpdate
                  updateState:updateState
                        reply:reply];
}

// Runs periodic updater tasks like checking for uninstalls and background
// update checks.
- (void)runPeriodicTasksWithReply:(void (^_Nullable)(void))reply {
  // This method does not need to be implemented in the RPC on-demand update
  // call from the browser to the updater.
  NOTIMPLEMENTED();
}

// Gets states of all registered apps.
- (void)getAppStatesWithReply:
    (void (^_Nonnull)(CRUAppStatesWrapper* _Nullable apps))reply {
  NOTIMPLEMENTED();
}

@end

BrowserUpdaterClientMac::BrowserUpdaterClientMac()
    : BrowserUpdaterClientMac(
          base::scoped_nsobject<CRUUpdateClientOnDemandImpl>(
              [[CRUUpdateClientOnDemandImpl alloc] init])) {}

BrowserUpdaterClientMac::BrowserUpdaterClientMac(
    base::scoped_nsobject<CRUUpdateClientOnDemandImpl> client)
    : client_(client) {}

BrowserUpdaterClientMac::~BrowserUpdaterClientMac() = default;

void BrowserUpdaterClientMac::GetUpdaterVersion(
    base::OnceCallback<void(const std::string&)> callback) {
  __block base::OnceCallback<void(const std::string&)> block_callback =
      std::move(callback);

  auto reply = ^(NSString* version) {
    std::string result = base::SysNSStringToUTF8(version);
    task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(std::move(block_callback), result));
  };

  [client_ getVersionWithReply:reply];
}

void BrowserUpdaterClientMac::ResetConnection(updater::UpdaterScope scope) {
  client_.reset([[CRUUpdateClientOnDemandImpl alloc] initWithScope:scope]);
}

void BrowserUpdaterClientMac::BeginRegister(
    const std::string& brand_code,
    const std::string& tag,
    const std::string& version,
    updater::UpdateService::Callback callback) {
  __block updater::UpdateService::Callback block_callback = std::move(callback);

  auto reply = ^(int error) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(block_callback),
                       static_cast<updater::UpdateService::Result>(error)));
  };

  [client_ registerForUpdatesWithAppId:GetAppIdForUpdaterAsNSString()
                             brandCode:base::SysUTF8ToNSString(brand_code)
                             brandPath:@""
                                   tag:base::SysUTF8ToNSString(tag)
                               version:base::SysUTF8ToNSString(version)
                  existenceCheckerPath:base::mac::FilePathToNSString(
                                           base::mac::OuterBundlePath())
                                 reply:reply];
}

void BrowserUpdaterClientMac::BeginUpdateCheck(
    updater::UpdateService::StateChangeCallback state_update,
    updater::UpdateService::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  __block updater::UpdateService::Callback block_callback = std::move(callback);

  auto reply = ^(int error) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(block_callback),
                       static_cast<updater::UpdateService::Result>(error)));
  };

  base::scoped_nsobject<CRUPriorityWrapper> priority_wrapper(
      [[CRUPriorityWrapper alloc]
          initWithPriority:updater::UpdateService::Priority::kForeground]);
  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> state_observer(
      [[CRUUpdateStateObserver alloc] initWithRepeatingCallback:state_update
                                                 callbackRunner:task_runner()]);
  base::scoped_nsobject<CRUPolicySameVersionUpdateWrapper>
      policySameVersionUpdateWrapper([[CRUPolicySameVersionUpdateWrapper alloc]
          initWithPolicySameVersionUpdate:
              updater::UpdateService::PolicySameVersionUpdate::kNotAllowed]);
  [client_ checkForUpdateWithAppID:GetAppIdForUpdaterAsNSString()
                          priority:priority_wrapper.get()
           policySameVersionUpdate:policySameVersionUpdateWrapper.get()
                       updateState:state_observer.get()
                             reply:reply];
}

// static
scoped_refptr<BrowserUpdaterClient> BrowserUpdaterClient::Create() {
  return base::MakeRefCounted<BrowserUpdaterClientMac>();
}

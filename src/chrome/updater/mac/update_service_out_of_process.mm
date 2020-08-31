// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/update_service_out_of_process.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "chrome/updater/mac/setup/info_plist.h"
#import "chrome/updater/mac/xpc_service_names.h"
#import "chrome/updater/server/mac/service_protocol.h"
#import "chrome/updater/server/mac/update_service_wrappers.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_version.h"
#include "components/update_client/update_client_errors.h"

using base::SysUTF8ToNSString;

// Interface to communicate with the XPC Updater Service.
@interface CRUUpdateServiceOutOfProcessImpl : NSObject <CRUUpdateChecking>

- (instancetype)initPrivileged;

@end

@implementation CRUUpdateServiceOutOfProcessImpl {
  base::scoped_nsobject<NSXPCConnection> _updateCheckXPCConnection;
}

- (instancetype)init {
  return [self initWithConnectionOptions:0];
}

- (instancetype)initPrivileged {
  return [self initWithConnectionOptions:NSXPCConnectionPrivileged];
}

- (instancetype)initWithConnectionOptions:(NSXPCConnectionOptions)options {
  if ((self = [super init])) {
    _updateCheckXPCConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:updater::GetGoogleUpdateServiceMachName().get()
                        options:options]);

    _updateCheckXPCConnection.get().remoteObjectInterface =
        updater::GetXPCUpdateCheckingInterface();

    _updateCheckXPCConnection.get().interruptionHandler = ^{
      LOG(WARNING) << "CRUUpdateCheckingService: XPC connection interrupted.";
    };

    _updateCheckXPCConnection.get().invalidationHandler = ^{
      LOG(WARNING) << "CRUUpdateCheckingService: XPC connection invalidated.";
    };

    [_updateCheckXPCConnection resume];
  }

  return self;
}

- (void)dealloc {
  [_updateCheckXPCConnection invalidate];
  [super dealloc];
}

- (void)getUpdaterVersionWithReply:
    (void (^_Nonnull)(NSString* _Nullable version))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(nil);
  };

  [[_updateCheckXPCConnection.get()
      remoteObjectProxyWithErrorHandler:errorHandler]
      getUpdaterVersionWithReply:reply];
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_updateCheckXPCConnection.get()
      remoteObjectProxyWithErrorHandler:errorHandler]
      registerForUpdatesWithAppId:appId
                        brandCode:brandCode
                              tag:tag
                          version:version
             existenceCheckerPath:existenceCheckerPath
                            reply:reply];
}

- (void)checkForUpdatesWithUpdateState:
            (id<CRUUpdateStateObserving> _Nonnull)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      checkForUpdatesWithUpdateState:updateState
                               reply:reply];
}

- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:
                        (id<CRUUpdateStateObserving> _Nonnull)updateState
                          reply:(void (^_Nonnull)(int rc))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(-1);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      checkForUpdateWithAppID:appID
                     priority:priority
                  updateState:updateState
                        reply:reply];
}

- (void)haltForUpdateToVersion:(NSString* _Nonnull)version
                         reply:(void (^_Nonnull)(bool shouldUpdate))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(NO);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      haltForUpdateToVersion:version
                       reply:reply];
}

@end

// Interface to communicate with the XPC Updater Service.
@interface CRUAdministrationServiceOutOfProcessImpl
    : NSObject <CRUAdministering>

- (instancetype)initPrivileged;

@end

@implementation CRUAdministrationServiceOutOfProcessImpl {
  base::scoped_nsobject<NSXPCConnection> _administrationXPCConnection;
}

- (instancetype)init {
  return [self initWithConnectionOptions:0];
}

- (instancetype)initPrivileged {
  return [self initWithConnectionOptions:NSXPCConnectionPrivileged];
}

- (instancetype)initWithConnectionOptions:(NSXPCConnectionOptions)options {
  if ((self = [super init])) {
    const std::unique_ptr<updater::InfoPlist> infoPlist =
        updater::InfoPlist::Create(updater::InfoPlistPath());
    CHECK(infoPlist);

    _administrationXPCConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:
            base::mac::CFToNSCast(
                infoPlist->GoogleUpdateServiceLaunchdNameVersioned().get())
                        options:options]);

    _administrationXPCConnection.get().remoteObjectInterface =
        updater::GetXPCAdministeringInterface();

    _administrationXPCConnection.get().interruptionHandler = ^{
      LOG(WARNING) << "CRUAdministering: XPC connection interrupted.";
    };

    _administrationXPCConnection.get().invalidationHandler = ^{
      LOG(WARNING) << "CRUAdministering: XPC connection invalidated.";
    };

    [_administrationXPCConnection resume];
  }

  return self;
}

- (void)dealloc {
  [_administrationXPCConnection invalidate];
  [super dealloc];
}

- (void)performAdminTasks {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
  };

  [[_administrationXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      performAdminTasks];
}

@end

namespace updater {

UpdateServiceOutOfProcess::UpdateServiceOutOfProcess(
    UpdateService::Scope scope) {
  switch (scope) {
    case UpdateService::Scope::kSystem:
      client_.reset([[CRUUpdateServiceOutOfProcessImpl alloc] initPrivileged]);
      break;
    case UpdateService::Scope::kUser:
      client_.reset([[CRUUpdateServiceOutOfProcessImpl alloc] init]);
      break;
    default:
      CHECK(false) << "Unexpected value for UpdateService::Scope";
  }
  callback_runner_ = base::SequencedTaskRunnerHandle::Get();
}

void UpdateServiceOutOfProcess::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  __block base::OnceCallback<void(const RegistrationResponse&)> block_callback =
      std::move(callback);

  auto reply = ^(int error) {
    RegistrationResponse response;
    response.status_code = error;
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback), response));
  };

  [client_
      registerForUpdatesWithAppId:SysUTF8ToNSString(request.app_id)
                        brandCode:SysUTF8ToNSString(request.brand_code)
                              tag:SysUTF8ToNSString(request.tag)
                          version:SysUTF8ToNSString(request.version.GetString())
             existenceCheckerPath:SysUTF8ToNSString(
                                      request.existence_checker_path
                                          .AsUTF8Unsafe())
                            reply:reply];
}

void UpdateServiceOutOfProcess::UpdateAll(StateChangeCallback state_update,
                                          Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceCallback<void(UpdateService::Result)> block_callback =
      std::move(callback);
  auto reply = ^(int error) {
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<UpdateService::Result>(error)));
  };

  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
      [[CRUUpdateStateObserver alloc]
          initWithRepeatingCallback:state_update
                     callbackRunner:callback_runner_]);
  [client_ checkForUpdatesWithUpdateState:stateObserver.get() reply:reply];
}

void UpdateServiceOutOfProcess::Update(const std::string& app_id,
                                       UpdateService::Priority priority,
                                       StateChangeCallback state_update,
                                       Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceCallback<void(UpdateService::Result)> block_callback =
      std::move(callback);
  auto reply = ^(int error) {
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  static_cast<UpdateService::Result>(error)));
  };

  base::scoped_nsobject<CRUPriorityWrapper> priorityWrapper(
      [[CRUPriorityWrapper alloc] initWithPriority:priority]);
  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
      [[CRUUpdateStateObserver alloc]
          initWithRepeatingCallback:state_update
                     callbackRunner:callback_runner_]);

  [client_ checkForUpdateWithAppID:SysUTF8ToNSString(app_id)
                          priority:priorityWrapper.get()
                       updateState:stateObserver.get()
                             reply:reply];
}

void UpdateServiceOutOfProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

UpdateServiceOutOfProcess::~UpdateServiceOutOfProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater

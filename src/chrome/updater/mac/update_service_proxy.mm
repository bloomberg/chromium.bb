// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/update_service_proxy.h"

#import <Foundation/Foundation.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "components/update_client/update_client_errors.h"

using base::SysUTF8ToNSString;

// Interface to communicate with the XPC Updater Service.
@interface CRUUpdateServiceProxyImpl : NSObject <CRUUpdateServicing>

- (instancetype)initWithScope:(updater::UpdaterScope)scope;

@end

@implementation CRUUpdateServiceProxyImpl {
  base::scoped_nsobject<NSXPCConnection> _updateCheckXPCConnection;
}

- (instancetype)initWithScope:(updater::UpdaterScope)scope {
  switch (scope) {
    case updater::UpdaterScope::kUser:
      return [self initWithConnectionOptions:0 withScope:scope];
    case updater::UpdaterScope::kSystem:
      return [self initWithConnectionOptions:NSXPCConnectionPrivileged
                                   withScope:scope];
  }
  return nil;
}

- (instancetype)initWithConnectionOptions:(NSXPCConnectionOptions)options
                                withScope:(updater::UpdaterScope)scope {
  if ((self = [super init])) {
    _updateCheckXPCConnection.reset([[NSXPCConnection alloc]
        initWithMachServiceName:updater::GetUpdateServiceMachName(scope).get()
                        options:options]);

    _updateCheckXPCConnection.get().remoteObjectInterface =
        updater::GetXPCUpdateServicingInterface();

    _updateCheckXPCConnection.get().interruptionHandler = ^{
      LOG(WARNING) << "CRUUpdateServicingService: XPC connection interrupted.";
    };

    _updateCheckXPCConnection.get().invalidationHandler = ^{
      LOG(WARNING) << "CRUUpdateServicingService: XPC connection invalidated.";
    };

    [_updateCheckXPCConnection resume];
  }

  return self;
}

- (void)dealloc {
  [_updateCheckXPCConnection invalidate];
  [super dealloc];
}

- (void)getVersionWithReply:(void (^_Nonnull)(NSString* version))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(nil);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      getVersionWithReply:reply];
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                          brandPath:(NSString* _Nullable)brandPath
                                tag:(NSString* _Nullable)ap
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
                        brandPath:brandPath
                              tag:ap
                          version:version
             existenceCheckerPath:existenceCheckerPath
                            reply:reply];
}

- (void)getAppStatesWithReply:(void (^_Nonnull)(CRUAppStatesWrapper*))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply(nil);
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      getAppStatesWithReply:reply];
}

- (void)runPeriodicTasksWithReply:(void (^)(void))reply {
  auto errorHandler = ^(NSError* xpcError) {
    LOG(ERROR) << "XPC connection failed: "
               << base::SysNSStringToUTF8([xpcError description]);
    reply();
  };

  [[_updateCheckXPCConnection remoteObjectProxyWithErrorHandler:errorHandler]
      runPeriodicTasksWithReply:reply];
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
        policySameVersionUpdate:
            (CRUPolicySameVersionUpdateWrapper* _Nonnull)policySameVersionUpdate
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
      policySameVersionUpdate:policySameVersionUpdate
                  updateState:updateState
                        reply:reply];
}

@end

namespace updater {

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope updater_scope) {
  return base::MakeRefCounted<UpdateServiceProxy>(updater_scope);
}

UpdateServiceProxy::UpdateServiceProxy(UpdaterScope scope) {
  client_.reset([[CRUUpdateServiceProxyImpl alloc] initWithScope:scope]);
  callback_runner_ = base::SequencedTaskRunnerHandle::Get();
}

void UpdateServiceProxy::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block base::OnceCallback<void(const base::Version&)> block_callback =
      std::move(callback);
  auto reply = ^(NSString* version) {
    callback_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(block_callback),
                       base::Version(base::SysNSStringToUTF8(version))));
  };
  [client_ getVersionWithReply:reply];
}

void UpdateServiceProxy::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  __block base::OnceCallback<void(const RegistrationResponse&)> block_callback =
      std::move(callback);

  auto reply = ^(int error) {
    RegistrationResponse response(error);
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback), response));
  };

  [client_
      registerForUpdatesWithAppId:SysUTF8ToNSString(request.app_id)
                        brandCode:SysUTF8ToNSString(request.brand_code)
                        brandPath:base::mac::FilePathToNSString(
                                      request.brand_path)
                              tag:SysUTF8ToNSString(request.ap)
                          version:SysUTF8ToNSString(request.version.GetString())
             existenceCheckerPath:base::mac::FilePathToNSString(
                                      request.existence_checker_path)
                            reply:reply];
}

void UpdateServiceProxy::GetAppStates(
    base::OnceCallback<void(
        const std::vector<updater::UpdateService::AppState>&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  __block base::OnceCallback<void(
      const std::vector<updater::UpdateService::AppState>&)>
      block_callback = std::move(callback);

  auto reply = ^(CRUAppStatesWrapper* wrapper) {
    callback_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback), wrapper.states));
  };
  [client_ getAppStatesWithReply:reply];
}

void UpdateServiceProxy::RunPeriodicTasks(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  __block base::OnceClosure block_callback = std::move(callback);
  auto reply = ^() {
    callback_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(block_callback)));
  };
  [client_ runPeriodicTasksWithReply:reply];
}

void UpdateServiceProxy::UpdateAll(StateChangeCallback state_update,
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

void UpdateServiceProxy::Update(
    const std::string& app_id,
    UpdateService::Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
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
  base::scoped_nsobject<CRUPolicySameVersionUpdateWrapper>
      policySameVersionUpdateWrapper([[CRUPolicySameVersionUpdateWrapper alloc]
          initWithPolicySameVersionUpdate:policy_same_version_update]);
  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> stateObserver(
      [[CRUUpdateStateObserver alloc]
          initWithRepeatingCallback:state_update
                     callbackRunner:callback_runner_]);

  [client_ checkForUpdateWithAppID:SysUTF8ToNSString(app_id)
                          priority:priorityWrapper.get()
           policySameVersionUpdate:policySameVersionUpdateWrapper.get()
                       updateState:stateObserver.get()
                             reply:reply];
}

void UpdateServiceProxy::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

UpdateServiceProxy::~UpdateServiceProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace updater

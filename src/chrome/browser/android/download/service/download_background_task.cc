// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "chrome/browser/android/download/download_manager_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/common/auto_resumption_handler.h"
#include "content/public/browser/browser_context.h"
#include "jni/DownloadBackgroundTask_jni.h"

using base::android::JavaParamRef;

namespace download {
namespace android {

DownloadService* GetDownloadService(
    const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  return DownloadServiceFactory::GetForBrowserContext(profile);
}

AutoResumptionHandler* GetAutoResumptionHandler() {
  if (!AutoResumptionHandler::Get())
    DownloadManagerService::CreateAutoResumptionHandler();
  return AutoResumptionHandler::Get();
}

// static
void JNI_DownloadBackgroundTask_StartBackgroundTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jprofile,
    jint task_type,
    const base::android::JavaParamRef<jobject>& jcallback) {
  TaskFinishedCallback finish_callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));

  switch (static_cast<DownloadTaskType>(task_type)) {
    case download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK: {
      GetAutoResumptionHandler()->OnStartScheduledTask(
          std::move(finish_callback));
      break;
    }
    case download::DownloadTaskType::DOWNLOAD_TASK:
      FALLTHROUGH;
    case download::DownloadTaskType::CLEANUP_TASK:
      GetDownloadService(jprofile)->OnStartScheduledTask(
          static_cast<DownloadTaskType>(task_type), std::move(finish_callback));
      break;
  }
}

// static
jboolean JNI_DownloadBackgroundTask_StopBackgroundTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jprofile,
    jint task_type) {
  switch (static_cast<DownloadTaskType>(task_type)) {
    case download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK: {
      GetAutoResumptionHandler()->OnStopScheduledTask();
      break;
    }
    case download::DownloadTaskType::DOWNLOAD_TASK:
      FALLTHROUGH;
    case download::DownloadTaskType::CLEANUP_TASK:
      return GetDownloadService(jprofile)->OnStopScheduledTask(
          static_cast<DownloadTaskType>(task_type));
  }
  return false;
}

}  // namespace android
}  // namespace download

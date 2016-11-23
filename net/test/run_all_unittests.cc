// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/build_time.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/test/net_test_suite.h"
#include "url/url_features.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "net/android/dummy_spnego_authenticator.h"
#include "net/android/net_jni_registrar.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "mojo/edk/embedder/embedder.h"  // nogncheck
#endif

using net::internal::ClientSocketPoolBaseHelper;

namespace {

bool VerifyBuildIsTimely() {
  // This lines up with various //net security features, like Certificate
  // Transparency or HPKP, in that they require the build time be less than 70
  // days old. Moreover, operating on the assumption that tests are run against
  // recently compiled builds, this also serves as a sanity check for the
  // system clock, which should be close to the build date.
  base::TimeDelta kMaxAge = base::TimeDelta::FromDays(70);

  base::Time build_time = base::GetBuildTime();
  base::Time now = base::Time::Now();

  if ((now - build_time).magnitude() <= kMaxAge)
    return true;

  std::cerr
      << "ERROR: This build is more than " << kMaxAge.InDays()
      << " days out of date.\n"
         "This could indicate a problem with the device's clock, or the build "
         "is simply too old.\n"
         "See crbug.com/666821 for why this is a problem\n"
      << "    base::Time::Now() --> " << now << " (" << now.ToInternalValue()
      << ")\n"
      << "    base::GetBuildTime() --> " << build_time << " ("
      << build_time.ToInternalValue() << ")\n";

  return false;
}

}  // namespace

int main(int argc, char** argv) {
  // Record histograms, so we can get histograms data in tests.
  base::StatisticsRecorder::Initialize();

#if defined(OS_ANDROID)
  const base::android::RegistrationMethod kNetTestRegisteredMethods[] = {
    {"DummySpnegoAuthenticator",
     net::android::DummySpnegoAuthenticator::RegisterJni},
    {"NetAndroid", net::android::RegisterJni},
  };

  // Register JNI bindings for android. Doing it early as the test suite setup
  // may initiate a call to Java.
  base::android::RegisterNativeMethods(
      base::android::AttachCurrentThread(),
      kNetTestRegisteredMethods,
      arraysize(kNetTestRegisteredMethods));
#endif

  if (!VerifyBuildIsTimely())
    return 1;

  NetTestSuite test_suite(argc, argv);
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  mojo::edk::Init();
#endif

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&NetTestSuite::Run,
                             base::Unretained(&test_suite)));
}

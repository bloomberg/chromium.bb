// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/constants_mac.h"

namespace remoting {

#define SERVICE_NAME "org.chromium.chromoting"

#define APPLICATIONS_DIR "/Applications/"
#define HELPER_TOOLS_DIR "/Library/PrivilegedHelperTools/"
#define LAUNCH_AGENTS_DIR "/Library/LaunchAgents/"
#define PREFERENCE_PANES_DIR "/Library/PreferencePanes/"
#define LOG_DIR "/var/log/"
#define LOG_CONFIG_DIR "/etc/newsyslog.d/"

const char kServiceName[] = SERVICE_NAME;

const char kUpdateSucceededNotificationName[] =
    SERVICE_NAME ".update_succeeded";
const char kUpdateFailedNotificationName[] = SERVICE_NAME ".update_failed";

const char kPrefPaneFileName[] = SERVICE_NAME ".prefPane";
const char kPrefPaneFilePath[] = PREFERENCE_PANES_DIR SERVICE_NAME ".prefPane";

const char kHostConfigFileName[] = SERVICE_NAME ".json";
const char kHostConfigFilePath[] = HELPER_TOOLS_DIR SERVICE_NAME ".json";

const char kHostHelperScriptPath[] = HELPER_TOOLS_DIR SERVICE_NAME ".me2me.sh";
const char kHostBinaryPath[] = HELPER_TOOLS_DIR SERVICE_NAME ".me2me_host.app";
const char kHostEnabledPath[] = HELPER_TOOLS_DIR SERVICE_NAME ".me2me_enabled";

const char kServicePlistPath[] = LAUNCH_AGENTS_DIR SERVICE_NAME ".plist";

const char kLogFilePath[] = LOG_DIR SERVICE_NAME ".log";
const char kLogFileConfigPath[] = LOG_CONFIG_DIR SERVICE_NAME ".conf";

const char kBrandedUninstallerPath[] = APPLICATIONS_DIR
    "Chrome Remote Desktop Host Uninstaller.app";
const char kUnbrandedUninstallerPath[] = APPLICATIONS_DIR
    "Chromoting Host Uninstaller.app";

}  // namespace remoting

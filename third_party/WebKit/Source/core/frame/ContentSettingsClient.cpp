// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/ContentSettingsClient.h"

#include "platform/ContentSettingCallbacks.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebContentSettingCallbacks.h"
#include "public/platform/WebContentSettingsClient.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebURL.h"

namespace blink {

ContentSettingsClient::ContentSettingsClient() = default;

bool ContentSettingsClient::AllowDatabase(const String& name,
                                          const String& display_name,
                                          unsigned estimated_size) {
  if (client_)
    return client_->AllowDatabase(name, display_name, estimated_size);
  return true;
}

bool ContentSettingsClient::AllowIndexedDB(const String& name,
                                           SecurityOrigin* security_origin) {
  if (client_)
    return client_->AllowIndexedDB(name, WebSecurityOrigin(security_origin));
  return true;
}

bool ContentSettingsClient::RequestFileSystemAccessSync() {
  if (client_)
    return client_->RequestFileSystemAccessSync();
  return true;
}

void ContentSettingsClient::RequestFileSystemAccessAsync(
    std::unique_ptr<ContentSettingCallbacks> callbacks) {
  if (client_)
    client_->RequestFileSystemAccessAsync(std::move(callbacks));
  else
    callbacks->OnAllowed();
}

bool ContentSettingsClient::AllowScript(bool enabled_per_settings) {
  if (client_)
    return client_->AllowScript(enabled_per_settings);
  return enabled_per_settings;
}

bool ContentSettingsClient::AllowScriptFromSource(bool enabled_per_settings,
                                                  const KURL& script_url) {
  if (client_)
    return client_->AllowScriptFromSource(enabled_per_settings, script_url);
  return enabled_per_settings;
}

bool ContentSettingsClient::AllowImage(bool enabled_per_settings,
                                       const KURL& image_url) {
  if (client_)
    return client_->AllowImage(enabled_per_settings, image_url);
  return enabled_per_settings;
}

bool ContentSettingsClient::AllowReadFromClipboard(bool default_value) {
  if (client_)
    return client_->AllowReadFromClipboard(default_value);
  return default_value;
}

bool ContentSettingsClient::AllowWriteToClipboard(bool default_value) {
  if (client_)
    return client_->AllowWriteToClipboard(default_value);
  return default_value;
}

bool ContentSettingsClient::AllowStorage(StorageType type) {
  if (client_)
    return client_->AllowStorage(type == StorageType::kLocal);
  return true;
}

bool ContentSettingsClient::AllowRunningInsecureContent(
    bool enabled_per_settings,
    SecurityOrigin* origin,
    const KURL& url) {
  if (client_) {
    return client_->AllowRunningInsecureContent(enabled_per_settings,
                                                WebSecurityOrigin(origin), url);
  }
  return enabled_per_settings;
}

bool ContentSettingsClient::AllowMutationEvents(bool default_value) {
  if (client_)
    return client_->AllowMutationEvents(default_value);
  return default_value;
}

bool ContentSettingsClient::AllowAutoplay(bool default_value) {
  if (client_)
    return client_->AllowAutoplay(default_value);
  return default_value;
}

void ContentSettingsClient::PassiveInsecureContentFound(const KURL& url) {
  if (client_)
    return client_->PassiveInsecureContentFound(url);
}

void ContentSettingsClient::DidNotAllowScript() {
  if (client_)
    client_->DidNotAllowScript();
}

void ContentSettingsClient::DidNotAllowPlugins() {
  if (client_)
    client_->DidNotAllowPlugins();
}

}  // namespace blink

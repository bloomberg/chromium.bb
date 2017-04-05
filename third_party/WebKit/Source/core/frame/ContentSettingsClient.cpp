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

bool ContentSettingsClient::allowDatabase(const String& name,
                                          const String& displayName,
                                          unsigned estimatedSize) {
  if (m_client)
    return m_client->allowDatabase(name, displayName, estimatedSize);
  return true;
}

bool ContentSettingsClient::allowIndexedDB(const String& name,
                                           SecurityOrigin* securityOrigin) {
  if (m_client)
    return m_client->allowIndexedDB(name, WebSecurityOrigin(securityOrigin));
  return true;
}

bool ContentSettingsClient::requestFileSystemAccessSync() {
  if (m_client)
    return m_client->requestFileSystemAccessSync();
  return true;
}

void ContentSettingsClient::requestFileSystemAccessAsync(
    std::unique_ptr<ContentSettingCallbacks> callbacks) {
  if (m_client)
    m_client->requestFileSystemAccessAsync(std::move(callbacks));
  else
    callbacks->onAllowed();
}

bool ContentSettingsClient::allowScript(bool enabledPerSettings) {
  if (m_client)
    return m_client->allowScript(enabledPerSettings);
  return enabledPerSettings;
}

bool ContentSettingsClient::allowScriptFromSource(bool enabledPerSettings,
                                                  const KURL& scriptURL) {
  if (m_client)
    return m_client->allowScriptFromSource(enabledPerSettings, scriptURL);
  return enabledPerSettings;
}

bool ContentSettingsClient::allowPlugins(bool enabledPerSettings) {
  if (m_client)
    return m_client->allowPlugins(enabledPerSettings);
  return enabledPerSettings;
}

bool ContentSettingsClient::allowImage(bool enabledPerSettings,
                                       const KURL& imageURL) {
  if (m_client)
    return m_client->allowImage(enabledPerSettings, imageURL);
  return enabledPerSettings;
}

bool ContentSettingsClient::allowReadFromClipboard(bool defaultValue) {
  if (m_client)
    return m_client->allowReadFromClipboard(defaultValue);
  return defaultValue;
}

bool ContentSettingsClient::allowWriteToClipboard(bool defaultValue) {
  if (m_client)
    return m_client->allowWriteToClipboard(defaultValue);
  return defaultValue;
}

bool ContentSettingsClient::allowStorage(StorageType type) {
  if (m_client)
    return m_client->allowStorage(type == StorageType::kLocal);
  return true;
}

bool ContentSettingsClient::allowRunningInsecureContent(bool enabledPerSettings,
                                                        SecurityOrigin* origin,
                                                        const KURL& url) {
  if (m_client) {
    return m_client->allowRunningInsecureContent(
        enabledPerSettings, WebSecurityOrigin(origin), url);
  }
  return enabledPerSettings;
}

bool ContentSettingsClient::allowMutationEvents(bool defaultValue) {
  if (m_client)
    return m_client->allowMutationEvents(defaultValue);
  return defaultValue;
}

bool ContentSettingsClient::allowAutoplay(bool defaultValue) {
  if (m_client)
    return m_client->allowAutoplay(defaultValue);
  return defaultValue;
}

void ContentSettingsClient::passiveInsecureContentFound(const KURL& url) {
  if (m_client)
    return m_client->passiveInsecureContentFound(url);
}

void ContentSettingsClient::didNotAllowScript() {
  if (m_client)
    m_client->didNotAllowScript();
}

void ContentSettingsClient::didNotAllowPlugins() {
  if (m_client)
    m_client->didNotAllowPlugins();
}

}  // namespace blink

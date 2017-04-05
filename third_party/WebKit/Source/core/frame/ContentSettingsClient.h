// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentSettingsClient_h
#define ContentSettingsClient_h

#include <memory>

#include "core/CoreExport.h"
#include "platform/heap/Heap.h"
#include "wtf/Forward.h"

namespace blink {

class ContentSettingCallbacks;
class KURL;
class SecurityOrigin;
class WebContentSettingsClient;

// This class provides the content settings information which tells
// whether each feature is allowed. Most of the methods return the
// given default values if embedder doesn't provide their own
// content settings client implementation (via m_client).
class CORE_EXPORT ContentSettingsClient {
 public:
  ContentSettingsClient();

  void setClient(WebContentSettingsClient* client) { m_client = client; }

  // Controls whether access to Web Databases is allowed.
  bool allowDatabase(const String& name,
                     const String& displayName,
                     unsigned estimatedSize);

  // Controls whether access to File System is allowed for this frame.
  bool requestFileSystemAccessSync();

  // Controls whether access to File System is allowed for this frame.
  void requestFileSystemAccessAsync(std::unique_ptr<ContentSettingCallbacks>);

  // Controls whether access to File System is allowed.
  bool allowIndexedDB(const String& name, SecurityOrigin*);

  // Controls whether scripts are allowed to execute.
  bool allowScript(bool enabledPerSettings);

  // Controls whether scripts loaded from the given URL are allowed to execute.
  bool allowScriptFromSource(bool enabledPerSettings, const KURL&);

  // Controls whether plugins are allowed.
  bool allowPlugins(bool enabledPerSettings);

  // Controls whether images are allowed.
  bool allowImage(bool enabledPerSettings, const KURL&);

  // Controls whether insecure scripts are allowed to execute for this frame.
  bool allowRunningInsecureContent(bool enabledPerSettings,
                                   SecurityOrigin*,
                                   const KURL&);

  // Controls whether HTML5 Web Storage is allowed for this frame.
  enum class StorageType { kLocal, kSession };
  bool allowStorage(StorageType);

  // Controls whether access to read the clipboard is allowed for this frame.
  bool allowReadFromClipboard(bool defaultValue);

  // Controls whether access to write the clipboard is allowed for this frame.
  bool allowWriteToClipboard(bool defaultValue);

  // Controls whether to enable MutationEvents for this frame.
  // The common use case of this method is actually to selectively disable
  // MutationEvents, but it's been named for consistency with the rest of the
  // interface.
  bool allowMutationEvents(bool defaultValue);

  // Controls whether autoplay is allowed for this frame.
  bool allowAutoplay(bool defaultValue);

  // Reports that passive mixed content was found at the provided URL. It may or
  // may not be actually displayed later, what would be flagged by
  // didDisplayInsecureContent.
  void passiveInsecureContentFound(const KURL&);

  // This callback notifies the client that the frame was about to run
  // JavaScript but did not because allowScript returned false. We have a
  // separate callback here because there are a number of places that need to
  // know if JavaScript is enabled but are not necessarily preparing to execute
  // script.
  void didNotAllowScript();

  // This callback is similar, but for plugins.
  void didNotAllowPlugins();

 private:
  WebContentSettingsClient* m_client = nullptr;
};

}  // namespace blink

#endif  // ContentSettingsClient_h

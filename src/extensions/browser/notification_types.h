// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_
#define EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_

#include "content/public/browser/notification_types.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/411569.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

namespace extensions {

// Only notifications fired by the extensions module should be here. The
// extensions module should not listen to notifications fired by the
// embedder.
enum NotificationType {
  // WARNING: This need to match chrome/browser/chrome_notification_types.h.
  NOTIFICATION_EXTENSIONS_START = content::NOTIFICATION_CONTENT_END,

  // Sent when a CrxInstaller finishes. Source is the CrxInstaller that
  // finished. The details are the extension which was installed.
  // DEPRECATED: Use extensions::InstallObserver::OnFinishCrxInstall()
  // TODO(https://crbug.com/1174728): Remove.
  NOTIFICATION_CRX_INSTALLER_DONE = NOTIFICATION_EXTENSIONS_START,

  // Sent when attempting to load a new extension, but they are disabled. The
  // details are an Extension, and the source is a BrowserContext*.
  // TODO(https://crbug.com/1174732): Remove.
  NOTIFICATION_EXTENSION_UPDATE_DISABLED,

  // Sent when an extension's permissions change. The details are an
  // UpdatedExtensionPermissionsInfo, and the source is a BrowserContext*.
  // TODO(https://crbug.com/1174733): Remove.
  NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,

  // An error occurred during extension install. The details are a string with
  // details about why the install failed.
  // TODO(https://crbug.com/1174734): Remove.
  NOTIFICATION_EXTENSION_INSTALL_ERROR,

  // Sent when an bookmarks extensions API function was successfully invoked.
  // The source is the id of the extension that invoked the function, and the
  // details are a pointer to the const BookmarksFunction in question.
  // TODO(https://crbug.com/1174748): Remove.
  NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,

  // Sent when the extension updater starts checking for updates to installed
  // extensions. The source is a BrowserContext*, and there are no details.
  // TODO(https://crbug.com/1174753): Remove.
  NOTIFICATION_EXTENSION_UPDATING_STARTED,

  // The extension updater found an update and will attempt to download and
  // install it. The source is a BrowserContext*, and the details are an
  // extensions::UpdateDetails object with the extension id and version of the
  // found update.
  // TODO(https://crbug.com/1174754): Remove.
  NOTIFICATION_EXTENSION_UPDATE_FOUND,

  NOTIFICATION_EXTENSIONS_END
};

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/411569.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_

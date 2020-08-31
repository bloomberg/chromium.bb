// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_mac.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

// KeystoneObserver is a simple notification observer for Keystone status
// updates. It will be created and managed by VersionUpdaterMac.
@interface KeystoneObserver : NSObject {
 @private
  VersionUpdaterMac* _versionUpdater;  // Weak.
}

// Initialize an observer with an updater. The updater owns this object.
- (id)initWithUpdater:(VersionUpdaterMac*)updater;

// Notification callback, called with the status of keystone operations.
- (void)handleStatusNotification:(NSNotification*)notification;

@end  // @interface KeystoneObserver

@implementation KeystoneObserver

- (id)initWithUpdater:(VersionUpdaterMac*)updater {
  if ((self = [super init])) {
    _versionUpdater = updater;
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(handleStatusNotification:)
                   name:kAutoupdateStatusNotification
                 object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)handleStatusNotification:(NSNotification*)notification {
  _versionUpdater->UpdateStatus([notification userInfo]);
}

@end  // @implementation KeystoneObserver

VersionUpdater* VersionUpdater::Create(
    content::WebContents* /* web_contents */) {
  return new VersionUpdaterMac;
}

VersionUpdaterMac::VersionUpdaterMac()
    : show_promote_button_(false),
      keystone_observer_([[KeystoneObserver alloc] initWithUpdater:this]) {
}

VersionUpdaterMac::~VersionUpdaterMac() {
}

void VersionUpdaterMac::CheckForUpdate(
    const StatusCallback& status_callback,
    const PromoteCallback& promote_callback) {
  // Copy the callbacks, we will re-use this for the remaining lifetime
  // of this object.
  status_callback_ = status_callback;
  promote_callback_ = promote_callback;

  KeystoneGlue* keystone_glue = [KeystoneGlue defaultKeystoneGlue];
  if (keystone_glue && ![keystone_glue isOnReadOnlyFilesystem]) {
    AutoupdateStatus recent_status = [keystone_glue recentStatus];
    if ([keystone_glue asyncOperationPending] ||
        recent_status == kAutoupdateRegisterFailed ||
        recent_status == kAutoupdateNeedsPromotion) {
      // If an asynchronous update operation is currently pending, such as a
      // check for updates or an update installation attempt, set the status
      // up correspondingly without launching a new update check.
      //
      // If registration failed, no other operations make sense, so just go
      // straight to the error.
      UpdateStatus([[keystone_glue recentNotification] userInfo]);
    } else {
      // Launch a new update check, even if one was already completed, because
      // a new update may be available or a new update may have been installed
      // in the background since the last time the Help page was displayed.
      [keystone_glue checkForUpdate];

      // Immediately, kAutoupdateStatusNotification will be posted, with status
      // kAutoupdateChecking.
      //
      // Upon completion, kAutoupdateStatusNotification will be posted with a
      // status indicating the result of the check.
    }

    UpdateShowPromoteButton();
  } else {
    // There is no glue, or the application is on a read-only filesystem.
    // Updates and promotions are impossible.
    status_callback_.Run(DISABLED, 0, false, std::string(), 0,
                         base::string16());
  }
}

void VersionUpdaterMac::PromoteUpdater() const {
  // Tell Keystone to make software updates available for all users.
  [[KeystoneGlue defaultKeystoneGlue] promoteTicket];

  // Immediately, kAutoupdateStatusNotification will be posted, and
  // UpdateStatus() will be called with status kAutoupdatePromoting.
  //
  // Upon completion, kAutoupdateStatusNotification will be posted, and
  // UpdateStatus() will be called with a status indicating a result of the
  // installation attempt.
  //
  // If the promotion was successful, KeystoneGlue will re-register the ticket
  // and UpdateStatus() will be called again indicating first that
  // registration is in progress and subsequently that it has completed.
}

void VersionUpdaterMac::UpdateStatus(NSDictionary* dictionary) {
  AutoupdateStatus keystone_status = static_cast<AutoupdateStatus>(
      [base::mac::ObjCCastStrict<NSNumber>(
          [dictionary objectForKey:kAutoupdateStatusStatus]) intValue]);
  std::string error_messages = base::SysNSStringToUTF8(
      base::mac::ObjCCastStrict<NSString>(
          [dictionary objectForKey:kAutoupdateStatusErrorMessages]));

  bool enable_promote_button = true;
  base::string16 message;

  Status status;
  switch (keystone_status) {
    case kAutoupdateRegistering:
    case kAutoupdateChecking:
      status = CHECKING;
      enable_promote_button = false;
      break;

    case kAutoupdateRegistered:
    case kAutoupdatePromoted:
      UpdateShowPromoteButton();
      // Go straight into an update check. Return immediately, this routine
      // will be re-entered shortly with kAutoupdateChecking.
      [[KeystoneGlue defaultKeystoneGlue] checkForUpdate];
      return;

    case kAutoupdateCurrent:
      status = UPDATED;
      break;

    case kAutoupdateAvailable:
      // Install the update automatically. Return immediately, this routine
      // will be re-entered shortly with kAutoupdateInstalling.
      [[KeystoneGlue defaultKeystoneGlue] installUpdate];
      return;

    case kAutoupdateInstalling:
      status = UPDATING;
      enable_promote_button = false;
      break;

    case kAutoupdateInstalled:
      status = NEARLY_UPDATED;
      break;

    case kAutoupdatePromoting:
      // TODO(mark): KSRegistration currently handles the promotion
      // synchronously, meaning that the main thread's loop doesn't spin,
      // meaning that animations and other updates to the window won't occur
      // until KSRegistration is done with promotion. This looks laggy and bad
      // and probably qualifies as "jank." For now, there just won't be any
      // visual feedback while promotion is in progress, but it should complete
      // (or fail) very quickly.  http://b/2290009.
      return;

    case kAutoupdateRegisterFailed:
      enable_promote_button = false;
      FALLTHROUGH;
    case kAutoupdateCheckFailed:
    case kAutoupdateInstallFailed:
    case kAutoupdatePromoteFailed:
      status = FAILED;
      message = l10n_util::GetStringFUTF16Int(IDS_UPGRADE_ERROR,
                                              keystone_status);
      break;

    case kAutoupdateNeedsPromotion:
      {
        status = FAILED;
        base::string16 product_name =
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
        message = l10n_util::GetStringFUTF16(IDS_PROMOTE_INFOBAR_TEXT,
                                             product_name);
      }
      break;

    default:
      NOTREACHED();
      return;
  }

  // If there are any detailed error messages being passed along by Keystone,
  // log them. If we have an error to display, include the detail messages
  // below the error in a <pre> block. Don't bother displaying detail messages
  // on a success/in-progress/indeterminate status.
  if (!error_messages.empty()) {
    VLOG(1) << "Update error messages: " << error_messages;

    if (status == FAILED) {
      if (!message.empty()) {
        message += base::UTF8ToUTF16("<br/><br/>");
      }

      message += l10n_util::GetStringUTF16(IDS_UPGRADE_ERROR_DETAILS);
      message += base::UTF8ToUTF16("<br/><pre>");
      message += base::UTF8ToUTF16(net::EscapeForHTML(error_messages));
      message += base::UTF8ToUTF16("</pre>");
    }
  }

  if (!status_callback_.is_null())
    status_callback_.Run(status, 0, false, std::string(), 0, message);

  PromotionState promotion_state;
  if (!promote_callback_.is_null()) {
    KeystoneGlue* keystone_glue = [KeystoneGlue defaultKeystoneGlue];
    if (keystone_glue && [keystone_glue isAutoupdateEnabledForAllUsers]) {
      promotion_state = PROMOTED;
    } else {
      promotion_state = PROMOTE_HIDDEN;

      if (show_promote_button_) {
        promotion_state = enable_promote_button ? PROMOTE_ENABLED
                                                : PROMOTE_DISABLED;
      }
    }

    promote_callback_.Run(promotion_state);
  }
}

void VersionUpdaterMac::UpdateShowPromoteButton() {
  if (ObsoleteSystem::IsObsoleteNowOrSoon()) {
    // Promotion is moot upon reaching the end of the line.
    show_promote_button_ = false;
    return;
  }

  KeystoneGlue* keystone_glue = [KeystoneGlue defaultKeystoneGlue];
  AutoupdateStatus recent_status = [keystone_glue recentStatus];
  if (recent_status == kAutoupdateRegistering ||
      recent_status == kAutoupdateRegisterFailed ||
      recent_status == kAutoupdatePromoted) {
    // Promotion isn't possible at this point.
    show_promote_button_ = false;
  } else if (recent_status == kAutoupdatePromoting ||
             recent_status == kAutoupdatePromoteFailed) {
    // Show promotion UI because the user either just clicked that button or
    // because the user should be able to click it again.
    show_promote_button_ = true;
  } else {
    // Show the promote button if promotion is a possibility.
    show_promote_button_ = [keystone_glue wantsPromotion];
  }
}

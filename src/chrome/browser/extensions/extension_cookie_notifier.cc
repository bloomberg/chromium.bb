// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_cookie_notifier.h"

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"

ExtensionCookieNotifier::ExtensionCookieNotifier(Profile* profile)
    : profile_(profile), binding_(this) {
  StartListening();
}

ExtensionCookieNotifier::~ExtensionCookieNotifier() = default;

void ExtensionCookieNotifier::StartListening() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(!binding_.is_bound());

  network::mojom::CookieManager* cookie_manager =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetCookieManagerForBrowserProcess();
  if (!cookie_manager)
    return;

  network::mojom::CookieChangeListenerPtr listener_ptr;
  binding_.Bind(mojo::MakeRequest(&listener_ptr));
  binding_.set_connection_error_handler(base::BindOnce(
      &ExtensionCookieNotifier::OnConnectionError, base::Unretained(this)));

  cookie_manager->AddGlobalChangeListener(std::move(listener_ptr));
}

void ExtensionCookieNotifier::OnConnectionError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  binding_.Close();
  StartListening();
}

void ExtensionCookieNotifier::OnCookieChange(
    const net::CanonicalCookie& cookie,
    network::mojom::CookieChangeCause cause) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ChromeCookieDetails cookie_details(
      &cookie, cause != network::mojom::CookieChangeCause::INSERTED, cause);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_COOKIE_CHANGED_FOR_EXTENSIONS,
      content::Source<Profile>(profile_),
      content::Details<ChromeCookieDetails>(&cookie_details));
}

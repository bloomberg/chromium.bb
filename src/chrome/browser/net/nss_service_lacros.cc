// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service.h"

#include "chrome/browser/lacros/cert_db_initializer.h"
#include "chrome/browser/lacros/cert_db_initializer_factory.h"
#include "content/public/browser/browser_thread.h"

NssService::NssService(content::BrowserContext* context) : context_(context) {}

NssService::~NssService() = default;

NssCertDatabaseGetter NssService::CreateNSSCertDatabaseGetterForIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return CertDbInitializerFactory::GetForBrowserContext(context_)
      ->CreateNssCertDatabaseGetterForIOThread();
}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/buildflags/buildflags.h"

namespace syncer {
class ExtensionsActivity;
}

namespace browser_sync {

// Observe and record usage of extension bookmark API.
class ExtensionsActivityMonitor : public content::NotificationObserver {
 public:
  ExtensionsActivityMonitor();
  ~ExtensionsActivityMonitor() override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  const scoped_refptr<syncer::ExtensionsActivity>& GetExtensionsActivity();

 private:
  scoped_refptr<syncer::ExtensionsActivity> extensions_activity_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Used only on UI loop.
  content::NotificationRegistrar registrar_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ExtensionsActivityMonitor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSIONS_ACTIVITY_MONITOR_H_

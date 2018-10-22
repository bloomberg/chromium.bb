// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_BOOKMARK_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_BOOKMARK_SYNC_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace sync_bookmarks {
class BookmarkSyncService;
}

// Singleton that owns the bookmark sync service.
class BookmarkSyncServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of BookmarkSyncService associated with this profile
  // (creating one if none exists).
  static sync_bookmarks::BookmarkSyncService* GetForProfile(Profile* profile);

  // Returns an instance of the BookmarkSyncServiceFactory singleton.
  static BookmarkSyncServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<BookmarkSyncServiceFactory>;

  BookmarkSyncServiceFactory();
  ~BookmarkSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BookmarkSyncServiceFactory);
};

#endif  // CHROME_BROWSER_SYNC_BOOKMARK_SYNC_SERVICE_FACTORY_H_

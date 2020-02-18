// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_INDEXED_DB_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_INDEXED_DB_HELPER_H_

#include <stddef.h>

#include <list>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/public/browser/indexed_db_context.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
struct StorageUsageInfo;
}

// BrowsingDataIndexedDBHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored in indexed databases.  A
// client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class BrowsingDataIndexedDBHelper
    : public base::RefCountedThreadSafe<BrowsingDataIndexedDBHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  // Create a BrowsingDataIndexedDBHelper instance for the indexed databases
  // stored in |context|'s associated profile's user data directory.
  explicit BrowsingDataIndexedDBHelper(content::IndexedDBContext* context);

  // Starts the fetching process, which will notify its completion via
  // |callback|. This must be called only on the UI thread.
  virtual void StartFetching(FetchCallback callback);
  // Requests a single indexed database to be deleted in the IndexedDB thread.
  virtual void DeleteIndexedDB(const GURL& origin);

 protected:
  virtual ~BrowsingDataIndexedDBHelper();

  scoped_refptr<content::IndexedDBContext> indexed_db_context_;

 private:
  friend class base::RefCountedThreadSafe<BrowsingDataIndexedDBHelper>;

  // Enumerates all indexed database files in the IndexedDB thread.
  void FetchIndexedDBInfoInIndexedDBThread(FetchCallback callback);
  // Delete a single indexed database in the IndexedDB thread.
  void DeleteIndexedDBInIndexedDBThread(const GURL& origin);

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataIndexedDBHelper);
};

// This class is an implementation of BrowsingDataIndexedDBHelper that does
// not fetch its information from the Indexed DB context, but gets them
// passed by a call when accessed.
class CannedBrowsingDataIndexedDBHelper
    : public BrowsingDataIndexedDBHelper {
 public:
  explicit CannedBrowsingDataIndexedDBHelper(
      content::IndexedDBContext* context);

  // Add a indexed database to the set of canned indexed databases that is
  // returned by this helper.
  void Add(const url::Origin& origin);

  // Clear the list of canned indexed databases.
  void Reset();

  // True if no indexed databases are currently stored.
  bool empty() const;

  // Returns the number of currently stored indexed databases.
  size_t GetCount() const;

  // Returns the current list of indexed data bases.
  const std::set<url::Origin>& GetOrigins() const;

  // BrowsingDataIndexedDBHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteIndexedDB(const GURL& origin) override;

 private:
  ~CannedBrowsingDataIndexedDBHelper() override;

  std::set<url::Origin> pending_origins_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataIndexedDBHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_INDEXED_DB_HELPER_H_

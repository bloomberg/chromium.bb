// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_HISTORY_CLIENT_FAKE_BOOKMARKS_H_
#define COMPONENTS_HISTORY_CORE_TEST_HISTORY_CLIENT_FAKE_BOOKMARKS_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/history/core/browser/history_client.h"

class GURL;

namespace history {

class FakeBookmarkDatabase;
class HistoryBackendClient;

// The class HistoryClientFakeBookmarks implements HistoryClient faking the
// methods relating to bookmarks for unit testing.
class HistoryClientFakeBookmarks : public HistoryClient {
 public:
  HistoryClientFakeBookmarks();
  ~HistoryClientFakeBookmarks() override;

  void ClearAllBookmarks();
  void AddBookmark(const GURL& url);
  void AddBookmarkWithTitle(const GURL& url, const base::string16& title);
  void DelBookmark(const GURL& url);
  bool IsBookmarked(const GURL& url);

  // HistoryClient implementation.
  void OnHistoryServiceCreated(HistoryService* history_service) override;
  void Shutdown() override;
  bool CanAddURL(const GURL& url) override;
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override;
  std::unique_ptr<HistoryBackendClient> CreateBackendClient() override;

 private:
  scoped_refptr<FakeBookmarkDatabase> bookmarks_;

  DISALLOW_COPY_AND_ASSIGN(HistoryClientFakeBookmarks);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_HISTORY_CLIENT_FAKE_BOOKMARKS_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_

#include <stddef.h>
#include <string>

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_pb {
class BookmarkSpecifics;
class EntitySpecifics;
}  // namespace sync_pb

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace sync_bookmarks {

// Canonicalize |node_title| similar to legacy client's implementation by
// truncating and the appending ' ' in some cases.
std::string FullTitleToLegacyCanonicalizedTitle(const std::string& node_title);

// Used to decide if entity needs to be reuploaded for each remote change which
// is true if the proto field for the full title is missing.
bool IsFullTitleReuploadNeeded(const sync_pb::BookmarkSpecifics& specifics);

// TODO(crbug.com/978430): Remove argument |include_guid| once the client tag
// hash is required to be populated during sync metadata validation upon
// startup in SyncedBookmarkTracker::BookmarkModelMatchesMetadata().
sync_pb::EntitySpecifics CreateSpecificsFromBookmarkNode(
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    bool force_favicon_load,
    bool include_guid);

// Creates a bookmark node under the given parent node from the given specifics.
// Returns the newly created node. Callers must verify that
// |specifics| passes the IsValidBookmarkSpecifics().
const bookmarks::BookmarkNode* CreateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool is_folder,
    bookmarks::BookmarkModel* model,
    favicon::FaviconService* favicon_service);

// Updates the bookmark node |node| with the data in |specifics|. Callers must
// verify that |specifics| passes the IsValidBookmarkSpecifics().
void UpdateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    favicon::FaviconService* favicon_service);

// Replaces |node| with a BookmarkNode of equal properties and original node
// creation timestamp but a different GUID, set to |guid|, which must be a
// valid version 4 GUID. Intended to be used in cases where the GUID must be
// modified despite being immutable within the BookmarkNode itself. Returns
// the newly created node, and the original node gets deleted.
const bookmarks::BookmarkNode* ReplaceBookmarkNodeGUID(
    const bookmarks::BookmarkNode* node,
    const std::string& guid,
    bookmarks::BookmarkModel* model);

// Checks if a bookmark specifics represents a valid bookmark. |is_folder| is
// whether this specifics is for a folder. Valid specifics must not be empty,
// non-folders must contains a valid url, and all keys in the meta_info must be
// unique.
bool IsValidBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics,
                              bool is_folder);

// Checks if bookmark specifics contain a GUID that matches the value that would
// be inferred from other redundant fields. |specifics| must be valid as per
// IsValidBookmarkSpecifics().
// TODO(crbug.com/1032052): Replace this with an analogous function that
// verifies that the bookmark's client tag hash matches the GUID.
bool HasExpectedBookmarkGuid(const sync_pb::BookmarkSpecifics& specifics,
                             const std::string& originator_cache_guid,
                             const std::string& originator_client_item_id);

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_

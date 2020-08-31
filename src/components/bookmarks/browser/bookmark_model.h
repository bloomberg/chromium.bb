// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_undo_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace query_parser {
enum class MatchingAlgorithm;
}

namespace bookmarks {

class BookmarkCodecTest;
class BookmarkExpandedStateTracker;
class BookmarkLoadDetails;
class BookmarkModelObserver;
class BookmarkStorage;
class BookmarkUndoDelegate;
class ModelLoader;
class ScopedGroupBookmarkActions;
class TestBookmarkClient;
class TitledUrlIndex;
class UrlIndex;

struct UrlAndTitle;
struct TitledUrlMatch;

// BookmarkModel --------------------------------------------------------------

// BookmarkModel provides a directed acyclic graph of URLs and folders.
// Three graphs are provided for the three entry points: those on the 'bookmarks
// bar', those in the 'other bookmarks' folder and those in the 'mobile' folder.
//
// An observer may be attached to observe relevant events.
//
// You should NOT directly create a BookmarkModel, instead go through the
// BookmarkModelFactory.
class BookmarkModel : public BookmarkUndoProvider,
                      public KeyedService {
 public:
  explicit BookmarkModel(std::unique_ptr<BookmarkClient> client);
  ~BookmarkModel() override;

  // Loads the bookmarks. This is called upon creation of the BookmarkModel. You
  // need not invoke this directly. All load operations will be executed on
  // |io_task_runner|. |ui_task_runner| is the task runner the model runs on.
  void Load(PrefService* pref_service,
            const base::FilePath& profile_path,
            const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
            const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner);

  // Returns true if the model finished loading.
  bool loaded() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return loaded_;
  }

  // Returns the object responsible for tracking loading.
  scoped_refptr<ModelLoader> model_loader();

  // Returns the root node. The 'bookmark bar' node and 'other' node are
  // children of the root node.
  const BookmarkNode* root_node() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return root_;
  }

  // Returns the 'bookmark bar' node. This is NULL until loaded.
  const BookmarkNode* bookmark_bar_node() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return bookmark_bar_node_;
  }

  // Returns the 'other' node. This is NULL until loaded.
  const BookmarkNode* other_node() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return other_node_;
  }

  // Returns the 'mobile' node. This is NULL until loaded.
  const BookmarkNode* mobile_node() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mobile_node_;
  }

  bool is_root_node(const BookmarkNode* node) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node == root_;
  }

  // Returns whether the given |node| is one of the permanent nodes - root node,
  // 'bookmark bar' node, 'other' node or 'mobile' node, or one of the root
  // nodes supplied by the |client_|.
  bool is_permanent_node(const BookmarkNode* node) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node && (node == root_ || node->parent() == root_);
  }

  void AddObserver(BookmarkModelObserver* observer);
  void RemoveObserver(BookmarkModelObserver* observer);

  // Notifies the observers that an extensive set of changes is about to happen,
  // such as during import or sync, so they can delay any expensive UI updates
  // until it's finished.
  void BeginExtensiveChanges();
  void EndExtensiveChanges();

  // Returns true if this bookmark model is currently in a mode where extensive
  // changes might happen, such as for import and sync. This is helpful for
  // observers that are created after the mode has started, and want to check
  // state during their own initializer, such as the NTP.
  bool IsDoingExtensiveChanges() const { return extensive_changes_ > 0; }

  // Removes |node| from the model and deletes it. Removing a folder node
  // recursively removes all nodes. Observers are notified immediately. |node|
  // must not be a permanent node.
  void Remove(const BookmarkNode* node);

  // Removes all the non-permanent bookmark nodes that are editable by the user.
  // Observers are only notified when all nodes have been removed. There is no
  // notification for individual node removals.
  void RemoveAllUserBookmarks();

  // Moves |node| to |new_parent| and inserts it at the given |index|.
  void Move(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            size_t index);

  // Inserts a copy of |node| into |new_parent| at |index|.
  void Copy(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            size_t index);

  // Returns the favicon for |node|. If the favicon has not yet been loaded,
  // a load will be triggered and the observer of the model notified when done.
  // This also means that, on return, the node's state is guaranteed to be
  // either LOADED_FAVICON (if it was already loaded prior to the call) or
  // LOADING_FAVICON (with the exception of folders, where the call is a no-op).
  const gfx::Image& GetFavicon(const BookmarkNode* node);

  // Returns the type of the favicon for |node|. If the favicon has not yet
  // been loaded, it returns |favicon_base::IconType::kInvalid|.
  favicon_base::IconType GetFaviconType(const BookmarkNode* node);

  // Sets the title of |node|.
  void SetTitle(const BookmarkNode* node, const base::string16& title);

  // Sets the URL of |node|.
  void SetURL(const BookmarkNode* node, const GURL& url);

  // Sets the date added time of |node|.
  void SetDateAdded(const BookmarkNode* node, base::Time date_added);

  // Returns the set of nodes with the |url|.
  void GetNodesByURL(const GURL& url, std::vector<const BookmarkNode*>* nodes);

  // Returns the most recently added user node for the |url|; urls from any
  // nodes that are not editable by the user are never returned by this call.
  // Returns NULL if |url| is not bookmarked.
  const BookmarkNode* GetMostRecentlyAddedUserNodeForURL(const GURL& url);

  // Returns true if there are bookmarks, otherwise returns false.
  bool HasBookmarks();

  // Returns true is there is no user created bookmarks or folders.
  bool HasNoUserCreatedBookmarksOrFolders();

  // Returns true if the specified URL is bookmarked.
  bool IsBookmarked(const GURL& url);

  // Returns, by reference in |bookmarks|, the set of bookmarked urls and their
  // titles. This returns the unique set of URLs. For example, if two bookmarks
  // reference the same URL only one entry is added not matter the titles are
  // same or not.
  void GetBookmarks(std::vector<UrlAndTitle>* urls);

  // Adds a new folder node at the specified position with the given |guid| and
  // |meta_info|. If a GUID is provided, it must be a valid version 4 GUID,
  // otherwise a new one is generated to replace it.
  const BookmarkNode* AddFolder(
      const BookmarkNode* parent,
      size_t index,
      const base::string16& title,
      const BookmarkNode::MetaInfoMap* meta_info = nullptr,
      base::Optional<std::string> guid = base::nullopt);

  // Adds a url at the specified position with the given |creation_time|,
  // |meta_info| and |guid|. If a GUID is provided, it must be a valid version 4
  // GUID, otherwise a new one is generated to replace it.
  const BookmarkNode* AddURL(
      const BookmarkNode* parent,
      size_t index,
      const base::string16& title,
      const GURL& url,
      const BookmarkNode::MetaInfoMap* meta_info = nullptr,
      base::Optional<base::Time> creation_time = base::nullopt,
      base::Optional<std::string> guid = base::nullopt);

  // Sorts the children of |parent|, notifying observers by way of the
  // BookmarkNodeChildrenReordered method.
  void SortChildren(const BookmarkNode* parent);

  // Order the children of |parent| as specified in |ordered_nodes|.  This
  // function should only be used to reorder the child nodes of |parent| and
  // is not meant to move nodes between different parent. Notifies observers
  // using the BookmarkNodeChildrenReordered method.
  void ReorderChildren(const BookmarkNode* parent,
                       const std::vector<const BookmarkNode*>& ordered_nodes);

  // Sets the date when the folder was modified.
  void SetDateFolderModified(const BookmarkNode* node, const base::Time time);

  // Resets the 'date modified' time of the node to 0. This is used during
  // importing to exclude the newly created folders from showing up in the
  // combobox of most recently modified folders.
  void ResetDateFolderModified(const BookmarkNode* node);

  // Returns up to |max_count| of bookmarks containing each term from |text|
  // in either the title or the URL. It uses default matching algorithm.
  void GetBookmarksMatching(const base::string16& text,
                            size_t max_count,
                            std::vector<TitledUrlMatch>* matches);

  // Returns up to |max_count| of bookmarks containing each term from |text|
  // in either the title or the URL.
  void GetBookmarksMatching(const base::string16& text,
                            size_t max_count,
                            query_parser::MatchingAlgorithm matching_algorithm,
                            std::vector<TitledUrlMatch>* matches);

  // Sets the store to NULL, making it so the BookmarkModel does not persist
  // any changes to disk. This is only useful during testing to speed up
  // testing.
  void ClearStore();

  // Returns the next node ID.
  int64_t next_node_id() const { return next_node_id_; }

  // Returns the object responsible for tracking the set of expanded nodes in
  // the bookmark editor.
  BookmarkExpandedStateTracker* expanded_state_tracker() {
    return expanded_state_tracker_.get();
  }

  // Sets/deletes meta info of |node|.
  void SetNodeMetaInfo(const BookmarkNode* node,
                       const std::string& key,
                       const std::string& value);
  void SetNodeMetaInfoMap(const BookmarkNode* node,
                          const BookmarkNode::MetaInfoMap& meta_info_map);
  void DeleteNodeMetaInfo(const BookmarkNode* node,
                          const std::string& key);

  // Adds |key| to the set of meta info keys that are not copied when a node is
  // cloned.
  void AddNonClonedKey(const std::string& key);

  // Returns the set of meta info keys that should not be copied when a node is
  // cloned.
  const std::set<std::string>& non_cloned_keys() const {
    return non_cloned_keys_;
  }

  // Notify BookmarkModel that the favicons for the given page URLs (e.g.
  // http://www.google.com) and the given icon URL (e.g.
  // http://www.google.com/favicon.ico) have changed. It is valid to call
  // OnFaviconsChanged() with non-empty |page_urls| and an empty |icon_url| and
  // vice versa.
  void OnFaviconsChanged(const std::set<GURL>& page_urls,
                         const GURL& icon_url);

  // Returns the client used by this BookmarkModel.
  BookmarkClient* client() const { return client_.get(); }

  void SetUndoDelegate(BookmarkUndoDelegate* undo_delegate);

  base::WeakPtr<BookmarkModel> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class BookmarkCodecTest;
  friend class BookmarkModelFaviconTest;
  friend class BookmarkStorage;
  friend class ScopedGroupBookmarkActions;
  friend class TestBookmarkClient;

  // BookmarkUndoProvider:
  void RestoreRemovedNode(const BookmarkNode* parent,
                          size_t index,
                          std::unique_ptr<BookmarkNode> node) override;

  // Notifies the observers for adding every descedent of |node|.
  void NotifyNodeAddedForAllDescendents(const BookmarkNode* node);

  // Removes the node from internal maps and recurses through all children. If
  // the node is a url, its url is added to removed_urls.
  //
  // This does NOT delete the node.
  void RemoveNodeFromIndexRecursive(BookmarkNode* node);

  // Called when done loading. Updates internal state and notifies observers.
  void DoneLoading(std::unique_ptr<BookmarkLoadDetails> details);

  // Adds the |node| at |parent| in the specified |index| and notifies its
  // observers.
  BookmarkNode* AddNode(BookmarkNode* parent,
                        size_t index,
                        std::unique_ptr<BookmarkNode> node);

  // Adds |node| to |index_| and recursisvely invokes this for all children.
  void AddNodeToIndexRecursive(BookmarkNode* node);

  // Returns true if the parent and index are valid.
  bool IsValidIndex(const BookmarkNode* parent, size_t index, bool allow_end);

  // Notification that a favicon has finished loading. If we can decode the
  // favicon, FaviconLoaded is invoked.
  void OnFaviconDataAvailable(
      BookmarkNode* node,
      favicon_base::IconType icon_type,
      const favicon_base::FaviconImageResult& image_result);

  // Invoked from the node to load the favicon. Requests the favicon from the
  // favicon service.
  void LoadFavicon(BookmarkNode* node, favicon_base::IconType icon_type);

  // Called to notify the observers that the favicon has been loaded.
  void FaviconLoaded(const BookmarkNode* node);

  // If we're waiting on a favicon for node, the load request is canceled.
  void CancelPendingFaviconLoadRequests(BookmarkNode* node);

  // Notifies the observers that a set of changes initiated by a single user
  // action is about to happen and has completed.
  void BeginGroupedChanges();
  void EndGroupedChanges();

  // Generates and returns the next node ID.
  int64_t generate_next_node_id();

  // Sets the maximum node ID to the given value.
  // This is used by BookmarkCodec to report the maximum ID after it's done
  // decoding since during decoding codec assigns node IDs.
  void set_next_node_id(int64_t id) { next_node_id_ = id; }

  BookmarkUndoDelegate* undo_delegate() const;

  std::unique_ptr<BookmarkClient> client_;

  // Whether the initial set of data has been loaded.
  bool loaded_ = false;

  // See |root_| for details.
  std::unique_ptr<BookmarkNode> owned_root_;

  // The root node. This contains the bookmark bar node, the 'other' node and
  // the mobile node as children. The value of |root_| is initially that of
  // |owned_root_|. Once loading has completed, |owned_root_| is destroyed and
  // this is set to url_index_->root(). |owned_root_| is done as lots of
  // existing code assumes the root is non-null while loading.
  BookmarkNode* root_ = nullptr;

  BookmarkPermanentNode* bookmark_bar_node_ = nullptr;
  BookmarkPermanentNode* other_node_ = nullptr;
  BookmarkPermanentNode* mobile_node_ = nullptr;

  // The maximum ID assigned to the bookmark nodes in the model.
  int64_t next_node_id_ = 1;

  // The observers.
  base::ObserverList<BookmarkModelObserver>::Unchecked observers_;

  // Used for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Reads/writes bookmarks to disk.
  std::unique_ptr<BookmarkStorage> store_;

  std::unique_ptr<TitledUrlIndex> titled_url_index_;

#if DCHECK_IS_ON()
  // GUID index used to verify uniqueness in DCHECK-enabled builds.
  std::set<std::string> guid_index_;
#endif  // DCHECK_IS_ON()

  // Owned by |model_loader_|.
  // WARNING: in some tests this does *not* refer to
  // |ModelLoader::history_bookmark_model_|. This is because some tests
  // directly call DoneLoading().
  // TODO: this is confusing, fix tests not to circumvent ModelLoader.
  scoped_refptr<UrlIndex> url_index_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_ = 0;

  std::unique_ptr<BookmarkExpandedStateTracker> expanded_state_tracker_;

  std::set<std::string> non_cloned_keys_;

  BookmarkUndoDelegate* undo_delegate_ = nullptr;
  std::unique_ptr<BookmarkUndoDelegate> empty_undo_delegate_;

  scoped_refptr<ModelLoader> model_loader_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BookmarkModel> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkModel);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/bookmarks_helper.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/containers/stack.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_iterator.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"

namespace bookmarks_helper {

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

void ApplyBookmarkFavicon(
    const BookmarkNode* bookmark_node,
    favicon::FaviconService* favicon_service,
    const GURL& icon_url,
    const scoped_refptr<base::RefCountedMemory>& bitmap_data) {
  // Some tests use no services.
  if (favicon_service == nullptr)
    return;

  favicon_service->AddPageNoVisitForBookmark(bookmark_node->url(),
                                             bookmark_node->GetTitle());

  GURL icon_url_to_use = icon_url;

  if (icon_url.is_empty()) {
    if (bitmap_data->size() == 0) {
      // Empty icon URL and no bitmap data means no icon mapping.
      favicon_service->DeleteFaviconMappings({bookmark_node->url()},
                                             favicon_base::IconType::kFavicon);
      return;
    } else {
      // Ancient clients (prior to M25) may not be syncing the favicon URL. If
      // the icon URL is not synced, use the page URL as a fake icon URL as it
      // is guaranteed to be unique.
      icon_url_to_use = bookmark_node->url();
    }
  }

  // The client may have cached the favicon at 2x. Use MergeFavicon() as not to
  // overwrite the cached 2x favicon bitmap. Sync favicons are always
  // gfx::kFaviconSize in width and height. Store the favicon into history
  // as such.
  gfx::Size pixel_size(gfx::kFaviconSize, gfx::kFaviconSize);
  favicon_service->MergeFavicon(bookmark_node->url(), icon_url_to_use,
                                favicon_base::IconType::kFavicon, bitmap_data,
                                pixel_size);
}

// Helper class used to wait for changes to take effect on the favicon of a
// particular bookmark node in a particular bookmark model.
class FaviconChangeObserver : public bookmarks::BookmarkModelObserver {
 public:
  FaviconChangeObserver(BookmarkModel* model, const BookmarkNode* node)
      : model_(model), node_(node) {
    model->AddObserver(this);
  }
  ~FaviconChangeObserver() override { model_->RemoveObserver(this); }
  void WaitForSetFavicon() {
    DCHECK(!run_loop_.running());
    content::RunThisRunLoop(&run_loop_);
  }

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override {
  }
  void BookmarkNodeMoved(BookmarkModel* model,
                         const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override {}
  void BookmarkNodeAdded(BookmarkModel* model,
                         const BookmarkNode* parent,
                         size_t index) override {}
  void BookmarkNodeRemoved(BookmarkModel* model,
                           const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override {}
  void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {}

  void BookmarkNodeChanged(BookmarkModel* model,
                           const BookmarkNode* node) override {
    if (model == model_ && node == node_)
      model->GetFavicon(node);
  }
  void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                     const BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                  const BookmarkNode* node) override {
    if (model == model_ && node == node_)
      run_loop_.Quit();
  }

 private:
  BookmarkModel* model_;
  const BookmarkNode* node_;
  base::RunLoop run_loop_;
  DISALLOW_COPY_AND_ASSIGN(FaviconChangeObserver);
};

// Returns the number of nodes of node type |node_type| in |model| whose
// titles match the string |title|.
size_t CountNodesWithTitlesMatching(BookmarkModel* model,
                                    BookmarkNode::Type node_type,
                                    const base::string16& title) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  // Walk through the model tree looking for bookmark nodes of node type
  // |node_type| whose titles match |title|.
  size_t count = 0;
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if ((node->type() == node_type) && (node->GetTitle() == title))
      ++count;
  }
  return count;
}

// Returns the number of nodes of node type |node_type| in |model|.
size_t CountNodes(BookmarkModel* model, BookmarkNode::Type node_type) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  // Walk through the model tree looking for bookmark nodes of node type
  // |node_type|.
  size_t count = 0;
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->type() == node_type)
      ++count;
  }
  return count;
}

// Checks if the favicon data in |bitmap_a| and |bitmap_b| are equivalent.
// Returns true if they match.
bool FaviconRawBitmapsMatch(const SkBitmap& bitmap_a,
                            const SkBitmap& bitmap_b) {
  if (bitmap_a.computeByteSize() == 0U && bitmap_b.computeByteSize() == 0U)
    return true;
  if ((bitmap_a.computeByteSize() != bitmap_b.computeByteSize()) ||
      (bitmap_a.width() != bitmap_b.width()) ||
      (bitmap_a.height() != bitmap_b.height())) {
    LOG(ERROR) << "Favicon size mismatch: " << bitmap_a.computeByteSize()
               << " (" << bitmap_a.width() << "x" << bitmap_a.height()
               << ") vs. " << bitmap_b.computeByteSize() << " ("
               << bitmap_b.width() << "x" << bitmap_b.height() << ")";
    return false;
  }
  void* node_pixel_addr_a = bitmap_a.getPixels();
  EXPECT_TRUE(node_pixel_addr_a);
  void* node_pixel_addr_b = bitmap_b.getPixels();
  EXPECT_TRUE(node_pixel_addr_b);
  if (memcmp(node_pixel_addr_a, node_pixel_addr_b,
             bitmap_a.computeByteSize()) != 0) {
    LOG(ERROR) << "Favicon bitmap mismatch";
    return false;
  } else {
    return true;
  }
}

// Represents a favicon image and the icon URL associated with it.
struct FaviconData {
  FaviconData() {
  }

  FaviconData(const gfx::Image& favicon_image,
              const GURL& favicon_url)
      : image(favicon_image),
        icon_url(favicon_url) {
  }

  gfx::Image image;
  GURL icon_url;
};

// Gets the favicon and icon URL associated with |node| in |model|. Returns
// nullopt if the favicon is still loading.
base::Optional<FaviconData> GetFaviconData(BookmarkModel* model,
                                           const BookmarkNode* node) {
  // We may need to wait for the favicon to be loaded via
  // BookmarkModel::GetFavicon(), which is an asynchronous operation.
  if (!node->is_favicon_loaded()) {
    model->GetFavicon(node);
    // Favicon still loading, no data available just yet.
    return base::nullopt;
  }

  // Favicon loaded: return actual image, if there is one (the no-favicon case
  // is also considered loaded).
  return FaviconData(model->GetFavicon(node),
                     node->icon_url() ? *node->icon_url() : GURL());
}

// Sets the favicon for |profile| and |node|. |profile| may be
// |test()->verifier()|.
void SetFaviconImpl(Profile* profile,
                    const BookmarkNode* node,
                    const GURL& icon_url,
                    const gfx::Image& image,
                    FaviconSource favicon_source) {
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);

  FaviconChangeObserver observer(model, node);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (favicon_source == FROM_UI) {
    favicon_service->SetFavicons({node->url()}, icon_url,
                                 favicon_base::IconType::kFavicon, image);
  } else {
    ApplyBookmarkFavicon(node, favicon_service, icon_url, image.As1xPNGBytes());
  }

  // Wait for the favicon for |node| to be invalidated.
  observer.WaitForSetFavicon();
  model->GetFavicon(node);
}

// Expires the favicon for |profile| and |node|. |profile| may be
// |test()->verifier()|.
void ExpireFaviconImpl(Profile* profile, const BookmarkNode* node) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon_service->SetFaviconOutOfDateForPage(node->url());
}

// Used to call FaviconService APIs synchronously by making |callback| quit a
// RunLoop.
void OnGotFaviconData(
    const base::Closure& callback,
    favicon_base::FaviconRawBitmapResult* output_bitmap_result,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  *output_bitmap_result = bitmap_result;
  callback.Run();
}

// Deletes favicon mappings for |profile| and |node|. |profile| may be
// |test()->verifier()|.
void DeleteFaviconMappingsImpl(Profile* profile,
                               const BookmarkNode* node,
                               FaviconSource favicon_source) {
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);

  FaviconChangeObserver observer(model, node);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  if (favicon_source == FROM_UI) {
    favicon_service->DeleteFaviconMappings({node->url()},
                                           favicon_base::IconType::kFavicon);
  } else {
    ApplyBookmarkFavicon(
        node, favicon_service, /*icon_url=*/GURL(),
        scoped_refptr<base::RefCountedString>(new base::RefCountedString()));
  }

  // Wait for the favicon for |node| to be invalidated.
  observer.WaitForSetFavicon();
  model->GetFavicon(node);
}

// Checks if the favicon in |node_a| from |model_a| matches that of |node_b|
// from |model_b|. Returns true if they match.
bool FaviconsMatch(BookmarkModel* model_a,
                   BookmarkModel* model_b,
                   const BookmarkNode* node_a,
                   const BookmarkNode* node_b) {
  DCHECK(!node_a->is_folder());
  DCHECK(!node_b->is_folder());

  base::Optional<FaviconData> favicon_data_a = GetFaviconData(model_a, node_a);
  base::Optional<FaviconData> favicon_data_b = GetFaviconData(model_b, node_b);

  // If either of the two favicons is still loading, let's return false now
  // because observers will get notified when the load completes. Note that even
  // the lack of favicon is considered a loaded favicon.
  if (!favicon_data_a.has_value() || !favicon_data_b.has_value())
    return false;

  if (favicon_data_a->icon_url != favicon_data_b->icon_url)
    return false;

  gfx::Image image_a = favicon_data_a->image;
  gfx::Image image_b = favicon_data_b->image;

  if (image_a.IsEmpty() && image_b.IsEmpty())
    return true;  // Two empty images are equivalent.

  if (image_a.IsEmpty() != image_b.IsEmpty())
    return false;

  // Compare only the 1x bitmaps as only those are synced.
  SkBitmap bitmap_a = image_a.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  SkBitmap bitmap_b = image_b.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  return FaviconRawBitmapsMatch(bitmap_a, bitmap_b);
}

// Does a deep comparison of BookmarkNode fields in |model_a| and |model_b|.
// Returns true if they are all equal.
bool NodesMatch(const BookmarkNode* node_a, const BookmarkNode* node_b) {
  if (node_a == nullptr || node_b == nullptr)
    return node_a == node_b;
  if (node_a->is_folder() != node_b->is_folder()) {
    LOG(ERROR) << "Cannot compare folder with bookmark";
    return false;
  }
  if (node_a->GetTitle() != node_b->GetTitle()) {
    LOG(ERROR) << "Title mismatch: " << node_a->GetTitle() << " vs. "
               << node_b->GetTitle();
    return false;
  }
  if (node_a->url() != node_b->url()) {
    LOG(ERROR) << "URL mismatch: " << node_a->url() << " vs. "
               << node_b->url();
    return false;
  }
  if (node_a->parent()->GetIndexOf(node_a) !=
      node_b->parent()->GetIndexOf(node_b)) {
    LOG(ERROR) << "Index mismatch: "
               << node_a->parent()->GetIndexOf(node_a) << " vs. "
               << node_b->parent()->GetIndexOf(node_b);
    return false;
  }
  if (node_a->guid() != node_b->guid()) {
    LOG(ERROR) << "GUID mismatch: " << node_a->guid() << " vs. "
               << node_b->guid();
    return false;
  }
  return true;
}

// Helper for BookmarkModelsMatch.
bool NodeCantBeSynced(bookmarks::BookmarkClient* client,
                      const BookmarkNode* node) {
  // Return true to skip a node.
  return !client->CanSyncNode(node);
}

// Checks if the hierarchies in |model_a| and |model_b| are equivalent in
// terms of the data model and favicon. Returns true if they both match.
// Note: Some peripheral fields like creation times are allowed to mismatch.
bool BookmarkModelsMatch(BookmarkModel* model_a, BookmarkModel* model_b) {
  ui::TreeNodeIterator<const BookmarkNode> iterator_a(
      model_a->root_node(), base::Bind(&NodeCantBeSynced, model_a->client()));
  ui::TreeNodeIterator<const BookmarkNode> iterator_b(
      model_b->root_node(), base::Bind(&NodeCantBeSynced, model_b->client()));
  while (iterator_a.has_next()) {
    const BookmarkNode* node_a = iterator_a.Next();
    if (!iterator_b.has_next()) {
      LOG(ERROR) << "Models do not match.";
      return false;
    }
    const BookmarkNode* node_b = iterator_b.Next();
    if (!NodesMatch(node_a, node_b)) {
      LOG(ERROR) << "Nodes do not match";
      return false;
    }
    if (node_a->is_folder() || node_b->is_folder()) {
      continue;
    }
    if (!FaviconsMatch(model_a, model_b, node_a, node_b)) {
      LOG(ERROR) << "Favicons do not match";
      return false;
    }
  }
  return !iterator_b.has_next();
}

// Finds the node in the verifier bookmark model that corresponds to
// |foreign_node| in |foreign_model| and stores its address in |result|.
void FindNodeInVerifier(BookmarkModel* foreign_model,
                        const BookmarkNode* foreign_node,
                        const BookmarkNode** result) {
  // Climb the tree.
  base::stack<size_t> path;
  const BookmarkNode* walker = foreign_node;
  while (walker != foreign_model->root_node()) {
    path.push(size_t{walker->parent()->GetIndexOf(walker)});
    walker = walker->parent();
  }

  // Swing over to the other tree.
  walker = GetVerifierBookmarkModel()->root_node();

  // Climb down.
  while (!path.empty()) {
    ASSERT_TRUE(walker->is_folder());
    ASSERT_LT(path.top(), walker->children().size());
    walker = walker->children()[path.top()].get();
    path.pop();
  }

  ASSERT_TRUE(NodesMatch(foreign_node, walker));
  *result = walker;
}

std::vector<const BookmarkNode*> GetAllBookmarkNodes(
    const BookmarkModel* model) {
  std::vector<const BookmarkNode*> all_nodes;

  // Add root node separately as iterator does not include it.
  all_nodes.push_back(model->root_node());

  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    all_nodes.push_back(iterator.Next());
  }

  return all_nodes;
}

void TriggerAllFaviconLoading(BookmarkModel* model) {
  for (const BookmarkNode* node : GetAllBookmarkNodes(model)) {
    if (!node->is_favicon_loaded()) {
      // GetFavicon() kicks off the loading.
      model->GetFavicon(node);
    }
  }
}

}  // namespace

BookmarkUndoService* GetBookmarkUndoService(int index) {
  return BookmarkUndoServiceFactory::GetForProfile(
      sync_datatype_helper::test()->GetProfile(index));
}

BookmarkModel* GetBookmarkModel(int index) {
  return BookmarkModelFactory::GetForBrowserContext(
      sync_datatype_helper::test()->GetProfile(index));
}

const BookmarkNode* GetBookmarkBarNode(int index) {
  return GetBookmarkModel(index)->bookmark_bar_node();
}

const BookmarkNode* GetOtherNode(int index) {
  return GetBookmarkModel(index)->other_node();
}

const BookmarkNode* GetSyncedBookmarksNode(int index) {
  return GetBookmarkModel(index)->mobile_node();
}

const BookmarkNode* GetManagedNode(int index) {
  return ManagedBookmarkServiceFactory::GetForProfile(
             sync_datatype_helper::test()->GetProfile(index))
      ->managed_node();
}

BookmarkModel* GetVerifierBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(
      sync_datatype_helper::test()->verifier());
}

const BookmarkNode* AddURL(int profile,
                           const std::string& title,
                           const GURL& url) {
  return AddURL(profile, GetBookmarkBarNode(profile), 0, title,  url);
}

const BookmarkNode* AddURL(int profile,
                           size_t index,
                           const std::string& title,
                           const GURL& url) {
  return AddURL(profile, GetBookmarkBarNode(profile), index, title, url);
}

const BookmarkNode* AddURL(int profile,
                           const BookmarkNode* parent,
                           size_t index,
                           const std::string& title,
                           const GURL& url) {
  BookmarkModel* model = GetBookmarkModel(profile);
  if (bookmarks::GetBookmarkNodeByID(model, parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return nullptr;
  }
  const BookmarkNode* result =
      model->AddURL(parent, index, base::UTF8ToUTF16(title), url);
  if (!result) {
    LOG(ERROR) << "Could not add bookmark " << title << " to Profile "
               << profile;
    return nullptr;
  }
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_parent = nullptr;
    FindNodeInVerifier(model, parent, &v_parent);
    const BookmarkNode* v_node = GetVerifierBookmarkModel()->AddURL(
        v_parent, index, base::UTF8ToUTF16(title), url,
        /*meta_info=*/nullptr, result->date_added(), result->guid());
    if (!v_node) {
      LOG(ERROR) << "Could not add bookmark " << title << " to the verifier";
      return nullptr;
    }
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

const BookmarkNode* AddFolder(int profile,
                              const std::string& title) {
  return AddFolder(profile, GetBookmarkBarNode(profile), 0, title);
}

const BookmarkNode* AddFolder(int profile,
                              size_t index,
                              const std::string& title) {
  return AddFolder(profile, GetBookmarkBarNode(profile), index, title);
}

const BookmarkNode* AddFolder(int profile,
                              const BookmarkNode* parent,
                              size_t index,
                              const std::string& title) {
  BookmarkModel* model = GetBookmarkModel(profile);
  if (bookmarks::GetBookmarkNodeByID(model, parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return nullptr;
  }
  const BookmarkNode* result =
      model->AddFolder(parent, index, base::UTF8ToUTF16(title));
  EXPECT_TRUE(result);
  if (!result) {
    LOG(ERROR) << "Could not add folder " << title << " to Profile "
               << profile;
    return nullptr;
  }
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_parent = nullptr;
    FindNodeInVerifier(model, parent, &v_parent);
    const BookmarkNode* v_node = GetVerifierBookmarkModel()->AddFolder(
        v_parent, index, base::UTF8ToUTF16(title),
        /*meta_info=*/nullptr, result->guid());
    if (!v_node) {
      LOG(ERROR) << "Could not add folder " << title << " to the verifier";
      return nullptr;
    }
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

void SetTitle(int profile,
              const BookmarkNode* node,
              const std::string& new_title) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_node = nullptr;
    FindNodeInVerifier(model, node, &v_node);
    GetVerifierBookmarkModel()->SetTitle(v_node, base::UTF8ToUTF16(new_title));
  }
  model->SetTitle(node, base::UTF8ToUTF16(new_title));
}

void SetFavicon(int profile,
                const BookmarkNode* node,
                const GURL& icon_url,
                const gfx::Image& image,
                FaviconSource favicon_source) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  ASSERT_EQ(BookmarkNode::URL, node->type()) << "Node " << node->GetTitle()
                                             << " must be a url.";
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_node = nullptr;
    FindNodeInVerifier(model, node, &v_node);
    SetFaviconImpl(sync_datatype_helper::test()->verifier(),
                   v_node,
                   icon_url,
                   image,
                   favicon_source);
  }
  SetFaviconImpl(sync_datatype_helper::test()->GetProfile(profile),
                 node,
                 icon_url,
                 image,
                 favicon_source);
}

void ExpireFavicon(int profile, const BookmarkNode* node) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  ASSERT_EQ(BookmarkNode::URL, node->type()) << "Node " << node->GetTitle()
                                             << " must be a url.";

  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_node = nullptr;
    FindNodeInVerifier(model, node, &v_node);
    ExpireFaviconImpl(sync_datatype_helper::test()->verifier(), node);
  }
  ExpireFaviconImpl(sync_datatype_helper::test()->GetProfile(profile), node);
}

void CheckFaviconExpired(int profile, const GURL& icon_url) {
  base::RunLoop run_loop;

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          sync_datatype_helper::test()->GetProfile(profile),
          ServiceAccessType::EXPLICIT_ACCESS);
  base::CancelableTaskTracker task_tracker;
  favicon_base::FaviconRawBitmapResult bitmap_result;
  favicon_service->GetRawFavicon(
      icon_url, favicon_base::IconType::kFavicon, 0,
      base::BindOnce(&OnGotFaviconData, run_loop.QuitClosure(), &bitmap_result),
      &task_tracker);
  run_loop.Run();

  ASSERT_TRUE(bitmap_result.is_valid());
  ASSERT_TRUE(bitmap_result.expired);
}

void CheckHasNoFavicon(int profile, const GURL& page_url) {
  base::RunLoop run_loop;

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          sync_datatype_helper::test()->GetProfile(profile),
          ServiceAccessType::EXPLICIT_ACCESS);
  base::CancelableTaskTracker task_tracker;
  favicon_base::FaviconRawBitmapResult bitmap_result;
  favicon_service->GetRawFaviconForPageURL(
      page_url, {favicon_base::IconType::kFavicon}, 0,
      /*fallback_to_host=*/false,
      base::BindOnce(&OnGotFaviconData, run_loop.QuitClosure(), &bitmap_result),
      &task_tracker);
  run_loop.Run();

  ASSERT_FALSE(bitmap_result.is_valid());
}

void DeleteFaviconMappings(int profile,
                           const BookmarkNode* node,
                           FaviconSource favicon_source) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  ASSERT_EQ(BookmarkNode::URL, node->type())
      << "Node " << node->GetTitle() << " must be a url.";

  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_node = nullptr;
    FindNodeInVerifier(model, node, &v_node);
    DeleteFaviconMappingsImpl(sync_datatype_helper::test()->verifier(), v_node,
                              favicon_source);
  }
  DeleteFaviconMappingsImpl(sync_datatype_helper::test()->GetProfile(profile),
                            node, favicon_source);
}

const BookmarkNode* SetURL(int profile,
                           const BookmarkNode* node,
                           const GURL& new_url) {
  BookmarkModel* model = GetBookmarkModel(profile);
  if (bookmarks::GetBookmarkNodeByID(model, node->id()) != node) {
    LOG(ERROR) << "Node " << node->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return nullptr;
  }
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_node = nullptr;
    FindNodeInVerifier(model, node, &v_node);
    if (v_node->is_url())
      GetVerifierBookmarkModel()->SetURL(v_node, new_url);
  }
  if (node->is_url())
    model->SetURL(node, new_url);
  return node;
}

void Move(int profile,
          const BookmarkNode* node,
          const BookmarkNode* new_parent,
          size_t index) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_new_parent = nullptr;
    const BookmarkNode* v_node = nullptr;
    FindNodeInVerifier(model, new_parent, &v_new_parent);
    FindNodeInVerifier(model, node, &v_node);
    GetVerifierBookmarkModel()->Move(v_node, v_new_parent, index);
  }
  model->Move(node, new_parent, index);
}

void Remove(int profile, const BookmarkNode* parent, size_t index) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_parent = nullptr;
    FindNodeInVerifier(model, parent, &v_parent);
    ASSERT_TRUE(NodesMatch(parent->children()[index].get(),
                           v_parent->children()[index].get()));
    GetVerifierBookmarkModel()->Remove(v_parent->children()[index].get());
  }
  model->Remove(parent->children()[index].get());
}

void RemoveAll(int profile) {
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* root_node = GetVerifierBookmarkModel()->root_node();
    for (const auto& permanent_node : root_node->children()) {
      while (!permanent_node->children().empty()) {
        GetVerifierBookmarkModel()->Remove(
            permanent_node->children().back().get());
      }
    }
  }
  GetBookmarkModel(profile)->RemoveAllUserBookmarks();
}

void SortChildren(int profile, const BookmarkNode* parent) {
  BookmarkModel* model = GetBookmarkModel(profile);
  ASSERT_EQ(bookmarks::GetBookmarkNodeByID(model, parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (sync_datatype_helper::test()->use_verifier()) {
    const BookmarkNode* v_parent = nullptr;
    FindNodeInVerifier(model, parent, &v_parent);
    GetVerifierBookmarkModel()->SortChildren(v_parent);
  }
  model->SortChildren(parent);
}

void ReverseChildOrder(int profile, const BookmarkNode* parent) {
  ASSERT_EQ(
      bookmarks::GetBookmarkNodeByID(GetBookmarkModel(profile), parent->id()),
      parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (parent->children().empty())
    return;
  for (size_t i = 0; i < parent->children().size(); ++i) {
    Move(profile, parent->children()[i].get(), parent,
         parent->children().size() - i);
  }
}

bool ModelMatchesVerifier(int profile) {
  if (!sync_datatype_helper::test()->use_verifier()) {
    LOG(ERROR) << "Illegal to call ModelMatchesVerifier() after "
               << "DisableVerifier(). Use ModelsMatch() instead.";
    return false;
  }
  return BookmarkModelsMatch(GetVerifierBookmarkModel(),
                             GetBookmarkModel(profile));
}

bool AllModelsMatchVerifier() {
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    if (!ModelMatchesVerifier(i)) {
      LOG(ERROR) << "Model " << i << " does not match the verifier.";
      return false;
    }
  }
  return true;
}

bool ModelsMatch(int profile_a, int profile_b) {
  return BookmarkModelsMatch(GetBookmarkModel(profile_a),
                             GetBookmarkModel(profile_b));
}

bool AllModelsMatch() {
  for (int i = 1; i < sync_datatype_helper::test()->num_clients(); ++i) {
    if (!ModelsMatch(0, i)) {
      LOG(ERROR) << "Model " << i << " does not match Model 0.";
      return false;
    }
  }
  return true;
}

bool ContainsDuplicateBookmarks(int profile) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      GetBookmarkModel(profile)->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_folder())
      continue;
    std::vector<const BookmarkNode*> nodes;
    GetBookmarkModel(profile)->GetNodesByURL(node->url(), &nodes);
    EXPECT_GE(nodes.size(), 1U);
    for (std::vector<const BookmarkNode*>::const_iterator it = nodes.begin();
         it != nodes.end(); ++it) {
      if (node->id() != (*it)->id() &&
          node->parent() == (*it)->parent() &&
          node->GetTitle() == (*it)->GetTitle()) {
        return true;
      }
    }
  }
  return false;
}

bool HasNodeWithURL(int profile, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  GetBookmarkModel(profile)->GetNodesByURL(url, &nodes);
  return !nodes.empty();
}

const BookmarkNode* GetUniqueNodeByURL(int profile, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  GetBookmarkModel(profile)->GetNodesByURL(url, &nodes);
  EXPECT_EQ(1U, nodes.size());
  if (nodes.empty())
    return nullptr;
  return nodes[0];
}

size_t CountAllBookmarks(int profile) {
  return CountNodes(GetBookmarkModel(profile), BookmarkNode::URL);
}

size_t CountBookmarksWithTitlesMatching(int profile, const std::string& title) {
  return CountNodesWithTitlesMatching(GetBookmarkModel(profile),
                                      BookmarkNode::URL,
                                      base::UTF8ToUTF16(title));
}

size_t CountBookmarksWithUrlsMatching(int profile, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  GetBookmarkModel(profile)->GetNodesByURL(url, &nodes);
  return nodes.size();
}

size_t CountFoldersWithTitlesMatching(int profile, const std::string& title) {
  return CountNodesWithTitlesMatching(GetBookmarkModel(profile),
                                      BookmarkNode::FOLDER,
                                      base::UTF8ToUTF16(title));
}

bool ContainsBookmarkNodeWithGUID(int profile, const std::string& guid) {
  for (const BookmarkNode* node :
       GetAllBookmarkNodes(GetBookmarkModel(profile))) {
    if (node->guid() == guid) {
      return true;
    }
  }
  return false;
}

gfx::Image CreateFavicon(SkColor color) {
  const int dip_width = 16;
  const int dip_height = 16;
  std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  gfx::ImageSkia favicon;
  for (size_t i = 0; i < favicon_scales.size(); ++i) {
    float scale = favicon_scales[i];
    int pixel_width = dip_width * scale;
    int pixel_height = dip_height * scale;
    SkBitmap bmp;
    bmp.allocN32Pixels(pixel_width, pixel_height);
    bmp.eraseColor(color);
    favicon.AddRepresentation(gfx::ImageSkiaRep(bmp, scale));
  }
  return gfx::Image(favicon);
}

gfx::Image Create1xFaviconFromPNGFile(const std::string& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const char* kPNGExtension = ".png";
  if (!base::EndsWith(path, kPNGExtension,
                      base::CompareCase::INSENSITIVE_ASCII))
    return gfx::Image();

  base::FilePath full_path;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &full_path))
    return gfx::Image();

  full_path = full_path.AppendASCII("sync").AppendASCII(path);
  std::string contents;
  base::ReadFileToString(full_path, &contents);
  return gfx::Image::CreateFrom1xPNGBytes(
      base::RefCountedString::TakeString(&contents));
}

std::string IndexedURL(size_t i) {
  return "http://www.host.ext:1234/path/filename/" + base::NumberToString(i);
}

std::string IndexedURLTitle(size_t i) {
  return "URL Title " + base::NumberToString(i);
}

std::string IndexedFolderName(size_t i) {
  return "Folder Name " + base::NumberToString(i);
}

std::string IndexedSubfolderName(size_t i) {
  return "Subfolder Name " + base::NumberToString(i);
}

std::string IndexedSubsubfolderName(size_t i) {
  return "Subsubfolder Name " + base::NumberToString(i);
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateBookmarkServerEntity(
    const std::string& title,
    const GURL& url) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  return bookmark_builder.BuildBookmark(url);
}

AnyBookmarkChangeObserver::AnyBookmarkChangeObserver(
    const base::RepeatingClosure& cb)
    : cb_(cb) {}

AnyBookmarkChangeObserver::~AnyBookmarkChangeObserver() = default;

void AnyBookmarkChangeObserver::BookmarkModelLoaded(BookmarkModel* model,
                                                    bool ids_reassigned) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    size_t old_index,
    const BookmarkNode* new_parent,
    size_t new_index) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkNodeAdded(BookmarkModel* model,
                                                  const BookmarkNode* parent,
                                                  size_t index) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::OnWillRemoveBookmarks(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::OnWillChangeBookmarkNode(
    BookmarkModel* model,
    const BookmarkNode* node) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkNodeChanged(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::OnWillChangeBookmarkMetaInfo(
    BookmarkModel* model,
    const BookmarkNode* node) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkMetaInfoChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkNodeFaviconChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::OnWillReorderBookmarkNode(
    BookmarkModel* model,
    const BookmarkNode* node) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::ExtensiveBookmarkChangesBeginning(
    BookmarkModel* model) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::ExtensiveBookmarkChangesEnded(
    BookmarkModel* model) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::OnWillRemoveAllUserBookmarks(
    BookmarkModel* model) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::GroupedBookmarkChangesBeginning(
    BookmarkModel* model) {
  cb_.Run();
}

void AnyBookmarkChangeObserver::GroupedBookmarkChangesEnded(
    BookmarkModel* model) {
  cb_.Run();
}

BookmarkModelStatusChangeChecker::BookmarkModelStatusChangeChecker() = default;

BookmarkModelStatusChangeChecker::~BookmarkModelStatusChangeChecker() {
  for (const auto& model_and_observer : observers_) {
    model_and_observer.first->RemoveObserver(model_and_observer.second.get());
  }
}

void BookmarkModelStatusChangeChecker::Observe(
    bookmarks::BookmarkModel* model) {
  auto observer =
      std::make_unique<AnyBookmarkChangeObserver>(base::BindRepeating(
          &SingleBookmarkModelStatusChangeChecker::PostCheckExitCondition,
          weak_ptr_factory_.GetWeakPtr()));
  model->AddObserver(observer.get());
  observers_.emplace_back(model, std::move(observer));
}

void BookmarkModelStatusChangeChecker::CheckExitCondition() {
  pending_check_exit_condition_ = false;
  StatusChangeChecker::CheckExitCondition();
}

void BookmarkModelStatusChangeChecker::PostCheckExitCondition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pending_check_exit_condition_) {
    // Already posted.
    return;
  }

  pending_check_exit_condition_ = true;

  // Use base::PostTask() instead of CheckExitCondition() directly to make sure
  // that the checker doesn't immediately kick in while bookmarks are modified.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BookmarkModelStatusChangeChecker::CheckExitCondition,
                     weak_ptr_factory_.GetWeakPtr()));
}

BookmarksMatchChecker::BookmarksMatchChecker() {
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    Observe(GetBookmarkModel(i));
  }
}

bool BookmarksMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching models";
  return AllModelsMatch();
}

bool BookmarksMatchChecker::Wait() {
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    TriggerAllFaviconLoading(GetBookmarkModel(i));
  }
  return BookmarkModelStatusChangeChecker::Wait();
}

BookmarksMatchVerifierChecker::BookmarksMatchVerifierChecker() {
  Observe(GetVerifierBookmarkModel());
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    Observe(GetBookmarkModel(i));
  }
}

bool BookmarksMatchVerifierChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for model to match verifier";
  return AllModelsMatchVerifier();
}

bool BookmarksMatchVerifierChecker::Wait() {
  TriggerAllFaviconLoading(GetVerifierBookmarkModel());
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    TriggerAllFaviconLoading(GetBookmarkModel(i));
  }
  return BookmarkModelStatusChangeChecker::Wait();
}

SingleBookmarkModelStatusChangeChecker::SingleBookmarkModelStatusChangeChecker(
    int profile_index)
    : profile_index_(profile_index),
      bookmark_model_(GetBookmarkModel(profile_index)) {
  Observe(bookmark_model_);
}

SingleBookmarkModelStatusChangeChecker::
    ~SingleBookmarkModelStatusChangeChecker() = default;

int SingleBookmarkModelStatusChangeChecker::profile_index() const {
  return profile_index_;
}

BookmarkModel* SingleBookmarkModelStatusChangeChecker::bookmark_model() const {
  return bookmark_model_;
}

SingleBookmarksModelMatcherChecker::SingleBookmarksModelMatcherChecker(
    int profile_index,
    const Matcher& matcher)
    : SingleBookmarkModelStatusChangeChecker(profile_index),
      matcher_(matcher) {}

SingleBookmarksModelMatcherChecker::~SingleBookmarksModelMatcherChecker() {}

bool SingleBookmarksModelMatcherChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  const std::vector<const BookmarkNode*> all_bookmark_nodes =
      GetAllBookmarkNodes(bookmark_model());

  testing::StringMatchResultListener result_listener;
  const bool matches = testing::ExplainMatchResult(matcher_, all_bookmark_nodes,
                                                   &result_listener);
  if (TimedOut() && !matches && result_listener.str().empty()) {
    // Some matchers don't provide details via ExplainMatchResult().
    *os << "Expected: ";
    matcher_.DescribeTo(os);
    *os << "  Actual: " << testing::PrintToString(all_bookmark_nodes);
  } else {
    *os << result_listener.str();
  }
  return matches;
}

BookmarksTitleChecker::BookmarksTitleChecker(int profile_index,
                                             const std::string& title,
                                             int expected_count)
    : SingleBookmarkModelStatusChangeChecker(profile_index),
      profile_index_(profile_index),
      title_(title),
      expected_count_(expected_count) {
  DCHECK_GE(expected_count, 0) << "expected_count must be non-negative.";
}

bool BookmarksTitleChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for bookmark count to match";
  int actual_count = CountBookmarksWithTitlesMatching(profile_index_, title_);
  return expected_count_ == actual_count;
}

BookmarkFaviconLoadedChecker::BookmarkFaviconLoadedChecker(int profile_index,
                                                           const GURL& page_url)
    : SingleBookmarkModelStatusChangeChecker(profile_index),
      bookmark_node_(GetUniqueNodeByURL(profile_index, page_url)) {
  DCHECK_NE(nullptr, bookmark_node_);
}

bool BookmarkFaviconLoadedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for the favicon to be loaded for " << bookmark_node_->url();
  return bookmark_node_->is_favicon_loaded();
}

ServerBookmarksEqualityChecker::ServerBookmarksEqualityChecker(
    syncer::ProfileSyncService* service,
    fake_server::FakeServer* fake_server,
    const std::vector<ExpectedBookmark>& expected_bookmarks,
    syncer::Cryptographer* cryptographer)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      cryptographer_(cryptographer),
      expected_bookmarks_(expected_bookmarks) {}

bool ServerBookmarksEqualityChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for server-side bookmarks to match expected.";

  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  if (expected_bookmarks_.size() != entities.size()) {
    return false;
  }

  // Make a copy so we can remove bookmarks that were found.
  std::vector<ExpectedBookmark> expected = expected_bookmarks_;
  for (const sync_pb::SyncEntity& entity : entities) {
    sync_pb::BookmarkSpecifics actual_specifics;
    if (entity.specifics().has_encrypted()) {
      // If no cryptographer was provided, we expect the specifics to have
      // unencrypted data.
      if (!cryptographer_) {
        return false;
      }
      sync_pb::EntitySpecifics entity_specifics;
      EXPECT_TRUE(cryptographer_->Decrypt(entity.specifics().encrypted(),
                                          &entity_specifics));
      actual_specifics = entity_specifics.bookmark();
    } else {
      // If the cryptographer was provided, we expect the specifics to have
      // encrypted data.
      if (cryptographer_) {
        return false;
      }
      actual_specifics = entity.specifics().bookmark();
    }

    auto it =
        std::find_if(expected.begin(), expected.end(),
                     [actual_specifics](const ExpectedBookmark& bookmark) {
                       return actual_specifics.legacy_canonicalized_title() ==
                                  bookmark.title &&
                              actual_specifics.url() == bookmark.url;
                     });
    if (it != expected.end()) {
      expected.erase(it);
    } else {
      ADD_FAILURE() << "Could not find expected bookmark with title '"
                    << actual_specifics.legacy_canonicalized_title()
                    << "' and URL '" << actual_specifics.url() << "'";
    }
  }

  return true;
}

ServerBookmarksEqualityChecker::~ServerBookmarksEqualityChecker() {}

BookmarksUrlChecker::BookmarksUrlChecker(int profile,
                                         const GURL& url,
                                         int expected_count)
    : SingleBookmarkModelStatusChangeChecker(profile),
      url_(url),
      expected_count_(expected_count) {}

bool BookmarksUrlChecker::IsExitConditionSatisfied(std::ostream* os) {
  int actual_count = CountBookmarksWithUrlsMatching(profile_index(), url_);
  *os << "Expected " << expected_count_ << " bookmarks with URL " << url_
      << " but found " << actual_count;
  if (TimedOut()) {
    *os << " in "
        << testing::PrintToString(
               GetAllBookmarkNodes(GetBookmarkModel(profile_index())));
  }
  return expected_count_ == actual_count;
}

BookmarksGUIDChecker::BookmarksGUIDChecker(int profile, const std::string& guid)
    : SingleBookmarksModelMatcherChecker(profile,
                                         testing::Contains(HasGuid(guid))) {}

BookmarksGUIDChecker::~BookmarksGUIDChecker() {}

}  // namespace bookmarks_helper

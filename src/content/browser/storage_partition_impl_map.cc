// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl_map.h"

#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/cookie_store/cookie_store_context.h"
#include "content/browser/devtools/devtools_url_request_interceptor.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_url_request_job.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "crypto/sha2.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/fileapi/file_system_url_request_job_factory.h"

using storage::FileSystemContext;
using storage::BlobStorageContext;

namespace content {

namespace {

// A derivative that knows about Streams too.
class BlobProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  BlobProtocolHandler(ChromeBlobStorageContext* blob_storage_context,
                      StreamContext* stream_context)
      : blob_storage_context_(blob_storage_context),
        stream_context_(stream_context) {}

  ~BlobProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    scoped_refptr<Stream> stream =
        stream_context_->registry()->GetStream(request->url());
    if (stream.get())
      return new StreamURLRequestJob(request, network_delegate, stream);

    if (!blob_protocol_handler_) {
      // Construction is deferred because 'this' is constructed on
      // the main thread but we want blob_protocol_handler_ constructed
      // on the IO thread.
      blob_protocol_handler_.reset(
          new storage::BlobProtocolHandler(blob_storage_context_->context()));
    }
    return blob_protocol_handler_->MaybeCreateJob(request, network_delegate);
  }

 private:
  const scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  const scoped_refptr<StreamContext> stream_context_;
  mutable std::unique_ptr<storage::BlobProtocolHandler> blob_protocol_handler_;
  DISALLOW_COPY_AND_ASSIGN(BlobProtocolHandler);
};

// These constants are used to create the directory structure under the profile
// where renderers with a non-default storage partition keep their persistent
// state. This will contain a set of directories that partially mirror the
// directory structure of BrowserContext::GetPath().
//
// The kStoragePartitionDirname contains an extensions directory which is
// further partitioned by extension id, followed by another level of directories
// for the "default" extension storage partition and one directory for each
// persistent partition used by a webview tag. Example:
//
//   Storage/ext/ABCDEF/def
//   Storage/ext/ABCDEF/hash(partition name)
//
// The code in GetStoragePartitionPath() constructs these path names.
//
// TODO(nasko): Move extension related path code out of content.
const base::FilePath::CharType kStoragePartitionDirname[] =
    FILE_PATH_LITERAL("Storage");
const base::FilePath::CharType kExtensionsDirname[] =
    FILE_PATH_LITERAL("ext");
const base::FilePath::CharType kDefaultPartitionDirname[] =
    FILE_PATH_LITERAL("def");
const base::FilePath::CharType kTrashDirname[] =
    FILE_PATH_LITERAL("trash");

// Because partition names are user specified, they can be arbitrarily long
// which makes them unsuitable for paths names. We use a truncation of a
// SHA256 hash to perform a deterministic shortening of the string. The
// kPartitionNameHashBytes constant controls the length of the truncation.
// We use 6 bytes, which gives us 99.999% reliability against collisions over
// 1 million partition domains.
//
// Analysis:
// We assume that all partition names within one partition domain are
// controlled by the the same entity. Thus there is no chance for adverserial
// attack and all we care about is accidental collision. To get 5 9s over
// 1 million domains, we need the probability of a collision in any one domain
// to be
//
//    p < nroot(1000000, .99999) ~= 10^-11
//
// We use the following birthday attack approximation to calculate the max
// number of unique names for this probability:
//
//    n(p,H) = sqrt(2*H * ln(1/(1-p)))
//
// For a 6-byte hash, H = 2^(6*8).  n(10^-11, H) ~= 75
//
// An average partition domain is likely to have less than 10 unique
// partition names which is far lower than 75.
//
// Note, that for 4 9s of reliability, the limit is 237 partition names per
// partition domain.
const int kPartitionNameHashBytes = 6;

// Needed for selecting all files in ObliterateOneDirectory() below.
#if defined(OS_POSIX)
const int kAllFileTypes = base::FileEnumerator::FILES |
                          base::FileEnumerator::DIRECTORIES |
                          base::FileEnumerator::SHOW_SYM_LINKS;
#else
const int kAllFileTypes = base::FileEnumerator::FILES |
                          base::FileEnumerator::DIRECTORIES;
#endif

base::FilePath GetStoragePartitionDomainPath(
    const std::string& partition_domain) {
  CHECK(base::IsStringUTF8(partition_domain));

  return base::FilePath(kStoragePartitionDirname).Append(kExtensionsDirname)
      .Append(base::FilePath::FromUTF8Unsafe(partition_domain));
}

// Helper function for doing a depth-first deletion of the data on disk.
// Examines paths directly in |current_dir| (no recursion) and tries to
// delete from disk anything that is in, or isn't a parent of something in
// |paths_to_keep|. Paths that need further expansion are added to
// |paths_to_consider|.
void ObliterateOneDirectory(const base::FilePath& current_dir,
                            const std::vector<base::FilePath>& paths_to_keep,
                            std::vector<base::FilePath>* paths_to_consider) {
  CHECK(current_dir.IsAbsolute());

  base::FileEnumerator enumerator(current_dir, false, kAllFileTypes);
  for (base::FilePath to_delete = enumerator.Next(); !to_delete.empty();
       to_delete = enumerator.Next()) {
    // Enum tracking which of the 3 possible actions to take for |to_delete|.
    enum { kSkip, kEnqueue, kDelete } action = kDelete;

    for (auto to_keep = paths_to_keep.begin(); to_keep != paths_to_keep.end();
         ++to_keep) {
      if (to_delete == *to_keep) {
        action = kSkip;
        break;
      } else if (to_delete.IsParent(*to_keep)) {
        // |to_delete| contains a path to keep. Add to stack for further
        // processing.
        action = kEnqueue;
        break;
      }
    }

    switch (action) {
      case kDelete:
        base::DeleteFile(to_delete, true);
        break;

      case kEnqueue:
        paths_to_consider->push_back(to_delete);
        break;

      case kSkip:
        break;
    }
  }
}

// Synchronously attempts to delete |unnormalized_root|, preserving only
// entries in |paths_to_keep|. If there are no entries in |paths_to_keep| on
// disk, then it completely removes |unnormalized_root|. All paths must be
// absolute paths.
void BlockingObliteratePath(
    const base::FilePath& unnormalized_browser_context_root,
    const base::FilePath& unnormalized_root,
    const std::vector<base::FilePath>& paths_to_keep,
    const scoped_refptr<base::TaskRunner>& closure_runner,
    const base::Closure& on_gc_required) {
  // Early exit required because MakeAbsoluteFilePath() will fail on POSIX
  // if |unnormalized_root| does not exist. This is safe because there is
  // nothing to do in this situation anwyays.
  if (!base::PathExists(unnormalized_root)) {
    return;
  }

  // Never try to obliterate things outside of the browser context root or the
  // browser context root itself. Die hard.
  base::FilePath root = base::MakeAbsoluteFilePath(unnormalized_root);
  base::FilePath browser_context_root =
      base::MakeAbsoluteFilePath(unnormalized_browser_context_root);
  CHECK(!root.empty());
  CHECK(!browser_context_root.empty());
  CHECK(browser_context_root.IsParent(root) && browser_context_root != root);

  // Reduce |paths_to_keep| set to those under the root and actually on disk.
  std::vector<base::FilePath> valid_paths_to_keep;
  for (auto it = paths_to_keep.begin(); it != paths_to_keep.end(); ++it) {
    if (root.IsParent(*it) && base::PathExists(*it))
      valid_paths_to_keep.push_back(*it);
  }

  // If none of the |paths_to_keep| are valid anymore then we just whack the
  // root and be done with it.  Otherwise, signal garbage collection and do
  // a best-effort delete of the on-disk structures.
  if (valid_paths_to_keep.empty()) {
    base::DeleteFile(root, true);
    return;
  }
  closure_runner->PostTask(FROM_HERE, on_gc_required);

  // Otherwise, start at the root and delete everything that is not in
  // |valid_paths_to_keep|.
  std::vector<base::FilePath> paths_to_consider;
  paths_to_consider.push_back(root);
  while(!paths_to_consider.empty()) {
    base::FilePath path = paths_to_consider.back();
    paths_to_consider.pop_back();
    ObliterateOneDirectory(path, valid_paths_to_keep, &paths_to_consider);
  }
}

// Ensures each path in |active_paths| is a direct child of storage_root.
void NormalizeActivePaths(const base::FilePath& storage_root,
                          std::unordered_set<base::FilePath>* active_paths) {
  std::unordered_set<base::FilePath> normalized_active_paths;

  for (auto iter = active_paths->begin(); iter != active_paths->end(); ++iter) {
    base::FilePath relative_path;
    if (!storage_root.AppendRelativePath(*iter, &relative_path))
      continue;

    std::vector<base::FilePath::StringType> components;
    relative_path.GetComponents(&components);

    DCHECK(!relative_path.empty());
    normalized_active_paths.insert(storage_root.Append(components.front()));
  }

  active_paths->swap(normalized_active_paths);
}

// Deletes all entries inside the |storage_root| that are not in the
// |active_paths|.  Deletion is done in 2 steps:
//
//   (1) Moving all garbage collected paths into a trash directory.
//   (2) Asynchronously deleting the trash directory.
//
// The deletion is asynchronous because after (1) completes, calling code can
// safely continue to use the paths that had just been garbage collected
// without fear of race conditions.
//
// This code also ignores failed moves rather than attempting a smarter retry.
// Moves shouldn't fail here unless there is some out-of-band error (eg.,
// FS corruption). Retry logic is dangerous in the general case because
// there is not necessarily a guaranteed case where the logic may succeed.
//
// This function is still named BlockingGarbageCollect() because it does
// execute a few filesystem operations synchronously.
void BlockingGarbageCollect(
    const base::FilePath& storage_root,
    const scoped_refptr<base::TaskRunner>& file_access_runner,
    std::unique_ptr<std::unordered_set<base::FilePath>> active_paths) {
  CHECK(storage_root.IsAbsolute());

  NormalizeActivePaths(storage_root, active_paths.get());

  base::FileEnumerator enumerator(storage_root, false, kAllFileTypes);
  base::FilePath trash_directory;
  if (!base::CreateTemporaryDirInDir(storage_root, kTrashDirname,
                                     &trash_directory)) {
    // Unable to continue without creating the trash directory so give up.
    return;
  }
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (active_paths->find(path) == active_paths->end() &&
        path != trash_directory) {
      // Since |trash_directory| is unique for each run of this function there
      // can be no colllisions on the move.
      base::Move(path, trash_directory.Append(path.BaseName()));
    }
  }

  file_access_runner->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                trash_directory, true));
}

}  // namespace

// static
base::FilePath StoragePartitionImplMap::GetStoragePartitionPath(
    const std::string& partition_domain,
    const std::string& partition_name) {
  if (partition_domain.empty())
    return base::FilePath();

  base::FilePath path = GetStoragePartitionDomainPath(partition_domain);

  // TODO(ajwong): Mangle in-memory into this somehow, either by putting
  // it into the partition_name, or by manually adding another path component
  // here.  Otherwise, it's possible to have an in-memory StoragePartition and
  // a persistent one that return the same FilePath for GetPath().
  if (!partition_name.empty()) {
    // For analysis of why we can ignore collisions, see the comment above
    // kPartitionNameHashBytes.
    char buffer[kPartitionNameHashBytes];
    crypto::SHA256HashString(partition_name, &buffer[0],
                             sizeof(buffer));
    return path.AppendASCII(base::HexEncode(buffer, sizeof(buffer)));
  }

  return path.Append(kDefaultPartitionDirname);
}

StoragePartitionImplMap::StoragePartitionImplMap(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      file_access_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      resource_context_initialized_(false) {}

StoragePartitionImplMap::~StoragePartitionImplMap() {
}

StoragePartitionImpl* StoragePartitionImplMap::Get(
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory,
    bool can_create) {
  // Find the previously created partition if it's available.
  StoragePartitionConfig partition_config(
      partition_domain, partition_name, in_memory);

  PartitionMap::const_iterator it = partitions_.find(partition_config);
  if (it != partitions_.end())
    return it->second.get();

  if (!can_create)
    return nullptr;

  base::FilePath relative_partition_path =
      GetStoragePartitionPath(partition_domain, partition_name);

  std::unique_ptr<StoragePartitionImpl> partition_ptr(
      StoragePartitionImpl::Create(browser_context_, in_memory,
                                   relative_partition_path, partition_domain));
  StoragePartitionImpl* partition = partition_ptr.get();
  partitions_[partition_config] = std::move(partition_ptr);

  ChromeBlobStorageContext* blob_storage_context =
      ChromeBlobStorageContext::GetFor(browser_context_);
  StreamContext* stream_context = StreamContext::GetFor(browser_context_);
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kBlobScheme] = std::make_unique<BlobProtocolHandler>(
      blob_storage_context, stream_context);
  protocol_handlers[url::kFileSystemScheme] = CreateFileSystemProtocolHandler(
      partition_domain, partition->GetFileSystemContext());
  for (const auto& scheme : URLDataManagerBackend::GetWebUISchemes()) {
    protocol_handlers[scheme] = URLDataManagerBackend::CreateProtocolHandler(
        browser_context_->GetResourceContext(), blob_storage_context);
  }

  URLRequestInterceptorScopedVector request_interceptors;

  auto devtools_interceptor =
      DevToolsURLRequestInterceptor::MaybeCreate(browser_context_);
  if (devtools_interceptor)
    request_interceptors.push_back(std::move(devtools_interceptor));
  request_interceptors.push_back(std::make_unique<AppCacheInterceptor>());

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // These calls must happen after StoragePartitionImpl::Create().
    if (partition_domain.empty()) {
      partition->SetURLRequestContext(browser_context_->CreateRequestContext(
          &protocol_handlers, std::move(request_interceptors)));
    } else {
      partition->SetURLRequestContext(
          browser_context_->CreateRequestContextForStoragePartition(
              partition->GetPath(), in_memory, &protocol_handlers,
              std::move(request_interceptors)));
    }
  }

  // A separate media cache isn't used with the network service.
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    partition->SetMediaURLRequestContext(
        partition_domain.empty()
            ? browser_context_->CreateMediaRequestContext()
            : browser_context_->CreateMediaRequestContextForStoragePartition(
                  partition->GetPath(), in_memory));
  }

  // Arm the serviceworker cookie change observation API.
  partition->GetCookieStoreContext()->ListenToCookieChanges(
      partition->GetNetworkContext(), /*success_callback=*/base::DoNothing());

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // This needs to happen after SetURLRequestContext() since we need this
    // code path only for non-NetworkService cases where NetworkContext needs to
    // be initialized using |url_request_context_|, which is initialized by
    // SetURLRequestContext().
    DCHECK(partition->url_loader_factory_getter());
    DCHECK(partition->url_request_context_);
    partition->url_loader_factory_getter()->HandleFactoryRequests();
  }

  PostCreateInitialization(partition, in_memory);

  return partition;
}

void StoragePartitionImplMap::AsyncObliterate(
    const GURL& site,
    const base::Closure& on_gc_required) {
  // This method should avoid creating any StoragePartition (which would
  // create more open file handles) so that it can delete as much of the
  // data off disk as possible.
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;
  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      browser_context_, site, false, &partition_domain,
      &partition_name, &in_memory);

  // Find the active partitions for the domain. Because these partitions are
  // active, it is not possible to just delete the directories that contain
  // the backing data structures without causing the browser to crash. Instead,
  // of deleteing the directory, we tell each storage context later to
  // remove any data they have saved. This will leave the directory structure
  // intact but it will only contain empty databases.
  std::vector<StoragePartitionImpl*> active_partitions;
  std::vector<base::FilePath> paths_to_keep;
  for (PartitionMap::const_iterator it = partitions_.begin();
       it != partitions_.end();
       ++it) {
    const StoragePartitionConfig& config = it->first;
    if (config.partition_domain == partition_domain) {
      it->second->ClearData(
          // All except shader cache.
          ~StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
          StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, GURL(),
          base::Time(), base::Time::Max(), base::DoNothing());
      if (!config.in_memory) {
        paths_to_keep.push_back(it->second->GetPath());
      }
    }
  }

  // Start a best-effort delete of the on-disk storage excluding paths that are
  // known to still be in use. This is to delete any previously created
  // StoragePartition state that just happens to not have been used during this
  // run of the browser.
  base::FilePath domain_root = browser_context_->GetPath().Append(
      GetStoragePartitionDomainPath(partition_domain));

  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BlockingObliteratePath, browser_context_->GetPath(),
                     domain_root, paths_to_keep,
                     base::ThreadTaskRunnerHandle::Get(), on_gc_required));
}

void StoragePartitionImplMap::GarbageCollect(
    std::unique_ptr<std::unordered_set<base::FilePath>> active_paths,
    const base::Closure& done) {
  // Include all paths for current StoragePartitions in the active_paths since
  // they cannot be deleted safely.
  for (PartitionMap::const_iterator it = partitions_.begin();
       it != partitions_.end();
       ++it) {
    const StoragePartitionConfig& config = it->first;
    if (!config.in_memory)
      active_paths->insert(it->second->GetPath());
  }

  // Find the directory holding the StoragePartitions and delete everything in
  // there that isn't considered active.
  base::FilePath storage_root = browser_context_->GetPath().Append(
      GetStoragePartitionDomainPath(std::string()));
  file_access_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&BlockingGarbageCollect, storage_root, file_access_runner_,
                     std::move(active_paths)),
      done);
}

void StoragePartitionImplMap::ForEach(
    const BrowserContext::StoragePartitionCallback& callback) {
  for (PartitionMap::const_iterator it = partitions_.begin();
       it != partitions_.end();
       ++it) {
    callback.Run(it->second.get());
  }
}

void StoragePartitionImplMap::PostCreateInitialization(
    StoragePartitionImpl* partition,
    bool in_memory) {
  // TODO(ajwong): ResourceContexts no longer have any storage related state.
  // We should move this into a place where it is called once per
  // BrowserContext creation rather than piggybacking off the default context
  // creation.
  // Note: moving this into Get() before partitions_[] is set causes reentrency.
  if (!resource_context_initialized_) {
    resource_context_initialized_ = true;
    InitializeResourceContext(browser_context_);
  }

  scoped_refptr<net::URLRequestContextGetter> request_context_getter;
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    request_context_getter = partition->GetURLRequestContext();

  // Check first to avoid memory leak in unittests.
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &ChromeAppCacheService::InitializeOnIOThread,
            partition->GetAppCacheService(),
            in_memory ? base::FilePath()
                      : partition->GetPath().Append(kAppCacheDirname),
            browser_context_->GetResourceContext(), request_context_getter,
            base::RetainedRef(browser_context_->GetSpecialStoragePolicy())));

    partition->GetCacheStorageContext()->SetBlobParametersForCache(
        ChromeBlobStorageContext::GetFor(browser_context_));

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ServiceWorkerContextWrapper::InitializeResourceContext,
                       partition->GetServiceWorkerContext(),
                       browser_context_->GetResourceContext()));

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &PrefetchURLLoaderService::InitializeResourceContext,
            partition->GetPrefetchURLLoaderService(),
            browser_context_->GetResourceContext(), request_context_getter,
            base::RetainedRef(
                ChromeBlobStorageContext::GetFor(browser_context_))));

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&BackgroundFetchContext::InitializeOnIOThread,
                       partition->GetBackgroundFetchContext()));

    // We do not call InitializeURLRequestContext() for media contexts because,
    // other than the HTTP cache, the media contexts share the same backing
    // objects as their associated "normal" request context.  Thus, the previous
    // call serves to initialize the media request context for this storage
    // partition as well.
  }
}

}  // namespace content

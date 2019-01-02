// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fileapi/webfilesystem_impl.h"

#include <stddef.h>
#include <string>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/file_info_util.h"
#include "content/renderer/fileapi/file_system_dispatcher.h"
#include "content/renderer/fileapi/webfilewriter_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_file_info.h"
#include "third_party/blink/public/platform/web_file_system_callbacks.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

using blink::WebFileInfo;
using blink::WebFileSystemCallbacks;
using blink::WebFileSystemEntry;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<WebFileSystemImpl>>::Leaky
    g_webfilesystem_tls = LAZY_INSTANCE_INITIALIZER;

enum CallbacksUnregisterMode {
  UNREGISTER_CALLBACKS,
  DO_NOT_UNREGISTER_CALLBACKS,
};

template <typename Method, typename Params>
void CallDispatcher(const WebFileSystemCallbacks& callbacks,
                    Method async_method,
                    Method sync_method,
                    FileSystemDispatcher* dispatcher,
                    Params&& params) {
  if (callbacks.ShouldBlockUntilCompletion())
    DispatchToMethod(dispatcher, sync_method, params);
  else
    DispatchToMethod(dispatcher, async_method, params);
}

// Bridging functions that convert the arguments into Blink objects
// (e.g. WebFileInfo, WebString, WebVector<WebFileSystemEntry>)
// and call WebFileSystemCallbacks's methods.
void DidSucceed(WebFileSystemCallbacks* callbacks) {
  callbacks->DidSucceed();
}

void DidReadMetadata(const base::File::Info& file_info,
                     WebFileSystemCallbacks* callbacks) {
  WebFileInfo web_file_info;
  FileInfoToWebFileInfo(file_info, &web_file_info);
  callbacks->DidReadMetadata(web_file_info);
}

void DidReadDirectory(
    const std::vector<filesystem::mojom::DirectoryEntry>& entries,
    bool has_more,
    WebFileSystemCallbacks* callbacks) {
  WebVector<WebFileSystemEntry> file_system_entries(entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    file_system_entries[i].name =
        blink::FilePathToWebString(base::FilePath(entries[i].name));
    file_system_entries[i].is_directory =
        entries[i].type == filesystem::mojom::FsFileType::DIRECTORY;
  }
  callbacks->DidReadDirectory(file_system_entries, has_more);
}

void DidOpenFileSystem(const std::string& name,
                       const GURL& root,
                       WebFileSystemCallbacks* callbacks) {
  callbacks->DidOpenFileSystem(blink::WebString::FromUTF8(name), root);
}

void DidResolveURL(const std::string& name,
                   const GURL& root_url,
                   storage::FileSystemType mount_type,
                   const base::FilePath& file_path,
                   bool is_directory,
                   WebFileSystemCallbacks* callbacks) {
  callbacks->DidResolveURL(blink::WebString::FromUTF8(name), root_url,
                           static_cast<blink::WebFileSystemType>(mount_type),
                           blink::FilePathToWebString(file_path), is_directory);
}

void DidFail(base::File::Error error, WebFileSystemCallbacks* callbacks) {
  callbacks->DidFail(storage::FileErrorToWebFileError(error));
}

void RunCallbacks(int callbacks_id,
                  base::OnceCallback<void(WebFileSystemCallbacks*)> callback,
                  CallbacksUnregisterMode callbacks_unregister_mode) {
  WebFileSystemImpl* filesystem =
      WebFileSystemImpl::ThreadSpecificInstance(nullptr);
  if (!filesystem)
    return;
  WebFileSystemCallbacks callbacks = filesystem->GetCallbacks(callbacks_id);
  if (callbacks_unregister_mode == UNREGISTER_CALLBACKS)
    filesystem->UnregisterCallbacks(callbacks_id);
  std::move(callback).Run(&callbacks);
}

//-----------------------------------------------------------------------------
// Callback adapters.

void OpenFileSystemCallbackAdapter(int callbacks_id,
                                   const std::string& name,
                                   const GURL& root) {
  RunCallbacks(callbacks_id, base::BindOnce(&DidOpenFileSystem, name, root),
               UNREGISTER_CALLBACKS);
}

void ResolveURLCallbackAdapter(int callbacks_id,
                               const storage::FileSystemInfo& info,
                               const base::FilePath& file_path,
                               bool is_directory) {
  base::FilePath normalized_path(
      storage::VirtualPath::GetNormalizedFilePath(file_path));
  RunCallbacks(callbacks_id,
               base::BindOnce(&DidResolveURL, info.name, info.root_url,
                              info.mount_type, normalized_path, is_directory),
               UNREGISTER_CALLBACKS);
}

void StatusCallbackAdapter(int callbacks_id, base::File::Error error) {
  if (error == base::File::FILE_OK) {
    RunCallbacks(callbacks_id, base::BindOnce(&DidSucceed),
                 UNREGISTER_CALLBACKS);
  } else {
    RunCallbacks(callbacks_id, base::BindOnce(&DidFail, error),
                 UNREGISTER_CALLBACKS);
  }
}

void ReadMetadataCallbackAdapter(int callbacks_id,
                                 const base::File::Info& file_info) {
  RunCallbacks(callbacks_id, base::BindOnce(&DidReadMetadata, file_info),
               UNREGISTER_CALLBACKS);
}

void ReadDirectoryCallbackAdapter(
    int callbacks_id,
    const std::vector<filesystem::mojom::DirectoryEntry>& entries,
    bool has_more) {
  RunCallbacks(callbacks_id,
               base::BindOnce(&DidReadDirectory, entries, has_more),
               has_more ? DO_NOT_UNREGISTER_CALLBACKS : UNREGISTER_CALLBACKS);
}

void DidCreateFileWriter(int callbacks_id,
                         const GURL& path,
                         blink::WebFileWriterClient* client,
                         base::SingleThreadTaskRunner* main_thread_task_runner,
                         const base::File::Info& file_info) {
  WebFileSystemImpl* filesystem =
      WebFileSystemImpl::ThreadSpecificInstance(nullptr);
  if (!filesystem)
    return;

  WebFileSystemCallbacks callbacks = filesystem->GetCallbacks(callbacks_id);
  filesystem->UnregisterCallbacks(callbacks_id);

  if (file_info.is_directory || file_info.size < 0) {
    callbacks.DidFail(blink::kWebFileErrorInvalidState);
    return;
  }
  WebFileWriterImpl::Type type = callbacks.ShouldBlockUntilCompletion()
                                     ? WebFileWriterImpl::TYPE_SYNC
                                     : WebFileWriterImpl::TYPE_ASYNC;
  callbacks.DidCreateFileWriter(
      new WebFileWriterImpl(path, client, type, main_thread_task_runner),
      file_info.size);
}

void CreateFileWriterCallbackAdapter(
    int callbacks_id,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    const GURL& path,
    blink::WebFileWriterClient* client,
    const base::File::Info& file_info) {
  DidCreateFileWriter(callbacks_id, path, client, main_thread_task_runner.get(),
                      file_info);
}

void DidCreateSnapshotFile(
    int callbacks_id,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    base::Optional<blink::mojom::ReceivedSnapshotListenerPtr> opt_listener,
    int request_id) {
  WebFileSystemImpl* filesystem =
      WebFileSystemImpl::ThreadSpecificInstance(nullptr);
  if (!filesystem)
    return;

  WebFileSystemCallbacks callbacks = filesystem->GetCallbacks(callbacks_id);
  filesystem->UnregisterCallbacks(callbacks_id);

  WebFileInfo web_file_info;
  FileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platform_path = blink::FilePathToWebString(platform_path);
  callbacks.DidCreateSnapshotFile(web_file_info);

  // TODO(michaeln,kinuko): Use ThreadSafeSender when Blob becomes
  // non-bridge model.
  if (opt_listener) {
    opt_listener.value()->DidReceiveSnapshotFile();
  }
}

void CreateSnapshotFileCallbackAdapter(
    int callbacks_id,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    base::Optional<blink::mojom::ReceivedSnapshotListenerPtr> opt_listener,
    int request_id) {
  DidCreateSnapshotFile(callbacks_id, file_info, platform_path,
                        std::move(opt_listener), request_id);
}

}  // namespace

//-----------------------------------------------------------------------------
// WebFileSystemImpl

WebFileSystemImpl* WebFileSystemImpl::ThreadSpecificInstance(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  if (g_webfilesystem_tls.Pointer()->Get() || !main_thread_task_runner)
    return g_webfilesystem_tls.Pointer()->Get();
  WebFileSystemImpl* filesystem =
      new WebFileSystemImpl(std::move(main_thread_task_runner));
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(filesystem);
  return filesystem;
}

void WebFileSystemImpl::DeleteThreadSpecificInstance() {
  DCHECK(!WorkerThread::GetCurrentId());
  if (g_webfilesystem_tls.Pointer()->Get())
    delete g_webfilesystem_tls.Pointer()->Get();
}

WebFileSystemImpl::WebFileSystemImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_task_runner_(main_thread_task_runner),
      next_callbacks_id_(1),
      file_system_dispatcher_(std::move(main_thread_task_runner)) {
  g_webfilesystem_tls.Pointer()->Set(this);
}

WebFileSystemImpl::~WebFileSystemImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_webfilesystem_tls.Pointer()->Set(nullptr);
}

void WebFileSystemImpl::WillStopCurrentWorkerThread() {
  delete this;
}

void WebFileSystemImpl::OpenFileSystem(const blink::WebURL& storage_partition,
                                       blink::WebFileSystemType type,
                                       WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(
      callbacks, &FileSystemDispatcher::OpenFileSystem,
      &FileSystemDispatcher::OpenFileSystemSync, &file_system_dispatcher_,
      std::make_tuple(
          GURL(storage_partition), static_cast<storage::FileSystemType>(type),
          base::BindRepeating(&OpenFileSystemCallbackAdapter, callbacks_id),
          base::BindRepeating(&StatusCallbackAdapter, callbacks_id)));
}

void WebFileSystemImpl::ResolveURL(const blink::WebURL& filesystem_url,
                                   WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(
      callbacks, &FileSystemDispatcher::ResolveURL,
      &FileSystemDispatcher::ResolveURLSync, &file_system_dispatcher_,
      std::make_tuple(
          GURL(filesystem_url),
          base::BindRepeating(&ResolveURLCallbackAdapter, callbacks_id),
          base::BindRepeating(&StatusCallbackAdapter, callbacks_id)));
}

void WebFileSystemImpl::Move(const blink::WebURL& src_path,
                             const blink::WebURL& dest_path,
                             WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(callbacks, &FileSystemDispatcher::Move,
                 &FileSystemDispatcher::MoveSync, &file_system_dispatcher_,
                 std::make_tuple(GURL(src_path), GURL(dest_path),
                                 base::BindRepeating(&StatusCallbackAdapter,
                                                     callbacks_id)));
}

void WebFileSystemImpl::Copy(const blink::WebURL& src_path,
                             const blink::WebURL& dest_path,
                             WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(callbacks, &FileSystemDispatcher::Copy,
                 &FileSystemDispatcher::CopySync, &file_system_dispatcher_,
                 std::make_tuple(GURL(src_path), GURL(dest_path),
                                 base::BindRepeating(&StatusCallbackAdapter,
                                                     callbacks_id)));
}

void WebFileSystemImpl::Remove(const blink::WebURL& path,
                               WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(callbacks, &FileSystemDispatcher::Remove,
                 &FileSystemDispatcher::RemoveSync, &file_system_dispatcher_,
                 std::make_tuple(GURL(path), /*recursive=*/false,
                                 base::BindRepeating(&StatusCallbackAdapter,
                                                     callbacks_id)));
}

void WebFileSystemImpl::RemoveRecursively(const blink::WebURL& path,
                                          WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(callbacks, &FileSystemDispatcher::Remove,
                 &FileSystemDispatcher::RemoveSync, &file_system_dispatcher_,
                 std::make_tuple(GURL(path), /*recursive=*/true,
                                 base::BindRepeating(&StatusCallbackAdapter,
                                                     callbacks_id)));
}

void WebFileSystemImpl::ReadMetadata(const blink::WebURL& path,
                                     WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(
      callbacks, &FileSystemDispatcher::ReadMetadata,
      &FileSystemDispatcher::ReadMetadataSync, &file_system_dispatcher_,
      std::make_tuple(
          GURL(path),
          base::BindRepeating(&ReadMetadataCallbackAdapter, callbacks_id),
          base::BindRepeating(&StatusCallbackAdapter, callbacks_id)));
}

void WebFileSystemImpl::CreateFile(const blink::WebURL& path,
                                   bool exclusive,
                                   WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(
      callbacks, &FileSystemDispatcher::CreateFile,
      &FileSystemDispatcher::CreateFileSync, &file_system_dispatcher_,
      std::make_tuple(
          GURL(path), exclusive,
          base::BindRepeating(&StatusCallbackAdapter, callbacks_id)));
}

void WebFileSystemImpl::CreateDirectory(const blink::WebURL& path,
                                        bool exclusive,
                                        WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(
      callbacks, &FileSystemDispatcher::CreateDirectory,
      &FileSystemDispatcher::CreateDirectorySync, &file_system_dispatcher_,
      std::make_tuple(
          GURL(path), exclusive, /*recursive=*/false,
          base::BindRepeating(&StatusCallbackAdapter, callbacks_id)));
}

void WebFileSystemImpl::FileExists(const blink::WebURL& path,
                                   WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(callbacks, &FileSystemDispatcher::Exists,
                 &FileSystemDispatcher::ExistsSync, &file_system_dispatcher_,
                 std::make_tuple(GURL(path), /*directory=*/false,
                                 base::BindRepeating(&StatusCallbackAdapter,
                                                     callbacks_id)));
}

void WebFileSystemImpl::DirectoryExists(const blink::WebURL& path,
                                        WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(callbacks, &FileSystemDispatcher::Exists,
                 &FileSystemDispatcher::ExistsSync, &file_system_dispatcher_,
                 std::make_tuple(GURL(path), /*directory=*/true,
                                 base::BindRepeating(&StatusCallbackAdapter,
                                                     callbacks_id)));
}

int WebFileSystemImpl::ReadDirectory(const blink::WebURL& path,
                                     WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(
      callbacks, &FileSystemDispatcher::ReadDirectory,
      &FileSystemDispatcher::ReadDirectorySync, &file_system_dispatcher_,
      std::make_tuple(
          GURL(path),
          base::BindRepeating(&ReadDirectoryCallbackAdapter, callbacks_id),
          base::BindRepeating(&StatusCallbackAdapter, callbacks_id)));
  return callbacks_id;
}

void WebFileSystemImpl::CreateFileWriter(const WebURL& path,
                                         blink::WebFileWriterClient* client,
                                         WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(
      callbacks, &FileSystemDispatcher::ReadMetadata,
      &FileSystemDispatcher::ReadMetadataSync, &file_system_dispatcher_,
      std::make_tuple(
          GURL(path),
          base::BindRepeating(&CreateFileWriterCallbackAdapter, callbacks_id,
                              main_thread_task_runner_, GURL(path), client),
          base::BindRepeating(&StatusCallbackAdapter, callbacks_id)));
}

void WebFileSystemImpl::CreateFileWriter(
    const blink::WebURL& path,
    std::unique_ptr<CreateFileWriterCallbacks> callbacks) {
  file_system_dispatcher_.CreateFileWriter(path, std::move(callbacks));
}

void WebFileSystemImpl::CreateSnapshotFileAndReadMetadata(
    const blink::WebURL& path,
    WebFileSystemCallbacks callbacks) {
  int callbacks_id = RegisterCallbacks(callbacks);
  CallDispatcher(
      callbacks, &FileSystemDispatcher::CreateSnapshotFile,
      &FileSystemDispatcher::CreateSnapshotFileSync, &file_system_dispatcher_,
      std::make_tuple(
          GURL(path),
          base::BindRepeating(&CreateSnapshotFileCallbackAdapter, callbacks_id),
          base::BindRepeating(&StatusCallbackAdapter, callbacks_id)));
}

void WebFileSystemImpl::ChooseEntry(
    blink::WebFrame* frame,
    std::unique_ptr<ChooseEntryCallbacks> callbacks) {
  file_system_dispatcher_.ChooseEntry(
      RenderFrame::GetRoutingIdForWebFrame(frame), std::move(callbacks));
}

int WebFileSystemImpl::RegisterCallbacks(
    const WebFileSystemCallbacks& callbacks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int id = next_callbacks_id_++;
  callbacks_[id] = callbacks;
  return id;
}

WebFileSystemCallbacks WebFileSystemImpl::GetCallbacks(int callbacks_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CallbacksMap::iterator found = callbacks_.find(callbacks_id);
  DCHECK(found != callbacks_.end());
  return found->second;
}

void WebFileSystemImpl::UnregisterCallbacks(int callbacks_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CallbacksMap::iterator found = callbacks_.find(callbacks_id);
  DCHECK(found != callbacks_.end());
  callbacks_.erase(found);
}

}  // namespace content

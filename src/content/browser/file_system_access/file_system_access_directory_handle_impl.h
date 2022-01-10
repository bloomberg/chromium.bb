// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DIRECTORY_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DIRECTORY_HANDLE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/browser/file_system_access/file_system_access_handle_base.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"

namespace content {
// This is the browser side implementation of the
// FileSystemAccessDirectoryHandle mojom interface. Instances of this class are
// owned by the FileSystemAccessManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods must be called from the same
// sequence.
class CONTENT_EXPORT FileSystemAccessDirectoryHandleImpl
    : public FileSystemAccessHandleBase,
      public blink::mojom::FileSystemAccessDirectoryHandle {
 public:
  FileSystemAccessDirectoryHandleImpl(FileSystemAccessManagerImpl* manager,
                                      const BindingContext& context,
                                      const storage::FileSystemURL& url,
                                      const SharedHandleState& handle_state);
  FileSystemAccessDirectoryHandleImpl(
      const FileSystemAccessDirectoryHandleImpl&) = delete;
  FileSystemAccessDirectoryHandleImpl& operator=(
      const FileSystemAccessDirectoryHandleImpl&) = delete;
  ~FileSystemAccessDirectoryHandleImpl() override;

  // blink::mojom::FileSystemAccessDirectoryHandle:
  void GetPermissionStatus(bool writable,
                           GetPermissionStatusCallback callback) override;
  void RequestPermission(bool writable,
                         RequestPermissionCallback callback) override;
  void GetFile(const std::string& basename,
               bool create,
               GetFileCallback callback) override;
  void GetDirectory(const std::string& basename,
                    bool create,
                    GetDirectoryCallback callback) override;
  void GetEntries(mojo::PendingRemote<
                  blink::mojom::FileSystemAccessDirectoryEntriesListener>
                      pending_listener) override;
  void Move(mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
                destination_directory,
            const std::string& new_entry_name,
            MoveCallback callback) override;
  void Rename(const std::string& new_entry_name,
              RenameCallback callback) override;
  void Remove(bool recurse, RemoveCallback callback) override;
  void RemoveEntry(const std::string& basename,
                   bool recurse,
                   RemoveEntryCallback callback) override;
  void Resolve(mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
                   possible_child,
               ResolveCallback callback) override;
  void Transfer(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token)
      override;

  // Calculates a FileSystemURL for a (direct) child of this directory with the
  // given basename.  Returns an error when `basename` includes invalid input
  // like "/".
  blink::mojom::FileSystemAccessErrorPtr GetChildURL(
      const std::string& basename,
      storage::FileSystemURL* result) WARN_UNUSED_RESULT;

  // The File System Access API should not give access to files that might
  // trigger special handling from the operating system. This method is used to
  // validate that all paths passed to GetFileHandle/GetDirectoryHandle are safe
  // to be exposed to the web.
  // TODO(https://crbug.com/1154757): Merge this with
  // net::IsSafePortablePathComponent.
  static bool IsSafePathComponent(const std::string& name);

 private:
  // This method creates the file if it does not currently exists. I.e. it is
  // the implementation for passing create=true to GetFile.
  void GetFileWithWritePermission(const storage::FileSystemURL& child_url,
                                  GetFileCallback callback);
  void DidGetFile(const storage::FileSystemURL& url,
                  GetFileCallback callback,
                  base::File::Error result);
  // This method creates the directory if it does not currently exists. I.e. it
  // is the implementation for passing create=true to GetDirectory.
  void GetDirectoryWithWritePermission(const storage::FileSystemURL& child_url,
                                       GetDirectoryCallback callback);
  void DidGetDirectory(const storage::FileSystemURL& url,
                       GetDirectoryCallback callback,
                       base::File::Error result);
  void DidReadDirectory(
      mojo::Remote<blink::mojom::FileSystemAccessDirectoryEntriesListener>*
          listener,
      base::File::Error result,
      std::vector<filesystem::mojom::DirectoryEntry> file_list,
      bool has_more_entries);

  void ResolveImpl(ResolveCallback callback,
                   FileSystemAccessTransferTokenImpl* possible_child);

  // Helper to create a blink::mojom::FileSystemAccessEntry struct.
  blink::mojom::FileSystemAccessEntryPtr CreateEntry(
      const std::string& basename,
      const storage::FileSystemURL& url,
      FileSystemAccessPermissionContext::HandleType handle_type);

  base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<FileSystemAccessDirectoryHandleImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DIRECTORY_HANDLE_IMPL_H_

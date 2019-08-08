// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/browser/native_file_system/native_file_system_handle_base.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom.h"


namespace content {
class NativeFileSystemTransferTokenImpl;

// This is the browser side implementation of the
// NativeFileSystemDirectoryHandle mojom interface. Instances of this class are
// owned by the NativeFileSystemManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods should be called on the same
// sequence as storage::FileSystemContext, which today always is the IO thread.
class NativeFileSystemDirectoryHandleImpl
    : public NativeFileSystemHandleBase,
      public blink::mojom::NativeFileSystemDirectoryHandle {
 public:
  NativeFileSystemDirectoryHandleImpl(
      NativeFileSystemManagerImpl* manager,
      const BindingContext& context,
      const storage::FileSystemURL& url,
      storage::IsolatedContext::ScopedFSHandle file_system);
  ~NativeFileSystemDirectoryHandleImpl() override;

  // blink::mojom::NativeFileSystemDirectoryHandle:
  void GetFile(const std::string& name,
               bool create,
               GetFileCallback callback) override;
  void GetDirectory(const std::string& name,
                    bool create,
                    GetDirectoryCallback callback) override;
  void GetEntries(GetEntriesCallback callback) override;
  void MoveFrom(blink::mojom::NativeFileSystemTransferTokenPtr source,
                const std::string& name,
                MoveFromCallback callback) override;
  void CopyFrom(blink::mojom::NativeFileSystemTransferTokenPtr source,
                const std::string& name,
                CopyFromCallback callback) override;
  void Remove(bool recurse, RemoveCallback callback) override;
  void Transfer(
      blink::mojom::NativeFileSystemTransferTokenRequest token) override;

 private:
  // State that is kept for the duration of a GetEntries/ReadDirectory call.
  struct ReadDirectoryState;

  void DidGetFile(storage::FileSystemURL url,
                  GetFileCallback callback,
                  base::File::Error result);
  void DidGetDirectory(storage::FileSystemURL url,
                       GetDirectoryCallback callback,
                       base::File::Error result);
  void DidReadDirectory(
      ReadDirectoryState* state,
      base::File::Error result,
      std::vector<filesystem::mojom::DirectoryEntry> file_list,
      bool has_more);

  using CopyOrMoveCallback = MoveFromCallback;
  void DoCopyOrMoveFrom(const std::string& new_name,
                        bool is_copy,
                        CopyOrMoveCallback callback,
                        NativeFileSystemTransferTokenImpl* source);
  void DidCopyOrMove(CopyOrMoveCallback callback,
                     const std::string& new_name,
                     const storage::FileSystemURL& new_url,
                     bool is_directory,
                     base::File::Error result);

  // Returns a FileSystemURL for a (direct) child of this directory with the
  // given name.
  storage::FileSystemURL GetChildURL(const std::string& name);

  // Helper to create a blink::mojom::NativeFileSystemEntry struct.
  blink::mojom::NativeFileSystemEntryPtr CreateEntry(
      const std::string& name,
      const storage::FileSystemURL& url,
      bool is_directory);

  base::WeakPtrFactory<NativeFileSystemDirectoryHandleImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemDirectoryHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_

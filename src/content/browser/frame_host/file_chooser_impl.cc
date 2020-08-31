// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/file_chooser_impl.h"

#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

FileChooserImpl::FileSelectListenerImpl::~FileSelectListenerImpl() {
#if DCHECK_IS_ON()
  DCHECK(was_file_select_listener_function_called_)
      << "Must call either FileSelectListener::FileSelected() or "
         "FileSelectListener::FileSelectionCanceled()";
  // TODO(avi): Turn on the DCHECK on the following line. This cannot yet be
  // done because I can't say for sure that I know who all the callers who bind
  // blink::mojom::FileChooser are. https://crbug.com/1054811
  /* DCHECK(was_fullscreen_block_set_) << "The fullscreen block was not set"; */
#endif
  if (owner_)
    owner_->ResetListenerImpl();
}

void FileChooserImpl::FileSelectListenerImpl::SetFullscreenBlock(
    base::ScopedClosureRunner fullscreen_block) {
#if DCHECK_IS_ON()
  DCHECK(!was_fullscreen_block_set_)
      << "Fullscreen block must only be set once";
  was_fullscreen_block_set_ = true;
#endif
  fullscreen_block_ = std::move(fullscreen_block);
}

void FileChooserImpl::FileSelectListenerImpl::FileSelected(
    std::vector<blink::mojom::FileChooserFileInfoPtr> files,
    const base::FilePath& base_dir,
    blink::mojom::FileChooserParams::Mode mode) {
#if DCHECK_IS_ON()
  DCHECK(!was_file_select_listener_function_called_)
      << "Must not call both of FileSelectListener::FileSelected() and "
         "FileSelectListener::FileSelectionCanceled()";
  was_file_select_listener_function_called_ = true;
#endif
  if (owner_)
    owner_->FileSelected(std::move(files), base_dir, mode);
}

void FileChooserImpl::FileSelectListenerImpl::FileSelectionCanceled() {
#if DCHECK_IS_ON()
  DCHECK(!was_file_select_listener_function_called_)
      << "Should not call both of FileSelectListener::FileSelected() and "
         "FileSelectListener::FileSelectionCanceled()";
  was_file_select_listener_function_called_ = true;
#endif
  if (owner_)
    owner_->FileSelectionCanceled();
}

void FileChooserImpl::FileSelectListenerImpl::
    SetListenerFunctionCalledTrueForTesting() {
#if DCHECK_IS_ON()
  was_file_select_listener_function_called_ = true;
#endif
}

// static
void FileChooserImpl::Create(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingReceiver<blink::mojom::FileChooser> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FileChooserImpl>(render_frame_host),
      std::move(receiver));
}

FileChooserImpl::FileChooserImpl(RenderFrameHostImpl* render_frame_host)
    : render_frame_host_(render_frame_host) {
  Observe(WebContents::FromRenderFrameHost(render_frame_host));
}

FileChooserImpl::~FileChooserImpl() {
  if (listener_impl_)
    listener_impl_->ResetOwner();
}

void FileChooserImpl::OpenFileChooser(blink::mojom::FileChooserParamsPtr params,
                                      OpenFileChooserCallback callback) {
  if (listener_impl_ || !render_frame_host_) {
    std::move(callback).Run(nullptr);
    return;
  }
  callback_ = std::move(callback);
  auto listener = std::make_unique<FileSelectListenerImpl>(this);
  listener_impl_ = listener.get();
  // Do not allow messages with absolute paths in them as this can permit a
  // renderer to coerce the browser to perform I/O on a renderer controlled
  // path.
  if (params->default_file_name != params->default_file_name.BaseName()) {
    mojo::ReportBadMessage(
        "FileChooser: The default file name must not be an absolute path.");
    listener->FileSelectionCanceled();
    return;
  }
  render_frame_host_->delegate()->RunFileChooser(render_frame_host_,
                                                 std::move(listener), *params);
}

void FileChooserImpl::EnumerateChosenDirectory(
    const base::FilePath& directory_path,
    EnumerateChosenDirectoryCallback callback) {
  if (listener_impl_ || !render_frame_host_) {
    std::move(callback).Run(nullptr);
    return;
  }
  callback_ = std::move(callback);
  auto listener = std::make_unique<FileSelectListenerImpl>(this);
  listener_impl_ = listener.get();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->CanReadFile(render_frame_host_->GetProcess()->GetID(),
                          directory_path)) {
    render_frame_host_->delegate()->EnumerateDirectory(
        render_frame_host_, std::move(listener), directory_path);
  } else {
    listener->FileSelectionCanceled();
  }
}

void FileChooserImpl::FileSelected(
    std::vector<blink::mojom::FileChooserFileInfoPtr> files,
    const base::FilePath& base_dir,
    blink::mojom::FileChooserParams::Mode mode) {
  listener_impl_ = nullptr;
  if (!render_frame_host_)
    return;
  storage::FileSystemContext* file_system_context = nullptr;
  const int pid = render_frame_host_->GetProcess()->GetID();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  // Grant the security access requested to the given files.
  for (const auto& file : files) {
    if (mode == blink::mojom::FileChooserParams::Mode::kSave) {
      policy->GrantCreateReadWriteFile(pid, file->get_native_file()->file_path);
    } else {
      if (file->is_file_system()) {
        if (!file_system_context) {
          file_system_context =
              BrowserContext::GetStoragePartition(
                  render_frame_host_->GetProcess()->GetBrowserContext(),
                  render_frame_host_->GetSiteInstance())
                  ->GetFileSystemContext();
        }
        policy->GrantReadFileSystem(
            pid, file_system_context->CrackURL(file->get_file_system()->url)
                     .mount_filesystem_id());
      } else {
        policy->GrantReadFile(pid, file->get_native_file()->file_path);
      }
    }
  }
  std::move(callback_).Run(FileChooserResult::New(std::move(files), base_dir));
}

void FileChooserImpl::FileSelectionCanceled() {
  listener_impl_ = nullptr;
  if (!render_frame_host_)
    return;
  std::move(callback_).Run(nullptr);
}

void FileChooserImpl::ResetListenerImpl() {
  listener_impl_ = nullptr;
}

void FileChooserImpl::RenderFrameHostChanged(RenderFrameHost* old_host,
                                             RenderFrameHost* new_host) {
  if (old_host == render_frame_host_)
    render_frame_host_ = nullptr;
}

void FileChooserImpl::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  if (render_frame_host == render_frame_host_)
    render_frame_host_ = nullptr;
}

void FileChooserImpl::WebContentsDestroyed() {
  render_frame_host_ = nullptr;
}

}  // namespace content

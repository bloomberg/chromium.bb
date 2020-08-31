// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_compositor_service_impl.h"

#include "base/callback.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_compositor_client_impl.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"

namespace paint_preview {

namespace {

base::OnceClosure BindToTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceClosure closure) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::OnceClosure closure) {
        task_runner->PostTask(FROM_HERE, std::move(closure));
      },
      task_runner, std::move(closure));
}

}  // namespace

PaintPreviewCompositorServiceImpl::PaintPreviewCompositorServiceImpl(
    mojo::PendingRemote<mojom::PaintPreviewCompositorCollection> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> compositor_task_runner_,
    base::OnceClosure disconnect_handler)
    : default_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      compositor_task_runner_(compositor_task_runner_),
      compositor_service_(
          new mojo::Remote<mojom::PaintPreviewCompositorCollection>(),
          base::OnTaskRunnerDeleter(compositor_task_runner_)),
      user_disconnect_closure_(std::move(disconnect_handler)) {
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::Remote<mojom::PaintPreviewCompositorCollection>* remote,
             mojo::PendingRemote<mojom::PaintPreviewCompositorCollection>
                 pending,
             base::OnceClosure disconnect_closure) {
            remote->Bind(std::move(pending));
            BindDiscardableSharedMemoryManager(remote);
            remote->set_disconnect_handler(std::move(disconnect_closure));
          },
          compositor_service_.get(), std::move(pending_remote),
          BindToTaskRunner(
              default_task_runner_,
              base::BindOnce(
                  &PaintPreviewCompositorServiceImpl::DisconnectHandler,
                  weak_ptr_factory_.GetWeakPtr()))));
}

// The destructor for the |compositor_service_| will automatically result in any
// active compositors being killed.
PaintPreviewCompositorServiceImpl::~PaintPreviewCompositorServiceImpl() =
    default;

std::unique_ptr<PaintPreviewCompositorClient>
PaintPreviewCompositorServiceImpl::CreateCompositor(
    base::OnceClosure connected_closure) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  auto compositor = std::make_unique<PaintPreviewCompositorClientImpl>(
      compositor_task_runner_, weak_ptr_factory_.GetWeakPtr());

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::Remote<mojom::PaintPreviewCompositorCollection>* remote,
             PaintPreviewCompositorClientImpl* compositor,
             base::OnceCallback<void(const base::UnguessableToken&)>
                 on_connected) {
            // This binds the remote in compositor to the
            // |compositor_task_runner_|.
            remote->get()->CreateCompositor(
                compositor->BindNewPipeAndPassReceiver(),
                std::move(on_connected));
          },
          compositor_service_.get(), compositor.get(),
          // This builder ensures the callback it returns is called on the
          // correct sequence.
          compositor->BuildCompositorCreatedCallback(
              std::move(connected_closure),
              base::BindOnce(
                  &PaintPreviewCompositorServiceImpl::OnCompositorCreated,
                  weak_ptr_factory_.GetWeakPtr()))));

  return compositor;
}

bool PaintPreviewCompositorServiceImpl::HasActiveClients() const {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  return !active_clients_.empty();
}

void PaintPreviewCompositorServiceImpl::MarkCompositorAsDeleted(
    const base::UnguessableToken& token) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  active_clients_.erase(token);
}

const base::flat_set<base::UnguessableToken>&
PaintPreviewCompositorServiceImpl::ActiveClientsForTesting() const {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  return active_clients_;
}

void PaintPreviewCompositorServiceImpl::OnCompositorCreated(
    const base::UnguessableToken& token) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  active_clients_.insert(token);
}

void PaintPreviewCompositorServiceImpl::DisconnectHandler() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  std::move(user_disconnect_closure_).Run();
  compositor_service_.reset();
}

}  // namespace paint_preview

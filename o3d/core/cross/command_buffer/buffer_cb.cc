/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of VertexBufferCB and IndexBufferCB.

#include "core/cross/precompile.h"
#include "core/cross/command_buffer/buffer_cb.h"
#include "command_buffer/client/cross/o3d_cmd_helper.h"
#include "command_buffer/client/cross/fenced_allocator.h"

namespace o3d {
using command_buffer::CommandBufferEntry;
using command_buffer::O3DCmdHelper;
using command_buffer::FencedAllocator;

VertexBufferCB::VertexBufferCB(ServiceLocator* service_locator,
                               RendererCB *renderer)
    : VertexBuffer(service_locator),
      lock_pointer_(NULL),
      has_data_(false),
      resource_id_(command_buffer::kInvalidResource),
      renderer_(renderer) {
}


VertexBufferCB::~VertexBufferCB() {
  ConcreteFree();
}

// Sends the DestroyVertexBuffer command, and frees the ID from the allocator.
void VertexBufferCB::ConcreteFree() {
  if (resource_id_ != command_buffer::kInvalidResource) {
    O3DCmdHelper* helper = renderer_->helper();
    helper->DestroyVertexBuffer(resource_id_);
    renderer_->vertex_buffer_ids().FreeID(resource_id_);
    resource_id_ = command_buffer::kInvalidResource;
  }
}

// Allocates a resource ID, and sends the CreateVertexBuffer command.
bool VertexBufferCB::ConcreteAllocate(size_t size_in_bytes) {
  ConcreteFree();
  if (size_in_bytes > 0) {
    resource_id_ = renderer_->vertex_buffer_ids().AllocateID();
    O3DCmdHelper* helper = renderer_->helper();
    helper->CreateVertexBuffer(resource_id_, size_in_bytes,
                               command_buffer::vertex_buffer::kNone);
    has_data_ = false;
  }
  return true;
}

// Allocates the locked region into the transfer shared memory area. If the
// buffer resource contains data, copies it back (by sending the
// GetVertexBufferData command).
bool VertexBufferCB::ConcreteLock(AccessMode access_mode, void **buffer_data) {
  *buffer_data = NULL;
  if (GetSizeInBytes() == 0 || lock_pointer_)
    return false;
  lock_pointer_ = renderer_->allocator()->Alloc(GetSizeInBytes());
  if (!lock_pointer_) return false;
  if (has_data_) {
    O3DCmdHelper* helper = renderer_->helper();
    helper->GetVertexBufferData(
        resource_id_, 0, GetSizeInBytes(),
        renderer_->transfer_shm_id(),
        renderer_->allocator()->GetOffset(lock_pointer_));
    helper->Finish();
  }
  *buffer_data = lock_pointer_;
  return true;
}

// Copies the data into the resource by sending the SetVertexBufferData
// command, then frees the shared memory, pending the transfer completion.
bool VertexBufferCB::ConcreteUnlock() {
  if (GetSizeInBytes() == 0 || !lock_pointer_)
    return false;
  O3DCmdHelper* helper = renderer_->helper();
  helper->SetVertexBufferData(
      resource_id_, 0, GetSizeInBytes(),
      renderer_->transfer_shm_id(),
      renderer_->allocator()->GetOffset(lock_pointer_));

  renderer_->allocator()->FreePendingToken(lock_pointer_,
                                           helper->InsertToken());
  lock_pointer_ = NULL;
  has_data_ = true;
  return true;
}

IndexBufferCB::IndexBufferCB(ServiceLocator* service_locator,
                             RendererCB *renderer)
    : IndexBuffer(service_locator),
      lock_pointer_(NULL),
      has_data_(false),
      resource_id_(command_buffer::kInvalidResource),
      renderer_(renderer) {
}


IndexBufferCB::~IndexBufferCB() {
  ConcreteFree();
}

// Sends the DestroyIndexBuffer command, and frees the ID from the allocator.
void IndexBufferCB::ConcreteFree() {
  if (resource_id_ != command_buffer::kInvalidResource) {
    O3DCmdHelper* helper = renderer_->helper();
    helper->DestroyIndexBuffer(resource_id_);
    renderer_->index_buffer_ids().FreeID(resource_id_);
    resource_id_ = command_buffer::kInvalidResource;
  }
}

// Allocates a resource ID, and sends the CreateIndexBuffer command.
bool IndexBufferCB::ConcreteAllocate(size_t size_in_bytes) {
  ConcreteFree();
  if (size_in_bytes > 0) {
    resource_id_ = renderer_->index_buffer_ids().AllocateID();
    O3DCmdHelper* helper = renderer_->helper();
    helper->CreateIndexBuffer(
        resource_id_, size_in_bytes,
        command_buffer::index_buffer::kIndex32Bit);
    has_data_ = false;
  }
  return true;
}

// Allocates the locked region into the transfer shared memory area. If the
// buffer resource contains data, copies it back (by sending the
// GetIndexBufferData command).
bool IndexBufferCB::ConcreteLock(AccessMode access_mode, void **buffer_data) {
  *buffer_data = NULL;
  if (GetSizeInBytes() == 0 || lock_pointer_)
    return false;
  lock_pointer_ = renderer_->allocator()->Alloc(GetSizeInBytes());
  if (!lock_pointer_) return false;
  if (has_data_) {
    O3DCmdHelper* helper = renderer_->helper();
    helper->GetIndexBufferData(
        resource_id_, 0, GetSizeInBytes(),
        renderer_->transfer_shm_id(),
        renderer_->allocator()->GetOffset(lock_pointer_));
    helper->Finish();
  }
  *buffer_data = lock_pointer_;
  return true;
}

// Copies the data into the resource by sending the SetIndexBufferData
// command, then frees the shared memory, pending the transfer completion.
bool IndexBufferCB::ConcreteUnlock() {
  if (GetSizeInBytes() == 0 || !lock_pointer_)
    return false;
  O3DCmdHelper* helper = renderer_->helper();
  helper->SetIndexBufferData(
      resource_id_, 0, GetSizeInBytes(),
      renderer_->transfer_shm_id(),
      renderer_->allocator()->GetOffset(lock_pointer_));
  renderer_->allocator()->FreePendingToken(lock_pointer_,
                                           helper->InsertToken());
  lock_pointer_ = NULL;
  has_data_ = true;
  return true;
}

}  // namespace o3d

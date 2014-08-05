// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_SERVICE_PROXY_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_SERVICE_PROXY_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner.h"
#include "sync/api/attachments/attachment.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/attachments/attachment_service.h"

namespace syncer {

// AttachmentServiceProxy wraps an AttachmentService allowing multiple threads
// to share the wrapped AttachmentService and invoke its methods in the
// appropriate thread.
//
// Callbacks passed to methods on this class will be invoked in the same thread
// from which the method was called.
//
// This class does not own its wrapped AttachmentService object.  This class
// holds a WeakPtr to the wrapped object.  Once the the wrapped object is
// destroyed, method calls on this object will be no-ops.
//
// Users of this class should take care to destroy the wrapped object on the
// correct thread (wrapped_task_runner).
//
// This class is thread-safe and is designed to be passed by const-ref.
class SYNC_EXPORT AttachmentServiceProxy : public AttachmentService {
 public:
  // Default copy and assignment are welcome.

  // Construct an invalid AttachmentServiceProxy.
  AttachmentServiceProxy();

  // Construct an AttachmentServiceProxy that forwards calls to |wrapped| on the
  // |wrapped_task_runner| thread.
  //
  // Note, this object does not own |wrapped|.  When |wrapped| is destroyed,
  // calls to this object become no-ops.
  AttachmentServiceProxy(
      const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
      const base::WeakPtr<syncer::AttachmentService>& wrapped);

  virtual ~AttachmentServiceProxy();

  // AttachmentService implementation.
  virtual void GetOrDownloadAttachments(
      const AttachmentIdList& attachment_ids,
      const GetOrDownloadCallback& callback) OVERRIDE;
  virtual void DropAttachments(const AttachmentIdList& attachment_ids,
                               const DropCallback& callback) OVERRIDE;
  virtual void StoreAttachments(const AttachmentList& attachment,
                                const StoreCallback& callback) OVERRIDE;

 protected:
  // Core does the work of proxying calls to AttachmentService methods from one
  // thread to another so AttachmentServiceProxy can be an easy-to-use,
  // non-ref-counted A ref-counted class.
  //
  // Callback from AttachmentService are proxied back using free functions
  // defined in the .cc file (ProxyFooCallback functions).
  //
  // Core is ref-counted because we want to allow AttachmentServiceProxy to be
  // copy-constructable while allowing for different implementations of Core
  // (e.g. one type of core might own the wrapped AttachmentService).
  //
  // Calls to objects of this class become no-ops once its wrapped object is
  // destroyed.
  class SYNC_EXPORT Core : public AttachmentService,
                           public base::RefCountedThreadSafe<Core> {
   public:
    // Construct an AttachmentServiceProxyCore that forwards calls to |wrapped|.
    Core(const base::WeakPtr<syncer::AttachmentService>& wrapped);

    // AttachmentService implementation.
    virtual void GetOrDownloadAttachments(
        const AttachmentIdList& attachment_ids,
        const GetOrDownloadCallback& callback) OVERRIDE;
    virtual void DropAttachments(const AttachmentIdList& attachment_ids,
                                 const DropCallback& callback) OVERRIDE;
    virtual void StoreAttachments(const AttachmentList& attachment,
                                  const StoreCallback& callback) OVERRIDE;

   protected:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

   private:
    base::WeakPtr<AttachmentService> wrapped_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // Used in tests to create an AttachmentServiceProxy with a custom Core.
  AttachmentServiceProxy(
      const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
      const scoped_refptr<Core>& core);

 private:
  scoped_refptr<base::SequencedTaskRunner> wrapped_task_runner_;
  scoped_refptr<Core> core_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_SERVICE_PROXY_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_uploader_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "sync/api/attachments/attachment.h"
#include "sync/protocol/sync.pb.h"
#include "url/gurl.h"

namespace {

const char kContentType[] = "application/octet-stream";

}  // namespace

namespace syncer {

// Encapsulates all the state associated with a single upload.
class AttachmentUploaderImpl::UploadState : public net::URLFetcherDelegate,
                                            public base::NonThreadSafe {
 public:
  // Construct an UploadState.
  //
  // |owner| is a pointer to the object that will own (and must outlive!) this
  // |UploadState.
  UploadState(const GURL& upload_url,
              const scoped_refptr<net::URLRequestContextGetter>&
                  url_request_context_getter,
              const Attachment& attachment,
              const UploadCallback& user_callback,
              AttachmentUploaderImpl* owner);

  virtual ~UploadState();

  // Add |user_callback| to the list of callbacks to be invoked when this upload
  // completed.
  void AddUserCallback(const UploadCallback& user_callback);

  // Return the Attachment this object is uploading.
  const Attachment& GetAttachment();

  // URLFetcher implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  typedef std::vector<UploadCallback> UploadCallbackList;

  GURL upload_url_;
  const scoped_refptr<net::URLRequestContextGetter>&
      url_request_context_getter_;
  Attachment attachment_;
  UploadCallbackList user_callbacks_;
  scoped_ptr<net::URLFetcher> fetcher_;
  // Pointer to the AttachmentUploaderImpl that owns this object.
  AttachmentUploaderImpl* owner_;

  DISALLOW_COPY_AND_ASSIGN(UploadState);
};

AttachmentUploaderImpl::UploadState::UploadState(
    const GURL& upload_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const Attachment& attachment,
    const UploadCallback& user_callback,
    AttachmentUploaderImpl* owner)
    : upload_url_(upload_url),
      url_request_context_getter_(url_request_context_getter),
      attachment_(attachment),
      user_callbacks_(1, user_callback),
      owner_(owner) {
  DCHECK(url_request_context_getter_);
  DCHECK(upload_url_.is_valid());
  DCHECK(owner_);
  fetcher_.reset(
      net::URLFetcher::Create(upload_url_, net::URLFetcher::POST, this));
  fetcher_->SetRequestContext(url_request_context_getter_.get());
  // TODO(maniscalco): Is there a better way?  Copying the attachment data into
  // a string feels wrong given how large attachments may be (several MBs).  If
  // we may end up switching from URLFetcher to URLRequest, this copy won't be
  // necessary.
  scoped_refptr<base::RefCountedMemory> memory = attachment.GetData();
  const std::string upload_content(memory->front_as<char>(), memory->size());
  fetcher_->SetUploadData(kContentType, upload_content);
  // TODO(maniscalco): Add authentication support (bug 371516).
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DISABLE_CACHE);
  // TODO(maniscalco): Set an appropriate headers (User-Agent, Content-type, and
  // Content-length) on the request and include the content's MD5,
  // AttachmentId's unique_id and the "sync birthday" (bug 371521).
  fetcher_->Start();
}

AttachmentUploaderImpl::UploadState::~UploadState() {
}

void AttachmentUploaderImpl::UploadState::AddUserCallback(
    const UploadCallback& user_callback) {
  DCHECK(CalledOnValidThread());
  user_callbacks_.push_back(user_callback);
}

const Attachment& AttachmentUploaderImpl::UploadState::GetAttachment() {
  DCHECK(CalledOnValidThread());
  return attachment_;
}

void AttachmentUploaderImpl::UploadState::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  // TODO(maniscalco): Once the protocol is better defined, deal with the
  // various HTTP response code we may encounter.
  UploadResult result = UPLOAD_UNSPECIFIED_ERROR;
  if (source->GetResponseCode() == net::HTTP_OK) {
    result = UPLOAD_SUCCESS;
  }
  // TODO(maniscalco): Update the attachment id with server address information
  // before passing it to the callback (bug 371522).
  AttachmentId updated_id = attachment_.GetId();
  UploadCallbackList::const_iterator iter = user_callbacks_.begin();
  UploadCallbackList::const_iterator end = user_callbacks_.end();
  for (; iter != end; ++iter) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(*iter, result, updated_id));
  }
  // Destroy this object and return immediately.
  owner_->DeleteUploadStateFor(attachment_.GetId().GetProto().unique_id());
  return;
}

AttachmentUploaderImpl::AttachmentUploaderImpl(
    const std::string& url_prefix,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter)
    : url_prefix_(url_prefix),
      url_request_context_getter_(url_request_context_getter) {
  DCHECK(CalledOnValidThread());
}

AttachmentUploaderImpl::~AttachmentUploaderImpl() {
  DCHECK(CalledOnValidThread());
}

void AttachmentUploaderImpl::UploadAttachment(const Attachment& attachment,
                                              const UploadCallback& callback) {
  DCHECK(CalledOnValidThread());
  const AttachmentId attachment_id = attachment.GetId();
  const std::string unique_id = attachment_id.GetProto().unique_id();
  DCHECK(!unique_id.empty());
  StateMap::iterator iter = state_map_.find(unique_id);
  if (iter == state_map_.end()) {
    const GURL url = GetUploadURLForAttachmentId(attachment_id);
    scoped_ptr<UploadState> upload_state(new UploadState(
        url, url_request_context_getter_, attachment, callback, this));
    state_map_.add(unique_id, upload_state.Pass());
  } else {
    DCHECK(
        attachment.GetData()->Equals(iter->second->GetAttachment().GetData()));
    // We already have an upload for this attachment.  "Join" it.
    iter->second->AddUserCallback(callback);
  }
}

GURL AttachmentUploaderImpl::GetUploadURLForAttachmentId(
    const AttachmentId& attachment_id) const {
  return GURL(url_prefix_ + attachment_id.GetProto().unique_id());
}

void AttachmentUploaderImpl::DeleteUploadStateFor(const UniqueId& unique_id) {
  state_map_.erase(unique_id);
}

}  // namespace syncer

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_uploader_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "sync/api/attachments/attachment.h"
#include "sync/protocol/sync.pb.h"

namespace {

const char kContentType[] = "application/octet-stream";
const char kAttachments[] = "attachments/";

}  // namespace

namespace syncer {

// Encapsulates all the state associated with a single upload.
class AttachmentUploaderImpl::UploadState : public net::URLFetcherDelegate,
                                            public OAuth2TokenService::Consumer,
                                            public base::NonThreadSafe {
 public:
  // Construct an UploadState.
  //
  // UploadState encapsulates the state associated with a single upload.  When
  // the upload completes, the UploadState object becomes "stopped".
  //
  // |owner| is a pointer to the object that owns this UploadState.  Upon
  // completion this object will PostTask to owner's OnUploadStateStopped
  // method.
  UploadState(
      const GURL& upload_url,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      const Attachment& attachment,
      const UploadCallback& user_callback,
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenServiceRequest::TokenServiceProvider* token_service_provider,
      const base::WeakPtr<AttachmentUploaderImpl>& owner);

  virtual ~UploadState();

  // Returns true if this object is stopped.  Once stopped, this object is
  // effectively dead and can be destroyed.
  bool IsStopped() const;

  // Add |user_callback| to the list of callbacks to be invoked when this upload
  // completed.
  //
  // It is an error to call |AddUserCallback| on a stopped UploadState (see
  // |IsStopped|).
  void AddUserCallback(const UploadCallback& user_callback);

  // Return the Attachment this object is uploading.
  const Attachment& GetAttachment();

  // URLFetcher implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // OAuth2TokenService::Consumer.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

 private:
  typedef std::vector<UploadCallback> UploadCallbackList;

  void GetToken();

  void StopAndReportResult(const UploadResult& result,
                           const AttachmentId& attachment_id);

  bool is_stopped_;
  GURL upload_url_;
  const scoped_refptr<net::URLRequestContextGetter>&
      url_request_context_getter_;
  Attachment attachment_;
  UploadCallbackList user_callbacks_;
  scoped_ptr<net::URLFetcher> fetcher_;
  std::string account_id_;
  OAuth2TokenService::ScopeSet scopes_;
  std::string access_token_;
  OAuth2TokenServiceRequest::TokenServiceProvider* token_service_provider_;
  // Pointer to the AttachmentUploaderImpl that owns this object.
  base::WeakPtr<AttachmentUploaderImpl> owner_;
  scoped_ptr<OAuth2TokenServiceRequest> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(UploadState);
};

AttachmentUploaderImpl::UploadState::UploadState(
    const GURL& upload_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const Attachment& attachment,
    const UploadCallback& user_callback,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenServiceRequest::TokenServiceProvider* token_service_provider,
    const base::WeakPtr<AttachmentUploaderImpl>& owner)
    : OAuth2TokenService::Consumer("attachment-uploader-impl"),
      is_stopped_(false),
      upload_url_(upload_url),
      url_request_context_getter_(url_request_context_getter),
      attachment_(attachment),
      user_callbacks_(1, user_callback),
      account_id_(account_id),
      scopes_(scopes),
      token_service_provider_(token_service_provider),
      owner_(owner) {
  DCHECK(upload_url_.is_valid());
  DCHECK(url_request_context_getter_.get());
  DCHECK(!account_id_.empty());
  DCHECK(!scopes_.empty());
  DCHECK(token_service_provider_);
  GetToken();
}

AttachmentUploaderImpl::UploadState::~UploadState() {
}

bool AttachmentUploaderImpl::UploadState::IsStopped() const {
  DCHECK(CalledOnValidThread());
  return is_stopped_;
}

void AttachmentUploaderImpl::UploadState::AddUserCallback(
    const UploadCallback& user_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_stopped_);
  user_callbacks_.push_back(user_callback);
}

const Attachment& AttachmentUploaderImpl::UploadState::GetAttachment() {
  DCHECK(CalledOnValidThread());
  return attachment_;
}

void AttachmentUploaderImpl::UploadState::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  if (is_stopped_) {
    return;
  }

  UploadResult result = UPLOAD_TRANSIENT_ERROR;
  AttachmentId attachment_id = attachment_.GetId();
  const int response_code = source->GetResponseCode();
  if (response_code == net::HTTP_OK) {
    result = UPLOAD_SUCCESS;
  } else if (response_code == net::HTTP_UNAUTHORIZED) {
    // Server tells us we've got a bad token so invalidate it.
    OAuth2TokenServiceRequest::InvalidateToken(
        token_service_provider_, account_id_, scopes_, access_token_);
    // Fail the request, but indicate that it may be successful if retried.
    result = UPLOAD_TRANSIENT_ERROR;
  } else if (response_code == net::HTTP_FORBIDDEN) {
    // User is not allowed to use attachments.  Retrying won't help.
    result = UPLOAD_UNSPECIFIED_ERROR;
  } else if (response_code == net::URLFetcher::RESPONSE_CODE_INVALID) {
    result = UPLOAD_TRANSIENT_ERROR;
  }
  StopAndReportResult(result, attachment_id);
}

void AttachmentUploaderImpl::UploadState::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(CalledOnValidThread());
  if (is_stopped_) {
    return;
  }

  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  access_token_ = access_token;
  fetcher_.reset(
      net::URLFetcher::Create(upload_url_, net::URLFetcher::POST, this));
  fetcher_->SetAutomaticallyRetryOn5xx(false);
  fetcher_->SetRequestContext(url_request_context_getter_.get());
  // TODO(maniscalco): Is there a better way?  Copying the attachment data into
  // a string feels wrong given how large attachments may be (several MBs).  If
  // we may end up switching from URLFetcher to URLRequest, this copy won't be
  // necessary.
  scoped_refptr<base::RefCountedMemory> memory = attachment_.GetData();
  const std::string upload_content(memory->front_as<char>(), memory->size());
  fetcher_->SetUploadData(kContentType, upload_content);
  const std::string auth_header("Authorization: Bearer " + access_token_);
  fetcher_->AddExtraRequestHeader(auth_header);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DISABLE_CACHE);
  // TODO(maniscalco): Set an appropriate headers (User-Agent, Content-type, and
  // Content-length) on the request and include the content's MD5,
  // AttachmentId's unique_id and the "sync birthday" (bug 371521).
  fetcher_->Start();
}

void AttachmentUploaderImpl::UploadState::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(CalledOnValidThread());
  if (is_stopped_) {
    return;
  }

  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  // TODO(maniscalco): We treat this as a transient error, but it may in fact be
  // a very long lived error and require user action.  Consider differentiating
  // between the causes of GetToken failure and act accordingly.  Think about
  // the causes of GetToken failure. Are there (bug 412802).
  StopAndReportResult(UPLOAD_TRANSIENT_ERROR, attachment_.GetId());
}

void AttachmentUploaderImpl::UploadState::GetToken() {
  access_token_request_ = OAuth2TokenServiceRequest::CreateAndStart(
      token_service_provider_, account_id_, scopes_, this);
}

void AttachmentUploaderImpl::UploadState::StopAndReportResult(
    const UploadResult& result,
    const AttachmentId& attachment_id) {
  DCHECK(!is_stopped_);
  is_stopped_ = true;
  UploadCallbackList::const_iterator iter = user_callbacks_.begin();
  UploadCallbackList::const_iterator end = user_callbacks_.end();
  for (; iter != end; ++iter) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(*iter, result, attachment_id));
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentUploaderImpl::OnUploadStateStopped,
                 owner_,
                 attachment_id.GetProto().unique_id()));
}

AttachmentUploaderImpl::AttachmentUploaderImpl(
    const GURL& sync_service_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>&
        token_service_provider)
    : sync_service_url_(sync_service_url),
      url_request_context_getter_(url_request_context_getter),
      account_id_(account_id),
      scopes_(scopes),
      token_service_provider_(token_service_provider),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK(!account_id.empty());
  DCHECK(!scopes.empty());
  DCHECK(token_service_provider_.get());
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
  if (iter != state_map_.end()) {
    // We have an old upload request for this attachment...
    if (!iter->second->IsStopped()) {
      // "join" to it.
      DCHECK(attachment.GetData()
                 ->Equals(iter->second->GetAttachment().GetData()));
      iter->second->AddUserCallback(callback);
      return;
    } else {
      // It's stopped so we can't use it.  Delete it.
      state_map_.erase(iter);
    }
  }

  const GURL url = GetURLForAttachmentId(sync_service_url_, attachment_id);
  scoped_ptr<UploadState> upload_state(
      new UploadState(url,
                      url_request_context_getter_,
                      attachment,
                      callback,
                      account_id_,
                      scopes_,
                      token_service_provider_.get(),
                      weak_ptr_factory_.GetWeakPtr()));
  state_map_.add(unique_id, upload_state.Pass());
}

// Static.
GURL AttachmentUploaderImpl::GetURLForAttachmentId(
    const GURL& sync_service_url,
    const AttachmentId& attachment_id) {
  std::string path = sync_service_url.path();
  if (path.empty() || *path.rbegin() != '/') {
    path += '/';
  }
  path += kAttachments;
  path += attachment_id.GetProto().unique_id();
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return sync_service_url.ReplaceComponents(replacements);
}

void AttachmentUploaderImpl::OnUploadStateStopped(const UniqueId& unique_id) {
  StateMap::iterator iter = state_map_.find(unique_id);
  // Only erase if stopped.  Because this method is called asynchronously, it's
  // possible that a new request for this same id arrived after the UploadState
  // stopped, but before this method was invoked.  In that case the UploadState
  // in the map might be a new one.
  if (iter != state_map_.end() && iter->second->IsStopped()) {
    state_map_.erase(iter);
  }
}

}  // namespace syncer

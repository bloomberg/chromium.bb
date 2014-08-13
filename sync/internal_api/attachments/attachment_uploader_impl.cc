// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_uploader_impl.h"

#include "base/bind.h"
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
  // |owner| is a pointer to the object that will own (and must outlive!) this
  // |UploadState.
  UploadState(
      const GURL& upload_url,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      const Attachment& attachment,
      const UploadCallback& user_callback,
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenServiceRequest::TokenServiceProvider* token_service_provider,
      AttachmentUploaderImpl* owner);

  virtual ~UploadState();

  // Add |user_callback| to the list of callbacks to be invoked when this upload
  // completed.
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

  void ReportResult(const UploadResult& result,
                    const AttachmentId& attachment_id);

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
  AttachmentUploaderImpl* owner_;
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
    AttachmentUploaderImpl* owner)
    : OAuth2TokenService::Consumer("attachment-uploader-impl"),
      upload_url_(upload_url),
      url_request_context_getter_(url_request_context_getter),
      attachment_(attachment),
      user_callbacks_(1, user_callback),
      account_id_(account_id),
      scopes_(scopes),
      token_service_provider_(token_service_provider),
      owner_(owner) {
  DCHECK(upload_url_.is_valid());
  DCHECK(url_request_context_getter_);
  DCHECK(!account_id_.empty());
  DCHECK(!scopes_.empty());
  DCHECK(token_service_provider_);
  DCHECK(owner_);
  GetToken();
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
  UploadResult result = UPLOAD_UNSPECIFIED_ERROR;
  AttachmentId attachment_id = attachment_.GetId();
  if (source->GetResponseCode() == net::HTTP_OK) {
    result = UPLOAD_SUCCESS;
  } else if (source->GetResponseCode() == net::HTTP_UNAUTHORIZED) {
    // TODO(maniscalco): One possibility is that we received a 401 because our
    // access token has expired.  We should probably fetch a new access token
    // and retry this upload before giving up and reporting failure to our
    // caller (bug 380437).
    OAuth2TokenServiceRequest::InvalidateToken(
        token_service_provider_, account_id_, scopes_, access_token_);
  } else {
    // TODO(maniscalco): Once the protocol is better defined, deal with the
    // various HTTP response codes we may encounter.
  }
  ReportResult(result, attachment_id);
}

void AttachmentUploaderImpl::UploadState::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  access_token_ = access_token;
  fetcher_.reset(
      net::URLFetcher::Create(upload_url_, net::URLFetcher::POST, this));
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
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  ReportResult(UPLOAD_UNSPECIFIED_ERROR, attachment_.GetId());
}

void AttachmentUploaderImpl::UploadState::GetToken() {
  access_token_request_ = OAuth2TokenServiceRequest::CreateAndStart(
      token_service_provider_, account_id_, scopes_, this);
}

void AttachmentUploaderImpl::UploadState::ReportResult(
    const UploadResult& result,
    const AttachmentId& attachment_id) {
  UploadCallbackList::const_iterator iter = user_callbacks_.begin();
  UploadCallbackList::const_iterator end = user_callbacks_.end();
  for (; iter != end; ++iter) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(*iter, result, attachment_id));
  }
  // Destroy this object and return immediately.
  owner_->DeleteUploadStateFor(attachment_.GetId().GetProto().unique_id());
  return;
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
      token_service_provider_(token_service_provider) {
  DCHECK(CalledOnValidThread());
  DCHECK(!account_id.empty());
  DCHECK(!scopes.empty());
  DCHECK(token_service_provider_);
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
    const GURL url = GetURLForAttachmentId(sync_service_url_, attachment_id);
    scoped_ptr<UploadState> upload_state(
        new UploadState(url,
                        url_request_context_getter_,
                        attachment,
                        callback,
                        account_id_,
                        scopes_,
                        token_service_provider_.get(),
                        this));
    state_map_.add(unique_id, upload_state.Pass());
  } else {
    DCHECK(
        attachment.GetData()->Equals(iter->second->GetAttachment().GetData()));
    // We already have an upload for this attachment.  "Join" it.
    iter->second->AddUserCallback(callback);
  }
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

void AttachmentUploaderImpl::DeleteUploadStateFor(const UniqueId& unique_id) {
  state_map_.erase(unique_id);
}

}  // namespace syncer

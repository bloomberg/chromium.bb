// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/simple_dom_storage_system.h"

#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "webkit/database/database_util.h"
#include "webkit/dom_storage/dom_storage_context.h"
#include "webkit/dom_storage/dom_storage_host.h"
#include "webkit/dom_storage/dom_storage_session.h"

using dom_storage::DomStorageContext;
using dom_storage::DomStorageHost;
using dom_storage::DomStorageSession;
using webkit_database::DatabaseUtil;
using WebKit::WebStorageNamespace;
using WebKit::WebStorageArea;
using WebKit::WebString;
using WebKit::WebURL;

namespace {
const int kInvalidNamespaceId = -1;
}

class SimpleDomStorageSystem::NamespaceImpl : public WebStorageNamespace {
 public:
  explicit NamespaceImpl(const base::WeakPtr<SimpleDomStorageSystem>& parent);
  NamespaceImpl(const base::WeakPtr<SimpleDomStorageSystem>& parent,
                int session_namespace_id);
  virtual ~NamespaceImpl();
  virtual WebStorageArea* createStorageArea(const WebString& origin) OVERRIDE;
  virtual WebStorageNamespace* copy() OVERRIDE;
  virtual void close() OVERRIDE;

 private:
  DomStorageContext* Context() {
    if (!parent_.get())
      return NULL;
    return parent_->context_.get();
  }

  base::WeakPtr<SimpleDomStorageSystem> parent_;
  int namespace_id_;
};

class SimpleDomStorageSystem::AreaImpl : public WebStorageArea {
 public:
  AreaImpl(const base::WeakPtr<SimpleDomStorageSystem>& parent,
          int namespace_id, const GURL& origin);
  virtual ~AreaImpl();
  virtual unsigned length() OVERRIDE;
  virtual WebString key(unsigned index) OVERRIDE;
  virtual WebString getItem(const WebString& key) OVERRIDE;
  virtual void setItem(const WebString& key, const WebString& newValue,
                       const WebURL&, Result&, WebString& oldValue) OVERRIDE;
  virtual void removeItem(const WebString& key, const WebURL& url,
                          WebString& oldValue) OVERRIDE;
  virtual void clear(const WebURL& url, bool& somethingCleared) OVERRIDE;

 private:
  DomStorageHost* Host() {
    if (!parent_.get())
      return NULL;
    return parent_->host_.get();
  }

  base::WeakPtr<SimpleDomStorageSystem> parent_;
  int connection_id_;
};

// NamespaceImpl -----------------------------

SimpleDomStorageSystem::NamespaceImpl::NamespaceImpl(
    const base::WeakPtr<SimpleDomStorageSystem>& parent)
    : parent_(parent),
      namespace_id_(dom_storage::kLocalStorageNamespaceId) {
}

SimpleDomStorageSystem::NamespaceImpl::NamespaceImpl(
    const base::WeakPtr<SimpleDomStorageSystem>& parent,
    int session_namespace_id)
    : parent_(parent),
      namespace_id_(session_namespace_id) {
}

SimpleDomStorageSystem::NamespaceImpl::~NamespaceImpl() {
  if (namespace_id_ == dom_storage::kLocalStorageNamespaceId ||
      namespace_id_ == kInvalidNamespaceId || !Context()) {
    return;
  }
  Context()->DeleteSessionNamespace(namespace_id_);
}

WebStorageArea* SimpleDomStorageSystem::NamespaceImpl::createStorageArea(
    const WebString& origin) {
  return new AreaImpl(parent_, namespace_id_, GURL(origin));
}

WebStorageNamespace* SimpleDomStorageSystem::NamespaceImpl::copy() {
  DCHECK_NE(dom_storage::kLocalStorageNamespaceId, namespace_id_);
  int new_id = kInvalidNamespaceId;
  if (Context()) {
    new_id = Context()->AllocateSessionId();
    Context()->CloneSessionNamespace(namespace_id_, new_id);
  }
  return new NamespaceImpl(parent_, new_id);
}

void SimpleDomStorageSystem::NamespaceImpl::close() {
  // TODO(michaeln): remove this deprecated method.
}

// AreaImpl -----------------------------

SimpleDomStorageSystem::AreaImpl::AreaImpl(
    const base::WeakPtr<SimpleDomStorageSystem>& parent,
    int namespace_id, const GURL& origin)
    : parent_(parent),
      connection_id_(0) {
  if (Host())
    connection_id_ = Host()->OpenStorageArea(namespace_id, origin);
}

SimpleDomStorageSystem::AreaImpl::~AreaImpl() {
  if (Host())
    Host()->CloseStorageArea(connection_id_);
}

unsigned SimpleDomStorageSystem::AreaImpl::length() {
  if (Host())
    return Host()->GetAreaLength(connection_id_);
  return 0;
}

WebString SimpleDomStorageSystem::AreaImpl::key(unsigned index) {
  if (Host())
    return Host()->GetAreaKey(connection_id_, index);
  return NullableString16(true);
}

WebString SimpleDomStorageSystem::AreaImpl::getItem(const WebString& key) {
  if (Host())
    return Host()->GetAreaItem(connection_id_, key);
  return NullableString16(true);
}

void SimpleDomStorageSystem::AreaImpl::setItem(
    const WebString& key, const WebString& newValue,
    const WebURL& pageUrl, Result& result, WebString& oldValue) {
  result = ResultBlockedByQuota;
  oldValue = NullableString16(true);
  if (!Host())
    return;

  NullableString16 old_value;
  if (!Host()->SetAreaItem(connection_id_, key, newValue, pageUrl,
                           &old_value))
    return;

  result = ResultOK;
  oldValue = old_value;
}

void SimpleDomStorageSystem::AreaImpl::removeItem(
    const WebString& key, const WebURL& pageUrl, WebString& oldValue) {
  oldValue = NullableString16(true);
  if (!Host())
    return;

  string16 old_value;
  if (!Host()->RemoveAreaItem(connection_id_, key, pageUrl, &old_value))
    return;

  oldValue = old_value;
}

void SimpleDomStorageSystem::AreaImpl::clear(
    const WebURL& pageUrl, bool& somethingCleared) {
  if (Host())
    somethingCleared = Host()->ClearArea(connection_id_, pageUrl);
  else
    somethingCleared = false;
}

// SimpleDomStorageSystem -----------------------------

SimpleDomStorageSystem* SimpleDomStorageSystem::g_instance_;

SimpleDomStorageSystem::SimpleDomStorageSystem()
    : weak_factory_(this),
      context_(new DomStorageContext(FilePath(), FilePath(), NULL, NULL)),
      host_(new DomStorageHost(context_)) {
  DCHECK(!g_instance_);
  g_instance_ = this;
}

SimpleDomStorageSystem::~SimpleDomStorageSystem() {
  g_instance_ = NULL;
  host_.reset();
}

WebStorageNamespace* SimpleDomStorageSystem::CreateLocalStorageNamespace() {
  return new NamespaceImpl(weak_factory_.GetWeakPtr());
}

WebStorageNamespace* SimpleDomStorageSystem::CreateSessionStorageNamespace() {
  int id = context_->AllocateSessionId();
  context_->CreateSessionNamespace(id);
  return new NamespaceImpl(weak_factory_.GetWeakPtr(), id);
}

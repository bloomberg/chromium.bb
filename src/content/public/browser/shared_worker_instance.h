// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SHARED_WORKER_INSTANCE_H_
#define CONTENT_PUBLIC_BROWSER_SHARED_WORKER_INSTANCE_H_

#include <string>

#include "content/common/content_export.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "third_party/blink/public/mojom/csp/content_security_policy.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_creation_context_type.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// SharedWorkerInstance is a value-type class that is the browser-side
// representation of one instance of a shared worker.
class CONTENT_EXPORT SharedWorkerInstance {
 public:
  SharedWorkerInstance(
      const GURL& url,
      const std::string& name,
      const url::Origin& constructor_origin,
      const std::string& content_security_policy,
      blink::mojom::ContentSecurityPolicyType content_security_policy_type,
      network::mojom::IPAddressSpace creation_address_space,
      blink::mojom::SharedWorkerCreationContextType creation_context_type);
  SharedWorkerInstance(const SharedWorkerInstance& other);
  SharedWorkerInstance(SharedWorkerInstance&& other);
  SharedWorkerInstance& operator=(const SharedWorkerInstance& other);
  SharedWorkerInstance& operator=(SharedWorkerInstance&& other);
  ~SharedWorkerInstance();

  // Checks if this SharedWorkerInstance matches the passed url, name, and
  // constructor origin params according to the SharedWorker constructor steps
  // in the HTML spec:
  // https://html.spec.whatwg.org/multipage/workers.html#shared-workers-and-the-sharedworker-interface
  bool Matches(const GURL& url,
               const std::string& name,
               const url::Origin& constructor_origin) const;
  bool Matches(const SharedWorkerInstance& other) const;

  // Accessors.
  const GURL& url() const { return url_; }
  const std::string name() const { return name_; }
  const url::Origin& constructor_origin() const { return constructor_origin_; }
  const std::string content_security_policy() const {
    return content_security_policy_;
  }
  blink::mojom::ContentSecurityPolicyType content_security_policy_type() const {
    return content_security_policy_type_;
  }
  network::mojom::IPAddressSpace creation_address_space() const {
    return creation_address_space_;
  }
  blink::mojom::SharedWorkerCreationContextType creation_context_type() const {
    return creation_context_type_;
  }

 private:
  GURL url_;
  std::string name_;

  // The origin of the document that created this shared worker instance. Used
  // for security checks. See Matches() for details.
  // https://html.spec.whatwg.org/multipage/workers.html#concept-sharedworkerglobalscope-constructor-origin
  url::Origin constructor_origin_;

  std::string content_security_policy_;
  blink::mojom::ContentSecurityPolicyType content_security_policy_type_;
  network::mojom::IPAddressSpace creation_address_space_;
  blink::mojom::SharedWorkerCreationContextType creation_context_type_;
};

CONTENT_EXPORT bool operator<(const SharedWorkerInstance& lhs,
                              const SharedWorkerInstance& rhs);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SHARED_WORKER_INSTANCE_H_

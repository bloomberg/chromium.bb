// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/reporting_service_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/platform/reporting.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

class ReportingServiceProxyImpl : public blink::mojom::ReportingServiceProxy {
 public:
  explicit ReportingServiceProxyImpl(int render_process_id)
      : render_process_id_(render_process_id) {}

  // blink::mojom::ReportingServiceProxy:

  void QueueInterventionReport(const GURL& url,
                               const std::string& message,
                               const base::Optional<std::string>& source_file,
                               int line_number,
                               int column_number) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("message", message);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (line_number)
      body->SetInteger("lineNumber", line_number);
    if (column_number)
      body->SetInteger("columnNumber", column_number);
    QueueReport(url, "default", "intervention", std::move(body));
  }

  void QueueDeprecationReport(const GURL& url,
                              const std::string& id,
                              base::Optional<base::Time> anticipatedRemoval,
                              const std::string& message,
                              const base::Optional<std::string>& source_file,
                              int line_number,
                              int column_number) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("id", id);
    if (anticipatedRemoval)
      body->SetDouble("anticipatedRemoval", anticipatedRemoval->ToDoubleT());
    body->SetString("message", message);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (line_number)
      body->SetInteger("lineNumber", line_number);
    if (column_number)
      body->SetInteger("columnNumber", column_number);
    QueueReport(url, "default", "deprecation", std::move(body));
  }

  void QueueCspViolationReport(const GURL& url,
                               const std::string& group,
                               const std::string& document_uri,
                               const std::string& referrer,
                               const std::string& violated_directive,
                               const std::string& effective_directive,
                               const std::string& original_policy,
                               const std::string& disposition,
                               const std::string& blocked_uri,
                               int line_number,
                               int column_number,
                               const base::Optional<std::string>& source_file,
                               int status_code,
                               const std::string& script_sample) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("document-uri", document_uri);
    body->SetString("referrer", referrer);
    body->SetString("violated-directive", violated_directive);
    body->SetString("effective-directive", effective_directive);
    body->SetString("original-policy", original_policy);
    body->SetString("disposition", disposition);
    body->SetString("blocked-uri", blocked_uri);
    if (line_number)
      body->SetInteger("line-number", line_number);
    if (column_number)
      body->SetInteger("column-number", column_number);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (status_code)
      body->SetInteger("status-code", status_code);
    body->SetString("script-sample", script_sample);
    QueueReport(url, group, "csp", std::move(body));
  }

  void QueueFeaturePolicyViolationReport(
      const GURL& url,
      const std::string& policy_id,
      const std::string& disposition,
      const base::Optional<std::string>& message,
      const base::Optional<std::string>& source_file,
      int line_number,
      int column_number) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("policyId", policy_id);
    body->SetString("disposition", disposition);
    if (message)
      body->SetString("message", *message);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (line_number)
      body->SetInteger("lineNumber", line_number);
    if (column_number)
      body->SetInteger("columnNumber", column_number);
    QueueReport(url, "default", "feature-policy-violation", std::move(body));
  }

 private:
  void QueueReport(const GURL& url,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<base::Value> body) {
    auto* rph = RenderProcessHost::FromID(render_process_id_);
    if (!rph)
      return;

    rph->GetStoragePartition()->GetNetworkContext()->QueueReport(
        type, group, url, /*user_agent=*/base::nullopt,
        base::Value::FromUniquePtrValue(std::move(body)));
  }

  int render_process_id_;
};

}  // namespace

// static
void CreateReportingServiceProxy(
    int render_process_id,
    blink::mojom::ReportingServiceProxyRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojo::MakeStrongBinding(
      std::make_unique<ReportingServiceProxyImpl>(render_process_id),
      std::move(request));
}

}  // namespace content

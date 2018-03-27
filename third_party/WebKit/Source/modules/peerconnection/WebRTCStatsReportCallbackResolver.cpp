#include "modules/peerconnection/WebRTCStatsReportCallbackResolver.h"

#include "core/dom/ExecutionContext.h"

namespace blink {

// static
std::unique_ptr<WebRTCStatsReportCallback>
WebRTCStatsReportCallbackResolver::Create(ScriptPromiseResolver* resolver) {
  return std::unique_ptr<WebRTCStatsReportCallback>(
      new WebRTCStatsReportCallbackResolver(resolver));
}

WebRTCStatsReportCallbackResolver::WebRTCStatsReportCallbackResolver(
    ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

WebRTCStatsReportCallbackResolver::~WebRTCStatsReportCallbackResolver() {
  DCHECK(
      ExecutionContext::From(resolver_->GetScriptState())->IsContextThread());
}

void WebRTCStatsReportCallbackResolver::OnStatsDelivered(
    std::unique_ptr<WebRTCStatsReport> report) {
  DCHECK(
      ExecutionContext::From(resolver_->GetScriptState())->IsContextThread());
  resolver_->Resolve(new RTCStatsReport(std::move(report)));
}

}  // namespace blink

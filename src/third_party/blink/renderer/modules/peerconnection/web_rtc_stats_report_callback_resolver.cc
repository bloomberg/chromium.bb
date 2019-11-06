#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

void WebRTCStatsReportCallbackResolver(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<WebRTCStatsReport> report) {
  DCHECK(ExecutionContext::From(resolver->GetScriptState())->IsContextThread());
  resolver->Resolve(MakeGarbageCollected<RTCStatsReport>(std::move(report)));
}

}  // namespace blink

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/navigator_base.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#if !defined(OS_MAC) && !defined(OS_WIN)
#include <sys/utsname.h>
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#endif

namespace blink {

NavigatorBase::NavigatorBase(ExecutionContext* context)
    : NavigatorLanguage(context), ExecutionContextClient(context) {}

String NavigatorBase::userAgent() const {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context)
    return String();

  execution_context->ReportNavigatorUserAgentAccess();
  return execution_context->UserAgent();
}

String NavigatorBase::platform() const {
  ExecutionContext* execution_context = GetExecutionContext();
  // Report as user agent access
  if (execution_context)
    execution_context->ReportNavigatorUserAgentAccess();

  // If the User-Agent string is frozen, platform should be a value
  // matching the frozen string per https://github.com/WICG/ua-client-hints.
  // See content::frozen_user_agent_strings.
  if (base::FeatureList::IsEnabled(features::kReduceUserAgent) ||
      RuntimeEnabledFeatures::UserAgentReductionEnabled(execution_context)) {
#if defined(OS_ANDROID)
    return "Linux armv81";
#elif defined(OS_MAC)
    return "MacIntel";
#elif defined(OS_WIN)
    return "Win32";
#else
    return "Linux x86_64";
#endif
  }

  return NavigatorID::platform();
}

void NavigatorBase::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  NavigatorLanguage::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  Supplementable<NavigatorBase>::Trace(visitor);
}

ExecutionContext* NavigatorBase::GetUAExecutionContext() const {
  return GetExecutionContext();
}

UserAgentMetadata NavigatorBase::GetUserAgentMetadata() const {
  ExecutionContext* execution_context = GetExecutionContext();
  return execution_context ? execution_context->GetUserAgentMetadata()
                           : blink::UserAgentMetadata();
}

}  // namespace blink

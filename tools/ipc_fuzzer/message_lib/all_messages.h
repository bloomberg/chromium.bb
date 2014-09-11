// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.
// Inclusion of all message files recognized by message_lib. All messages
// received by RenderProcessHost should be included here for the IPC fuzzer.

// Force all multi-include optional files to be included again.
#undef CHROME_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#undef COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_
#undef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#undef CONTENT_COMMON_FRAME_PARAM_MACROS_H_
#undef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_

#include "chrome/common/all_messages.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/pdf/common/pdf_messages.h"
#include "components/tracing/tracing_messages.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/visitedlink/common/visitedlink_messages.h"
#include "content/child/plugin_message_generator.h"
#include "content/common/all_messages.h"
#include "extensions/common/extension_messages.h"
#include "remoting/host/chromoting_messages.h"

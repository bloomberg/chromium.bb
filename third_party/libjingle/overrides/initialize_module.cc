// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#include <math.h> // needed for _set_FMA3_enable
#endif  // WIN && ARCH_CPU_X86_64

#include "allocator_shim/allocator_stub.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "init_webrtc.h"
#include "talk/media/webrtc/webrtcmediaengine.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/logging.h"

#if !defined(LIBPEERCONNECTION_IMPLEMENTATION) || defined(LIBPEERCONNECTION_LIB)
#error "Only compile the allocator proxy with the shared_library implementation"
#endif

#if defined(OS_WIN)
#define ALLOC_EXPORT __declspec(dllexport)
#else
#define ALLOC_EXPORT __attribute__((visibility("default")))
#endif

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
// These are used by our new/delete overrides in
// allocator_shim/allocator_proxy.cc
AllocateFunction g_alloc = NULL;
DellocateFunction g_dealloc = NULL;
#endif

// Forward declare of the libjingle internal factory and destroy methods for the
// WebRTC media engine.
cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory);

void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine);

namespace {
// Provide webrtc with a field trial and metrics implementations.
// The implementations are provided by the loader via the InitializeModule.

// Defines webrtc::field_trial::FindFullName.
FieldTrialFindFullName g_field_trial_find_ = NULL;
// Defines webrtc::metrics::RtcFactoryGetCounts.
RtcHistogramFactoryGetCounts g_factory_get_counts = NULL;
// Defines webrtc::metrics::RtcFactoryGetEnumeration.
RtcHistogramFactoryGetEnumeration g_factory_get_enumeration = NULL;
// Defines webrtc::metrics::RtcAdd.
RtcHistogramAdd g_histogram_add = NULL;
}

namespace webrtc {
namespace field_trial {
std::string FindFullName(const std::string& trial_name) {
  return g_field_trial_find_(trial_name);
}
}  // namespace field_trial

namespace metrics {
Histogram* HistogramFactoryGetCounts(
    const std::string& name, int min, int max, int bucket_count) {
  return g_factory_get_counts(name, min, max, bucket_count);
}

Histogram* HistogramFactoryGetEnumeration(
    const std::string& name, int boundary) {
  return g_factory_get_enumeration(name, boundary);
}

void HistogramAdd(
    Histogram* histogram_pointer, const std::string& name, int sample) {
  g_histogram_add(histogram_pointer, name, sample);
}
}  // namespace metrics
}  // namespace webrtc

extern "C" {

// Initialize logging, set the forward allocator functions (not on mac), and
// return pointers to libjingle's WebRTC factory methods.
// Called from init_webrtc.cc.
ALLOC_EXPORT
bool InitializeModule(const base::CommandLine& command_line,
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
                      AllocateFunction alloc,
                      DellocateFunction dealloc,
#endif
                      FieldTrialFindFullName field_trial_find,
                      RtcHistogramFactoryGetCounts factory_get_counts,
                      RtcHistogramFactoryGetEnumeration factory_get_enumeration,
                      RtcHistogramAdd histogram_add,
                      logging::LogMessageHandlerFunction log_handler,
                      webrtc::GetCategoryEnabledPtr trace_get_category_enabled,
                      webrtc::AddTraceEventPtr trace_add_trace_event,
                      CreateWebRtcMediaEngineFunction* create_media_engine,
                      DestroyWebRtcMediaEngineFunction* destroy_media_engine,
                      InitDiagnosticLoggingDelegateFunctionFunction*
                          init_diagnostic_logging) {
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  g_alloc = alloc;
  g_dealloc = dealloc;
#endif

  g_field_trial_find_ = field_trial_find;
  g_factory_get_counts = factory_get_counts;
  g_factory_get_enumeration = factory_get_enumeration;
  g_histogram_add = histogram_add;

  *create_media_engine = &CreateWebRtcMediaEngine;
  *destroy_media_engine = &DestroyWebRtcMediaEngine;
  *init_diagnostic_logging = &rtc::InitDiagnosticLoggingDelegateFunction;

#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
  // VS2013 only checks the existence of FMA3 instructions, not the enabled-ness
  // of them at the OS level (this is fixed in VS2015). We force off usage of
  // FMA3 instructions in the CRT to avoid using that path and hitting illegal
  // instructions when running on CPUs that support FMA3, but OSs that don't.
  // See http://crbug.com/436603 and http://crbug.com/446983.
  _set_FMA3_enable(0);
#endif  // WIN && ARCH_CPU_X86_64

  if (base::CommandLine::Init(0, NULL)) {
#if !defined(OS_WIN)
    // This is not needed on Windows since CommandLine::Init has already
    // done the equivalent thing via the GetCommandLine() API.
    base::CommandLine::ForCurrentProcess()->AppendArguments(command_line, true);
#endif
    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    logging::InitLogging(settings);

    // Override the log message handler to forward logs to chrome's handler.
    logging::SetLogMessageHandler(log_handler);
    webrtc::SetupEventTracer(trace_get_category_enabled,
                             trace_add_trace_event);
  }

  return true;
}
}  // extern "C"

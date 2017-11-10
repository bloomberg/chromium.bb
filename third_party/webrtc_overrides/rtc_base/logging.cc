// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE:
// Since this file includes Chromium headers, it must not include
// third_party/webrtc/rtc_base/logging.h since it defines some of the same
// macros as Chromium does and we'll run into conflicts.

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <CoreServices/CoreServices.h>
#endif  // OS_MACOSX

#include <algorithm>
#include <iomanip>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "third_party/webrtc/rtc_base/stringencode.h"
#include "third_party/webrtc/rtc_base/stringutils.h"

// This needs to be included after base/logging.h.
#include "third_party/webrtc_overrides/rtc_base/diagnostic_logging.h"
#include "third_party/webrtc_overrides/rtc_base/logging.h"

#if defined(WEBRTC_MAC)
#include "base/mac/mac_logging.h"
#endif

// Disable logging when fuzzing, for performance reasons.
// WEBRTC_UNSAFE_FUZZER_MODE is defined by WebRTC's BUILD.gn when
// built with use_libfuzzer or use_drfuzz.
#if defined(WEBRTC_UNSAFE_FUZZER_MODE)
#define WEBRTC_ENABLE_LOGGING false
#else
#define WEBRTC_ENABLE_LOGGING true
#endif

// From this file we can't use VLOG since it expands into usage of the __FILE__
// macro (for correct filtering). The actual logging call from DIAGNOSTIC_LOG in
// ~DiagnosticLogMessage. Note that the second parameter to the LAZY_STREAM
// macro is not used since the filter check has already been done for
// DIAGNOSTIC_LOG.
#define LOG_LAZY_STREAM_DIRECT(file_name, line_number, sev)              \
  LAZY_STREAM(logging::LogMessage(file_name, line_number, sev).stream(), \
              WEBRTC_ENABLE_LOGGING)

namespace rtc {

void (*g_logging_delegate_function)(const std::string&) = NULL;
void (*g_extra_logging_init_function)(
    void (*logging_delegate_function)(const std::string&)) = NULL;
#ifndef NDEBUG
static_assert(sizeof(base::subtle::Atomic32) == sizeof(base::PlatformThreadId),
              "Atomic32 not same size as PlatformThreadId");
base::subtle::Atomic32 g_init_logging_delegate_thread_id = 0;
#endif

/////////////////////////////////////////////////////////////////////////////
// Constant Labels
/////////////////////////////////////////////////////////////////////////////

const char* FindLabel(int value, const ConstantLabel entries[]) {
  for (int i = 0; entries[i].label; ++i) {
    if (value == entries[i].value)
      return entries[i].label;
  }
  return 0;
}

std::string ErrorName(int err, const ConstantLabel* err_table) {
  if (err == 0)
    return "No error";

  if (err_table != 0) {
    if (const char* value = FindLabel(err, err_table))
      return value;
  }

  char buffer[16];
  base::snprintf(buffer, sizeof(buffer), "0x%08x", err);
  return buffer;
}

/////////////////////////////////////////////////////////////////////////////
// Log helper functions
/////////////////////////////////////////////////////////////////////////////

inline int WebRtcSevToChromeSev(LoggingSeverity sev) {
  switch (sev) {
    case LS_ERROR:
      return ::logging::LOG_ERROR;
    case LS_WARNING:
      return ::logging::LOG_WARNING;
    case LS_INFO:
      return ::logging::LOG_INFO;
    case LS_VERBOSE:
    case LS_SENSITIVE:
      return ::logging::LOG_VERBOSE;
    default:
      NOTREACHED();
      return ::logging::LOG_FATAL;
  }
}

inline int WebRtcVerbosityLevel(LoggingSeverity sev) {
  switch (sev) {
    case LS_ERROR:
      return -2;
    case LS_WARNING:
      return -1;
    case LS_INFO:  // We treat 'info' and 'verbose' as the same verbosity level.
    case LS_VERBOSE:
      return 1;
    case LS_SENSITIVE:
      return 2;
    default:
      NOTREACHED();
      return 0;
  }
}

// Logs extra information for LOG_E.
static void LogExtra(std::ostringstream* print_stream,
                     LogErrorContext err_ctx,
                     int err,
                     const char* module) {
  if (err_ctx == ERRCTX_NONE)
    return;

  (*print_stream) << ": ";
  (*print_stream) << "[0x" << std::setfill('0') << std::hex << std::setw(8)
                  << err << "]";
  switch (err_ctx) {
    case ERRCTX_ERRNO:
      (*print_stream) << " " << strerror(err);
      break;
#if defined(WEBRTC_WIN)
    case ERRCTX_HRESULT: {
      char msgbuf[256];
      DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM;
      HMODULE hmod = GetModuleHandleA(module);
      if (hmod)
        flags |= FORMAT_MESSAGE_FROM_HMODULE;
      if (DWORD len = FormatMessageA(
              flags, hmod, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
              msgbuf, sizeof(msgbuf) / sizeof(msgbuf[0]), NULL)) {
        while ((len > 0) &&
               isspace(static_cast<unsigned char>(msgbuf[len - 1]))) {
          msgbuf[--len] = 0;
        }
        (*print_stream) << " " << msgbuf;
      }
      break;
    }
#elif defined(WEBRTC_IOS)
    case ERRCTX_OSSTATUS:
      (*print_stream) << " "
                      << "Unknown LibJingle error: " << err;
      break;
#elif defined(WEBRTC_MAC)
    case ERRCTX_OSSTATUS: {
      (*print_stream) << " " << logging::DescriptionFromOSStatus(err);
      break;
    }
#endif  // defined(WEBRTC_WIN)
    default:
      break;
  }
}

DiagnosticLogMessage::DiagnosticLogMessage(const char* file,
                                           int line,
                                           LoggingSeverity severity,
                                           LogErrorContext err_ctx,
                                           int err)
    : DiagnosticLogMessage(file, line, severity, err_ctx, err, nullptr) {}

DiagnosticLogMessage::DiagnosticLogMessage(const char* file,
                                           int line,
                                           LoggingSeverity severity,
                                           LogErrorContext err_ctx,
                                           int err,
                                           const char* module)
    : file_name_(file),
      line_(line),
      severity_(severity),
      err_ctx_(err_ctx),
      err_(err),
      module_(module),
      log_to_chrome_(CheckVlogIsOnHelper(severity, file, strlen(file) + 1)) {}

DiagnosticLogMessage::~DiagnosticLogMessage() {
  const bool call_delegate =
      g_logging_delegate_function && severity_ <= LS_INFO;

  if (call_delegate || log_to_chrome_) {
    LogExtra(&print_stream_, err_ctx_, err_, module_);
    const std::string& str = print_stream_.str();
    if (log_to_chrome_) {
      LOG_LAZY_STREAM_DIRECT(file_name_, line_,
                             rtc::WebRtcSevToChromeSev(severity_))
          << str;
    }

    if (g_logging_delegate_function && severity_ <= LS_INFO) {
      g_logging_delegate_function(str);
    }
  }
}

// static
void LogMessage::LogToDebug(int min_sev) {
  logging::SetMinLogLevel(min_sev);
}

// Note: this function is a copy from the overriden libjingle implementation.
void LogMultiline(LoggingSeverity level,
                  const char* label,
                  bool input,
                  const void* data,
                  size_t len,
                  bool hex_mode,
                  LogMultilineState* state) {
  // TODO(grunell): This will not do the expected verbosity level checking. We
  // need a macro for the multiline logging.
  // https://code.google.com/p/webrtc/issues/detail?id=5011
  if (!RTC_LOG_CHECK_LEVEL_V(level))
    return;

  const char* direction = (input ? " << " : " >> ");

  // NULL data means to flush our count of unprintable characters.
  if (!data) {
    if (state && state->unprintable_count_[input]) {
      RTC_LOG_V(level) << label << direction << "## "
                       << state->unprintable_count_[input]
                       << " consecutive unprintable ##";
      state->unprintable_count_[input] = 0;
    }
    return;
  }

  // The ctype classification functions want unsigned chars.
  const unsigned char* udata = static_cast<const unsigned char*>(data);

  if (hex_mode) {
    const size_t LINE_SIZE = 24;
    char hex_line[LINE_SIZE * 9 / 4 + 2], asc_line[LINE_SIZE + 1];
    while (len > 0) {
      memset(asc_line, ' ', sizeof(asc_line));
      memset(hex_line, ' ', sizeof(hex_line));
      size_t line_len = std::min(len, LINE_SIZE);
      for (size_t i = 0; i < line_len; ++i) {
        unsigned char ch = udata[i];
        asc_line[i] = isprint(ch) ? ch : '.';
        hex_line[i * 2 + i / 4] = hex_encode(ch >> 4);
        hex_line[i * 2 + i / 4 + 1] = hex_encode(ch & 0xf);
      }
      asc_line[sizeof(asc_line) - 1] = 0;
      hex_line[sizeof(hex_line) - 1] = 0;
      RTC_LOG_V(level) << label << direction << asc_line << " " << hex_line
                       << " ";
      udata += line_len;
      len -= line_len;
    }
    return;
  }

  size_t consecutive_unprintable = state ? state->unprintable_count_[input] : 0;

  const unsigned char* end = udata + len;
  while (udata < end) {
    const unsigned char* line = udata;
    const unsigned char* end_of_line =
        strchrn<unsigned char>(udata, end - udata, '\n');
    if (!end_of_line) {
      udata = end_of_line = end;
    } else {
      udata = end_of_line + 1;
    }

    bool is_printable = true;

    // If we are in unprintable mode, we need to see a line of at least
    // kMinPrintableLine characters before we'll switch back.
    const ptrdiff_t kMinPrintableLine = 4;
    if (consecutive_unprintable && ((end_of_line - line) < kMinPrintableLine)) {
      is_printable = false;
    } else {
      // Determine if the line contains only whitespace and printable
      // characters.
      bool is_entirely_whitespace = true;
      for (const unsigned char* pos = line; pos < end_of_line; ++pos) {
        if (isspace(*pos))
          continue;
        is_entirely_whitespace = false;
        if (!isprint(*pos)) {
          is_printable = false;
          break;
        }
      }
      // Treat an empty line following unprintable data as unprintable.
      if (consecutive_unprintable && is_entirely_whitespace) {
        is_printable = false;
      }
    }
    if (!is_printable) {
      consecutive_unprintable += (udata - line);
      continue;
    }
    // Print out the current line, but prefix with a count of prior unprintable
    // characters.
    if (consecutive_unprintable) {
      RTC_LOG_V(level) << label << direction << "## " << consecutive_unprintable
                       << " consecutive unprintable ##";
      consecutive_unprintable = 0;
    }
    // Strip off trailing whitespace.
    while ((end_of_line > line) && isspace(*(end_of_line - 1))) {
      --end_of_line;
    }
    // Filter out any private data
    std::string substr(reinterpret_cast<const char*>(line), end_of_line - line);
    std::string::size_type pos_private = substr.find("Email");
    if (pos_private == std::string::npos) {
      pos_private = substr.find("Passwd");
    }
    if (pos_private == std::string::npos) {
      RTC_LOG_V(level) << label << direction << substr;
    } else {
      RTC_LOG_V(level) << label << direction << "## omitted for privacy ##";
    }
  }

  if (state) {
    state->unprintable_count_[input] = consecutive_unprintable;
  }
}

void InitDiagnosticLoggingDelegateFunction(
    void (*delegate)(const std::string&)) {
#ifndef NDEBUG
  // Ensure that this function is always called from the same thread.
  base::subtle::NoBarrier_CompareAndSwap(
      &g_init_logging_delegate_thread_id, 0,
      static_cast<base::subtle::Atomic32>(base::PlatformThread::CurrentId()));
  DCHECK_EQ(
      g_init_logging_delegate_thread_id,
      static_cast<base::subtle::Atomic32>(base::PlatformThread::CurrentId()));
#endif
  CHECK(delegate);
  // This function may be called with the same argument several times if the
  // page is reloaded or there are several PeerConnections on one page with
  // logging enabled. This is OK, we simply don't have to do anything.
  if (delegate == g_logging_delegate_function)
    return;
  CHECK(!g_logging_delegate_function);
  g_logging_delegate_function = delegate;

  if (g_extra_logging_init_function)
    g_extra_logging_init_function(delegate);
}

void SetExtraLoggingInit(
    void (*function)(void (*delegate)(const std::string&))) {
  CHECK(function);
  CHECK(!g_extra_logging_init_function);
  g_extra_logging_init_function = function;
}

bool CheckVlogIsOnHelper(rtc::LoggingSeverity severity,
                         const char* file,
                         size_t N) {
  return rtc::WebRtcVerbosityLevel(severity) <=
         ::logging::GetVlogLevelHelper(file, N);
}

}  // namespace rtc

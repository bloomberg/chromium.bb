/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "gpu_error_message_observer.h"

#include "base/no_destructor.h"

#include <vector>
namespace {
static const std::vector<std::string>& GetFatalCandidates() {
  static base::NoDestructor<std::vector<std::string>> g_candidates;
  if (g_candidates->empty()) {
    // Error message to be synced with that in
    // third_party\angle\src\libANGLE\renderer\d3d\HLSLCompiler.cpp
    const std::string d3dLoadFailureMsg{
        "D3D compiler module failed in dll loading"};
    g_candidates->emplace_back(d3dLoadFailureMsg);

    // Add other fatal candidates if needed
  }
  return *(g_candidates.get());
}
}  // namespace

namespace content {
GpuErrorMessageObserver::GpuErrorMessageObserver(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

GpuErrorMessageObserver::~GpuErrorMessageObserver() = default;

void GpuErrorMessageObserver::OnAddLogMessage(int level,
                                              const std::string& header,
                                              const std::string& message) {
  if (level != logging::LOG_ERROR && level != logging::LOG_FATAL) {
    return;
  }
  for (const auto& candidate : GetFatalCandidates()) {
    if (message.find(candidate) != std::string::npos) {
      delegate_->OnFatalErrorDetected(header, message);
    }
  }

  return;
}

}  // namespace content

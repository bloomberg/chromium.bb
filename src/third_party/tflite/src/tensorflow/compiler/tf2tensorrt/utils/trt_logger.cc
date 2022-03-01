/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/tf2tensorrt/utils/trt_logger.h"

#if GOOGLE_CUDA && GOOGLE_TENSORRT
#include "tensorflow/compiler/tf2tensorrt/common/utils.h"
#include "tensorflow/compiler/tf2tensorrt/convert/logger_registry.h"
#include "tensorflow/core/platform/logging.h"

namespace tensorflow {
namespace tensorrt {

// Use TF logging for TensorRT informations
void Logger::log(Severity severity, const char* msg) noexcept {
  // Suppress info-level messages
  switch (severity) {
    case Severity::kVERBOSE:
    case Severity::kINFO: {  // Mark TRT info messages as debug!
      VLOG(2) << name_ << " " << msg;
      break;
    }
    case Severity::kWARNING: {
      LOG_WARNING_WITH_PREFIX << name_ << " " << msg;
      break;
    }
    case Severity::kERROR: {
      LOG(ERROR) << name_ << " " << msg;
      break;
    }
    case Severity::kINTERNAL_ERROR: {
      LOG(FATAL) << name_ << " " << msg;
      break;
    }
    // This is useless for now. But would catch it in future if enum changes. It
    // is always good to have default case!
    default: {
      LOG(FATAL) << name_ << "Got unknown severity level " << int(severity)
                 << " from TensorRT: " << msg;
      break;
    }
  }
}

// static
Logger* Logger::GetLogger() {
  static Logger* logger = new Logger("DefaultLogger");
  return logger;
}

REGISTER_TENSORRT_LOGGER("DefaultLogger", Logger::GetLogger());

}  // namespace tensorrt
}  // namespace tensorflow

#endif  // GOOGLE_CUDA && GOOGLE_TENSORRT

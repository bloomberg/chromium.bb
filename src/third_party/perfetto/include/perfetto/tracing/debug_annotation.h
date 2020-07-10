/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_TRACING_DEBUG_ANNOTATION_H_
#define INCLUDE_PERFETTO_TRACING_DEBUG_ANNOTATION_H_

#include "perfetto/base/export.h"

#include <stdint.h>

#include <string>

namespace perfetto {
namespace protos {
namespace pbzero {
class DebugAnnotation;
}  // namespace pbzero
}  // namespace protos

// A base class for custom track event debug annotations.
class PERFETTO_EXPORT DebugAnnotation {
 public:
  DebugAnnotation() = default;
  virtual ~DebugAnnotation();

  // Called to write the contents of the debug annotation into the trace.
  virtual void Add(protos::pbzero::DebugAnnotation*) const = 0;
};

namespace internal {

// Overloads for all the supported built in debug annotation types.
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, bool);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, uint64_t);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, unsigned);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, int64_t);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, int);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, double);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, float);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, const char*);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, const std::string&);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*, const void*);
void WriteDebugAnnotation(protos::pbzero::DebugAnnotation*,
                          const DebugAnnotation&);

}  // namespace internal
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_DEBUG_ANNOTATION_H_

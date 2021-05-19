// Copyright 2021 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DAWNNATIVE_COMPILATIONMESSAGES_H_
#define DAWNNATIVE_COMPILATIONMESSAGES_H_

#include "dawn_native/dawn_platform.h"

#include "common/NonCopyable.h"

#include <string>
#include <vector>

namespace tint { namespace diag {
    class Diagnostic;
    class List;
}}  // namespace tint::diag

namespace dawn_native {

    class OwnedCompilationMessages : public NonCopyable {
      public:
        OwnedCompilationMessages();
        ~OwnedCompilationMessages() = default;

        void AddMessage(std::string message,
                        wgpu::CompilationMessageType type = wgpu::CompilationMessageType::Info,
                        uint64_t lineNum = 0,
                        uint64_t linePos = 0);
        void AddMessage(const tint::diag::Diagnostic& diagnostic);
        void AddMessages(const tint::diag::List& diagnostics);
        void ClearMessages();

        const WGPUCompilationInfo* GetCompilationInfo();

      private:
        WGPUCompilationInfo mCompilationInfo;
        std::vector<std::string> mMessageStrings;
        std::vector<WGPUCompilationMessage> mMessages;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_COMPILATIONMESSAGES_H_
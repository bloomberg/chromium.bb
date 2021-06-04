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

#include "dawn_native/CompilationMessages.h"

#include "common/Assert.h"
#include "dawn_native/dawn_platform.h"

#include <tint/tint.h>

namespace dawn_native {

    namespace {

        WGPUCompilationMessageType tintSeverityToMessageType(tint::diag::Severity severity) {
            switch (severity) {
                case tint::diag::Severity::Note:
                    return WGPUCompilationMessageType_Info;
                case tint::diag::Severity::Warning:
                    return WGPUCompilationMessageType_Warning;
                default:
                    return WGPUCompilationMessageType_Error;
            }
        }

    }  // anonymous namespace

    OwnedCompilationMessages::OwnedCompilationMessages() {
        mCompilationInfo.messageCount = 0;
        mCompilationInfo.messages = nullptr;
    }

    void OwnedCompilationMessages::AddMessage(std::string message,
                                              wgpu::CompilationMessageType type,
                                              uint64_t lineNum,
                                              uint64_t linePos,
                                              uint64_t offset,
                                              uint64_t length) {
        // Cannot add messages after GetCompilationInfo has been called.
        ASSERT(mCompilationInfo.messages == nullptr);

        mMessageStrings.push_back(message);
        mMessages.push_back({nullptr, static_cast<WGPUCompilationMessageType>(type), lineNum,
                             linePos, offset, length});
    }

    void OwnedCompilationMessages::AddMessage(const tint::diag::Diagnostic& diagnostic) {
        // Cannot add messages after GetCompilationInfo has been called.
        ASSERT(mCompilationInfo.messages == nullptr);

        // Tint line and column values are 1-based.
        uint64_t lineNum = diagnostic.source.range.begin.line;
        uint64_t linePos = diagnostic.source.range.begin.column;
        // The offset is 0-based.
        uint64_t offset = 0;
        uint64_t length = 0;

        if (lineNum && linePos && diagnostic.source.file_content) {
            const std::vector<std::string>& lines = diagnostic.source.file_content->lines;
            size_t i = 0;
            // To find the offset of the message position, loop through each of the first lineNum-1
            // lines and add it's length (+1 to account for the line break) to the offset.
            for (; i < lineNum - 1; ++i) {
                offset += lines[i].length() + 1;
            }

            // If the end line is on a different line from the beginning line, add the length of the
            // lines in between to the ending offset.
            uint64_t endLineNum = diagnostic.source.range.end.line;
            uint64_t endLinePos = diagnostic.source.range.end.column;
            uint64_t endOffset = offset;
            for (; i < endLineNum - 1; ++i) {
                endOffset += lines[i].length() + 1;
            }

            // Add the line positions to the offset and endOffset to get their final positions
            // within the code string.
            offset += linePos - 1;
            endOffset += endLinePos - 1;

            // The length of the message is the difference between the starting offset and the
            // ending offset.
            length = endOffset - offset;
        }

        if (diagnostic.code) {
            mMessageStrings.push_back(std::string(diagnostic.code) + ": " + diagnostic.message);
        } else {
            mMessageStrings.push_back(diagnostic.message);
        }

        mMessages.push_back({nullptr, tintSeverityToMessageType(diagnostic.severity), lineNum,
                             linePos, offset, length});
    }

    void OwnedCompilationMessages::AddMessages(const tint::diag::List& diagnostics) {
        // Cannot add messages after GetCompilationInfo has been called.
        ASSERT(mCompilationInfo.messages == nullptr);

        for (const auto& diag : diagnostics) {
            AddMessage(diag);
        }
    }

    void OwnedCompilationMessages::ClearMessages() {
        // Cannot clear messages after GetCompilationInfo has been called.
        ASSERT(mCompilationInfo.messages == nullptr);

        mMessageStrings.clear();
        mMessages.clear();
    }

    const WGPUCompilationInfo* OwnedCompilationMessages::GetCompilationInfo() {
        mCompilationInfo.messageCount = mMessages.size();
        mCompilationInfo.messages = mMessages.data();

        // Ensure every message points at the correct message string. Cannot do this earlier, since
        // vector reallocations may move the pointers around.
        for (size_t i = 0; i < mCompilationInfo.messageCount; ++i) {
            WGPUCompilationMessage& message = mMessages[i];
            std::string& messageString = mMessageStrings[i];
            message.message = messageString.c_str();
        }

        return &mCompilationInfo;
    }

}  // namespace dawn_native

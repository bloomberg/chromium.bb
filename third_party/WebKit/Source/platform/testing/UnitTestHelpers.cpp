/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/testing/UnitTestHelpers.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "platform/SharedBuffer.h"
#include "platform/WebTaskRunner.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {
namespace testing {

namespace {

base::FilePath BlinkRootFilePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return base::MakeAbsoluteFilePath(
      path.Append(FILE_PATH_LITERAL("third_party/WebKit")));
}

}  // namespace

void RunPendingTasks() {
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&ExitRunLoop));

  // We forbid GC in the tasks. Otherwise the registered GCTaskObserver tries
  // to run GC with NoHeapPointerOnStack.
  ThreadState::Current()->EnterGCForbiddenScope();
  EnterRunLoop();
  ThreadState::Current()->LeaveGCForbiddenScope();
}

void RunDelayedTasks(TimeDelta delay) {
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE, WTF::Bind(&ExitRunLoop), delay);
  EnterRunLoop();
}

void EnterRunLoop() {
  base::RunLoop().Run();
}

void ExitRunLoop() {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void YieldCurrentThread() {
  base::PlatformThread::YieldCurrentThread();
}

String BlinkRootDir() {
  return FilePathToWebString(BlinkRootFilePath());
}

String ExecutableDir() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return FilePathToWebString(base::MakeAbsoluteFilePath(path));
}

String CoreTestDataPath(const String& relative_path) {
  return FilePathToWebString(
      BlinkRootFilePath()
          .Append(FILE_PATH_LITERAL("Source/core/testing/data"))
          .Append(WebStringToFilePath(relative_path)));
}

String PlatformTestDataPath(const String& relative_path) {
  return FilePathToWebString(
      BlinkRootFilePath()
          .Append(FILE_PATH_LITERAL("Source/platform/testing/data"))
          .Append(WebStringToFilePath(relative_path)));
}

PassRefPtr<SharedBuffer> ReadFromFile(const String& path) {
  base::FilePath file_path = blink::WebStringToFilePath(path);
  std::string buffer;
  base::ReadFileToString(file_path, &buffer);
  return SharedBuffer::Create(buffer.data(), buffer.size());
}

}  // namespace testing
}  // namespace blink

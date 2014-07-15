// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_native_library.h"
#include "remoting/base/auto_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <objbase.h>
#include "base/win/windows_version.h"
#endif

namespace {

const char kThreadName[] = "Test thread";

void SetFlagTask(bool* success) {
  *success = true;
}

void PostSetFlagTask(
    scoped_refptr<base::TaskRunner> task_runner,
    bool* success) {
  task_runner->PostTask(FROM_HERE, base::Bind(&SetFlagTask, success));
}

#if defined(OS_WIN)
void CheckComAptTypeTask(APTTYPE* apt_type_out, HRESULT* hresult) {
  typedef HRESULT (WINAPI * CoGetApartmentTypeFunc)
      (APTTYPE*, APTTYPEQUALIFIER*);

  // CoGetApartmentType requires Windows 7 or above.
  if (base::win::GetVersion() < base::win::VERSION_WIN7) {
    *hresult = E_NOTIMPL;
    return;
  }

  // Dynamic link to the API so the same test binary can run on older systems.
  base::ScopedNativeLibrary com_library(base::FilePath(L"ole32.dll"));
  ASSERT_TRUE(com_library.is_valid());
  CoGetApartmentTypeFunc co_get_apartment_type =
      reinterpret_cast<CoGetApartmentTypeFunc>(
          com_library.GetFunctionPointer("CoGetApartmentType"));
  ASSERT_TRUE(co_get_apartment_type != NULL);

  APTTYPEQUALIFIER apt_type_qualifier;
  *hresult = (*co_get_apartment_type)(apt_type_out, &apt_type_qualifier);
}
#endif

}  // namespace

namespace remoting {

class AutoThreadTest : public testing::Test {
 public:
  AutoThreadTest() : message_loop_quit_correctly_(false) {
  }

  void RunMessageLoop() {
    // Release |main_task_runner_|, then run |message_loop_| until other
    // references created in tests are gone.  We also post a delayed quit
    // task to |message_loop_| so the test will not hang on failure.
    main_task_runner_ = NULL;
    message_loop_.PostDelayedTask(FROM_HERE,
                                  base::MessageLoop::QuitClosure(),
                                  base::TimeDelta::FromSeconds(5));
    message_loop_.Run();
  }

  virtual void SetUp() OVERRIDE {
    main_task_runner_ = new AutoThreadTaskRunner(
        message_loop_.message_loop_proxy(),
        base::Bind(&AutoThreadTest::QuitMainMessageLoop,
                   base::Unretained(this)));
  }

  virtual void TearDown() OVERRIDE {
    // Verify that |message_loop_| was quit by the AutoThreadTaskRunner.
    EXPECT_TRUE(message_loop_quit_correctly_);
  }

 protected:
  void QuitMainMessageLoop() {
    message_loop_quit_correctly_ = true;
    message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }

  base::MessageLoop message_loop_;
  bool message_loop_quit_correctly_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
};

TEST_F(AutoThreadTest, StartAndStop) {
  // Create an AutoThread joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner.get());

  task_runner = NULL;
  RunMessageLoop();
}

TEST_F(AutoThreadTest, ProcessTask) {
  // Create an AutoThread joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner.get());

  // Post a task to it.
  bool success = false;
  task_runner->PostTask(FROM_HERE, base::Bind(&SetFlagTask, &success));

  task_runner = NULL;
  RunMessageLoop();

  EXPECT_TRUE(success);
}

TEST_F(AutoThreadTest, ThreadDependency) {
  // Create two AutoThreads joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner1 =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner1.get());
  scoped_refptr<base::TaskRunner> task_runner2 =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner2.get());

  // Post a task to thread 1 that will post a task to thread 2.
  bool success = false;
  task_runner1->PostTask(FROM_HERE,
      base::Bind(&PostSetFlagTask, task_runner2, &success));

  task_runner1 = NULL;
  task_runner2 = NULL;
  RunMessageLoop();

  EXPECT_TRUE(success);
}

#if defined(OS_WIN)
TEST_F(AutoThreadTest, ThreadWithComMta) {
  scoped_refptr<base::TaskRunner> task_runner =
      AutoThread::CreateWithLoopAndComInitTypes(kThreadName,
                                                main_task_runner_,
                                                base::MessageLoop::TYPE_DEFAULT,
                                                AutoThread::COM_INIT_MTA);
  EXPECT_TRUE(task_runner.get());

  // Post a task to query the COM apartment type.
  HRESULT hresult = E_FAIL;
  APTTYPE apt_type = APTTYPE_NA;
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&CheckComAptTypeTask, &apt_type, &hresult));

  task_runner = NULL;
  RunMessageLoop();

  // CoGetApartmentType requires Windows 7 or above.
  if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
    EXPECT_EQ(S_OK, hresult);
    EXPECT_EQ(APTTYPE_MTA, apt_type);
  } else {
    EXPECT_EQ(E_NOTIMPL, hresult);
  }
}

TEST_F(AutoThreadTest, ThreadWithComSta) {
  scoped_refptr<base::TaskRunner> task_runner =
      AutoThread::CreateWithLoopAndComInitTypes(kThreadName,
                                                main_task_runner_,
                                                base::MessageLoop::TYPE_UI,
                                                AutoThread::COM_INIT_STA);
  EXPECT_TRUE(task_runner.get());

  // Post a task to query the COM apartment type.
  HRESULT hresult = E_FAIL;
  APTTYPE apt_type = APTTYPE_NA;
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&CheckComAptTypeTask, &apt_type, &hresult));

  task_runner = NULL;
  RunMessageLoop();

  // CoGetApartmentType requires Windows 7 or above.
  if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
    EXPECT_EQ(S_OK, hresult);
    // Whether the thread is the "main" STA apartment depends upon previous
    // COM activity in this test process, so allow both types here.
    EXPECT_TRUE(apt_type == APTTYPE_MAINSTA || apt_type == APTTYPE_STA);
  } else {
    EXPECT_EQ(E_NOTIMPL, hresult);
  }
}
#endif // defined(OS_WIN)

}  // namespace remoting

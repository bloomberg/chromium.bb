// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/desktop_environment.h"

namespace remoting {

// Used to create audio/video capturers and event executor that work with
// the local console.
class BasicDesktopEnvironment
    : public base::NonThreadSafe,
      public DesktopEnvironment {
 public:
  BasicDesktopEnvironment();
  virtual ~BasicDesktopEnvironment();

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) OVERRIDE;
  virtual scoped_ptr<EventExecutor> CreateEventExecutor(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) OVERRIDE;
  virtual scoped_ptr<VideoFrameCapturer> CreateVideoCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironment);
};

// Used to create |BasicDesktopEnvironment| instances.
class BasicDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  BasicDesktopEnvironmentFactory();
  virtual ~BasicDesktopEnvironmentFactory();

  // DesktopEnvironmentFactory implementation.
  virtual scoped_ptr<DesktopEnvironment> Create(
      const std::string& client_jid,
      const base::Closure& disconnect_callback) OVERRIDE;
  virtual bool SupportsAudioCapture() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_

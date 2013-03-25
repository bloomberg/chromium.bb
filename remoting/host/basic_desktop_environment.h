// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/desktop_environment.h"

namespace remoting {

// Used to create audio/video capturers and event executor that work with
// the local console.
class BasicDesktopEnvironment : public DesktopEnvironment {
 public:
  virtual ~BasicDesktopEnvironment();

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer() OVERRIDE;
  virtual scoped_ptr<InputInjector> CreateInputInjector() OVERRIDE;
  virtual scoped_ptr<ScreenControls> CreateScreenControls() OVERRIDE;
  virtual scoped_ptr<media::ScreenCapturer> CreateVideoCapturer() OVERRIDE;

 protected:
  friend class BasicDesktopEnvironmentFactory;
  BasicDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner() const {
    return input_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() const {
    return ui_task_runner_;
  }

 private:
  // Task runner on which methods of DesktopEnvironment interface should be
  // called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to run input-related tasks.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to run UI code.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironment);
};

// Used to create |BasicDesktopEnvironment| instances.
class BasicDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  BasicDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual ~BasicDesktopEnvironmentFactory();

  // DesktopEnvironmentFactory implementation.
  virtual scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) OVERRIDE;
  virtual bool SupportsAudioCapture() const OVERRIDE;

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner() const {
    return input_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() const {
    return ui_task_runner_;
  }

 private:
  // Task runner on which methods of DesktopEnvironmentFactory interface should
  // be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to run input-related tasks.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to run UI code.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_

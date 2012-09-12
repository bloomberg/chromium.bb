// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_FACTORY_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class ChromotingHostContext;
class DesktopEnvironment;

class DesktopEnvironmentFactory {
 public:
  DesktopEnvironmentFactory();
  virtual ~DesktopEnvironmentFactory();

  virtual scoped_ptr<DesktopEnvironment> Create(
      ChromotingHostContext* context);

  // Returns |true| if created |DesktopEnvironment| instances support audio
  // capture.
  virtual bool SupportsAudioCapture() const;

 protected:
  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_FACTORY_H_

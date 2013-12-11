# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The android specific platform implementation module."""

import os
import subprocess

import cr

# This is the set of environment variables that are not automatically
# copied back from the envsetup shell
_IGNORE_ENV = [
    'SHLVL',  # Because it's nothing to do with envsetup
    'GYP_GENERATOR_FLAGS',  # because we set them in they gyp handler
    'GYP_GENERATORS',  # because we set them in they gyp handler
    'PATH',  # Because it gets a special merge handler
    'GYP_DEFINES',  # Because it gets a special merge handler
]

# The message to print when we detect use of an android output directory in a
# client that cannot build android.
_NOT_ANDROID_MESSAGE = """
You are trying to build for the android platform in a client that is not
android capable.
Please get a new client using "fetch android".
See https://code.google.com/p/chromium/wiki/AndroidBuildInstructions for more
details.
"""


class AndroidPlatform(cr.Platform):
  """The implementation of Platform for the android target."""

  ACTIVE = cr.Config.From(
      CR_ENVSETUP=os.path.join('{CR_SRC}', 'build', 'android', 'envsetup.sh'),
      CR_ADB=os.path.join('{ANDROID_SDK_ROOT}', 'platform-tools', 'adb'),
      CR_TARGET_SUFFIX='_apk',
      CR_BINARY=os.path.join('{CR_BUILD_DIR}', 'apks', '{CR_TARGET_NAME}.apk'),
      CR_ACTION='android.intent.action.VIEW',
      CR_PACKAGE='com.google.android.apps.{CR_TARGET}',
      CR_PROCESS='{CR_PACKAGE}',
      CR_ACTIVITY='.Main',
      CR_INTENT='{CR_PACKAGE}/{CR_ACTIVITY}',
      CR_TEST_RUNNER=os.path.join(
          '{CR_SRC}', 'build', 'android', 'test_runner.py'),
      CR_ADB_GDB=os.path.join('{CR_SRC}', 'build', 'android', 'adb_gdb'),
      CHROMIUM_OUT_DIR='{CR_OUT_BASE}',
      CR_DEFAULT_TARGET='chromium_testshell',
  )

  def __init__(self):
    super(AndroidPlatform, self).__init__()
    self._env = cr.Config('android-env', literal=True, export=True)
    self.detected_config.AddChild(self._env)
    self._env_ready = False
    self._env_paths = []

  @property
  def priority(self):
    return super(AndroidPlatform, self).priority + 1

  def Prepare(self, context):
    """Override Prepare from cr.Platform."""
    super(AndroidPlatform, self).Prepare(context)
    # Check we are an android capable client
    is_android = 'android' in context.gclient.get('target_os', '')
    if not is_android:
      url = context.gclient.get('solutions', [{}])[0].get('url')
      is_android = (url.startswith(
                        'https://chrome-internal.googlesource.com/') and
                    url.endswith('/internal/apps.git'))
    if not is_android:
      print _NOT_ANDROID_MESSAGE
      exit(1)
    try:
      # capture the result of env setup if we have not already done so
      if not self._env_ready:
        # See what the env would be without env setup
        before = context.exported
        # Run env setup and capture/parse it's output
        envsetup = 'source {CR_ENVSETUP} --target-arch={CR_ENVSETUP_ARCH}'
        output = cr.Host.CaptureShell(context, envsetup + ' > /dev/null && env')
        env_setup = cr.Config('envsetup', literal=True, export=True)
        for line in output.split('\n'):
          (key, op, value) = line.partition('=')
          if op:
            key = key.strip()
            if key not in _IGNORE_ENV:
              env_setup[key] = env_setup.ParseValue(value.strip())
            if key == 'PATH':
              self._env_paths = value.strip().split(os.path.pathsep)
            if key == 'GYP_DEFINES':
              # Make a version of GYP_DEFINES that is the combination of base
              # setting and envsetup, needs to override the overrides
              # Note: Forcing it into the top level scope - sledge-hammer
              context[key] = value.strip() + ' ' + before.get(key, '')
        items = env_setup.exported.items()
        if not items:
          # Because of the way envsetup is run, the exit code does not make it
          # back to us. Instead, we assume if we got no environment at all, it
          # must have failed.
          print 'Envsetup failed!'
          exit(1)
        # Find all the things that envsetup changed
        for key, value in env_setup.exported.items():
          if str(value) != str(before.get(key, None)):
            self._env[key] = value
      self._env_ready = True
    except subprocess.CalledProcessError, e:
      exit(e.returncode)

  @property
  def paths(self):
    return self._env_paths

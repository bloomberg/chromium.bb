# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The `osx_sdk` module provides safe functions to access a semi-hermetic
XCode installation.

Available only to Google-run bots."""

from contextlib import contextmanager

from recipe_engine import recipe_api


class OSXSDKApi(recipe_api.RecipeApi):
  """API for using OS X SDK distributed via CIPD."""

  def __init__(self, sdk_properties, *args, **kwargs):
    super(OSXSDKApi, self).__init__(*args, **kwargs)

    self._sdk_version = sdk_properties['sdk_version'].lower()
    self._tool_pkg = sdk_properties['toolchain_pkg']
    self._tool_ver = sdk_properties['toolchain_ver']

  @contextmanager
  def __call__(self, kind):
    """Sets up the XCode SDK environment.

    Is a no-op on non-mac platforms.

    This will deploy the helper tool and the XCode.app bundle at
    `[START_DIR]/cache/osx_sdk`.

    To avoid machines rebuilding these on every run, set up a named cache in
    your cr-buildbucket.cfg file like:

        caches: {
          # Cache for mac_toolchain tool and XCode.app
          name: "osx_sdk"
          path: "osx_sdk"
        }

    If you have builders which e.g. use a non-current SDK, you can give them
    a uniqely named cache:

        caches: {
          # Cache for N-1 version mac_toolchain tool and XCode.app
          name: "osx_sdk_old"
          path: "osx_sdk"
        }

    Similarly, if you have mac and iOS builders you may want to distinguish the
    cache name by adding '_ios' to it. However, if you're sharing the same bots
    for both mac and iOS, consider having a single cache and just always
    fetching the iOS version. This will lead to lower overall disk utilization
    and should help to reduce cache thrashing.

    Usage:
      with api.osx_sdk('mac'):
        # sdk with mac build bits

      with api.osx_sdk('ios'):
        # sdk with mac+iOS build bits

    Args:
      kind ('mac'|'ios'): How the SDK should be configured. iOS includes the
        base XCode distribution, as well as the iOS simulators (which can be
        quite large).

    Raises:
        StepFailure or InfraFailure.
    """
    assert kind in ('mac', 'ios'), 'Invalid kind %r' % (kind,)
    if not self.m.platform.is_mac:
      yield
      return

    try:
      with self.m.context(infra_steps=True):
        app = self._ensure_sdk(kind)
        self.m.step('select XCode', ['sudo', 'xcode-select', '--switch', app])
      yield
    finally:
      with self.m.context(infra_steps=True):
        self.m.step('reset XCode', ['sudo', 'xcode-select', '--reset'])

  def _ensure_sdk(self, kind):
    """Ensures the mac_toolchain tool and OS X SDK packages are installed.

    Returns Path to the installed sdk app bundle."""
    cache_dir = self.m.path['cache'].join('osx_sdk')

    ef = self.m.cipd.EnsureFile()
    ef.add_package(self._tool_pkg, self._tool_ver)
    self.m.cipd.ensure(cache_dir, ef)

    sdk_app = cache_dir.join('XCode.app')
    self.m.step('install xcode', [
        cache_dir.join('mac_toolchain'), 'install',
        '-kind', kind,
        '-xcode-version', self._sdk_version,
        '-output-dir', sdk_app,
    ])
    return sdk_app

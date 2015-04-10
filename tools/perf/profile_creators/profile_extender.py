# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import platform
from telemetry.core import wpr_modes


class ProfileExtender(object):
  """Abstract base class for an object that constructs a Chrome profile."""

  def __init__(self):
    # The path of the profile that the browser will use while it's running.
    # This member is initialized during SetUp().
    self._profile_path = None

    # A reference to the browser that will be performing all of the tab
    # navigations.
    # This member is initialized during SetUp().
    self._browser = None

  def Run(self, options):
    """Creates or extends the profile.

    |options| is an instance of BrowserFinderOptions. When subclass
    implementations of this method inevitably attempt to find and launch a
    browser, they should pass |options| to the relevant methods.

    Several properties of |options| might require direct manipulation by
    subclasses. These are:
      |options.output_profile_path|: The path at which the profile should be
      created.
      |options.browser_options.profile_dir|: If this property is None, then a
      new profile is created. Otherwise, the existing profile is appended on
      to.
    """
    raise NotImplementedError()

  def WebPageReplayArchivePath(self):
    """Returns the path to the WPR archive.

    Can be overridden by subclasses.
    """
    return None

  @property
  def profile_path(self):
    return self._profile_path

  @property
  def browser(self):
    return self._browser

  def SetUp(self, finder_options):
    """Finds and starts the browser.

    Can be overridden by subclasses. Subclasses must call the super class
    implementation.
    """
    self._profile_path = finder_options.output_profile_path
    possible_browser = self._GetPossibleBrowser(finder_options)

    assert possible_browser.supports_tab_control
    assert (platform.GetHostPlatform().GetOSName() in
        ["win", "mac", "linux"])

    self._SetUpWebPageReplay(finder_options, possible_browser)
    self._browser = possible_browser.Create(finder_options)

  def TearDown(self):
    """Teardown that is guaranteed to be executed before the instance is
    destroyed.

    Can be overridden by subclasses. Subclasses must call the super class
    implementation.
    """
    if self._browser:
      self._browser.Close()
      self._browser = None

  def FetchWebPageReplayArchives(self):
    """Fetches the web page replay archives.

    Can be overridden by subclasses.
    """
    pass

  def _SetUpWebPageReplay(self, finder_options, possible_browser):
    """Sets up Web Page Replay, if necessary."""

    wpr_archive_path = self.WebPageReplayArchivePath()
    if not wpr_archive_path:
      return

    self.FetchWebPageReplayArchives()

    # The browser options needs to be passed to both the network controller
    # as well as the browser backend.
    browser_options = finder_options.browser_options
    if finder_options.use_live_sites:
      browser_options.wpr_mode = wpr_modes.WPR_OFF
    else:
      browser_options.wpr_mode = wpr_modes.WPR_REPLAY

    network_controller = possible_browser.platform.network_controller
    make_javascript_deterministic = True

    network_controller.SetReplayArgs(
        wpr_archive_path, browser_options.wpr_mode, browser_options.netsim,
        browser_options.extra_wpr_args, make_javascript_deterministic)

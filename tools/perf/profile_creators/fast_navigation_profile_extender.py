# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from telemetry.core import browser_finder
from telemetry.core import browser_finder_exceptions
from telemetry.core import exceptions
from telemetry.core import platform
from telemetry.core.backends.chrome_inspector import devtools_http


class FastNavigationProfileExtender(object):
  """Extends a Chrome profile.

  This class creates or extends an existing profile by performing a set of tab
  navigations in large batches. This is accomplished by opening a large number
  of tabs, simultaneously navigating all the tabs, and then waiting for all the
  tabs to load. This provides two benefits:
    - Takes advantage of the high number of logical cores on modern CPUs.
    - The total time spent waiting for navigations to time out scales linearly
      with the number of batches, but does not scale with the size of the
      batch.
  """
  def __init__(self, maximum_batch_size):
    """Initializer.

    Args:
      maximum_batch_size: A positive integer indicating the number of tabs to
      simultaneously perform navigations.
    """
    super(FastNavigationProfileExtender, self).__init__()

    # The path of the profile that the browser will use while it's running.
    # This member is initialized during SetUp().
    self._profile_path = None

    # A reference to the browser that will be performing all of the tab
    # navigations.
    # This member is initialized during SetUp().
    self._browser = None

    # The instance keeps a list of Tabs that can be navigated successfully.
    # This means that the Tab is not crashed, and is processing JavaScript in a
    # timely fashion.
    self._navigation_tabs = []

    # The number of tabs to use.
    self._NUM_TABS = maximum_batch_size

    # The amount of time to wait for a batch of pages to finish loading.
    self._BATCH_PAGE_LOAD_TIMEOUT_IN_SECONDS = 10

    # The default amount of time to wait for the retrieval of the URL of a tab.
    self._TAB_URL_RETRIEVAL_TIMEOUT_IN_SECONDS = 1

  def Run(self, finder_options):
    """Extends the profile.

    Args:
      finder_options: An instance of BrowserFinderOptions that contains the
      directory of the input profile, the directory to place the output
      profile, and sufficient information to choose a specific browser binary.
    """
    try:
      self.SetUp(finder_options)
      self._PerformNavigations()
    finally:
      self.TearDown()

  def GetUrlIterator(self):
    """Gets URLs for the browser to navigate to.

    Intended for subclass override.

    Returns:
      An iterator whose elements are urls to be navigated to.
    """
    raise NotImplementedError()

  def ShouldExitAfterBatchNavigation(self):
    """Returns a boolean indicating whether profile extension is finished.

    Intended for subclass override.
    """
    raise NotImplementedError()

  def SetUp(self, finder_options):
    """Finds the browser, starts the browser, and opens the requisite number of
    tabs.

    Can be overridden by subclasses. Subclasses must call the super class
    implementation.
    """
    self._profile_path = finder_options.output_profile_path
    possible_browser = self._GetPossibleBrowser(finder_options)

    assert possible_browser.supports_tab_control
    assert (platform.GetHostPlatform().GetOSName() in
        ["win", "mac", "linux"])
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

  def CleanUpAfterBatchNavigation(self):
    """A hook for subclasses to perform cleanup after each batch of
    navigations.

    Can be overridden by subclasses.
    """
    pass

  @property
  def profile_path(self):
    return self._profile_path

  def _RefreshNavigationTabs(self):
    """Updates the member self._navigation_tabs to contain self._NUM_TABS
    elements, each of which is not crashed. The crashed tabs are intentionally
    leaked, since Telemetry doesn't have a good way of killing crashed tabs.

    It is also possible for a tab to be stalled in an infinite JavaScript loop.
    These tabs will be in self._browser.tabs, but not in self._navigation_tabs.
    There is no way to kill these tabs, so they are also leaked. This method is
    careful to only use tabs in self._navigation_tabs, or newly created tabs.
    """
    live_tabs = [tab for tab in self._navigation_tabs if tab.IsAlive()]
    self._navigation_tabs = live_tabs

    while len(self._navigation_tabs) < self._NUM_TABS:
      self._navigation_tabs.append(self._browser.tabs.New())

  def _RemoveNavigationTab(self, tab):
    """Removes a tab which is no longer in a useable state from
    self._navigation_tabs. The tab is not removed from self._browser.tabs,
    since there is no guarantee that the tab can be safely removed."""
    self._navigation_tabs.remove(tab)

  def _GetPossibleBrowser(self, finder_options):
    """Return a possible_browser with the given options."""
    possible_browser = browser_finder.FindBrowser(finder_options)
    if not possible_browser:
      raise browser_finder_exceptions.BrowserFinderException(
          'No browser found.\n\nAvailable browsers:\n%s\n' %
          '\n'.join(browser_finder.GetAllAvailableBrowserTypes(finder_options)))
    finder_options.browser_options.browser_type = (
        possible_browser.browser_type)

    return possible_browser

  def _RetrieveTabUrl(self, tab, timeout):
    """Retrives the URL of the tab."""
    try:
      return tab.EvaluateJavaScript('document.URL', timeout)
    except (exceptions.Error,
        devtools_http.DevToolsClientConnectionError,
        devtools_http.DevToolsClientUrlError):
      return None

  def _WaitForUrlToChange(self, tab, initial_url, timeout):
    """Waits for the tab to navigate away from its initial url."""
    end_time = time.time() + timeout
    while True:
      seconds_to_wait = end_time - time.time()
      seconds_to_wait = max(0, seconds_to_wait)

      if seconds_to_wait == 0:
        break

      current_url = self._RetrieveTabUrl(tab, seconds_to_wait)
      if current_url != initial_url:
        break

      # Retrieving the current url is a non-trivial operation. Add a small
      # sleep here to prevent this method from contending with the actual
      # navigation.
      time.sleep(0.01)

  def _BatchNavigateTabs(self, batch):
    """Performs a batch of tab navigations with minimal delay.

    Args:
      batch: A list of tuples (tab, url).

    Returns:
      A list of tuples (tab, initial_url). |initial_url| is the url of the
      |tab| prior to a navigation command being sent to it.
    """
    timeout_in_seconds = 0

    queued_tabs = []
    for tab, url in batch:
      initial_url = self._RetrieveTabUrl(tab,
          self._TAB_URL_RETRIEVAL_TIMEOUT_IN_SECONDS)

      try:
        tab.Navigate(url, None, timeout_in_seconds)
      except (exceptions.Error,
          devtools_http.DevToolsClientConnectionError,
          devtools_http.DevToolsClientUrlError):
        # We expect a time out. It's possible for other problems to arise, but
        # this method is not responsible for dealing with them. Ignore all
        # exceptions.
        pass

      queued_tabs.append((tab, initial_url))
    return queued_tabs

  def _WaitForQueuedTabsToLoad(self, queued_tabs):
    """Waits for all the batch navigated tabs to finish loading.

    Args:
      queued_tabs: A list of tuples (tab, initial_url). Each tab is guaranteed
      to have already been sent a navigation command.
    """
    end_time = time.time() + self._BATCH_PAGE_LOAD_TIMEOUT_IN_SECONDS
    for tab, initial_url in queued_tabs:
      seconds_to_wait = end_time - time.time()
      seconds_to_wait = max(0, seconds_to_wait)

      if seconds_to_wait == 0:
        break

      # Since we don't wait any time for the tab url navigation to commit, it's
      # possible that the tab hasn't started navigating yet.
      self._WaitForUrlToChange(tab, initial_url, seconds_to_wait)

      seconds_to_wait = end_time - time.time()
      seconds_to_wait = max(0, seconds_to_wait)

      try:
        tab.WaitForDocumentReadyStateToBeComplete(seconds_to_wait)
      except exceptions.TimeoutException:
        # Ignore time outs.
        pass
      except (exceptions.Error,
          devtools_http.DevToolsClientConnectionError,
          devtools_http.DevToolsClientUrlError):
        # If any error occurs, remove the tab. it's probably in an
        # unrecoverable state.
        self._RemoveNavigationTab(tab)

  def _GetUrlsToNavigate(self, url_iterator):
    """Returns an array of urls to navigate to, given a url_iterator."""
    urls = []
    for _ in xrange(self._NUM_TABS):
      try:
        urls.append(url_iterator.next())
      except StopIteration:
        break
    return urls

  def _PerformNavigations(self):
    """Repeatedly fetches a batch of urls, and navigates to those urls. This
    will run until an empty batch is returned, or
    ShouldExitAfterBatchNavigation() returns True.
    """
    url_iterator = self.GetUrlIterator()
    while True:
      self._RefreshNavigationTabs()
      urls = self._GetUrlsToNavigate(url_iterator)

      if len(urls) == 0:
        break

      batch = []
      for i in range(len(urls)):
        url = urls[i]
        tab = self._navigation_tabs[i]
        batch.append((tab, url))

      queued_tabs = self._BatchNavigateTabs(batch)
      self._WaitForQueuedTabsToLoad(queued_tabs)

      self.CleanUpAfterBatchNavigation()

      if self.ShouldExitAfterBatchNavigation():
        break

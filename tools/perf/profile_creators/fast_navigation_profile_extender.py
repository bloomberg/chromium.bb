# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from profile_creators import profile_extender
from telemetry.core import exceptions


class FastNavigationProfileExtender(profile_extender.ProfileExtender):
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
  def __init__(self, finder_options, maximum_batch_size):
    """Initializer.

    Args:
      maximum_batch_size: A positive integer indicating the number of tabs to
      simultaneously perform navigations.
    """
    super(FastNavigationProfileExtender, self).__init__(finder_options)

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

  def Run(self):
    """Superclass override."""
    try:
      self.SetUpBrowser()
      self._PerformNavigations()
    finally:
      self.TearDownBrowser()

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

  def CleanUpAfterBatchNavigation(self):
    """A hook for subclasses to perform cleanup after each batch of
    navigations.

    Can be overridden by subclasses.
    """
    pass

  def _AddNewTab(self):
    """Adds a new tab to the browser."""

    # Adding a new tab requires making a request over devtools. This can fail
    # for a variety of reasons. Retry 3 times.
    retry_count = 3
    for i in range(retry_count):
      try:
        self._navigation_tabs.append(self._browser.tabs.New())
      except exceptions.Error:
        if i == retry_count - 1:
          raise
      else:
        break

  def _RefreshNavigationTabs(self):
    """Updates the member self._navigation_tabs to contain self._NUM_TABS
    elements, each of which is not crashed. The crashed tabs are intentionally
    leaked, since Telemetry doesn't have a good way of killing crashed tabs.

    It is also possible for a tab to be stalled in an infinite JavaScript loop.
    These tabs will be in self.browser.tabs, but not in self._navigation_tabs.
    There is no way to kill these tabs, so they are also leaked. This method is
    careful to only use tabs in self._navigation_tabs, or newly created tabs.
    """
    live_tabs = [tab for tab in self._navigation_tabs if tab.IsAlive()]
    self._navigation_tabs = live_tabs

    while len(self._navigation_tabs) < self._NUM_TABS:
      self._AddNewTab()

  def _RemoveNavigationTab(self, tab):
    """Removes a tab which is no longer in a useable state from
    self._navigation_tabs. The tab is not removed from self.browser.tabs,
    since there is no guarantee that the tab can be safely removed."""
    self._navigation_tabs.remove(tab)

  def _RetrieveTabUrl(self, tab, timeout):
    """Retrives the URL of the tab."""
    try:
      return tab.EvaluateJavaScript('document.URL', timeout)
    except exceptions.Error:
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
      except exceptions.Error:
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
      except exceptions.Error:
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

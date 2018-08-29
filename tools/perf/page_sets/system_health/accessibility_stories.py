# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story


LONG_TEXT = """Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla
suscipit enim ut nunc vestibulum, vitae porta dui eleifend. Donec
condimentum ante malesuada mi sodales maximus."""


class _AccessibilityStory(system_health_story.SystemHealthStory):
  """Abstract base class for accessibility System Health user stories."""
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY

  def __init__(self, story_set, take_memory_measurement):
    super(_AccessibilityStory, self).__init__(
        story_set, take_memory_measurement,
        extra_browser_args=['--force-renderer-accessibility'])


class AccessibilityScrollingCodeSearchStory(_AccessibilityStory):
  """Tests scrolling an element within a page."""
  NAME = 'browse_accessibility:tech:codesearch'
  URL = 'https://cs.chromium.org/chromium/src/ui/accessibility/platform/ax_platform_node_mac.mm'
  TAGS = [story_tags.ACCESSIBILITY, story_tags.SCROLL]

  def RunNavigateSteps(self, action_runner):
    super(AccessibilityScrollingCodeSearchStory, self).RunNavigateSteps(
        action_runner)
    action_runner.WaitForElement(text='// namespace ui')
    action_runner.ScrollElement(selector='#file_scroller', distance=1000)


class AccessibilityWikipediaStory(_AccessibilityStory):
  """Wikipedia page on Accessibility. Long, but very simple, clean layout."""
  NAME = 'load_accessibility:media:wikipedia'
  URL = 'https://en.wikipedia.org/wiki/Accessibility'
  TAGS = [story_tags.ACCESSIBILITY]


class AccessibilityAmazonStory(_AccessibilityStory):
  """Amazon results page. Good example of a site with a data table."""
  NAME = 'load_accessibility:shopping:amazon'
  URL = 'https://www.amazon.com/gp/offer-listing/B01IENFJ14'
  TAGS = [story_tags.ACCESSIBILITY]


class AccessibilityYouTubeHomepageStory(_AccessibilityStory):
  """Tests interacting with the YouTube home page."""
  NAME = 'browse_accessibility:media:youtube'
  URL = 'https://www.youtube.com/'
  TAGS = [story_tags.ACCESSIBILITY, story_tags.KEYBOARD_INPUT]

  def RunNavigateSteps(self, action_runner):
    action_runner.Navigate('https://www.youtube.com/')
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()

    # Open and close the sidebar.
    action_runner.ClickElement(selector='[aria-label="Guide"]')
    action_runner.Wait(1)
    action_runner.ClickElement(selector='[aria-label="Guide"]')
    action_runner.Wait(1)

    # Open the apps menu.
    action_runner.ClickElement(selector='[aria-label="YouTube apps"]')
    action_runner.Wait(1)

    # Navigate through the items in the apps menu.
    for _ in range(6):
      action_runner.PressKey('Tab')

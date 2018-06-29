# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughCompositorPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOUGH_COMPOSITOR]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(ToughCompositorPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=['--report-silk-details', '--disable-top-sites',
                            '--disable-gpu-rasterization'])

  def RunNavigateSteps(self, action_runner):
    super(ToughCompositorPage, self).RunNavigateSteps(action_runner)
    # TODO(epenner): Remove this wait (http://crbug.com/366933)
    action_runner.Wait(5)


class ToughCompositorScrollPage(ToughCompositorPage):
  ABSTRACT_STORY = True

  def RunPageInteractions(self, action_runner):
    # Make the scroll longer to reduce noise.
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(direction='down', speed_in_pixels_per_second=300)


# Why: Baseline CC scrolling page. A long page with only text. """
class CCScrollTextPage(ToughCompositorScrollPage):
  BASE_NAME = 'cc_scroll_text_only'
  URL = 'http://jsbin.com/pixavefe/1/quiet?CC_SCROLL_TEXT_ONLY'


# Why: Baseline JS scrolling page. A long page with only text. """
class JSScrollTextPage(ToughCompositorScrollPage):
  BASE_NAME = 'js_scroll_text_only'
  URL = 'http://jsbin.com/wixadinu/2/quiet?JS_SCROLL_TEXT_ONLY'


# Why: Scroll by a large number of CC layers """
class CCScroll200LayerPage(ToughCompositorScrollPage):
  BASE_NAME = 'cc_scroll_200_layer_grid'
  URL = 'http://jsbin.com/yakagevo/1/quiet?CC_SCROLL_200_LAYER_GRID'


# Why: Scroll by a large number of JS layers """
class JSScroll200LayerPage(ToughCompositorScrollPage):
  BASE_NAME = 'js_scroll_200_layer_grid'
  URL = 'http://jsbin.com/jevibahi/4/quiet?JS_SCROLL_200_LAYER_GRID'


class ToughCompositorWaitPage(ToughCompositorPage):
  ABSTRACT_STORY = True

  def RunPageInteractions(self, action_runner):
    # We scroll back and forth a few times to reduce noise in the tests.
    with action_runner.CreateInteraction('Animation'):
      action_runner.Wait(8)


# Why: CC Poster circle animates many layers """
class CCPosterCirclePage(ToughCompositorWaitPage):
  BASE_NAME = 'cc_poster_circle'
  URL = 'http://jsbin.com/falefice/1/quiet?CC_POSTER_CIRCLE'


# Why: JS poster circle animates/commits many layers """
class JSPosterCirclePage(ToughCompositorWaitPage):
  BASE_NAME = 'js_poster_circle'
  URL = 'http://jsbin.com/giqafofe/1/quiet?JS_POSTER_CIRCLE'


# Why: JS invalidation does lots of uploads """
class JSFullScreenPage(ToughCompositorWaitPage):
  BASE_NAME = 'js_full_screen_invalidation'
  URL = 'http://jsbin.com/beqojupo/1/quiet?JS_FULL_SCREEN_INVALIDATION'


# Why: Creates a large number of new tilings """
class NewTilingsPage(ToughCompositorWaitPage):
  BASE_NAME = 'new_tilings'
  URL = 'http://jsbin.com/covoqi/1/quiet?NEW_TILINGS'

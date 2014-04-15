# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class KeySilkCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(KeySilkCasesPage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/key_silk_cases.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction())


class Page1(KeySilkCasesPage):

  """ Why: Infinite scroll. Brings out all of our perf issues. """

  def __init__(self, page_set):
    super(Page1, self).__init__(
      url='http://groupcloned.com/test/plain/list-recycle-transform.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction(
      {
        'scrollable_element_function': '''
          function(callback) {
            callback(document.getElementById('scrollable'));
          }'''
      }))


class Page2(KeySilkCasesPage):

  """ Why: Brings out layer management bottlenecks. """

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url='http://groupcloned.com/test/plain/list-animation-simple.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 2}))


class Page3(KeySilkCasesPage):

  """
  Why: Best-known method for fake sticky. Janks sometimes. Interacts badly with
  compositor scrolls.
  """

  def __init__(self, page_set):
    super(Page3, self).__init__(
      # pylint: disable=C0301
      url='http://groupcloned.com/test/plain/sticky-using-webkit-backface-visibility.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction(
      {
        'scrollable_element_function': '''
          function(callback) {
            callback(document.getElementById('container'));
          }'''
      }))


class Page4(KeySilkCasesPage):

  """
  Why: Card expansion: only the card should repaint, but in reality lots of
  storms happen.
  """

  def __init__(self, page_set):
    super(Page4, self).__init__(
      url='http://jsfiddle.net/3yDKh/4/embedded/result',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 3}))


class Page5(KeySilkCasesPage):

  """
  Why: Card expansion with animated contents, using will-change on the card
  """

  def __init__(self, page_set):
    super(Page5, self).__init__(
      url='http://jsfiddle.net/jx5De/12/embedded/result',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 4}))


class Page6(KeySilkCasesPage):

  """
  Why: Card fly-in: It should be fast to animate in a bunch of cards using
  margin-top and letting layout do the rest.
  """

  def __init__(self, page_set):
    super(Page6, self).__init__(
      url='http://jsfiddle.net/3yDKh/6/embedded/result',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 3}))


class Page7(KeySilkCasesPage):

  """
  Why: Image search expands a spacer div when you click an image to accomplish
  a zoomin effect. Each image has a layer. Even so, this triggers a lot of
  unnecessary repainting.
  """

  def __init__(self, page_set):
    super(Page7, self).__init__(
      url='http://jsfiddle.net/R8DX9/1/embedded/result/',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 3}))


class Page8(KeySilkCasesPage):

  """
  Why: Swipe to dismiss of an element that has a fixed-position child that is
  its pseudo-sticky header. Brings out issues with layer creation and
  repainting.
  """

  def __init__(self, page_set):
    super(Page8, self).__init__(
      url='http://jsfiddle.net/rF9Gh/3/embedded/result/',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 3}))


class Page9(KeySilkCasesPage):

  """
  Why: Horizontal and vertical expansion of a card that is cheap to layout but
  costly to rasterize.
  """

  def __init__(self, page_set):
    super(Page9, self).__init__(
      url='http://jsfiddle.net/humper/yEX8u/3/embedded/result/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 4}))


class Page10(KeySilkCasesPage):

  """
  Why: Vertical Expansion of a card that is cheap to layout but costly to
  rasterize.
  """

  def __init__(self, page_set):
    super(Page10, self).__init__(
      url='http://jsfiddle.net/humper/cKB9D/3/embedded/result/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 4}))


class Page11(KeySilkCasesPage):

  """
  Why: Parallax effect is common on photo-viewer-like applications, overloading
  software rasterization
  """

  def __init__(self, page_set):
    super(Page11, self).__init__(
      url='http://jsfiddle.net/vBQHH/3/embedded/result/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 4}))


class Page12(KeySilkCasesPage):

  """ Why: Addressing paint storms during coordinated animations. """

  def __init__(self, page_set):
    super(Page12, self).__init__(
      url='http://jsfiddle.net/ugkd4/9/embedded/result/',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 5}))


class Page13(KeySilkCasesPage):

  """ Why: Mask transitions are common mobile use cases. """

  def __init__(self, page_set):
    super(Page13, self).__init__(
      url='http://jankfree.org/silk/text-mask.html',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 4}))


class Page14(KeySilkCasesPage):

  """ Why: Card expansions with images and text are pretty and common. """

  def __init__(self, page_set):
    super(Page14, self).__init__(
      url='http://jankfree.org/silk/rectangle_transition.html',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 4}))


class Page15(KeySilkCasesPage):

  """ Why: Coordinated animations for expanding elements. """

  def __init__(self, page_set):
    super(Page15, self).__init__(
      url='file://key_silk_cases/font_wipe.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'seconds': 5}))


class Page16(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page16, self).__init__(
      url='file://key_silk_cases/inbox_app.html?swipe_to_dismiss',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction({'seconds': 2}))

  def SwipeToDismiss(self, action_runner):
    action_runner.RunAction(SwipeAction(
      {
        'left_start_percentage': 0.8,
        'distance': 200,
        'direction': 'left',
        'wait_after': {
          'javascript': 'document.getElementsByClassName("message").length < 18'
        },
        'top_start_percentage': 0.2,
        'element_function': '''
          function(callback) {
            callback(document.getElementsByClassName('message')[2]);
          }''',
        'speed': 5000
      }))

  def RunSmoothness(self, action_runner):
    self.SwipeToDismiss(action_runner)


class Page17(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page17, self).__init__(
      url='file://key_silk_cases/inbox_app.html?stress_hidey_bars',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction({'seconds': 2}))

  def RunSmoothness(self, action_runner):
    self.StressHideyBars(action_runner)

  def StressHideyBars(self, action_runner):
    action_runner.RunAction(ScrollAction(
      {
        'direction': 'down',
        'speed': 200,
        'scrollable_element_function': '''
          function(callback) {
            callback(document.getElementById('messages'));
          }'''
      }))
    action_runner.RunAction(ScrollAction(
      {
        'direction': 'up',
        'speed': 200,
        'scrollable_element_function': '''
          function(callback) {
            callback(document.getElementById('messages'));
          }'''
      }))
    action_runner.RunAction(ScrollAction(
      {
        'direction': 'down',
        'speed': 200,
        'scrollable_element_function': '''
          function(callback) {
            callback(document.getElementById('messages'));
          }'''
      }))


class Page18(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page18, self).__init__(
      url='file://key_silk_cases/inbox_app.html?toggle_drawer',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))

  def RunSmoothness(self, action_runner):
    for _ in xrange(6):
      self.ToggleDrawer(action_runner)

  def ToggleDrawer(self, action_runner):
    action_runner.RunAction(TapAction(
      {
        'wait_after': {'seconds': 1},
        'selector': '#menu-button'
      }))


class Page19(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page19, self).__init__(
      url='file://key_silk_cases/inbox_app.html?slide_drawer',
      page_set=page_set)

  def ToggleDrawer(self, action_runner):
    action_runner.RunAction(TapAction(
      {
        'wait_after': {
          'javascript': 'document.getElementById("nav-drawer").active'
        },
        'selector': '#menu-button'
      }))

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction({'seconds': 2}))
    self.ToggleDrawer(action_runner)

  def RunSmoothness(self, action_runner):
    self.SlideDrawer(action_runner)

  def SlideDrawer(self, action_runner):
    action_runner.RunAction(SwipeAction(
      {
        'left_start_percentage': 0.8,
        'distance': 200,
        'direction': 'left',
        'wait_after': {
          'javascript': '!document.getElementById("nav-drawer").active'
        },
        'top_start_percentage': 0.2,
        'element_function': '''
          function(callback) {
            callback(document.getElementById('nav-drawer').children[0]);
          }'''
      }))


class Page20(KeySilkCasesPage):

  """ Why: Shadow DOM infinite scrolling. """

  def __init__(self, page_set):
    super(Page20, self).__init__(
      url='file://key_silk_cases/infinite_scrolling.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction(
      {
        'speed': 5000,
        'scrollable_element_function': '''
          function(callback) {
            callback(document.getElementById('container'));
          }'''
      }))


class Page21(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page21, self).__init__(
      url='http://www.google.com/#q=google',
      page_set=page_set)

  def ScrollKnowledgeCardToTop(self, action_runner):
    # scroll until the knowledge card is at the top
    action_runner.RunAction(ScrollAction(
      {
        'scroll_distance_function': '''
          function() {
            return
              document.getElementById('kno-result')
                      .getBoundingClientRect().top -
              document.body.scrollTop;
          }'''
      }))

  def ExpandKnowledgeCard(self, action_runner):
    # expand card
    action_runner.RunAction(TapAction(
      {
        'element_function': '''
          function(callback) {
            callback(document.getElementsByClassName("vk_arc")[0]);
          }''',
       'wait_after': { 'seconds': 2 }
      }))

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction({'seconds': 3}))
    self.ScrollKnowledgeCardToTop(action_runner)

  def RunSmoothness(self, action_runner):
    self.ExpandKnowledgeCard(action_runner)


class Page22(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page22, self).__init__(
      url='http://plus.google.com/app/basic/stream',
      page_set=page_set)

    self.disabled = 'Times out on Windows; crbug.com/338838'
    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'document.getElementsByClassName("fHa").length > 0'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction(
      {
        'scrollable_element_function': '''
          function(callback) {
            callback(document.getElementById('mainContent'));
          }'''
      }))


class Page23(KeySilkCasesPage):

  """
  Why: Physical simulation demo that does a lot of element.style mutation
  triggering JS and recalc slowness
  """

  def __init__(self, page_set):
    super(Page23, self).__init__(
      url='http://jsbin.com/UVIgUTa/6/quiet',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction(
      {
        'direction': 'down',
        'scroll_requires_touch': True,
        'wait_after': { 'seconds': 1 },
        'scroll_distance_function':
          'function() { return window.innerHeight / 2; }'
      }))


class Page24(KeySilkCasesPage):

  """
  Why: Google News: this iOS version is slower than accelerated scrolling
  """

  def __init__(self, page_set):
    super(Page24, self).__init__(
      url='http://mobile-news.sandbox.google.com/news/pt0?scroll',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'document.getElementById(":h") != null'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 1
      }))

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction(
      {
        'scroll_distance_function': 'function() { return 2500; }',
        'scrollable_element_function':
          'function(callback) { callback(document.getElementById(":5")); }',
        'scroll_requires_touch': True
      }))


class Page25(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page25, self).__init__(
      url='http://mobile-news.sandbox.google.com/news/pt0?swipe',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'document.getElementById(":h") != null'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 1
      }))

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(SwipeAction(
      {
        'distance': 100,
        'direction': "left",
        'element_function': '''
          function(callback) {
            callback(document.getElementById(':f'));
          }''',
        'wait_after': { 'seconds': 1 }
      }))


class Page26(KeySilkCasesPage):

  """ Why: famo.us twitter demo """

  def __init__(self, page_set):
    super(Page26, self).__init__(
      url='http://s.codepen.io/befamous/fullpage/pFsqb?scroll',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': 'document.getElementsByClassName("tweet").length > 0'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 1
      }))

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction(
      {
        'scroll_distance_function': 'function() { return 5000; }'
      }))


class KeySilkCasesPageSet(page_set_module.PageSet):

  """ Pages hand-picked for project Silk. """

  def __init__(self):
    super(KeySilkCasesPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='mobile',
      archive_data_file='data/key_silk_cases.json')

    self.AddPage(Page1(self))
    self.AddPage(Page2(self))
    self.AddPage(Page3(self))
    self.AddPage(Page4(self))
    self.AddPage(Page5(self))
    self.AddPage(Page6(self))
    self.AddPage(Page7(self))
    self.AddPage(Page8(self))
    self.AddPage(Page9(self))
    self.AddPage(Page10(self))
    self.AddPage(Page11(self))
    self.AddPage(Page12(self))
    self.AddPage(Page13(self))
    self.AddPage(Page14(self))
    self.AddPage(Page15(self))
    self.AddPage(Page16(self))
    self.AddPage(Page17(self))
    self.AddPage(Page18(self))
    self.AddPage(Page19(self))
    self.AddPage(Page20(self))
    self.AddPage(Page21(self))
    self.AddPage(Page22(self))
    self.AddPage(Page23(self))
    self.AddPage(Page24(self))
    self.AddPage(Page25(self))
    self.AddPage(Page26(self))

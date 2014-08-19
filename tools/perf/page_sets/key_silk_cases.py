# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class KeySilkCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(KeySilkCasesPage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/key_silk_cases.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class Page1(KeySilkCasesPage):

  """ Why: Infinite scroll. Brings out all of our perf issues. """

  def __init__(self, page_set):
    super(Page1, self).__init__(
      url='http://groupcloned.com/test/plain/list-recycle-transform.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(selector='#scrollable')
    interaction.End()


class Page2(KeySilkCasesPage):

  """ Why: Brings out layer management bottlenecks. """

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url='http://groupcloned.com/test/plain/list-animation-simple.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(2)


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
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(selector='#container')
    interaction.End()


class Page4(KeySilkCasesPage):

  """
  Why: Card expansion: only the card should repaint, but in reality lots of
  storms happen.
  """

  def __init__(self, page_set):
    super(Page4, self).__init__(
      url='http://jsfiddle.net/3yDKh/15/show/',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(3)


class Page5(KeySilkCasesPage):

  """
  Why: Card expansion with animated contents, using will-change on the card
  """

  def __init__(self, page_set):
    super(Page5, self).__init__(
      url='http://jsfiddle.net/jx5De/14/show/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.Wait(4)


class Page6(KeySilkCasesPage):

  """
  Why: Card fly-in: It should be fast to animate in a bunch of cards using
  margin-top and letting layout do the rest.
  """

  def __init__(self, page_set):
    super(Page6, self).__init__(
      url='http://jsfiddle.net/3yDKh/16/show/',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(3)


class Page7(KeySilkCasesPage):

  """
  Why: Image search expands a spacer div when you click an image to accomplish
  a zoomin effect. Each image has a layer. Even so, this triggers a lot of
  unnecessary repainting.
  """

  def __init__(self, page_set):
    super(Page7, self).__init__(
      url='http://jsfiddle.net/R8DX9/4/show/',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(3)


class Page8(KeySilkCasesPage):

  """
  Why: Swipe to dismiss of an element that has a fixed-position child that is
  its pseudo-sticky header. Brings out issues with layer creation and
  repainting.
  """

  def __init__(self, page_set):
    super(Page8, self).__init__(
      url='http://jsfiddle.net/rF9Gh/7/show/',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(3)


class Page9(KeySilkCasesPage):

  """
  Why: Horizontal and vertical expansion of a card that is cheap to layout but
  costly to rasterize.
  """

  def __init__(self, page_set):
    super(Page9, self).__init__(
      url='http://jsfiddle.net/TLXLu/3/show/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.Wait(4)


class Page10(KeySilkCasesPage):

  """
  Why: Vertical Expansion of a card that is cheap to layout but costly to
  rasterize.
  """

  def __init__(self, page_set):
    super(Page10, self).__init__(
      url='http://jsfiddle.net/cKB9D/7/show/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.Wait(4)


class Page11(KeySilkCasesPage):

  """
  Why: Parallax effect is common on photo-viewer-like applications, overloading
  software rasterization
  """

  def __init__(self, page_set):
    super(Page11, self).__init__(
      url='http://jsfiddle.net/vBQHH/11/show/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.Wait(4)


class Page12(KeySilkCasesPage):

  """ Why: Addressing paint storms during coordinated animations. """

  def __init__(self, page_set):
    super(Page12, self).__init__(
      url='http://jsfiddle.net/ugkd4/10/show/',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(5)


class Page13(KeySilkCasesPage):

  """ Why: Mask transitions are common mobile use cases. """

  def __init__(self, page_set):
    super(Page13, self).__init__(
      url='http://jsfiddle.net/xLuvC/1/show/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.Wait(4)


class Page14(KeySilkCasesPage):

  """ Why: Card expansions with images and text are pretty and common. """

  def __init__(self, page_set):
    super(Page14, self).__init__(
      url='http://jsfiddle.net/bNp2h/3/show/',
      page_set=page_set)

    self.gpu_raster = True

  def RunSmoothness(self, action_runner):
    action_runner.Wait(4)


class Page15(KeySilkCasesPage):

  """ Why: Coordinated animations for expanding elements. """

  def __init__(self, page_set):
    super(Page15, self).__init__(
      url='file://key_silk_cases/font_wipe.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(5)


class Page16(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page16, self).__init__(
      url='file://key_silk_cases/inbox_app.html?swipe_to_dismiss',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)

  def SwipeToDismiss(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'SwipeAction', is_smooth=True)
    action_runner.SwipeElement(
        left_start_ratio=0.8, top_start_ratio=0.2,
        direction='left', distance=200, speed_in_pixels_per_second=5000,
        element_function='document.getElementsByClassName("message")[2]')
    interaction.End()
    interaction = action_runner.BeginInteraction('Wait', is_smooth=True)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("message").length < 18')
    interaction.End()

  def RunSmoothness(self, action_runner):
    self.SwipeToDismiss(action_runner)


class Page17(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page17, self).__init__(
      url='file://key_silk_cases/inbox_app.html?stress_hidey_bars',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)

  def RunSmoothness(self, action_runner):
    self.StressHideyBars(action_runner)

  def StressHideyBars(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        selector='#messages', direction='down', speed_in_pixels_per_second=200)
    interaction.End()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        selector='#messages', direction='up', speed_in_pixels_per_second=200)
    interaction.End()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        selector='#messages', direction='down', speed_in_pixels_per_second=200)
    interaction.End()


class Page18(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page18, self).__init__(
      url='file://key_silk_cases/inbox_app.html?toggle_drawer',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)

  def RunSmoothness(self, action_runner):
    for _ in xrange(6):
      self.ToggleDrawer(action_runner)

  def ToggleDrawer(self, action_runner):
    interaction = action_runner.BeginInteraction(
        'Action_TapAction', is_smooth=True)
    action_runner.TapElement('#menu-button')
    action_runner.Wait(1)
    interaction.End()


class Page19(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page19, self).__init__(
      url='file://key_silk_cases/inbox_app.html?slide_drawer',
      page_set=page_set)

  def ToggleDrawer(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'TapAction', is_smooth=True)
    action_runner.TapElement('#menu-button')
    interaction.End()

    interaction = action_runner.BeginInteraction('Wait', is_smooth=True)
    action_runner.WaitForJavaScriptCondition('''
        document.getElementById("nav-drawer").active &&
        document.getElementById("nav-drawer").children[0]
            .getBoundingClientRect().left == 0''')
    interaction.End()

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(2)
    self.ToggleDrawer(action_runner)

  def RunSmoothness(self, action_runner):
    self.SlideDrawer(action_runner)

  def SlideDrawer(self, action_runner):
    interaction = action_runner.BeginInteraction(
        'Action_SwipeAction', is_smooth=True)
    action_runner.SwipeElement(
        left_start_ratio=0.8, top_start_ratio=0.2,
        direction='left', distance=200,
        element_function='document.getElementById("nav-drawer").children[0]')
    action_runner.WaitForJavaScriptCondition(
        '!document.getElementById("nav-drawer").active')
    interaction.End()


class Page20(KeySilkCasesPage):

  """ Why: Shadow DOM infinite scrolling. """

  def __init__(self, page_set):
    super(Page20, self).__init__(
      url='file://key_silk_cases/infinite_scrolling.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        selector='#container', speed_in_pixels_per_second=5000)
    interaction.End()


class GwsExpansionPage(KeySilkCasesPage):
  """Abstract base class for pages that expand Google knowledge panels."""

  def NavigateWait(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(3)

  def ExpandKnowledgeCard(self, action_runner):
    # expand card
    interaction = action_runner.BeginInteraction(
        'Action_TapAction', is_smooth=True)
    action_runner.TapElement(
        element_function='document.getElementsByClassName("vk_arc")[0]')
    action_runner.Wait(2)
    interaction.End()

  def ScrollKnowledgeCardToTop(self, action_runner, card_id):
    # scroll until the knowledge card is at the top
    action_runner.ExecuteJavaScript(
        "document.getElementById('%s').scrollIntoView()" % card_id)

  def RunSmoothness(self, action_runner):
    self.ExpandKnowledgeCard(action_runner)


class GwsGoogleExpansion(GwsExpansionPage):

  """ Why: Animating height of a complex content card is common. """

  def __init__(self, page_set):
    super(GwsGoogleExpansion, self).__init__(
      url='http://www.google.com/#q=google',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    self.NavigateWait(action_runner)
    self.ScrollKnowledgeCardToTop(action_runner, 'kno-result')


class GwsBoogieExpansion(GwsExpansionPage):

  """ Why: Same case as Google expansion but text-heavy rather than image. """

  def __init__(self, page_set):
    super(GwsBoogieExpansion, self).__init__(
      url='https://www.google.com/search?hl=en&q=define%3Aboogie',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    self.NavigateWait(action_runner)
    self.ScrollKnowledgeCardToTop(action_runner, 'rso')


class Page22(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page22, self).__init__(
      url='http://plus.google.com/app/basic/stream',
      page_set=page_set)

    self.disabled = 'Times out on Windows; crbug.com/338838'
    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("fHa").length > 0')
    action_runner.Wait(2)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(selector='#mainContent')
    interaction.End()


class Page23(KeySilkCasesPage):

  """
  Why: Physical simulation demo that does a lot of element.style mutation
  triggering JS and recalc slowness
  """

  def __init__(self, page_set):
    super(Page23, self).__init__(
      url='http://jsbin.com/UVIgUTa/38/quiet',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage(
        distance_expr='window.innerHeight / 2',
        direction='down',
        use_touch=True)
    interaction.End()
    interaction = action_runner.BeginInteraction('Wait', is_smooth=True)
    action_runner.Wait(1)
    interaction.End()


class Page24(KeySilkCasesPage):

  """
  Why: Google News: this iOS version is slower than accelerated scrolling
  """

  def __init__(self, page_set):
    super(Page24, self).__init__(
      url='http://mobile-news.sandbox.google.com/news/pt0?scroll',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById(":h") != null')
    action_runner.Wait(1)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        element_function='document.getElementById(":5")',
        distance=2500,
        use_touch=True)
    interaction.End()


class Page25(KeySilkCasesPage):

  def __init__(self, page_set):
    super(Page25, self).__init__(
      url='http://mobile-news.sandbox.google.com/news/pt0?swipe',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById(":h") != null')
    action_runner.Wait(1)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'SwipeAction', is_smooth=True)
    action_runner.SwipeElement(
        direction='left', distance=100,
        element_function='document.getElementById(":f")')
    interaction.End()
    interaction = action_runner.BeginInteraction('Wait', is_smooth=True)
    action_runner.Wait(1)
    interaction.End()


class Page26(KeySilkCasesPage):

  """ Why: famo.us twitter demo """

  def __init__(self, page_set):
    super(Page26, self).__init__(
      url='http://s.codepen.io/befamous/fullpage/pFsqb?scroll',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("tweet").length > 0')
    action_runner.Wait(1)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage(distance=5000)
    interaction.End()


class SVGIconRaster(KeySilkCasesPage):

  """ Why: Mutating SVG icons; these paint storm and paint slowly. """

  def __init__(self, page_set):
    super(SVGIconRaster, self).__init__(
      url='http://wiltzius.github.io/shape-shifter/',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'loaded = true')
    action_runner.Wait(1)

  def RunSmoothness(self, action_runner):
    for i in xrange(9):
      button_func = ('document.getElementById("demo").$.'
                     'buttons.children[%d]') % i
      interaction = action_runner.BeginInteraction(
            'Action_TapAction', is_smooth=True)
      action_runner.TapElement(element_function=button_func)
      action_runner.Wait(1)
      interaction.End()


class UpdateHistoryState(KeySilkCasesPage):

  """ Why: Modern apps often update history state, which currently is janky."""

  def __init__(self, page_set):
    super(UpdateHistoryState, self).__init__(
      url='file://key_silk_cases/pushState.html',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.ExecuteJavaScript('''
        window.requestAnimationFrame(function() {
            window.__history_state_loaded = true;
          });
        ''')
    action_runner.WaitForJavaScriptCondition(
        'window.__history_state_loaded == true;')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginInteraction('animation_interaction',
        is_smooth=True)
    action_runner.Wait(5) # JS runs the animation continuously on the page
    interaction.End()


class KeySilkCasesPageSet(page_set_module.PageSet):

  """ Pages hand-picked for project Silk. """

  def __init__(self):
    super(KeySilkCasesPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='mobile',
      archive_data_file='data/key_silk_cases.json',
      bucket=page_set_module.PARTNER_BUCKET)

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
    self.AddPage(GwsGoogleExpansion(self))
    self.AddPage(GwsBoogieExpansion(self))
    self.AddPage(Page22(self))
    self.AddPage(Page23(self))
    self.AddPage(Page24(self))
    self.AddPage(Page25(self))
    self.AddPage(Page26(self))
    self.AddPage(SVGIconRaster(self))
    self.AddPage(UpdateHistoryState(self))

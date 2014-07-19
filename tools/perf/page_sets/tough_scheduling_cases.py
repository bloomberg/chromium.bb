# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughSchedulingCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughSchedulingCasesPage, self).__init__(url=url, page_set=page_set)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class Page1(ToughSchedulingCasesPage):

  """ Why: Simulate oversubscribed main thread """

  def __init__(self, page_set):
    super(Page1, self).__init__(
      url='file://tough_scheduling_cases/simple_text_page.html?main_busy',
      page_set=page_set)

    self.synthetic_delays = {'cc.BeginMainFrame': {'target_duration': 0.008}}


class Page2(ToughSchedulingCasesPage):

  """ Why: Simulate oversubscribed main thread """

  def __init__(self, page_set):
    super(Page2, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/simple_text_page.html?main_very_busy',
      page_set=page_set)

    self.synthetic_delays = {'cc.BeginMainFrame': {'target_duration': 0.024}}


class Page3(ToughSchedulingCasesPage):

  """ Why: Simulate a page with a a few graphics layers """

  def __init__(self, page_set):
    super(Page3, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/simple_text_page.html?medium_layers',
      page_set=page_set)

    self.synthetic_delays = {
      'cc.DrawAndSwap': {'target_duration': 0.004},
      'gpu.PresentingFrame': {'target_duration': 0.004},
      'cc.BeginMainFrame': {'target_duration': 0.004}
    }


class Page4(ToughSchedulingCasesPage):

  """ Why: Simulate a page with many graphics layers """

  def __init__(self, page_set):
    super(Page4, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/simple_text_page.html?many_layers',
      page_set=page_set)

    self.synthetic_delays = {
      'cc.DrawAndSwap': {'target_duration': 0.012},
      'gpu.PresentingFrame': {'target_duration': 0.012},
      'cc.BeginMainFrame': {'target_duration': 0.012}
    }


class Page5(ToughSchedulingCasesPage):

  """ Why: Simulate a page with expensive recording and rasterization """

  def __init__(self, page_set):
    super(Page5, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/simple_text_page.html?medium_raster',
      page_set=page_set)

    self.synthetic_delays = {
      'cc.RasterRequiredForActivation': {'target_duration': 0.004},
      'cc.BeginMainFrame': {'target_duration': 0.004},
      'gpu.AsyncTexImage': {'target_duration': 0.004}
    }


class Page6(ToughSchedulingCasesPage):

  """ Why: Simulate a page with expensive recording and rasterization """

  def __init__(self, page_set):
    super(Page6, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/simple_text_page.html?heavy_raster',
      page_set=page_set)

    self.synthetic_delays = {
      'cc.RasterRequiredForActivation': {'target_duration': 0.024},
      'cc.BeginMainFrame': {'target_duration': 0.024},
      'gpu.AsyncTexImage': {'target_duration': 0.024}
    }


class Page7(ToughSchedulingCasesPage):

  """ Why: Medium cost touch handler """

  def __init__(self, page_set):
    super(Page7, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/touch_handler_scrolling.html?medium_handler',
      page_set=page_set)

    self.synthetic_delays = {'blink.HandleInputEvent':
                             {'target_duration': 0.008}}


class Page8(ToughSchedulingCasesPage):

  """ Why: Slow touch handler """

  def __init__(self, page_set):
    super(Page8, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/touch_handler_scrolling.html?slow_handler',
      page_set=page_set)

    self.synthetic_delays = {'blink.HandleInputEvent':
                             {'target_duration': 0.024}}


class Page9(ToughSchedulingCasesPage):

  """ Why: Touch handler that often takes a long time """

  def __init__(self, page_set):
    super(Page9, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/touch_handler_scrolling.html?janky_handler',
      page_set=page_set)

    self.synthetic_delays = {'blink.HandleInputEvent':
                             {'target_duration': 0.024, 'mode': 'alternating'}
                            }


class Page10(ToughSchedulingCasesPage):

  """ Why: Touch handler that occasionally takes a long time """

  def __init__(self, page_set):
    super(Page10, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/touch_handler_scrolling.html?occasionally_janky_handler',
      page_set=page_set)

    self.synthetic_delays = {'blink.HandleInputEvent':
                             {'target_duration': 0.024, 'mode': 'oneshot'}}


class Page11(ToughSchedulingCasesPage):

  """ Why: Super expensive touch handler causes browser to scroll after a
  timeout.
  """

  def __init__(self, page_set):
    super(Page11, self).__init__(
      # pylint: disable=C0301
      url='file://tough_scheduling_cases/touch_handler_scrolling.html?super_slow_handler',
      page_set=page_set)

    self.synthetic_delays = {'blink.HandleInputEvent':
                             {'target_duration': 0.2}}


class Page12(ToughSchedulingCasesPage):

  """ Why: Super expensive touch handler that only occupies a part of the page.
  """

  def __init__(self, page_set):
    super(Page12, self).__init__(
      url='file://tough_scheduling_cases/div_touch_handler.html',
      page_set=page_set)

    self.synthetic_delays = {'blink.HandleInputEvent': {'target_duration': 0.2}}


class Page13(ToughSchedulingCasesPage):

  """ Why: Test a moderately heavy requestAnimationFrame handler """

  def __init__(self, page_set):
    super(Page13, self).__init__(
      url='file://tough_scheduling_cases/raf.html?medium_handler',
      page_set=page_set)

    self.synthetic_delays = {
      'cc.RasterRequiredForActivation': {'target_duration': 0.004},
      'cc.BeginMainFrame': {'target_duration': 0.004},
      'gpu.AsyncTexImage': {'target_duration': 0.004}
    }


class Page14(ToughSchedulingCasesPage):

  """ Why: Test a moderately heavy requestAnimationFrame handler """

  def __init__(self, page_set):
    super(Page14, self).__init__(
      url='file://tough_scheduling_cases/raf.html?heavy_handler',
      page_set=page_set)

    self.synthetic_delays = {
      'cc.RasterRequiredForActivation': {'target_duration': 0.024},
      'cc.BeginMainFrame': {'target_duration': 0.024},
      'gpu.AsyncTexImage': {'target_duration': 0.024}
    }


class Page15(ToughSchedulingCasesPage):

  """ Why: Simulate a heavily GPU bound page """

  def __init__(self, page_set):
    super(Page15, self).__init__(
      url='file://tough_scheduling_cases/raf.html?gpu_bound',
      page_set=page_set)

    self.synthetic_delays = {'gpu.PresentingFrame': {'target_duration': 0.1}}


class Page16(ToughSchedulingCasesPage):

  """ Why: Test a requestAnimationFrame handler with a heavy first frame """

  def __init__(self, page_set):
    super(Page16, self).__init__(
      url='file://tough_scheduling_cases/raf.html?heavy_first_frame',
      page_set=page_set)

    self.synthetic_delays = {'cc.BeginMainFrame': {'target_duration': 0.15}}


class Page17(ToughSchedulingCasesPage):

  """ Why: Medium stress test for the scheduler """

  def __init__(self, page_set):
    super(Page17, self).__init__(
      url='file://tough_scheduling_cases/raf_touch_animation.html?medium',
      page_set=page_set)

    self.synthetic_delays = {
      'cc.DrawAndSwap': {'target_duration': 0.004},
      'cc.BeginMainFrame': {'target_duration': 0.004}
    }


class Page18(ToughSchedulingCasesPage):

  """ Why: Heavy stress test for the scheduler """

  def __init__(self, page_set):
    super(Page18, self).__init__(
      url='file://tough_scheduling_cases/raf_touch_animation.html?heavy',
      page_set=page_set)

    self.synthetic_delays = {
      'cc.DrawAndSwap': {'target_duration': 0.012},
      'cc.BeginMainFrame': {'target_duration': 0.012}
    }


class Page19(ToughSchedulingCasesPage):

  """ Why: Both main and impl thread animating concurrently """

  def __init__(self, page_set):
    super(Page19, self).__init__(
      url='file://tough_scheduling_cases/split_animation.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.Wait(3)


class Page20(ToughSchedulingCasesPage):

  """ Why: Simple JS touch dragging """

  def __init__(self, page_set):
    super(Page20, self).__init__(
      url='file://tough_scheduling_cases/simple_touch_drag.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        selector='#card',
        use_touch=True,
        direction='up',
        speed_in_pixels_per_second=150,
        distance=400)
    interaction.End()


class EmptyTouchHandlerPage(ToughSchedulingCasesPage):

  """ Why: Scrolling on a page with a touch handler that consumes no events but
      may be slow """

  def __init__(self, name, desktop, slow_handler, bounce, page_set):
    super(EmptyTouchHandlerPage, self).__init__(
      url='file://tough_scheduling_cases/empty_touch_handler' +
        ('_desktop' if desktop else '') + '.html?' + name,
      page_set=page_set)

    if slow_handler:
      self.synthetic_delays = {
        'blink.HandleInputEvent': {'target_duration': 0.2}
      }

    self.bounce = bounce

  def RunSmoothness(self, action_runner):
    if self.bounce:
      interaction = action_runner.BeginGestureInteraction(
          'ScrollBounceAction', is_smooth=True)
      action_runner.ScrollBouncePage()
      interaction.End()
    else:
      interaction = action_runner.BeginGestureInteraction(
          'ScrollAction', is_smooth=True)
      # Speed and distance are tuned to run exactly as long as a scroll
      # bounce.
      action_runner.ScrollPage(use_touch=True, speed_in_pixels_per_second=400,
                               distance=2100)
      interaction.End()


class SynchronizedScrollOffsetPage(ToughSchedulingCasesPage):

  """Why: For measuring the latency of scroll-synchronized effects."""

  def __init__(self, page_set):
    super(SynchronizedScrollOffsetPage, self).__init__(
      url='file://tough_scheduling_cases/sync_scroll_offset.html',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollBounceAction', is_smooth=True)
    action_runner.ScrollBouncePage()
    interaction.End()


class ToughSchedulingCasesPageSet(page_set_module.PageSet):

  """ Tough scheduler latency test cases """

  def __init__(self):
    super(ToughSchedulingCasesPageSet, self).__init__()

    # Why: Simple scrolling baseline
    self.AddPage(ToughSchedulingCasesPage(
      'file://tough_scheduling_cases/simple_text_page.html',
      self))
    self.AddPage(Page1(self))
    self.AddPage(Page2(self))
    self.AddPage(Page3(self))
    self.AddPage(Page4(self))
    self.AddPage(Page5(self))
    # self.AddPage(Page6(self)) Flaky crbug.com/368532
    # Why: Touch handler scrolling baseline
    self.AddPage(ToughSchedulingCasesPage(
      'file://tough_scheduling_cases/touch_handler_scrolling.html',
      self))
    self.AddPage(Page7(self))
    self.AddPage(Page8(self))
    self.AddPage(Page9(self))
    self.AddPage(Page10(self))
    self.AddPage(Page11(self))
    self.AddPage(Page12(self))
    # Why: requestAnimationFrame scrolling baseline
    self.AddPage(ToughSchedulingCasesPage(
      'file://tough_scheduling_cases/raf.html',
      self))
    # Why: Test canvas blocking behavior
    self.AddPage(ToughSchedulingCasesPage(
      'file://tough_scheduling_cases/raf_canvas.html',
      self))
    self.AddPage(Page13(self))
    # Disabled for flakiness. See 368532
    # self.AddPage(Page14(self))
    self.AddPage(Page15(self))
    self.AddPage(Page16(self))
    # Why: Test a requestAnimationFrame handler with concurrent CSS animation
    self.AddPage(ToughSchedulingCasesPage(
      'file://tough_scheduling_cases/raf_animation.html',
      self))
    # Why: Stress test for the scheduler
    self.AddPage(ToughSchedulingCasesPage(
      'file://tough_scheduling_cases/raf_touch_animation.html',
      self))
    self.AddPage(Page17(self))
    self.AddPage(Page18(self))
    self.AddPage(Page19(self))
    self.AddPage(Page20(self))
    # Why: Baseline for scrolling in the presence of a no-op touch handler
    self.AddPage(EmptyTouchHandlerPage(
      name='baseline',
      desktop=False,
      slow_handler=False,
      bounce=False,
      page_set=self))
    # Why: Slow handler blocks scroll start
    self.AddPage(EmptyTouchHandlerPage(
      name='slow_handler',
      desktop=False,
      slow_handler=True,
      bounce=False,
      page_set=self))
    # Why: Slow handler blocks scroll start until touch ACK timeout
    self.AddPage(EmptyTouchHandlerPage(
      name='desktop_slow_handler',
      desktop=True,
      slow_handler=True,
      bounce=False,
      page_set=self))
    # Why: Scroll bounce showing repeated transitions between scrolling and
    # sending synchronous touchmove events.  Should be nearly as fast as
    # scroll baseline.
    self.AddPage(EmptyTouchHandlerPage(
      name='bounce',
      desktop=False,
      slow_handler=False,
      bounce=True,
      page_set=self))
    # Why: Scroll bounce with slow handler, repeated blocking.
    self.AddPage(EmptyTouchHandlerPage(
      name='bounce_slow_handler',
      desktop=False,
      slow_handler=True,
      bounce=True,
      page_set=self))
    # Why: Scroll bounce with slow handler on desktop, blocks only once until
    # ACK timeout.
    self.AddPage(EmptyTouchHandlerPage(
      name='bounce_desktop_slow_handler',
      desktop=True,
      slow_handler=True,
      bounce=True,
      page_set=self))
    # Why: For measuring the latency of scroll-synchronized effects.
    self.AddPage(SynchronizedScrollOffsetPage(page_set=self))

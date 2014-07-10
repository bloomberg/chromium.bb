# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughCanvasCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughCanvasCasesPage, self).__init__(url=url, page_set=page_set)
    self.archive_data_file = 'data/tough_canvas_cases.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        "document.readyState == 'complete'")

  def RunSmoothness(self, action_runner):
    action_runner.Wait(5)


class MicrosofFirefliesPage(ToughCanvasCasesPage):

  def __init__(self, page_set):
    super(MicrosofFirefliesPage, self).__init__(
      # pylint: disable=C0301
      url='http://ie.microsoft.com/testdrive/Performance/Fireflies/Default.html',
      page_set=page_set)

    self.disabled = 'Crashes on Galaxy Nexus. crbug.com/314131'


class ToughCanvasCasesPageSet(page_set_module.PageSet):

  """
  Description: Self-driven Canvas2D animation examples
  """

  def __init__(self):
    super(ToughCanvasCasesPageSet, self).__init__(
      archive_data_file='data/tough_canvas_cases.json',
      bucket=page_set_module.PARTNER_BUCKET)

    self.AddPage(MicrosofFirefliesPage(self))

    # Failing on Nexus 5 (http://crbug.com/364248):
    # 'http://geoapis.appspot.com/agdnZW9hcGlzchMLEgtFeGFtcGxlQ29kZRjh1wIM',

    urls_list = [
      'http://mudcu.be/labs/JS1k/BreathingGalaxies.html',
      'http://runway.countlessprojects.com/prototype/performance_test.html',
      # pylint: disable=C0301
      'http://ie.microsoft.com/testdrive/Performance/FishIETank/Default.html',
      'http://ie.microsoft.com/testdrive/Performance/SpeedReading/Default.html',
      'http://acko.net/dumpx/996b.html',
      'http://www.kevs3d.co.uk/dev/canvask3d/k3d_test.html',
      'http://www.megidish.net/awjs/',
      'http://themaninblue.com/experiment/AnimationBenchmark/canvas/',
      'http://mix10k.visitmix.com/Entry/Details/169',
      'http://www.craftymind.com/factory/guimark2/HTML5ChartingTest.html',
      'http://www.chiptune.com/starfield/starfield.html',
      'http://jarrodoverson.com/static/demos/particleSystem/',
      'http://www.effectgames.com/demos/canvascycle/',
      'http://www.thewildernessdowntown.com/',
      'http://spielzeugz.de/html5/liquid-particles.html',
      'http://hakim.se/experiments/html5/magnetic/02/',
      'http://ie.microsoft.com/testdrive/Performance/LetItSnow/',
      'http://ie.microsoft.com/testdrive/Graphics/WorkerFountains/Default.html',
      'http://ie.microsoft.com/testdrive/Graphics/TweetMap/Default.html',
      'http://ie.microsoft.com/testdrive/Graphics/VideoCity/Default.html',
      'http://ie.microsoft.com/testdrive/Performance/AsteroidBelt/Default.html',
      'http://www.smashcat.org/av/canvas_test/',
      # pylint: disable=C0301
      'file://tough_canvas_cases/canvas2d_balls_common/bouncing_balls.html?ball=canvas_sprite&back=canvas',
      # pylint: disable=C0301
      'file://tough_canvas_cases/canvas2d_balls_common/bouncing_balls.html?ball=image_with_shadow&back=image',
      # pylint: disable=C0301
      'file://tough_canvas_cases/canvas2d_balls_common/bouncing_balls.html?ball=filled_path&back=gradient',
      # pylint: disable=C0301
      'file://tough_canvas_cases/canvas2d_balls_common/bouncing_balls.html?ball=text&back=white&ball_count=15',
      'file://tough_canvas_cases/canvas-animation-no-clear.html',
      'file://../../../chrome/test/data/perf/canvas_bench/single_image.html',
      'file://../../../chrome/test/data/perf/canvas_bench/many_images.html'
    ]

    for url in urls_list:
      self.AddPage(ToughCanvasCasesPage(url, self))

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story


class ToughCanvasCasesPage(page_module.Page):

  def __init__(self, name, url, page_set):
    super(ToughCanvasCasesPage, self).__init__(url=url, page_set=page_set,
                                               name=name)

  def RunNavigateSteps(self, action_runner):
    super(ToughCanvasCasesPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        "document.readyState == 'complete'")

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CanvasAnimation'):
      action_runner.Wait(5)


class MicrosofFirefliesPage(ToughCanvasCasesPage):

  def __init__(self, page_set):
    super(MicrosofFirefliesPage, self).__init__(
      name='microsoft_fireflies',
      # pylint: disable=line-too-long
      url='http://ie.microsoft.com/testdrive/Performance/Fireflies/Default.html',
      page_set=page_set)


class ToughCanvasCasesPageSet(story.StorySet):

  """
  Description: Self-driven Canvas2D animation examples
  """

  def __init__(self):
    super(ToughCanvasCasesPageSet, self).__init__(
      archive_data_file='data/tough_canvas_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    # Crashes on Galaxy Nexus. crbug.com/314131
    # TODO(rnephew): Rerecord this story.
    # self.AddStory(MicrosofFirefliesPage(self))

    urls_list = [
      ('geo_apis',
       'http://geoapis.appspot.com/agdnZW9hcGlzchMLEgtFeGFtcGxlQ29kZRjh1wIM'),
      ('runway',
       'http://runway.countlessprojects.com/prototype/performance_test.html'),
      ('microsoft_fish_ie_tank',
       # pylint: disable=line-too-long
       'http://ie.microsoft.com/testdrive/Performance/FishIETank/Default.html'),
      ('microsoft_speed_reading',
       # pylint: disable=line-too-long
       'http://ie.microsoft.com/testdrive/Performance/SpeedReading/Default.html'),
      ('kevs_3d',
       'http://www.kevs3d.co.uk/dev/canvask3d/k3d_test.html'),
      ('megi_dish',
       'http://www.megidish.net/awjs/'),
      ('man_in_blue',
       'http://themaninblue.com/experiment/AnimationBenchmark/canvas/'),
      ('mix_10k',
       'http://mix10k.visitmix.com/Entry/Details/169'),
      ('crafty_mind',
       'http://www.craftymind.com/factory/guimark2/HTML5ChartingTest.html'),
      ('chip_tune',
       'http://www.chiptune.com/starfield/starfield.html'),
      ('jarro_doverson',
       'http://jarrodoverson.com/static/demos/particleSystem/'),
      ('effect_games',
       'http://www.effectgames.com/demos/canvascycle/'),
      ('spielzeugz',
       'http://spielzeugz.de/html5/liquid-particles.html'),
      ('hakim',
       'http://hakim.se/experiments/html5/magnetic/02/'),
      ('microsoft_snow',
       'http://ie.microsoft.com/testdrive/Performance/LetItSnow/'),
      ('microsoft_worker_fountains',
       # pylint: disable=line-too-long
       'http://ie.microsoft.com/testdrive/Graphics/WorkerFountains/Default.html'),
      ('microsoft_tweet_map',
       'http://ie.microsoft.com/testdrive/Graphics/TweetMap/Default.html'),
      ('microsoft_video_city',
       'http://ie.microsoft.com/testdrive/Graphics/VideoCity/Default.html'),
      ('microsoft_asteroid_belt',
       # pylint: disable=line-too-long
       'http://ie.microsoft.com/testdrive/Performance/AsteroidBelt/Default.html'),
      ('smash_cat',
       'http://www.smashcat.org/av/canvas_test/'),
      ('bouncing_balls_shadow',
       # pylint: disable=line-too-long
       'file://tough_canvas_cases/canvas2d_balls_common/bouncing_balls.html?ball=image_with_shadow&back=image'),
      ('bouncing_balls_15',
       # pylint: disable=line-too-long
       'file://tough_canvas_cases/canvas2d_balls_common/bouncing_balls.html?ball=text&back=white&ball_count=15'),
      ('canvas_font_cycler',
       'file://tough_canvas_cases/canvas-font-cycler.html'),
      ('canvas_animation_no_clear',
       'file://tough_canvas_cases/canvas-animation-no-clear.html'),
      ('canvas_to_blob',
       'file://tough_canvas_cases/canvas_toBlob.html'),
      ('many_images',
       # pylint: disable=line-too-long
       'file://../../../chrome/test/data/perf/canvas_bench/many_images.html'),
      ('canvas_arcs',
       'file://tough_canvas_cases/rendering_throughput/canvas_arcs.html'),
      ('canvas_lines',
       'file://tough_canvas_cases/rendering_throughput/canvas_lines.html'),
      ('put_get_image_data',
       # pylint: disable=line-too-long
       'file://tough_canvas_cases/rendering_throughput/put_get_image_data.html'),
      ('fill_shapes',
       'file://tough_canvas_cases/rendering_throughput/fill_shapes.html'),
      ('stroke_shapes',
       'file://tough_canvas_cases/rendering_throughput/stroke_shapes.html'),
      ('bouncing_clipped_rectangles',
       # pylint: disable=line-too-long
       'file://tough_canvas_cases/rendering_throughput/bouncing_clipped_rectangles.html'),
      ('bouncing_gradient_circles',
       # pylint: disable=line-too-long
       'file://tough_canvas_cases/rendering_throughput/bouncing_gradient_circles.html'),
      ('bouncing_svg_images',
       # pylint: disable=line-too-long
       'file://tough_canvas_cases/rendering_throughput/bouncing_svg_images.html'),
      ('bouncing_png_images',
       'file://tough_canvas_cases/rendering_throughput/bouncing_png_images.html')
    ]

    for name, url in urls_list:
      self.AddStory(ToughCanvasCasesPage(name=name,url=url,page_set=self))

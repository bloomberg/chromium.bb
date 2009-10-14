import os
import re
import subprocess
import unittest
import sys

import selenium_utilities
import selenium_constants

class PDiffTest(unittest.TestCase):
  """A perceptual diff test class, for running perceptual diffs on any
  number of screenshots."""
  
  def __init__(self, name, num_screenshots, screenshot_name, pdiff_path,
               gen_dir, ref_dir, options):
    unittest.TestCase.__init__(self, name)
    self.name = name
    self.num_screenshots = num_screenshots
    self.screenshot_name = screenshot_name
    self.pdiff_path = pdiff_path
    self.gen_dir = gen_dir
    self.ref_dir = ref_dir
    self.options = options

  def shortDescription(self):
    """override unittest.TestCase shortDescription for our own descriptions."""
    return "Screenshot comparison for: " + self.name
    
  def PDiffTest(self):
    """Runs a generic Perceptual Diff test."""
    # Get arguments for perceptual diff.
    pixel_threshold = "10"
    alpha_threshold = "1.0"
    use_colorfactor = False
    use_downsample = False
    use_edge = True
    edge_threshold = "5"

    for opt in self.options:
      if opt.startswith("pdiff_threshold"):
        pixel_threshold = selenium_utilities.GetArgument(opt)
      elif (opt.startswith("pdiff_threshold_mac") and
            sys.platform == "darwin"):
        pixel_threshold = selenium_utilities.GetArgument(opt)
      elif (opt.startswith("pdiff_threshold_win") and
            sys.platform == 'win32' or sys.platform == "cygwin"):
        pixel_threshold = selenium_utilities.GetArgument(opt)
      elif (opt.startswith("pdiff_threshold_linux") and
            sys.platform[:5] == "linux"):
        pixel_threshold = selenium_utilities.GetArgument(opt)
      elif (opt.startswith("colorfactor")):
        colorfactor = selenium_utilities.GetArgument(opt)
        use_colorfactor = True
      elif (opt.startswith("downsample")):
        downsample_factor = selenium_utilities.GetArgument(opt)
        use_downsample = True
      elif (opt.startswith("pdiff_edge_ignore_off")):
        use_edge = False
      elif (opt.startswith("pdiff_edge_threshold")):
        edge_threshold = selenium_utilities.GetArgument(opt)

    results = []
    # Loop over number of screenshots.
    for screenshot_no in range(self.num_screenshots):
      # Find reference image.
      shotname = self.screenshot_name + str(screenshot_no + 1)
      J = os.path.join
      platform_img_path = J(self.ref_dir,
                            selenium_constants.PLATFORM_SCREENSHOT_DIR,
                            shotname + '_reference.png')
      reg_img_path = J(self.ref_dir,
                       selenium_constants.DEFAULT_SCREENSHOT_DIR,
                       shotname + '_reference.png')

      if os.path.exists(platform_img_path):
        ref_img_path = platform_img_path
      elif os.path.exists(reg_img_path):
        ref_img_path = reg_img_path
      else:
        self.fail('Reference image for ' + shotname + ' not found.')
  
      # Find generated image.
      gen_img_path = J(self.gen_dir, shotname + '.png')
      diff_img_path = J(self.gen_dir, 'cmp_' + shotname + '.png')
       
      self.assertTrue(os.path.exists(gen_img_path),
                      'Generated screenshot for ' + shotname + ' not found.\n')

      # Run perceptual diff
      arguments = [self.pdiff_path,
                   ref_img_path,
                   gen_img_path,
                   "-output", diff_img_path,
                   "-fov", "45",
                   "-alphaThreshold", alpha_threshold,
                   # Turn on verbose output for the percetual diff so we
                   # can see how far off we are on the threshold.
                   "-verbose",
                   # Set the threshold to zero so we can get a count
                   # of the different pixels.  This causes the program
                   # to return failure for most images, but we can compare
                   # the values ourselves below.
                   "-threshold", "0"]
      if use_colorfactor:
        arguments += ["-colorfactor", colorfactor]
      if use_downsample:
        arguments += ["-downsample", downsample_factor]
      if use_edge:
        arguments += ["-ignoreEdges", edge_threshold]

      pdiff_pipe = subprocess.Popen(arguments,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
      (pdiff_stdout, pdiff_stderr) = pdiff_pipe.communicate()
      result = pdiff_pipe.returncode
      # Find out how many pixels were different by looking at the output.
      pixel_re = re.compile("(\d+) pixels are different", re.DOTALL)
      pixel_match = pixel_re.search(pdiff_stdout)
      different_pixels = "0"
      if pixel_match:
        different_pixels = pixel_match.group(1)

      results += [(shotname, int(different_pixels))]

    all_tests_passed = True
    msg = "Pixel threshold is %s. Failing screenshots:\n" % pixel_threshold
    for name, pixels in results:
      if pixels >= int(pixel_threshold):
        all_tests_passed = False
        msg += "     %s, differing by %s\n" % (name, str(pixels))

    self.assertTrue(all_tests_passed, msg)

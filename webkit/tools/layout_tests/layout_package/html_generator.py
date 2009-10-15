# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from failure import Failure

CHROMIUM_BUG_URL = "http://crbug.com/"

def ExtractFirstValue(string, regex):
  m = re.search(regex, string)
  if m and m.group(1):
    return m.group(1)
  return None

# TODO(gwilson): Refactor HTML generation into a HTML templating system like
# Django templates.
class HTMLGenerator(object):

  def __init__(self, failures, output_dir, build, platform,
               exclude_known_failures):
    self.failures = failures
    self.output_dir = output_dir
    self.build = build
    self.platform = platform
    self.exclude_known_failures = exclude_known_failures
    self.image_size = "200px"

  def GenerateHTML(self):
    html = ""
    html += """
            <html>
              <head>
                <style>
                  body {
                    font-family: sans-serif;
                  }
                  h2 {
                  }
                  .mainTable {
                    background: #666666;
                  }
                  .mainTable td , .mainTable th {
                    background: white;
                  }
                  .titlelink {
                    font-size: 18pt;
                    font-weight: bold;
                  }
                  .detail {
                    margin-left: 10px;
                    margin-top: 3px;
                  }
                </style>
              </head>
              <body>
            """
    title = "All failures"
    if self.exclude_known_failures:
      title = "Regressions"

    html += """
            <h1>%s for build %s (%s)</h1>
            """ % (title, self.build, self.platform)

    test_number = 0

    # TODO(gwilson): Refactor this to do a join() on an array of HTML, rather
    # than appending strings in a loop.
    for failure in self.failures:
      test_number += 1
      html += """
              <table style="border: 1px solid black; width: 1200px;
                -webkit-border-radius: 5px;" cellspacing="0" cellpadding="4">
                <tr>
                  <td style="background-color: #CDECDE;
                    border-bottom: 1px solid black;">
                    <span class="titlelink">%s.&nbsp;&nbsp;%s</span></td></tr>
                <tr><td>&nbsp;&nbsp;Last modified: <a href="%s">%s</a>
              """ % (test_number,
                     failure.test_path,
                     failure.GetTestHome(),
                     failure.test_age)
      html += "<div class='detail'>"
      html += "<pre>%s</pre>" % \
              (self._GenerateLinkifiedTextExpectations(failure))

      html += self._GenerateFlakinessHTML(failure)

      if failure.crashed:
        html += "<div>Test <b>CRASHED</b></div>"
      elif failure.timeout:
        html += "<div>Test <b>TIMED OUT</b></div>"
      else:
        html += """
                <table class="mainTable" cellspacing=1 cellpadding=5>
                  <tr>
                    <th width='250'>&nbsp;</th>
                    <th width='200'>Expected</th>
                    <th width='200'>Actual</th>
                    <th width='200'>Difference</th>
                    <th width='200'>Upstream</th>
                  </tr>
                """

        if failure.text_diff_mismatch:
          html += self._GenerateTextFailureHTML(failure)

        if failure.image_mismatch:
          html += self._GenerateImageFailureHTML(failure)

        html += "</table>"
      html += "</div></td></tr></table><br>"
    html += """</body></html>"""

    # TODO(gwilson): Change this filename to be passed in as an argument.
    html_filename = "%s/index-%s.html" % (self.output_dir, self.build)
    htmlFile = open(html_filename, 'w')
    htmlFile.write(html)
    htmlFile.close()
    return html_filename

  def _GenerateLinkifiedTextExpectations(self, failure):
    if not failure.test_expectations_line:
      return ""
    bug_number = ExtractFirstValue(failure.test_expectations_line, "BUG(\d+)")
    if not bug_number or bug_number == "":
      return ""
    return failure.test_expectations_line.replace(
      "BUG" + bug_number,
      "<a href='%s%s'>BUG%s</a>" % (CHROMIUM_BUG_URL, bug_number, bug_number))

  # TODO(gwilson): Fix this so that it shows the last ten runs
  # not just a "meter" of flakiness.
  def _GenerateFlakinessHTML(self, failure):
    html = ""
    if not failure.flakiness:
      return html
    html += """
            <table>
              <tr>
                <td>Flakiness: (%s)</td>
            """ % (failure.flakiness)

    flaky_red = int(round(int(failure.flakiness) / 5))
    flaky_green = 10 - flaky_red
    for i in range(0, flaky_green):
      html += """
              <td style="background: green">&nbsp;&nbsp;</td>
              """
    for i in range(0, flaky_red):
      html += """
              <td style="background: red">&nbsp;&nbsp;</td>
              """
    html += """
              </tr>
            </table><br>
            """
    return html

  def _GenerateTextFailureHTML(self, failure):
    html = ""
    if not failure.GetTextBaselineLocation():
      return """<tr><td colspan='5'>This test likely does not have any
                TEXT baseline for this platform, or one could not
                be found.</td></tr>"""
    html += """
              <tr>
                <td>
                  <b><a href="%s">Render Tree Dump</a></b><br>
                  <b>%s</b> baseline<br>
                  Age: %s<br>
                </td>
            """ % (failure.text_baseline_url,
                   failure.GetTextBaselineLocation(),
                   failure.text_baseline_age)
    html += self._GenerateTextFailureTD(failure.GetExpectedTextFilename(),
                                        "expected text")
    html += self._GenerateTextFailureTD(failure.GetActualTextFilename(),
                                        "actual text")
    html += self._GenerateTextFailureTD(failure.GetTextDiffFilename(),
                                        "text diff")
    html += "<td>&nbsp;</td>"
    html += "</tr>"
    return html

  def _GenerateTextFailureTD(self, file_path, anchor_text):
    return ("<td align=center>"
            "<a href='./layout-test-results-%s/%s'>%s</a></td>") % (self.build,
                                                                    file_path,
                                                                    anchor_text)

  def _GenerateImageFailureHTML(self, failure):
    if not failure.GetImageBaselineLocation():
      return """<tr><td colspan='5'>This test likely does not have any
                IMAGE baseline for this platform, or one could not be found.
                </td></tr>"""
    html = \
      """
      <tr>
        <td><b><a href="%s">Pixel Dump</a></b><br>
        <b>%s</b> baseline<br>Age: %s</td>
      """ % (failure.image_baseline_url,
             failure.GetImageBaselineLocation(),
             failure.image_baseline_age)
    html += self._GenerateImageFailureTD(failure.GetExpectedImageFilename())
    html += self._GenerateImageFailureTD(failure.GetActualImageFilename())
    html += self._GenerateImageFailureTD(failure.GetImageDiffFilename())
    if (failure.image_baseline_upstream_local and
        failure.image_baseline_upstream_local != ""):
      html += self._GenerateImageFailureTD(failure.GetImageUpstreamFilename())
    else:
      html += """
              <td>&nbsp;</td>
              """
    html += "</tr>"
    return html

  def _GenerateImageFailureTD(self, filename):
    return ("<td><a href='./layout-test-results-%s/%s'>"
            "<img style='width: %s' src='./layout-test-results-%s/%s' />"
            "</a></td>") % (self.build, filename, self.image_size,
                            self.build, filename)

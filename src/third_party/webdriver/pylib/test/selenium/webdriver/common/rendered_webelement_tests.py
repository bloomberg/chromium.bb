# Copyright 2008-2009 WebDriver committers
# Copyright 2008-2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License")
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import unittest
import pytest

from selenium.webdriver.common.by import By


class RenderedWebElementTests(unittest.TestCase):

    @pytest.mark.ignore_chrome
    def testShouldPickUpStyleOfAnElement(self):
        self._loadPage("javascriptPage")

        element = self.driver.find_element(by=By.ID, value="green-parent")
        backgroundColour = element.value_of_css_property("background-color")

        self.assertEqual("rgba(0, 128, 0, 1)", backgroundColour)

        element = self.driver.find_element(by=By.ID, value="red-item")
        backgroundColour = element.value_of_css_property("background-color")

        self.assertEqual("rgba(255, 0, 0, 1)", backgroundColour)

    @pytest.mark.ignore_chrome
    def testShouldAllowInheritedStylesToBeUsed(self):
        self._loadPage("javascriptPage")

        element = self.driver.find_element(by=By.ID, value="green-item")
        backgroundColour = element.value_of_css_property("background-color")

        self.assertEqual("transparent", backgroundColour)
  

    def testShouldCorrectlyIdentifyThatAnElementHasWidth(self):
        self._loadPage("xhtmlTest")

        shrinko =  self.driver.find_element(by=By.ID, value="linkId")
        size = shrinko.size
        self.assertTrue(size["width"] > 0, "Width expected to be greater than 0")
        self.assertTrue(size["height"] > 0, "Height expected to be greater than 0")
  
    def _pageURL(self, name):
        return "http://localhost:%d/%s.html" % (self.webserver.port, name)

    def _loadSimplePage(self):
        self._loadPage("simpleTest")

    def _loadPage(self, name):
        self.driver.get(self._pageURL(name))


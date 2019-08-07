#!/usr/bin/python

# Copyright 2008-2010 WebDriver committers
# Copyright 2008-2010 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License")
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS.
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import unittest
from selenium.webdriver.common.by import By
from selenium.common.exceptions import StaleElementReferenceException


class StaleReferenceTests(unittest.TestCase):

    def testOldPage(self):
        self._loadSimplePage()
        elem = self.driver.find_element(by=By.ID, value="links")
        self._loadPage("xhtmlTest")
        try:
            elem.click()
            self.fail("Should Throw a StaleElementReferenceException but didnt")
        except StaleElementReferenceException,e:
            pass

    def testShouldNotCrashWhenCallingGetSizeOnAnObsoleteElement(self):
        self._loadSimplePage()
        elem = self.driver.find_element(by=By.ID, value="links")
        self._loadPage("xhtmlTest")
        try: 
            elem.size
            self.fail("Should Throw a StaleElementReferenceException but didnt")
        except StaleElementReferenceException,e:
            pass

    def testShouldNotCrashWhenQueryingTheAttributeOfAStaleElement(self):
        self._loadPage("xhtmlTest")
        heading = self.driver.find_element(by=By.XPATH, value="//h1")
        self._loadSimplePage()
        try:
            heading.get_attribute("class")
            self.fail("Should Throw a StaleElementReferenceException but didnt")
        except StaleElementReferenceException, e:
            pass

    def _pageURL(self, name):
        return "http://localhost:%d/%s.html" % (self.webserver.port, name)
    
    def _loadSimplePage(self):
        self._loadPage("simpleTest")

    def _loadPage(self, name):
        self.driver.get(self._pageURL(name))

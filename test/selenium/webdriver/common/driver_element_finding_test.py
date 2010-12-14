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
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import os
import re
import tempfile
import time
import shutil
import unittest
from selenium.webdriver.common.exceptions import NoSuchElementException
from selenium.webdriver.common.exceptions import NoSuchFrameException


class DriverElementFindingTests(unittest.TestCase):

    def testShouldFindElementById(self):
        self._loadSimplePage()
        e = self.driver.find_element_by_id("oneline")
        self.assertEqual("A single line of text", e.get_text())
        

    def testShouldFindElementByLinkText(self):
        self._loadSimplePage()
        e = self.driver.find_element_by_link_text("link with leading space")
        self.assertEqual("link with leading space", e.get_text())
        

    def testShouldFindElementByName(self):
        self._loadPage("nestedElements")
        e = self.driver.find_element_by_name("div1")
        self.assertEqual("hello world hello world", e.get_text())
        
    def testShouldFindElementByXPath(self):
        self._loadSimplePage()
        e = self.driver.find_element_by_xpath("/html/body/p[1]")
        self.assertEqual("A single line of text", e.get_text())
        
    def testShouldFindElementByClassName(self):
        self._loadPage("nestedElements")
        e = self.driver.find_element_by_class_name("one")
        self.assertEqual("Span with class of one", e.get_text())
        
    def testShouldFindElementByPartialLinkText(self):
        self._loadSimplePage()
        e = self.driver.find_element_by_partial_link_text("leading space")
        self.assertEqual("link with leading space", e.get_text())
        
    def testShouldFindElementByTagName(self):
        self._loadSimplePage()
        e = self.driver.find_element_by_tag_name("H1")
        self.assertEqual("Heading", e.get_text())
        
    def testShouldFindElementsById(self):    
        self._loadPage("nestedElements")
        elements = self.driver.find_elements_by_id("test_id")
        self.assertEqual(2, len(elements))
        
    def testShouldFindElementsByLinkText(self):
        self._loadPage("nestedElements")
        elements = self.driver.find_elements_by_link_text("hello world")
        self.assertEqual(12, len(elements))
        
    def testShouldFindElementsByName(self):
        self._loadPage("nestedElements")
        elements = self.driver.find_elements_by_name("form1")
        self.assertEqual(4, len(elements))
        
    def testShouldFindElementsByXPath(self):
        self._loadPage("nestedElements")
        elements = self.driver.find_elements_by_xpath("//a")
        self.assertEqual(12, len(elements))
        
    def testShouldFindElementsByClassName(self):
        self._loadPage("nestedElements")
        elements = self.driver.find_elements_by_class_name("one")
        self.assertEqual(3, len(elements))
        
    def testShouldFindElementsByPartialLinkText(self):
        self._loadPage("nestedElements")
        elements = self.driver.find_elements_by_partial_link_text("world")
        self.assertEqual(12, len(elements))
        
    def testShouldFindElementsByTagName(self):
        self._loadPage("nestedElements")
        elements = self.driver.find_elements_by_tag_name("a")
        self.assertEqual(12, len(elements))
    
    def _pageURL(self, name):
        return "http://localhost:%d/%s.html" % (self.webserver.port, name)
 
    def _loadSimplePage(self):
        self._loadPage("simpleTest")

    def _loadPage(self, name):
        self.driver.get(self._pageURL(name))

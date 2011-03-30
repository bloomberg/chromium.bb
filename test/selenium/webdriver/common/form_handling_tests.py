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


import os
import re
import tempfile
import time
import shutil
import unittest
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import NoSuchFrameException
from selenium.common.exceptions import InvalidElementStateException
from selenium.common.exceptions import ElementNotSelectableException

from selenium.common.exceptions import WebDriverException 


class FormHandlingTests(unittest.TestCase):

    def testShouldClickOnSubmitInputElements(self):
        self._loadPage("formPage")
        self.driver.find_element_by_id("submitButton").click()
        self.driver.implicitly_wait(5)
        self.assertEqual(self.driver.title, "We Arrive Here")
        
    def testClickingOnUnclickableElementsDoesNothing(self):    
        self._loadPage("formPage")
        self.driver.find_element_by_xpath("//body").click()
        
    def testShouldBeAbleToClickImageButtons(self): 
        self._loadPage("formPage")
        self.driver.find_element_by_id("imageButton").click()
        self.driver.implicitly_wait(5)
        self.assertEqual(self.driver.title, "We Arrive Here")
        
    def testShouldBeAbleToSubmitForms(self):
        self._loadPage("formPage")
        self.driver.find_element_by_name("login").submit()
        self.driver.implicitly_wait(5)
        self.assertEqual(self.driver.title, "We Arrive Here")
        
    def testShouldSubmitAFormWhenAnyInputElementWithinThatFormIsSubmitted(self):
        self._loadPage("formPage")
        self.driver.find_element_by_id("checky").submit()
        self.driver.implicitly_wait(5)
        self.assertEqual(self.driver.title, "We Arrive Here")
        
    def testShouldSubmitAFormWhenAnyElementWihinThatFormIsSubmitted(self):
        self._loadPage("formPage")
        self.driver.find_element_by_xpath("//form/p").submit()
        self.driver.implicitly_wait(5)
        self.assertEqual(self.driver.title, "We Arrive Here")
        
    def testShouldNotBeAbleToSubmitAFormThatDoesNotExist(self):
        self._loadPage("formPage")
        try:
            self.driver.find_element_by_name("there is no spoon").submit()
            self.fail("Expected NoSuchElementException to have been thrown")
        except NoSuchElementException, e:
            pass
        except Exception, e:
            self.fail("Expected NoSuchElementException but got " + str(e))
        
    def testShouldBeAbleToEnterTextIntoATextAreaBySettingItsValue(self):
        self._loadPage("javascriptPage")
        textarea = self.driver.find_element_by_id("keyUpArea")
        cheesey = "Brie and cheddar"
        textarea.send_keys(cheesey)
        self.assertEqual(textarea.get_attribute("value"), cheesey)
        
    def testShouldEnterDataIntoFormFields(self):
        self._loadPage("xhtmlTest")
        element = self.driver.find_element_by_xpath("//form[@name='someForm']/input[@id='username']")
        originalValue = element.get_attribute("value")
        self.assertEqual(originalValue, "change")

        element.clear()
        element.send_keys("some text")

        element = self.driver.find_element_by_xpath("//form[@name='someForm']/input[@id='username']")
        newFormValue = element.get_attribute("value")
        self.assertEqual(newFormValue, "some text")
        
    def testShouldBeAbleToSelectACheckBox(self):
        self._loadPage("formPage")
        checkbox = self.driver.find_element_by_id("checky")
        self.assertEqual(checkbox.is_selected(), False)
        checkbox.select()
        self.assertEqual(checkbox.is_selected(), True)
        checkbox.select()
        self.assertEqual(checkbox.is_selected(), True)
        
    def testShouldToggleTheCheckedStateOfACheckbox(self):
        self._loadPage("formPage")
        checkbox = self.driver.find_element_by_id("checky")
        self.assertEqual(checkbox.is_selected(), False)
        checkbox.toggle()
        self.assertEqual(checkbox.is_selected(), True)
        checkbox.toggle()
        self.assertEqual(checkbox.is_selected(), False)
        
    def testTogglingACheckboxShouldReturnItsCurrentState(self):
        self._loadPage("formPage")
        checkbox = self.driver.find_element_by_id("checky")
        self.assertEqual(checkbox.is_selected(), False)
        isChecked = checkbox.toggle()
        self.assertEqual(isChecked, True)
        isChecked = checkbox.toggle()
        self.assertEqual(isChecked, False)
        
    def testShouldNotBeAbleToSelectSomethingThatIsDisabled(self):
        self._loadPage("formPage")
        radioButton = self.driver.find_element_by_id("nothing")
        self.assertEqual(radioButton.is_enabled(), False)
        try:
            radioButton.select()
            self.fail("Expected WebDriverException but didnt get it")
        except WebDriverException, e:
            pass
        except Exception, e:
            self.fail("Expected WebDriverException but got " + str(type(e)))
        
    def testShouldBeAbleToSelectARadioButton(self):
        self._loadPage("formPage")
        radioButton = self.driver.find_element_by_id("peas")
        self.assertEqual(radioButton.is_selected(), False)
        radioButton.select()
        self.assertEqual(radioButton.is_selected(), True)
        
    def testShouldBeAbleToSelectARadioButtonByClickingOnIt(self):
        self._loadPage("formPage")
        radioButton = self.driver.find_element_by_id("peas")
        self.assertEqual(radioButton.is_selected(), False)
        radioButton.click()
        self.assertEqual(radioButton.is_selected(), True)
        
    def testShouldReturnStateOfRadioButtonsBeforeInteration(self):
        self._loadPage("formPage")
        radioButton = self.driver.find_element_by_id("cheese_and_peas")
        self.assertEqual(radioButton.is_selected(), True)

        radioButton = self.driver.find_element_by_id("cheese")
        self.assertEqual(radioButton.is_selected(), False)
        
     #   [ExpectedException(typeof(NotImplementedException))]
    #def testShouldThrowAnExceptionWhenTogglingTheStateOfARadioButton(self):
    #    self._loadPage("formPage")
    #    radioButton = self.driver.find_element_by_id("cheese"))
    #    radioButton.toggle()
        

        
    #    [IgnoreBrowser(Browser.IE, "IE allows toggling of an option not in a multiselect")]
    #    [ExpectedException(typeof(NotImplementedException))]
    #def testTogglingAnOptionShouldThrowAnExceptionIfTheOptionIsNotInAMultiSelect(self):
    #    self._loadPage("formPage")
    #
    #    select = self.driver.find_element_by_name("selectomatic"))
    #    option = select.find_elements_by_tag_name("option"))[0]
    #    option.toggle()
        
    def testTogglingAnOptionShouldToggleOptionsInAMultiSelect(self):    
        self._loadPage("formPage")

        select = self.driver.find_element_by_name("multi")
        option = select.find_elements_by_tag_name("option")[0]

        selected = option.is_selected()
        current = option.toggle()
        self.assertFalse(selected == current)

        current = option.toggle()
        self.assertTrue(selected == current)
        
    def testShouldThrowAnExceptionWhenSelectingAnUnselectableElement(self):    
        self._loadPage("formPage")
    
        element = self.driver.find_element_by_xpath("//title")
        try:
            element.select()
            self.fail("Expected WebDriverException to have been thrown")
        except WebDriverException, e:
            pass
        except Exception, e:
            self.fail("Expected WebDriverException but got " + str(type(e)))
        
    def testSendingKeyboardEventsShouldAppendTextInInputs(self):    
        self._loadPage("formPage")
        element = self.driver.find_element_by_id("working")
        element.send_keys("Some")
        value = element.get_attribute("value")
        self.assertEqual(value, "Some")

        element.send_keys(" text")
        value = element.get_attribute("value")
        self.assertEqual(value, "Some text")

    def testShouldBeAbleToClearTextFromInputElements(self):
        self._loadPage("formPage")
        element = self.driver.find_element_by_id("working")
        element.send_keys("Some text")
        value = element.get_attribute("value")
        self.assertTrue(len(value) > 0)

        element.clear()
        value = element.get_attribute("value")

        self.assertEqual(len(value), 0)

    def testEmptyTextBoxesShouldReturnAnEmptyStringNotNull(self):
        self._loadPage("formPage")
        emptyTextBox = self.driver.find_element_by_id("working")
        self.assertEqual(emptyTextBox.get_attribute("value"), "")

        emptyTextArea = self.driver.find_element_by_id("emptyTextArea")
        self.assertEqual(emptyTextBox.get_attribute("value"), "")
        
    def testShouldBeAbleToClearTextFromTextAreas(self):
        self._loadPage("formPage")
        element = self.driver.find_element_by_id("withText")
        element.send_keys("Some text")
        value = element.get_attribute("value")
        self.assertTrue(len(value) > 0)

        element.clear()
        value = element.get_attribute("value")

        self.assertEqual(len(value), 0)
        
    def testRadioShouldNotBeSelectedAfterSelectingSibling(self):        
        self._loadPage("formPage")
        cheese = self.driver.find_element_by_id("cheese")
        peas = self.driver.find_element_by_id("peas")

        cheese.select()

        self.assertEqual(True, cheese.is_selected())
        self.assertEqual(False, peas.is_selected())

        peas.select()

        self.assertEqual(False, cheese.is_selected())
        self.assertEqual(True, peas.is_selected())
        
    def _pageURL(self, name):
        return "http://localhost:%d/%s.html" % (self.webserver.port, name)
    
    def _loadSimplePage(self):
        self._loadPage("simpleTest")

    def _loadPage(self, name):
        self.driver.get(self._pageURL(name))

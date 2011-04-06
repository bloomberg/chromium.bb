# Copyright 2008-2009 WebDriver committers
# Copyright 2008-2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
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

"""WebElement implementation."""
from command import Command
import warnings
from selenium.webdriver.common.by import By
from selenium.common.exceptions import NoSuchAttributeException


class WebElement(object):
    """Represents an HTML element.

    Generally, all interesting operations to do with interacting with a page
    will be performed through this interface."""
    def __init__(self, parent, id_):
        self._parent = parent
        self._id = id_

    @property
    def tag_name(self):
        """Gets this element's tagName property."""
        return self._execute(Command.GET_ELEMENT_TAG_NAME)['value']

    @property
    def text(self):
        """Gets the text of the element."""
        return self._execute(Command.GET_ELEMENT_TEXT)['value']

    def click(self):
        """Clicks the element."""
        self._execute(Command.CLICK_ELEMENT)

    def submit(self):
        """Submits a form."""
        self._execute(Command.SUBMIT_ELEMENT)

    @property
    def value(self):
        """Gets the value of the element's value attribute. Note that this call has been
            deprecated and will be removed in a future version
        """
        warnings.warn("value property has been deprecated, please use get_attribute('value')")
        return self._execute(Command.GET_ELEMENT_VALUE)['value']

    def clear(self):
        """Clears the text if it's a text entry element."""
        self._execute(Command.CLEAR_ELEMENT)

    def get_attribute(self, name):
        """Gets the attribute value."""
        resp = self._execute(Command.GET_ELEMENT_ATTRIBUTE, {'name': name})
        attributeValue = ''
        if resp['value'] is None:
            attributeValue = None
        else:
            attributeValue = str(resp['value'])
            if type(resp['value']) is bool:
                attributeValue = attributeValue.lower()

        return attributeValue

    def toggle(self):
        """Toggles the element state."""
        resp = self._execute(Command.TOGGLE_ELEMENT)
        return resp['value']

    def is_selected(self):
        """Whether the element is selected."""
        return self._execute(Command.IS_ELEMENT_SELECTED)['value']

    def select(self):
        """Selects an element."""
        self._execute(Command.SET_ELEMENT_SELECTED)

    def is_enabled(self):
        """Whether the element is enabled."""
        return self._execute(Command.IS_ELEMENT_ENABLED)['value']

    def find_element_by_id(self, id_):
        """Finds element by id."""
        return self.find_element(by=By.ID, value=id_)

    def find_elements_by_id(self, id_):
        return self.find_elements(by=By.ID, value=id_)

    def find_element_by_name(self, name):
        """Find element by name."""
        return self.find_element(by=By.NAME, value=name)

    def find_elements_by_name(self, name):
        return self.find_elements(by=By.NAME, value=name)

    def find_element_by_link_text(self, link_text):
        """Finds element by link text."""
        return self.find_element(by=By.LINK_TEXT, value=link_text)

    def find_elements_by_link_text(self, link_text):
        return self.find_elements(by=By.LINK_TEXT, value=link_text)

    def find_element_by_partial_link_text(self, link_text):
        return self.find_element(by=By.PARTIAL_LINK_TEXT, value=link_text)

    def find_elements_by_partial_link_text(self, link_text):
        return self.find_elements(by=By.PARTIAL_LINK_TEXT, value=link_text)

    def find_element_by_tag_name(self, name):
        return self.find_element(by=By.TAG_NAME, value=name)

    def find_elements_by_tag_name(self, name):
        return self.find_elements(by=By.TAG_NAME, value=name)

    def find_element_by_xpath(self, xpath):
        """Finds element by xpath."""
        return self.find_element(by=By.XPATH, value=xpath)

    def find_elements_by_xpath(self, xpath):
        """Finds elements within the elements by xpath."""
        return self.find_elements(by=By.XPATH, value=xpath)
    
    def find_element_by_class_name(self, name):
        """Finds an element by their class name."""
        return self.find_element(by=By.CLASS_NAME, value=name)

    def find_elements_by_class_name(self, name):
        """Finds elements by their class name."""
        return self.find_elements(by=By.CLASS_NAME, value=name)

    def find_element_by_css_selector(self, css_selector):
        """Find and return an element by CSS selector."""
        return self.find_element(by=By.CSS_SELECTOR, value=css_selector)
    
    def find_elements_by_css_selector(self, css_selector):
        """Find and return list of multiple elements by CSS selector."""
        return self.find_elements(by=By.CSS_SELECTOR, value=css_selector)

    def send_keys(self, *value):
        """Simulates typing into the element."""
        self._execute(Command.SEND_KEYS_TO_ELEMENT, {'value': value})

    # RenderedWebElement Items
    def is_displayed(self):
        """Whether the element would be visible to a user"""
        return self._execute(Command.IS_ELEMENT_DISPLAYED)['value']

    @property
    def size(self):
        """ Returns the size of the element """
        size = self._execute(Command.GET_ELEMENT_SIZE)['value']
        new_size = {}
        new_size["height"] = size["height"]
        new_size["width"] = size["width"]
        return new_size

    def value_of_css_property(self, property_name):
        """ Returns the value of a CSS property """
        return self._execute(Command.GET_ELEMENT_VALUE_OF_CSS_PROPERTY,
                        {'propertyName': property_name})['value']

    @property
    def location(self):
        """ Returns the location of the element in the renderable canvas"""
        return self._execute(Command.GET_ELEMENT_LOCATION)['value']

    @property
    def parent(self):
        return self._parent

    @property
    def id(self):
        return self._id

    # Private Methods
    def _execute(self, command, params=None):
        """Executes a command against the underlying HTML element.

        Args:
          command: The name of the command to _execute as a string.
          params: A dictionary of named parameters to send with the command.

        Returns:
          The command's JSON response loaded into a dictionary object.
        """
        if not params:
            params = {}
        params['id'] = self._id
        return self._parent.execute(command, params)

    def find_element(self, by=By.ID, value=None):
        return self._execute(Command.FIND_CHILD_ELEMENT,
                             {"using": by, "value": value})['value']

    def find_elements(self, by=By.ID, value=None):
        return self._execute(Command.FIND_CHILD_ELEMENTS,
                             {"using": by, "value": value})['value']

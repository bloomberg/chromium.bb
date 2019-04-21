#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the UKM XML file is well-formatted."""

import sys
from xml_validations import UkmXmlValidation
from xml.dom import minidom

UKM_XML = 'ukm.xml'


def main():
  with open(UKM_XML, 'r') as config_file:
    document = minidom.parse(config_file)
    [config] = document.getElementsByTagName('ukm-configuration')
    validator = UkmXmlValidation(config)

    ownerCheckSuccess, ownerCheckErrors = validator.checkEventsHaveOwners()

    if not ownerCheckSuccess:
      return ownerCheckErrors


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import unicode_literals

import copy
import json
import unittest

import model

PUBLIC_XML_SNIPPET = """
<rappor-configuration>

<noise-levels>
<noise-level name="NO_NOISE">
  <summary>
    NO_NOISE description.
  </summary>
  <noise-values fake-prob="0.0" fake-one-prob="0.0" one-coin-prob="1.0"
      zero-coin-prob="0.0"/>
</noise-level>

<noise-level name="NORMAL_NOISE">
  <summary>
    NORMAL_NOISE description.
  </summary>
  <noise-values fake-prob="0.5" fake-one-prob="0.5" one-coin-prob="0.75"
      zero-coin-prob="0.25"/>
</noise-level>
</noise-levels>

<rappor-parameter-types>
<rappor-parameters name="ETLD_PLUS_ONE">
  <summary>
    ETLD+1 parameters.
  </summary>
  <parameters num-cohorts="128"
              bytes="16"
              hash-functions="2"
              reporting-level="FINE"
              noise-level="NORMAL_NOISE"/>
</rappor-parameters>
<rappor-parameters name="SAFEBROWSING_RAPPOR_TYPE">
  <summary>
    SAFEBROWSING parameters.
  </summary>
  <parameters num-cohorts="128"
              bytes="1"
              hash-functions="2"
              reporting-level="COARSE"
              noise-level="NORMAL_NOISE"/>
</rappor-parameters>
</rappor-parameter-types>

<rappor-metrics>

<rappor-metric name="MultiD.TestMetric" type="SAFEBROWSING_RAPPOR_TYPE">
  <owner>holte@chromium.org</owner>
  <summary>
    Metric Summary.
  </summary>
  <string-field name="domain">
    <summary>
      Domain Summary.
    </summary>
  </string-field>
  <flags-field name="flags">
    <flag>Bit 0</flag>
    <flag label="MyBit" bit="1">What it means</flag>
    <summary>
      Flags Summary.
    </summary>
  </flags-field>
</rappor-metric>

<rappor-metric name="Search.DefaultSearchProvider" type="ETLD_PLUS_ONE">
  <owner>
    holte@chromium.org
  </owner>
  <summary>

    BLAH BLAH  BLAH
  </summary>
</rappor-metric>
</rappor-metrics>
</rappor-configuration>
"""

PARSED_XML = {
    model.models.COMMENT_KEY: [],
    'noiseLevels': {
        model.models.COMMENT_KEY: [],
        'levels': [
            {
                model.models.COMMENT_KEY: [],
                'name': 'NO_NOISE',
                'summary': 'NO_NOISE description.',
                'values': {
                    model.models.COMMENT_KEY: [],
                    'fake-prob': 0.0,
                    'fake-one-prob': 0.0,
                    'one-coin-prob': 1.0,
                    'zero-coin-prob': 0.0,
                },
            },
            {
                model.models.COMMENT_KEY: [],
                'name': 'NORMAL_NOISE',
                'summary': 'NORMAL_NOISE description.',
                'values': {
                    model.models.COMMENT_KEY: [],
                    'fake-prob': 0.5,
                    'fake-one-prob': 0.5,
                    'one-coin-prob': 0.75,
                    'zero-coin-prob': 0.25,
                },
            },
        ]
    },
    'parameterTypes': {
        model.models.COMMENT_KEY: [],
        'types': [
            {
                model.models.COMMENT_KEY: [],
                'name': 'ETLD_PLUS_ONE',
                'summary': 'ETLD+1 parameters.',
                'parameters': {
                    model.models.COMMENT_KEY: [],
                    'num-cohorts': 128,
                    'bytes': 16,
                    'hash-functions': 2,
                    'reporting-level': 'FINE',
                    'noise-level': 'NORMAL_NOISE',
                },
            },
            {
                model.models.COMMENT_KEY: [],
                'name': 'SAFEBROWSING_RAPPOR_TYPE',
                'summary': 'SAFEBROWSING parameters.',
                'parameters': {
                    model.models.COMMENT_KEY: [],
                    'num-cohorts': 128,
                    'bytes': 1,
                    'hash-functions': 2,
                    'reporting-level': 'COARSE',
                    'noise-level': 'NORMAL_NOISE',
                },
            },
        ]
    },
    'metrics': {
        model.models.COMMENT_KEY: [],
        'metrics': [{
            model.models.COMMENT_KEY: [],
            'name':
                'MultiD.TestMetric',
            'flags': [{
                model.models.COMMENT_KEY: [],
                'name':
                    'flags',
                'summary':
                    'Flags Summary.',
                'flags': [
                    {
                        model.models.COMMENT_KEY: [],
                        'summary': 'Bit 0'
                    },
                    {
                        model.models.COMMENT_KEY: [],
                        'summary': 'What it means',
                        'label': 'MyBit',
                        'bit': 1,
                    },
                ],
            }],
            'owners': ['holte@chromium.org'],
            'strings': [{
                model.models.COMMENT_KEY: [],
                'name': 'domain',
                'summary': 'Domain Summary.',
            }],
            'summary':
                'Metric Summary.',
            'type':
                'SAFEBROWSING_RAPPOR_TYPE',
            'uint64': [],
        }, {
            model.models.COMMENT_KEY: [],
            'name': 'Search.DefaultSearchProvider',
            'flags': [],
            'owners': ['holte@chromium.org'],
            'strings': [],
            'summary': 'BLAH BLAH  BLAH',
            'type': 'ETLD_PLUS_ONE',
            'uint64': [],
        }]
    }
}

PRETTY_PRINTED_XML = """<rappor-configuration>
<noise-levels>
<noise-level name="NO_NOISE">
  <summary>
    NO_NOISE description.
  </summary>
  <noise-values fake-prob="0.0" fake-one-prob="0.0" one-coin-prob="1.0"
      zero-coin-prob="0.0"/>
</noise-level>

<noise-level name="NORMAL_NOISE">
  <summary>
    NORMAL_NOISE description.
  </summary>
  <noise-values fake-prob="0.5" fake-one-prob="0.5" one-coin-prob="0.75"
      zero-coin-prob="0.25"/>
</noise-level>

</noise-levels>

<rappor-parameter-types>
<rappor-parameters name="ETLD_PLUS_ONE">
  <summary>
    ETLD+1 parameters.
  </summary>
  <parameters num-cohorts="128" bytes="16" hash-functions="2"
      reporting-level="FINE" noise-level="NORMAL_NOISE"/>
</rappor-parameters>

<rappor-parameters name="SAFEBROWSING_RAPPOR_TYPE">
  <summary>
    SAFEBROWSING parameters.
  </summary>
  <parameters num-cohorts="128" bytes="1" hash-functions="2"
      reporting-level="COARSE" noise-level="NORMAL_NOISE"/>
</rappor-parameters>

</rappor-parameter-types>

<rappor-metrics>
<rappor-metric name="MultiD.TestMetric" type="SAFEBROWSING_RAPPOR_TYPE">
  <owner>holte@chromium.org</owner>
  <summary>
    Metric Summary.
  </summary>
  <string-field name="domain">
    <summary>
      Domain Summary.
    </summary>
  </string-field>
  <flags-field name="flags">
    <flag bit="0" label="Bit 0"/>
    <flag bit="1" label="MyBit">What it means</flag>
    <summary>
      Flags Summary.
    </summary>
  </flags-field>
</rappor-metric>

<rappor-metric name="Search.DefaultSearchProvider" type="ETLD_PLUS_ONE">
  <owner>holte@chromium.org</owner>
  <summary>
    BLAH BLAH  BLAH
  </summary>
</rappor-metric>

</rappor-metrics>

</rappor-configuration>
"""


class RapporModelTest(unittest.TestCase):

  def testParse(self):
    parsed = model.RAPPOR_XML_TYPE.Parse(PUBLIC_XML_SNIPPET)
    self.assertEqual(PARSED_XML, parsed)

  def testUpdate(self):
    updated = model.UpdateXML(PUBLIC_XML_SNIPPET)
    # Compare list of lines for nicer diff on errors.
    self.assertEqual(PRETTY_PRINTED_XML.split('\n'), updated.split('\n'))
    reprinted = model.UpdateXML(updated)
    self.assertEqual(PRETTY_PRINTED_XML.split('\n'), reprinted.split('\n'))

  def testIsValidNoise(self):
    valid_noise_level = {
        'name': 'A',
        'summary': 'B',
        'values': {
            'fake-prob': 0.5,
            'fake-one-prob': 0.5,
            'one-coin-prob': 0.75,
            'zero-coin-prob': 0.25,
        },
    }
    self.assertTrue(model._IsValidNoiseLevel(valid_noise_level))
    invalid_noise_level = copy.copy(valid_noise_level)
    del invalid_noise_level['name']
    self.assertFalse(model._IsValidNoiseLevel(invalid_noise_level))
    invalid_noise_level = copy.copy(valid_noise_level)
    del invalid_noise_level['summary']
    self.assertFalse(model._IsValidNoiseLevel(invalid_noise_level))
    invalid_noise_level = copy.copy(valid_noise_level)
    del invalid_noise_level['values']
    self.assertFalse(model._IsValidNoiseLevel(invalid_noise_level))
    invalid_noise_level = copy.deepcopy(valid_noise_level)
    del invalid_noise_level['values']['fake-prob']
    self.assertFalse(model._IsValidNoiseLevel(invalid_noise_level))

  def testIsValidRapporType(self):
    noise_level_names = {'NORMAL_NOISE'}
    valid_type = {
        'name': 'ETLD_PLUS_ONE',
        'summary': 'ETLD+1 parameters.',
        'parameters': {
            'num-cohorts': 128,
            'bytes': 16,
            'hash-functions': 2,
            'reporting-level': 'FINE',
            'noise-level': 'NORMAL_NOISE',
        },
    }
    self.assertTrue(model._IsValidRapporType(valid_type, noise_level_names))

  def testIsValidRapporMetric(self):
    type_names = {'ETLD_PLUS_ONE'}
    valid_metric = {
        'name': 'Search.DefaultSearchProvider',
        'flags': [],
        'owners': ['holte@chromium.org'],
        'strings': [],
        'summary': 'BLAH BLAH  BLAH',
        'type': 'ETLD_PLUS_ONE',
        'uint64': [],
    }
    self.assertTrue(model._IsValidMetric(valid_metric, type_names))
    invalid_metric = copy.copy(valid_metric)
    del invalid_metric['name']
    self.assertFalse(model._IsValidMetric(invalid_metric, type_names))
    invalid_metric = copy.copy(valid_metric)
    del invalid_metric['summary']
    self.assertFalse(model._IsValidMetric(invalid_metric, type_names))
    invalid_metric = copy.copy(valid_metric)
    invalid_metric['type'] = 'FOO'
    self.assertFalse(model._IsValidMetric(invalid_metric, type_names))
    invalid_metric = copy.copy(valid_metric)
    invalid_metric['owners'] = []
    self.assertFalse(model._IsValidMetric(invalid_metric, type_names))


if __name__ == '__main__':
  unittest.main()

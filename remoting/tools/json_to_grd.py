#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script can be used to convert messages.json to grd translations.

import codecs
import json
import optparse
import os
import string
import sys
import xml.dom.minidom as dom

import sys
sys.path.append('../../tools/grit')
sys.path.append('../../tools/grit/grit')
import tclib

def camelCase2ALL_CAPS(s):
  for c in string.ascii_uppercase:
    s = s.replace(c, '_' + c)
  return s.upper()

def load_messages(filename):
  import collections
  def create_ordered_dict(list):
    return collections.OrderedDict(list)
  messages = json.load(file(filename), object_pairs_hook=create_ordered_dict)
  ids = messages.keys()

  result = {}
  for id in ids:
    text = messages[id]['message']
    placeholders = []
    if messages[id].has_key('placeholders'):
      for name, p in messages[id]['placeholders'].items():
        index = p['content']
        caps_name = camelCase2ALL_CAPS(name)
        placeholders.append(tclib.Placeholder(caps_name, index, p['example']))
        text = text.replace('$' + name + '$', caps_name)

    msg = tclib.Message(text, placeholders,
                        messages[id]['description'])
    result[id] = msg

  return ids, result

def json_to_grd(input_dir, output_dir):
  # load list of string IDs and placeholder names from the untranslated file
  # because order of messages and placeholder names are not persisted in
  # translations.
  message_ids, messages = load_messages(
      os.path.join(input_dir, '_locales/en', 'messages.json'))

  grd_name = os.path.join(output_dir,
                          'string_resources.grd')
  grd_xml = dom.parse(grd_name)
  grd_translations = grd_xml.getElementsByTagName('translations')[0]
  grd_outputs = grd_xml.getElementsByTagName('outputs')[0]
  grd_messages = grd_xml.getElementsByTagName('messages')[0]

  for id in message_ids:
    grit_id = 'IDR_' + id
    message = grd_xml.createElement('message')
    grd_messages.appendChild(grd_xml.createTextNode('  '))
    grd_messages.appendChild(message)
    grd_messages.appendChild(grd_xml.createTextNode('\n    '))
    message.setAttribute('name', grit_id)
    message.setAttribute('desc', messages[id].description)
    message.appendChild(grd_xml.createTextNode('\n        '))
    for part in messages[id].parts:
      if isinstance(part, tclib.Placeholder):
        ph = grd_xml.createElement('ph')
        message.appendChild(ph)
        ph.setAttribute('name', part.presentation)
        ph.appendChild(grd_xml.createTextNode(part.original))
        ex = grd_xml.createElement('ex')
        ph.appendChild(ex)
        ex.appendChild(grd_xml.createTextNode(part.example))
      else:
        message.appendChild(grd_xml.createTextNode(part))
    message.appendChild(grd_xml.createTextNode('\n      '))

  translations_dir = os.path.join(input_dir, '_locales.official')
  lang_list = os.listdir(translations_dir)
  lang_list.sort()
  for lang in lang_list:
    # Convert locale name representation to the Grit format.
    grit_lang = lang.replace('_', '-') if lang != 'en' else 'en_US'

    # Load translations
    translations_file = file(
        os.path.join(translations_dir, lang, 'messages.json'))
    translations = json.load(translations_file)
    translations_file.close()

    # Uppercase all string name.
    translations = dict(zip(map(lambda x: x.upper(), translations.iterkeys()),
                        translations.values()))

    doc = dom.Document()

    bundle = doc.createElement('translationbundle')
    doc.appendChild(bundle)
    bundle.setAttribute('lang', grit_lang)
    written_translations = {}
    for id in message_ids:
      bundle.appendChild(doc.createTextNode('\n'))

      if not translations.has_key(id):
        # Skip not translated messages.
        continue
      grit_id = 'IDR_' + id

      placeholders = messages[id].placeholders
      text = translations[id]['message']
      for p in placeholders:
        text = text.replace(p.original, p.presentation)

      msg_id = messages[id].GetId()
      if written_translations.has_key(msg_id):
        # We've already written translation for this message.
        continue
      parsed_translation = tclib.Translation(text, msg_id, placeholders)
      written_translations[msg_id] = 1

      translation = doc.createElement('translation')
      bundle.appendChild(translation)
      translation.setAttribute('id', parsed_translation.GetId())
      for part in parsed_translation.parts:
        if isinstance(part, tclib.Placeholder):
          ph = doc.createElement('ph')
          translation.appendChild(ph)
          ph.setAttribute('name', part.presentation)
        else:
          translation.appendChild(doc.createTextNode(part))
    bundle.appendChild(doc.createTextNode('\n'))

    out_name = os.path.join(output_dir,
                            'string_resources_' + grit_lang + '.xtb')
    out = codecs.open(out_name, mode='w', encoding = 'UTF-8')
    out.write('<?xml version="1.0" ?>\n'
              '<!DOCTYPE translationbundle>\n')
    bundle.writexml(out)
    out.close()

    if grit_lang == 'en_US':
      continue

    t = grd_xml.createElement('file')
    grd_translations.appendChild(grd_xml.createTextNode('  '))
    grd_translations.appendChild(t)
    grd_translations.appendChild(grd_xml.createTextNode('\n  '))
    t.setAttribute('path', 'string_resources_' + grit_lang + '.xtb')
    t.setAttribute('lang', grit_lang)

    t = grd_xml.createElement('output')
    grd_outputs.appendChild(grd_xml.createTextNode('  '))
    grd_outputs.appendChild(t)
    grd_outputs.appendChild(grd_xml.createTextNode('\n  '))
    t.setAttribute('filename', 'remoting/resources/' + grit_lang + '.pak')
    t.setAttribute('type', 'data_package')
    t.setAttribute('lang', grit_lang)

    t = grd_xml.createElement('output')
    grd_outputs.appendChild(grd_xml.createTextNode('  '))
    grd_outputs.appendChild(t)
    grd_outputs.appendChild(grd_xml.createTextNode('\n  '))
    t.setAttribute('filename',
                   'remoting/webapp/_locales/' + lang + '/messages.json')
    t.setAttribute('type', 'chrome_messages_json')
    t.setAttribute('lang', grit_lang)

  grd_out = codecs.open(grd_name + '_new', mode='w', encoding = 'UTF-8')
  grd_xml.writexml(grd_out, encoding = 'UTF-8')
  grd_out.close()

def main():
  usage = 'Usage: json_to_grd <input_dir> <output_dir>'
  parser = optparse.OptionParser(usage=usage)
  options, args = parser.parse_args()
  if len(args) != 2:
    parser.error('two positional arguments expected')

  return json_to_grd(args[0], args[1])

if __name__ == '__main__':
  sys.exit(main())

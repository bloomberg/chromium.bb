#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generated JSON for translations, and merges translated files.

  Typical usage:

  update_locales --generate > messages.json
  update_locales --merge dir_with_translations
"""

import argparse
from collections import OrderedDict
import json
import os

LOCALE_DIR = os.path.relpath('../src/_locales', os.path.dirname(
    os.path.realpath(__file__)))


def load_lang(path, lang):
  messages = None
  with open('%s/%s/messages.json' % (path, lang)) as json_file:
    messages = json.load(json_file, object_pairs_hook=OrderedDict)
  return messages


def save_lang(path, lang, messages):
  with open('%s/%s/messages.json' % (path, lang), 'w') as json_file:
    json_file.write(json.dumps(messages, ensure_ascii=False, indent=2,
                               separators=(',', ': ')).encode('utf8'))


def list_locales(path):
  return os.listdir(path)


def generate():
  """Generates a JSON with untranslated entries to stdout."""
  locales = list_locales(LOCALE_DIR)
  reference = load_lang(LOCALE_DIR, 'en')
  missing = {}
  for locale in locales:
    messages = load_lang(LOCALE_DIR, locale)
    for key in reference:
      if key not in messages.keys():
        if key not in missing.keys():
          missing[key] = reference[key]
  print json.dumps(missing, indent=4)


def to_chrome_locale(locale):
  """Converts locale codes to Chrome compatible ones."""
  if locale == 'iw':
    return 'he'
  return locale.replace('-', '_')


def merge(translation_dir):
  """Merges translated locales in the passed dir with existing ones."""
  translated_locales = list_locales(translation_dir)
  locales = list_locales(LOCALE_DIR)
  done = []

  for locale in translated_locales:
    source = load_lang(translation_dir, locale)
    chrome_locale = to_chrome_locale(locale)
    try:
      target = load_lang(LOCALE_DIR, chrome_locale)
    except IOError:
      print '"Locale not recognized: %s' % locale
      continue

    target.update(source)
    save_lang(LOCALE_DIR, chrome_locale, target)
    done.append(chrome_locale)

  for locale in locales:
    if locale not in done:
      print 'Translations for locale %s not found.' % locale


def main():
  parser = argparse.ArgumentParser(
      description='Generates and merges translations.')
  parser.add_argument('--generate', action='store_true',
                      help='Outputs JSON with missing translations.')
  parser.add_argument(
      '--merge', help='Merges translated files with existing translations.')
  args = parser.parse_args()

  if args.generate:
    generate()
    return

  if args.merge is not None:
    merge(args.merge)
    return

  parser.print_help()


if __name__ == '__main__':
  main()


#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate keyboard layout and hotkey data for the keyboard overlay.

This script fetches data from the keyboard layout and hotkey data spreadsheet,
and output the data depending on the option.

  --cc: Generates C++ code snippet to be inserted into
      chrome/browser/chromeos/webui/keyboard_overlay_ui.cc

  --grd: Generates grd message snippet to be inserted into
      chrome/app/generated_resources.grd

  --js: Generates a JavaScript file used as
      chrome/browser/resources/keyboard_overlay/keyboard_overlay_data.js

  --altgr: Generates a list of layouts to be inserted into
      chrome/browser/chromeos/input_method/xkeyboard.cc


These options can be specified at the same time, and the files are generated
with corresponding extensions appended at the end of the name specified by
--out.

e.g.
python gen_keyboard_overlay_data.py --cc --grd --js --out=keyboard_overlay_data

The command above generates keyboard_overlay_data.cc,
keyboard_overlay_data.grd, and keyboard_overlay_data.js.
"""

import datetime
import gdata.spreadsheet.service
import getpass
import json
import optparse
import os
import re
import sys

MODIFIER_SHIFT = 1 << 0
MODIFIER_CTRL = 1 << 1
MODIFIER_ALT = 1 << 2

KEYBOARD_GLYPH_SPREADSHEET_KEY = '0Ao3KldW9piwEdExLbGR6TmZ2RU9aUjFCMmVxWkVqVmc'
HOTKEY_SPREADSHEET_KEY = '0AqzoqbAMLyEPdE1RQXdodk1qVkFyTWtQbUxROVM1cXc'

LABEL_MAP = {
  'glyph_arrow_down': 'down',
  'glyph_arrow_left': 'left',
  'glyph_arrow_right': 'right',
  'glyph_arrow_up': 'up',
  'glyph_back': 'back',
  'glyph_backspace': 'backspace',
  'glyph_brightness_down': 'bright down',
  'glyph_brightness_up': 'bright up',
  'glyph_enter': 'enter',
  'glyph_forward': 'forward',
  'glyph_fullscreen': 'full screen',
  # Kana/Eisu key on Japanese keyboard
  'glyph_ime': u'\u304b\u306a\u0020\u002f\u0020\u82f1\u6570',
  'glyph_lock': 'lock',
  'glyph_overview': 'switch window',
  'glyph_power': 'power',
  'glyph_right': 'right',
  'glyph_reload': 'reload',
  'glyph_search': 'search',
  'glyph_shift': 'shift',
  'glyph_tab': 'tab',
  'glyph_tools': 'tools',
  'glyph_volume_down': 'vol. down',
  'glyph_volume_mute': 'mute',
  'glyph_volume_up': 'vol. up',
};

COPYRIGHT_HEADER_TEMPLATE=(
"""// Copyright (c) %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
""")

# A snippet for grd file
GRD_SNIPPET_TEMPLATE="""      <message name="%s" desc="%s">
        %s
      </message>
"""

# A snippet for C++ file
CC_SNIPPET_TEMPLATE="""  localized_strings.SetString("%s",
      l10n_util::GetStringUTF16(%s));
"""

ALTGR_TEMPLATE="""// These are the overlay names of layouts that shouldn't
// remap the right alt key.
const char* kKeepRightAltOverlays[] = {
%s
};

// These are the overlay names with caps lock remapped
const char* kCapsLockRemapped[] = {
%s
};

"""

def SplitBehavior(behavior):
  """Splits the behavior to compose a message or i18n-content value.

  Examples:
    'Activate last tab' => ['Activate', 'last', 'tab']
    'Close tab' => ['Close', 'tab']
  """
  return [x for x in re.split('[ ()"-.,]', behavior) if len(x) > 0]


def ToMessageName(behavior):
  """Composes a message name for grd file.

  Examples:
    'Activate last tab' => IDS_KEYBOARD_OVERLAY_ACTIVATE_LAST_TAB
    'Close tab' => IDS_KEYBOARD_OVERLAY_CLOSE_TAB
  """
  segments = [segment.upper() for segment in SplitBehavior(behavior)]
  return 'IDS_KEYBOARD_OVERLAY_' + ('_'.join(segments))


def Toi18nContent(behavior):
  """Composes a i18n-content value for HTML/JavaScript files.

  Examples:
    'Activate last tab' => keyboardOverlayActivateLastTab
    'Close tab' => keyboardOverlayCloseTab
  """
  segments = [segment.lower() for segment in SplitBehavior(behavior)]
  result = 'keyboardOverlay'
  for segment in segments:
    result += segment[0].upper() + segment[1:]
  return result


def ToKeys(hotkey):
  """Converts the action value to shortcut keys used from JavaScript.

  Examples:
    'Ctrl - 9' => '9<>CTRL'
    'Ctrl - Shift - Tab' => 'tab<>CTRL<>SHIFT'
  """
  values = hotkey.split(' - ')
  modifiers = sorted(value.upper() for value in values
                     if value in ['Shift', 'Ctrl', 'Alt'])
  keycode = [value.lower() for value in values
             if value not in ['Shift', 'Ctrl', 'Alt']]
  # The keys which are highlighted even without modifier keys.
  base_keys = ['backspace', 'power']
  if not modifiers and (keycode and keycode[0] not in base_keys):
    return None
  return '<>'.join(keycode + modifiers)


def ParseOptions():
  """Parses the input arguemnts and returns options."""
  # default_username = os.getusername() + '@google.com';
  default_username = '%s@google.com' % os.environ.get('USER')
  parser = optparse.OptionParser()
  parser.add_option('--key', dest='key',
                    help='The key of the spreadsheet (required).')
  parser.add_option('--username', dest='username',
                    default=default_username,
                    help='Your user name (default: %s).' % default_username)
  parser.add_option('--password', dest='password',
                    help='Your password.')
  parser.add_option('--account_type', default='GOOGLE', dest='account_type',
                    help='Account type used for gdata login (default: GOOGLE)')
  parser.add_option('--js', dest='js', default=False, action='store_true',
                    help='Output js file.')
  parser.add_option('--grd', dest='grd', default=False, action='store_true',
                    help='Output resource file.')
  parser.add_option('--cc', dest='cc', default=False, action='store_true',
                    help='Output cc file.')
  parser.add_option('--altgr', dest='altgr', default=False, action='store_true',
                    help='Output altgr file.')
  parser.add_option('--out', dest='out', default='keyboard_overlay_data',
                    help='The output file name without extension.')
  (options, unused_args) = parser.parse_args()

  if not options.username.endswith('google.com'):
    print 'google.com account is necessary to use this script.'
    sys.exit(-1)

  if (not (options.js or options.grd or options.cc or options.altgr)):
    print 'Either --js, --grd, --cc or --altgr needs to be specified.'
    sys.exit(-1)

  # Get the password from the terminal, if needed.
  if not options.password:
    options.password = getpass.getpass('Password for %s: ' % options.username)
  return options


def InitClient(options):
  """Initializes the spreadsheet client."""
  client = gdata.spreadsheet.service.SpreadsheetsService()
  client.email = options.username
  client.password = options.password
  client.source = 'Spread Sheet'
  client.account_type = options.account_type
  print 'Logging in as %s (%s)' % (client.email, client.account_type)
  client.ProgrammaticLogin()
  return client


def PrintDiffs(message, lhs, rhs):
  """Prints the differences between |lhs| and |rhs|."""
  dif = set(lhs).difference(rhs)
  if dif:
    print message, ', '.join(dif)


def FetchSpreadsheetFeeds(client, key, sheets, cols):
  """Fetch feeds from the spreadsheet.

  Args:
    client: A spreadsheet client to be used for fetching data.
    key: A key string of the spreadsheet to be fetched.
    sheets: A list of the sheet names to read data from.
    cols: A list of columns to read data from.
  """
  worksheets_feed = client.GetWorksheetsFeed(key)
  print 'Fetching data from the worksheet: %s' % worksheets_feed.title.text
  worksheets_data = {}
  titles = []
  for entry in worksheets_feed.entry:
    worksheet_id = entry.id.text.split('/')[-1]
    list_feed = client.GetListFeed(key, worksheet_id)
    list_data = []
    # Hack to deal with sheet names like 'sv (Copy of fl)'
    title = list_feed.title.text.split('(')[0].strip()
    titles.append(title)
    if title not in sheets:
      continue
    print 'Reading data from the sheet: %s' % list_feed.title.text
    for i, entry in enumerate(list_feed.entry):
      line_data = {}
      for k in entry.custom:
        if (k not in cols) or (not entry.custom[k].text):
          continue
        line_data[k] = entry.custom[k].text
      list_data.append(line_data)
    worksheets_data[title] = list_data
  PrintDiffs('Exist only on the spreadsheet: ', titles, sheets)
  PrintDiffs('Specified but do not exist on the spreadsheet: ', sheets, titles)
  return worksheets_data


def FetchKeyboardGlyphData(client):
  """Fetches the keyboard glyph data from the spreadsheet."""
  languages = ['en_US', 'en_US_colemak', 'en_US_dvorak', 'en_US_intl',
               'en_US_altgr_intl', 'ar', 'ar_fr', 'bg', 'ca', 'cs', 'da',
               'de', 'de_neo', 'el', 'en_fr_hybrid_CA', 'en_GB', 'en_GB_dvorak',
               'es', 'es_419', 'et', 'fi', 'fil', 'fr', 'fr_CA', 'hi',
               'hr', 'hu', 'id', 'it', 'iw', 'ja', 'ko', 'lt', 'lv', 'nl', 'no',
               'pl', 'pt_BR', 'pt_PT', 'ro', 'ru', 'sk', 'sl', 'sr', 'sv', 'th',
               'tr', 'uk', 'vi', 'zh_CN', 'zh_TW']
  glyph_cols = ['scancode', 'p0', 'p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7',
                'p8', 'p9', 'label', 'format', 'notes']
  keyboard_glyph_data = FetchSpreadsheetFeeds(
      client, KEYBOARD_GLYPH_SPREADSHEET_KEY, languages, glyph_cols)
  ret = {}
  for lang in keyboard_glyph_data:
    ret[lang] = {}
    keys = {}
    for line in keyboard_glyph_data[lang]:
      scancode = line.get('scancode')
      if (not scancode) and line.get('notes'):
        ret[lang]['layoutName'] = line['notes']
        continue
      del line['scancode']
      if 'notes' in line:
        del line['notes']
      if 'label' in line:
        line['label'] = LABEL_MAP.get(line['label'], line['label'])
      keys[scancode] = line
    # Add a label to space key
    if '39' not in keys:
      keys['39'] = {'label': 'space'}
    ret[lang]['keys'] = keys
  return ret


def FetchLayoutsData(client):
  """Fetches the keyboard glyph data from the spreadsheet."""
  layout_names = ['U_layout', 'J_layout', 'E_layout', 'B_layout']
  cols = ['scancode', 'x', 'y', 'w', 'h']
  layouts = FetchSpreadsheetFeeds(client, KEYBOARD_GLYPH_SPREADSHEET_KEY,
                                  layout_names, cols)
  ret = {}
  for layout_name, layout in layouts.items():
    ret[layout_name[0]] = []
    for row in layout:
      line = []
      for col in cols:
        value = row.get(col)
        if not value:
          line.append('')
        else:
          if col != 'scancode':
            value = float(value)
          line.append(value)
      ret[layout_name[0]].append(line)
  return ret


def FetchHotkeyData(client):
  """Fetches the hotkey data from the spreadsheet."""
  hotkey_sheet = ['Cross Platform Behaviors']
  hotkey_cols = ['behavior', 'context', 'kind', 'actionctrlctrlcmdonmac',
                 'chromeos']
  hotkey_data = FetchSpreadsheetFeeds(client, HOTKEY_SPREADSHEET_KEY,
                                      hotkey_sheet, hotkey_cols)
  action_to_id = {}
  id_to_behavior = {}
  # (behavior, action)
  result = []
  for line in hotkey_data['Cross Platform Behaviors']:
    if (not line.get('chromeos')) or (line.get('kind') != 'Key'):
      continue
    action = ToKeys(line['actionctrlctrlcmdonmac'])
    if not action:
      continue
    behavior = line['behavior'].strip()
    result.append((behavior, action))
  return result


def GenerateCopyrightHeader():
  """Generates the copyright header for JavaScript code."""
  return COPYRIGHT_HEADER_TEMPLATE % datetime.date.today().year


def OutputJson(keyboard_glyph_data, hotkey_data, layouts, var_name, outfile):
  """Outputs the keyboard overlay data as a JSON file."""
  print 'Generating: %s' % outfile
  action_to_id = {}
  for (behavior, action) in hotkey_data:
    i18nContent = Toi18nContent(behavior)
    action_to_id[action] = i18nContent
  data = {'keyboardGlyph': keyboard_glyph_data,
          'shortcut': action_to_id,
          'layouts': layouts}
  out = file(outfile, 'w')
  out.write(GenerateCopyrightHeader() + '\n')
  json_data =  json.dumps(data, sort_keys=True, indent=2)
  # Remove redundant spaces after ','
  json_data = json_data.replace(', \n', ',\n')
  out.write('var %s = %s;' % (var_name, json_data))


def OutputGrd(hotkey_data, outfile):
  """Outputs a snippet used for a grd file."""
  print 'Generating: %s' % outfile
  desc = 'The text in the keyboard overlay to explain the shortcut.'
  out = file(outfile, 'w')
  for (behavior, _) in hotkey_data:
    out.write(GRD_SNIPPET_TEMPLATE % (ToMessageName(behavior), desc, behavior))


def OutputCC(hotkey_data, outfile):
  """Outputs a snippet used for C++ file."""
  print 'Generating: %s' % outfile
  out = file(outfile, 'w')
  for (behavior, _) in hotkey_data:
    message_name = ToMessageName(behavior)
    # Indent the line if message_name is longer than 45 characters, which means
    # the second line in the generated code is longer than 80 characters.
    if len(message_name) > 45:
      message_name = '\n          %s' % message_name
    out.write(CC_SNIPPET_TEMPLATE % (Toi18nContent(behavior),
                                     message_name))


def OutputAltGr(keyboard_glyph_data, outfile):
  """Outputs the keyboard overlay data as a JSON file."""
  print 'Generating: %s' % outfile

  altgr_output = []
  capslock_output = []

  for layout in keyboard_glyph_data.keys():
    try:
      # If left and right alt have different values, this layout to the list of
      # layouts that don't remap the right alt key.
      right_alt = keyboard_glyph_data[layout]["keys"]["E0 38"]["label"].strip()
      left_alt = keyboard_glyph_data[layout]["keys"]["38"]["label"].strip()
      if right_alt.lower() != left_alt.lower():
        altgr_output.append('  "%s",' % layout)
    except KeyError:
      pass

    try:
      caps_lock = keyboard_glyph_data[layout]["keys"]["E0 5B"]["label"].strip()
      if caps_lock.lower() != "search":
        capslock_output.append('  "%s",' % layout)
    except KeyError:
      pass

  out = file(outfile, 'w')
  out.write(ALTGR_TEMPLATE % (
      "\n".join(altgr_output), "\n".join(capslock_output)))



def main():
  options = ParseOptions()
  client = InitClient(options)
  hotkey_data = FetchHotkeyData(client)

  if options.js or options.altgr:
    keyboard_glyph_data = FetchKeyboardGlyphData(client)

  if options.js:
    layouts = FetchLayoutsData(client)
    OutputJson(keyboard_glyph_data, hotkey_data, layouts, 'keyboardOverlayData',
               options.out + '.js')
  if options.grd:
    OutputGrd(hotkey_data, options.out + '.grd')
  if options.cc:
    OutputCC(hotkey_data, options.out + '.cc')
  if options.altgr:
    OutputAltGr(keyboard_glyph_data, options.out + '.altgr')


if __name__ == '__main__':
  main()

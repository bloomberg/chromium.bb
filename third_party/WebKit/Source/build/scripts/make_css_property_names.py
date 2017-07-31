#!/usr/bin/env python

import subprocess
import sys

from core.css import css_properties
import json5_generator
import template_expander
import license


GPERF_TEMPLATE = """
%%{
%(license)s

#include "%(class_name)s.h"
#include "core/css/HashTools.h"
#include <string.h>

#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"

#ifdef _MSC_VER
// Disable the warnings from casting a 64-bit pointer to 32-bit long
// warning C4302: 'type cast': truncation from 'char (*)[28]' to 'long'
// warning C4311: 'type cast': pointer truncation from 'char (*)[18]' to 'long'
#pragma warning(disable : 4302 4311)
#endif

#if defined(__clang__)
#pragma clang diagnostic push
// TODO(thakis): Remove once we use a gperf that no longer produces "register".
#pragma clang diagnostic ignored "-Wdeprecated-register"
#endif

namespace blink {
static const char propertyNameStringsPool[] = {
%(property_name_strings)s
};

static const unsigned short propertyNameStringsOffsets[] = {
%(property_name_offsets)s
};

%%}
%%struct-type
struct Property;
%%omit-struct-type
%%language=C++
%%readonly-tables
%%global-table
%%compare-strncmp
%%define class-name %(class_name)sHash
%%define lookup-function-name findPropertyImpl
%%define hash-function-name property_hash_function
%%define slot-name name_offset
%%define word-array-name property_word_list
%%enum
%%%%
%(property_to_enum_map)s
%%%%

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

const Property* FindProperty(const char* str, unsigned int len) {
  return %(class_name)sHash::findPropertyImpl(str, len);
}

const char* getPropertyName(CSSPropertyID id) {
  DCHECK(isCSSPropertyIDWithName(id));
  int index = id - firstCSSProperty;
  return propertyNameStringsPool + propertyNameStringsOffsets[index];
}

const AtomicString& getPropertyNameAtomicString(CSSPropertyID id) {
  DCHECK(isCSSPropertyIDWithName(id));
  int index = id - firstCSSProperty;
  static AtomicString* propertyStrings =
      new AtomicString[lastUnresolvedCSSProperty]; // Leaked.
  AtomicString& propertyString = propertyStrings[index];
  if (propertyString.IsNull()) {
    propertyString = AtomicString(propertyNameStringsPool +
                     propertyNameStringsOffsets[index]);
  }
  return propertyString;
}

String getPropertyNameString(CSSPropertyID id) {
  // We share the StringImpl with the AtomicStrings.
  return getPropertyNameAtomicString(id).GetString();
}

String getJSPropertyName(CSSPropertyID id) {
  char result[maxCSSPropertyNameLength + 1];
  const char* cssPropertyName = getPropertyName(id);
  const char* propertyNamePointer = cssPropertyName;
  if (!propertyNamePointer)
    return g_empty_string;

  char* resultPointer = result;
  while (char character = *propertyNamePointer++) {
    if (character == '-') {
      char nextCharacter = *propertyNamePointer++;
      if (!nextCharacter)
        break;
      character = (propertyNamePointer - 2 != cssPropertyName)
                      ? ToASCIIUpper(nextCharacter) : nextCharacter;
    }
    *resultPointer++ = character;
  }
  *resultPointer = '\\0';
  return String(result);
}

CSSPropertyID cssPropertyID(const String& string)
{
    return resolveCSSPropertyID(unresolvedCSSPropertyID(string));
}

} // namespace blink
"""


class CSSPropertyNamesWriter(css_properties.CSSProperties):
    class_name = "CSSPropertyNames"

    def __init__(self, json5_file_path):
        super(CSSPropertyNamesWriter, self).__init__(json5_file_path)
        self._outputs = {(self.class_name + ".h"): self.generate_header,
                         (self.class_name + ".cpp"): self.generate_implementation,
                        }

    def _enum_declaration(self, property_):
        return "    %(property_id)s = %(enum_value)s," % property_

    def _array_item(self, property_):
        return "    static_cast<CSSPropertyID>(%(enum_value)s), // %(property_id)s" % property_

    @template_expander.use_jinja('templates/CSSPropertyNames.h.tmpl')
    def generate_header(self):
        return {
            'alias_offset': self._alias_offset,
            'class_name': self.class_name,
            'property_enums': "\n".join(map(self._enum_declaration, self._properties_including_aliases)),
            'property_aliases': "\n".join(map(self._array_item, self._aliases)),
            'first_property_id': self._first_enum_value,
            'properties_count': len(self._properties),
            'last_property_id': self._first_enum_value + len(self._properties) - 1,
            'last_unresolved_property_id': self.last_unresolved_property_id,
            'max_name_length': max(map(len, self._properties)),
        }

    def generate_implementation(self):
        enum_value_to_name = {property['enum_value']: property['name'] for property in self._properties_including_aliases}
        property_offsets = []
        property_names = []
        current_offset = 0
        for enum_value in range(self._first_enum_value, max(enum_value_to_name) + 1):
            property_offsets.append(current_offset)
            if enum_value in enum_value_to_name:
                name = enum_value_to_name[enum_value]
                property_names.append(name)
                current_offset += len(name) + 1

        css_name_and_enum_pairs = [(property['name'], property['property_id']) for property in self._properties_including_aliases]

        gperf_input = GPERF_TEMPLATE % {
            'license': license.license_for_generated_cpp(),
            'class_name': self.class_name,
            'property_name_strings': '\n'.join('    "%s\\0"' % name for name in property_names),
            'property_name_offsets': '\n'.join('    %d,' % offset for offset in property_offsets),
            'property_to_enum_map': '\n'.join('%s, %s' % property for property in css_name_and_enum_pairs),
        }
        # FIXME: If we could depend on Python 2.7, we would use subprocess.check_output
        gperf_args = [self.gperf_path, '--key-positions=*', '-P', '-n']
        gperf_args.extend(['-m', '50'])  # Pick best of 50 attempts.
        gperf_args.append('-D')  # Allow duplicate hashes -> More compact code.
        gperf_args.extend(['-Q', 'CSSPropStringPool'])  # Unique var names.

        # If gperf isn't in the path we get an OSError. We don't want to use
        # the normal solution of shell=True (as this has to run on many
        # platforms), so instead we catch the error and raise a
        # CalledProcessError like subprocess would do when shell=True is set.
        try:
            gperf = subprocess.Popen(gperf_args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)
            return gperf.communicate(gperf_input)[0]
        except OSError:
            raise subprocess.CalledProcessError(127, gperf_args, output='Command not found.')


if __name__ == "__main__":
    json5_generator.Maker(CSSPropertyNamesWriter).main()

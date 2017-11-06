#!/usr/bin/env python

import subprocess
import sys

from core.css import css_properties
import json5_generator
import template_expander


class CSSPropertyNamesWriter(json5_generator.Writer):
    class_name = "CSSPropertyNames"

    def __init__(self, json5_file_path):
        super(CSSPropertyNamesWriter, self).__init__(json5_file_path)
        self._outputs = {
            (self.class_name + ".h"): self.generate_header,
            (self.class_name + ".cpp"): self.generate_implementation,
        }
        self._css_properties = css_properties.CSSProperties(json5_file_path)

    def _enum_declaration(self, property_):
        return "    %(property_id)s = %(enum_value)s," % property_

    def _array_item(self, property_):
        return "    static_cast<CSSPropertyID>(%(enum_value)s), " \
            "// %(property_id)s" % property_

    @template_expander.use_jinja('templates/CSSPropertyNames.h.tmpl')
    def generate_header(self):
        return {
            'alias_offset': self._css_properties.alias_offset,
            'class_name': self.class_name,
            'property_enums': "\n".join(map(
                self._enum_declaration,
                self._css_properties.properties_including_aliases)),
            'property_aliases': "\n".join(
                map(self._array_item, self._css_properties.aliases)),
            'first_property_id': self._css_properties.first_property_id,
            'properties_count':
                len(self._css_properties.properties_including_aliases),
            'last_property_id': self._css_properties.last_property_id,
            'last_unresolved_property_id':
                self._css_properties.last_unresolved_property_id,
            'max_name_length':
                max(map(len, self._css_properties.properties_by_id)),
        }

    def generate_implementation(self):
        enum_value_to_name = {}
        for property_ in self._css_properties.properties_including_aliases:
            enum_value_to_name[property_['enum_value']] = property_['name']
        property_offsets = []
        property_names = []
        current_offset = 0
        for enum_value in range(self._css_properties.first_property_id,
                                max(enum_value_to_name) + 1):
            property_offsets.append(current_offset)
            if enum_value in enum_value_to_name:
                name = enum_value_to_name[enum_value]
                property_names.append(name)
                current_offset += len(name) + 1

        css_name_and_enum_pairs = [
            (property_['name'], property_['property_id'])
            for property_ in self._css_properties.properties_including_aliases]

        template_params = {
            'class_name': 'CSSPropertyNames',
            'property_names': property_names,
            'property_offsets': property_offsets,
            'property_to_enum_map':
                '\n'.join('%s, %s' % property_
                          for property_ in css_name_and_enum_pairs),
        }
        gperf_input = template_expander.apply_template(
            'templates/CSSPropertyNames.cpp.tmpl', template_params)

        # FIXME: If we could depend on Python 2.7, we would use
        # subprocess.check_output
        gperf_args = [self.gperf_path, '--key-positions=*', '-P', '-n']
        gperf_args.extend(['-m', '50'])  # Pick best of 50 attempts.
        gperf_args.append('-D')  # Allow duplicate hashes -> More compact code.
        gperf_args.extend(['-Q', 'CSSPropStringPool'])  # Unique var names.

        # If gperf isn't in the path we get an OSError. We don't want to use
        # the normal solution of shell=True (as this has to run on many
        # platforms), so instead we catch the error and raise a
        # CalledProcessError like subprocess would do when shell=True is set.
        try:
            gperf = subprocess.Popen(
                gperf_args,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                universal_newlines=True)
            return gperf.communicate(gperf_input)[0]
        except OSError:
            raise subprocess.CalledProcessError(
                127, gperf_args, output='Command not found.')


if __name__ == "__main__":
    json5_generator.Maker(CSSPropertyNamesWriter).main()

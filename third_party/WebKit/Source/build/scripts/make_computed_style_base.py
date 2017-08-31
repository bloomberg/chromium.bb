#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math

import json5_generator
import template_expander
import make_style_builder
import keyword_utils
import bisect

from name_utilities import (
    enum_for_css_keyword, enum_type_name, enum_value_name, class_member_name, method_name,
    class_name, join_names
)
from itertools import chain

# Heuristic ordering of types from largest to smallest, used to sort fields by their alignment sizes.
# Specifying the exact alignment sizes for each type is impossible because it's platform specific,
# so we define an ordering instead.
# The ordering comes from the data obtained in:
# https://codereview.chromium.org/2841413002
# TODO(shend): Put alignment sizes into code form, rather than linking to a CL which may disappear.
ALIGNMENT_ORDER = [
    # Aligns like double
    'ScaleTransformOperation', 'RotateTransformOperation', 'TranslateTransformOperation', 'double',
    # Aligns like a pointer (can be 32 or 64 bits)
    'NamedGridLinesMap', 'OrderedNamedGridLines', 'NamedGridAreaMap', 'TransformOperations',
    'Vector<CSSPropertyID>', 'Vector<GridTrackSize>', 'GridPosition', 'AtomicString',
    'RefPtr', 'Persistent', 'std::unique_ptr',
    'Vector<String>', 'Font', 'FillLayer', 'NinePieceImage',
    # Aligns like float
    'StyleOffsetRotation', 'TransformOrigin', 'ScrollPadding', 'ScrollSnapMargin', 'LengthBox',
    'LengthSize', 'FloatSize', 'LengthPoint', 'Length', 'TextSizeAdjust', 'TabSize', 'float',
    # Aligns like int
    'ScrollSnapType', 'ScrollSnapAlign', 'BorderValue', 'StyleColor', 'Color', 'LayoutUnit',
    'LineClampValue', 'OutlineValue', 'unsigned', 'size_t', 'int',
    # Aligns like short
    'unsigned short', 'short',
    # Aligns like char
    'StyleSelfAlignmentData', 'StyleContentAlignmentData', 'uint8_t', 'char',
    # Aligns like bool
    'bool'
]

# TODO(shend): Improve documentation and add docstrings.


def _flatten_list(x):
    """Flattens a list of lists into a single list."""
    return list(chain.from_iterable(x))


def _num_32_bit_words_for_bit_fields(bit_fields):
    """Gets the number of 32 bit unsigned integers needed store a list of bit fields."""
    num_buckets, cur_bucket = 0, 0
    for field in bit_fields:
        if field.size + cur_bucket > 32:
            num_buckets += 1
            cur_bucket = 0
        cur_bucket += field.size
    return num_buckets + (cur_bucket > 0)


class Group(object):
    """Represents a group of fields stored together in a class.

    Attributes:
        name: The name of the group as a string.
        subgroups: List of Group instances that are stored as subgroups under this group.
        fields: List of Field instances stored directly under this group.
        parent: The parent group, or None if this is the root group.
    """
    def __init__(self, name, subgroups, fields):
        self.name = name
        self.subgroups = subgroups
        self.fields = fields
        self.parent = None

        self.type_name = class_name(['style', name, 'data'])
        self.member_name = class_member_name([name, 'data'])
        self.num_32_bit_words_for_bit_fields = _num_32_bit_words_for_bit_fields(
            field for field in fields if field.is_bit_field
        )

        # Recursively get all the fields in the subgroups as well
        self.all_fields = _flatten_list(subgroup.all_fields for subgroup in subgroups) + fields

        # Ensure that all fields/subgroups on this group link to it
        for field in fields:
            field.group = self

        for subgroup in subgroups:
            subgroup.parent = self

    def path_without_root(self):
        """Return list of ancestor groups, excluding the root group.

        The first item is the current group, second item is the parent, third is the grandparent
        and so on.
        """
        group_path = []
        current_group = self
        while current_group.name:
            group_path.insert(0, current_group)
            current_group = current_group.parent
        return group_path


class Enum(object):
    """Represents a generated enum in ComputedStyleBaseConstants."""
    def __init__(self, type_name, keywords, is_set):
        self.type_name = type_name
        self.values = [enum_value_name(keyword) for keyword in keywords]
        self.is_set = is_set


class DiffGroup(object):
    """Represents a group of expressions and subgroups that need to be diffed
    for a function in ComputedStyle.

    Attributes:
        subgroups: List of DiffGroup instances that are stored as subgroups under this group.
        expressions: List of expression that are on this group that need to be diffed.
    """
    def __init__(self, group):
        self.group = group
        self.subgroups = []
        self.fields = []
        self.expressions = []
        self.predicates = []


class Field(object):
    """
    The generated ComputedStyle object is made up of a series of Fields.
    Each Field has a name, size, type, etc, and a bunch of attributes to
    determine which methods it will be used in.

    A Field also has enough information to use any storage type in C++, such as
    regular member variables, or more complex storage like vectors or hashmaps.
    Almost all properties will have at least one Field, often more than one.

    Most attributes in this class correspond to parameters in CSSProperties.json5.
    See that file for a more detailed explanation of each attribute.

    Attributes:
        field_role: The semantic role of the field. Can be:
            - 'property': for fields that store CSS properties
            - 'inherited_flag': for single-bit flags that store whether a property is
                                inherited by this style or set explicitly
        name_for_methods: String used to form the names of getters and setters.
            Should be in upper camel case.
        property_name: Name of the property that the field is part of.
        type_name: Name of the C++ type exposed by the generated interface (e.g. EClear, int).
        wrapper_pointer_name: Name of the pointer type that wraps this field (e.g. RefPtr).
        field_template: Determines the interface generated for the field. Can be one of:
           keyword, flag, or monotonic_flag.
        size: Number of bits needed for storage.
        default_value: Default value for this field when it is first initialized.
    """

    def __init__(self, field_role, name_for_methods, property_name, type_name, wrapper_pointer_name,
                 field_template, size, default_value, custom_copy, custom_compare, mutable,
                 getter_method_name, setter_method_name, initial_method_name, default_generated_functions, **kwargs):
        """Creates a new field."""
        self.name = class_member_name(name_for_methods)
        self.property_name = property_name
        self.type_name = type_name
        self.wrapper_pointer_name = wrapper_pointer_name
        self.alignment_type = self.wrapper_pointer_name or self.type_name
        self.field_template = field_template
        self.size = size
        self.default_value = default_value
        self.custom_copy = custom_copy
        self.custom_compare = custom_compare
        self.mutable = mutable
        self.group = None

        # Field role: one of these must be true
        self.is_property = field_role == 'property'
        self.is_inherited_flag = field_role == 'inherited_flag'
        assert (self.is_property, self.is_inherited_flag).count(True) == 1, \
            'Field role has to be exactly one of: property, inherited_flag'

        if not self.is_inherited_flag:
            self.is_inherited = kwargs.pop('inherited')
            self.is_independent = kwargs.pop('independent')
            assert self.is_inherited or not self.is_independent, 'Only inherited fields can be independent'

            self.is_inherited_method_name = method_name([name_for_methods, 'is inherited'])

        # Method names
        # TODO(nainar): Method name generation is inconsistent. Fix.
        self.getter_method_name = getter_method_name
        self.setter_method_name = setter_method_name
        self.internal_getter_method_name = method_name([self.name, 'internal'])
        self.internal_mutable_method_name = method_name(['mutable', name_for_methods, 'internal'])
        self.internal_setter_method_name = method_name([setter_method_name, 'internal'])
        self.initial_method_name = initial_method_name
        self.resetter_method_name = method_name(['reset', name_for_methods])
        self.default_generated_functions = default_generated_functions
        # If the size of the field is not None, it means it is a bit field
        self.is_bit_field = self.size is not None

        assert len(kwargs) == 0, 'Unexpected arguments provided to Field: ' + str(kwargs)


def _get_include_paths(properties):
    """
    Get a list of paths that need to be included for ComputedStyleBase.
    """
    include_paths = set()
    for property_ in properties:
        include_paths.update(property_['include_paths'])
    return list(sorted(include_paths))


def _create_groups(properties):
    """Create a tree of groups from a list of properties.

    Returns:
        Group: The root group of the tree. The name of the group is set to None.
    """
    # We first convert properties into a dictionary structure. Each dictionary
    # represents a group. The None key corresponds to the fields directly stored
    # on that group. The other keys map from group name to another dictionary.
    # For example:
    # {
    #   None: [field1, field2, ...]
    #   'groupA': { None: [field3] },
    #   'groupB': {
    #      None: [],
    #      'groupC': { None: [field4] },
    #   },
    # }
    #
    # We then recursively convert this dictionary into a tree of Groups.
    # TODO(shend): Skip the first step by changing Group attributes into methods.
    def _dict_to_group(name, group_dict):
        fields_in_current_group = group_dict.pop(None)
        subgroups = [_dict_to_group(subgroup_name, subgroup_dict) for subgroup_name, subgroup_dict in group_dict.items()]
        return Group(name, subgroups, _reorder_fields(fields_in_current_group))

    root_group_dict = {None: []}
    for property_ in properties:
        current_group_dict = root_group_dict
        if property_['field_group']:
            for group_name in property_['field_group'].split('->'):
                current_group_dict[group_name] = current_group_dict.get(group_name, {None: []})
                current_group_dict = current_group_dict[group_name]
        current_group_dict[None].extend(_create_fields(property_))

    return _dict_to_group(None, root_group_dict)


def _create_diff_groups_map(diff_function_inputs, root_group):
    diff_functions_map = {}

    for entry in diff_function_inputs:
        # error handling
        field_names = entry['fields_to_diff'] + _list_field_dependencies(entry['methods_to_diff'] + entry['predicates_to_test'])
        for name in field_names:
            assert name in [field.property_name for field in root_group.all_fields], \
                ("The field '" + name + "' isn't a defined field on ComputedStyle. "
                 "Please check that there's an entry for '" + name + "' in CSSProperties.json5 or ComputedStyleExtraFields.json5")
        diff_functions_map[entry['name']] = _create_diff_groups(entry['fields_to_diff'],
                                                                entry['methods_to_diff'], entry['predicates_to_test'], root_group)
    return diff_functions_map


def _list_field_dependencies(entries_with_field_dependencies):
    field_dependencies = []
    for entry in entries_with_field_dependencies:
        field_dependencies += entry['field_dependencies']
    return field_dependencies


def _create_diff_groups(fields_to_diff, methods_to_diff, predicates_to_test, root_group):
    diff_group = DiffGroup(root_group)
    field_dependencies = _list_field_dependencies(methods_to_diff + predicates_to_test)
    for subgroup in root_group.subgroups:
        if any(field.property_name in (fields_to_diff + field_dependencies) for field in subgroup.all_fields):
            diff_group.subgroups.append(_create_diff_groups(fields_to_diff, methods_to_diff, predicates_to_test, subgroup))
    for entry in fields_to_diff:
        for field in root_group.fields:
            if not field.is_inherited_flag and entry == field.property_name:
                diff_group.fields.append(field)
    for entry in methods_to_diff:
        for field in root_group.fields:
            if (not field.is_inherited_flag and field.property_name in entry['field_dependencies']
                    and entry['method'] not in diff_group.expressions):
                diff_group.expressions.append(entry['method'])
    for entry in predicates_to_test:
        for field in root_group.fields:
            if (not field.is_inherited_flag and field.property_name in entry['field_dependencies']
                    and entry['predicate'] not in diff_group.predicates):
                diff_group.predicates.append(entry['predicate'])
    return diff_group


def _create_enums(properties):
    """Returns a list of Enums to be generated"""
    enums = {}
    for property_ in properties:
        # Only generate enums for keyword properties that do not require includes.
        if property_['field_template'] in ('keyword', 'multi_keyword') and len(property_['include_paths']) == 0:
            enum = Enum(property_['type_name'], property_['keywords'],
                        is_set=(property_['field_template'] == 'multi_keyword'))
            if property_['field_template'] == 'multi_keyword':
                assert property_['keywords'][0] == 'none', \
                    "First keyword in a 'multi_keyword' field must be 'none' in '{}'.".format(property_['name'])

            if enum.type_name in enums:
                # There's an enum with the same name, check if the enum values are the same
                assert set(enums[enum.type_name].values) == set(enum.values), \
                    ("'" + property_['name'] + "' can't have type_name '" + enum.type_name + "' "
                     "because it was used by a previous property, but with a different set of keywords. "
                     "Either give it a different name or ensure the keywords are the same.")
            else:
                enums[enum.type_name] = enum

    # Return the enums sorted by type name
    return list(sorted(enums.values(), key=lambda e: e.type_name))


def _create_property_field(property_):
    """
    Create a property field.
    """
    name_for_methods = property_['name_for_methods']

    assert property_['default_value'] is not None, \
        ('MakeComputedStyleBase requires an default value for all fields, none specified '
         'for property ' + property_['name'])

    if property_['field_template'] == 'keyword':
        type_name = property_['type_name']
        default_value = type_name + '::' + enum_value_name(property_['default_value'])
        assert property_['field_size'] is None, \
            ("'" + property_['name'] + "' is a keyword field, "
             "so it should not specify a field_size")
        size = int(math.ceil(math.log(len(property_['keywords']), 2)))
    elif property_['field_template'] == 'multi_keyword':
        type_name = property_['type_name']
        default_value = type_name + '::' + enum_value_name(property_['default_value'])
        size = len(property_['keywords']) - 1  # Subtract 1 for 'none' keyword
    elif property_['field_template'] == 'storage_only':
        type_name = property_['type_name']
        default_value = property_['default_value']
        size = None
        if type_name == 'bool':
            size = 1
        elif len(property_["keywords"]) > 0 and len(property_["include_paths"]) == 0:
            # Assume that no property will ever have one keyword.
            assert len(property_['keywords']) > 1, "There must be more than 1 keywords in a CSS property"
            # Each keyword is represented as a number and the number of bit
            # to represent the maximum number is calculated here
            size = int(math.ceil(math.log(len(property_['keywords']), 2)))
        else:
            size = property_["field_size"]
    elif property_['field_template'] == 'external':
        type_name = property_['type_name']
        default_value = property_['default_value']
        size = None
    elif property_['field_template'] == 'primitive':
        type_name = property_['type_name']
        default_value = property_['default_value']
        size = 1 if type_name == 'bool' else None  # pack bools with 1 bit.
    elif property_['field_template'] == 'pointer':
        type_name = property_['type_name']
        default_value = property_['default_value']
        size = None
    elif property_['field_template'] == '<length>':
        property_['field_template'] = 'external'
        property_['type_name'] = type_name = 'Length'
        default_value = property_['default_value']
        property_['include_paths'] = ["platform/Length.h"]
        size = None
    else:
        assert property_['field_template'] in ('monotonic_flag',)
        type_name = 'bool'
        default_value = 'false'
        size = 1

    if property_['wrapper_pointer_name']:
        assert property_['field_template'] in ['storage_only', 'pointer']
        if property_['field_template'] == 'storage_only':
            type_name = '{}<{}>'.format(property_['wrapper_pointer_name'], type_name)

    return Field(
        'property',
        name_for_methods,
        property_name=property_['name'],
        inherited=property_['inherited'],
        independent=property_['independent'],
        type_name=type_name,
        wrapper_pointer_name=property_['wrapper_pointer_name'],
        field_template=property_['field_template'],
        size=size,
        default_value=default_value,
        custom_copy=property_['custom_copy'],
        custom_compare=property_['custom_compare'],
        mutable=property_['mutable'],
        getter_method_name=property_['getter'],
        setter_method_name=property_['setter'],
        initial_method_name=property_['initial'],
        default_generated_functions=property_['default_generated_functions'],
    )


def _create_inherited_flag_field(property_):
    """
    Create the field used for an inheritance fast path from an independent CSS property,
    and return the Field object.
    """
    name_for_methods = join_names(property_['name_for_methods'], 'is', 'inherited')
    return Field(
        'inherited_flag',
        name_for_methods,
        property_name=property_['name'],
        type_name='bool',
        wrapper_pointer_name=None,
        field_template='primitive',
        size=1,
        default_value='true',
        custom_copy=False,
        custom_compare=False,
        mutable=False,
        getter_method_name=method_name(name_for_methods),
        setter_method_name=method_name(['set', name_for_methods]),
        initial_method_name=method_name(['initial', name_for_methods]),
        default_generated_functions=property_["default_generated_functions"]
    )


def _create_fields(property_):
    """
    Create ComputedStyle fields from a property and return a list of Field objects.
    """
    fields = []
    # Only generate properties that have a field template
    if property_['field_template'] is not None:
        # If the property is independent, add the single-bit sized isInherited flag
        # to the list of Fields as well.
        if property_['independent']:
            fields.append(_create_inherited_flag_field(property_))

        fields.append(_create_property_field(property_))

    return fields


def _reorder_bit_fields(bit_fields):
    # Since fields cannot cross word boundaries, in order to minimize
    # padding, group fields into buckets so that as many buckets as possible
    # are exactly 32 bits. Although this greedy approach may not always
    # produce the optimal solution, we add a static_assert to the code to
    # ensure ComputedStyleBase results in the expected size. If that
    # static_assert fails, this code is falling into the small number of
    # cases that are suboptimal, and may need to be rethought.
    # For more details on packing bit fields to reduce padding, see:
    # http://www.catb.org/esr/structure-packing/#_bitfields
    field_buckets = []
    # Consider fields in descending order of size to reduce fragmentation
    # when they are selected. Ties broken in alphabetical order by name.
    for field in sorted(bit_fields, key=lambda f: (-f.size, f.name)):
        added_to_bucket = False
        # Go through each bucket and add this field if it will not increase
        # the bucket's size to larger than 32 bits. Otherwise, make a new
        # bucket containing only this field.
        for bucket in field_buckets:
            if sum(f.size for f in bucket) + field.size <= 32:
                bucket.append(field)
                added_to_bucket = True
                break
        if not added_to_bucket:
            field_buckets.append([field])

    return _flatten_list(field_buckets)


def _reorder_non_bit_fields(non_bit_fields):
    # A general rule of thumb is to sort members by their alignment requirement
    # (from biggest aligned to smallest).
    for field in non_bit_fields:
        assert field.alignment_type in ALIGNMENT_ORDER, \
            "Type {} has unknown alignment. Please update ALIGNMENT_ORDER to include it.".format(field.alignment_type)
    return list(sorted(non_bit_fields, key=lambda f: ALIGNMENT_ORDER.index(f.alignment_type)))


def _reorder_fields(fields):
    """
    Returns a list of fields ordered to minimise padding.
    """
    # Separate out bit fields from non bit fields
    bit_fields = [field for field in fields if field.is_bit_field]
    non_bit_fields = [field for field in fields if not field.is_bit_field]

    # Non bit fields go first, then the bit fields.
    return _reorder_non_bit_fields(non_bit_fields) + _reorder_bit_fields(bit_fields)


def _get_properties_ranking(properties_ranking_file, partition_rule):
    """Read the properties ranking file and produce a dictionary of css
    properties with their group number based on the partition_rule

    Args:
        properties_ranking_file: file path to the ranking file
        partition_rule: cumulative distribution over properties_ranking

    Returns:
        dictionary with keys are css properties' name values are the group
        that each css properties belong to. Smaller group number is higher
        popularity in the ranking.
    """
    properties_ranking = [x["name"] for x in
                          json5_generator.Json5File.load_from_files([properties_ranking_file]).name_dictionaries]
    return dict(zip(properties_ranking,
                    [bisect.bisect_left(partition_rule, float(i) / len(properties_ranking)) + 1
                     for i in range(len(properties_ranking))]))


def _evaluate_rare_non_inherited_group(all_properties, properties_ranking_file,
                                       number_of_layer, partition_rule=None):
    """Re-evaluate the grouping of RareNonInherited groups based on each property's
    popularity.

    Args:
        all_properties: list of all css properties
        properties_ranking_file: file path to the ranking file
        number_of_layer: the number of group to split
        partition_rule: cumulative distribution over properties_ranking
                        Ex: [0.3, 0.6, 1]
    """
    if partition_rule is None:
        partition_rule = [1.0 * (i + 1) / number_of_layer for i in range(number_of_layer)]

    assert number_of_layer == len(partition_rule), "Length of rule and number_of_layer mismatch"

    layers_name = ["rare-non-inherited-usage-less-than-" + str(int(round(partition_rule[i] * 100))) + "-percent"
                   for i in range(number_of_layer)]
    properties_ranking = _get_properties_ranking(properties_ranking_file, partition_rule)

    for property_ in all_properties:
        if property_["field_group"] is not None:
            if "rare-non-inherited" in property_["field_group"] and property_["name"] in properties_ranking:
                property_["field_group"] = "->".join(layers_name[0:properties_ranking[property_["name"]]])
            elif "rare-non-inherited" in property_["field_group"] and property_["name"] not in properties_ranking:
                group_tree = property_["field_group"].split("->")
                group_tree = [layers_name[0]] + group_tree
                property_["field_group"] = "->".join(group_tree)


def _evaluate_rare_inherit_group(all_properties, properties_ranking_file,
                                 number_of_layer, partition_rule=None):
    """Re-evaluate the grouping of RareInherited groups based on each property's
    popularity.

    Args:
        all_properties: list of all css properties
        properties_ranking_file: file path to the ranking file
        number_of_layer: the number of group to split
        partition_rule: cumulative distribution over properties_ranking
                        Ex: [0.4, 1]
    """
    if partition_rule is None:
        partition_rule = [1.0 * (i + 1) / number_of_layer for i in range(number_of_layer)]

    assert number_of_layer == len(partition_rule), "Length of rule and number_of_layer mismatch"

    layers_name = ["rare-inherited-usage-less-than-" + str(int(round(partition_rule[i] * 100))) + "-percent"
                   for i in range(number_of_layer)]
    properties_ranking = _get_properties_ranking(properties_ranking_file, partition_rule)

    for property_ in all_properties:
        if property_["field_group"] is not None \
           and "rare-inherited" in property_["field_group"] \
           and property_["name"] in properties_ranking:
            property_["field_group"] = "->".join(["rare-inherited"] + layers_name[1:properties_ranking[property_["name"]]])


class ComputedStyleBaseWriter(make_style_builder.StyleBuilderWriter):
    def __init__(self, json5_file_paths):
        # Read CSSProperties.json5
        super(ComputedStyleBaseWriter, self).__init__([json5_file_paths[0]])

        # Ignore shorthand properties
        for property_ in self._properties.values():
            if property_['field_template'] is not None:
                assert not property_['longhands'], \
                    "Shorthand '{}' cannot have a field_template.".format(property_['name'])

        css_properties = [value for value in self._properties.values() if not value['longhands']]
        # We sort the enum values based on each value's position in
        # the keywords as listed in CSSProperties.json5. This will ensure that if there is a continuous
        # segment in CSSProperties.json5 matching the segment in this enum then
        # the generated enum will have the same order and continuity as
        # CSSProperties.json5 and we can get the longest continuous segment.
        # Thereby reduce the switch case statement to the minimum.
        css_properties = keyword_utils.sort_keyword_properties_by_canonical_order(css_properties,
                                                                                  json5_file_paths[3],
                                                                                  self.json5_file.parameters)

        for property_ in css_properties:
            # Set default values for extra parameters in ComputedStyleExtraFields.json5.
            property_['custom_copy'] = False
            property_['custom_compare'] = False
            property_['mutable'] = False

        # Read ComputedStyleExtraFields.json5 using the parameter specification from the CSS properties file.
        extra_fields = json5_generator.Json5File.load_from_files(
            [json5_file_paths[1]],
            default_parameters=self.json5_file.parameters
        ).name_dictionaries

        for property_ in extra_fields:
            if property_['mutable']:
                assert property_['field_template'] == 'monotonic_flag', \
                    'mutable keyword only implemented for monotonic_flag'
            make_style_builder.apply_property_naming_defaults(property_)

        all_properties = css_properties + extra_fields

        self._generated_enums = _create_enums(all_properties)

        # Organise fields into a tree structure where the root group
        # is ComputedStyleBase.

        # [0.134, 0.327, 1.0] is the best RareNonInherited partition parameter
        # that was found by experiments
        _evaluate_rare_non_inherited_group(all_properties, json5_file_paths[4], 3, [0.134, 0.327, 1.0])
        # [0.4, 1.0] is the best RareInherited partition parameter that was
        # found by experiments
        _evaluate_rare_inherit_group(all_properties, json5_file_paths[4], 2, [0.4, 1.0])
        self._root_group = _create_groups(all_properties)
        self._diff_functions_map = _create_diff_groups_map(json5_generator.Json5File.load_from_files(
            [json5_file_paths[2]]
        ).name_dictionaries, self._root_group)

        self._include_paths = _get_include_paths(all_properties)
        self._outputs = {
            'ComputedStyleBase.h': self.generate_base_computed_style_h,
            'ComputedStyleBase.cpp': self.generate_base_computed_style_cpp,
            'ComputedStyleBaseConstants.h': self.generate_base_computed_style_constants,
        }

    @template_expander.use_jinja('templates/ComputedStyleBase.h.tmpl', tests={'in': lambda a, b: a in b})
    def generate_base_computed_style_h(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'enums': self._generated_enums,
            'include_paths': self._include_paths,
            'computed_style': self._root_group,
            'diff_functions_map': self._diff_functions_map,
        }

    @template_expander.use_jinja('templates/ComputedStyleBase.cpp.tmpl', tests={'in': lambda a, b: a in b})
    def generate_base_computed_style_cpp(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'enums': self._generated_enums,
            'include_paths': self._include_paths,
            'computed_style': self._root_group,
            'diff_functions_map': self._diff_functions_map,
        }

    @template_expander.use_jinja('templates/ComputedStyleBaseConstants.h.tmpl')
    def generate_base_computed_style_constants(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'enums': self._generated_enums,
        }

if __name__ == '__main__':
    json5_generator.Maker(ComputedStyleBaseWriter).main()

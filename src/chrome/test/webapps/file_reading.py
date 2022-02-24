#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parsing logic to read files for the Web App testing framework.
"""

from collections import defaultdict
import logging
import os
import re
from typing import Dict, List, Set, Tuple

from models import Action
from models import ActionCoverage
from models import ActionType
from models import ActionsByName
from models import CoverageTest
from models import PartialAndFullCoverageByBaseName
from models import TestIdsByPlatform
from models import TestIdsByPlatformSet
from models import TestPartitionDescription
from models import TestPlatform

MIN_COLUMNS_ACTIONS_FILE = 9
MIN_COLUMNS_SUPPORTED_ACTIONS_FILE = 5
MIN_COLUMNS_UNPROCESSED_COVERAGE_FILE = 6


def human_friendly_name_to_canonical_action_name(
        human_friendly_action_name: str,
        action_base_name_to_default_mode: Dict[str, str]):
    """
    Converts a human-friendly action name (used in the spreadsheet) and turns
    into the format compatible with this testing framework. This does two
    things:
    1) Resolving specified modes, from "()" format into a "_". For example,
       "action(with_mode)" turns into "action_with_mode".
    2) Resolving modeless actions (that have a default mode) to
       include the default mode. For example, if
       |action_base_name_to_default_mode| contains an entry for
       |human_friendly_action_name|, then that entry is appended to the action.
       "action" and {"action": "default_mode"} will respectively will return
       "action_default_mode".
    If neither of those cases apply, then the |human_friendly_action_name| is
    returned.
    """
    human_friendly_action_name = human_friendly_action_name.strip()
    if human_friendly_action_name in action_base_name_to_default_mode:
        # Handle default mode.
        human_friendly_action_name += "_" + action_base_name_to_default_mode[
            human_friendly_action_name]
    elif '(' in human_friendly_action_name:
        # Handle mode being specified.
        human_friendly_action_name = human_friendly_action_name.replace(
            "(", "_").rstrip(")")
    return human_friendly_action_name


def read_platform_supported_actions(csv_file
                                    ) -> PartialAndFullCoverageByBaseName:
    """Reads the action base names and coverage from the given csv file.

    Args:
        csv_file: The comma-separated-values file which lists action base names
                  and whether it is fully or partially supported.

    Returns:
        A dictionary of action base name to a set of partially supported
        and fully supported platforms.
    """
    actions_base_name_to_coverage: PartialAndFullCoverageByBaseName = {}
    column_offset_to_platform = {
        0: TestPlatform.MAC,
        1: TestPlatform.WINDOWS,
        2: TestPlatform.LINUX,
        3: TestPlatform.CHROME_OS
    }
    for i, row in enumerate(csv_file):
        if not row:
            continue
        if row[0].startswith("#"):
            continue
        if len(row) < MIN_COLUMNS_SUPPORTED_ACTIONS_FILE:
            raise ValueError(f"Row {i} does not contain enough entries. "
                             f"Got {row}.")
        action_base_name: str = row[0].strip()
        if action_base_name in actions_base_name_to_coverage:
            raise ValueError(f"Action base name '{action_base_name}' on "
                             f"row {i} is already specified.")
        if not re.fullmatch(r'[a-z_]+', action_base_name):
            raise ValueError(
                f"Invald action base name '{action_base_name}' on "
                f"row {i}. Please use snake_case.")
        fully_supported_platforms: Set[TestPlatform] = set()
        partially_supported_platforms: Set[TestPlatform] = set()
        for j, value in enumerate(row[1:5]):
            value = value.strip()
            if not value:
                continue
            if value == "🌕":
                fully_supported_platforms.add(column_offset_to_platform[j])
            elif value == "🌓":
                partially_supported_platforms.add(column_offset_to_platform[j])
            elif value != "🌑":
                raise ValueError(f"Invalid coverage '{value}' on row {i}. "
                                 f"Please use '🌕', '🌓', or '🌑'.")

        actions_base_name_to_coverage[action_base_name] = (
            partially_supported_platforms, fully_supported_platforms)
    return actions_base_name_to_coverage


def read_actions_file(
        actions_csv_file,
        supported_platform_actions: PartialAndFullCoverageByBaseName
) -> Tuple[ActionsByName, Dict[str, str]]:
    """Reads the actions comma-separated-values file.

    If modes are specified for an action in the file, then one action is
    added to the results dictionary per action_base_name + mode
    parameterized. A mode marked with a "*" is considered the default
    mode for that action.

    If output actions are specified for an action, then it will be a
    PARAMETERIZED action and the output actions will be resolved into the
    `Action.output_actions` field.

    See the README.md for more information about actions and action templates.

    Args:
        actions_csv_file: The comma-separated-values file read to parse all
                          actions.
        supported_platform_actions: A dictionary of platform to the actions that
                                    are fully or partially covered on that
                                    platform.

    Returns (actions_by_name,
             action_base_name_to_default_mode):
        actions_by_name:
            Index of all actions by action name.
        action_base_name_to_default_mode:
            Index of action base names to the default mode. Only populated
            for actions with default modes.

    Raises:
        ValueError: The input file is invalid.
    """
    actions_by_name: Dict[str, Action] = {}
    action_base_name_to_default_mode: Dict[str, str] = {}
    action_base_names: Set[str] = set()
    all_output_action_names: List[str] = []
    all_short_name: Set[str] = set()
    for i, row in enumerate(actions_csv_file):
        if not row:
            continue
        if row[0].startswith("#"):
            continue
        if len(row) < MIN_COLUMNS_ACTIONS_FILE:
            raise ValueError(f"Row {i!r} does not contain enough entries. "
                             f"Got {row}.")
        if row[8] == "Abandoned":
            continue

        action_base_name = row[0].strip()
        action_base_names.add(action_base_name)
        if not re.fullmatch(r'[a-z_]+', action_base_name):
            raise ValueError(f"Invald action base name {action_base_name} on "
                             f"row {i!r}. Please use snake_case.")
        short_name_base = row[3].strip()
        if not short_name_base or short_name_base in all_short_name:
            raise ValueError(
                f"Short name '{short_name_base}' on line {i!r} is "
                f"not populated or already used.")

        type = ActionType.STATE_CHANGE
        if action_base_name.startswith("check_"):
            type = ActionType.STATE_CHECK

        output_action_names = []
        output_actions_str = row[2].strip()
        if output_actions_str:
            type = ActionType.PARAMETERIZED
            # Output actions for parameterized actions can also specify (or
            # assume default) action modes (e.g. `do_action(Mode1)`) if the
            # parameterized action doesn't have a mode. However, they cannot be
            # fully resolved yet without reading all actions. So the resolution
            # must happen later.
            output_action_names = [
                output.strip() for output in output_actions_str.split("&")
            ]
            # Keep track of all specified output actions for error checking.
            # Resolve any parameters if they are specified.
            all_output_action_names.extend([
                name.replace("(", "_").rstrip(")")
                for name in output_action_names
            ])

        (partially_supported_platforms,
         fully_supported_platforms) = supported_platform_actions.get(
             action_base_name, (set(), set()))

        modes = [mode.strip() for mode in row[1].split("|")]
        if not modes:
            modes = [""]
        for mode in modes:
            if not re.fullmatch(r'([A-Z]\w*\*?)|', mode):
                raise ValueError(f"Invald action mode name {mode!r}) on row "
                                 f"{i!r}. Please use PascalCase.")
            if "*" in mode:
                action_base_name_to_default_mode[
                    action_base_name] = mode.rstrip("*")
            mode = mode.rstrip("*")
            name = action_base_name
            # Convert the `cpp_method` to Pascal-case
            cpp_method = ''.join(word.title()
                                 for word in action_base_name.split('_'))
            output_action_names_with_mode: List[str] = []
            short_name: str = ""
            if mode != "":
                # If the action has a real mode, then modify the name, short
                # name, output actions, and cpp method.
                name += "_" + mode
                short_name = short_name_base + mode
                output_action_names_with_mode = [
                    action_name + "_" + mode
                    for action_name in output_action_names
                ]
                cpp_method += "(\"" + mode + "\")"
            else:
                output_action_names_with_mode = output_action_names
                short_name = short_name_base
                cpp_method += "()"
            if name in actions_by_name:
                raise ValueError(f"Cannot add duplicate action {name} on row "
                                 f"{i!r}")

            action = Action(name, action_base_name, short_name, cpp_method,
                            type, fully_supported_platforms,
                            partially_supported_platforms)
            all_short_name.add(short_name)
            if output_action_names_with_mode:
                action._output_action_names = output_action_names_with_mode
            actions_by_name[action.name] = action

    unused_supported_actions = set(
        supported_platform_actions.keys()).difference(action_base_names)
    if unused_supported_actions:
        raise ValueError(f"Actions specified as suppored that are not in "
                         f"the actions list: {unused_supported_actions}.")

    # Filter out empty strings from the output_action_base_names.
    all_output_action_names = list(filter(len, all_output_action_names))
    # Make sure all output actions are either resolvable or are base names.
    for output_action_name in all_output_action_names:
        if (output_action_name not in actions_by_name
                and output_action_name not in action_base_names):
            raise ValueError(f"Could not find action for "
                             f"specified output action {output_action_name}.")
    # Resolve the output actions
    for action in actions_by_name.values():
        if action.type is not ActionType.PARAMETERIZED:
            continue
        assert (action._output_action_names)
        for output_action_name in action._output_action_names:
            # Output actions can specify a mode or assume default action modes
            # if the parameterized action doesn't have a mode.
            canonical_name = human_friendly_name_to_canonical_action_name(
                output_action_name, action_base_name_to_default_mode)
            if canonical_name in actions_by_name:
                action.output_actions.append(actions_by_name[canonical_name])
            else:
                # Having this lookup fail is a feature, it allows a
                # parameterized action to reference output actions that might
                # not all support every mode of the parameterized action. When
                # that mode is specified in a test case, then that action would
                # be excluded & one less test case would be generated.
                logging.info(f"Output action {canonical_name} not found for "
                             f"parameterized action {action.name}.")
        assert (action.output_actions)
    return (actions_by_name, action_base_name_to_default_mode)

def read_unprocessed_coverage_tests_file(
        coverage_csv_file, actions_by_name: ActionsByName,
        action_base_name_to_default_mode: Dict[str, str]
) -> List[CoverageTest]:
    """Reads the coverage tests comma-separated-values file.

    The coverage tests file can have blank entries in the test row, and does not
    have test names.

    Args:
        coverage_csv_file: The comma-separated-values file with all coverage
                           tests.
        actions_by_name: An index of action name to Action
        action_base_name_to_default_mode: An index of action base name to
                                           default mode, if there is one.

    Returns:
        A list of CoverageTests read from the file.

    Raises:
        ValueError: The input file is invalid.
    """
    missing_actions = []
    required_coverage_tests = []
    for i, row in enumerate(coverage_csv_file):
        if not row:
            continue
        if row[0].strip().startswith("#"):
            continue
        if len(row) < MIN_COLUMNS_UNPROCESSED_COVERAGE_FILE:
            raise ValueError(f"Row {i!r} does not have test actions: {row}")
        platforms = TestPlatform.get_platforms_from_chars(row[0])
        actions = []
        for action_name in row[4:]:
            action_name = action_name.strip()
            if "," in action_name:
                raise ValueError(f"Actions on row {i!r} cannot have "
                                 f"multiple modes: {action_name}")
            if action_name == "":
                continue
            action_name = human_friendly_name_to_canonical_action_name(
                action_name, action_base_name_to_default_mode)
            if action_name not in actions_by_name:
                missing_actions.append(action_name)
                logging.error(f"Could not find action on row {i!r}: "
                              f"{action_name}")
                continue
            actions.append(actions_by_name[action_name])
        coverage_test = CoverageTest(actions, platforms)
        required_coverage_tests.append(coverage_test)
    if missing_actions:
        raise ValueError(f"Actions missing from actions dictionary: "
                         f"{', '.join(missing_actions)}")
    return required_coverage_tests


def get_tests_in_browsertest(file: str) -> Dict[str, Set[TestPlatform]]:
    """
    Returns a dictionary of all test ids found to the set of detected platforms
    the test is enabled on.

    For reference, this is what a disabled test by a sheriff typically looks
    like:

    TEST_F(WebAppIntegrationTestBase, DISABLED_NavSiteA_InstallIconShown) {
        ...
    }

    In the above case, the test will be considered disabled on all platforms.
    This is what a test disabled by a sheriff on a specific platform looks like:

    #if BUILDFLAG(IS_WIN)
    #define MAYBE_NavSiteA_InstallIconShown DISABLED_NavSiteA_InstallIconShown
    #else
    #define MAYBE_NavSiteA_InstallIconShown NavSiteA_InstallIconShown
    #endif
    TEST_F(WebAppIntegrationTestBase, MAYBE_NavSiteA_InstallIconShown) {
        ...
    }

    In the above case, the test will be considered disabled on
    `TestPlatform.WINDOWS` and thus enabled on {`TestPlatform.MAC`,
    `TestPlatform.CHROME_OS`, and `TestPlatform.LINUX`}.
    """
    tests: Dict[str, Set[TestPlatform]] = {}
    # Attempts to only match test test name in a test declaration, where the
    # name contains the test id prefix. Purposefully allows any prefixes on
    # the test name (like MAYBE_ or DISABLED_).
    for match in re.finditer(fr'{CoverageTest.TEST_ID_PREFIX}(\w+)\)', file):
        test_id = match.group(1)
        tests[test_id] = set(TestPlatform)
        test_name = f"{CoverageTest.TEST_ID_PREFIX}{test_id}"
        if f"DISABLED_{test_name}" not in file:
            continue
        enabled_platforms: Set[TestPlatform] = tests[test_id]
        for platform in TestPlatform:
            # Search for macro that specifies the given platform before
            # the string "DISABLED_<test_name>".
            macro_for_regex = re.escape(platform.macro)
            # This pattern ensures that there aren't any '{' or '}' characters
            # between the macro and the disabled test name, which ensures that
            # the macro is applying to the correct test.
            if re.search(fr"{macro_for_regex}[^{{}}]+DISABLED_{test_name}",
                         file):
                enabled_platforms.remove(platform)
        if len(enabled_platforms) == len(TestPlatform):
            enabled_platforms.clear()
    return tests


def find_existing_and_disabled_tests(
        test_partitions: List[TestPartitionDescription]
) -> Tuple[TestIdsByPlatformSet, TestIdsByPlatform]:
    """
    Returns a dictionary of platform set to test id, and a dictionary of
    platform to disabled test ids.
    """
    existing_tests: TestIdsByPlatformSet = defaultdict(lambda: set())
    disabled_tests: TestIdsByPlatform = defaultdict(lambda: set())
    for partition in test_partitions:
        for file in os.listdir(partition.browsertest_dir):
            if not file.startswith(partition.test_file_prefix):
                continue
            platforms = frozenset(
                TestPlatform.get_platforms_from_browsertest_filename(file))
            filename = os.path.join(partition.browsertest_dir, file)
            with open(filename) as f:
                file = f.read()
                tests = get_tests_in_browsertest(file)
                for test_id in tests.keys():
                    if test_id in existing_tests[platforms]:
                        raise ValueError(f"Already found test {test_id}. "
                                         f"Duplicate test in {filename}")
                    existing_tests[platforms].add(test_id)
                for platform in platforms:
                    for test_id, enabled_platforms in tests.items():
                        if platform not in enabled_platforms:
                            disabled_tests[platform].add(test_id)
                logging.info(f"Found tests in {filename}:\n{tests.keys()}")
    return (existing_tests, disabled_tests)

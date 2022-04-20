# Copyright 2022 The Dawn & Tint Authors
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

import re

USE_PYTHON3 = True

NONINCLUSIVE_REGEXES = [
    r"(?i)black[-_]?list",
    r"(?i)white[-_]?list",
    r"(?i)gr[ea]y[-_]?list",
    r"(?i)(first class citizen)",
    r"(?i)black[-_]?hat",
    r"(?i)white[-_]?hat",
    r"(?i)gr[ea]y[-_]?hat",
    r"(?i)master",
    r"(?i)slave",
    r"(?i)\bhim\b",
    r"(?i)\bhis\b",
    r"(?i)\bshe\b",
    r"(?i)\bher\b",
    r"(?i)\bguys\b",
    r"(?i)\bhers\b",
    r"(?i)\bman\b",
    r"(?i)\bwoman\b",
    r"(?i)\she\s",
    r"(?i)\she$",
    r"(?i)^he\s",
    r"(?i)^he$",
    r"(?i)\she['|\u2019]d\s",
    r"(?i)\she['|\u2019]d$",
    r"(?i)^he['|\u2019]d\s",
    r"(?i)^he['|\u2019]d$",
    r"(?i)\she['|\u2019]s\s",
    r"(?i)\she['|\u2019]s$",
    r"(?i)^he['|\u2019]s\s",
    r"(?i)^he['|\u2019]s$",
    r"(?i)\she['|\u2019]ll\s",
    r"(?i)\she['|\u2019]ll$",
    r"(?i)^he['|\u2019]ll\s",
    r"(?i)^he['|\u2019]ll$",
    r"(?i)grandfather",
    r"(?i)\bmitm\b",
    r"(?i)\bcrazy\b",
    r"(?i)\binsane\b",
    r"(?i)\bblind\sto\b",
    r"(?i)\bflying\sblind\b",
    r"(?i)\bblind\seye\b",
    r"(?i)\bcripple\b",
    r"(?i)\bcrippled\b",
    r"(?i)\bdumb\b",
    r"(?i)\bdummy\b",
    r"(?i)\bparanoid\b",
    r"(?i)\bsane\b",
    r"(?i)\bsanity\b",
    r"(?i)red[-_]?line",
]

NONINCLUSIVE_REGEX_LIST = []
for reg in NONINCLUSIVE_REGEXES:
    NONINCLUSIVE_REGEX_LIST.append(re.compile(reg))

LINT_FILTERS = []


def _CheckNonInclusiveLanguage(input_api, output_api, source_file_filter=None):
    """Checks the files for non-inclusive language."""

    matches = []
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=source_file_filter):
        for line_num, line in f.ChangedContents():
            for reg in NONINCLUSIVE_REGEX_LIST:
                match = reg.search(line)
                if match:
                    matches.append(
                        "{} ({}): found non-inclusive language: {}".format(
                            f.LocalPath(), line_num, match.group(0)))

    if len(matches):
        return [
            output_api.PresubmitPromptWarning("Non-inclusive language found:",
                                              items=matches)
        ]

    return []


def _NonInclusiveFileFilter(file):
    filter_list = [
        "PRESUBMIT.py",  # Non-inclusive language check data
        "docs/tint/spirv-input-output-variables.md",  # External URL
        "test/tint/samples/compute_boids.wgsl ",  # External URL
    ]
    return file in filter_list


def _DoCommonChecks(input_api, output_api):
    results = []
    results.extend(
        input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckPatchFormatted(input_api,
                                                    output_api,
                                                    check_python=True))
    results.extend(
        input_api.canned_checks.CheckChangeHasDescription(
            input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckGNFormatted(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckChangeHasNoCrAndHasOnlyOneEol(
            input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckChangeHasNoTabs(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckChangeTodoHasOwner(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
            input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckDoNotSubmit(input_api, output_api))
    # Note, the verbose_level here should match what is set in tools/lint so
    # the same set of lint errors are reported on the CQ and Kokoro bots.
    results.extend(
        input_api.canned_checks.CheckChangeLintsClean(
            input_api, output_api, lint_filters=LINT_FILTERS, verbose_level=1))
    results.extend(
        _CheckNonInclusiveLanguage(input_api, output_api,
                                   _NonInclusiveFileFilter))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _DoCommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _DoCommonChecks(input_api, output_api)

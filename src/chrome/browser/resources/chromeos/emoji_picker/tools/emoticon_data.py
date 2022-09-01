# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 6))
sys.path.append(os.path.join(_CHROME_SOURCE, 'build/android/gyp'))

import argparse
import json
from util import build_utils


def process_emoticon_data(metadata):
    """Produce the emoticon data to be consumed by the emoji picker.
    Args:
        metadata (list(dict)): list of emoticon group data.

    Returns:
        list(dict): list of readily used emoticon groups.
    """
    return [{
        "group":
        group["group"],
        "emoji": [{
            "base": {
                "string": emoticon["value"],
                "name": emoticon["description"],
                "keywords": []
            },
            "alternates": []
        } for emoticon in group["emoticon"]]
    } for group in metadata]


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--metadata',
                        required=True,
                        help='emoji metadata ordering file as JSON')
    parser.add_argument('--output',
                        required=True,
                        help='output JSON file path')
    options = parser.parse_args(args)

    metadata_file = options.metadata
    output_file = options.output

    # Parse emoticon ordering data.
    metadata = []
    with open(metadata_file, 'r') as file:
        metadata = json.load(file)

    emoticon_data = process_emoticon_data(metadata)

    # Write output file atomically in utf-8 format.
    with build_utils.AtomicOutput(output_file) as tmp_file:
        tmp_file.write(
            json.dumps(emoticon_data,
                       separators=(',', ':'),
                       ensure_ascii=False).encode('utf-8'))


if __name__ == '__main__':
    main(sys.argv[1:])

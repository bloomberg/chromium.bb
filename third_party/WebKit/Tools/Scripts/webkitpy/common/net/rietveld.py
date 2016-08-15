# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions to communicate with Rietveld."""

import collections
import json
import logging
import urllib2

from webkitpy.common.net.buildbot import Build


_log = logging.getLogger(__name__)

BASE_CODEREVIEW_URL = 'https://codereview.chromium.org/api'


def latest_try_jobs(issue_number, builder_names, web, patchset_number=None):
    """Returns a list of Build objects for jobs on the latest patchset.

    Args:
        issue_number: A Rietveld issue number.
        builder_names: Builders that we're interested in; try jobs for only
            these builders will be listed.
        web: webkitpy.common.net.web.Web object (which can be mocked out).
        patchset_number: Use a specific patchset instead of the latest one.

    Returns:
        A list of Build objects for the latest job for each builder, on the
        latest patchset. If none were found, an empty list is returned.
    """
    try:
        if patchset_number:
            url = _patchset_url(issue_number, patchset_number)
        else:
            url = _latest_patchset_url(issue_number, web)
        patchset_data = _get_json(url, web)
    except (urllib2.URLError, ValueError):
        return []
    jobs = []
    for job in patchset_data['try_job_results']:
        if job['builder'] not in builder_names:
            continue
        jobs.append(Build(
            builder_name=job['builder'],
            build_number=job['buildnumber']))
    return filter_latest_jobs(jobs)


def changed_files(issue_number, web):
    """Lists the files included in a CL, or None if this can't be determined.

    File paths are sorted and relative to the repository root.
    """
    try:
        url = _latest_patchset_url(issue_number, web)
        issue_data = _get_json(url, web)
        return sorted(issue_data['files'])
    except (urllib2.URLError, ValueError, KeyError):
        _log.warning('Failed to list changed files for issue %s.', issue_number)
        return None


def _latest_patchset_url(issue_number, web):
    issue_data = _get_json(_issue_url(issue_number), web)
    latest_patchset_number = issue_data["patchsets"][-1]
    return _patchset_url(issue_number, latest_patchset_number)


def _get_json(url, web):
    """Fetches JSON from a URL, and logs errors if the request was unsuccessful.

    Raises:
        urllib2.URLError: Something went wrong with the request.
        ValueError: The response wasn't valid JSON.
    """
    try:
        contents = web.get_binary(url)
    except urllib2.URLError:
        _log.error('Request failed to URL: %s', url)
        raise
    try:
        return json.loads(contents)
    except ValueError:
        _log.error('Invalid JSON: %s', contents)
        raise


def _issue_url(issue_number):
    return '%s/%s' % (BASE_CODEREVIEW_URL, issue_number)


def _patchset_url(issue_number, patchset_number):
    return '%s/%s' % (_issue_url(issue_number), patchset_number)


def filter_latest_jobs(jobs):
    """Filters out the list of jobs to include only the latest for each builder.

    Args:
        jobs: A list of Build objects.

    Returns:
        A list of Build objects such that only the latest job for each builder
        is kept.
    """
    builder_to_highest_number = {}
    for j in jobs:
        if j.build_number > builder_to_highest_number.get(j.builder_name, 0):
            builder_to_highest_number[j.builder_name] = j.build_number
    return [j for j in jobs if (
        j.builder_name in builder_to_highest_number and
        builder_to_highest_number[j.builder_name] == j.build_number
    )]


def get_latest_try_job_results(issue_number, web):
    url = _latest_patchset_url(issue_number, web)
    patchset_data = _get_json(url, web)
    results = {}
    for job in patchset_data['try_job_results']:
        results[job['builder']] = job['result']
    return results

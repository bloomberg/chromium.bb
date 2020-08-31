# Chromite Development: Starter Guide

[TOC]

## Objective

This doc tries to give an overview and head start to anyone just starting out on
Chromite development.

## Background

Before you get started on Chromite, we recommend that you go through ChromeOS
developer guides at
[external (first)](https://chromium.googlesource.com/chromiumos/docs/+/master/developer_guide.md)
and then [goto/chromeos-building](http://goto/chromeos-building) for internal.
The
[Gerrit starter guide](https://sites.google.com/a/google.com/android/development/repo-gerrit-git-workflow)
may also be helpful. You should flash a built image on a test device (Ask around
for one!).

Chromite was intended to be the unified codebase for anything related to
building ChromeOS/ChromiumOS. Currently, it is the codebase responsible for
several things including: building the OS from the requisite packages for the
necessary board (`parallel_emerge`), driving the infrastructure build workflow
(CBuildBot), hosting a Google App Engine App, and providing utility functions
for various scripts scattered around ChromeOS repositories. It is written for
the most part in Python with some Bash sprinkled in.

## Directory Overview

You can use [Code Search](https://cs.corp.google.com/) to lookup things in
Chromite or ChromeOS in general. You can add a ChromeOS filter to only show
files from CrOS repositories by going to
[CS Settings](https://cs.corp.google.com/settings/) and adding a new Saved
query: “`package:^chromeos`” named “chromeos”.

### chromite/api

The Chromite API for the CI system. The API exposes a subset of the chromite
functionality that needs to be strictly maintained as much as possible.

### chromite/cbuildbot

CBuildBot is the collection of entire code that runs on both the parent and the
child build machines. It kicks off the individual stages in a particular build.
It is a configurable bot that builds ChromeOS. More details on CBuildBot can be
found in
[this tech talk](https://drive.google.com/a/google.com/file/d/0BwPS_JpKyELWR2k0Z3JSWUhPSEE/view)
([slides](https://docs.google.com/presentation/d/1nUZFCAADgPp48SmrAFZVV_ngR27BdhKjL32nyu_hbOo/edit#slide=id.i0)).

### chromite/cbuildbot/builders

This folder contains configurations of the different builders in use. Each has
its own set of stages to run usually called under RunStages function. Most
builders used regularly are derived from SimpleBuilder class.

### chromite/cbuildbot/stages

Each file here has implementations of stages in the build process grouped by
similarity. Each stage usually has PerformStage as its primary function.

### chromite/docs

Additional documentation.

### chromite/lib

Code here is expected to be imported whenever necessary throughout Chromite.

### chromite/scripts

Unlike lib, code in scripts will not and should not be imported anywhere.
Instead they are executed as required in the build process. Each executable is
linked to either `wrapper.py` or `virtualenv_wrapper.py`. Some of these links
are in `chromite/bin`. The wrapper figures out the directory of the executable
script and the `$PYTHONPATH`. Finally, it invokes the correct Python
installation by moving up the directory structure to find which git repo is
making the call.

### chromite/service

These files act as the centralized business logic for processes, utilizing lib
for the implementation details. Any process that's implemented in chromite
should generally have an entry point somewhere in a service such that it can be
called from a script, the API, or anywhere else in lib where the process may be
useful.

### chromite/third_party

This folder contains all the third_party python libraries required by Chromite.
You need a very strong reason to add any library to the current list. Please
confirm with the owners beforehand.

### chromite/utils

This folder contains smaller, generic utility functionality that is not tied to
any specific entities in the codebase that would make them more at home in a lib
module.

### chromite/infra

This folder contains the chromite-specific infra repos.

### chromite/test

This folder contains test-only utilities and helper functions used to make
writing tests in other modules easier.

### chromite/*

There are smaller folders with miscellaneous functions like config, licencing,
cidb, etc.

## Testing your Chromite changes

Before any testing, you should check your code for lint errors with:

```shell
$ cros lint <filename>
```

### Unit Tests

Chromite now uses [pytest](https://docs.pytest.org/en/latest/) for running and
writing unit tests. All new code & tests should be written with the expectation
to be run under pytest.

Pytest is responsible for running unit tests under Python 3, with the legacy
unit test runner `scripts/run_tests` responsible for running unit tests under
Python 2.

### Running Chromite's unit tests

Chromite has two unit test runners: `scripts/run_pytest` and
`scripts/run_tests`, and two top-level entry points to the unit test suite:
`run_pytest` and `run_tests`.

Every Python file in Chromite is accompanied by a corresponding `*_unittest.py`
file. Running a particular file's unit tests is best done via
```shell
~/trunk/chromite $ ./run_pytest example_file_unittest.py
```

#### scripts/run_pytest

This script initializes a Python 3 virtualenv with necessary test dependencies
and runs `pytest` inside that virtualenv over all tests in Chromite, with the
configuration specified in [pytest.ini](./pytest.ini). The default configuration
runs tests in parallel and skips some tests known to be flaky or take a very
long time.

#### scripts/run_tests

This script was the unit test runner prior to Chromite transitioning to using
pytest. It is responsible for running unit tests under Python 2 while Chromite
is in the process of migrating to Python 3. Once Chromite runs under Python 3
only, this script will be deprecated and removed.

#### run_pytest

This top-level script is a convenience wrapper for `scripts/run_pytest` and has
identical behavior.

#### run_tests

This top-level script runs both `scripts/run_tests` and `scripts/run_pytest` as
a convenience wrapper for the full testing run done in the CQ. This script
accepts no arguments, so if you want to customize test run behavior you should
run one of `scripts/run_pytest` or `scripts/run_tests` directly.

### Writing unit tests

As of writing this document (April 2020), Chromite is still in the process of
migrating to run entirely under Python 3. New `*_unittest.py` files can assume
that they are run on Python 3 only, but existing files must maintain
backwards compatibility unless marked with a `sys.version` assertion at the
top of the file.

Chromite's unit tests make use of pytest
[fixtures](https://doc.pytest.org/en/latest/fixture.html). Fixtures that are
defined in a
[`conftest.py`](https://doc.pytest.org/en/latest/fixture.html#conftest-py-sharing-fixture-functions)
file are visible to tests in the same directory and all child directories. If
it's unclear where a test function is getting an argument from, try searching
for a fixture with that argument's name in a `conftest.py` file.

Be sure to consult pytest's
[excellent documentation](https://doc.pytest.org/en/latest/contents.html) for
guidance on how to take advantage of the features pytest offers when writing
unit tests.

Unit tests must clean up after themselves and in particular must not leak child
processes after running. There is no guaranteed order in which tests are run or
that tests are even run in the same process.

### Pre-CQ

Once you mark your CL as Commit-Queue +1 on the
[Chromium Gerrit](https://chromium-review.googlesource.com), the PreCQ will pick
up your change and fire few preset config runs as a precursor to CQ.

### Commit Queue

This is the final step in getting your change pushed. CQ is the most
comprehensive of all tests. Once a CL is verified by CQ, it is merged into the codebase.

## How does ChromeOS build work?

Refer to these
[talk slides](https://docs.google.com/presentation/d/1q8POSy8-LgqVvZu37KeXdd2-6F_4CpnfPzqu1fDlnW4)
on ChromeOS Build Overview.

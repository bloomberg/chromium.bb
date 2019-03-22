# CQ

This document describes how the Chromium Commit Queue (CQ) is structured and
managed. This is specific for the Chromium CQ. Questions about other CQs should
be directed to infra-dev@chromium.org.

[TOC]

## Purpose

The Chromium CQ exists to test developer changes before they land into
[chromium/src](https://chromium.googlesource.com/chromium/src/). It runs all the
test suites which a given CL affects, and ensures that they all pass.

## Options

* `COMMIT=false`

  You can mark a CL with this if you are working on experimental code and do not
  want to risk accidentally submitting it via the CQ. The CQ will immediately
  stop processing the change if it contains this option.

* `CQ_INCLUDE_TRYBOTS=<trybots>`

  This flag allows you to specify some additional bots to run for this CL, in
  addition to the default bots. The format for the list of trybots is
  "bucket:trybot1,trybot2;bucket2:trybot3".

* `NOPRESUBMIT=true`

  If you want to skip the presubmit check, you can add this line, and the commit
  queue won't run the presubmit for your change. This should only be used when
  there's a bug in the PRESUBMIT scripts. Please check that there's a bug filed
  against the bad script, and if there isn't, [file one](https://crbug.com/new).

* `NOTREECHECKS=true`

  Add this line if you want to skip the tree status checks. This means the CQ
  will commit a CL even if the tree is closed. Obviously this is strongly
  discouraged, since the tree is usually closed for a reason. However, in rare
  cases this is acceptable, primarily to fix build breakages (i.e., your CL will
  help in reopening the tree).

* `NOTRY=true`

  This should only be used for reverts to green the tree, since it skips try
  bots and might therefore break the tree. You shouldn't use this otherwise.

* `TBR=<username>`

  [See policy](https://chromium.googlesource.com/chromium/src/+/master/docs/code_reviews.md#TBR-To-Be-Reviewed)
  of when it's acceptable to use TBR ("To be reviewed"). If a change has a TBR
  line with a valid reviewer, the CQ will skip checks for LGTMs.

## FAQ

### What exactly does CQ run?

CQ runs the jobs specified in [cq.cfg](../../infra/config/branch/cq.cfg). See
[`cq_builders.md`](cq_builders.md) for an auto generated file with links to
information about the builders on the CQ.

Some of these jobs are experimental. This means they are executed on a
percentage of CQ builds, and the outcome of the build doesn't affect if the CL
can land or not. See the schema linked at the top of the file for more
information on what the fields in the config do.

### Why did my CL fail the CQ?

Please follow these general guidelines:
1. Check to see if your patch caused the build failures, and fix if possible.
1. If compilation or individual tests are failing on one or more CQ bots and you
   suspect that your CL is not responsible, please contact your friendly
   neighborhood sheriff by filing a
   [sheriff bug](https://bugs.chromium.org/p/chromium/issues/entry?template=Defect%20report%20from%20developer&labels=Sheriff-Chromium&summary=%5BBrief%20description%20of%20problem%5D&comment=What%27s%20wrong?).
   If the code in question has appropriate OWNERS, consider contacting or CCing
   them.
1. If other parts of CQ bot execution (e.g. `bot_update`) are failing, or you
   have reason to believe the CQ itself is broken, or you can't really
   tell what's wrong, please file a [trooper bug](g.co/bugatrooper).

In both cases, when filing bugs, please include links to the build and/or CL
(including relevant patchset information) in question.

### How do I add a new builder to the CQ?

There are several requirements for a builder to be added to the Commit Queue.

* All the code for this configuration must be in Chromium's public repository or
  brought in through [src/DEPS](../../DEPS).
* Setting up the build should be straightforward for a Chromium developer
  familiar with existing configurations.
* Tests should use existing test harnesses i.e.
  [gtest](../../third_party/googletest).
* It should be possible for any committer to replicate any testing run; i.e.
  tests and their data must be in the public repository.
* Median cycle time needs to be under 40 minutes for trybots. 90th percentile
  should be around an hour (preferrably shorter).
* Configurations need to catch enough failures to be worth adding to the CQ.
  Running builds on every CL requires a significant amount of compute resources.
  If a configuration only fails once every couple of weeks on the waterfalls,
  then it's probably not worth adding it to the commit queue.

Please email dpranke@chromium.org, who will approve new build configurations.

## Flakiness

The CQ can sometimes be flaky. Flakiness is when a test on the CQ fails, but
should have passed (commonly known as a false negative). There are a few common
causes of flaky tests on the CQ:

* Machine issues; weird system processes running, running out of disk space,
  etc...
* Test issues; individual tests not being independent and relying on the order
  of tests being run, not mocking out network traffic or other real world
  interactions.

CQ handles flakiness mainly by retrying tests. It retries at a few different
levels:

1. Per test retries.

   Most test suites have test retries built into them, which
   retry failed tests a few times.
1. Per build retries.

   After a test suite fails in a build, the build will retry the test suite
   again, both without patch, and with the patch applied.
1. Per CQ run retries.

   If a build fails, CQ will retry the individual trybot which failed.

## Help!

Have other questions? Run into any issues with the CQ? Email
infra-dev@chromium.org, or file a [trooper bug](g.co/bugatrooper).

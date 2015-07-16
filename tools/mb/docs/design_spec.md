# The MB (Meta-Build wrapper) design spec

[TOC]

## Intro

MB is intended to address two major aspects of the GYP -> GN transition
for Chromium:

1. "bot toggling" - make it so that we can easily flip a given bot
   back and forth between GN and GYP.

2. "bot configuration" - provide a single source of truth for all of
   the different configurations (os/arch/`gyp_define` combinations) of
   Chromium that are supported.

MB must handle at least the `gen` and `analyze` steps on the bots, i.e.,
we need to wrap both the `gyp_chromium` invocation to generate the
Ninja files, and the `analyze` step that takes a list of modified files
and a list of targets to build and returns which targets are affected by
the files.

## Design

MB is intended to be as simple as possible, and to defer as much work as
possible to GN or GYP. It should live as a very simple Python wrapper
that offers little in the way of surprises.

### Command line

It is structured as a single binary that supports a list of subcommands:

* `mb gen -c linux_rel_bot //out/Release`
* `mb analyze -m tryserver.chromium.linux -b linux_rel /tmp/input.json /tmp/output.json`

### Configurations

`mb` looks in the `//tools/mb/mb_config.pyl` config file to determine whether
to use GYP or GN for a particular build directory, and what set of flags
(`GYP_DEFINES` or `gn args`) to use.

A config can either be specified directly (useful for testing) or by specifying
the master name and builder name (useful on the bots so that they do not need
to specify a config directly and can be hidden from the details).

See the [user guide](user_guide.md#mb_config.pyl) for details.

### Handling the analyze step

The interface to `mb analyze` is described in the
[user\_guide](user_guide.md#mb_analyze).

Since the interface basically mirrors the way the "analyze" step on the bots
invokes gyp\_chromium today, when the config is found to be a gyp config,
the arguments are passed straight through.

It implements the equivalent functionality in GN by calling `'gn refs
[list of files] --type=executable --all --as=output` and filtering the
output to match the list of targets.

## Detailed Design Requirements and Rationale

This section is collection of semi-organized notes on why MB is the way
it is ...

### in-tree or out-of-tree

The first issue is whether or not this should exist as a script in
Chromium at all; an alternative would be to simply change the bot
configurations to know whether to use GYP or GN, and which flags to
pass.

That would certainly work, but experience over the past two years
suggests a few things:

  * we should push as much logic as we can into the source repositories
    so that they can be versioned and changed atomically with changes to
    the product code; having to coordinate changes between src/ and
    build/ is at best annoying and can lead to weird errors.
  * the infra team would really like to move to providing
    product-independent services (i.e., not have to do one thing for
    Chromium, another for NaCl, a third for V8, etc.).
  * we found that during the SVN->GIT migration the ability to flip bot
    configurations between the two via changes to a file in chromium
    was very useful.

All of this suggests that the interface between bots and Chromium should
be a simple one, hiding as much of the chromium logic as possible.

### Why not have MB be smarter about de-duping flags?

This just adds complexity to the MB implementation, and duplicates logic
that GYP and GN already have to support anyway; in particular, it might
require MB to know how to parse GYP and GN values. The belief is that
if MB does *not* do this, it will lead to fewer surprises.

It will not be hard to change this if need be.

### Integration w/ gclient runhooks

On the bots, we will disable `gyp_chromium` as part of runhooks (using
`GYP_CHROMIUM_NO_ACTION=1`), so that mb shows up as a separate step.

At the moment, we expect most developers to either continue to use
`gyp_chromium` in runhooks or to disable at as above if they have no
use for GYP at all. We may revisit how this works once we encourage more
people to use GN full-time (i.e., we might take `gyp_chromium` out of
runhooks altogether).

### Config per flag set or config per (os/arch/flag set)?

Currently, mb_config.pyl does not specify the host_os, target_os, host_cpu, or
target_cpu values for every config that Chromium runs on, it only specifies
them for when the values need to be explicitly set on the command line.

Instead, we have one config per unique combination of flags only.

In other words, rather than having `linux_rel_bot`, `win_rel_bot`, and
`mac_rel_bot`, we just have `rel_bot`.

This design allows us to determine easily all of the different sets
of flags that we need to support, but *not* which flags are used on which
host/target combinations.

It may be that we should really track the latter. Doing so is just a 
config file change, however.

### Non-goals

* MB is not intended to replace direct invocation of GN or GYP for
  complicated build scenarios (aka ChromeOS), where multiple flags need
  to be set to user-defined paths for specific toolchains (e.g., where
  ChromeOS needs to specify specific board types and compilers).

* MB is not intended at this time to be something developers use frequently,
  or to add a lot of features to. We hope to be able to get rid of it once
  the GYP->GN migration is done, and so we should not add things for
  developers that can't easily be added to GN itself.

* MB is not intended to replace the 
  [CR tool](https://code.google.com/p/chromium/wiki/CRUserManual). Not
  only is it only intended to replace the gyp\_chromium part of `'gclient
  runhooks'`, it is not really meant as a developer-facing tool.

### Open issues

* Some common flags (goma\_dir being the obvious one) may need to be
  specified via the user, and it's unclear how to integrate this with
  the concept of build\_configs.
  
  Right now, MB has hard-coded support for a few flags (i.e., you can
  pass the --goma-dir flag, and it will know to expand "${goma\_dir}" in
  the string before calling out to the tool. We may want to generalize
  this to a common key/value approach (perhaps then meeting the
  ChromeOS non-goal, above), or we may want to keep this very strictly
  limited for simplicity.

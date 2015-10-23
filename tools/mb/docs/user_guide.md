# The MB (Meta-Build wrapper) user guide

[TOC]

## Introduction

`mb` is a simple python wrapper around the GYP and GN meta-build tools to
be used as part of the GYP->GN migration.

It is intended to be used by bots to make it easier to manage the configuration
each bot builds (i.e., the configurations can be changed from chromium
commits), and to consolidate the list of all of the various configurations
that Chromium is built in.

Ideally this tool will no longer be needed after the migration is complete.

For more discussion of MB, see also [the design spec](design_spec.md).

## MB subcommands

### `mb analyze`

`mb analyze` is reponsible for determining what targets are affected by
a list of files (e.g., the list of files in a patch on a trybot):

```
mb analyze -c chromium_linux_rel //out/Release input.json output.json
```

Either the `-c/--config` flag or the `-m/--master` and `-b/--builder` flags
must be specified so that `mb` can figure out which config to use.

The first positional argument must be a GN-style "source-absolute" path
to the build directory.

The second positional argument is a (normal) path to a JSON file containing
a single object with two fields:

  * `files`: an array of the modified filenames to check (as
    paths relative to the checkout root).
  * `targets`: an array of the unqualified target names to check.

The third positional argument is a (normal) path to where mb will write
the result, also as a JSON object. This object may contain the following
fields:

  * `error`: this should only be present if something failed.
  * `targets`: the subset of the input `targets` that depend on the
    input `files`.
  * `build_targets`: the minimal subset of targets needed to build all
    of `targets` that were affected.
  * `status`: one of three strings:
    * `"Found dependency"` (build the `build_targets`)
    * `"No dependency"` (i.e., no build needed)
    * `"Found dependency (all)"` (build everything, in which case
      `targets` and `build_targets` are not returned).

The `-b/--builder`, `-c/--config`, `-f/--config-file`, `-m/--master`,
`-q/--quiet`, and `-v/--verbose` flags work as documented for `mb gen`.


### `mb gen`

`mb gen` is responsible for generating the Ninja files by invoking either GYP
or GN as appropriate. It takes arguments to specify a build config and
a directory, then runs GYP or GN as appropriate:

```
% mb gen -m tryserver.chromium.linux -b linux_rel //out/Release
% mb gen -c linux_rel_trybot //out/Release
```

Either the `-c/--config` flag or the `-m/--master` and `-b/--builder` flags
must be specified so that `mb` can figure out which config to use.

By default, MB will look in `//tools/mb/mb_config.pyl` to look up the config
information, but you can specify a custom config file using the
`-f/--config-file` flag.

The path must be a GN-style "source-absolute" path (as above).

You can pass the `-n/--dryrun` flag to mb gen to see what will happen without
actually writing anything.

You can pass the `-q/--quiet` flag to get mb to be silent unless there is an
error, and pass the `-v/--verbose` flag to get mb to log all of the files
that are read and written, and all the commands that are run.

If the build config will use the Goma distributed-build system, you can pass
the path to your Goma client in the `-g/--goma-dir` flag, and it will be
incorporated into the appropriate flags for GYP or GN as needed.

If gen ends up using GYP, the path must have a valid GYP configuration as the
last component of the path (i.e., specify `//out/Release_x64`, not `//out`).

### `mb help`

Produces help output on the other subcommands

### `mb lookup`

Prints what command will be run by `mb gen` (like `mb gen -n` but does
not require you to specify a path).

The `-b/--builder`, `-c/--config`, `-f/--config-file`, `-m/--master`,
`-q/--quiet`, and `-v/--verbose` flags work as documented for `mb gen`.

### `mb validate`

Does internal checking to make sure the config file is syntactically
valid and that all of the entries are used properly. It does not validate
that the flags make sense, or that the builder names are legal or
comprehensive, but it does complain about configs and mixins that aren't
used.

The `-f/--config-file` and `-q/--quiet` flags work as documented for
`mb gen`.

This is mostly useful as a presubmit check and for verifying changes to
the config file.

## Isolates and Swarming

`mb gen` is also responsible for generating the `.isolate` and
`.isolated.gen.json` files needed to run test executables through swarming
in a GN build (in a GYP build, this is done as part of the compile step).

If you wish to generate the isolate files, pass `mb gen` the
`--swarming-targets-file` command line argument; that arg should be a path
to a file containing a list of ninja build targets to compute the runtime
dependencies for (on Windows, use the ninja target name, not the file, so
`base_unittests`, not `base_unittests.exe`).

MB will take this file, translate each build target to the matching GN
label (e.g., `base_unittests` -> `//base:base_unittests`, write that list
to a file called `runtime_deps` in the build directory, and pass that to
`gn gen $BUILD ... --runtime-deps-list-file=$BUILD/runtime_deps`.

Once GN has computed the lists of runtime dependencies, MB will then
look up the command line for each target (currently this is hard-coded
in [mb.py](https://code.google.com/p/chromium/codesearch?q=mb.py#chromium/src/tools/mb/mb.py&q=mb.py%20GetIsolateCommand&sq=package:chromium&type=cs)), and write out the
matching `.isolate` and `.isolated.gen.json` files.

## The `mb_config.pyl` config file

The `mb_config.pyl` config file is intended to enumerate all of the
supported build configurations for Chromium. Generally speaking, you
should never need to (or want to) build a configuration that isn't
listed here, and so by using the configs in this file you can avoid
having to juggle long lists of GYP_DEFINES and gn args by hand.

`mb_config.pyl` is structured as a file containing a single PYthon Literal
expression: a dictionary with three main keys, `masters`, `configs` and
`mixins`.

The `masters` key contains a nested series of dicts containing mappings
of master -> builder -> config . This allows us to isolate the buildbot
recipes from the actual details of the configs.

The `configs` key points to a dictionary of named build
configurations.

There should be an key in this dict for every supported configuration
of Chromium, meaning every configuration we have a bot for, and every
configuration commonly used by develpers but that we may not have a bot
for.

The value of each key is a list of "mixins" that will define what that
build_config does. Each item in the list must be an entry in the dictionary
value of the `mixins` key.

Each mixin value is itself a dictionary that contains one or more of the
following keys:

  * `gyp_crosscompile`: a boolean; if true, GYP_CROSSCOMPILE=1 is set in
    the environment and passed to GYP.
  * `gyp_defines`: a string containing a list of GYP_DEFINES.
  * `gn_args`: a string containing a list of values passed to gn --args.
  * `mixins`: a list of other mixins that should be included.
  * `type`: a string with either the value `gyp` or `gn`;
    setting this indicates which meta-build tool to use.

When `mb gen` or `mb analyze` executes, it takes a config name, looks it
up in the 'configs' dict, and then does a left-to-right expansion of the
mixins; gyp_defines and gn_args values are concatenated, and the type values
override each other.

For example, if you had:

```
{
  'configs`: {
    'linux_release_trybot': ['gyp_release', 'trybot'],
    'gn_shared_debug': None,
  }
  'mixins': {
    'bot': {
      'gyp_defines': 'use_goma=1 dcheck_always_on=0',
      'gn_args': 'use_goma=true dcheck_always_on=false',
    },
    'debug': {
      'gn_args': 'is_debug=true',
    },
    'gn': {'type': 'gn'},
    'gyp_release': {
      'mixins': ['release'],
      'type': 'gyp',
    },
    'release': {
      'gn_args': 'is_debug=false',
    }
    'shared': {
      'gn_args': 'is_component_build=true',
      'gyp_defines': 'component=shared_library',
    },
    'trybot': {
      'gyp_defines': 'dcheck_always_on=1',
      'gn_args': 'dcheck_always_on=true',
    }
  }
}
```

and you ran `mb gen -c linux_release_trybot //out/Release`, it would
translate into a call to `gyp_chromium -G Release` with `GYP_DEFINES` set to
`"use_goma=true dcheck_always_on=false dcheck_always_on=true"`.

(From that you can see that mb is intentionally dumb and does not
attempt to de-dup the flags, it lets gyp do that).

## Debugging MB

By design, MB should be simple enough that very little can go wrong.

The most obvious issue is that you might see different commands being
run than you expect; running `'mb -v'` will print what it's doing and
run the commands; `'mb -n'` will print what it will do but *not* run
the commands.

If you hit weirder things than that, add some print statements to the
python script, send a question to gn-dev@chromium.org, or
[file a bug](https://crbug.com/new) with the label
'mb' and cc: dpranke@chromium.org.



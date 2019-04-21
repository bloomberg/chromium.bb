# Open Screen Library

This library implements the Open Screen Protocol.  Information about the
protocol can be found in the Open Screen [GitHub
repository](https://github.com/webscreens/openscreenprotocol).

## Getting the code

### Depot Tools

Openscreen dependencies are managed using gclient, from the depot_tools repo.
To get gclient, run the following command in your terminal:
```bash
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```
Then add the depot_tools folder to your PATH environment variable.

For more setup information on depot_tools, see:
https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up

### Checking out

From the parent directory of where you want the openscreen checkout (typically
/usr/local/src), configure gclient and check out openscreen with the
following commands:

```bash
    gclient config https://chromium.googlesource.com/openscreen
    gclient sync
```

Now, you should have openscreen checked out, with all module dependencies
checked out to the appropriate revision.

## Continuous Build

Open Screen uses [LUCI builders](https://ci.chromium.org/p/openscreen/builders)
to monitor the build and test health of the library.

Coming soon: tryjob and submit queue integration with Gerrit code review.

## Build guide

Open Screen Library code should follow the
[Open Screen Library Style Guide](docs/style_guide.md).  In addition, you should
also run `//PRESUBMIT.sh` before uploading changes for review (which primarily
checks formatting).

## Build dependencies

Run `./tools/install-build-tools.sh` from the root source directory to obtain a
copy of the following build tools:

 - Build file generator: `gn`
 - Code formatter: `clang-format`

 - Builder: `ninja`

   [GitHub releases](https://github.com/ninja-build/ninja/releases)


You also need to ensure that you have the compiler toolchain dependencies.
Currently, both Linux and Mac OS X build configurations use clang. On Linux,
we download the Clang compiler from the Google storage cache, the same way
that Chromium does it. On Mac OS X, we just use the clang instance provided
by XCode.

On Mac, ensure XCode is installed. On Linux, ensure that libstdc++ 8 is installed,
as clang depends on the system instance of it. On Debian flavors, you can run:

   sudo apt-get install libstdc++-8-dev

Finally, Passing the "is_gcc=true" flag on Linux enables building using gcc instead.
Note that g++ must be installed.

## Building an example with GN and Ninja

The following commands will build the current example executable and run it.

``` bash
  ./gn gen out/Default    # Creates the build directory and necessary ninja files
  ninja -C out/Default  # Builds the executable with ninja
  ./out/Default/hello     # Runs the executable
```

The `-C` argument to `ninja` works just like it does for GNU Make: it specifies
the working directory for the build.  So the same could be done as follows:

``` bash
  ./gn gen out/Default
  cd out/Default
  ninja
  ./hello
```

After editing a file, only `ninja` needs to be rerun, not `gn`.

## Gerrit

The following sections contain some tips about dealing with Gerrit for code
reviews, specifically when pushing patches for review.

### Uploading a new change

There is official [Gerrit
documentation](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-upload.html#push_create)
for this which essentially amounts to:

``` bash
  git push origin HEAD:refs/for/master
```

You may also wish to install the
[Change-Id commit-msg hook](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/cmd-hook-commit-msg.html).
This adds a `Change-Id` line to each commit message locally, which Gerrit uses
to track changes.  Once installed, this can be toggled with `git config
gerrit.createChangeId <true|false>`.

To download the commit-msg hook for the Open Screen repository, use the
following command:

```bash
  curl -Lo .git/hooks/commit-msg https://chromium-review.googlesource.com/tools/hooks/commit-msg
```

Gerrit keeps track of changes using a [Change-Id
line](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-changeid.html)
in each commit.

When there is no `Change-Id` line, Gerrit creates a new `Change-Id` for the
commit, and therefore a new change.  Gerrit's documentation for
[replacing a change](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-upload.html#push_replace)
describes this.  So if you want to upload a new patchset to an existing review,
it should contain the matching `Change-Id` line in the commit message.

### Adding a new patchset to an existing change

By default, each commit to your local branch will get its own Gerrit change when
pushed, unless it has a `Change-Id` corresponding to an existing review.

If you need to modify commits on your local branch to ensure they have the
correct `Change-Id`, you can do one of two things:

After committing to the local branch, run:

```bash
  git commit --amend
  git show
```

to attach the current `Change-Id` to the most recent commit. Check that the
correct one was inserted by comparing it with the one shown on
`chromium-review.googlesource.com` for the existing review.

If you have made multiple local commits, you can squash them all into a single
commit with the correct Change-Id:

```bash
  git rebase -i HEAD~4
  git show
```

where '4' means that you want to squash three additional commits onto an
existing commit that has been uploaded for review.


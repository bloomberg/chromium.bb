# Third-Party Dependencies

The Open Screen library includes its dependencies as submodules in the source
tree under the `//third_party/` directory.  They are structured as follows:

```
  //third_party/<library>
   BUILD.gn
   ...other necessary adapter files...
   src/
     <library>'s source
```

## Adding a new dependency

When adding a new dependency to the project, you should first create a submodule
under `//third_party/` with its source.  For example, let's say we want to add a
new library called `alpha`.

``` bash
  git submodule add https://repo.com/path/to/alpha.git third_party/alpha/src
  git commit
```

Then you need to add a BUILD.gn file for it under `//third_party/alpha`,
assuming it doesn't already provide its own BUILD.gn.

## Roll a dependency to a new version

Rolling a dependency forward (or to any different version really) consists of
two steps:
  1. Put HEAD of the submodule repository at the desired commit (make sure this
     public before pushing the version-bump change).
  1. `git add` the submodule path and commit.

Once again, assume we have a dependency called `alpha` which we want to update:
``` bash
  cd third_party/alpha/src
  # pull new changes e.g. git pull origin master
  git add third_party/alpha/src
  git commit
```

Of course, you should also make sure that the new change is still compatible
with the rest of the project, including any adapter files under
`//third_party/<library>` (e.g. BUILD.gn).  Any necessary updates to make the
rest of the project work with the new dependency version should happen in the
same change.

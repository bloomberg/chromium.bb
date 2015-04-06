# How to update the [GN binaries](gn.md) that Chromium uses.

## Prerequisites

You'll need a checkout of Chromium, and commit access to the
`buildtools/` repo. Check with scottmg, thakis, brettw, dpranke, or a
member of chrome-infra to get access to that repo.

## Instructions

_Hopefully there will be a script that does this all for you shortly._

  1. cd to your Chromium checkout.
  2. Create a whitespace change in DEPS and upload it to create a "dummy" CL
  3. Run the following commands:
    1. git-cl try -b linux\_chromium\_gn\_upload\_x86
       -b linux\_chromium\_gn\_upload\_x64
       -r $GIT\_REVISION\_YOU\_WANT\_TO\_BUILD
    2. git-cl try -b mac\_chromium\_gn\_upload
       -r $GIT\_REVISION\_YOU\_WANT\_TO\_BUILD
    3. git-cl try -b win8\_chromium\_gn\_upload
       -r $GIT\_REVISION\_YOU\_WANT\_TO\_BUILD
  4. Wait for the try jobs to finish.
  5. If they all ran successfully, copy the digests from the tryjob build
     log output into `src/buildtools/{mac,linux32,linux64}gn.sha1` and
     `src/buildtools/win/gn.exe.sha1` as appropriate.
  6. Upload a buildtools CL with the updated digests and get it reviewed
     and committed. Make sure you note the revision of GN that you built
     against in the commit message.
  7. Go back to your "dummy" CL w/ the change to the DEPS file in src/
     and update the buildtools revision to your newly-committed
     buildtools change.
  8. Get that reviewed and landed in Chromium. Make sure you note the
     revision of GN that you built against in the commit message, along
     with the revision of buildtools that you're rolling to.

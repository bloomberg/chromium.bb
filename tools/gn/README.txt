GN "Generate Ninja"

GN is a meta-build system that generates ninja files. It's meant to be faster
and simpler than GYP.

Chromium uses binary versions of GN downloaded from Google Cloud Storage during
gclient runhooks, so that we don't have to worry about bootstrapping a build
of GN from scratch.

In order to make changes to GN and update the binaries, you must
have access to the "chromium-gn" bucket in Google Cloud Storage.  To date,
only a few Googlers have this access; if you don't have it, ask someone
who does for help :).

Assuming you have access to the bucket, do the following:

1) Make the changes to the GN source.

2) Upload a patch.

3) Get it reviewed and landed.

4) Obtain local build machines (with Chromium checkouts with your change)
   for Mac, Windows, and 32-bit and 64-bit Linux environments.

   On Linux, make sure you have the debian sysroots needed to do proper builds:
   For 32-bits:
   % export GYP_DEFINES="branding=Chrome buildtype=Official target_arch=ia32"
   For 64-bits:
   % export GYP_DEFINES="branding=Chrome buildtype=Official target_arch=x64"
   Then:
   % gclient runhooks

5) Sync each checkout to the revision of Chromium containing your change (or
   a suitable later one).

6) Ensure that you have initialized the gsutil credentials as described in
   buildtools/README.txt . The username should be the one that is on the ACL
   for the "chromium-gn" bucket (probably your @google.com address).
   Contact the build team for help getting access if necessary.

7) Run the checked-in GN binary to create a new build directory. Use the args
   specified in the buildtools/*/gn_args.txt files:

   On Linux:
   % gn gen //out/Release_gn_rel32 --args="<flags from buildtools/linux32/gn_args.txt>"
   % ninja -C out/Release_gn_rel32 gn
   % cp out/Release_gn_rel32/gn buildtools/linux32
   % buildtools/linux32/upload_gn
   % gn gen //out/Release_gn_rel64 --args="<flags from buildtools/linux32/gn_args.txt>"
   % ninja -C out/Release_gn_rel64 gn
   % cp out/Release_gn_rel64/gn buildtools/linux64
   % buildtools/linux64/upload_gn

   On Mac:
   % gn gen //out/Release_gn_rel --args="<flags from buildtools/mac/gn_args.txt>"
   % ninja -C out/Release_gn_rel gn
   % cp out/Release_gn_rel/gn buildtools/mac/
   % buildtools/mac/upload_gn

   On Win:
   > gn gen //out/Release_gn_rel --args="<flags from buildtools/win/gn_args.txt>"
   > ninja -C out\Release_gn_rel gn
   > copy out\Release_gn_rel\gn.exe buildtools\win
   > buildtools\win\upload_gn.bat

7) The uploads in the previous step will produce updated .sha1 files in
   src/buildtools/*/*.sha1 . Those hashes need to be uploaded, reviewed,
   and committed into the buildtools repo.

8) Once buildtools has been uploaded, roll the new version of buildtools/
   back into chrome via src/DEPS.

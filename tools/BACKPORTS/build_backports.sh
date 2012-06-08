#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ "x${OSTYPE}" = "xcygwin" ]]; then
  cd "$(cygpath "${PWD}")"
fi
if [[ ${PWD} != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [[ $# -ne 3 ]]; then
  echo "USAGE: $0 version_file win/mac/linux glibc/newlib"
  exit 2
fi

cd tools/BACKPORTS

if [[ "$(../glibc_revision.sh)" == "$(cat "$1.lastver")" ]]; then
  # Everything is already done
  exit 0
fi

set -x
set -e
set -u

declare -r GIT_BASE_URL=http://git.chromium.org/native_client

rm -f "$$.error"

# Checkout toolchain sources from git repo.  We'll need them later when we'll
# patch sources toolchain before calling build script.
for dirname in binutils gcc gdb glibc linux-headers-for-nacl newlib ; do
  repo="${dirname}"
  if [[ "$repo" != "linux-headers-for-nacl" ]]; then
    repo="nacl-${repo}"
  fi
  if [[ -d "$dirname" ]]; then (
    cd "$dirname"
    git pull --all
  ) else (
    git clone "${GIT_BASE_URL}/${repo}.git" "$dirname"
  ) fi || touch $$.error &
done

wait

# If we were unable to checkout some sources then it's time to stop.
if [[ -e "$$.error" ]]; then
  rm -f "$$.error"
  # Errors are reported by git above already
  exit 1
fi

# Here we'll checkout correct versions of all the sources for all supported
# ppapi versions.
while read name id comment ; do
  case "$name" in
    binutils | gcc | glibc | newlib | '' | \#*)
      # Skip this line
      ;;
    *)
    ( # First step is to use glient to sync sources.
      if [[ -d "$name" ]]; then
        cd "$name"
	if [[ "$2" == "win" ]]; then (
	  # Use extended globbing (cygwin should always have it).
	  shopt -s extglob
	  # Filter out cygwin python (everything under /usr or /bin, or *cygwin*).
	  export PATH=${PATH/#\/bin*([^:])/}
	  export PATH=${PATH//:\/bin*([^:])/}
	  export PATH=${PATH/#\/usr*([^:])/}
	  export PATH=${PATH//:\/usr*([^:])/}
	  export PATH=${PATH/#*([^:])cygwin*([^:])/}
	  export PATH=${PATH//:*([^:])cygwin*([^:])/}
	  gclient revert
	) else
	  gclient revert
	fi
      else
	mkdir "$name"
	cd "$name"
	cat >.gclient <<-END
	solutions = [
	  { "name"        : "native_client",
	    "url"         : "http://src.chromium.org/native_client/trunk/src/native_client@$id",
	    "deps_file"   : "DEPS",
	    "managed"     : True,
	    "custom_deps" : {
	    },
	    "safesync_url": "",
	  },
	]
	END
	if [[ "$2" == "win" ]]; then (
	  # Use extended globbing (cygwin should always have it).
	  shopt -s extglob
	  # Filter out cygwin python (everything under /usr or /bin, or *cygwin*).
	  export PATH=${PATH/#\/bin*([^:])/}
	  export PATH=${PATH//:\/bin*([^:])/}
	  export PATH=${PATH/#\/usr*([^:])/}
	  export PATH=${PATH//:\/usr*([^:])/}
	  export PATH=${PATH/#*([^:])cygwin*([^:])/}
	  export PATH=${PATH//:*([^:])cygwin*([^:])/}
	  gclient sync
	) else
	  gclient sync
	fi
      fi
      # Now we need to change versions to officialy mark binaries.  We don't
      # show git revision because we combine different branches here.
      patch -p0 <<-END
	--- native_client/buildbot/buildbot_standard.py
	+++ native_client/buildbot/buildbot_standard.py
	@@ -148 +148,2 @@
	-  with Step('cleanup_temp', status):
	+  if False:
	+   with Step('cleanup_temp', status):
	--- native_client/tools/glibc_download.sh
	+++ native_client/tools/glibc_download.sh
	@@ -42 +42 @@
	-  for ((j=glibc_revision+1;j<glibc_revision+revisions_count;j++)); do
	+  for ((j=\${glibc_revision%@*}+1;j<\${glibc_revision%@*}+revisions_count;j++)); do
	--- native_client/tools/glibc_revision.sh
	+++ native_client/tools/glibc_revision.sh
	@@ -13 +13 @@
	-for i in REVISIONS glibc_revision.sh Makefile ; do
	+for i in ../../../VERSIONS ../../../build_backports.sh REVISIONS glibc_revision.sh Makefile ; do
	@@ -20 +20 @@
	-echo "\$REVISION"
	+echo "\$REVISION@$name"
	END
      declare -r rev="$(native_client/tools/glibc_revision.sh)"
      patch -p0 <<-END
	--- native_client/tools/Makefile
	+++ native_client/tools/Makefile
	@@ -850 +850,2 @@
	-	echo "Native Client r\`LC_ALL=C svnversion\`, Git Commit \`cd SRC/gcc ; LC_ALL=C git rev-parse HEAD\`" > SRC/gcc/gcc/DEV-PHASE
	+	echo "Native Client r$rev" > SRC/gcc/gcc/DEV-PHASE
	+	cp -aiv SRC/gdb/gdb/version.in SRC/gdb/gdb/version.inT
	END
      if [[ "$name" == ppapi14 ]]; then
	patch -p0 <<-END
	--- native_client/tools/Makefile
	+++ native_client/tools/Makefile
	@@ -847 +847 @@
	-	+#define BFD_VERSION_STRING  @bfd_version_package@ @bfd_version_string@ \\" \`LC_ALL=C svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \\(....\\)-\\(..\\)-\\(..\\).*+\\1-\\2-\\3+'\` (Native Client r\`LC_ALL=C svnversion\`, Git Commit \`cd SRC/binutils ; LC_ALL=C git rev-parse HEAD\`)\\"\\n" |\\
	+	+#define BFD_VERSION_STRING  @bfd_version_package@ @bfd_version_string@ \\" \`cd ../../.. ; LC_ALL=C svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \\(....\\)-\\(..\\)-\\(..\\).*+\\1-\\2-\\3+'\` (Native Client r$rev)\\"\\n" |\\
	@@ -855 +855 @@
	-	+\`cat SRC/gdb/gdb/version.in\` \`LC_ALL=C svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \\(....\\)-\\(..\\)-\\(..\\).*+\\1-\\2-\\3+'\` (Native Client r\`LC_ALL=C svnversion\`, Git Commit \`cd SRC/gdb ; LC_ALL=C git rev-parse HEAD\`)\\n" |\\
	+	+\`cat SRC/gdb/gdb/version.in\` \`cd ../../.. ; LC_ALL=C svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \\(....\\)-\\(..\\)-\\(..\\).*+\\1-\\2-\\3+'\` (Native Client r$rev)\\n" |\\
	END
      else
	patch -p0 <<-END
	--- native_client/tools/Makefile
	+++ native_client/tools/Makefile
	@@ -847 +847 @@
	-	+#define BFD_VERSION_STRING  @bfd_version_package@ @bfd_version_string@ \\" \`LC_ALL=C svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \\(....\\)-\\(..\\)-\\(..\\).*+\\1\\2\\3+'\` (Native Client r\`LC_ALL=C svnversion\`, Git Commit \`cd SRC/binutils ; LC_ALL=C git rev-parse HEAD\`)\\"\\n" |\\
	+	+#define BFD_VERSION_STRING  @bfd_version_package@ @bfd_version_string@ \\" \`LC_ALL=C svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \\(....\\)-\\(..\\)-\\(..\\).*+\\1\\2\\3+'\` (Native Client r$rev)\\"\\n" |\\
	@@ -855 +855 @@
	-	+\`cat SRC/gdb/gdb/version.in\` \`LC_ALL=C svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \\(....\\)-\\(..\\)-\\(..\\).*+\\1\\2\\3+'\` (Native Client r\`LC_ALL=C svnversion\`, Git Commit \`cd SRC/gdb ; LC_ALL=C git rev-parse HEAD\`)\\n" |\\
	+	+\`cat SRC/gdb/gdb/version.in\` \`cd ../../.. LC_ALL=C svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \\(....\\)-\\(..\\)-\\(..\\).*+\\1\\2\\3+'\` (Native Client r$rev)\\n" |\\
	END
	if [[ "$name" != ppapi15 ]]; then
	  patch -p0 <<-END
	--- native_client/tools/Makefile
	+++ native_client/tools/Makefile
	@@ -950,5 +950,5 @@
	-BINUTILS_PATCHNAME := naclbinutils-\$(BINUTILS_VERSION)-r\$(shell svnversion | tr : _)
	-GCC_PATCHNAME := naclgcc-\$(GCC_VERSION)-r\$(shell svnversion | tr : _)
	-#GDB_PATCHNAME := naclgdb-\$(GDB_VERSION)-r\$(shell svnversion | tr : _)
	-GLIBC_PATCHNAME := naclglibc-\$(GLIBC_VERSION)-r\$(shell svnversion | tr : _)
	-NEWLIB_PATCHNAME := naclnewlib-\$(NEWLIB_VERSION)-r\$(shell svnversion | tr : _)
	+BINUTILS_PATCHNAME := naclbinutils-\$(BINUTILS_VERSION)-r\$(shell ./glibc_revision.sh)
	+GCC_PATCHNAME := naclgcc-\$(GCC_VERSION)-r\$(shell ./glibc_revision.sh)
	+#GDB_PATCHNAME := naclgdb-\$(GDB_VERSION)-r\$(shell ./glibc_revision.sh)
	+GLIBC_PATCHNAME := naclglibc-\$(GLIBC_VERSION)-r\$(shell ./glibc_revision.sh)
	+NEWLIB_PATCHNAME := naclnewlib-\$(NEWLIB_VERSION)-r\$(shell ./glibc_revision.sh)
	END
	fi
      fi
      patch -p0 <<-END
	--- native_client/buildbot/buildbot_lib.py
	+++ native_client/buildbot/buildbot_lib.py
	@@ -75 +75 @@
	-  context['max_jobs'] = 8
	+  context['max_jobs'] = 1
	--- native_client/buildbot/buildbot_lucid64-glibc-makefile.sh
	+++ native_client/buildbot/buildbot_lucid64-glibc-makefile.sh
	@@ -27 +27 @@
	-rm -rf scons-out tools/SRC/* tools/BUILD/* tools/out tools/toolchain \\
	+rm -rf scons-out tools/BUILD/* tools/out tools/toolchain \\
	@@ -35 +35 @@
	-  make -j8 buildbot-build-with-glibc
	+  make buildbot-build-with-glibc
	@@ -107 +107 @@
	-  rev="\$(tools/glibc_revision.sh)"
	+  rev="$rev"
	--- native_client/buildbot/buildbot_mac-glibc-makefile.sh
	+++ native_client/buildbot/buildbot_mac-glibc-makefile.sh
	@@ -28 +28 @@
	-rm -rf scons-out tools/SRC/* tools/BUILD/* tools/out/* tools/toolchain \\
	+rm -rf scons-out tools/BUILD/* tools/out/* tools/toolchain \\
	@@ -52 +52 @@
	-  make -j8 buildbot-build-with-glibc
	+  make buildbot-build-with-glibc
	--- native_client/buildbot/buildbot_toolchain.sh
	+++ native_client/buildbot/buildbot_toolchain.sh
	@@ -40 +40 @@
	-make -j8 clean buildbot-build-with-newlib
	+make clean buildbot-build-with-newlib
	--- native_client/buildbot/buildbot_windows-glibc-makefile.sh
	+++ native_client/buildbot/buildbot_windows-glibc-makefile.sh
	@@ -40 +40 @@
	-rm -rf scons-out tools/SRC/* tools/BUILD/* tools/out tools/toolchain \\
	+rm -rf scons-out tools/BUILD/* tools/out tools/toolchain \\
	@@ -65 +65 @@
	-  make -j8 buildbot-build-with-glibc
	+  make buildbot-build-with-glibc
	--- native_client/buildbot/gsutil.sh
	+++ native_client/buildbot/gsutil.sh
	@@ -29 +29 @@
	-  gsutil="\${SCRIPT_DIR_ABS}/../../../../../scripts/slave/gsutil.bat"
	+  gsutil="\${SCRIPT_DIR_ABS}/../../../../../../../../../scripts/slave/gsutil.bat"
	END
      mv native_client/buildbot/buildbot_windows-glibc-makefile.bat \
	native_client/buildbot/buildbot_windows-glibc-makefile.bat.orig
      sed -e s'/  ..\\..\\..\\..\\scripts\\slave\\gsutil/  ..\\..\\..\\..\\..\\..\\..\\..\\scripts\\slave\\gsutil/' \
        < native_client/buildbot/buildbot_windows-glibc-makefile.bat.orig \
        > native_client/buildbot/buildbot_windows-glibc-makefile.bat
      rm native_client/buildbot/buildbot_windows-glibc-makefile.bat.orig
      if [[ "$name" == ppapi14 ]]; then
	patch -p0 <<-END
	--- native_client/buildbot/buildbot_toolchain.sh
	+++ native_client/buildbot/buildbot_toolchain.sh
	@@ -36 +36 @@
	-rm -rf ../scons-out sdk-out sdk ../toolchain SRC/* BUILD/*
	+rm -rf ../scons-out sdk-out sdk ../toolchain BUILD/*
	@@ -61 +61 @@
	-      \${GS_BASE}/latest/naclsdk_\${PLATFORM}_x86.tgz
	+      \${GS_BASE}/latest@"$name"/naclsdk_\${PLATFORM}_x86.tgz
	END
      elif [[ "$name" == ppapi1[567] ]]; then
	patch -p0 <<-END
	--- native_client/buildbot/buildbot_toolchain.sh
	+++ native_client/buildbot/buildbot_toolchain.sh
	@@ -36 +36 @@
	-rm -rf ../scons-out sdk-out sdk ../toolchain SRC/* BUILD/*
	+rm -rf ../scons-out sdk-out sdk ../toolchain BUILD/*
	@@ -69 +69 @@
	-    for destrevision in \${BUILDBOT_GOT_REVISION} latest ; do
	+    for destrevision in \${BUILDBOT_GOT_REVISION} latest@"$name" ; do
	@@ -73 +73 @@
	-          \${GS_BASE}/\${destrevision}/naclsdk_\${PLATFORM}_x86.\${suffix}
	+          \${GS_BASE}/"\${destrevision}"/naclsdk_\${PLATFORM}_x86.\${suffix}
	END
      else
	patch -p0 <<-END
	--- native_client/buildbot/buildbot_toolchain.sh
	+++ native_client/buildbot/buildbot_toolchain.sh
	@@ -36 +36 @@
	-rm -rf ../scons-out sdk-out sdk ../toolchain/*_newlib SRC/* BUILD/*
	+rm -rf ../scons-out sdk-out sdk ../toolchain/*_newlib BUILD/*
	@@ -69 +69 @@
	-    for destrevision in \${BUILDBOT_GOT_REVISION} latest ; do
	+    for destrevision in \${BUILDBOT_GOT_REVISION} latest@"$name" ; do
	@@ -73 +73 @@
	-          \${GS_BASE}/\${destrevision}/naclsdk_\${PLATFORM}_x86.\${suffix}
	+          \${GS_BASE}/"\${destrevision}"/naclsdk_\${PLATFORM}_x86.\${suffix}
	END
      fi
      cp -vf ../../glibc-tests/xfail_list.txt native_client/tools/glibc-tests/xfail_list.txt
      # Patch sources and build the toolchains.
      if [[ "$name" != "ppapi14" || "$3" != glibc ]]; then
	make -C native_client/tools clean
	for i in binutils gcc gdb glibc linux-headers-for-nacl newlib ; do (
	  if [[ "$name" != "ppapi14" || "$i" != glibc ]]; then
	    rm -rf native_client/tools/SRC/"$i"
	    git clone ../"$i" native_client/tools/SRC/"$i"
	    cd native_client/tools/SRC/"$i"
	    . ../../REVISIONS
	    declare -r varname="NACL_$(echo "$i" | LC_ALL=C tr a-z A-Z)_COMMIT"
	    if [[ "$varname" == "NACL_LINUX-HEADERS-FOR-NACL_COMMIT" ]]; then
	      . ../../../../../../REVISIONS
	      git checkout "$LINUX_HEADERS_FOR_NACL_COMMIT"
	    else
	      git checkout "${!varname}"
	    fi
	    cd ../../../../..
	    ( while read n id comment && [[ "$n" != "$name" ]]; do
	        : # Nothing
	      done
	      cd "$name/native_client/tools/SRC/$i"
	      while read name id comment ; do
	        if [[ "$i" == "$name" ]]; then
	          git diff "$id"{^..,} | patch -p1 || touch "$$.error"
	        fi
	      done
	    ) < "$1"
	  fi
	) done
	if [[ "$name" != ppapi1* ]]; then
	  patch -p0 <<-END
	--- native_client/tools/SRC/gdb/gdb/doc/Makefile.in
	+++ native_client/tools/SRC/gdb/gdb/doc/Makefile.in
	@@ -306 +306 @@
	-	echo "@set GDBVN \`sed q \$(srcdir)/../version.in\`" > ./GDBvn.new
	+	echo "@set GDBVN \`sed q \$(srcdir)/../version.inT\`" > ./GDBvn.new
	END
	fi
	declare -r url_prefix=http://commondatastorage.googleapis.com/nativeclient-archive2
	if [[ "$3" == "glibc" ]]; then
	  declare -r url=$url_prefix/x86_toolchain/r"$rev"/toolchain_"$2"_x86.tar.gz
	else
	  declare -r url=$url_prefix/toolchain/"$rev"/naclsdk_"$2"_x86.tgz
	fi
	# If toolchain is already available then another try will not change anything
	curl --fail --location --url "$url" -o /dev/null || (
	  cd native_client
	  export BUILD_COMPATIBLE_TOOLCHAINS=no
	  export BUILDBOT_GOT_REVISION="$rev"
	  if [[ "$2" == "win" ]]; then (
	    # Use extended globbing (cygwin should always have it).
	    shopt -s extglob
	    # Filter out cygwin python (everything under /usr or /bin, or *cygwin*).
	    export PATH=${PATH/#\/bin*([^:])/}
	    export PATH=${PATH//:\/bin*([^:])/}
	    export PATH=${PATH/#\/usr*([^:])/}
	    export PATH=${PATH//:\/usr*([^:])/}
	    export PATH=${PATH/#*([^:])cygwin*([^:])/}
	    export PATH=${PATH//:*([^:])cygwin*([^:])/}
	    python_slave buildbot/buildbot_selector.py
	  ) else
	    python buildbot/buildbot_selector.py
	  fi
	)
      fi
    ) ;;
  esac || touch $$.error &
done < "$1"

wait

# If we were unable to checkout some sources then it's time to stop.
if [[ -e "$$.error" ]]; then
  rm -f "$$.error"
  # Errors are reported by git above already
  exit 2
fi

../glibc_revision.sh > "$1.lastver"

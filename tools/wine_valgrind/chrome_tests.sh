#!/bin/sh
# chromium-runtests.sh [testsuite]
# Script to run a respectable subset of Chromium's test suite
# (excepting parts that run the browser itself, and excepting layout tests).
# Run from parent of src directory.
# By default, runs all test suites.  If you specify one testsuite
# (e.g. base_unittests), it only runs that one.
#
# Chromium's test suite uses gtest, so each executable obeys the options
# documented in the wiki at http://code.google.com/p/googletest
# In particular, you can run a single test with --gtest_filter=Foo.Bar,
# and get a full list of tests in each exe with --gtest_list_tests.
#
# Before running the tests, regardless of operating system:
# 1) Make sure your system has at least one printer installed,
# or printing_unittests and unit_tests' PrintJobTest.SimplePrint
# will fail.  A fake printer is fine, nothing will be printed.
# 2) Install the test cert as described at
# http://bugs.winehq.org/show_bug.cgi?id=20370
# or net_unittests' HTTPSRequestTest.*, SSLClientSocketTest.*
# and others may fail.
#
# Chrome doesn't start without the --no-sandbox
# option in wine, so skip test suites that invoke it directly until I
# figure out how to jam that in there.

# The bot that runs this script seemed to ignore stderr, so redirect stderr to stdout by default
2>&1

usage() {
  cat <<_EOF_
Usage: sh chromium-runtests.sh [--options] [suite ...]
Runs chromium tests on Windows or Wine.
Options:
  --individual        - run tests individually
  --groups            - run tests grouped by their major gtest name
  --gtest_filter X    - only run the tests matching X
  --target X          - test with Debug or Release binaries, default to Debug
  --just-crashes      - run only tests epected to crash
  --just-fails        - run only tests epected to fail
  --just-flaky        - run only tests epected to fail sometimes
  --just-hangs        - run only tests epected to hang
  --list-failures     - show list of expected failures
  --logfiles          - log to one file per test, in logs subdir, rather than stdout
  --loops N           - run tests N times
  -n                  - dry run, only show what will be done
  --suppression_dir   - directory containing the suppression files
  --timeout N         - let each executable run for N seconds (default varies)
  --used-suppressions - extract histogram of used valgrind suppressions from current contents of logs directory
  --valgrind          - run the tests under valgrind
  --vnc N             - run the tests inside a vnc server running on display N
  --winedebug chan    - e.g. --windebug +relay,+seh
Currently supported suites:
app_unittests base_unittests courgette_unittests googleurl_unittests
ipc_tests media_unittests net_unittests printing_unittests sbox_unittests
sbox_validation_tests setup_unittests tcmalloc_unittests unit_tests
Default is to run all suites.  It takes about five minutes to run them all
together, 22 minutes to run them all individually.
_EOF_
 exit 1
}

# Tests, grouped by how long they take to run
# Skip ones that require chrome itself for the moment
SUITES_1="googleurl_unittests printing_unittests sbox_validation_tests setup_unittests"
#SUITES_10="app_unittests courgette_unittests ipc_tests reliability_tests sbox_integration_tests sbox_unittests tab_switching_test tcmalloc_unittests url_fetch_test"
SUITES_10="app_unittests courgette_unittests ipc_tests sbox_unittests tcmalloc_unittests"
#SUITES_100="automated_ui_tests installer_util_unittests media_unittests nacl_ui_tests net_perftests net_unittests plugin_tests sync_unit_tests"
SUITES_100="media_unittests net_unittests"
#SUITES_1000="base_unittests interactive_ui_tests memory_test page_cycler_tests perf_tests test_shell_tests unit_tests"
SUITES_1000="base_unittests unit_tests"
#SUITES_10000="ui_tests startup_tests"

THE_VALGRIND_CMD="/usr/local/valgrind-10880/bin/valgrind \
--gen-suppressions=all \
--leak-check=full \
--num-callers=25 \
--show-possible=no \
--smc-check=all \
--trace-children=yes \
--track-origins=yes \
-v \
--workaround-gcc296-bugs=yes \
"

LANG=C

PATTERN="are definitely|uninitialised|Unhandled exception|Invalid read|Invalid write|Invalid free|Source and desti|Mismatched free|unaddressable byte|vex x86|impossible|Assertion |INTERNAL ERROR|Terminated|Test failed|Alarm clock|Command exited with non-zero status"

reduce_verbosity() {
  # Filter out valgrind's extra -v output except for the 'used_suppression' lines
  # Also remove extra carriage returns
  awk '!/^--/ || /^--.*used_suppression:/' | tr -d '\015'
}

# Filter out known failures
# Avoid tests that hung, failed, or crashed on windows in Dan's reference run,
# or which fail in a way we don't care about on Wine,
# or which hang or crash on wine in a way that keeps other tests from running.
# Also lists url of bug report, if any.
# Format with
#  sh chromium-runtests.sh --list-failures | sort |  awk '{printf("%-21s %-20s %-52s %s\n", $1, $2, $3, $4);}'

list_known_failures() {
cat <<_EOF_
app_unittests         crash-valgrind       IconUtilTest.TestCreateSkBitmapFromHICON             http://bugs.winehq.org/show_bug.cgi?id=20634, not a bug, need to figure out how to handle DIB faults
base_unittests        hang                 EtwTraceControllerTest.EnableDisable                 http://bugs.winehq.org/show_bug.cgi?id=20946, advapi32.ControlTrace() not yet implemented
base_unittests        crash                EtwTraceConsumer*Test.*                              http://bugs.winehq.org/show_bug.cgi?id=20946, advapi32.OpenTrace() unimplemented
base_unittests        crash                EtwTraceProvider*Test.*                              http://bugs.winehq.org/show_bug.cgi?id=20946, advapi32.RegisterTraceGuids() unimplemented
base_unittests        dontcare             BaseWinUtilTest.FormatMessageW
base_unittests        dontcare             FileUtilTest.CountFilesCreatedAfter
base_unittests        dontcare             FileUtilTest.GetFileCreationLocalTime
base_unittests        dontcare             PEImageTest.EnumeratesPE                             Alexandre triaged
base_unittests        dontcare-winfail     TimeTicks.HighResNow                                 fails if run individually on windows
base_unittests        dontcare             WMIUtilTest.*
base_unittests        fail                 HMACTest.HMACObjectReuse                             http://bugs.winehq.org/show_bug.cgi?id=20340
base_unittests        fail                 HMACTest.HmacSafeBrowsingResponseTest                http://bugs.winehq.org/show_bug.cgi?id=20340
base_unittests        fail                 HMACTest.RFC2202TestCases                            http://bugs.winehq.org/show_bug.cgi?id=20340
base_unittests        fail_wine_vmware     RSAPrivateKeyUnitTest.ShortIntegers
base_unittests        flaky-dontcare       StatsTableTest.MultipleProcesses                     http://bugs.winehq.org/show_bug.cgi?id=20606
base_unittests        hang-dontcare        DirectoryWatcherTest.*
base_unittests        hang-valgrind        JSONReaderTest.Reading                               # not really a hang, takes 400 seconds
base_unittests        hang-valgrind        RSAPrivateKeyUnitTest.InitRandomTest                 # not really a hang, takes 300 seconds
base_unittests        flaky-valgrind       TimeTicks.Deltas                                     # fails half the time under valgrind, timing issue?
base_unittests        hang-valgrind        TimerTest.RepeatingTimer*
base_unittests        hang-valgrind        TimeTicks.WinRollover                                # not really a hang, takes 1000 seconds
base_unittests        fail-valgrind        ConditionVariableTest.LargeFastTaskTest              # fails under wine + valgrind TODO(thestig): investigate
base_unittests        fail-valgrind        ProcessUtilTest.CalcFreeMemory                       # fails under wine + valgrind TODO(thestig): investigate
base_unittests        fail-valgrind        ProcessUtilTest.KillSlowChild                        # fails under wine + valgrind TODO(thestig): investigate
base_unittests        fail-valgrind        ProcessUtilTest.SpawnChild                           # fails under wine + valgrind TODO(thestig): investigate
base_unittests        flaky-valgrind       StatsTableTest.StatsCounterTimer                     # flaky, timing issues? TODO(thestig): investigate
base_unittests        fail-valgrind        StatsTableTest.StatsRate                             # fails under wine + valgrind TODO(thestig): investigate
base_unittests        fail-valgrind        StatsTableTest.StatsScope                            # fails under wine + valgrind TODO(thestig): investigate
ipc_tests             flaky                IPCChannelTest.ChannelTest                           http://bugs.winehq.org/show_bug.cgi?id=20628
ipc_tests             flaky                IPCChannelTest.SendMessageInChannelConnected         http://bugs.winehq.org/show_bug.cgi?id=20628
ipc_tests             hang                 IPCSyncChannelTest.*                                 http://bugs.winehq.org/show_bug.cgi?id=20390
media_unittests       crash                FFmpegGlueTest.OpenClose
media_unittests       crash                FFmpegGlueTest.Read
media_unittests       crash                FFmpegGlueTest.Seek
media_unittests       crash                FFmpegGlueTest.Write
media_unittests       fail_wine_vmware     WinAudioTest.PCMWaveStreamTripleBuffer
media_unittests       hang-valgrind        WinAudioTest.PCMWaveSlowSource
net_unittests         fail                 SSLClientSocketTest.Read_Interrupted                 http://bugs.winehq.org/show_bug.cgi?id=20748
net_unittests         fail                 HTTPSRequestTest.HTTPSExpiredTest                    # https/ssl failing on the bot, bad Wine? TODO(thestig): investigate
net_unittests         fail                 HTTPSRequestTest.HTTPSGetTest                        # https/ssl failing on the bot, bad Wine? TODO(thestig): investigate
net_unittests         fail                 HTTPSRequestTest.HTTPSMismatchedTest                 # https/ssl failing on the bot, bad Wine? TODO(thestig): investigate
net_unittests         fail                 SSLClientSocketTest.Connect                          # https/ssl failing on the bot, bad Wine? TODO(thestig): investigate
net_unittests         fail                 SSLClientSocketTest.Read                             # https/ssl failing on the bot, bad Wine? TODO(thestig): investigate
net_unittests         fail                 SSLClientSocketTest.Read_FullDuplex                  # https/ssl failing on the bot, bad Wine? TODO(thestig): investigate
net_unittests         fail                 SSLClientSocketTest.Read_SmallChunks                 # https/ssl failing on the bot, bad Wine? TODO(thestig): investigate
net_unittests         fail                 URLRequestTestHTTP.HTTPSToHTTPRedirectNoRefererTest  # https/ssl failing on the bot, bad Wine? TODO(thestig): investigate
sbox_unittests        fail                 JobTest.ProcessInJob
sbox_unittests        fail                 JobTest.TestCreation
sbox_unittests        fail                 JobTest.TestDetach
sbox_unittests        fail                 JobTest.TestExceptions
sbox_unittests        fail                 RestrictedTokenTest.AddAllSidToRestrictingSids
sbox_unittests        fail                 RestrictedTokenTest.AddMultipleRestrictingSids
sbox_unittests        fail                 RestrictedTokenTest.AddRestrictingSid
sbox_unittests        fail                 RestrictedTokenTest.AddRestrictingSidCurrentUser
sbox_unittests        fail                 RestrictedTokenTest.AddRestrictingSidLogonSession
sbox_unittests        fail                 RestrictedTokenTest.DefaultDacl
sbox_unittests        fail                 RestrictedTokenTest.DeleteAllPrivileges
sbox_unittests        fail                 RestrictedTokenTest.DeleteAllPrivilegesException
sbox_unittests        fail                 RestrictedTokenTest.DeletePrivilege
sbox_unittests        fail                 RestrictedTokenTest.DenyOwnerSid
sbox_unittests        fail                 RestrictedTokenTest.DenySid
sbox_unittests        fail                 RestrictedTokenTest.DenySids
sbox_unittests        fail                 RestrictedTokenTest.DenySidsException
sbox_unittests        fail                 RestrictedTokenTest.ResultToken
sbox_unittests        fail                 ServiceResolverTest.PatchesServices
sbox_unittests        flaky                IPCTest.ClientFastServer
sbox_validation_tests fail                 ValidationSuite.*
unit_tests            crash                BlacklistManagerTest.*                               http://crbug.com/27726
unit_tests            crash                SafeBrowsingProtocolParsingTest.TestGetHashWithMac   http://bugs.winehq.org/show_bug.cgi?id=20340
unit_tests            crash-valgrind       DnsMasterTest.MassiveConcurrentLookupTest
unit_tests            crash-valgrind       NullModelTableViewTest.*                             http://bugs.winehq.org/show_bug.cgi?id=20553
unit_tests            crash-valgrind       RenderViewTest.OnPrintPageAsBitmap                   http://bugs.winehq.org/show_bug.cgi?id=20657 (for wine oom)
unit_tests            crash-valgrind       TableViewTest.*                                      http://bugs.winehq.org/show_bug.cgi?id=20553
unit_tests            dontcare-hangwin     UtilityProcessHostTest.ExtensionUnpacker
unit_tests            dontcare             SpellCheckTest.SpellCheckText
unit_tests            fail                 EncryptorTest.EncryptionDecryption                   http://bugs.winehq.org/show_bug.cgi?id=20495
unit_tests            fail                 EncryptorTest.String16EncryptionDecryption           http://bugs.winehq.org/show_bug.cgi?id=20495
unit_tests            hang-valgrind        ExtensionAPIClientTest.*                             Not really a hang, just takes 30 minutes
unit_tests            fail                 ImporterTest.IEImporter                              http://bugs.winehq.org/show_bug.cgi?id=20625
unit_tests            fail                 RenderViewTest.InsertCharacters                      http://bugs.winehq.org/show_bug.cgi?id=20624
unit_tests            fail                 SafeBrowsingProtocolParsingTest.TestVerifyChunkMac   http://bugs.winehq.org/show_bug.cgi?id=20340
unit_tests            fail                 SafeBrowsingProtocolParsingTest.TestVerifyUpdateMac  http://bugs.winehq.org/show_bug.cgi?id=20340
unit_tests            fail_wine_vmware     RenderProcessTest.TestTransportDIBAllocation
_EOF_
}

# Times are in seconds, and are twice as high as slowest observed runtime so far in valgrind,
# rounded to the nearest power of two multiple of 100 seconds.
# TODO: make the returned value lower if --valgrind is not given
get_expected_runtime() {
  case "$timeout_manual" in
  [0-9]*)                echo $timeout_manual; return;;
  esac

  case $1 in
  app_unittests)         echo 200;;
  base_unittests)        echo 1000;;
  courgette_unittests)   echo 1000;;
  googleurl_unittests)   echo 200;;
  ipc_tests)             echo 400;;
  media_unittests)       echo 400;;
  net_unittests)         echo 2000;;
  printing_unittests)    echo 100;;
  sbox_unittests)        echo 100;;
  sbox_validation_tests) echo 100;;
  setup_unittests)       echo 100;;
  tcmalloc_unittests)    echo 1000;;
  unit_tests)            echo 4000;;
  *)                     echo "unknown test $1" >&2 ; exec false;;
  esac
}

# Run $2... but kill it if it takes longer than $1 seconds
alarm() { time perl -e 'alarm shift; exec @ARGV' "$@"; }

init_runtime() {
  CHROME_ALLOCATOR=winheap
  export CHROME_ALLOCATOR

  if test "$WINDIR" = ""
  then
    WINE=${WINE:-/usr/local/wine/bin/wine}
    export WINE
    WINESERVER=${WINESERVER:-/usr/local/wine/bin/wineserver}
    WINEPREFIX=${WINEPREFIX:-$HOME/.wine-chromium-tests}
    export WINEPREFIX
    WINE_HEAP_REDZONE=16
    export WINE_HEAP_REDZONE

    if netstat -tlnp | grep :1337
    then
      echo Please kill the server listening on port 1337, or reboot.  The net tests need this port.
      exit 1
    fi
    if test ! -f /usr/share/ca-certificates/root_ca_cert.crt
    then
      echo "You need to do"
      echo   "sudo cp src/net/data/ssl/certificates/root_ca_cert.crt /usr/share/ca-certificates/"
      echo   "sudo vi /etc/ca-certificates.conf    (and add the line root_ca_cert.crt)"
      echo   "sudo update-ca-certificates"
      echo "else ssl tests will fail."
      echo "(Alternately, modify this script to run Juan's importer, http://bugs.winehq.org/show_bug.cgi?id=20370#c4 )"
      exit 1
    fi

    if test -n "$VNC"
    then
      export DISPLAY=":$VNC"
      vncserver -kill "$DISPLAY" || true
      # VNC servers don't clean these up if they get a SIGKILL, and would then
      # refuse to start because these files are there.
      rm -f "/tmp/.X${VNC}-lock" "/tmp/.X11-unix/X${VNC}"
      vncserver "$DISPLAY" -ac -depth 24 -geometry 1024x768
    fi
    $dry_run rm -rf $WINEPREFIX
    $dry_run test -f winetricks || wget http://kegel.com/wine/winetricks
    $dry_run sh winetricks nocrashdialog corefonts gecko > /dev/null
    $dry_run sleep 1
    $dry_run $WINE winemine &
  fi
}

shutdown_runtime() {
  if test "$WINDIR" = ""
  then
    $dry_run $WINESERVER -k
    if test -n "$VNC"
    then
      vncserver -kill "$DISPLAY"
    fi
  fi
}

# Looks up tests from our list of known bad tests.  If $2 is not '.', picks tests expected to fail in a particular way.
get_test_filter()
{
  mysuite=$1
  myfilter=$2
  list_known_failures | tee tmp.1 |
   awk '$1 == "'$mysuite'" && /'$myfilter'/ {print $3}' |tee tmp.2 |
   tr '\012' : |tee tmp.3 |
   sed 's/:$/\n/'
}

# Output the logical and of the two gtest filters $1 and $2.
# Handle the case where $1 is empty.
and_gtest_filters()
{
  # FIXME: handle more complex cases
  case "$1" in
  "") ;;
  *) echo -n "$1": ;;
  esac
  echo $2
}

# Expands a gtest filter spec to a plain old list of tests separated by whitespace
expand_test_list()
{
  mysuite=$1    # e.g. base_unittests
  myfilter=$2   # existing gtest_filter specification with wildcard
  # List just the tests matching $myfilter, separated by colons
  $WINE ./$mysuite.exe --gtest_filter=$myfilter --gtest_list_tests |
   tr -d '\015' |
   grep -v FLAKY |
   perl -e 'while (<STDIN>) { chomp; if (/^[A-Z]/) { $testname=$_; } elsif (/./) { s/\s*//; print "$testname$_\n"} }'
}

# Parse arguments

announce=true
do_individual=no
dry_run=
extra_gtest_filter=
fail_filter="."
loops=1
logfiles=
SUITES=
suppression_dirs=
TARGET=Debug
timeout_manual=
VALGRIND_CMD=
VNC=
want_fails=no
winedebug=

while test "$1" != ""
do
  case $1 in
  --individual) do_individual=yes;;
  --groups) do_individual=groups;;
  --gtest_filter) extra_gtest_filter=$2; shift;;
  --just-crashes) fail_filter="crash"; want_fails=yes;;
  --just-fails) fail_filter="fail"; want_fails=yes;;
  --just-flaky) fail_filter="flaky"; want_fails=yes;;
  --just-hangs) fail_filter="hang"; want_fails=yes;;
  --list-failures) list_known_failures; exit 0;;
  --list-failures-html) list_known_failures | sed 's,http://\(.*\),<a href="http://\1">\1</a>,;s/$/<br>/' ; exit 0;;
  --loops) loops=$2; shift;;
  -n) dry_run=true; announce=echo ;;
  --suppression_dir) suppression_dirs="$suppression_dirs $2"; shift;;
  --target) TARGET=$2; shift;;
  --timeout) timeout_manual=$2; shift;;
  --used-suppressions) cd logs; grep used_suppression *.log | sed 's/-1.*--[0-9]*-- used_suppression//'; exit 0;;
  --valgrind) VALGRIND_CMD="$THE_VALGRIND_CMD";;
  --vnc) VNC=$2; shift;;
  --winedebug) winedebug=$2; shift;;
  --logfiles) logfiles=yes;;
  -*) usage; exit 1;;
  *) SUITES="$SUITES $1" ;;
  esac
  shift
done

if test "$SUITES" = ""
then
   SUITES="$SUITES_1 $SUITES_10 $SUITES_100 $SUITES_1000"
fi

if test "$VALGRIND_CMD" != ""
then
  if test "$suppression_dirs" = ""
  then
    # Default value for winezeug.
    suppression_dirs="../../../ ../../../../../valgrind"
    # Also try the script dir.
    suppression_dirs="$suppression_dirs $(dirname $0)"
  fi
  # Check suppression_dirs for suppression files to create suppression_options
  suppression_options=
  for dir in $suppression_dirs
  do
    for f in valgrind-suppressions chromium-valgrind-suppressions
    do
      if test -f "$dir/$f"
      then
        dir="`cd $dir; pwd`"
        suppression_options="$suppression_options --suppressions=$dir/$f"
      fi
    done
  done
  VALGRIND_CMD="$VALGRIND_CMD $suppression_options"
fi

set -e

trap shutdown_runtime 0
init_runtime
export WINEDEBUG=$winedebug

set -x

mkdir -p logs
cd "src/chrome/$TARGET"

i=1
while test $i -le $loops
do
  for suite in $SUITES
  do
    expected_to_fail="`get_test_filter $suite $fail_filter`"
    case $want_fails in
    no)  filterspec=`and_gtest_filters "${extra_gtest_filter}" -${expected_to_fail}` ;;
    yes) filterspec=`and_gtest_filters "${extra_gtest_filter}"  ${expected_to_fail}` ;;
    esac

    case $do_individual in
    no)
      $announce $VALGRIND_CMD $WINE ./$suite.exe --gtest_filter=$filterspec
      LOG=../../../logs/$suite-$i.log
      $dry_run alarm `get_expected_runtime $suite` \
                $VALGRIND_CMD $WINE ./$suite.exe --gtest_filter=$filterspec 2>&1 | eval reduce_verbosity | tee $LOG || errors=yes true
      egrep -q "$PATTERN" $LOG && errors=yes
      test "$logfiles" = yes || rm $LOG
      ;;
    yes)
      for test in `expand_test_list $suite $filterspec`
      do
        $announce $VALGRIND_CMD $WINE ./$suite.exe --gtest_filter="$test"
        LOG=../../../logs/$suite-$test-$i.log
        $dry_run alarm `get_expected_runtime $suite` \
                  $VALGRIND_CMD $WINE ./$suite.exe --gtest_filter="$test" 2>&1 | eval reduce_verbosity | tee $LOG || errors=yes true
        egrep -q "$PATTERN" $LOG && errors=yes
      test "$logfiles" = yes || rm $LOG
      done
      ;;
    groups)
      for test in `expand_test_list $suite $filterspec | sed 's/\..*//' | sort -u`
      do
        $announce $VALGRIND_CMD $WINE ./$suite.exe --gtest_filter="$test.*-${expected_to_fail}"
        LOG=../../../logs/$suite-$test-$i.log
        $dry_run alarm `get_expected_runtime $suite` \
                  $VALGRIND_CMD $WINE ./$suite.exe --gtest_filter="$test.*-${expected_to_fail}" 2>&1 | eval reduce_verbosity | tee $LOG || errors=yes true
        egrep -q "$PATTERN" tmp.log && errors=yes
        test "$logfiles" = yes || rm $LOG
      done
      ;;
    esac
  done
  i=`expr $i + 1`
done

case "$errors" in
yes) echo "Errors detected, condition red.  Battle stations!" ; exit 1;;
*) echo "No errors detected." ;;
esac


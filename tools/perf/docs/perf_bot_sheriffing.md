# Perf Bot Sheriffing

The perf bot sheriff is responsible for keeping the bots on the chromium.perf
waterfall up and running, and triaging performance test failures and flakes.

## Key Responsibilities

  * [Keeping the chromium.perf waterfall green](#chromiumperf)
     * [Handling Test Failures](#testfailures)
     * [Handling Device and Bot Failures](#botfailures)
     * [Follow up on failures](#followup)
  * [Triaging Data Stoppage Alerts](#datastoppage)

###<a name="chromiumperf"></a> Keeping the chromium.perf waterfall green

The primary responsibility of the perfbot health sheriff is to keep the
chromium.perf waterfall green.

####<a name="waterfallstate"></a> Understanding the Waterfall State

Everyone can view the chromium.perf waterfall at
https://build.chromium.org/p/chromium.perf/, but for Googlers it is recommended
that you use the url **[https://uberchromegw.corp.google.com/i/chromium.perf/]
(https://uberchromegw.corp.google.com/i/chromium.perf/)** instead. The reason
for this is that in order to make the performance tests as realistic as
possible, the chromium.perf waterfall runs release official builds of Chrome.
But the logs from release official builds may leak info from our partners that
we do not have permission to share outside of Google. So the logs are available
to Googlers only. To avoid manually rewriting the URL when switching between
the upstream and downstream views of the waterfall and bots, you can install the
[Chromium Waterfall View Switcher extension](https://chrome.google.com/webstore/a/google.com/detail/chromium-waterfall-view-s/hnnplblfkmfaadpjdpkepbkdjhjpjbdp),
which adds a switching button to Chrome's URL bar.

Note that there are four different views:

   1. [Console view](https://uberchromegw.corp.google.com/i/chromium.perf/)
      makes it easier to see a summary.
   2. [Waterfall view](https://uberchromegw.corp.google.com/i/chromium.perf/waterfall)
      shows more details, including recent changes.
   3. [Firefighter](https://chromiumperfstats.appspot.com/) shows traces of
      recent builds. It takes url parameter arguments:
      * **master** can be chromium.perf, tryserver.chromium.perf
      * **builder** can be a builder or tester name, like
        "Android Nexus5 Perf (2)"
      * **start_time** is seconds since the epoch.

You can see a list of all previously filed bugs using the
**[Performance-BotHealth](https://code.google.com/p/chromium/issues/list?can=2&q=label%3APerformance-BotHealth)**
label in crbug.

Please also check the recent
**[perf-sheriffs@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/perf-sheriffs)**
postings for important announcements about bot turndowns and other known issues.

####<a name="testfailures"></a> Handling Test Failures

You want to keep the waterfall green! So any bot that is red or purple needs to
be investigated. When a test fails:

1. File a bug using
   [this template](https://code.google.com/p/chromium/issues/entry?labels=Performance-BotHealth,Pri-1,Type-Bug-Regression,OS-?&comment=Revision+range+first+seen:%0ALink+to+failing+step+log:%0A%0A%0AIf%20the%20test%20is%20disabled,%20please%20downgrade%20to%20Pri-2.&summary=%3Ctest%3E+failure+on+chromium.perf+at+%3Crevisionrange%3E).
   You'll want to be sure to include:
   * Link to buildbot status page of failing build.
   * Copy and paste of relevant failure snippet from the stdio.
   * CC the test owner from
     [go/perf-owners](https://docs.google.com/spreadsheets/d/1R_1BAOd3xeVtR0jn6wB5HHJ2K25mIbKp3iIRQKkX38o/edit#gid=0).
   * The revision range the test occurred on.
   * A list of all platforms the test fails on.

2. Disable the failing test if it is failing more than one out of five runs.
   (see below for instructions on telemetry and other types of tests). Make sure
   your disable cl includes a BUG= line with the bug from step 1 and the test
   owner is cc-ed on the bug.
3. After the disable CL lands, you can downgrade the priority to Pri-2 and
   ensure that the bug title reflects something like "Fix and re-enable
   testname".
4. Investigate the failure. Some tips for investigating:
   * [Debugging telemetry failures](https://www.chromium.org/developers/telemetry/diagnosing-test-failures)
   * If you suspect a specific CL in the range, you can revert it locally and
     run the test on the
     [perf trybots](https://www.chromium.org/developers/telemetry/performance-try-bots).
   * You can run a return code bisect to narrow down the culprit CL:
      1. Open up the graph in the [perf dashboard](https://chromeperf.appspot.com/report)
         on one of the failing platforms.
      2. Hover over a data point and click the "Bisect" button on the tooltip.
      3. Type the **Bug ID** from step 1, the **Good Revision** the last commit
         pos data was received from, the **Bad Revision** the last commit pos
         and set **Bisect mode** to `return_code`.
   * On Android and Mac, you can view platform-level screenshots of the device
     screen for failing tests, links to which are printed in the logs. Often
     this will immediately reveal failure causes that are opaque from the logs
     alone. On other platforms, Devtools will produce tab screenshots as long as
     the tab did not crash.


#####<a name="telemetryfailures"></a> Disabling Telemetry Tests

If the test is a telemetry test, its name will have a '.' in it, such as
thread_times.key_mobile_sites, or page_cycler.top_10. The part before the first
dot will be a python file in [tools/perf/benchmarks](
https://code.google.com/p/chromium/codesearch#chromium/src/tools/perf/benchmarks/).

If a telemetry test is failing and there is no clear culprit to revert
immediately, disable the test. You can do this with the `@benchmark.Disabled`
decorator. **Always add a comment next to your decorator with the bug id which
has background on why the test was disabled, and also include a BUG= line in
the CL.**

Please disable the narrowest set of bots possible; for example, if
the benchmark only fails on Windows Vista you can use `@benchmark.Disabled('vista')`.
Supported disabled arguments include:

   * `win`
   * `mac`
   * `chromeos`
   * `linux`
   * `android`
   * `vista`
   * `win7`
   * `win8`
   * `yosemite`
   * `elcapitan`
   * `all` (please use as a last resort)

If the test fails consistently in a very narrow set of circumstances, you may
consider implementing a ShouldDisable method on the benchmark instead.
[Here](https://code.google.com/p/chromium/codesearch#chromium/src/tools/perf/benchmarks/power.py&q=svelte%20file:%5Esrc/tools/perf/&sq=package:chromium&type=cs&l=72) is
and example of disabling a benchmark which OOMs on svelte.

Disabling CLs can be TBR-ed to anyone in tools/perf/OWNERS, but please do **not**
submit with NOTRY=true.

#####<a name="otherfailures"></a> Disabling Other Tests

Non-telemetry tests are configured in [chromium.perf.json](https://code.google.com/p/chromium/codesearch#chromium/src/testing/buildbot/chromium.perf.json).
You can TBR any of the per-file OWNERS, but please do **not** submit with
NOTRY=true.

####<a name="botfailures"></a> Handling Device and Bot Failures

#####<a name="purplebots"></a> Purple bots

When a bot goes purple, it's it's usually because of an infrastructure failure
outside of the tests. But you should first check the logs of a purple bot to
try to better understand the problem. Sometimes a telemetry test failure can
turn the bot purple, for example.

If the bot goes purple and you believe it's an infrastructure issue, file a bug
with
[this template](https://code.google.com/p/chromium/issues/entry?labels=Pri-1,Performance-BotHealth,Infra-Troopers,OS-?&comment=Link+to+buildbot+status+page:&summary=Purple+Bot+on+chromium.perf),
which will automatically add the bug to the trooper queue. Be sure to note
which step is failing, and paste any relevant info from the logs into the bug.

#####<a name="devicefailures"></a> Android Device failures

There are two types of device failures:

1. A device is blacklisted in the `device_status_check` step. You can look at
   the buildbot status page to see how many devices were listed as online during
   this step. You should always see 7 devices online. If you see fewer than 7
   devices online, there is a problem in the lab.
2. A device is passing `device_status_check` but still in poor health. The
   symptom of this is that all the tests are failing on it. You can see that on
   the buildbot status page by looking at the `Device Affinity`. If all tests
   with the same device affinity number are failing, it's probably a device
   failure.

For both types of failures, please file a bug with [this template](https://code.google.com/p/chromium/issues/entry?labels=Pri-1,Performance-BotHealth,Infra-Labs,OS-Android&comment=Link+to+buildbot+status+page:&summary=Device+offline+on+chromium.perf)
which will add an issue to the infra labs queue.

If you need help triaging, here are the common labels you should use:

   * **Performance-BotHealth** should go on all bugs you file about the bots;
     it's the label we use to track all the issues.
   * **Infra-Troopers** adds the bug to the trooper queue. This is for high
     priority issues, like a build breakage. Please add a comment explaining
     what you want the trooper to do.
   * **Infra-Labs** adds the bug to the labs queue. If there is a hardware
     problem, like an android device not responding or a bot that likely needs
     a restart, please use this label. Make sure you set the **OS-** label
     correctly as well, and add a comment explaining what you want the labs
     team to do.
   * **Infra** label is appropriate for bugs that are not high priority, but we
     need infra team's help to triage. For example, the buildbot status page
     UI is weird or we are getting some infra-related log spam. The infra team
     works to triage these bugs within 24 hours, so you should ping if you do
     not get a response.
   * **Cr-Tests-Telemetry** for telemetry failures.
   * **Cr-Tests-AutoBisect** for bisect and perf try job failures.

 If you still need help, ask the speed infra chat, or escalate to sullivan@.

####<a name="followup"></a> Follow up on failures

**[Pri-0 bugs](https://code.google.com/p/chromium/issues/list?can=2&q=label%3APerformance-BotHealth+label%3APri-0)**
should have an owner or contact on speed infra team and be worked on as top
priority. Pri-0 generally implies an entire waterfall is down.

**[Pri-1 bugs](https://code.google.com/p/chromium/issues/list?can=2&q=label%3APerformance-BotHealth+label%3APri-1)**
should be pinged daily, and checked to make sure someone is following up. Pri-1
bugs are for a red test (not yet disabled), purple bot, or failing device.

**[Pri-2 bugs](https://code.google.com/p/chromium/issues/list?can=2&q=label%3APerformance-BotHealth+label%3APri-2)**
are for disabled tests. These should be pinged weekly, and work towards fixing
should be ongoing when the sheriff is not working on a Pri-1 issue. Here is the
[list of Pri-2 bugs that have not been pinged in a week](https://code.google.com/p/chromium/issues/list?can=2&q=label:Performance-BotHealth%20label:Pri-2%20modified-before:today-7&sort=modified)

###<a name="datastoppage"></a> Triaging data stoppage alerts

Data stoppage alerts are listed on the
[perf dashboard alerts page](https://chromeperf.appspot.com/alerts). Whenever
the dashboard is monitoring a metric, and that metric stops sending data, an
alert is fired. Some of these alerts are expected:

   * When a telemetry benchmark is disabled, we get a data stoppage alert.
     Check the [code for the benchmark](https://code.google.com/p/chromium/codesearch#chromium/src/tools/perf/benchmarks/)
     to see if it has been disabled, and if so associate the alert with the
     bug for the disable.
   * When a bot has been turned down. These should be announced to
     perf-sheriffs@chromium.org, but if you can't find the bot on the waterfall
     and you didn't see the announcement, double check in the speed infra chat.
     Ideally these will be associated with the bug for the bot turndown, but
     it's okay to mark them invalid if you can't find the bug.

If there doesn't seem to be a valid reason for the alert, file a bug on it
using the perf dashboard, and cc [the owner](http://go/perf-owners). Then do
some diagnosis:

   * Look at the perf dashboard graph to see the last revision we got data for,
     and note that in the bug. Click on the `buildbot stdio` link in the tooltip
     to find the buildbot status page for the last good build, and increment
     the build number to get the first build with no data, and note that in the
     bug as well. Check for any changes to the test in the revision range.
   * Go to the buildbot status page of the bot which should be running the test.
     Is it running the test? If not, note that in the bug.
   * If it is running the test and the test is failing, diagnose as a test
     failure.
   * If it is running the test and the test is passing, check the `json.output`
     link on the buildbot status page for the test. This is the data the test
     sent to the perf dashboard. Are there null values? Sometimes it lists a
     reason as well. Please put your finding in the bug.

<!-- Unresolved issues:
1. Do perf sheriffs watch the bisect waterfall?
2. Do perf sheriffs watch the internal clank waterfall?
3. Do we use sheriff-o-matic?
4. Should we add some list of bugs that bot sheriffs could help fix, to improve
their workflow?
-->

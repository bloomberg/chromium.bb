Ash
---
Ash is the "Aura Shell", the window manager and system UI for Chrome OS.
Ash uses the views UI toolkit (e.g. views::View, views::Widget, etc.) backed
by the aura native widget and layer implementations.

Ash sits below chrome in the dependency graph (i.e. it cannot depend on code
in //chrome). It has a few dependencies on //content, but these are isolated
in their own module in //ash/content. This allows targets like ash_unittests
to build more quickly.

Tests
-----
Most tests should be added to the ash_unittests target. Tests that rely on
//content should be added to ash_content_unittests, but these should be rare.

Tests can bring up most of the ash UI and simulate a login session by deriving
from AshTestBase. This is often needed to test code that depends on ash::Shell
and the controllers it owns.

Test support code (TestFooDelegate, FooControllerTestApi, etc.) lives in the
same directory as the class under test (e.g. //ash/foo rather than //ash/test).
Test code uses namespace ash; there is no special "test" namespace.

Mash
----------
Ash is transitioning to run as a mojo service in its own process. This change
means that code in chrome cannot call into ash directly, but must use the mojo
interfaces in //ash/public/interfaces.

Out-of-process Ash is referred to as "mash" (mojo ash). In-process ash is
referred to as "classic ash". Ash can run in either mode depending on the
--enable-features=Mash command line flag.

In the few cases where chrome code is allowed to call into ash (e.g. code that
will only ever run in classic ash) the #include lines have "// mash-ok"
appended. This makes it easier to use grep to determine which parts of chrome
have not yet been adapted to mash.

Ash used to support a "mus" mode that ran the mojo window service from
//services/ui on a background thread in the browser process. This configuration
was deprecated in April 2018.

SingleProcessMash
-----------------

Before launching Mash we plan to launch SingleProcessMash. SingleProcessMash is
similar to "classic ash" in that ash and the browser still live in the same
process and on the same thread, but all non-ash UI code (such as browser
windows) will use the WindowService over mojo. This results in exercising much
of the same code as in mash, but everything is still in the process.

In SingleProcessMash mode there are two aura::Envs. Ash (Shell) creates one and
the browser creates one. In order to ensure the right one is used do the
following:

. When creating a Widget set the parent and/or context. If you don't need a
  specific parent (container), more often than not using a context of
  Shell::GetRootWindowForNewWindows() is what you want.
. If you are creating aura::Windows directly, use the ash::window_factory.
. If you need access to aura::Env, get it from Shell. Shell always returns the
  right one, regardless of mode.

See https://docs.google.com/document/d/11ha_KioDdXe4iZS2AML1foKnCJlNKm7Q1hFr6VW8dV4/edit for more details.

Mash Tests
-----
ash_unittests has some tests specific to Mash, but in general Ash code should
not need to do anything special for Mash. AshTestBase offers functions that
simulate a remote client (such as the browser) creating a window.

To enable browser_tests to run in Mash use "--enable-features=Mash". As
Mash is still a work in progress not all tests pass. A filter file is used on
the bots to exclude failing tests 
(testing/buildbot/filters/mash.browser_tests.filter).

To simulate what the bots run (e.g. to check if you broke an existing test that
works under mash) you can run:

`browser_tests --enable-features=Mash --test-launcher-filter-file=testing/buildbot/filters/mash.browser_tests.filter`

Any new feature you add (and its tests) should work under mash. If your test
cannot pass under mash due to some dependency being broken you may add the test
to the filter file. Make sure there is a bug for the underlying issue and cite
it in the filter file.

Prefs
-----
Ash supports both per-user prefs and device-wide prefs. These are called
"profile prefs" and "local state" to match the naming conventions in chrome. Ash
also supports "signin screen" prefs, bound to a special profile that allows
users to toggle features like spoken feedback at the login screen.

Local state prefs are loaded asynchronously during startup. User prefs are
loaded asynchronously after login, and after adding a multiprofile user. Code
that wants to observe prefs must wait until they are loaded. See
ShellObserver::OnLocalStatePrefServiceInitialized() and
SessionObserver::OnActiveUserPrefServiceChanged(). All PrefService objects exist
for the lifetime of the login session, including the signin prefs.

Pref names are in //ash/public/cpp so that code in chrome can also use the
names. Prefs are registered in the classes that use them because those classes
have the best knowledge of default values.

All PrefService instances in ash are backed by the mojo preferences service.
This means an update to a pref is asynchronous between code in ash and code in
chrome. For example, if code in chrome changes a pref value then immediately
calls a C++ function in ash, that ash function may not see the new value yet.
(This pattern only happens in the classic ash configuration; code in chrome
cannot call directly into the ash process in the mash config.)

Prefs are either "owned" by ash or by chrome browser. New prefs used by ash
should be owned by ash. See NightLightController and LogoutButtonTray for
examples of ash-owned prefs. See //services/preferences/README.md for details of
pref ownership and "foreign" prefs.

Historical notes
----------------
Ash shipped on Windows for a couple years to support Windows 8 Metro mode.
Windows support was removed in 2016.

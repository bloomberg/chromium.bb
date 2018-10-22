# GTM: Google Toolbox for Mac #
## Release History ##

*NOTE:* There haven't been release as most users of this code have just been
pulling trunk the HEAD code to get the latest updates/fixes.  This code is also
a little different in that it is more of a "collection" of things than a single
library providing one thing, so by pulling in individual source files,
developers can pick up the individual parts they need.

**Release 2.0.??**<br>
Changes since 1.6.0<br>
??-??-??

- Removed iPhone/GTMABAddressBook in favor of AddressBook/GTMABAddressBook.

- Removed Foundation/GTMHTTPServer and UnitTesting/GTMTestHTTPServer, they
  are going to go live with the fetcher used by GData (since they were done
  for that testing).

- Removed Foundation/GTMBase64 and Foundation/GTMNSData+Hex in favor of
  Foundation/GTMStringEncoding.

- Added Foundation/GTMURITemplate to support the pending standard.

- Changed the xcconfig files so that the SDK and the minimum supported OS
  version must be set in the project file.  This is the model Apple is pushing
  for and since they are removing older SDKs with each tool chain release it
  has forced GTM into this model. By default the SDK will be set to the most
  recent SDK installed.

- Initial support for using the Xcode provided XCTest for unittesting on iOS.
  Define GTM_USING_XCTEST to 1 to use this.

- Removed support for Garbage Collection, leaving just the shell for other code
  that might have depended on some of the constants/method GTM provided.

- Removed GTMNSNumber+64Bit methods as obsolete.

- Removed GTM_ENABLE_LEAKS.

- Removed GTMGarbageCollection.h.


**Release 1.6.0**<br>
Changes since 1.5.1<br>
18-August-2010

- Added GTMNSImage+SearchCache for fetching images based on a variety of
  specification methods (path, OSType, etc)

- Added GTMFadeTruncatingTextFieldCell, for eliding with a gradient

- Added GTMWindowSheetController for creating and controlling tab-modal sheets.

- Added GTMNSArray+Merge for merging one array into another with or without
  a custom merging function, returning a new array with the merged contents.

- Added GTMSignalHandler for simple signal handling (via kqueue/runloop). This
  has gotten an api tweak, so some code that started using it will need
  updating.  Initial landing had a bug where it could leak memory due to
  how CFRunLoops work, now fixed.

- Fixed up GTMIPhoneUnitTestDelegate to be pickier about which tests it runs

- Added GTMNSString+URLArguments to GTMiPhone

- Added GTMHTTPFetcher and GTMHTTPServer to GTMiPhone

- Made sure that build would work with iPhone device attached, and that all
  tests run directly on the phone.

- Added GTMValidatingContainers which are a set of mutable container classes
  that allow you to have a selector on a target that is called to verify that
  the objects being put into the container are valid. This can be controlled
  at compile time so that you don't take the performance hit in a release build.

- Added GTMPath, which represents an existing absolute path on the file system.
  It also makes it very easy to construct new paths in the file system as well
  as whole directory hierarchies.

- Added GTMNSString+Replace for a common replacement need.

- Added NSString+FindFolder for two comment helpers for building paths to common
  locations.

- Added GTMLargeTypeWindow for doing display windows similar to Address Book
  Large Type display for phone numbers.

- Removed GTMNSWorkspace+ScreenSaver as it has always been a little dodgy due
  to it's dependencies on undocumented frameworks, and the ScreenSaver
  framework doesn't play nicely in GC mode.

- Added property methods to GTMHTTPFetcher.  These are convenient alternatives
  to storing an NSDictionary in the userData.

- Renamed GTMDevLog.m to GTMDevLogUnitTestingBridge.m and added some more
  comments where it comes into play to hopefully make it more clear that it
  isn't needed in most cases.

- Fixed a potential GTMHTTPFetcher crash on failed authentication.

- Added a obj-c logging package, GTMLogger, for applications that want an
  application level logging system.  See GTMLogger.h, GTMLogger+ASL.h, and
  GTMLoggerRingBufferWriter.h for what the basic system and two optional
  additions can do.

- Added GTMNSMakeUncollectable for forcing objects to survive in a GC world.

- Added GTMCFAutorelease to make the [GTMNSMakeCollectable(cfFoo) autorelease]
  simpler and clearer, it's now just GTMCFAutorelease(cfFoo), and works in
  both GC and non-GC world.

- Added GTMIsGarbageCollectionEnabled to GTMGarbageCollection.h.  See the note
  there for it's usage.

- Disabled the unittests for things on top of NSAppleScript in a GC world since
  Apple has bugs and it can crash.  See the unittest for a note about it.

- GTMStackTrace now can figure out ObjC symbols. Downside it is now ObjC only.

- GTMFourCharCode can now be used with NSAppleEventDescriptors easily.
  typeType, typeKeyword, typeApplSignature, and typeEnumerated all get
  turned into GTMFourCharCodes.

- Fixed up crash in GTMLoggerRingBufferWriter when used with GC on.

- Significant updates to GTMNSAppleScript+Handler allowing you to
  list all handlers and properties (including inherited) and cleans up
  several errors in how scripting was being handled.

- Added GTMGetURLHandler class that gives you a very easy way of supporting
  Get URL events just by adding a key to your plists, and adding a single
  method to your class. See GTMGetURLHandler.m for more details.

- Added XcodeProject, AppleScript, and InterfaceBuilder Spotlight Plugins.
  Allows you to index .xcodeproj, .scpt, .scptd, .xib, .nib, and
  .aib files. See ReadMes beside individual projects in SpotlightPlugins.

- Added GTMExceptionalInlines for dealing with cases where you get
  warning: variable 'r' might be clobbered by 'longjmp' or 'vfork'
  when using certain Apple inlined functions in @synchronized/@try blocks.

- Updated to Xcode 3.1 so the GTM and iPhone project have the same baseline.
  The code should work in other version of xcode, but the projects and
  xcconfig files now use 3.1 features.

- Added GTMABAddressBook which is a cocoa wrapper for the 'C' AddressBook
  APIs on the iPhone.

- Added several set environment variable statements to RunIPhoneUnitTest.sh
  to encourage bugs to come out of the woodwork.

- Added GTMTestTimer.h for doing high fidelity timings.

- Added ability to control using zombies to iPhone unittest script. It can be
  controlled by the GTM_DISABLE_ZOMBIES environment variable

- Added ability to control termination to iPhone unittest script. It can be
  controlled by the GTM_DISABLE_TERMINATION environment variable

- Fixed several leaks found with leak checking enabled.

- Updated the iPhone xcconfigs to support the different OS versions.

- GTM_INLINE will make sure a function gets inlined, and provides a consistent
  way for all GTM code to do it.

- Added GTMDebugThreadValidation to allow you to enforce the fact that your
  code must run in the main thread in DEBUG builds.

- Updated some internals of the iPhone unittesting so it doesn't double print
  the test descriptions, file names, or lines numbers of a test failure line.
  Also includes the test names in the error output.

- Changed the xcconfigs so that know it's easier to set different settings at
  the different levels and not accidentally overwrite settings set at lower
  levels in the "settings collapse". Also tightened up warnings significantly.

- Changed how gtm_unitTestExposedBindingsTestValues works. If you have an
  implementation of gtm_unitTestExposedBindingsTestValues in your own code
  you will need to update to the new way of calling. See implementations in
  GTMNSObject+BindingUnitTesting.m for details.

- Added support for grabbing the build number for a particular OS in
  GTMSystemVersion and easily comparing it to known build numbers, and switched
  some types from in GTMSystemVersion from "int" to SInt32 to make 64 bit work
  better.

- Added support for SnowLeopard (10A96). We build cleanly with the 10.6 SDKs and
  all radar checks were updated accordingly. Build All script was also updated
  to build on SnowLeopard if you have the SDK available.

- Turned off building ppc64 GTM because the SnowLeopard SDK currently
  doesn't have ppc64 support, so SenTestCase isn't defined. This makes it
  impossible to build the ppc64 10.5 config on SnowLeopard. We have left the
  setting in the xcconfig for those of you who need it, but have disabled
  it in the GTM project settings.

- Turned on stack smashing protection on the debug builds for all Leopard
  and above.

- Added ability to easily do leak checking by defining the GTM_ENABLE_LEAKS
  environment variable. It isn't on by default because several of Apple's
  frameworks leak. You can work around these false positives by using the
  GTM_LEAKS_SYMBOLS_TO_IGNORE environment variable. Also if you turn on leaks
  make sure to turn off zombies by defining the GTM_DISABLE_ZOMBIES variable,
  otherwise every memory allocation you do will look like a leak.

- Added has ability to check if a script has an open handler to
  GTMNSAppleScript+Handler.

- GTMStackTrace support for building a trace from the call stack in an
  NSException (for 10.5+ and iPhone).

- GTMStackTrace works on 10.5+ (and iPhone) using NSThread to build the call
  stack.

- GTMLightweightProxy for breaking retain cycles.

- Added GTM_EXTERN that makes it easier to mix and match objc and objc++ code.

- Added GTMHotKeysTextField for display and editing of hot key settings.

- Added GTMCarbonEvent for dealing with Carbon Events and HotKeys in a ObjC
  like way.

- Backported the Atomic Barrier Swap functions for Objective C back to Tiger.

- Added a variety of new functions to GTMUnitTestingUtilities for checking
  if the screensaver is in the way, waiting on user events, and generating
  keystrokes.

- If you are using any Carbon routines that log (DebugStr, AssertMacros.h) and
  use GTMUnitTestDevLog, the log routines now go through _GTMDevLog so that
  they can be caught in GTMUnitTestDevLog and verified like any _GTMDevLog calls
  you may make. For an example of this in action see GTMCarbonEventTest.m.
  Since we have turned this on, we have turned off using _debug frameworks
  from the RunUnitTests.sh because it was reporting a pile of uninteresting
  issues that were interfering with unittests.

- Added GTMFileSystemKQueue.  It provides a simple wrapper for kqueuing
  something in the file system and tracking changes to it. Initial landing
  had a bug where it could leak memory due to how CFRunLoops work, now fixed.

- RunIPhoneUnitTest.sh now cleans up the user home directory and creates
  a documents directory within it, used when requesting a NSDocumentDirectory.

- Added GTMNSFileManager+Carbon which contains routines for path <-> Alias
  conversion and path <-> FSRef conversion.

- Added GTM_EXPORT as a standard way of exporting symbols.

- Added GTMUnitTestDevLogDebug which extends GTMUnitTestDevLog to only look
  for the messages in debug builds, to make it easier to validate messages
  that are only present in debug builds.

- Added GTM_SUPPORT_GC for controlling the inclusion of GC related code.

- If you are using GTMUnitTestDevLog, it also tries to capture logs from
  NSAssert.

- Added GTM_FOREACH_OBJECT/GTM_FOREACH_KEY/GTM_FOREACH_ENUMEREE that uses
  NSEnumerator and objectEnumerator/keyEnumerator on 10.4, but on 10.5+/iPhone
  uses FastEnumeration.

- GTMNSWorkspace+Running gives a variety of ways of determining the attributes
  of running processes.

- If the iPhone unittesting support is exiting when done, it now properly sets
  the exit code based on test success/failure.

- Added GTMNSObject+KeyValueObserving to make it easier on folks to do KVO
  "correctly". Based on some excellent code by Michael Ash.
  http://www.mikeash.com/?page=pyblog/key-value-observing-done-right.html
  This has been added for iPhone and OS X.

- Fixed up GTMSenTestCase on iPhone so that it has a description that matches
  that of OCUnit.

- Added GTMAbstractDOListener, GTMTransientRootProxy, and
  GTMTransientRootPortProxy.  These classes can be used to simplify the
  use of distributed objects.  GTMAbstractDOListener can be used to handle
  connections from any type of port.  GTMTransientRootProxy is designed for
  using named connections while GTMTransientRootPortProxy is for connections
  with supplied NSPorts.

- Finally dropped GTMHTTPFetcher and GTMProgressMonitorInputStream, GData
  versions now pretty much line up with these, so rather then both projects
  maintaining them, we've dropped them and point folks at the gdata versions
  which can be used independent of the rest of GData.

- Changed gtm_createCGPath to gtm_cgPath in GTMNSBezier+CGPath. The path
  returned is now autoreleased so you don't need to worry about releasing it.

- Made some changes to the GTMNSObject+UnitTesting APIs. Specifically renamed
  gtm_createUnitTestImage to gtm_unitTestImage. The value it returns is now
  autoreleased, so no need to release it. Also change
  gtm_createUnitTestBitmapOfSize:withData: to a C function.

- Cleaned up GTM so that it passes the Clang checker without any warnings.

- Added GTMLuminance for working with colors in HSL space easily.

- Added GTMTheme for doing product wide theme modifications.

- The Run*UnitTest.sh script now delete the current projects *.gcda files to
  avoid coverage data warning when you edit source. If you do not want this to
  occur, you can set GTM_DO_NOT_REMOVE_GCOV_DATA to a non-zero value.

- Added OBJC_DEBUG_UNLOAD=YES, and OBJC_DEBUG_NIL_SYNC=YES to our unittest shell
  scripts to try and flush out some more bugs. We have intentionally NOT turned
  on OBJC_DEBUG_FINALIZERS because it spits out a lot of unnecessary false
  positives.

- Added GTMUILocalizer.m for automatically localizing nib files with strings.

- Added better support for NSTabViews to GTMAppKit+UnitTesting. Previously we
  didn't check the tabs, or recurse into the views.

- Adds support for toolTips, accessibilityHelp and accessibilityDescription to
  GTMAppKit+UnitTesting. This will break your UI tests based on the older
  state information.

- Added support for duration to GTMLargeTypeWindow mainly to make the unittests
  run at a decent speed.

- All calls to GTMNSAppleScript+Handler execute: calls will now actually
  execute the script on the main thread.

- Added gtm_launchedApplications to GTMNSWorkspace+Running. It is significantly
  faster than calling [NSWorkspace launchedApplications]

- Moved GTMABAddressBook out of iPhone and into the AddressBook directory,
  because it now works on both the Desktop and the iPhone giving you a single
  interface to do AddressBook work on both platforms.

- Added GTMNSScanner+JSON for scanning out JSON objects and arrays. We don't
  parse JSON as there are several other frameworks out there for doing that.

- Fixed up GTMABAddressBook so that it will compile and run on Tiger as well.
  This did mean some slight functional differences in terms of the
  *WithCompositeNameWithPrefix methods as they can't do diacritic or width
  insensitive search on Tiger, but this shouldn't affect most users.

- Added GTMGoogleSearch to foundation to make doing google searches easier.

- Added GTMUIImage+Resize for iPhone to conveniently handle generating resized
  UIImages while preserving aspect ratios.

- Added support for passing in a context object to some of the
  GTMNSEnumerator+Filter routines.

- Fixed up bug in GTMFileSystemKQueue where we were passing the kqueue argument
  in incorrectly. Added appropriate tests.

- Added NSMatrix to the UIState support.

- Added NSMatrix and NSCell to GTMLocalizer support.

- Added gtm_dictionaryWithHttpArgumentsString to NSDictionary+URLArguments.

- Added GTMDebugKeyValueObserving category to NSObject. This makes debugging
  KVO a little easier in some cases. To turn it on, set the "GTMDebugKVO"
  environment variable to "1". It will output a lot of data about adding and
  removing KVO observers, and when the values are actually changed.

- Added better support for NSBox to GTMAppKit+UnitTesting. Previously we
  didn't check any box specific attributes.

- Updated how GTMNSObject+UnitTesting searches for files to include the Target
  SDK from compile time.  Also removed some of the formatting options so
  try and make it simpler to follow.

- Added GTMNSDictionary+CaseInsensitive, e.g. for use with HTTP headers.

- Added GTMNSData+Hex for conversion to and from hex strings.

- Added GTMNSNumber+64Bit for working with CGFloats, NSIntegers and
  NSUIntegers using NSNumber on all supported SDKs.

- Added GTMIBArray for building arrays in nib files.

- Added SDEFCompiler.sh for making it easier to error check SDEFs at
  compile time. See BuildScripts/SDEFCompiler.sh for details on how to set it
  up. If you work with SDEFs at all, this one is worth checking out.

- GTMNSAnimation+Duration.m adds support for checking for control and shift
  keys when trying to decide how to calculate durations for animations.

- Added Xcode configs for iPhone 2.2, 2.2.1, 3.0, 3.1, 3.1.2.

- Added configurations GTMiPhone for all the new configs, updated the build
  scripts to build all iPhone SDKs also.

- RunMacOSUnitTests supports GTM_REMOVE_TARGET_GCOV_ONLY to have only gcda
  removed from the target and not the whole project.

- RunMacOSUnitTests supports GTM_ONE_TEST_AT_A_TIME to have only one test
  run at a time to support global state (color sync profile, etc.).

- Added the GTM XcodePlugin. This plugin enhances Xcode with the following
  features:
  - Cleanup line ending white space on saves.
    See Xcode Preferences > Google panel to turn this on.
  - Create Unit Test Executable.
    Select a unittest target, and then select "Create Unit Test Executable"
    from the project menu, and it will create an executable you can debug.
  - Turn Code Coverage On
    Turns on code coverage for the current target. Nice when working with
    CoverStory.
  - Show Code Coverage/Clean Code Coverage/Clean Project Coverage and Build
    Utilities for working with CoverStory.
  - Under the help menu, quick links to the Google Style guides, Radar,
    and our favorite tech note.

  Note that you can see all the menu items that GTM Xcode Plugin has added by
  turning on the "Show Icon on Menu Items" option in the Xcode Preferences >
  Google panel.

- iPhone unittests now print "Test Case '-[TEST SELECTOR]' started." before
  each test.

- Added GTMTypeCasting.h which gives you safer objective-c casts based on
  C++ static_cast and dynamic_cast.

- Added GTMStringEncoding which is a generic base 2-128 encoder/decoder with
  support for custom character maps.

- Added support for localizing binding options in GTMUILocalizer.

- Cleaned up several leaks in tests and elsewhere.

- Added PListCompiler.sh for compiling plists.

- Added GTM_NSSTRINGIFY_MACRO for turning other macros into NSStrings.

- Removed GTMTheme because it wasn't generic enough for inclusion in GTM, and
  was never fully implemented

- Added GTM_NONNULL, NS_RETURNS_RETAINED, and CF_RETURNS_RETAINED macrs to
  support clang analysis.

- Changed GTMStackTrace to put out a cleaner trace, and to work on 64 bit.
  NOTE that if you are parsing this format, that it has changed.
  eg 32 bit
  \#0  UnitTest - Foundation               0x0001c392 -[SenTest run]
  and 64 bit
  \#0  UnitTest - Foundation               0x10010000001c3921 -[SenTest run]

- Added GTMNSAnimatablePropertyContainer methods that allow you to stop
  animations properly in 10.5.

- Added gtm_imageByRotating for rotating a UIImage. Based on code by Trevor
  Harmon:
  http://vocaro.com/trevor/blog/wp-content/uploads/2009/10/UIImage+Resize.h
  http://vocaro.com/trevor/blog/wp-content/uploads/2009/10/UIImage+Resize.m

- Added support for creating uniquely named files and directories easily with
  GTMNSFileHandle+UniqueName.


**Release 1.5.1**<br>
Changes since 1.5.0<br>
16-June-2008

- Fixed building tiger gcov with a directory path that contains a space.


**Release 1.5.0**<br>
Changes since 1.0.0<br>
13-June-2008

- Updated the project to Xcode 3.  This is the only supported Xcode version
  for the project.  The code can build against the Tiger or Leopard SDKs, and
  developers can pull individual files into a Xcode 2.x project and things
  should work just fine.

- Fixed up the prefix header of the project and prefix handing in the Unittest
  Xcode Config. (thanks schafdog)

- Fixed error in handling default compression for NSData+zlib

- Changed name on API in NSString+XML and added another api to make this a
  litte more clear. (thanks Kent)

- GTMRegex
  - Found and fixed a bug in the enumerators that was causing them to
    incorrectly walk a string when using '^' in an expression.
  - Added helpers for substring tests and unittests for the new apis.
  - Added initializer that takes an outError to allow the collection of any
    pattern parsing error message (in case the pattern came from a user and
    complete error information is needed to message the user).

- Added GTMScriptRunner for spawning scripts.

- Added GTMNSFileManager+Path for two small helpers.

- Added GTMNSWorkspace+ScreenSaver

- Added GTMNSString+Data

- added a common header (GTMDefines) for any common defines so the conditionals
  are all in one place

- Support for things compiling against the iPhone SDK
  - Everything in the GTMiPhone project works in the iPhone
  - Added iPhone xcconfig files
  - Added iPhone unittests (See below)

- More work on the UI unittests
  - support pretty much any part of a UI
  - support for CALayers
  - full support for the iPhone
    - the iPhone uses the same macro set at OCUnit, but has its own runtime
      for running tests.
  - extended capabilities of UIUnitTesting to be more flexible and give better
    error reporting for states.

- Renamed the actual framework to "GoogleToolboxForMac.framework" (it should
  have matched the project on code.google.com from the start)

- added a Debug-gcov target that will product debug bits with code coverage
  support to check unittests, etc.

- GTMDebugSelectorValidation to provide something to include in class impls
  to get validation of object/selector pair(s) being implemented so you don't
  have to wait for a runtime invocation failures.  (especially useful for
  things that take a success and failure selector so one doesn't always get
  called)

- added _GTMDevLog (really in GTMDefines) that are a set of macros that can be
  used for logging.  This allows any project to redefine them to direct logging
  into its runtime needs.

- Moved GTMGeometryUtils into Foundation from AppKit

- Removed several HI* calls from GTMGeometryUtils as Carbon UI in general is
  deprecated.

- Xcode configs
  - changed the layout to make it a little easier to tell how to use them.
  - added Leopard or later configs

- Unittest coverage greatly increased

- Added RunMacOSUnitTests shell script. We run this script for starting up our
  unittests because it turns on a variety of "enhancements" (such as zombies,
  scribbling etc) to encourage our unittests to fail for us.

  https://connect.apple.com/cgi-bin/WebObjects/MemberSite.woa/wa/getSoftware?bundleID=19915

- Remove NSColor+Theme and NSWorkspace+Theme as they are no longer needed for
  testing things for unittests, instead GTMUnitTestingUtilities.m(Lines 64-79)
  force the user settable things to ensure tests are consistent.

- Added GTMBase64.

- Added GTMHTTPFetcher and GTMProgressMonitorInputStream.

- Moved the data files for unittests into subdirectories call TestData to
  help make it a little easier to find files within the main directories.

- GTMDelegatingTableColumn get an overhaul to match the 10.5 sdk so it's closer
  to a dropin for previous sdks.

- Added a lot of functionality to NSAppleEventDescriptor and NSAppleScript
  allowing you to easily call labeled and positional handlers in an AppleScript,
  get/set properties and get NSAppleEventDescriptors for most basic datatypes.

- Added GTMFourCharCode for wrapping FourCharCodes in an ObjC object. Mainly for
  use by the NSAppleEventDescriptor code, and also useful for storing them
  in ObjC collection classes.

- Added GTMStackTrace.

- Added NSString+URLArguments and NSDictionary+URLArguments

- Added GTMHTTPServer as a simple server but mainly for use in unittesting.

- Added _GTMCompileAssert for doing compile time assertions to GTMDefines.h

- Added GTMUnitTestDevLog and GTMTestCase for logging and tracking logs while
  running unittests to verify what is being logged is what you expect. All
  unittests should now inherit from GTMTestCase instead of SenTestCase to take
  advantage of the new log tracking. See GTMUnitTestDevLog.h for details.

- Extracted GTMIPhoneUnitTestDelegate from GTMIPhoneUnitTestMain.m to its own
  file. Tests can now be run from another application.


**Release 1.0.0**<br>
14-January-2008

- Initial public release.  Includes some simple utils, xcode configs, and
  some support for doing unittests of graphical things.

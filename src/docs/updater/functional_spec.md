# Chromium Updater Functional Specification

This is the functional specification for
[Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/).
It describes the externally observable behavior of the updater, including APIs
and UI.


[TOC]

## Installation

### Metainstaller
The metainstaller (UpdaterSetup) is a small executable that contains a
compressed copy of the updater as a resource, extracts it, and triggers
installation of the updater / an app. The metainstaller is downloaded by the
user and can be run from any directory.

The metainstaller may have a tag attached to it. The tag is a piece of unsigned
data from which the metainstaller extracts the ID of the application to be
installed, along with the application's brand code, usage-stats opt-in status,
and any additional parameters to be associated with the application.

After the metainstaller installs the updater, the updater installs an
application by connecting to update servers and [downloading and executing an
application installer](#Updates).

On Windows, the tag is embedded in one of the certificates in the metainstaller
PE. The tag is supported for both EXE and MSI formats.

#### Tag Format
TODO(crbug.com/1328903) - document the rest of the tag format.

##### Brand code
The brand code is a string of up to 4 characters long. The brand code is
persisted during the install, over-installs, and updates.

#### Elevation (Windows)
The metainstaller parses its tag and re-launches itself at high integrity if
installing an application with `needsadmin=true` or `needsadmin=prefers`.

#### Localization
Metainstaller localization presents the metainstaller UI with the user's
preferred language on the current system. Every string shown in the UI is
translated.

### Bundle Installer
The bundle installer allows installation of more than one application. The
bundle installer is typically used in software distribution scenarios.

TODO(crbug.com/1035895): Document bundled installers.

### Standalone Installer
TODO(crbug.com/1035895): Document the standalone installer, including building
a standalone installer for a given application.

Standalone installers embed all data required to install the application,
including the payload and various configuration data needed by the application
setup. Such an install completes even if a network connection is not available.

Standalone installers are used:

1. when an interactive user experience is not needed, such as automated
deployments in an enterprise.
2. when downloading the application payload is not desirable for any reason.
3. during OEM installation.

TODO(crbug.com/1139014): Document OEM.

Applications on macOS frequently install via "drag-install", and then install
the updater using a standalone installer on the application's first-run. The
updater app can be embedded in a macOS application bundle as a helper and then
invoked with appropriate command line arguments to install itself.

### MSI Wrapper
TODO(crbug.com/1327497) - document.

### Scope
The updater is installed in one of the following modes (or scopes):
1. per-system (or per-machine). This mode requires administrator privileges.
2. per-user

Per-machine and per-user instances of the updater can run side by side.

Depending on the scope, the updater is installed at:

* (Windows, User): `%LOCAL_APP_DATA%\{COMPANY}\{UPDATERNAME}\{VERSION}\updater.exe`
* (Windows, System): `%PROGRAM_FILES%\{COMPANY}\{UPDATERNAME}\{VERSION}\updater.exe`
* (macOS, User): `~/Library/{COMPANY}/{UPDATERNAME}/{VERSION}/{UPDATERNAME}.app`
* (macOS, System): `/Library/{COMPANY}/{UPDATERNAME}/{VERSION}/{UPDATERNAME}.app`

### Command Line

The updater's functionality is split between several processes. The mode of a
process is determined by command-line arguments:

*   --install [--app-id=...]
    *   Install and activate this version of the updater if there is no active
        updater.
    *   --app-id=...
        *   Also install the given application.
    *   --tag=...
        *   Supplies the install metadata needed when installing an
            application. Typically, a tagged metainstaller invokes the updater
            with this command line argument.
        *   If --tag is specified, --install is assumed.
    *   --handoff=...
        *   As --tag.
    *  --offlinedir=...
        *   Performs offline install, which means no update check or file
            download is performed against the server during installation.
            All data is read from the files in the directory instead.
        *   Files in offline directory:
            * Manifest file, named `OfflineManifest.gup` or *`<app-id>`*`.gup`.
              The file contains the update check response in XML format.
            * App installer.
        *   The switch can be combined with `--handoff` above.
*   --uninstall
    *   Uninstall all versions of the updater.
*   --uninstall-self
    *   Uninstall this version of the updater.
*   --uninstall-if-unused
    *   Uninstall all versions of the updater, only if there are no apps being
        kept up to date by the updater.
*   --wake
    *   Trigger the updater's periodic background tasks. If this version of the
        updater is inactive, it may qualify and activate, or uninstall itself.
        If this version of the updater is active, it may check for updates for
        applications, unregister uninstalled applications, and more.
*   --crash-me
    *   Record a backtrace in the log, crash the program, save a crash dump,
        and report the crash.
*   --crash-handler
    *   Starts a crash handler for the parent process.
*   --server
    *   Launch the updater RPC server. The server answers RPC messages on
        the UpdateService interface only.
    *   --service=update|update-internal
        *   If `update`, the server answers RPC messages on the
            UpdateService interface only.
        *   If `update-internal`, the server answers RPC messages on the
            UpdateServiceInternal interface only.
*   --windows-service
    *   This switch starts the Windows service. This switch is invoked by the
        SCM either as a part of system startup (`SERVICE_AUTO_START`) or when
        `CoCreate` is called on one of several CLSIDs that the server supports.
    *   --console
        *   Run in interactive mode.
    *   -–com-service
        *   If present, run in a mode analogous to --server --service=update.
            This switch is passed to `ServiceMain` by the SCM when CoCreate is
            called on one of several CLSIDs that the server supports. This is
            used for:
            *   The Server for the UI when installing Machine applications.
            *   The On-Demand COM Server for Machine applications.
            *   COM Server for launching processes at System Integrity, i.e.,
                an Elevator.
*   --update
    *   Install this version of the updater as an inactive instance.
*   --recover
    *   Repair the installation of the updater.
    *   --appguid=...
        *   After recovery, register an application with this id.
    *   --browser-version=...
        *   Register an application with this version.
        *   If --browser-version is specified, --recover can be omitted.
    *  --sessionid=...
        *   Specifies the sesionid associated with this recovery attempt.
*   --test
    *   Exit immediately with no error.
*   --healthcheck
    *   Exit immediately with no error.

If none of the above arguments are set, the updater exits with an error.

Additionally, the mode may be modified by combining it with:
*   --system
    *   The updater operates in system scope if and only if this switch is
        present.

### Backward-Compatible Updater Shims
To maintain backwards compatibility with
[Omaha](https://github.com/google/omaha) and
[Keystone](https://code.google.com/archive/p/update-engine/), the updater
installs small versions of those programs that implement a subset of their APIs.

#### Keystone Shims
The updater installs a Keystone-like application that contains these shims:

1.  The Keystone app executable.
1.  The ksadmin helper executable.
2.  The ksinstall helper executable.
3.  The Keystone Agent helper app executable.

Both the Keystone and Keystone Agent executables simply exit immediately when
run.

The ksinstall executable expects to be called with `--uninstall` and possibly
`--system`. If so, it deletes the Keystone shim (but not the overall updater)
from the file system. Otherwise, it exits with a non-zero exit code.

The ksadmin shim is frequently called by applications and handles a variety of
command lines:

*   --delete, -d
    *   Delete a ticket.
    *   Accepts -P.
*   --install, -i
    *   Check for and apply updates.
*   --ksadmin-version, -k
    *   Print the version of ksadmin.
*   --print
    *   An alias for --print-tickets.
*   --print-tag, -G
    *   Print a ticket's tag.
    *   Accepts -P.
*   --print-tickets, -p
    *   Print all tickets.
    *   Accepts -P.
*   --register, -r
    *   Register a new ticket or update an existing one.
    *   Accepts -P, -v, -x, -e, -a, -K, -H, -g.

Some of these actions accept parameters:

*   --brand-key, -b plistKeyName
    *   Set the brand code key. Use with -P and -B. Value must be empty or
        KSBrandID.
*   --brand-path, -B pathToPlist
    *   Set the brand code path. Use with -P and -b.
*   --productid, -P id
    *   Specifies the application ID.
*   --system-store, -S
    *   Use the system-scope updater, even if not running as root.
    *   Not all operations can be performed with -S if not running as root.
*   --tag, -g ap
    *   Set the application's additional parameters. Use with -P.
*   --tag-key, -K plistKeyName
    *   Set the additional parameters path key. Use with -P and -H.
*   --tag-path, -H pathToPlist
    *   Set the tag path. Use with -P and -K.
*   --user-initiated, -F
    *   Set foreground priority for this operation.
*   --user-store, -U
    *   Use a per-user ticket store, even if running as root.
*   --version, -v version
    *   Set the application's version. Use with -P.
*   --version-key, -e plistKeyName
    *   Set the version path key. Use with -P and -a.
*   --version-path, -a pathToPlist
    *   Set the version path. Use with -P and -e.
*   --xcpath, -x PATH
    *   Set a path to use as an existence checker.

#### Omaha Shims
On Windows, the updater replaces Omaha's files with a copy of the updater, and
keeps the Omaha registry entry
(`CLIENTS/{430FD4D0-B729-4F61-AA34-91526481799D}`) up-to-date with the latest
`pv` value. Additionally, the updater replaces the Omaha uninstall command line
with its own.

### Installer User Interface
TODO(crbug.com/1035895): Document UI/UX.

The user interface is localized in the following languages: TBD.

TODO(crbug.com/1014591): Implement install cancellation.

The install flow can be stopped before the payload finished downloading.

TODO(crbug.com/1286580): Implement silent mode.

Has a silent mode where the UI is not displayed at all.

#### Help Button
If the installation fails, the updater shows an error message with a "Help"
button. Clicking the help button opens a web page in the user's default browser.
The page is opened with a query string:
`?product={AppId}&errorcode={ErrorCode}`.

### Install Source

TODO(crbug.com/1327491) - Implement the following algorithm.

The `installsource` identifies the originator of an install. It is provided on
the command line of the metainstaller.

TODO(crbug.com/1327491) - is this needed? If yes, document the algorithm.

## Updates
There is no limit for the number of retries to update an application if the
update fails repeatedly.

### Protocol
The updater communicates with update servers using the
[Omaha Protocol](protocol_3_1.md).

#### Security
It is not possible to MITM the updater even if the network (including TLS) is
compromised. The integrity of the client-server communication is guaranteed
by the [Client Update Protocol (CUP)](cup.md).

#### Retries
The updater does not retry an update check that transacted with the backend,
even if the response was erroneous (misformatted or unparsable), until the
next normally scheduled update check.

#### DOS Mitigation
The updater sends DoS mitigation headers in requests to the server.

When the server responds with an `X-Retry-After header`, the client does not
issue another update check until the specified period has passed (maximum 24
hours).

* The updater distinguishes between foreground and background priority: if an
  `X-Retry-After` was received in the background case, a foreground update is
  still permitted (but not if it was received in response to a foreground
  update).

#### Usage Counts
TODO(crbug.com/1329328) - document the client responsibilities.

#### Cohort Tracking
The client records the `cohort`, `cohortname`, and `cohorthint` values from the
server in each update response (even if there is no-update) and reports them on
subsequent update checks.

### Installer APIs
As part of installing or updating an application, the updater executes the
application's installer. The API for the application installer is platform-
specific.

The macOS API is [defined here](installer_api_mac.md).

TODO(crbug.com/1035895): Document Windows installer APIs

### Enterprise Policies
Enterprise policies can prevent the installation of applications:
*   A per-application setting may specify whether an application is installable.
*   If no per-application setting specifies otherwise, the default install
    policy is used.
*   If the default install policy is unset, the application may be installed.

Refer to chrome/updater/protos/omaha\_settings.proto for more details.

### Dynamic Install Parameters

#### `needsadmin`

`needsadmin` is one of the install parameters that can be specified for
first installs via the
[metainstaller tag](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag.py).
`needsadmin` is used to indicate whether the application needs admin rights to
install.

For example, here is a command line for the Updater on Windows that includes:
```
UpdaterSetup.exe --install --tag="appguid=YourAppID&needsadmin=False"
```

In this case, the updater client understands that the application installer
needs to install the application on a per-user basis for the current user.

`needsadmin` has the following supported values:
* `true`: the application supports being installed systemwide and once
installed, is available to all users on the system.
* `false`: the application supports only user installs.
* `prefers`: the application installation is first attempted systemwide. If the
user refuses the
[UAC prompt](https://docs.microsoft.com/en-us/windows/security/identity-protection/user-account-control/how-user-account-control-works)
however, the application is then only installed for the current user. The
application installer needs to be able to support the installation as system, or
per-user, or both modes.

#### `installdataindex`

`installdataindex` is one of the install parameters that can be specified for
first installs on the command line or via the
[metainstaller tag](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag.py).

For example, here is a typical command line for the Updater on Windows:
```
UpdaterSetup.exe /install "appguid=YourAppID&appname=YourAppName&needsadmin=False&lang=en&installdataindex =verboselog"
```

In this case, the updater client sends the `installdataindex` of `verboselog` to
the update server.

This is how a [JSON](https://www.json.org/) request from the updater client may
look like:

```
{
   "request":{
      "@os":"win",
      "@updater":"updater",
      "acceptformat":"crx3",
      "app":[
         {
            "appid":"YourAppID",
            "data":[
               {
                  "index":"verboselog",
                  "name":"install"
               }
            ],
            "enabled":true,
            "installsource":"ondemand",
            "ping":{
               "r":-2
            },
            "updatecheck":{
               "sameversionupdate":true
            },
            "version":"0.1"
         }
      ],
      "arch":"x86",
      "dedup":"cr",
      "domainjoined":true,
      "hw":{
         "avx":true,
         "physmemory":32,
         "sse":true,
         "sse2":true,
         "sse3":true,
         "sse41":true,
         "sse42":true,
         "ssse3":true
      },
      "ismachine":false,
      "lang":"en-US",
      "nacl_arch":"x86-64",
      "os":{
         "arch":"x86_64",
         "platform":"Windows",
         "version":"10.0.19042.1586"
      },
      "prodversion":"101.0.4949.0",
      "protocol":"3.1",
      "requestid":"{6b417770-1f68-4d52-8843-356760c84d33}",
      "sessionid":"{37775211-4487-48d5-845d-35a1d71b03bc}",
      "updaterversion":"101.0.4949.0",
      "wow64":true
   }
}
```

The server retrieves the data corresponding to `installdataindex=verboselog`
and returns it back to the updater client.

This is how a JSON response from the update server may look like:

```
  "response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "data":[{
      "status":"ok",
      "name":"install",
      "index":"verboselog",
      "#text":"{\"logging\":{\"verbose\":true}}"
     }],
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"},
                    {"codebasediff":"http://diff.example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "run":"UpdaterSetup.exe",
      "arguments":"--arg1 --arg2",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }
```

The updater client writes this data to a temporary file in the same directory as
the application installer. This is for security reasons, since writing the data
to the temp directory could potentially allow a man-in-the-middle attack.

The updater client provides the temporary file as a parameter to the application
installer.

Let's say, as shown above, that the update server responds with these example
file contents:
```
{"logging":{"verbose":true}}
```

The updater client creates a temporary file, say `c:\my
path\temporaryfile.dat` (assuming the application installer is running from
`c:\my path\YesExe.exe`), with the following file contents:
```
\xEF\xBB\xBF{"logging":{"verbose":true}}
```

and then provide the file as a parameter to the application installer:
```
"c:\my path\YesExe.exe" --installerdata="c:\my path\temporaryfile.dat"
```

* Notice above that the temp file contents are prefixed with an UTF-8 Byte Order
Mark of `EF BB BF`.
* For MSI installers, a property is passed to the installer:
`INSTALLERDATA="pathtofile"`.
* For exe-based installers, as shown above, a command line parameter is
passed to the installer: `--installerdata="pathtofile"`.
* For Mac installers, an environment variable is set:
`INSTALLERDATA="pathtofile"`.
* Ownership of the temp file is the responsibility of the application installer.
The updater does not delete this file.
* This installerdata is not persisted anywhere else, and it is not sent as a
part of pings to the update server.

### Update Formats
The updater accepts updates packaged as CRX₃ files. All files are signed with a
publisher key. The corresponding public key is hardcoded into the updater.

### Differential Updates
TODO(crbug.com/1035895): Document differential updates.

TODO(crbug.com/1331030): Implement differential update support.

### Update Timing
The updater runs periodic tasks every hour, checking its own status, detecting
application uninstalls, and potential checking for updates (if it has been at
least 5 hours since the last update check).

TODO(crbug.com/1329868): implement the following algorithm.
The updater scatters its routine updates:
* Two updaters installed in identical situations and at the same time, after
  some period of time, do not have synchronized times at which they check
  for updates.
  * For example, the updater may randomly choose to wait 6 hours instead of 5 to
    perform the next check. The probability of choosing 6 hours is .1.
  * The change only applies to users who have not overridden their
    `UpdateCheckMs` feature.
  * For testing purposes, the feature can be disabled by creating an
    DWORD value `DisableUpdateAppsHourlyJitter` in `UpdateDev`.
  * For testing purposes, an `UpdateDev` value can set the jitter time to a
    constant value, in the same [0, 60) seconds range. The name of the value is
    `AutoUpdateJitterMs` and it represents the time to wait before an update
    check is made in milliseconds.
* The global load from updaters is scattered among the minutes of the hour, the
  seconds of the minute, and the milliseconds of the second.
  * For example, load spikes on the first minute of the hour, or
    the first second of every minute, or even the first millisecond of every
    second are undesirable.

#### Windows Scheduling of Updates
The update tasks are scheduled using the OS task scheduler.

The time resolution for tasks is 1 minute. Tasks are set to run 5 minutes after
they've been created.

TODO(crbug.com/1328935): implement built in task scheduler as a failover
mechanism

TODO(crbug.com/1035895): Does the updater run at user login on Windows?

### On-Demand Updates
The updater exposes an RPC interface for any user to trigger an update check.
The update can be triggered by any user on the system, even in the system scope.

The caller provides the ID of the item to update, the install data index to
request, the priority, whether a same-version update (repair) is permitted, and
callbacks to monitor the progress and completion of the operation.

The interface provides the version of the update, if an update is available.

Regardless of the normal update check timing, the update check is attempted
immediately.

### App Registration
The updater exposes an RPC interface for users to register an application with
the updater. Unlike on-demand updates, cross-user application registration is
not permitted.

If the application is already installed, it is registered with the version
present on disk. If it has not yet been installed, a version of `0` is used.

### App Activity Reporting
Applications can report whether they are actively used or not through the
updater. Update servers can then aggregate this information to produce user
counts.

#### Windows:
*   When active, the application sets
    HKCU\SOFTWARE\{Company}\Update\ClientState\{AppID} → dr (REG_SZ): 1.
    *   Note: both user-scoped and system-scoped updaters use HKCU.
*   When reporting active use, the updater resets the value to 0.

#### macOS:
*   The application touches
    ~/Library/{Company}/{Company}SoftwareUpdate/Actives/{APPID}.
*   The updater deletes the file when reporting active use.

### EULA/ToS Acceptance
Software can be installed or updated only if the user has agreed to the `Terms
of Service`. The updater only runs if the user has accepted the ToS for at
least one application.

TODO(crbug.com/1035895): Document EULA signals.

### Usage Stats Acceptance
The updater may upload its crash reports and send usage stats if and only if
any piece of software it manages is permitted to send usage stats.

#### Windows:
*   Applications enable usage stats by writing:
    `HKCU\SOFTWARE\{Company}\Update\ClientState\{APPID}` → usagestats (DWORD): 1
    or
    `HKLM\SOFTWARE\{Company}\Update\ClientStateMedium\{APPID}` → usagestats
    (DWORD): 1
*   Applications rescind this permission by writing a value of 0.

#### macOS:
*   Application enable usage stats by setting `IsUploadEnabled` to true for a
    crashpad database maintained in a "Crashpad" subdirectory of their
    application data directory.
*   The updater searches the file system for Crashpad directories belonging
    to {Company}.

### Enterprise Policies
TODO(crbug.com/1035895): Document relevant enterprise policies.

#### Windows
TODO(crbug.com/1035895): Implement this section. (ADMX file export.)

ADMX templates are provided.

### Telemetry
When the updater installs an application (an installer is run) it sends an
event with `"eventtype": 2` indicating the outcome of installation. The updater
does not send such a ping for its own installation.

When the updater updates an application (including itself) it sends an
event with `"eventtype": 3` indicating the outcome of update operation.

When the updater detects the uninstallation of an application, it sends an
event with `"eventtype": 4` to notify the server of the uninstallation.

When the updater attempts to download a file, it sends an event with
`"eventtype": 14` describing the parameters and outcome of the download.

Multiple events associated with an update session are bundled together into a
single request.

### Downloading
There could be multiple URLs for a given application payload. The URLs are tried
in the order they are returned in the update response.

The integrity of the payload is verified.

There is no download cache. Payloads are re-downloaded for applications which
fail to install.

## Services

### Crash Reporting
TODO(crbug.com/1035895): Document updater crash reporting.

### Application Commands (applicable to the Windows version of the Updater)
The feature allows installed products to pre-register and later run command
lines in the format `c:\path-to-exe\exe.exe {params}` (elevated for system
applications). `{params}` is optional and can also include replaceable
parameters substituted at runtime.

The program path is always an absolute path. Additionally, for system
applications,  the program path is also a child of %ProgramFiles% or
%ProgramFiles(x86)%. For instance:
* `c:\path-to-exe\exe.exe` is an invalid path.
* `"c:\Program Files\subdir\exe.exe"` is a valid path.
* `"c:\Program Files (x86)\subdir\exe.exe"` is also a valid path.

#### Registration
App commands are registered in the registry with the following formats:

* New command layout format:
```
    Update\Clients\{`app_id`}\Commands\`command_id`
        REG_SZ "CommandLine" == {command format}
```
* Older command layout format, which may be deprecated in the future:
```
    Update\Clients\{`app_id`}
        REG_SZ `command_id` == {command format}
```

Example `{command format}`: `c:\path-to\echo.exe %1 %2 %3 StaticParam4`

As shown above, `{command format}` needs to be the complete path to an
executable followed by optional parameters.

#### Usage
Once registered, commands may be invoked using the `execute` method in the
`IAppCommandWeb` interface.

```
interface IAppCommandWeb : IDispatch {
  // Use values from the AppCommandStatus enum.
  [propget] HRESULT status([out, retval] UINT*);
  [propget] HRESULT exitCode([out, retval] DWORD*);
  [propget] HRESULT output([out, retval] BSTR*);
  HRESULT execute([in, optional] VARIANT parameter1,
                  [in, optional] VARIANT parameter2,
                  [in, optional] VARIANT parameter3,
                  [in, optional] VARIANT parameter4,
                  [in, optional] VARIANT parameter5,
                  [in, optional] VARIANT parameter6,
                  [in, optional] VARIANT parameter7,
                  [in, optional] VARIANT parameter8,
                  [in, optional] VARIANT parameter9);
};
```

Here is a code snippet a client may use, with error checking omitted:

```
var bundle = update3WebServer.createAppBundleWeb();
bundle.initialize();
bundle.createInstalledApp(appGuid);
var app = bundle.appWeb(0);
cmd = app.command(command);
cmd.execute();
```

Parameters placeholders (`%1-%9`) are filled by the numbered parameters in
`IAppCommandWeb::execute`. Placeholders without corresponding parameters
cause the execution to fail.

Clients may poll for the execution status of commands that they have invoked by
using the `status` method of `IAppCommandWeb`. When the status is
`COMMAND_STATUS_COMPLETE`, the `exitCode` method can be used to get the process
exit code.

#### Command-Line Format
* for system applications, the executable path is in a secure location such
as `%ProgramFiles%` for security, since it runs elevated.
* placeholders are not permitted in the executable path.
* placeholders take the form of a percent character `%` followed by a digit.
Literal `%` characters are escaped by doubling them.

For example, if parameters to `IAppCommandWeb::execute` are `AA` and `BB`
respectively, a command format of:
  `echo.exe %1 %%2 %%%2`
becomes the command line
  `echo.exe AA %2 %BB`

## Uninstallation
On Mac and Linux, if the application was registered with an existence path
checker and no file at that path exists (or if the file at that path is owned
by another user), the updater considers the application uninstalled, sends
the ping, and stops trying to keep it up to date.

On Windows, if the ClientState entry for for the application is deleted, the
app is considered uninstalled.

On Windows, the updater registers a "UninstallCmdLine" under the `Software\
{Company}\Updater` key. This command line can be invoked by application
uninstallers to cause the updater to  update its registrations. The updater
also checks for uninstallations in every periodic task execution.

When the last registered application is uninstalled, the updater uninstalls
itself immediately. The updater also uninstalls itself if it has started
24 times but never had a product (besides itself) registered for updates.

The updater uninstaller removes all updater files, registry keys, RPC hooks,
scheduled tasks, and so forth from the file system, except that it leaves a
small log file in its data directory.

## Associated Tools

### External Constants Overrides
Building the updater produces both a production-ready updater executable and a
version of the executable used for the purposes of testing. The test executable
is identical to the production one except that it allows cetain constants to be
overridden by the execution environment:

*   `url`: Update check & ping-back URL.
*   `use_cup`: Whether CUP is used at all.
*   `cup_public_key`: An unarmored PEM-encoded ASN.1 SubjectPublicKeyInfo with
    the ecPublicKey algorithm and containing a named elliptic curve.
*   `group_policies`: Allows setting group policies, such as install and update
    policies.

Overrides are specified in an overrides.json file placed in the updater data
directory.

### Tagging Tools
TODO(crbug.com/1035895): Document tagging tools.


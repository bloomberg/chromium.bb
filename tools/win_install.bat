@ECHO OFF
SETLOCAL

SET MOZILLA_FOLDER="%ProgramFiles%\Mozilla Firefox\Plugins"

ECHO This script will install the Native Client NPAPI browser plugin in %MOZILLA_FOLDER%
SET /P M=Press y to continue, n to terminate:
IF %M%==y GOTO :INSTALL
ECHO OK, terminating.
GOTO :EXIT

:INSTALL
ECHO Copying...

COPY sel_ldr.exe %MOZILLA_FOLDER%
IF %ERRORLEVEL% NEQ 0 GOTO :ERROR

COPY sdl.dll %MOZILLA_FOLDER%
IF %ERRORLEVEL% NEQ 0 GOTO :ERROR

COPY npGoogleNaClPlugin.dll %MOZILLA_FOLDER%
IF %ERRORLEVEL% NEQ 0 GOTO :ERROR

ECHO Installation completed successfully.
GOTO :EXIT

:ERROR
ECHO Installation failed.

:EXIT
ENDLOCAL





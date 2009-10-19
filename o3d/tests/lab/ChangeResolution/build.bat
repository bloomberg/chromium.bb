@echo off
REM Builds the change resolution project.

:vs2005
reg query HKLM\Software\Microsoft\VisualStudio\8.0 /v InstallDir || goto vs2008
set vs_reg=HKLM\Software\Microsoft\VisualStudio\8.0
goto getkey

:vs2008
reg query HKLM\Software\Microsoft\VisualStudio\9.0 /v InstallDir || goto vsNone
set vs_reg=HKLM\Software\Microsoft\VisualStudio\9.0
goto getkey

:vsNone
echo Visual Studio path cannot be found in registry. Are you using 2005 or 2008?
exit

:getkey
for /f "tokens=2,*" %%a in ('reg query %vs_reg% /v InstallDir ^| findstr InstallDir') do set vs_dir=%%b

"%vs_dir%devenv.com" change_resolution.sln /build
@echo off
rem simplified replacement for the original shell script
set ROOT=%~dp0

set XLIBDIR=%ROOT%..\lib
set XINCLUDEDIR=%ROOT%..\include
set XVERSION=2.2.5
set XCFLAGS=-I"%XINCLUDEDIR%"
set XLIBS=-ljpeg -lpng -lfreetype -lXpm -ltiff -lz -liconv
set XLDFLAGS=-L"%XLIBDIR%"

for %%p in (%*) do (
  if x%%p == x--libdir     echo %XLIBDIR%
  if x%%p == x--includedir echo %XINCLUDEDIR%
  if x%%p == x--version    echo %XVERSION%
  if x%%p == x--cflags     echo %XCFLAGS%
  if x%%p == x--libs       echo %XLIBS%
  if x%%p == x--all        goto ALL
) 
goto END

:ALL
echo GD library  %XVERSION%
echo includedir: %XINCLUDEDIR%
echo cflags:     %XCFLAGS%
echo ldflags:    %XLDFLAGS%
echo libs:       %XLIBS%
echo libdir:     %XLIBDIR%
echo features: GD_GIF GD_GIFANIM GD_OPENPOLYGON GD_ZLIB GD_PNG GD_FREETYPE GD_JPEG GD_XPM GD_TIFF

:END

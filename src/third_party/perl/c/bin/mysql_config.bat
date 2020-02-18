@echo off
rem simplified replacement for the original shell script
set ROOT=%~dp0

set XCFLAGS=-I%ROOT%..\include\mysql50716
set XLIBS=-L%ROOT%..\lib -lmysql
set XVERSION=5.7.16
set XPREFIX=%ROOT%..\

for %%p in (%*) do (
  if x%%p == x--cflags     echo %XCFLAGS%
  if x%%p == x--libs       echo %XLIBS%
  if x%%p == x--version    echo %XVERSION%
  if x%%p == x--prefix     echo %XPREFIX%
) 

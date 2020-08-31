@echo off
rem simplified replacement for the original shell script

for %%p in (%*) do (
  if x%%p == x--version  echo 2.10.0&                        goto END
  if x%%p == x--cflags   echo -I"%~dp0..\include\freetype2"& goto END
  if x%%p == x--libs     echo -L"%~dp0..\lib" -lfreetype&    goto END
)

echo Usage: %~n0 --version or --cflags or --libs

:END
@echo off
set drive=%~dp0
set drivep=%drive%
If $#\#$==$#%drive:~-1%#$ set drivep=%drive:~0,-1%
set PATH=%drivep%\perl\site\bin;%drivep%\perl\bin;%drivep%\c\bin;%PATH%
rem env variables
set TERM=dumb
set PERL_JSON_BACKEND=JSON::XS
set PERL_YAML_BACKEND=YAML
rem avoid collisions with other perl stuff on your system
set PERL5LIB=
set PERL5OPT=
set PERL_MM_OPT=
set PERL_MB_OPT=
perl %*

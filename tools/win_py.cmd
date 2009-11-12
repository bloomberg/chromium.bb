@set TOOL_DIR=%~dp0
@set NACL_ROOT=%TOOL_DIR%\..\..
@%NACL_ROOT%\third_party\\python_24\\setup_env.bat && python.exe %*


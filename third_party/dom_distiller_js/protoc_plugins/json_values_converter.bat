@echo off

set PYTHONPATH=..\..\third_party\protobuf\python;..\..\third_party\protobuf\third_party\six
call python %~dp0\json_values_converter.py

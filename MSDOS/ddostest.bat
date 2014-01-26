
@echo off

rem This file assumes that the main executable is one dir higher

echo Test Results log
echo .
echo Creating test log file for DEBUG DOS code
echo Ideally this will be part of a redirected output file
echo .
echo This may take a while!
echo .

for %%A in (Descr*.*) do ..\mddealer %%A



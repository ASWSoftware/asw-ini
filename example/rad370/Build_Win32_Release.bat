@echo off

REM Builds the ASWIni example with RAD Studio (Win32, Release). Requires the Rad370 environment variable.

call "%Rad370%\bin\rsvars.bat"

pushd "%~dp0"
call cmd /c msbuild /t:Build /p:Config=Release;Platform=Win32 ASWIniExample.cbproj
set "BUILD_RESULT=%ERRORLEVEL%"
popd

exit /b %BUILD_RESULT%

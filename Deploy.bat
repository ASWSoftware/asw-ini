@echo off
setlocal EnableExtensions

REM Copies the ASWIni library sources, build files and docs to a folder, e.g. for a release package or to drop into
REM another project. Usage: Deploy.bat "C:\path\to\staging"

set "SCRIPT_DIR=%~dp0"
set "TARGET_DIR=%~1"

if "%TARGET_DIR%"=="" (
    echo ERROR: A staging/deploy folder is required.
    echo Usage: Deploy.bat "C:\path\to\staging"
    exit /b 1
)

echo Deploying ASWIni release files to:
echo   %TARGET_DIR%

if not exist "%TARGET_DIR%\." (
    echo Creating target folder...
    mkdir "%TARGET_DIR%"
    if errorlevel 1 (
        echo ERROR: Could not create target folder.
        exit /b 1
    )
)

if not exist "%TARGET_DIR%\ASWIni\." (
    echo Creating ASWIni folder...
    mkdir "%TARGET_DIR%\ASWIni"
    if errorlevel 1 (
        echo ERROR: Could not create "%TARGET_DIR%\ASWIni".
        exit /b 1
    )
)

call :CopyFile "CHANGELOG.md" "%TARGET_DIR%\CHANGELOG.md"
if errorlevel 1 exit /b 1
call :CopyFile "CMakeLists.txt" "%TARGET_DIR%\CMakeLists.txt"
if errorlevel 1 exit /b 1
call :CopyFile "LICENSE" "%TARGET_DIR%\LICENSE"
if errorlevel 1 exit /b 1
call :CopyFile "README.md" "%TARGET_DIR%\README.md"
if errorlevel 1 exit /b 1

call :CopyFile "cmake\ASWIniConfig.cmake.in" "%TARGET_DIR%\cmake\ASWIniConfig.cmake.in"
if errorlevel 1 exit /b 1

call :CopyFile "ASWIni\ASWIni_Document.cpp" "%TARGET_DIR%\ASWIni\ASWIni_Document.cpp"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Document.h" "%TARGET_DIR%\ASWIni\ASWIni_Document.h"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_File.cpp" "%TARGET_DIR%\ASWIni\ASWIni_File.cpp"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_File.h" "%TARGET_DIR%\ASWIni\ASWIni_File.h"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Options.cpp" "%TARGET_DIR%\ASWIni\ASWIni_Options.cpp"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Options.h" "%TARGET_DIR%\ASWIni\ASWIni_Options.h"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Types.cpp" "%TARGET_DIR%\ASWIni\ASWIni_Types.cpp"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Types.h" "%TARGET_DIR%\ASWIni\ASWIni_Types.h"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Utils.cpp" "%TARGET_DIR%\ASWIni\ASWIni_Utils.cpp"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Utils.h" "%TARGET_DIR%\ASWIni\ASWIni_Utils.h"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Version.cpp" "%TARGET_DIR%\ASWIni\ASWIni_Version.cpp"
if errorlevel 1 exit /b 1
call :CopyFile "ASWIni\ASWIni_Version.h" "%TARGET_DIR%\ASWIni\ASWIni_Version.h"
if errorlevel 1 exit /b 1

echo Deployment completed successfully.
exit /b 0

:CopyFile
set "SOURCE_FILE=%~1"
set "DESTINATION_FILE=%~2"
echo Copying %SOURCE_FILE%...
if not exist "%SCRIPT_DIR%%SOURCE_FILE%" (
    echo ERROR: Source file not found: "%SCRIPT_DIR%%SOURCE_FILE%"
    exit /b 1
)
if not exist "%DESTINATION_FILE%\.." (
    mkdir "%DESTINATION_FILE%\.."
)
copy /Y "%SCRIPT_DIR%%SOURCE_FILE%" "%DESTINATION_FILE%" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy "%SOURCE_FILE%".
    exit /b 1
)
exit /b 0

@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "SCRIPT_DIR=%~dp0"
set "PROJECT_DIR=%SCRIPT_DIR:~0,-1%"
set "ROOT_DIR=%PROJECT_DIR%\.."
set "BUILD_DIR=%ROOT_DIR%\build\xai_type_aq_plugin"
set "DIST_DIR=%ROOT_DIR%\dist\aq_hollister_plugin_hostvf_ocv470_release"
set "AQ_SDK_DIR=%ROOT_DIR%\new"

echo [1/4] Configuring CMake...
cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -DXAI_TYPE_AQ_BUILD_SHARED=ON
if errorlevel 1 (
    echo CMake configure failed.
    exit /b 1
)

echo [2/4] Building Release...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo Release build failed.
    exit /b 1
)

echo [3/4] Preparing output folders...
if not exist "%DIST_DIR%\bin" mkdir "%DIST_DIR%\bin"
if not exist "%DIST_DIR%\tools" mkdir "%DIST_DIR%\tools"
if not exist "%DIST_DIR%\lib" mkdir "%DIST_DIR%\lib"
if not exist "%DIST_DIR%\include" mkdir "%DIST_DIR%\include"

echo [4/4] Syncing build artifacts...
copy /Y "%BUILD_DIR%\Release\xai_type_aq.dll" "%DIST_DIR%\bin\xai_type_aq.dll" >nul
if errorlevel 1 (
    echo Failed to copy bin\xai_type_aq.dll
    exit /b 1
)

copy /Y "%BUILD_DIR%\Release\xai_type_aq.dll" "%DIST_DIR%\tools\xai_type_aq.dll" >nul
if errorlevel 1 (
    echo Failed to copy tools\xai_type_aq.dll
    exit /b 1
)

if exist "%BUILD_DIR%\Release\xai_type_aq_verify_plugin.exe" (
    copy /Y "%BUILD_DIR%\Release\xai_type_aq_verify_plugin.exe" "%DIST_DIR%\tools\xai_type_aq_verify_plugin.exe" >nul
    if errorlevel 1 (
        echo Failed to copy tools\xai_type_aq_verify_plugin.exe
        exit /b 1
    )
)

if exist "%BUILD_DIR%\Release\xai_type_aq.lib" (
    copy /Y "%BUILD_DIR%\Release\xai_type_aq.lib" "%DIST_DIR%\lib\xai_type_aq.lib" >nul
    if errorlevel 1 (
        echo Failed to copy lib\xai_type_aq.lib
        exit /b 1
    )
)

if exist "%AQ_SDK_DIR%\xai_api.lib" (
    copy /Y "%AQ_SDK_DIR%\xai_api.lib" "%DIST_DIR%\lib\xai_api.lib" >nul
    if errorlevel 1 (
        echo Failed to copy lib\xai_api.lib
        exit /b 1
    )
)

if exist "%AQ_SDK_DIR%\xai_apid.lib" (
    copy /Y "%AQ_SDK_DIR%\xai_apid.lib" "%DIST_DIR%\lib\xai_apid.lib" >nul
    if errorlevel 1 (
        echo Failed to copy lib\xai_apid.lib
        exit /b 1
    )
)

for %%F in (
    mmessage.h
    xai_api.h
    xai_def.h
    xai_err.h
    xai_inst_base.h
    xai_meth.h
    xai_type_api.h
    xai_type_base.h
    xai_type_aq.h
) do (
    copy /Y "%PROJECT_DIR%\%%F" "%DIST_DIR%\include\%%F" >nul
    if errorlevel 1 (
        echo Failed to copy include\%%F
        exit /b 1
    )
)

echo Build and sync completed.
echo Build dir: %BUILD_DIR%
echo Dist dir : %DIST_DIR%
exit /b 0

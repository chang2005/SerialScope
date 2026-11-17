@echo off
setlocal EnableExtensions

rem Build SerialScope and create a self-contained Windows release in build\release.
rem Override QT_ROOT / MINGW_ROOT / BUILD_TYPE from the environment if your kit differs.

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_DIR=%%~fI"
set "BUILD_DIR=%PROJECT_DIR%\build"

if not defined QT_ROOT set "QT_ROOT=D:\Qt\6.8.3\mingw_64"
if not defined MINGW_ROOT set "MINGW_ROOT=D:\Qt\Tools\mingw1310_64"
if not defined BUILD_TYPE set "BUILD_TYPE=Release"

if not exist "%QT_ROOT%\bin\windeployqt.exe" (
    echo [ERROR] windeployqt not found under "%QT_ROOT%\bin".
    echo         Set QT_ROOT to your Qt 6 mingw_64 directory.
    exit /b 1
)
if not exist "%MINGW_ROOT%\bin\g++.exe" (
    echo [ERROR] g++.exe not found under "%MINGW_ROOT%\bin".
    echo         Set MINGW_ROOT to the MinGW toolchain directory.
    exit /b 1
)

set "PATH=%QT_ROOT%\bin;%MINGW_ROOT%\bin;%PATH%"

echo [1/4] Configuring...
cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
    -DCMAKE_CXX_COMPILER="%MINGW_ROOT%\bin\g++.exe" ^
    -DCMAKE_BUILD_TYPE="%BUILD_TYPE%"
if errorlevel 1 goto :fail

echo [2/4] Building...
cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 goto :fail

echo [3/4] Deploying Qt runtime...
cmake --build "%BUILD_DIR%" --target deploy
if errorlevel 1 goto :fail

echo [4/4] Packaging Windows release...
cmake --build "%BUILD_DIR%" --target package_release
if errorlevel 1 goto :fail

echo.
echo Done: "%BUILD_DIR%\dist\SerialScope.exe"
echo Release ZIP: "%BUILD_DIR%\release"
echo Copy the whole "dist" folder or the ZIP to the target Windows machine.
exit /b 0

:fail
echo.
echo [ERROR] Build/deploy failed. See the output above.
exit /b 1

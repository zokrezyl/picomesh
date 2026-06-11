@echo off
rem ---------------------------------------------------------------------------
rem picomesh Windows build driver.
rem
rem Locates the Visual Studio install via vswhere (fixed path), sources its x64
rem build environment so cl.exe + the MSVC libraries are on PATH, then runs the
rem CMake configure + build. The woodpecker Windows agent runs a bare cmd shell
rem with no vcvars sourced, so the mesh build cannot rely on cl being present —
rem this wrapper makes the step self-contained.
rem
rem Usage:  build-tools\windows\build.bat
rem ---------------------------------------------------------------------------
setlocal enableextensions enabledelayedexpansion

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo build.bat: vswhere not found at "%VSWHERE%" 1>&2
  exit /b 1
)

set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
  echo build.bat: no Visual Studio with the VC x64 toolset found 1>&2
  exit /b 1
)
echo build.bat: using Visual Studio at "%VSINSTALL%"

call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
  echo build.bat: vcvarsall x64 failed 1>&2
  exit /b 1
)

rem Ninja ships with the VS "C++ CMake tools" component but vcvarsall does not
rem add it to PATH. If it isn't already reachable, prepend the bundled copy.
where ninja >nul 2>nul
if errorlevel 1 (
  for /f "usebackq tokens=*" %%n in (`dir /b /s "%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" 2^>nul`) do (
    set "PATH=%%~dpn;!PATH!"
  )
)

cmake -S . -B build-desktop-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl
if errorlevel 1 exit /b 1

cmake --build build-desktop-release --parallel
if errorlevel 1 exit /b 1

echo build.bat: build complete
endlocal

@echo off
REM Build and run GTWTests. Run from the solution root or tests\ directory.
setlocal

set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set PROJ=%~dp0GTWTests.vcxproj
set EXE=%~dp0bin\Debug\GTWTests.exe

echo [build] GTWTests Debug x64
%MSBUILD% "%PROJ%" /p:Configuration=Debug /p:Platform=x64 /v:minimal
if errorlevel 1 (
    echo [FAIL] Build failed.
    exit /b 1
)

echo.
echo [run]
"%EXE%"
exit /b %errorlevel%

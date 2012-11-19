@echo off

REM Automatic windows build script

set inputpath=%1
set inputconfig=%2
set inputcommands=%3
for /f "useback tokens=*" %%a in ('%inputcommands%') do set inputcommands=%%~a
echo.%inputcommands%

set unixlike=/%inputpath:\=/%
set localized=%unixlike::=%
set makename=bash_make_%inputconfig%.sh
set sourcemake=%inputpath%%makename%
set targetfile=%localized%%makename%

if exist %sourcemake% goto build_program
REM create unix build script
echo.cd %localized%>>%sourcemake%
echo.%inputcommands%>>%sourcemake%
echo.make config=%inputconfig%>>%sourcemake%

:build_program
REM call builder
echo.Building with %targetfile% and config %inputconfig%
REM C:\mingw64
%MINGW_ROOT%\msys\bin\bash.exe --login -i %targetfile%
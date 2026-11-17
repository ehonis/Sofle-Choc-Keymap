@echo off
rem Run the `sofle` bash script inside QMK MSYS from PowerShell or cmd.
rem   sofle flash:work
setlocal
set "QMK_MSYS=C:\QMK_MSYS"
if not exist "%QMK_MSYS%\usr\bin\bash.exe" (
    echo QMK MSYS not found at %QMK_MSYS%. Install it from https://msys.qmk.fm/ 1>&2
    exit /b 1
)
set "MSYSTEM=MINGW64"
set "CHERE_INVOKING=1"
set "SOFLE_SCRIPT=%~dp0sofle"
"%QMK_MSYS%\usr\bin\bash.exe" --login -c "exec bash \"$(cygpath -u \"$SOFLE_SCRIPT\")\" \"$@\"" sofle %*

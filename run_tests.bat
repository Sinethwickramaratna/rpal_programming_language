@echo off
setlocal enabledelayedexpansion
set PASS=0
set FAIL=0
set TESTS_DIR=tests
set BINARY=rpal\rpal20.exe

if not exist "%BINARY%" (
    echo Error: %BINARY% not found
    exit /b 1
)

for %%f in (%TESTS_DIR%\*_input.txt %TESTS_DIR%\*_input) do (
    set "name=%%~nf"
    set "expected=%TESTS_DIR%\!name:_input=_expected!%%~xf"

    if not exist "!expected!" (
        set "expected=%TESTS_DIR%\!name:_input=_expected!"
    )

    %BINARY% %%f > %TESTS_DIR%\actual_output.txt 2>&1

    fc /w %TESTS_DIR%\actual_output.txt "!expected!" >nul 2>&1
    if errorlevel 1 (
        echo FAIL: %%~nf
        set /a FAIL+=1
    ) else (
        echo PASS: %%~nf
        set /a PASS+=1
    )
)

echo.
echo Results: %PASS% passed, %FAIL% failed
endlocal
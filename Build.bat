@echo off
setlocal

rem === Fast Pre-UBT Linter Check (<150ms) ===
where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    if exist "%USERPROFILE%\.gemini\config\plugins\doc-craft\scripts\ue_linter.py" (
        python "%USERPROFILE%\.gemini\config\plugins\doc-craft\scripts\ue_linter.py" "%~dp0Source"
        if errorlevel 1 (
            echo.
            echo [!] FAST PRE-UBT LINTER FAILED: UHT or C++ syntax errors detected!
            echo [!] Aborted before launching slow UBT process. Fix errors above.
            exit /b 1
        )
    )
)

"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ThreadTalesEditor Win64 Development "%~dp0ThreadTales.uproject" -waitmutex -NoHotReloadFromIDE %*
exit /b %ERRORLEVEL%

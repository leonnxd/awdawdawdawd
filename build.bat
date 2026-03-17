@echo off
title lain Build Script

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

cd /d "%~dp0"

echo Starting build for lain.sln...
echo Configuration: Release, Platform: x64

rem Delete any previously renamed executables
for %%f in (
    "release\Obs.exe" "release\Paint.exe" "release\Firefox.exe" "release\Notion.exe" "release\ShareX.exe"
    "release\HandBrake.exe" "release\Trello.exe" "release\Slack.exe" "release\Krita.exe" "release\Miro.exe"
    "release\Notepad++.exe" "release\ClipClip.exe" "release\LastPass.exe" "release\QuickSync.exe" "release\CleanUpUtility.exe"
    "release\SmartUpdater.exe" "release\DocManager.exe" "release\AutoBackup.exe" "release\SystemGuard.exe" "release\TaskOptimizer.exe"
    "release\MediaFixer.exe" "release\SystemMonitor.exe" "release\DriveCleaner.exe" "release\FileGuard.exe" "release\EasyUpdater.exe"
    "release\StreamOptimizer.exe" "release\NetworkAnalyzer.exe" "release\AutoRestore.exe" "release\TaskManagerPlus.exe" "release\PowerCleaner.exe"
    "release\SecureDefender.exe" "release\SilentScanner.exe" "release\MemoryBoost.exe" "release\CloudSyncer.exe" "release\DataWiper.exe"
    "release\SystemRebuilder.exe" "release\FileShredder.exe" "release\SpeedEnhancer.exe" "release\DiskDefragPro.exe" "release\PrivacyProtector.exe"
    "release\AutoFixer.exe" "release\RegistryCleaner.exe" "release\QuickDefender.exe" "release\SafeConnect.exe" "release\PrivacyGuard.exe"
    "release\WebProtect.exe" "release\FirewallPlus.exe" "release\SystemGuard.exe" "release\CryptGuard.exe" "release\PhishShield.exe" "release\SafeLogin.exe"
    "release\ClearCache.exe" "release\Lockdown.exe"
) do (
    if exist "%%f" del "%%f" 2>nul
)

msbuild "lain.sln" /p:Configuration=Release /p:Platform=x64 /m

if %errorlevel% neq 0 (
    echo.
    echo ==================
    echo      BUILD FAILED
    echo ==================
    echo Check the output above for errors.
    exit
) else (
    echo.
    echo =====================
    echo   BUILD SUCCESSFUL
    echo =====================
    echo The output files should be in the 'release' or 'x64\Release' directory.
    
    setlocal enabledelayedexpansion
    
    echo Debug: Raw random value: %random%
    timeout /t 1 /nobreak >nul
    set /a "randomIndex=%random% %% 53"  rem Changed to 53 for 53 options
    
    rem Assign a random application name
    if !randomIndex! equ 0 set "selectedName=Obs"
    if !randomIndex! equ 1 set "selectedName=Paint"
    if !randomIndex! equ 2 set "selectedName=Firefox"
    if !randomIndex! equ 3 set "selectedName=Notion"
    if !randomIndex! equ 4 set "selectedName=ShareX"
    if !randomIndex! equ 5 set "selectedName=HandBrake"
    if !randomIndex! equ 6 set "selectedName=Trello"
    if !randomIndex! equ 7 set "selectedName=Slack"
    if !randomIndex! equ 8 set "selectedName=Krita"
    if !randomIndex! equ 9 set "selectedName=Miro"
    if !randomIndex! equ 10 set "selectedName=Notepad++"
    if !randomIndex! equ 11 set "selectedName=ClipClip"
    if !randomIndex! equ 12 set "selectedName=LastPass"
    if !randomIndex! equ 13 set "selectedName=QuickSync"
    if !randomIndex! equ 14 set "selectedName=CleanUpUtility"
    if !randomIndex! equ 15 set "selectedName=SmartUpdater"
    if !randomIndex! equ 16 set "selectedName=DocManager"
    if !randomIndex! equ 17 set "selectedName=AutoBackup"
    if !randomIndex! equ 18 set "selectedName=SecureVault"
    if !randomIndex! equ 19 set "selectedName=TaskOptimizer"
    if !randomIndex! equ 20 set "selectedName=MediaFixer"
    if !randomIndex! equ 21 set "selectedName=SystemMonitor"
    if !randomIndex! equ 22 set "selectedName=DriveCleaner"
    if !randomIndex! equ 23 set "selectedName=FileGuard"
    if !randomIndex! equ 24 set "selectedName=EasyUpdater"
    if !randomIndex! equ 25 set "selectedName=StreamOptimizer"
    if !randomIndex! equ 26 set "selectedName=NetworkAnalyzer"
    if !randomIndex! equ 27 set "selectedName=AutoRestore"
    if !randomIndex! equ 28 set "selectedName=TaskManagerPlus"
    if !randomIndex! equ 29 set "selectedName=PowerCleaner"
    if !randomIndex! equ 30 set "selectedName=SecureDefender"
    if !randomIndex! equ 31 set "selectedName=SilentScanner"
    if !randomIndex! equ 32 set "selectedName=MemoryBoost"
    if !randomIndex! equ 33 set "selectedName=CloudSyncer"
    if !randomIndex! equ 34 set "selectedName=DataWiper"
    if !randomIndex! equ 35 set "selectedName=SystemRebuilder"
    if !randomIndex! equ 36 set "selectedName=FileShredder"
    if !randomIndex! equ 37 set "selectedName=SpeedEnhancer"
    if !randomIndex! equ 38 set "selectedName=DiskDefragPro"
    if !randomIndex! equ 39 set "selectedName=PrivacyProtector"
    if !randomIndex! equ 40 set "selectedName=AutoFixer"
    if !randomIndex! equ 41 set "selectedName=RegistryCleaner"
    if !randomIndex! equ 42 set "selectedName=QuickDefender"
    if !randomIndex! equ 43 set "selectedName=SafeConnect"
    if !randomIndex! equ 44 set "selectedName=PrivacyGuard"
    if !randomIndex! equ 45 set "selectedName=WebProtect"
    if !randomIndex! equ 46 set "selectedName=FirewallPlus"
    if !randomIndex! equ 47 set "selectedName=SystemGuard"
    if !randomIndex! equ 48 set "selectedName=CryptGuard"
    if !randomIndex! equ 49 set "selectedName=PhishShield"
    if !randomIndex! equ 50 set "selectedName=SafeLogin"
    if !randomIndex! equ 51 set "selectedName=ClearCache"
    if !randomIndex! equ 52 set "selectedName=Lockdown"
    
    echo Debug: randomIndex is !randomIndex!
    echo Debug: selected name is !selectedName!
    
    rem Check if the executable already exists and delete it before renaming
    set "existingExe=release\!selectedName!.exe"
    
    if exist "!existingExe!" (
        echo Deleting existing !selectedName!.exe...
        del "!existingExe!"
    )
    
    if defined selectedName (
        set "newName=!selectedName!.exe"
        echo Debug: newName is !newName!
        
        if exist "release\lain.exe" (
            echo.
            echo Renaming lain.exe to !newName!...
            move "release\lain.exe" "release\!newName!" >nul 2>&1
            if exist "release\!newName!" (
                if not exist "release\lain.exe" (
                    echo Successfully renamed lain.exe to !newName!.
                ) else (
                    echo Warning: lain.exe still exists after rename attempt.
                )
            ) else (
                echo Failed to rename lain.exe.
            )
        ) else (
            echo Error: lain.exe does not exist in the release directory.
        )
    ) else (
        echo Error: Invalid random selection, selectedName is not defined.
    )
    exit
)
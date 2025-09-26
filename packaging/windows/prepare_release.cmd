echo %KLOGG_QT%
echo %KLOGG_QT_DIR%

md %KLOGG_WORKSPACE%\release

echo "Copying klogg binaries..."
xcopy %KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%\output\klogg_portable.exe %KLOGG_WORKSPACE%\release\ /y
xcopy %KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%\output\klogg_portable.pdb %KLOGG_WORKSPACE%\release\ /y
xcopy %KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%\output\klogg.exe %KLOGG_WORKSPACE%\release\ /y
xcopy %KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%\output\klogg.pdb %KLOGG_WORKSPACE%\release\ /y

xcopy %KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%\output\klogg_crashpad_handler.exe %KLOGG_WORKSPACE%\release\ /y
xcopy %KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%\output\klogg_minidump_dump.exe %KLOGG_WORKSPACE%\release\ /y

set "TBB_FOUND="
pushd "%KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%" 2>nul
if errorlevel 1 (
    echo "Warning: could not locate build root '%KLOGG_BUILD_ROOT%' when looking for TBB runtime files"
) else (
    for /d %%D in (msvc_*_relwithdebinfo) do (
        if exist "%%~fD\tbb12.dll" (
            echo "Copying TBB runtime from %%~fD"
            xcopy "%%~fD\tbb12.dll" %KLOGG_WORKSPACE%\release\ /y >nul
            set "TBB_FOUND=1"
        )
        if exist "%%~fD\tbb12.pdb" (
            xcopy "%%~fD\tbb12.pdb" %KLOGG_WORKSPACE%\release\ /y >nul
            set "TBB_FOUND=1"
        )
    )
    popd
)
if not defined TBB_FOUND (
    echo "Warning: no TBB runtime files were copied"
)

xcopy %KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%\generated\documentation.html %KLOGG_WORKSPACE%\release\ /y
xcopy %KLOGG_WORKSPACE%\COPYING %KLOGG_WORKSPACE%\release\ /y
xcopy %KLOGG_WORKSPACE%\NOTICE %KLOGG_WORKSPACE%\release\ /y
xcopy %KLOGG_WORKSPACE%\README.md %KLOGG_WORKSPACE%\release\ /y
xcopy %KLOGG_WORKSPACE%\DOCUMENTATION.md %KLOGG_WORKSPACE%\release\ /y

echo "Copying vc runtime..."
xcopy "%VCToolsRedistDir%%platform%\Microsoft.VC143.CRT\msvcp140.dll" %KLOGG_WORKSPACE%\release\ /y
xcopy "%VCToolsRedistDir%%platform%\Microsoft.VC143.CRT\msvcp140_1.dll" %KLOGG_WORKSPACE%\release\ /y
xcopy "%VCToolsRedistDir%%platform%\Microsoft.VC143.CRT\msvcp140_2.dll" %KLOGG_WORKSPACE%\release\ /y
xcopy "%VCToolsRedistDir%%platform%\Microsoft.VC143.CRT\vcruntime140.dll" %KLOGG_WORKSPACE%\release\ /y
xcopy "%VCToolsRedistDir%%platform%\Microsoft.VC143.CRT\vcruntime140_1.dll" %KLOGG_WORKSPACE%\release\ /y

echo "Copying ssl..."
xcopy %SSL_DIR%\libcrypto-1_1%SSL_ARCH%.dll %KLOGG_WORKSPACE%\release\ /y
xcopy %SSL_DIR%\libssl-1_1%SSL_ARCH%.dll %KLOGG_WORKSPACE%\release\ /y

echo "Copying Qt..."
set "QTDIR=%KLOGG_QT_DIR:/=\%"
echo %QTDIR%
xcopy %QTDIR%\bin\%KLOGG_QT%Core.dll %KLOGG_WORKSPACE%\release\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Gui.dll %KLOGG_WORKSPACE%\release\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Network.dll %KLOGG_WORKSPACE%\release\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Widgets.dll %KLOGG_WORKSPACE%\release\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Concurrent.dll %KLOGG_WORKSPACE%\release\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Xml.dll %KLOGG_WORKSPACE%\release\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Core5Compat.dll %KLOGG_WORKSPACE%\release\ /y

echo "Staging Total Commander lister plugin runtime..."
set "KLOGG_TOTALCMD_ROOT=%KLOGG_WORKSPACE%\release\totalcmd"
md %KLOGG_TOTALCMD_ROOT%
md %KLOGG_TOTALCMD_ROOT%\platforms
md %KLOGG_TOTALCMD_ROOT%\styles

copy /y "%KLOGG_WORKSPACE%\%KLOGG_BUILD_ROOT%\output\klogg_lister.dll" "%KLOGG_TOTALCMD_ROOT%\klogg_lister.wlx" >nul

xcopy %QTDIR%\bin\%KLOGG_QT%Core.dll %KLOGG_TOTALCMD_ROOT%\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Gui.dll %KLOGG_TOTALCMD_ROOT%\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Network.dll %KLOGG_TOTALCMD_ROOT%\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Widgets.dll %KLOGG_TOTALCMD_ROOT%\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Concurrent.dll %KLOGG_TOTALCMD_ROOT%\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Xml.dll %KLOGG_TOTALCMD_ROOT%\ /y
xcopy %QTDIR%\bin\%KLOGG_QT%Core5Compat.dll %KLOGG_TOTALCMD_ROOT%\ /y

xcopy %QTDIR%\plugins\platforms\qwindows.dll %KLOGG_TOTALCMD_ROOT%\platforms\ /y
if exist %QTDIR%\plugins\styles\qwindowsvistastyle.dll (
    xcopy %QTDIR%\plugins\styles\qwindowsvistastyle.dll %KLOGG_TOTALCMD_ROOT%\styles\ /y
) else (
    echo "Warning: %QTDIR%\plugins\styles\qwindowsvistastyle.dll not found for lister plugin"
)
if exist %QTDIR%\plugins\styles\qmodernwindowsstyle.dll (
    xcopy %QTDIR%\plugins\styles\qmodernwindowsstyle.dll %KLOGG_TOTALCMD_ROOT%\styles\ /y
) else (
    echo "Warning: %QTDIR%\plugins\styles\qmodernwindowsstyle.dll not found for lister plugin"
)

copy /y "%KLOGG_WORKSPACE%\docs\total_commander_lister.md" "%KLOGG_TOTALCMD_ROOT%\README.md" >nul
xcopy %KLOGG_WORKSPACE%\COPYING %KLOGG_TOTALCMD_ROOT%\ /y
xcopy %KLOGG_WORKSPACE%\NOTICE %KLOGG_TOTALCMD_ROOT%\ /y

echo "Writing Total Commander auto-install manifest..."
for %%F in ("%KLOGG_TOTALCMD_ROOT%\pluginst.inf") do del "%%~fF" 2>nul
powershell -NoLogo -NoProfile -Command ^
  "$pluginRoot = Join-Path $env:KLOGG_WORKSPACE 'release/totalcmd';" ^
  "$qtPrefix = $env:KLOGG_QT;" ^
  "$files = @('klogg_lister.wlx', \"${qtPrefix}Core.dll\", \"${qtPrefix}Gui.dll\", \"${qtPrefix}Network.dll\", \"${qtPrefix}Widgets.dll\", \"${qtPrefix}Concurrent.dll\", \"${qtPrefix}Xml.dll\", \"${qtPrefix}Core5Compat.dll\", 'platforms\\qwindows.dll');" ^
  "if (Test-Path (Join-Path $pluginRoot 'styles/qwindowsvistastyle.dll')) { $files += 'styles\\qwindowsvistastyle.dll' }" ^
  "if (Test-Path (Join-Path $pluginRoot 'styles/qmodernwindowsstyle.dll')) { $files += 'styles\\qmodernwindowsstyle.dll' }" ^
  "$files += 'README.md', 'COPYING', 'NOTICE';" ^
  "$content = @('[plugininstall]', 'type=wlx', 'description=Klogg Lister Plugin ' + $env:KLOGG_VERSION + ' (' + $env:KLOGG_ARCH + ')', 'targetdir=%COMMANDER_PATH%\\plugins\\wlx\\klogg_lister', '', '[source]') + $files;" ^
  "Set-Content -Path (Join-Path $pluginRoot 'pluginst.inf') -Value $content -Encoding Ascii"

md %KLOGG_WORKSPACE%\release\platforms
xcopy %QTDIR%\plugins\platforms\qwindows.dll %KLOGG_WORKSPACE%\release\platforms\ /y

md %KLOGG_WORKSPACE%\release\styles
if exist %QTDIR%\plugins\styles\qwindowsvistastyle.dll (
    xcopy %QTDIR%\plugins\styles\qwindowsvistastyle.dll %KLOGG_WORKSPACE%\release\styles /y
) else (
    echo "Warning: %QTDIR%\plugins\styles\qwindowsvistastyle.dll not found for main runtime"
)
if exist %QTDIR%\plugins\styles\qmodernwindowsstyle.dll (
    xcopy %QTDIR%\plugins\styles\qmodernwindowsstyle.dll %KLOGG_WORKSPACE%\release\styles /y
) else (
    echo "Warning: %QTDIR%\plugins\styles\qmodernwindowsstyle.dll not found for main runtime"
)

echo "Copying packaging files..."
md %KLOGG_WORKSPACE%\chocolatey
xcopy %KLOGG_WORKSPACE%\packaging\windows\chocolatey\klogg.nuspec %KLOGG_WORKSPACE%\chocolatey\ /y

md %KLOGG_WORKSPACE%\chocolatey\tools
xcopy %KLOGG_WORKSPACE%\packaging\windows\chocolatey\tools\chocolateyInstall.ps1 %KLOGG_WORKSPACE%\chocolatey\tools\ /y

xcopy %KLOGG_WORKSPACE%\packaging\windows\klogg.nsi  /y
xcopy %KLOGG_WORKSPACE%\packaging\windows\FileAssociation.nsh  /y

echo "Making portable archive..."
7z a -r %KLOGG_WORKSPACE%\klogg-%KLOGG_VERSION%-%KLOGG_ARCH%-%KLOGG_QT%-portable.zip @%KLOGG_WORKSPACE%\packaging\windows\7z_klogg_listfile.txt
set "TBB_PDB_ARGS="
if exist %KLOGG_WORKSPACE%\release\tbb12.dll set "TBB_PDB_ARGS=%TBB_PDB_ARGS% .\release\tbb12.dll"
if exist %KLOGG_WORKSPACE%\release\tbb12.pdb set "TBB_PDB_ARGS=%TBB_PDB_ARGS% .\release\tbb12.pdb"
7z a %KLOGG_WORKSPACE%\klogg-%KLOGG_VERSION%-%KLOGG_ARCH%-%KLOGG_QT%-pdb.zip .\release\klogg.exe .\release\klogg.pdb .\release\klogg_portable.exe .\release\klogg_portable.pdb%TBB_PDB_ARGS%
pushd "%KLOGG_TOTALCMD_ROOT%"
7z a -tzip %KLOGG_WORKSPACE%\klogg-totalcmd-lister-%KLOGG_VERSION%-%KLOGG_ARCH%-%KLOGG_QT%.zip *
popd

echo "Done!"

# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory=$true)][string]$DepsRoot,
    [Parameter(Mandatory=$true)][string]$VcRoot,
    [Parameter(Mandatory=$true)][string]$SdkRoot,
    [string]$VcIncludeRoot = '',
    [string]$OutputDirectory = '',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$DepsRoot = (Resolve-Path -LiteralPath $DepsRoot).Path
$VcRoot = (Resolve-Path -LiteralPath $VcRoot).Path
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
if (!$VcIncludeRoot) { $VcIncludeRoot = $VcRoot }
$VcIncludeRoot = (Resolve-Path -LiteralPath $VcIncludeRoot).Path
if (!$OutputDirectory) { $OutputDirectory = Join-Path $PSScriptRoot 'build' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$compilerDirectory = Join-Path $VcRoot 'bin\amd64'
if (!(Test-Path -LiteralPath (Join-Path $compilerDirectory 'cl.exe'))) {
    $compilerDirectory = Join-Path $VcRoot 'bin\x86_amd64'
}
$compiler = Join-Path $compilerDirectory 'cl.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'VC100 x64 cl.exe not found under VcRoot.' }
if ((Get-Item -LiteralPath $compiler).VersionInfo.FileMajorPart -ne 16) {
    throw 'KenshiLib requires the Visual C++ 2010 compiler (version 16.x).'
}
foreach ($required in @(
    (Join-Path $VcIncludeRoot 'include\vector'),
    (Join-Path $VcRoot 'lib\amd64\msvcrt.lib'),
    (Join-Path $SdkRoot 'Include\Windows.h'),
    (Join-Path $SdkRoot 'Lib\x64\Kernel32.lib'),
    (Join-Path $DepsRoot 'KenshiLib\Include\Debug.h'),
    (Join-Path $DepsRoot 'KenshiLib\Libraries\KenshiLib.lib'),
    (Join-Path $DepsRoot 'boost_1_60_0\boost\version.hpp')
)) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing dependency: $required" }
}
$oldPath = $env:PATH
$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
try {
    $env:PATH = $compilerDirectory + ';' + (Join-Path $VcRoot 'bin') + ';' + $oldPath
    $env:INCLUDE = (Join-Path $VcIncludeRoot 'include') + ';' + (Join-Path $SdkRoot 'Include') + ';' +
        (Join-Path $DepsRoot 'KenshiLib\Include') + ';' + (Join-Path $DepsRoot 'boost_1_60_0')
    $env:LIB = (Join-Path $VcRoot 'lib\amd64') + ';' + (Join-Path $SdkRoot 'Lib\x64') + ';' +
        (Join-Path $DepsRoot 'KenshiLib\Libraries') + ';' + (Join-Path $DepsRoot 'boost_1_60_0\stage\lib')
    $targetNames = if ($Test) { @('RaceSectionsTests', 'InputEventsTests', 'TooltipControlGeometryTests', 'PageNavigationTests') } else { @('RaceListCollapse') }
    foreach ($targetName in $targetNames) {
        $extension = if ($Test) { '.exe' } else { '.dll' }
        $target = Join-Path $OutputDirectory ($targetName + $extension)
        $objects = Join-Path $OutputDirectory ($targetName + '-objects')
        New-Item -ItemType Directory -Force -Path $OutputDirectory,$objects | Out-Null
        $arguments = @('/nologo','/MD','/EHsc','/O2','/GL','/W3','/DNDEBUG','/DWIN32',
            '/D_WINDOWS','/DUNICODE','/D_UNICODE','/D_ITERATOR_DEBUG_LEVEL=0')
        if (!$Test) { $arguments += '/LD' }
        $arguments += '/Fo' + $objects + '\'
        $arguments += '/Fd' + (Join-Path $objects 'compiler.pdb')
        $arguments += '/Fe' + $target
        if ($Test) { $arguments += Join-Path $PSScriptRoot ('source\tests\' + $targetName + '.cpp') }
        else { $arguments += Join-Path $PSScriptRoot 'source\RaceListCollapse.cpp' }
        $arguments += @('/link','/LTCG','/MACHINE:X64','/INCREMENTAL:NO','/OPT:REF','/OPT:ICF','/MANIFEST:NO')
        if (!$Test) { $arguments += @('KenshiLib.lib','MyGUIEngine_x64.lib','OgreMain_x64.lib','user32.lib') }
        elseif ($targetName -eq 'InputEventsTests') { $arguments += 'user32.lib' }
        & $compiler @arguments
        if ($LASTEXITCODE -ne 0) { throw "$targetName build failed: $LASTEXITCODE" }
        if ($Test) {
            & $target
            if ($LASTEXITCODE -ne 0) { throw "$targetName failed: $LASTEXITCODE" }
        }
        Write-Output "Built: $target"
    }
} finally {
    $env:PATH = $oldPath
    $env:INCLUDE = $oldInclude
    $env:LIB = $oldLib
}

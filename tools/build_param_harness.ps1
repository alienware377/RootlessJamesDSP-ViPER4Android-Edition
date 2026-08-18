# Builds and runs tools/param_harness.cpp.
#
# Unlike the effect harnesses, this one links the WHOLE engine - applyParam
# touches every subsystem - so it compiles all of jdsp/ to objects first. The
# sources are C and the harness is C++, so they cannot go through one driver.
#
#   .\tools\build_param_harness.ps1                 # arm64, run on a tablet
#   .\tools\build_param_harness.ps1 -Abi x86_64     # run on an emulator
#
# The engine predates some of clang's newer errors-by-default, hence the
# -Wno-error flags; the app's own CMake is equally permissive.
param(
    [ValidateSet('arm64-v8a', 'x86_64')]
    [string]$Abi = 'arm64-v8a',
    [string]$Device = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ndk = Get-ChildItem "$env:LOCALAPPDATA\Android\Sdk\ndk" -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
$bin = "$($ndk.FullName)\toolchains\llvm\prebuilt\windows-x86_64\bin"
$triple = if ($Abi -eq 'x86_64') { 'x86_64-linux-android29' } else { 'aarch64-linux-android29' }
$cc  = "$bin\$triple-clang.cmd"
$cxx = "$bin\$triple-clang++.cmd"

$jdsp = "$root\app\src\main\cpp\libjamesdsp\Main\libjamesdsp\jni\jamesdsp\jdsp"
$objs = "$root\tools\objs_$Abi"
New-Item -ItemType Directory -Force $objs | Out-Null

$sources = Get-ChildItem $jdsp -Recurse -Filter *.c |
    ForEach-Object { $_.FullName -replace '\\', '/' }
$flags = @(
    '-O1', '-w', '-c', '-fPIC',
    '-Wno-error=implicit-function-declaration',
    '-Wno-error=incompatible-function-pointer-types',
    '-Wno-error=implicit-int',
    '-Wno-error=int-conversion',
    '-I', ($jdsp -replace '\\', '/')
)
# A response file: the source list is far past the Windows command-line limit.
Set-Content "$root\tools\cc.rsp" ($flags + $sources) -Encoding ascii
Push-Location $objs
& $cc "@$($root -replace '\\','/')/tools/cc.rsp"
Pop-Location

$out = "$root/tools/param_harness_$Abi" -replace '\\', '/'
$objList = Get-ChildItem $objs -Filter *.o | ForEach-Object { $_.FullName -replace '\\', '/' }
Set-Content "$root\tools\link.rsp" (
    @('-O1', '-w', '-std=c++17', '-static-libstdc++',
      '-I', ($jdsp -replace '\\', '/'), '-o', $out,
      ("$root/tools/param_harness.cpp" -replace '\\', '/')) + $objList + @('-lm', '-llog')
) -Encoding ascii
& $cxx "@$($root -replace '\\','/')/tools/link.rsp"

$target = if ($Device) { @('-s', $Device) } else { @() }
& adb @target push $out /data/local/tmp/param_harness | Out-Null
& adb @target shell 'chmod 755 /data/local/tmp/param_harness; /data/local/tmp/param_harness'

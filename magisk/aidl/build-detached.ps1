# Runs the AIDL service build detached, writing to a log.
#
# Why not just run it inline: the Claude Desktop bridge abandons a tool call
# after about four minutes regardless of the timeout requested, so any build
# longer than that comes back as "no result received" even though it is still
# running fine on the machine. Detaching turns a long build into a short call
# plus polling, which stays well inside that ceiling.
#
#   powershell -File build-detached.ps1          # start it
#   Get-Content $env:TEMP\aidlsvc\build.log -Tail 20   # check on it
param([string]$Abi = "arm64-v8a")

$ndk   = "$env:LOCALAPPDATA\Android\Sdk\ndk\27.0.12077973"
$cmake = "$env:LOCALAPPDATA\Android\Sdk\cmake\3.22.1\bin\cmake.exe"
$ninja = "$env:LOCALAPPDATA\Android\Sdk\cmake\3.22.1\bin\ninja.exe"
$src   = "$PSScriptRoot"
$bd    = "$env:TEMP\aidlsvc"
$log   = "$bd\build.log"

Remove-Item $bd -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $bd | Out-Null

$script = @"
& '$cmake' -S '$src' -B '$bd' -G Ninja ``
    '-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake' ``
    '-DANDROID_ABI=$Abi' '-DANDROID_PLATFORM=android-33' ``
    '-DCMAKE_MAKE_PROGRAM=$ninja' *>&1 | Out-File -Encoding utf8 -FilePath '$log'
& '$cmake' --build '$bd' *>&1 | Out-File -Encoding utf8 -FilePath '$log' -Append
'BUILD_FINISHED' | Out-File -FilePath '$log' -Append
"@

Start-Process powershell -ArgumentList "-NoProfile","-Command",$script -WindowStyle Hidden
Write-Output "started; log at $log"

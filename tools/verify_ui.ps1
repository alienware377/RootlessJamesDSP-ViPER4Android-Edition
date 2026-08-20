# Drive the app on a real device and confirm the cards added since v2.24 actually
# render, open, and respond. Eight things have never been seen on a screen:
# six cards, plus two controls that live inside older cards.
#
#   powershell -File tools/verify_ui.ps1                 # auto-detect device
#   powershell -File tools/verify_ui.ps1 -Serial 1.2.3.4:5555 -SkipInstall
#
# This exists because the tablet is usually locked, and when it is finally
# unlocked that window may be short. Working out the tap coordinates by hand at
# that point wastes the window, so it is all worked out in advance here.
#
# Nothing here touches the lockscreen. If the device is locked the script says so
# and stops - failed unlock attempts make a Samsung device harder to get into,
# not easier, and the owner is the only one who should be entering that
# credential.
#
# Everything is located by its on-screen text and scrolled to, never by fixed
# coordinates, because the owner arranges their cards into custom groups and the
# order is theirs to change.

param(
    [string]$Serial,
    [switch]$SkipInstall,
    [string]$Package = "me.timschneeberger.rootlessjamesdsp.v4a"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$results = @()

function Add-Result($area, $what, $ok, $note) {
    $script:results += [pscustomobject]@{ Area = $area; Check = $what; Ok = $ok; Note = $note }
    $tag = if ($ok) { "ok  " } else { "FAIL" }
    Write-Host ("  [{0}] {1,-34} {2}" -f $tag, $what, $note)
}

# Resolved once, to the executable rather than by name. PowerShell matches
# command names without regard to case, so a function called Adb that calls adb
# calls itself, and the only symptom is "call depth overflow".
$adbExe = (Get-Command adb -CommandType Application | Select-Object -First 1).Source
if (-not $adbExe) { Write-Host "adb is not on PATH." -ForegroundColor Yellow; exit 3 }

function Adb { param([Parameter(ValueFromRemainingArguments)]$a) & $adbExe -s $Serial @a }

# ---- device ---------------------------------------------------------------

if (-not $Serial) {
    $line = (& $adbExe devices) | Select-String '\sdevice$' | Select-Object -First 1
    if (-not $line) { Write-Host "No device. Try: adb mdns services" -ForegroundColor Yellow; exit 3 }
    $Serial = ($line -split '\s+')[0]
}
Write-Host "Device: $Serial"

# 2>/dev/null because grep -m1 closes the pipe as soon as it has its match and
# dumpsys complains about the broken pipe on the way out.
$locked = (Adb shell "dumpsys window 2>/dev/null | grep -m1 mDreamingLockscreen") -match 'mDreamingLockscreen=true'
if ($locked) {
    Write-Host ""
    Write-Host "Device is LOCKED. Unlock it by hand and run this again." -ForegroundColor Yellow
    Write-Host "Not attempting the credential - that is the owner's to enter." -ForegroundColor Yellow
    exit 2
}

# ---- install --------------------------------------------------------------

if (-not $SkipInstall) {
    $apk = Get-ChildItem "$repo\app\build\outputs\apk\rootlessFull" -Recurse -Filter *.apk -EA SilentlyContinue |
           Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($apk) {
        Write-Host "Installing $($apk.Name)"
        $out = Adb install -r -d $apk.FullName 2>&1
        if ($out -match 'Success') { Write-Host "  installed" }
        else { Write-Host "  install failed: $out" -ForegroundColor Yellow }
    }
    else { Write-Host "No rootlessFull APK built; using what is on the device." }
}

# ---- ui helpers -----------------------------------------------------------

$dumpPath = "$env:TEMP\rv4a_ui.xml"

function Get-Ui {
    for ($try = 0; $try -lt 3; $try++) {
        $r = Adb shell "uiautomator dump /sdcard/rv4a_ui.xml" 2>&1
        if ($r -match 'dumped') {
            Adb pull /sdcard/rv4a_ui.xml $dumpPath 2>&1 | Out-Null
            try { return [xml](Get-Content $dumpPath -Raw) } catch {}
        }
        Start-Sleep -Milliseconds 700
    }
    return $null
}

# Every node whose text or content-desc contains this fragment.
function Find-Nodes($ui, $text) {
    if (-not $ui) { return @() }
    $ui.SelectNodes("//node") | Where-Object {
        ($_.text -and $_.text -like "*$text*") -or
        ($_.'content-desc' -and $_.'content-desc' -like "*$text*")
    }
}

function Get-Center($node) {
    if ($node.bounds -match '\[(\d+),(\d+)\]\[(\d+),(\d+)\]') {
        return @{ X = [int](([int]$Matches[1] + [int]$Matches[3]) / 2)
                  Y = [int](([int]$Matches[2] + [int]$Matches[4]) / 2)
                  Top = [int]$Matches[2]; Bottom = [int]$Matches[4] }
    }
    return $null
}

function Tap($node) {
    $c = Get-Center $node
    if (-not $c) { return $false }
    Adb shell "input tap $($c.X) $($c.Y)" | Out-Null
    Start-Sleep -Milliseconds 900
    return $true
}

# Scroll the list looking for some text. Scrolls back to the top first so the
# search is repeatable whatever the previous check left on screen.
# A signature of what is currently on screen, used to tell "the list moved" from
# "the list is already at the end".
function Get-UiSignature($ui) {
    if (-not $ui) { return "" }
    (($ui.SelectNodes("//node") | ForEach-Object { $_.text }) -join '|')
}

function Scroll-To($text, [int]$maxSwipes = 20) {
    # Last line wins: a device with a display override reports both a Physical
    # and an Override size, and the override is the one input coordinates use.
    $sizeLine = (Adb shell wm size) | Where-Object { $_ -match '(\d+)x(\d+)' } | Select-Object -Last 1
    if ($sizeLine -notmatch '(\d+)x(\d+)') { return $null }
    $w = [int]$Matches[1]; $h = [int]$Matches[2]
    $cx = [int]($w / 2)
    $top = [int]($h * 0.30); $bottom = [int]($h * 0.82)

    # Back to the top, detected rather than counted - a fixed number of swipes
    # is either too few on a long list or a waste of seconds on a short one,
    # and this runs eight times.
    $prev = ""
    for ($i = 0; $i -lt 25; $i++) {
        Adb shell "input swipe $cx $top $cx $bottom 60" | Out-Null
        Start-Sleep -Milliseconds 250
        $sig = Get-UiSignature (Get-Ui)
        if ($sig -eq $prev) { break }
        $prev = $sig
    }

    $prev = ""
    for ($i = 0; $i -le $maxSwipes; $i++) {
        $ui = Get-Ui
        $hit = Find-Nodes $ui $text | Select-Object -First 1
        if ($hit) { return @{ Ui = $ui; Node = $hit } }
        $sig = Get-UiSignature $ui
        if ($sig -eq $prev) { break }   # bottom reached, it is not here
        $prev = $sig
        Adb shell "input swipe $cx $bottom $cx $top 120" | Out-Null
        Start-Sleep -Milliseconds 450
    }
    return $null
}

# ---- launch, and read any crash the last run left behind ------------------

Write-Host "`nLaunching"
Adb shell "am force-stop $Package" | Out-Null
Adb shell "monkey -p $Package -c android.intent.category.LAUNCHER 1" 2>&1 | Out-Null
Start-Sleep -Seconds 5

Write-Host "`ncrash from the previous run"
$ui = Get-Ui
$crash = Find-Nodes $ui "crash" | Select-Object -First 1
if ($crash) {
    $detail = (Find-Nodes $ui "Exception" | Select-Object -First 1)
    Add-Result "crash" "no crash dialog on launch" $false ("dialog present: " + $crash.text)
    if ($detail) { Write-Host "      $($detail.text)" -ForegroundColor Red }
    # Dismiss so the rest of the run can proceed.
    $ok = Find-Nodes $ui "OK" | Select-Object -First 1
    if ($ok) { Tap $ok | Out-Null }
}
else { Add-Result "crash" "no crash dialog on launch" $true "clean" }

# ---- the six cards --------------------------------------------------------
#
# Each card is checked three ways: that its title is on screen at all, that
# tapping it reveals the controls it is supposed to contain, and that one of
# those controls responds to being dragged. A card that renders but whose
# sliders do nothing is the failure mode worth catching - it is what a missing
# parameter id in EffectParams.h looks like from the outside.

$cards = @(
    @{ Title = "Dynamic EQ";                Expect = @("Works on", "Band 1") }
    @{ Title = "Stereo imaging (per band)"; Expect = @("Mono below", "Low width") }
    @{ Title = "Impact (attack";            Expect = @("Attack", "Sustain") }
    @{ Title = "Low end";                   Expect = @("Subsonic", "Weight") }
    @{ Title = "Exciter";                   Expect = @("Character", "Drive") }
    @{ Title = "Vintage tape";              Expect = @("Wow", "Flutter", "Saturation") }
)

foreach ($card in $cards) {
    Write-Host "`n$($card.Title)"
    $found = Scroll-To $card.Title
    if (-not $found) {
        Add-Result $card.Title "card renders" $false "title never appeared while scrolling"
        continue
    }
    Add-Result $card.Title "card renders" $true "found"

    Tap $found.Node | Out-Null
    $ui = Get-Ui
    $missing = @($card.Expect | Where-Object { -not (Find-Nodes $ui $_) })
    Add-Result $card.Title "controls present" ($missing.Count -eq 0) `
        $(if ($missing.Count) { "missing: " + ($missing -join ', ') } else { "all " + $card.Expect.Count })

    # Drag the first slider and see whether its readout changes.
    $slider = $ui.SelectNodes("//node") | Where-Object { $_.class -match 'SeekBar|Slider' } | Select-Object -First 1
    if ($slider) {
        $c = Get-Center $slider
        $before = Get-UiSignature $ui
        Adb shell "input swipe $($c.X) $($c.Y) $([int]($c.X * 1.25)) $($c.Y) 300" | Out-Null
        Start-Sleep -Milliseconds 800
        $after = Get-UiSignature (Get-Ui)
        Add-Result $card.Title "slider moves" ($before -ne $after) `
            $(if ($before -ne $after) { "readout changed" } else { "nothing changed" })
    }
    else { Add-Result $card.Title "slider moves" $false "no slider found in the card" }
}

# ---- the two controls inside older cards ----------------------------------
#
# Both are dropdowns, so the check is that the list opens and offers exactly the
# options it is meant to. A ListPreference whose entries and entryValues arrays
# are different lengths looks fine until it is opened.

$lists = @(
    @{ Card = "Maximizer"; Control = "Clip shape"
       Options = @("Smooth", "Classic", "Hard") }
    @{ Card = "Dynamic EQ"; Control = "Works on"
       Options = @("Whole stereo image", "Centre only", "Edges only") }
)

foreach ($l in $lists) {
    Write-Host "`n$($l.Card) -> $($l.Control)"
    $found = Scroll-To $l.Card
    if (-not $found) { Add-Result $l.Control "parent card found" $false "no $($l.Card)"; continue }
    Tap $found.Node | Out-Null

    $found2 = Scroll-To $l.Control
    if (-not $found2) { Add-Result $l.Control "control renders" $false "not inside the card"; continue }
    Add-Result $l.Control "control renders" $true "found"

    Tap $found2.Node | Out-Null
    $ui = Get-Ui
    $missing = @($l.Options | Where-Object { -not (Find-Nodes $ui $_) })
    Add-Result $l.Control "all options offered" ($missing.Count -eq 0) `
        $(if ($missing.Count) { "missing: " + ($missing -join ', ') } else { "$($l.Options.Count) options" })

    # Choose the last one, which is the one furthest from the default, then
    # confirm the card's summary caught up.
    $pick = Find-Nodes $ui $l.Options[-1] | Select-Object -First 1
    if ($pick) {
        Tap $pick | Out-Null
        $ui = Get-Ui
        $echo = Find-Nodes $ui $l.Options[-1] | Select-Object -First 1
        Add-Result $l.Control "selection sticks" ($null -ne $echo) `
            $(if ($echo) { "summary shows it" } else { "summary did not update" })
    }
    Adb shell "input keyevent KEYCODE_BACK" | Out-Null
    Start-Sleep -Milliseconds 500
}

# ---- did any of that crash it? --------------------------------------------

Write-Host "`nrelaunch"
Adb shell "am force-stop $Package" | Out-Null
Start-Sleep -Seconds 2
Adb shell "monkey -p $Package -c android.intent.category.LAUNCHER 1" 2>&1 | Out-Null
Start-Sleep -Seconds 5
$ui = Get-Ui
$crash = Find-Nodes $ui "crash" | Select-Object -First 1
Add-Result "crash" "no crash after driving every card" (-not $crash) `
    $(if ($crash) { $crash.text } else { "clean" })

# ---- report ---------------------------------------------------------------

Write-Host "`n---- summary ----"
$results | Format-Table -AutoSize | Out-String | Write-Host
$bad = @($results | Where-Object { -not $_.Ok })
if ($bad.Count) {
    Write-Host "$($bad.Count) of $($results.Count) checks failed." -ForegroundColor Red
    exit 1
}
Write-Host "All $($results.Count) checks passed." -ForegroundColor Green
exit 0

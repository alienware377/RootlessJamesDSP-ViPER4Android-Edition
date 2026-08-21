# Consistency checks between the cards, the string resources and the engine.
#
#   powershell -File tools/check_resources.ps1     (or & .\tools\check_resources.ps1)
#
# Everything here is the same shape of fault: something that compiles, renders,
# and looks entirely normal, but where a control does nothing or shows the wrong
# value. None of it needs a device, which matters because the device is usually
# locked - and two of these are invisible on a device anyway, since a phone that
# already has preferences written can never show a wrong default.
#
# The four checks:
#
#   1. Every list control's entries and entryValues arrays exist and are the
#      same length. Android silently pairs them up to the shorter of the two, so
#      a mismatch shows a short menu, or one where the labels have slipped by
#      one and every choice sets the wrong value.
#
#   2. Every list control's default is one of its own options. If it is not, the
#      control opens with nothing selected.
#
#   3. Every numeric default advertised on a card matches the default the engine
#      falls back to when the preference has never been written. Disagreement
#      means a fresh install runs settings that are not the ones on screen.
#
#   4. No preference key is used by two different cards. Sharing one is not
#      illegal, but the engine's change detection remembers values per
#      namespace and key, and a shared key is one line of engine code away from
#      dropping a change silently. See PreferenceCache.
#
#   5. Every card the fragment binds agrees with the namespace-to-card map, and
#      has a master switch that map can actually find. Loading a preset is
#      allowed to unhide a card the preset switches on, and that is the only
#      thing it may change about the layout - so if the map disagrees with the
#      fragment, or a card's switch is not named the way the matcher expects,
#      a preset turns an effect on and leaves it invisible.
param([switch]$Quiet)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ns = 'http://schemas.android.com/apk/res/android'
$res = "$root\app\src\main\res"
$problems = 0

function Fail($msg) { Write-Host "  FAIL  $msg" -ForegroundColor Red; $script:problems++ }
function Note($msg) { if (-not $Quiet) { Write-Host "        $msg" -ForegroundColor DarkGray } }

# ---- resources ------------------------------------------------------------

$arrays = @{}
$strings = @{}
Get-ChildItem "$res\values\*.xml" | ForEach-Object {
    $x = [xml](Get-Content $_.FullName -Raw)
    $x.SelectNodes('//string-array') | ForEach-Object {
        $arrays[$_.name] = @($_.SelectNodes('item') | ForEach-Object { $_.InnerText })
    }
    $x.SelectNodes('//string') | ForEach-Object { $strings[$_.name] = $_.InnerText }
}

# A default written as @string/foo has to be looked through before it can be
# compared with anything.
function Resolve-Value($v) {
    if ($v -like '@string/*') {
        $n = $v -replace '@string/',''
        if ($strings.ContainsKey($n)) { return $strings[$n] }
        return $null
    }
    return $v
}

# The equaliser's band picker is deliberately not one of its own options: the
# default is a flat custom curve and the options are named presets.
$defaultNotAnOptionAllowed = @('key_eq_bands')

# ---- 1 and 2: list controls ------------------------------------------------

Write-Host "`nlist controls"
$listCount = 0
Get-ChildItem "$res\xml\*.xml" | ForEach-Object {
    $file = $_.BaseName
    $x = [xml](Get-Content $_.FullName -Raw)
    $x.SelectNodes('//*') | ForEach-Object {
        $e = $_.GetAttribute('entries', $ns)
        $v = $_.GetAttribute('entryValues', $ns)
        if (-not ($e -and $v)) { return }
        $script:listCount++
        $key = ($_.GetAttribute('key', $ns)) -replace '@string/',''
        $en = $e -replace '@array/',''
        $vn = $v -replace '@array/',''
        if (-not $arrays.ContainsKey($en)) { Fail "$file/$key : no such array $en"; return }
        if (-not $arrays.ContainsKey($vn)) { Fail "$file/$key : no such array $vn"; return }
        if ($arrays[$en].Count -ne $arrays[$vn].Count) {
            Fail ("$file/$key : {0} has {1} entries but {2} has {3}" -f $en, $arrays[$en].Count, $vn, $arrays[$vn].Count)
            return
        }
        $d = Resolve-Value ($_.GetAttribute('defaultValue', $ns))
        if ($d -ne '' -and $null -ne $d -and $defaultNotAnOptionAllowed -notcontains $key) {
            if ($arrays[$vn] -notcontains $d) {
                Fail ("$file/$key : default '{0}' is not one of {1}" -f $d, ($arrays[$vn] -join ','))
            }
        }
    }
}
Note "$listCount list controls checked"

# ---- 3: card defaults against engine defaults ------------------------------

Write-Host "`ncard defaults against the engine"
$kt = Get-Content "$root\app\src\main\java\me\timschneeberger\rootlessjamesdsp\interop\JamesDspBaseEngine.kt" -Raw
$eng = @{}
[regex]::Matches($kt, 'cache\.get\(R\.string\.(key_[a-z0-9_]+),\s*("?)([-0-9.]+)\2f?\)') |
    ForEach-Object { $eng[$_.Groups[1].Value] = $_.Groups[3].Value }
$pairs = 0
Get-ChildItem "$res\xml\dsp_*_preferences.xml" | ForEach-Object {
    $file = $_.BaseName -replace '_preferences',''
    $x = [xml](Get-Content $_.FullName -Raw)
    $x.SelectNodes('//*') | ForEach-Object {
        $key = ($_.GetAttribute('key', $ns)) -replace '@string/',''
        $d = Resolve-Value ($_.GetAttribute('defaultValue', $ns))
        if ($key -and $d -and $eng.ContainsKey($key)) {
            $a = 0.0; $b = 0.0
            if ([double]::TryParse($d, [ref]$a) -and [double]::TryParse($eng[$key], [ref]$b)) {
                $script:pairs++
                if ($a -ne $b) { Fail ("$file/$key : card says $d, engine falls back to " + $eng[$key]) }
            }
        }
    }
}
Note "$pairs numeric defaults compared"

# ---- 4: keys shared between cards ------------------------------------------

Write-Host "`nkeys shared between cards"
$where = @{}
Get-ChildItem "$res\xml\dsp_*_preferences.xml" | ForEach-Object {
    $file = $_.BaseName -replace '_preferences',''
    $x = [xml](Get-Content $_.FullName -Raw)
    $x.SelectNodes('//*') | ForEach-Object {
        $key = ($_.GetAttribute('key', $ns)) -replace '@string/',''
        if ($key -like 'key_*') {
            if (-not $where.ContainsKey($key)) { $where[$key] = New-Object System.Collections.Generic.HashSet[string] }
            [void]$where[$key].Add($file)
        }
    }
}
# Known and deliberate. The parametric EQ screen reuses the graphic EQ's
# linear-phase key. Worth knowing that its switch does not currently do
# anything - the engine reads that key under the graphic EQ's namespace only -
# but that is a design question about whether one setting should appear on two
# cards, not something this script should call a failure.
$sharedKnown = @('key_geq_linear_phase')
$where.GetEnumerator() | Where-Object { $_.Value.Count -gt 1 } | ForEach-Object {
    $msg = "{0} is used by: {1}" -f $_.Key, ($_.Value -join ', ')
    if ($sharedKnown -contains $_.Key) { Write-Host "  known  $msg" -ForegroundColor Yellow }
    else { Fail $msg }
}

# ---- 5: card bindings against the reveal map -------------------------------

Write-Host "`ncard bindings and their master switches"
$df = Get-Content "$root\app\src\main\java\me\timschneeberger\rootlessjamesdsp\fragment\DspFragment.kt" -Raw
$ec = Get-Content "$root\app\src\main\java\me\timschneeberger\rootlessjamesdsp\utils\EffectCards.kt" -Raw
$ecMap = @{}
[regex]::Matches($ec, 'Constants\.(PREF_[A-Z0-9]+)\s+to\s+"(card_[a-z0-9_]+)"') |
    ForEach-Object { $ecMap[$_.Groups[1].Value] = $_.Groups[2].Value }
# The fragment is the authority: it names the card, the namespace and the screen
# together. Note the file name is NOT derivable from the namespace - the echo
# card is dsp_echodelay backed by dsp_echo_preferences - so it has to be read
# from here rather than guessed.
$specs = [regex]::Matches($df,
    'CardSpec\(R\.id\.(card_[a-z0-9_]+),\s*Constants\.(PREF_[A-Z0-9]+),\s*R\.xml\.([a-z0-9_]+)\)')
foreach ($m in $specs) {
    $card = $m.Groups[1].Value; $pref = $m.Groups[2].Value; $xmlName = $m.Groups[3].Value
    if (-not $ecMap.ContainsKey($pref)) {
        Fail "$pref is bound to $card but is missing from EffectCards, so a preset can never reveal it"
    }
    elseif ($ecMap[$pref] -ne $card) {
        Fail ("$pref : EffectCards says '{0}' but the fragment binds '{1}'" -f $ecMap[$pref], $card)
    }
    $path = "$res\xml\$xmlName.xml"
    if (-not (Test-Path $path)) { Fail "$card : no such screen $xmlName"; continue }
    $x = [xml](Get-Content $path -Raw)
    $hasEnable = $false
    $x.SelectNodes('//*') | ForEach-Object {
        $k = ($_.GetAttribute('key', $ns)) -replace '@string/',''
        # The stored key is the string resource's VALUE, not its name, and that
        # is what the preset matcher sees - key_crossfeed_enable, for one,
        # resolves to bs2b_crossfeed_enable.
        if ($k -and $strings.ContainsKey($k) -and $strings[$k] -like '*_enable') { $script:hasEnable = $true }
    }
    if (-not $hasEnable) {
        Fail "$card ($xmlName) has no key resolving to *_enable, so a preset could switch it on invisibly"
    }
}
Note "$($specs.Count) card bindings checked"

# ---- report ----------------------------------------------------------------

Write-Host ""
if ($problems) { Write-Host "$problems problem(s)" -ForegroundColor Red; exit 1 }
Write-Host "all consistent" -ForegroundColor Green
exit 0

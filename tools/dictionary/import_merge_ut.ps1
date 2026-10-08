param(
    [ValidateSet("sample", "safe", "daily", "rich", "max")]
    [string]$Profile = "sample",
    [int]$SampleLines = 5000,
    [string]$BashPath = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$WorkRoot = Join-Path $RepoRoot "dist\dictionary\merge-ut"
$OutDir = Join-Path $RepoRoot "src\data\dictionary_koyasi\generated"
$MergeRepo = Join-Path $WorkRoot "merge-ut-dictionaries"

if ($Profile -eq "safe") {
    $EffectiveProfile = "sample"
} else {
    $EffectiveProfile = $Profile
}

Write-Host "Repo root: $RepoRoot"
Write-Host "Work root: $WorkRoot"
Write-Host "Output dir: $OutDir"
Write-Host "Profile: $EffectiveProfile"

New-Item -ItemType Directory -Force $WorkRoot | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null

function Test-IsWindowsHost {
    if ($PSVersionTable.ContainsKey("Platform")) {
        return $PSVersionTable.Platform -eq "Win32NT"
    }

    return $env:OS -eq "Windows_NT"
}

function Test-BashPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        return $false
    }

    try {
        $Output = & $Path -lc "printf MOZC_BASH_OK" 2>$null
        return ($LASTEXITCODE -eq 0 -and $Output -eq "MOZC_BASH_OK")
    } catch {
        return $false
    }
}

function Find-Bash {
    param(
        [string]$RequestedBashPath = ""
    )

    if ($RequestedBashPath) {
        if (Test-BashPath $RequestedBashPath) {
            return (Resolve-Path $RequestedBashPath).Path
        }
        throw "Specified bash is not usable: $RequestedBashPath"
    }

    if (Test-IsWindowsHost) {
        $BashCandidates = @(
            (Join-Path $RepoRoot "src\third_party\msys64\usr\bin\bash.exe"),
            (Join-Path $RepoRoot "src\third_party\msys64\bin\bash.exe"),
            "C:\Program Files\Git\bin\bash.exe",
            "C:\Program Files\Git\usr\bin\bash.exe",
            "C:\Program Files (x86)\Git\bin\bash.exe",
            "C:\Program Files (x86)\Git\usr\bin\bash.exe"
        )

        foreach ($Candidate in $BashCandidates) {
            if (Test-BashPath $Candidate) {
                return (Resolve-Path $Candidate).Path
            }
        }

        $PathBashes = @(Get-Command bash.exe -ErrorAction SilentlyContinue)
        foreach ($Command in $PathBashes) {
            $Source = $Command.Source

            # Do not select WSL bash launchers blindly. They depend on the
            # default WSL distribution and may resolve to docker-desktop,
            # which is not a normal Linux user environment.
            if ($Source -like "$env:WINDIR\System32\bash.exe" -or
                $Source -like "$env:LOCALAPPDATA\Microsoft\WindowsApps\bash.exe") {
                if (Test-BashPath $Source) {
                    return $Source
                }
                continue
            }

            if (Test-BashPath $Source) {
                return $Source
            }
        }

        throw "Usable bash was not found. Install Git for Windows, use Mozc's MSYS bash, or pass -BashPath explicitly."
    }

    $Command = Get-Command bash -ErrorAction SilentlyContinue
    if ($Command -and (Test-BashPath $Command.Source)) {
        return $Command.Source
    }

    foreach ($Candidate in @("/bin/bash", "/usr/bin/bash")) {
        if (Test-BashPath $Candidate) {
            return $Candidate
        }
    }

    throw "Usable bash was not found."
}

$BashPath = Find-Bash -RequestedBashPath $BashPath
Write-Host "Using bash: $BashPath"

if (-not (Test-Path $MergeRepo)) {
    Write-Host "Cloning merge-ut-dictionaries..."
    git clone --depth 1 https://github.com/utuhiro78/merge-ut-dictionaries.git $MergeRepo
    if ($LASTEXITCODE -ne 0) {
        throw "git clone failed."
    }
} else {
    Write-Host "Updating merge-ut-dictionaries..."
    git -C $MergeRepo pull --ff-only
    if ($LASTEXITCODE -ne 0) {
        throw "git pull failed."
    }
}

$MakePath = Join-Path $MergeRepo "src\merge\make.sh"
if (-not (Test-Path $MakePath)) {
    throw "make.sh was not found: $MakePath"
}

$Flags = @{
    alt_cannadic   = $false
    edict2         = $false
    jawiki         = $false
    neologd        = $false
    personal_names = $false
    place_names    = $false
    skk_jisyo      = $false
    sudachidict    = $false
    generate_latest = $false
}

switch ($EffectiveProfile) {
    "sample" {
        # Minimal and reproducible first profile.
        $Flags.place_names = $true
        $Flags.sudachidict = $true
    }

    "daily" {
        # Current daily baseline.
        # Keep this conservative until we add cost profiling.
        $Flags.place_names = $true
        $Flags.sudachidict = $true
    }

    "rich" {
        # Broad recall profile, but not full legacy/GPL-heavy set.
        $Flags.jawiki = $true
        $Flags.neologd = $true
        $Flags.personal_names = $true
        $Flags.place_names = $true
        $Flags.sudachidict = $true
    }

    "max" {
        # Experimental full-recall profile.
        $Flags.alt_cannadic = $true
        $Flags.edict2 = $true
        $Flags.jawiki = $true
        $Flags.neologd = $true
        $Flags.personal_names = $true
        $Flags.place_names = $true
        $Flags.skk_jisyo = $true
        $Flags.sudachidict = $true
    }

    default {
        throw "Unknown profile: $EffectiveProfile"
    }
}

Write-Host "Enabled dictionaries:"
foreach ($Key in $Flags.Keys | Sort-Object) {
    if ($Key -eq "generate_latest") {
        continue
    }

    if ($Flags[$Key]) {
        Write-Host "  + $Key"
    }
}

Write-Host "Disabled dictionaries:"
foreach ($Key in $Flags.Keys | Sort-Object) {
    if ($Key -eq "generate_latest") {
        continue
    }

    if (-not $Flags[$Key]) {
        Write-Host "  - $Key"
    }
}

function Set-MakeFlag {
    param(
        [string]$Content,
        [string]$Name,
        [bool]$Enabled
    )

    if ($Enabled) {
        $Replacement = "$Name=`"true`""
    } else {
        $Replacement = "#$Name=`"true`""
    }

    $Pattern = "(?m)^#?" + [System.Text.RegularExpressions.Regex]::Escape("$Name=`"true`"")
    return [System.Text.RegularExpressions.Regex]::Replace($Content, $Pattern, $Replacement)
}

Write-Host "Patching make.sh for profile: $EffectiveProfile"

$Content = Get-Content -Raw -Encoding UTF8 $MakePath

foreach ($Key in $Flags.Keys) {
    $Content = Set-MakeFlag -Content $Content -Name $Key -Enabled $Flags[$Key]
}

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($MakePath, $Content, $Utf8NoBom)

# ---------------------------------------------------------------------------
# merge-ut-dictionaries resolves Mozc's dictionary entries through the GitHub
# REST API with an unauthenticated urllib call.  Shared GitHub Actions runner
# IPs exhaust the 60 requests/hour anonymous quota easily, which aborts the
# whole daily-dictionary build with "HTTP Error 403: rate limit exceeded".
# When the workflow provides a token (MOZKEY_DICT_GITHUB_TOKEN), install a
# small preamble in the freshly cloned merge script that attaches it to
# api.github.com requests.  Without a token the preamble stays inert, so local
# runs keep exactly the previous behaviour.
$MergeScript = Join-Path $MergeRepo "src\merge\merge_dictionaries.py"
$MergeMarker = "# mozkey: authenticated GitHub API preamble"
try {
    if ((Test-Path $MergeScript) -and
        ((Get-Content -Raw -Encoding UTF8 $MergeScript) -notlike "*$MergeMarker*")) {
        $Preamble = @'
# mozkey: authenticated GitHub API preamble
import os as _mozkey_os
import urllib.request as _mozkey_urllib

_mozkey_token = (
    _mozkey_os.environ.get("MOZKEY_DICT_GITHUB_TOKEN")
    or _mozkey_os.environ.get("GITHUB_TOKEN")
    or _mozkey_os.environ.get("GH_TOKEN")
)
if _mozkey_token and not getattr(_mozkey_urllib, "_mozkey_patched", False):
    _mozkey_urlopen_original = _mozkey_urllib.urlopen

    def _mozkey_urlopen(_mozkey_request, *args, **kwargs):
        if isinstance(_mozkey_request, str):
            _mozkey_request = _mozkey_urllib.Request(_mozkey_request)
        if "api.github.com" in getattr(_mozkey_request, "full_url", ""):
            _mozkey_request.add_header(
                "Authorization", "Bearer " + _mozkey_token
            )
        return _mozkey_urlopen_original(_mozkey_request, *args, **kwargs)

    _mozkey_urllib.urlopen = _mozkey_urlopen
    _mozkey_urllib._mozkey_patched = True
# mozkey: end preamble

'@
        $MergeSource = Get-Content -Raw -Encoding UTF8 $MergeScript
        if ($MergeSource.StartsWith("#!")) {
            $Break = $MergeSource.IndexOf("`n")
            $MergeSource = $MergeSource.Substring(0, $Break + 1) + $Preamble +
                $MergeSource.Substring($Break + 1)
        } else {
            $MergeSource = $Preamble + $MergeSource
        }
        [System.IO.File]::WriteAllText($MergeScript, $MergeSource, $Utf8NoBom)

        if ($env:MOZKEY_DICT_GITHUB_TOKEN -or $env:GITHUB_TOKEN -or $env:GH_TOKEN) {
            Write-Host "Installed authenticated GitHub API preamble in merge_dictionaries.py"
        } else {
            Write-Host "Installed inert GitHub API preamble (no token present)"
        }
    }
} catch {
    Write-Warning "Could not install the GitHub API preamble: $_"
}

$MergeDir = Join-Path $MergeRepo "src\merge"

Write-Host "Running make.sh..."
Push-Location $MergeDir
try {
    & $BashPath -lc "bash ./make.sh"
    if ($LASTEXITCODE -ne 0) {
        throw "make.sh failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

$Generated = Join-Path $MergeDir "mozcdic-ut.txt"
if (-not (Test-Path $Generated)) {
    throw "mozcdic-ut.txt was not generated."
}

if ($EffectiveProfile -eq "sample") {
    # Keep legacy file names used in the previous step.
    $OutFile = Join-Path $OutDir "mozcdic-ut-safe.txt"
    $SampleFile = Join-Path $OutDir "mozcdic-ut-sample.txt"
} else {
    $OutFile = Join-Path $OutDir "mozcdic-ut-$EffectiveProfile.txt"
    $SampleFile = Join-Path $OutDir "mozcdic-ut-$EffectiveProfile-sample.txt"
}

Copy-Item $Generated $OutFile -Force

$LineCount = (Get-Content -Encoding UTF8 $OutFile | Measure-Object -Line).Lines
$Size = (Get-Item $OutFile).Length

if ($LineCount -le 0) {
    throw "Generated dictionary is empty: $OutFile"
}

if ($SampleLines -gt 0) {
    [string[]]$Sample = @(Get-Content -Encoding UTF8 $OutFile -TotalCount $SampleLines)

    if ($Sample.Count -le 0) {
        throw "Generated sample dictionary would be empty: $OutFile"
    }

    [System.IO.File]::WriteAllLines($SampleFile, $Sample, $Utf8NoBom)
}

Write-Host ""
Write-Host "Generated full dictionary:"
Write-Host "  $OutFile"
Write-Host "Lines:"
Write-Host "  $LineCount"
Write-Host "Bytes:"
Write-Host "  $Size"

if ($SampleLines -gt 0) {
    $SampleLineCount = (Get-Content -Encoding UTF8 $SampleFile | Measure-Object -Line).Lines
    $SampleSize = (Get-Item $SampleFile).Length

    Write-Host ""
    Write-Host "Generated sample dictionary:"
    Write-Host "  $SampleFile"
    Write-Host "Lines:"
    Write-Host "  $SampleLineCount"
    Write-Host "Bytes:"
    Write-Host "  $SampleSize"
}

Write-Host ""
Write-Host "Done."

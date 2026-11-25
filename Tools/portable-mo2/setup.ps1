# First-time setup for the Skyrim Together Co-op portable Mod Organizer 2.
# Finds your Skyrim Special Edition folder, checks the game version and SKSE,
# points this MO2 at your game, then starts MO2. Safe to run again any time
# (for example after you move this folder).

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$wantVersion = '1.7.104'

function Test-GameDir($dir) { $dir -and (Test-Path (Join-Path $dir 'SkyrimSE.exe')) }

function Find-Game {
    # 1. Bethesda registry key written by Steam
    foreach ($k in 'HKLM:\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition',
                   'HKLM:\SOFTWARE\Bethesda Softworks\Skyrim Special Edition') {
        try { $p = (Get-ItemProperty $k -ErrorAction Stop).'installed path'; if (Test-GameDir $p) { return $p.TrimEnd('\') } } catch {}
    }
    # 2. Every Steam library folder
    $steam = $null
    try { $steam = (Get-ItemProperty 'HKCU:\SOFTWARE\Valve\Steam' -ErrorAction Stop).SteamPath } catch {}
    if (-not $steam) { $steam = "${env:ProgramFiles(x86)}\Steam" }
    $libs = @($steam)
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path $vdf) {
        foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value -replace '\\\\', '\' }
    }
    foreach ($l in $libs) {
        $p = Join-Path $l 'steamapps\common\Skyrim Special Edition'
        if (Test-GameDir $p) { return $p }
    }
    return $null
}

function Pick-Folder {
    Add-Type -AssemblyName System.Windows.Forms
    $d = New-Object System.Windows.Forms.FolderBrowserDialog
    $d.Description = 'Pick your Skyrim Special Edition folder (the one with SkyrimSE.exe in it)'
    if ($d.ShowDialog() -eq 'OK') { return $d.SelectedPath }
    return $null
}

Write-Host ''
Write-Host '=== Skyrim Together Co-op: first-time setup ===' -ForegroundColor Cyan

$game = Find-Game
if ($game) {
    Write-Host "Found Skyrim here: $game"
} else {
    Write-Host 'Could not find Skyrim automatically. A window will open: pick your Skyrim Special Edition folder.' -ForegroundColor Yellow
    $game = Pick-Folder
}
if (-not (Test-GameDir $game)) {
    Write-Host 'That folder has no SkyrimSE.exe. Install Skyrim Special Edition from Steam, start it once, then run this again.' -ForegroundColor Red
    Read-Host 'Press Enter to close'; exit 1
}

# Game version
$v = (Get-Item (Join-Path $game 'SkyrimSE.exe')).VersionInfo.FileVersion
if (-not $v.StartsWith($wantVersion)) {
    Write-Host "WARNING: your game is version $v, but this build was tested on $wantVersion." -ForegroundColor Yellow
    Write-Host 'Update Skyrim in Steam (or get the matching SKSE build). It may not start otherwise.' -ForegroundColor Yellow
} else {
    Write-Host "Game version $v - good."
}

# SKSE (cannot be bundled: its authors do not allow redistribution)
# This script installs it from the archive YOU download from its official Nexus page.
$skseNexus = 'https://www.nexusmods.com/skyrimspecialedition/mods/30379?tab=files'

function Find-SkseArchive {
    $dirs = @((Join-Path $env:USERPROFILE 'Downloads'), (Join-Path $root 'downloads'), $root)
    try { $dirs += (New-Object -ComObject Shell.Application).NameSpace('shell:Downloads').Self.Path } catch {}
    $files = foreach ($d in ($dirs | Select-Object -Unique)) {
        if (Test-Path $d) {
            Get-ChildItem $d -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Extension -in '.7z', '.zip' -and ($_.Name -match '^skse64_\d' -or $_.Name -match 'Skyrim Script Extender' -or $_.Name -match '-30379-') -and $_.Name -notmatch 'gog' }
        }
    }
    $files | Sort-Object LastWriteTime -Descending | Select-Object -First 1
}

function Install-Skse($archive) {
    $tmp = Join-Path $env:TEMP ('skse-install-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $tmp | Out-Null
    try {
        & tar.exe -x -f $archive.FullName -C $tmp
        if ($LASTEXITCODE -ne 0) { throw "could not unpack $($archive.Name)" }
        $loader = Get-ChildItem $tmp -Recurse -Filter 'skse64_loader.exe' | Select-Object -First 1
        if (-not $loader) { throw "$($archive.Name) has no skse64_loader.exe in it" }
        $top = $loader.DirectoryName
        # loader + dll must sit next to SkyrimSE.exe (MO2 cannot provide these)
        Get-ChildItem $top -File | Where-Object { $_.Name -like 'skse64_*.exe' -or $_.Name -like 'skse64_*.dll' } |
            ForEach-Object { Copy-Item $_.FullName $game -Force }
        # SKSE's scripts go in as a Mod Organizer mod
        $data = Join-Path $top 'Data'
        if (Test-Path $data) {
            $mod = Join-Path $root 'mods\Skyrim Script Extender (SKSE64)'
            New-Item -ItemType Directory $mod -Force | Out-Null
            Copy-Item (Join-Path $data '*') $mod -Recurse -Force
            $ml = Join-Path $root 'profiles\Co-op\modlist.txt'
            $lines = @(Get-Content $ml) | Where-Object { $_ -notmatch '^[+-]Skyrim Script Extender \(SKSE64\)$' }
            $lines = @($lines[0], '+Skyrim Script Extender (SKSE64)') + @($lines | Select-Object -Skip 1)
            [IO.File]::WriteAllLines($ml, $lines, (New-Object System.Text.UTF8Encoding($false)))
        }
        Write-Host "SKSE installed from $($archive.Name)." -ForegroundColor Green
        return $true
    } catch {
        Write-Host "Could not install SKSE automatically: $_" -ForegroundColor Red
        return $false
    } finally {
        Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

$skseOk = Test-Path (Join-Path $game 'skse64_loader.exe')
if ($skseOk) {
    Write-Host 'SKSE found - good.'
} else {
    $arc = Find-SkseArchive
    for ($try = 0; -not $arc -and $try -lt 3; $try++) {
        Write-Host ''
        Write-Host 'SKSE is needed. Its authors do not allow it to be shared, so it has to come from its own page.' -ForegroundColor Yellow
        Write-Host '  The SKSE page on Nexus opens now. Under "Main files" click "Manual download"'
        Write-Host '  (log in to your free Nexus account if asked) and let it save to your Downloads folder.'
        Start-Process $skseNexus
        Read-Host '  When the download has finished, press Enter here'
        $arc = Find-SkseArchive
        if (-not $arc) { Write-Host '  I could not find the SKSE file in your Downloads folder yet.' -ForegroundColor Yellow }
    }
    if ($arc) { $skseOk = Install-Skse $arc }
    $want = 'skse64_' + (($v -split '\.')[0..2] -join '_') + '.dll'
    if ($skseOk -and -not (Test-Path (Join-Path $game $want))) {
        Write-Host "WARNING: that SKSE file ($($arc.Name)) is for a different game version (no $want)." -ForegroundColor Yellow
        Write-Host "  Download the current Anniversary Edition build from $skseNexus and run setup again." -ForegroundColor Yellow
    }
    if (-not $skseOk) {
        Write-Host ''
        Write-Host 'Install SKSE by hand: open the SKSE 7z, and copy everything inside its skse64_x_xx_xx folder into:' -ForegroundColor Yellow
        Write-Host "  $game   (say Yes to merging the Data folder)" -ForegroundColor Yellow
        Start-Process explorer.exe $game
    }
}

# Leftover SKSE plugins installed straight into the game folder (from earlier modding) also get
# loaded by Skyrim Together, and many (e.g. PrismaUI.dll) then fail with
# "REL/ID.h: failed to open address library file". Offer to move them out of the way.
$gp = Join-Path $game 'Data\SKSE\Plugins'
$extra = @(Get-ChildItem $gp -Filter '*.dll' -File -ErrorAction SilentlyContinue)
if ($extra.Count -gt 0) {
    Write-Host ''
    Write-Host "Your Skyrim folder has $($extra.Count) other SKSE plugin(s) installed directly in Data\SKSE\Plugins:" -ForegroundColor Yellow
    $extra | ForEach-Object { Write-Host "  $($_.Name)" }
    Write-Host 'These are not part of the co-op build and can crash it (e.g. "failed to open address library file").' -ForegroundColor Yellow
    $bk = Join-Path $game 'Data\SKSE\Plugins (moved by co-op setup)'
    $ans = Read-Host "Move them to '$bk' (you can move them back any time)? [Y/n]"
    if ($ans -notmatch '^[nN]') {
        New-Item -ItemType Directory $bk -Force | Out-Null
        foreach ($f in $extra) {
            $base = [IO.Path]::GetFileNameWithoutExtension($f.Name)
            # move the dll plus its matching config files (same base name: .ini, .toml, .json, .pdb ...)
            Get-ChildItem $gp -File | Where-Object { [IO.Path]::GetFileNameWithoutExtension($_.Name) -eq $base -or $_.Name -like "$base.*" } |
                ForEach-Object { Move-Item $_.FullName $bk -Force }
            $sub = Join-Path $gp $base
            if (Test-Path $sub) { Move-Item $sub $bk -Force }
        }
        Write-Host 'Moved.' -ForegroundColor Green
    }
}

# Address Library check (also cannot be bundled).
# Skyrim Together needs Data\SKSE\Plugins\versionlib-<game version>.bin, e.g. versionlib-1-7-104-0.bin
$verParts = ($v -split '[^0-9]+' | Where-Object { $_ -ne '' })[0..3]
while ($verParts.Count -lt 4) { $verParts += '0' }
$alName = 'versionlib-' + ($verParts -join '-') + '.bin'
$enabled = @()
$ml = Join-Path $root 'profiles\Co-op\modlist.txt'
if (Test-Path $ml) { $enabled = Get-Content $ml | Where-Object { $_ -like '+*' } | ForEach-Object { $_.Substring(1) } }
$good = $null; $wrongPlace = $null; $off = $null
foreach ($d in Get-ChildItem (Join-Path $root 'mods') -Directory) {
    if (Test-Path (Join-Path $d.FullName "SKSE\Plugins\$alName")) {
        if ($enabled -contains $d.Name) { $good = $d.Name } else { $off = $d.Name }
    } elseif (Get-ChildItem $d.FullName -Recurse -Filter $alName -ErrorAction SilentlyContinue | Select-Object -First 1) {
        $wrongPlace = $d.Name
    } elseif ((Get-ChildItem $d.FullName -Recurse -Filter 'versionlib-*.bin' -ErrorAction SilentlyContinue | Select-Object -First 1)) {
        if (-not $wrongPlace) { $script:oldAl = $d.Name }
    }
}
if (-not $good -and (Test-Path (Join-Path $game "Data\SKSE\Plugins\$alName"))) { $good = '(your Skyrim Data folder)' }
Write-Host ''
if ($good) {
    Write-Host "Address Library for your game version found ($alName in $good) - good."
} elseif ($off) {
    Write-Host "Address Library is installed as '$off' but NOT ticked. Tick it in Mod Organizer's left list." -ForegroundColor Yellow
} elseif ($wrongPlace) {
    Write-Host "Address Library in '$wrongPlace' is in the wrong folder layout." -ForegroundColor Yellow
    Write-Host "  In Mod Organizer: right-click it -> Open in Explorer. $alName must be at SKSE\Plugins\$alName" -ForegroundColor Yellow
    Write-Host '  (delete that mod and install the zip again, choosing the folder that contains "SKSE" as the top level).' -ForegroundColor Yellow
} elseif ($script:oldAl) {
    Write-Host "Address Library '$($script:oldAl)' is installed but has no file for game version $v ($alName)." -ForegroundColor Yellow
    Write-Host '  You downloaded an older file. On the Nexus Files tab get the NEWEST Address Library file that' -ForegroundColor Yellow
    Write-Host "  lists your game version ($v), install it in Mod Organizer and remove the old one." -ForegroundColor Yellow
} else {
    Write-Host "Address Library is not installed yet (Skyrim Together needs $alName). See README step 4." -ForegroundColor Yellow
}

# Write ModOrganizer.ini for this PC
function Fwd($p) { $p -replace '\\', '/' }
function Ba($p)  { '@ByteArray(' + ($p -replace '\\', '\\') + ')' }

# Skyrim Together remembers its own game folder in the registry (from any earlier Skyrim Together
# install, e.g. another modlist's "Stock Game"). If that differs from the folder Mod Organizer uses,
# Mod Organizer's mods (including Address Library) are invisible to it and you get
# "Failed to load Skyrim Address Library". Point it at the same folder, and also pass --exePath.
$tKey = 'HKCU:\Software\TiltedPhoques\TiltedEvolution\Skyrim Special Edition'
$gameExe = Join-Path $game 'SkyrimSE.exe'
$old = $null
try { $old = (Get-ItemProperty $tKey -ErrorAction Stop).TitlePath } catch {}
if ($old -and ($old.TrimEnd('\') -ne $game.TrimEnd('\'))) {
    Write-Host ''
    Write-Host "Skyrim Together was set to a different game folder: $old" -ForegroundColor Yellow
    Write-Host "Switching it to: $game  (old value saved in setup\skyrim-together-old-path.txt)" -ForegroundColor Yellow
    Set-Content (Join-Path $PSScriptRoot 'skyrim-together-old-path.txt') $old
}
if (-not (Test-Path $tKey)) { New-Item $tKey -Force | Out-Null }
Set-ItemProperty $tKey -Name TitlePath -Value $game
Set-ItemProperty $tKey -Name TitleExe -Value $gameExe

$str = Join-Path $root 'mods\Skyrim Together Reborn - Coop Test Build\SkyrimTogetherReborn'
$strArgs = ''
if ($gameExe -match '^[\x20-\x7E]+$') { $strArgs = '--exePath \"' + ($gameExe -replace '\\', '\\') + '\"' }
$exes = @(
    @{ t = 'Skyrim Together (Coop Test Build)'; b = (Join-Path $str 'SkyrimTogether.exe');   w = $str;  a = $strArgs; tb = 'true' },
    @{ t = 'SKSE';                              b = (Join-Path $game 'skse64_loader.exe');   w = $game; a = ''; tb = 'false' },
    @{ t = 'Skyrim Special Edition';            b = (Join-Path $game 'SkyrimSE.exe');        w = $game; a = ''; tb = 'false' },
    @{ t = 'Skyrim Special Edition Launcher';   b = (Join-Path $game 'SkyrimSELauncher.exe');w = $game; a = ''; tb = 'false' },
    @{ t = 'Explore Virtual Folder';            b = (Join-Path $root 'explorer++\Explorer++.exe'); w = (Join-Path $root 'explorer++');
       a = '\"' + ((Join-Path $game 'data') -replace '\\', '\\') + '\"'; tb = 'false' }
)
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('[customExecutables]')
[void]$sb.AppendLine("size=$($exes.Count)")
for ($i = 0; $i -lt $exes.Count; $i++) {
    $n = $i + 1; $e = $exes[$i]
    [void]$sb.AppendLine("$n\title=$($e.t)")
    [void]$sb.AppendLine("$n\binary=$(Fwd $e.b)")
    [void]$sb.AppendLine("$n\workingDirectory=$(Fwd $e.w)")
    [void]$sb.AppendLine("$n\arguments=$($e.a)")
    [void]$sb.AppendLine("$n\ownicon=true")
    [void]$sb.AppendLine("$n\hide=false")
    [void]$sb.AppendLine("$n\steamAppID=")
    [void]$sb.AppendLine("$n\toolbar=$($e.tb)")
}

$tpl = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'ModOrganizer.template.ini'))
$ini = $tpl.Replace('{{GAMEPATH}}', (Ba $game)).Replace('{{EXECUTABLES}}', $sb.ToString().TrimEnd())
$iniPath = Join-Path $root 'ModOrganizer.ini'
if (Test-Path $iniPath) { Copy-Item $iniPath "$iniPath.previous" -Force }
[IO.File]::WriteAllText($iniPath, $ini, (New-Object System.Text.UTF8Encoding($false)))
Write-Host ''
Write-Host 'Mod Organizer is now set up for this PC.' -ForegroundColor Green

if (-not $skseOk) {
    Read-Host 'Install SKSE as shown above, then press Enter to start Mod Organizer'
}
Start-Process (Join-Path $root 'ModOrganizer.exe') -WorkingDirectory $root

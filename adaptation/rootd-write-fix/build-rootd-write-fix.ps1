param(
    [Parameter(Mandatory = $true)][string]$NdkHome,
    [switch]$Install
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$source = Join-Path $root "adaptation\aph136g142\src"
$out = Join-Path $root "build\rootd-write-fix"
$cc = Join-Path $NdkHome "toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android35-clang.cmd"
if (-not (Test-Path $cc)) { throw "NDK clang not found: $cc" }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$profiles = @(
    @{ Id = "61258"; Define = ""; Path = "magic104\98260\98260" },
    @{ Id = "6138"; Define = "PC98260_TARGET_6138"; Path = "magic104\98260\98260-6138" },
    @{ Id = "aph112"; Define = "PC98260_TARGET_APH112"; Path = "root_extra\g459a\98260\98260-aph112" },
    @{ Id = "aph136g142"; Define = "PC98260_TARGET_APH136G142"; Path = "root_extra\g142\98260\98260-aph136g142" }
)
foreach ($profile in $profiles) {
    $id = $profile.Id
    $binary = Join-Path $out "exploit-android-key-ptr-$id"
    $define = if ($profile.Define) { "-D" + $profile.Define } else { $null }
    if ($define) {
        & $cc -std=gnu11 -O2 -w -pthread -static "-I$source" $define -o $binary `
            (Join-Path $source "uaf_plant_61258.c") (Join-Path $source "stubs.c") (Join-Path $source "rootd.c")
    } else {
        & $cc -std=gnu11 -O2 -w -pthread -static "-I$source" -o $binary `
            (Join-Path $source "uaf_plant_61258.c") (Join-Path $source "stubs.c") (Join-Path $source "rootd.c")
    }
    if ($LASTEXITCODE -ne 0) { throw "rootd build failed: $id" }
    $hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "$hash  exploit-android-key-ptr ($id)"
    if ($Install) {
        $destination = Join-Path $root (Join-Path "app\src\main\assets" $profile.Path)
        Copy-Item $binary (Join-Path $destination "exploit-android-key-ptr") -Force
        $manifest = Join-Path $destination "SHA256SUMS.txt"
        if (-not (Test-Path $manifest)) { throw "missing profile manifest: $manifest" }
        $lines = [IO.File]::ReadAllLines($manifest)
        $found = $false
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i].EndsWith("  exploit-android-key-ptr")) {
                $lines[$i] = "$hash  exploit-android-key-ptr"
                $found = $true
            }
        }
        if (-not $found) { $lines += "$hash  exploit-android-key-ptr" }
        [IO.File]::WriteAllText($manifest, (($lines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
    }
}

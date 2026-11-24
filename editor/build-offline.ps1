$ErrorActionPreference = 'Stop'
$releaseDirectory = Join-Path $PSScriptRoot 'releases'
New-Item -ItemType Directory -Path $releaseDirectory -Force | Out-Null
$releaseRoot = [System.IO.Path]::GetFullPath($releaseDirectory).TrimEnd('\') + '\'
$stage = Join-Path $releaseDirectory ('.stage-' + [guid]::NewGuid().ToString('N'))
$package = Join-Path $stage 'Modulox'
$zip = Join-Path $releaseDirectory 'Modulox-Windows-offline.zip'
try {
    New-Item -ItemType Directory -Path (Join-Path $package 'dist/presets/assets') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $package 'dist/demo-samples') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $package 'dist/examples') -Force | Out-Null
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'dist/firmware/latest.json')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dist/firmware') -Destination (Join-Path $package 'dist') -Recurse
    }
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'dist/vendor')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dist/vendor') -Destination (Join-Path $package 'dist') -Recurse
    }
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'dist/factory/latest.json')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dist/factory') -Destination (Join-Path $package 'dist') -Recurse
    }
    foreach ($name in @('lancer.cmd', 'arreter.cmd', 'offline.ps1', 'HORS-LIGNE.txt')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $package
    }
    foreach ($name in @('flags', 'locales')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot "dist/$name") -Destination (Join-Path $package 'dist') -Recurse
    }
    foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'dist') -File)) {
        if ($file.Extension -in @('.mjs', '.css', '.html')) { Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $package 'dist') }
    }
    foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'dist/presets') -File)) {
        if ($file.Name -match '^[a-z0-9-]+\.json$' -and $file.Name -ne 'index.json') {
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $package 'dist/presets')
        }
    }
    foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'dist/presets/assets') -File)) {
        if ($file.Name -match '^[a-f0-9]{14}\.pcm$' -or $file.Name -in @('index.json', 'AKWF-CREDITS.txt')) {
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $package 'dist/presets/assets')
        }
    }
    foreach ($name in @('kick01.wav', 'kick04.wav', 'snare02.wav', 'voice.wav')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot "dist/demo-samples/$name") -Destination (Join-Path $package 'dist/demo-samples')
    }
    foreach ($name in @('01-basic.json', '02-triple.json')) {
        $example = Join-Path $PSScriptRoot "../firmware/patches/$name"
        Copy-Item -LiteralPath $example -Destination (Join-Path $package 'dist/examples')
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../LICENSE') -Destination $package
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'PRESETS.md') -Destination $package
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../firmware/UPDATES.md') -Destination (Join-Path $package 'MISES-A-JOUR.md')
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../firmware/RELEASE-1.1.md') -Destination (Join-Path $package 'NOUVEAUTES-1.1.md')
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../firmware/RELEASE-1.2.md') -Destination (Join-Path $package 'NOUVEAUTES-1.2.md')
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'INSTALLATION-USINE.md') -Destination $package
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../firmware/BOARDS.md') -Destination (Join-Path $package 'MODELES-MATERIELS.md')
    Compress-Archive -LiteralPath $package -DestinationPath (Join-Path $stage 'package.zip') -CompressionLevel Optimal
    Move-Item -LiteralPath (Join-Path $stage 'package.zip') -Destination $zip -Force
    Get-Item -LiteralPath $zip | Select-Object FullName, Length
} finally {
    $stageTarget = [System.IO.Path]::GetFullPath($stage)
    if (!$stageTarget.StartsWith($releaseRoot, [System.StringComparison]::OrdinalIgnoreCase)) { throw 'Nettoyage refuse : chemin hors du dossier releases.' }
    if (Test-Path -LiteralPath $stageTarget) { Remove-Item -LiteralPath $stageTarget -Recurse -Force }
}

<#
Build complete Modulox firmware images flashable at address 0x0.

The generated images include bootloader, partition table, boot_app0, app,
and the FFat data partition used by the sketches.
#>
[CmdletBinding()]
param(
    [string[]] $Synth = @("FMbrain", "granulizer", "organbrain", "sampbrain", "VAsynth"),
    [string] $ArduinoCli = "",
    [string] $Mkfatfs = "",
    [string] $OutputDir = "",
    [string] $BuildDir = "",
    [switch] $NoClean
)

$ErrorActionPreference = "Stop"

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Esp32CoreVersion = "3.0.5"
$Fqbn = "esp32:esp32:esp32s3:FlashMode=qio,FlashSize=16M,PSRAM=opi,USBMode=default,UploadMode=cdc,PartitionScheme=app3M_fat9M_16MB"
$DataOffset = "0x610000"
$DataSize = 0x9E0000
$FlashSize = "16MB"

function Resolve-InRepo {
    param([string] $Path)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if (-not $fullPath.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path must stay inside repository: $fullPath"
    }
    return $fullPath
}

function Ensure-Directory {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Resolve-CommandPath {
    param(
        [string] $ExplicitPath,
        [string] $CommandName,
        [string[]] $Candidates
    )

    if ($ExplicitPath) {
        $fullPath = [System.IO.Path]::GetFullPath($ExplicitPath)
        if (-not (Test-Path -LiteralPath $fullPath)) {
            throw "$CommandName not found at $fullPath"
        }
        return $fullPath
    }

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw "$CommandName not found. Pass its path explicitly or add it to PATH."
}

function Get-ArduinoDataDir {
    param([string] $ArduinoCliPath)

    try {
        $configJson = & $ArduinoCliPath config dump --format json 2>$null
        $config = $configJson | ConvertFrom-Json
        if ($config.directories.data) {
            return [System.IO.Path]::GetFullPath($config.directories.data)
        }
    } catch {
        # Fall through to the standard Windows Arduino data location.
    }

    if (-not $env:LOCALAPPDATA) {
        throw "Unable to locate Arduino data directory. Run arduino-cli config dump or set LOCALAPPDATA."
    }
    return [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "Arduino15"))
}

function Get-OneFile {
    param(
        [string] $Directory,
        [string] $Filter,
        [scriptblock] $Predicate
    )

    $files = @(Get-ChildItem -Path $Directory -Recurse -File -Filter $Filter | Where-Object $Predicate)
    if ($files.Count -ne 1) {
        $fileList = ($files | Select-Object -ExpandProperty FullName) -join [Environment]::NewLine
        throw "Expected exactly one $Filter in $Directory, got $($files.Count):$([Environment]::NewLine)$fileList"
    }
    return $files[0].FullName
}

function Get-RelativeRepoPath {
    param([string] $Path)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if (-not $fullPath.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath
    }
    return $fullPath.Substring($RepoRoot.Length + 1).Replace("\", "/")
}

$OutputDir = if ($OutputDir) { Resolve-InRepo $OutputDir } else { Resolve-InRepo (Join-Path $RepoRoot "firmware") }
$BuildDir = if ($BuildDir) { Resolve-InRepo $BuildDir } else { Resolve-InRepo (Join-Path $RepoRoot ".build\firmware-16MB") }
$LibrariesDir = Resolve-InRepo (Join-Path $RepoRoot "libraries")

$ArduinoCli = Resolve-CommandPath `
    -ExplicitPath $ArduinoCli `
    -CommandName "arduino-cli" `
    -Candidates @((Join-Path $RepoRoot ".tools\arduino-cli.exe"))

$Mkfatfs = Resolve-CommandPath `
    -ExplicitPath $Mkfatfs `
    -CommandName "mkfatfs" `
    -Candidates @(
        (Join-Path $RepoRoot ".tools\mkfatfs.exe"),
        (Join-Path $env:USERPROFILE ".platformio\packages\tool-mkfatfs\mkfatfs.exe")
    )

$ArduinoDataDir = Get-ArduinoDataDir -ArduinoCliPath $ArduinoCli
$CorePath = Join-Path $ArduinoDataDir "packages\esp32\hardware\esp32\$Esp32CoreVersion"
if (-not (Test-Path -LiteralPath $CorePath)) {
    throw "ESP32 Arduino core $Esp32CoreVersion not found at $CorePath. Install it with: arduino-cli core install esp32:esp32@$Esp32CoreVersion"
}

$EsptoolRoot = Join-Path $ArduinoDataDir "packages\esp32\tools\esptool_py"
$Esptool = @(Get-ChildItem -Path $EsptoolRoot -Recurse -File -Filter "esptool.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1)
if ($Esptool.Count -ne 1) {
    throw "esptool.exe not found under $EsptoolRoot"
}
$Esptool = $Esptool[0].FullName

$BootApp0 = Join-Path $CorePath "tools\partitions\boot_app0.bin"
if (-not (Test-Path -LiteralPath $BootApp0)) {
    throw "boot_app0.bin not found at $BootApp0"
}

if ((Test-Path -LiteralPath $BuildDir) -and -not $NoClean) {
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
Ensure-Directory $BuildDir
Ensure-Directory $OutputDir

$EmptyDataDir = Join-Path $BuildDir "empty-data"
Ensure-Directory $EmptyDataDir

$manifest = @()

foreach ($synthName in $Synth) {
    $SketchDir = Resolve-InRepo (Join-Path $RepoRoot "synths\$synthName")
    if (-not (Test-Path -LiteralPath $SketchDir)) {
        throw "Synth sketch directory not found: $SketchDir"
    }

    Write-Host "==> Building $synthName"

    $DataDir = Join-Path $SketchDir "data"
    if (-not (Test-Path -LiteralPath $DataDir)) {
        $DataDir = $EmptyDataDir
    }

    $SynthBuildDir = Join-Path $BuildDir $synthName
    $ExportDir = Join-Path $BuildDir "$synthName-export"
    $SynthOutputDir = Join-Path $OutputDir $synthName
    Ensure-Directory $SynthBuildDir
    Ensure-Directory $ExportDir
    Ensure-Directory $SynthOutputDir

    $DataBin = Join-Path $SynthBuildDir "$synthName-data.ffat.bin"
    & $Mkfatfs -t fatfs -c $DataDir -s $DataSize $DataBin
    if ($LASTEXITCODE -ne 0) {
        throw "mkfatfs failed for $synthName"
    }

    $DataImageBytes = (Get-Item -LiteralPath $DataBin).Length
    if ($DataImageBytes -ne $DataSize) {
        throw "$synthName data image size is $DataImageBytes, expected $DataSize"
    }

    & $ArduinoCli compile `
        --fqbn $Fqbn `
        --libraries $LibrariesDir `
        --build-path $SynthBuildDir `
        --output-dir $ExportDir `
        --export-binaries `
        $SketchDir
    if ($LASTEXITCODE -ne 0) {
        throw "arduino-cli compile failed for $synthName"
    }

    $BootloaderBin = Get-OneFile $SynthBuildDir "*.bootloader.bin" { $true }
    $PartitionsBin = Get-OneFile $SynthBuildDir "*.partitions.bin" { $true }
    $AppBin = Get-OneFile $SynthBuildDir "*.bin" {
        $_.Name -notmatch "\.(bootloader|partitions|merged)\.bin$" -and
        $_.Name -notmatch "-data\.ffat\.bin$"
    }

    $FirmwareBin = Join-Path $SynthOutputDir "$synthName-firmware-0x0.bin"
    if (Test-Path -LiteralPath $FirmwareBin) {
        Remove-Item -LiteralPath $FirmwareBin -Force
    }

    & $Esptool --chip esp32s3 merge_bin `
        -o $FirmwareBin `
        --fill-flash-size $FlashSize `
        --flash_mode keep `
        --flash_freq keep `
        --flash_size keep `
        0x0 $BootloaderBin `
        0x8000 $PartitionsBin `
        0xe000 $BootApp0 `
        0x10000 $AppBin `
        $DataOffset $DataBin
    if ($LASTEXITCODE -ne 0) {
        throw "esptool merge_bin failed for $synthName"
    }

    $DataIsEmpty = ([System.IO.Path]::GetFullPath($DataDir) -eq [System.IO.Path]::GetFullPath($EmptyDataDir))
    $DataSourceFiles = if ($DataIsEmpty) { 0 } else { @(Get-ChildItem -Path $DataDir -File -Recurse).Count }
    $DataSourceBytes = if ($DataIsEmpty) { 0 } else { (Get-ChildItem -Path $DataDir -File -Recurse | Measure-Object Length -Sum).Sum }
    $FirmwareItem = Get-Item -LiteralPath $FirmwareBin
    $Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $FirmwareBin).Hash.ToLowerInvariant()

    $manifest += [PSCustomObject][ordered]@{
        synth = $synthName
        firmware = Get-RelativeRepoPath $FirmwareBin
        flash_address = "0x0"
        flash_size = $FlashSize
        partition_scheme = "app3M_fat9M_16MB"
        firmware_bytes = $FirmwareItem.Length
        data_partition = "ffat"
        data_offset = $DataOffset
        data_image_bytes = $DataImageBytes
        data_source_files = $DataSourceFiles
        data_source_bytes = $DataSourceBytes
        sha256 = $Sha256
    }
}

$ManifestPath = Join-Path $OutputDir "manifest.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ManifestPath -Encoding ASCII

Write-Host ""
Write-Host "Firmware images generated in $(Get-RelativeRepoPath $OutputDir)"
$manifest | Format-Table synth, firmware_bytes, data_offset, data_source_files, data_source_bytes, sha256 -AutoSize

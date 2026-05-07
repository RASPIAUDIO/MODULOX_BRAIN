<#
Build complete Modulox firmware images flashable at address 0x0.

The generated images include bootloader, partition table, boot_app0, app,
and the FFat data partition used by the sketches.

Pass -NoData to generate smaller images without the FFat data partition.
#>
[CmdletBinding()]
param(
    [string[]] $Synth = @("FMbrain", "granulizer", "organbrain", "sampbrain", "VAsynth"),
    [string] $ArduinoCli = "",
    [string] $Mkfatfs = "",
    [string] $WlFatfsGen = "",
    [string] $Python = "",
    [string] $OutputDir = "",
    [string] $BuildDir = "",
    [string] $Board = "esp32s3",
    [string] $FlashMode = "qio",
    [string] $FlashSize = "16M",
    [string] $CPUFreq = "240",
    [string] $PartitionScheme = "app3M_fat9M_16MB",
    [string] $USBMode = "default",
    [string] $CDCOnBoot = "cdc",
    [string] $UploadMode = "cdc",
    [ValidateSet("auto", "raw", "wear-leveling")]
    [string] $DataImageMode = "auto",
    [switch] $NoData,
    [switch] $NoClean
)

$ErrorActionPreference = "Stop"

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Esp32CoreVersion = "3.0.5"
$Fqbn = "esp32:esp32:$($Board):FlashMode=$FlashMode,FlashSize=$FlashSize,PSRAM=opi,USBMode=$USBMode,CDCOnBoot=$CDCOnBoot,UploadMode=$UploadMode,CPUFreq=$CPUFreq,PartitionScheme=$PartitionScheme"
$FillFlashSizeByOption = @{
    "4M" = "4MB"
    "8M" = "8MB"
    "16M" = "16MB"
    "32M" = "32MB"
}
if (-not $FillFlashSizeByOption.ContainsKey($FlashSize)) {
    throw "Unsupported FlashSize '$FlashSize'. Expected one of: $($FillFlashSizeByOption.Keys -join ', ')"
}
$FlashSizeBytesLabel = $FillFlashSizeByOption[$FlashSize]

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

function Get-WlFatfsGenCandidates {
    $candidates = @()
    if ($env:IDF_PATH) {
        $candidates += (Join-Path $env:IDF_PATH "components\fatfs\wl_fatfsgen.py")
    }

    $frameworksDir = "C:\Espressif\frameworks"
    if (Test-Path -LiteralPath $frameworksDir) {
        $candidates += Get-ChildItem -Path $frameworksDir -Directory -Filter "esp-idf*" -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "components\fatfs\wl_fatfsgen.py" }
    }

    return @($candidates | Where-Object { $_ } | Select-Object -Unique)
}

function Get-PythonCandidates {
    $candidates = @()
    $pythonEnvDir = "C:\Espressif\python_env"
    if (Test-Path -LiteralPath $pythonEnvDir) {
        $candidates += Get-ChildItem -Path $pythonEnvDir -Directory -Filter "*_py*_env" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            ForEach-Object { Join-Path $_.FullName "Scripts\python.exe" }
    }

    return @($candidates | Where-Object { $_ } | Select-Object -Unique)
}

function Get-SynthDataImageMode {
    param(
        [string] $SketchDir,
        [string] $RequestedMode
    )

    if ($RequestedMode -ne "auto") {
        return $RequestedMode
    }

    $sourceFiles = @(Get-ChildItem -Path $SketchDir -File | Where-Object { $_.Extension -in @(".ino", ".h", ".hpp", ".c", ".cpp") })
    if ($sourceFiles.Count -eq 0) {
        return "raw"
    }

    $mountingBrainHeaders = Select-String `
        -Path ($sourceFiles | Select-Object -ExpandProperty FullName) `
        -Pattern '#include\s*[<"](?:modubrain|mbrain)\.h[>"]' `
        -ErrorAction SilentlyContinue

    if ($mountingBrainHeaders) {
        return "wear-leveling"
    }

    return "raw"
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

function Convert-PartitionNumber {
    param([string] $Value)

    $trimmed = $Value.Trim()
    if ($trimmed.StartsWith("0x", [System.StringComparison]::OrdinalIgnoreCase)) {
        return [Convert]::ToInt64($trimmed.Substring(2), 16)
    }
    return [Convert]::ToInt64($trimmed, 10)
}

function Get-PartitionCsvName {
    param(
        [string] $CorePath,
        [string] $PartitionScheme
    )

    $boardsPath = Join-Path $CorePath "boards.txt"
    $pattern = "^esp32s3\.menu\.PartitionScheme\.$([regex]::Escape($PartitionScheme))\.build\.partitions=(.+)$"
    $match = Select-String -Path $boardsPath -Pattern $pattern | Select-Object -First 1
    if ($match) {
        return $match.Matches[0].Groups[1].Value.Trim()
    }
    return $PartitionScheme
}

function Get-FatPartitionInfo {
    param(
        [string] $CorePath,
        [string] $PartitionScheme
    )

    $csvName = Get-PartitionCsvName -CorePath $CorePath -PartitionScheme $PartitionScheme
    $csvPath = Join-Path $CorePath "tools\partitions\$csvName.csv"
    if (-not (Test-Path -LiteralPath $csvPath)) {
        throw "Partition CSV not found for '$PartitionScheme': $csvPath"
    }

    foreach ($line in Get-Content -LiteralPath $csvPath) {
        $clean = ($line -replace "#.*$", "").Trim()
        if (-not $clean) {
            continue
        }

        $parts = @($clean.Split(",") | ForEach-Object { $_.Trim() })
        if ($parts.Count -lt 5) {
            continue
        }

        $name = $parts[0]
        $type = $parts[1]
        $subType = $parts[2]
        if ($type -eq "data" -and ($subType -eq "fat" -or $name -eq "ffat")) {
            $offset = Convert-PartitionNumber $parts[3]
            $size = Convert-PartitionNumber $parts[4]
            return [PSCustomObject]@{
                CsvName = $csvName
                CsvPath = $csvPath
                Name = $name
                Offset = $offset
                OffsetHex = ("0x{0:X}" -f $offset)
                Size = $size
                SizeHex = ("0x{0:X}" -f $size)
            }
        }
    }

    return $null
}

$DefaultOutputDir = if ($NoData) {
    if ($FlashSize -eq "16M" -and $PartitionScheme -eq "app3M_fat9M_16MB") { "firmware-no-data" } else { "firmware-$FlashSizeBytesLabel-no-data" }
} else {
    if ($FlashSize -eq "16M" -and $PartitionScheme -eq "app3M_fat9M_16MB") { "firmware" } else { "firmware-$FlashSizeBytesLabel" }
}
$DefaultBuildDir = if ($NoData) { ".build\firmware-$FlashSizeBytesLabel-no-data" } else { ".build\firmware-$FlashSizeBytesLabel" }
$OutputDir = if ($OutputDir) { Resolve-InRepo $OutputDir } else { Resolve-InRepo (Join-Path $RepoRoot $DefaultOutputDir) }
$BuildDir = if ($BuildDir) { Resolve-InRepo $BuildDir } else { Resolve-InRepo (Join-Path $RepoRoot $DefaultBuildDir) }
$LibrariesDir = Resolve-InRepo (Join-Path $RepoRoot "libraries")

$ArduinoCli = Resolve-CommandPath `
    -ExplicitPath $ArduinoCli `
    -CommandName "arduino-cli" `
    -Candidates @((Join-Path $RepoRoot ".tools\arduino-cli.exe"))

$MkfatfsPath = $null
$WlFatfsGenPath = $null
$PythonPath = $null

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

$PartitionCsvName = Get-PartitionCsvName -CorePath $CorePath -PartitionScheme $PartitionScheme
$PartitionCsvPath = Join-Path $CorePath "tools\partitions\$PartitionCsvName.csv"
if (-not (Test-Path -LiteralPath $PartitionCsvPath)) {
    throw "Partition CSV not found for '$PartitionScheme': $PartitionCsvPath"
}

$FatPartition = Get-FatPartitionInfo -CorePath $CorePath -PartitionScheme $PartitionScheme
if (-not $NoData -and -not $FatPartition) {
    throw "Partition scheme '$PartitionScheme' does not contain a FAT/FFat data partition. Use -NoData or select a FFat partition scheme."
}
$DataOffset = if ($FatPartition) { $FatPartition.OffsetHex } else { $null }
$DataSize = if ($FatPartition) { [int]$FatPartition.Size } else { 0 }

if ((Test-Path -LiteralPath $BuildDir) -and -not $NoClean) {
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
Ensure-Directory $BuildDir
Ensure-Directory $OutputDir

$EmptyDataDir = Join-Path $BuildDir "empty-data"
if (-not $NoData) {
    Ensure-Directory $EmptyDataDir
}

$manifest = @()

foreach ($synthName in $Synth) {
    $SketchDir = Resolve-InRepo (Join-Path $RepoRoot "synths\$synthName")
    if (-not (Test-Path -LiteralPath $SketchDir)) {
        throw "Synth sketch directory not found: $SketchDir"
    }

    Write-Host "==> Building $synthName"

    $DataDir = Join-Path $SketchDir "data"
    if (-not $NoData -and -not (Test-Path -LiteralPath $DataDir)) {
        $DataDir = $EmptyDataDir
    }

    $SynthBuildDir = Join-Path $BuildDir $synthName
    $ExportDir = Join-Path $BuildDir "$synthName-export"
    $SynthOutputDir = Join-Path $OutputDir $synthName
    Ensure-Directory $SynthBuildDir
    Ensure-Directory $ExportDir
    Ensure-Directory $SynthOutputDir

    $SketchPartitionCsv = Join-Path $SketchDir "partitions.csv"
    $CompilePartitionCsv = if (Test-Path -LiteralPath $SketchPartitionCsv) { $SketchPartitionCsv } else { $PartitionCsvPath }
    Copy-Item -LiteralPath $CompilePartitionCsv -Destination (Join-Path $SynthBuildDir "partitions.csv") -Force

    $DataBin = Join-Path $SynthBuildDir "$synthName-data.ffat.bin"
    $DataImageBytes = 0
    $ResolvedDataImageMode = if ($NoData) { "none" } else { Get-SynthDataImageMode -SketchDir $SketchDir -RequestedMode $DataImageMode }
    if (-not $NoData) {
        if ($ResolvedDataImageMode -eq "wear-leveling") {
            if (-not $WlFatfsGenPath) {
                $WlFatfsGenPath = Resolve-CommandPath `
                    -ExplicitPath $WlFatfsGen `
                    -CommandName "wl_fatfsgen.py" `
                    -Candidates (Get-WlFatfsGenCandidates)
            }
            if (-not $PythonPath) {
                $PythonPath = Resolve-CommandPath `
                    -ExplicitPath $Python `
                    -CommandName "python" `
                    -Candidates (Get-PythonCandidates)
            }

            & $PythonPath $WlFatfsGenPath `
                --partition_size $DataSize `
                --sector_size 4096 `
                --long_name_support `
                --output_file $DataBin `
                $DataDir
            if ($LASTEXITCODE -ne 0) {
                throw "wl_fatfsgen failed for $synthName"
            }
        } else {
            if (-not $MkfatfsPath) {
                $MkfatfsPath = Resolve-CommandPath `
                    -ExplicitPath $Mkfatfs `
                    -CommandName "mkfatfs" `
                    -Candidates @(
                        (Join-Path $RepoRoot ".tools\mkfatfs.exe"),
                        (Join-Path $env:USERPROFILE ".platformio\packages\tool-mkfatfs\mkfatfs.exe")
                    )
            }

            & $MkfatfsPath -t fatfs -c $DataDir -s $DataSize $DataBin
            if ($LASTEXITCODE -ne 0) {
                throw "mkfatfs failed for $synthName"
            }
        }

        $DataImageBytes = (Get-Item -LiteralPath $DataBin).Length
        if ($DataImageBytes -ne $DataSize) {
            throw "$synthName data image size is $DataImageBytes, expected $DataSize"
        }
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

    $FirmwareFileName = if ($NoData) { "$synthName-firmware-0x0-no-data.bin" } else { "$synthName-firmware-0x0.bin" }
    $FirmwareBin = Join-Path $SynthOutputDir $FirmwareFileName
    if (Test-Path -LiteralPath $FirmwareBin) {
        Remove-Item -LiteralPath $FirmwareBin -Force
    }

    $MergeArgs = @(
        "--chip", "esp32s3",
        "merge_bin",
        "-o", $FirmwareBin
    )
    if (-not $NoData) {
        $MergeArgs += @("--fill-flash-size", $FlashSizeBytesLabel)
    }
    $MergeArgs += @(
        "--flash_mode", "keep",
        "--flash_freq", "keep",
        "--flash_size", "keep",
        "0x0", $BootloaderBin,
        "0x8000", $PartitionsBin,
        "0xe000", $BootApp0,
        "0x10000", $AppBin
    )
    if (-not $NoData) {
        $MergeArgs += @($DataOffset, $DataBin)
    }

    & $Esptool @MergeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "esptool merge_bin failed for $synthName"
    }

    $DataSourceFiles = 0
    $DataSourceBytes = 0
    if (-not $NoData) {
        $DataIsEmpty = ([System.IO.Path]::GetFullPath($DataDir) -eq [System.IO.Path]::GetFullPath($EmptyDataDir))
        $DataSourceFiles = if ($DataIsEmpty) { 0 } else { @(Get-ChildItem -Path $DataDir -File -Recurse).Count }
        $DataSourceBytes = if ($DataIsEmpty) { 0 } else { (Get-ChildItem -Path $DataDir -File -Recurse | Measure-Object Length -Sum).Sum }
    }
    $FirmwareItem = Get-Item -LiteralPath $FirmwareBin
    $Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $FirmwareBin).Hash.ToLowerInvariant()

    $manifest += [PSCustomObject][ordered]@{
        synth = $synthName
        firmware = Get-RelativeRepoPath $FirmwareBin
        flash_address = "0x0"
        board = $Board
        flash_size = $FlashSizeBytesLabel
        flash_mode = $FlashMode
        cpu_freq = $CPUFreq
        arduino_flash_size = $FlashSize
        partition_scheme = $PartitionScheme
        partition_csv = $PartitionCsvName
        usb_mode = $USBMode
        cdc_on_boot = $CDCOnBoot
        upload_mode = $UploadMode
        firmware_bytes = $FirmwareItem.Length
        data_included = -not $NoData
        data_image_mode = $ResolvedDataImageMode
        data_partition = "ffat"
        data_partition_csv = if ($FatPartition) { $FatPartition.CsvName } else { $null }
        data_offset = $DataOffset
        data_image_bytes = $DataImageBytes
        data_source_files = $DataSourceFiles
        data_source_bytes = $DataSourceBytes
        sha256 = $Sha256
    }
}

$ManifestPath = Join-Path $OutputDir "manifest.json"
ConvertTo-Json -InputObject @($manifest) -Depth 4 | Set-Content -LiteralPath $ManifestPath -Encoding ASCII

Write-Host ""
Write-Host "Firmware images generated in $(Get-RelativeRepoPath $OutputDir)"
$manifest | Format-Table synth, firmware_bytes, data_included, data_offset, data_source_files, data_source_bytes, sha256 -AutoSize

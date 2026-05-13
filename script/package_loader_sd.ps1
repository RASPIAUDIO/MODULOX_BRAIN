<#
Build and package SD-card files for the Modulox loader prototype.

The generated card layout is:

  MODULOX/manifest.json
  MODULOX/wifi.json
  MODULOX/<synth>/app.bin
  MODULOX/<synth>/data.ffat.bin

The app.bin file is the application binary only, not a merged 0x0 image.

The generated server catalog layout is:

  catalog.json
  <synth>/app.bin
  <synth>/data.ffat.bin
#>
[CmdletBinding()]
param(
    [string[]] $Synth = @("FMbrain", "granulizer", "organbrain", "sampbrain", "VAsynth"),
    [string] $PackageDir = "loader-sd",
    [string] $CatalogDir = "loader-catalog",
    [string] $CatalogBaseUrl = "https://apps.raspiaudio.com/modulox",
    [string] $Version = "0.1.0",
    [int] $VersionCode = 1,
    [string] $BuildDir = ".build\loader-sd-build",
    [string] $FirmwareOutputDir = ".build\loader-sd-firmware",
    [string] $Board = "esp32s3-octal",
    [string] $FlashMode = "opi",
    [string] $FlashSize = "32M",
    [string] $CPUFreq = "240",
    [string] $PartitionScheme = "app5M_fat24M_32MB",
    [string] $USBMode = "hwcdc",
    [string] $CDCOnBoot = "cdc",
    [string] $UploadMode = "default",
    [switch] $NoTrimRawData,
    [switch] $NoBuild
)

$ErrorActionPreference = "Stop"

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

function Resolve-InRepo {
    param([string] $Path)
    $fullPath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
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

function Get-JsonArray {
    param([string] $Path)
    $json = Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
    if ($json -is [array]) { return @($json) }
    return @($json)
}

function Get-RelativeUnixPath {
    param([string] $Path)
    $repo = $RepoRoot.TrimEnd("\", "/") + "\"
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if ($fullPath.StartsWith($repo, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relative = $fullPath.Substring($repo.Length)
    } else {
        $relative = $fullPath
    }
    return ($relative -replace "\\", "/")
}

function Get-UsefulRawImageLength {
    param([string] $Path)

    $file = [System.IO.File]::OpenRead($Path)
    try {
        $bufferSize = 1024 * 1024
        $buffer = New-Object byte[] $bufferSize
        $remaining = $file.Length
        while ($remaining -gt 0) {
            $readSize = [Math]::Min($bufferSize, $remaining)
            $remaining -= $readSize
            $file.Seek($remaining, [System.IO.SeekOrigin]::Begin) | Out-Null
            $read = $file.Read($buffer, 0, $readSize)
            for ($i = $read - 1; $i -ge 0; $i--) {
                $value = $buffer[$i]
                if ($value -ne 0 -and $value -ne 255) {
                    $lastUsed = $remaining + $i + 1
                    return [int64]([Math]::Ceiling($lastUsed / 4096.0) * 4096)
                }
            }
        }

        return $file.Length
    } finally {
        $file.Close()
    }
}

function Copy-FilePrefix {
    param(
        [string] $Source,
        [string] $Destination,
        [Int64] $Length
    )

    $inputStream = [System.IO.File]::OpenRead($Source)
    try {
        $outputStream = [System.IO.File]::Create($Destination)
        try {
            $buffer = New-Object byte[] (1024 * 1024)
            $remaining = $Length
            while ($remaining -gt 0) {
                $toRead = [int][Math]::Min($buffer.Length, $remaining)
                $read = $inputStream.Read($buffer, 0, $toRead)
                if ($read -le 0) {
                    throw "Unexpected end of file while copying $Source"
                }
                $outputStream.Write($buffer, 0, $read)
                $remaining -= $read
            }
        } finally {
            $outputStream.Close()
        }
    } finally {
        $inputStream.Close()
    }
}

$PackageRoot = Resolve-InRepo $PackageDir
$CatalogRoot = Resolve-InRepo $CatalogDir
$BuildRoot = Resolve-InRepo $BuildDir
$FirmwareRoot = Resolve-InRepo $FirmwareOutputDir
$ModuloxRoot = Join-Path $PackageRoot "MODULOX"
$BuildScript = Join-Path $RepoRoot "script\build_firmwares.ps1"

if (-not $NoBuild) {
    & $BuildScript `
        -Synth $Synth `
        -Board $Board `
        -FlashMode $FlashMode `
        -FlashSize $FlashSize `
        -CPUFreq $CPUFreq `
        -PartitionScheme $PartitionScheme `
        -USBMode $USBMode `
        -CDCOnBoot $CDCOnBoot `
        -UploadMode $UploadMode `
        -OutputDir $FirmwareRoot `
        -BuildDir $BuildRoot
    if ($LASTEXITCODE -ne 0) {
        throw "build_firmwares.ps1 failed"
    }
}

Ensure-Directory $ModuloxRoot
Ensure-Directory $CatalogRoot

$buildManifestPath = Join-Path $FirmwareRoot "manifest.json"
if (-not (Test-Path -LiteralPath $buildManifestPath)) {
    throw "Build manifest not found: $buildManifestPath"
}

$buildManifest = Get-JsonArray $buildManifestPath
$machines = New-Object System.Collections.Generic.List[object]
$catalogMachines = New-Object System.Collections.Generic.List[object]
$baseUrl = $CatalogBaseUrl.TrimEnd("/")

foreach ($synthName in $Synth) {
    $entry = @($buildManifest | Where-Object { $_.synth -eq $synthName })[0]
    if (-not $entry) {
        throw "Synth '$synthName' not found in build manifest"
    }

    $synthBuildDir = Join-Path $BuildRoot $synthName
    $appBin = Join-Path $synthBuildDir "$synthName.ino.bin"
    $dataBin = Join-Path $synthBuildDir "$synthName-data.ffat.bin"
    if (-not (Test-Path -LiteralPath $appBin)) {
        throw "Application binary not found: $appBin"
    }
    if (-not (Test-Path -LiteralPath $dataBin)) {
        throw "Data image not found: $dataBin"
    }

    $sdSynthDir = Join-Path $ModuloxRoot $synthName
    $catalogSynthDir = Join-Path $CatalogRoot $synthName
    Ensure-Directory $sdSynthDir
    Ensure-Directory $catalogSynthDir
    $sdAppBin = Join-Path $sdSynthDir "app.bin"
    $sdDataBin = Join-Path $sdSynthDir "data.ffat.bin"
    $catalogAppBin = Join-Path $catalogSynthDir "app.bin"
    $catalogDataBin = Join-Path $catalogSynthDir "data.ffat.bin"
    Copy-Item -LiteralPath $appBin -Destination $sdAppBin -Force
    $dataCopyLength = (Get-Item -LiteralPath $dataBin).Length
    if (-not $NoTrimRawData -and $entry.data_image_mode -eq "raw") {
        $trimmedLength = Get-UsefulRawImageLength $dataBin
        if ($trimmedLength -gt 0 -and $trimmedLength -lt $dataCopyLength) {
            Write-Host "Trimming raw data image for $synthName from $dataCopyLength to $trimmedLength bytes"
            $dataCopyLength = $trimmedLength
        }
    }

    Copy-FilePrefix -Source $dataBin -Destination $sdDataBin -Length $dataCopyLength
    Copy-Item -LiteralPath $appBin -Destination $catalogAppBin -Force
    Copy-FilePrefix -Source $dataBin -Destination $catalogDataBin -Length $dataCopyLength

    $appItem = Get-Item -LiteralPath $sdAppBin
    $dataItem = Get-Item -LiteralPath $sdDataBin
    $appSha = (Get-FileHash -Algorithm SHA256 -LiteralPath $sdAppBin).Hash.ToLowerInvariant()
    $dataSha = (Get-FileHash -Algorithm SHA256 -LiteralPath $sdDataBin).Hash.ToLowerInvariant()

    [void]$machines.Add([PSCustomObject][ordered]@{
        id = $synthName
        name = $synthName
        version = $Version
        version_code = $VersionCode
        app = [PSCustomObject][ordered]@{
            path = "/MODULOX/$synthName/app.bin"
            size = $appItem.Length
            sha256 = $appSha
        }
        data = [PSCustomObject][ordered]@{
            path = "/MODULOX/$synthName/data.ffat.bin"
            size = $dataItem.Length
            sha256 = $dataSha
            partition = "ffat"
            offset = $entry.data_offset
            image_mode = $entry.data_image_mode
        }
    })

    [void]$catalogMachines.Add([PSCustomObject][ordered]@{
        id = $synthName
        name = $synthName
        version = $Version
        version_code = $VersionCode
        app = [PSCustomObject][ordered]@{
            url = "$baseUrl/$synthName/app.bin"
            size = $appItem.Length
            sha256 = $appSha
        }
        data = [PSCustomObject][ordered]@{
            url = "$baseUrl/$synthName/data.ffat.bin"
            size = $dataItem.Length
            sha256 = $dataSha
            partition = "ffat"
            offset = $entry.data_offset
            image_mode = $entry.data_image_mode
        }
    })
}

$manifest = [PSCustomObject][ordered]@{
    version = 1
    format = "modulox-loader-sd-v1"
    target = "ESP32-S3-WROOM-2-N32R16V"
    partition_scheme = $PartitionScheme
    partition_csv = "large_fat_32MB"
    flash_size = "32MB"
    machines = $machines.ToArray()
}

$manifestPath = Join-Path $ModuloxRoot "manifest.json"
ConvertTo-Json -InputObject $manifest -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding ASCII

$wifiConfigPath = Join-Path $ModuloxRoot "wifi.json"
if (-not (Test-Path -LiteralPath $wifiConfigPath)) {
    $wifiConfig = [PSCustomObject][ordered]@{
        ssid = ""
        password = ""
    }
    ConvertTo-Json -InputObject $wifiConfig -Depth 3 | Set-Content -LiteralPath $wifiConfigPath -Encoding ASCII
}

$catalog = [PSCustomObject][ordered]@{
    version = 1
    format = "modulox-loader-catalog-v1"
    target = "ESP32-S3-WROOM-2-N32R16V"
    partition_scheme = $PartitionScheme
    partition_csv = "large_fat_32MB"
    flash_size = "32MB"
    machines = $catalogMachines.ToArray()
}

$catalogPath = Join-Path $CatalogRoot "catalog.json"
ConvertTo-Json -InputObject $catalog -Depth 8 | Set-Content -LiteralPath $catalogPath -Encoding ASCII

Write-Host ""
Write-Host "SD loader package generated in $(Get-RelativeUnixPath $PackageRoot)"
Get-ChildItem -Path $ModuloxRoot -Recurse -File |
    Select-Object @{Name="file";Expression={Get-RelativeUnixPath $_.FullName}}, Length |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Server catalog package generated in $(Get-RelativeUnixPath $CatalogRoot)"
Get-ChildItem -Path $CatalogRoot -Recurse -File |
    Select-Object @{Name="file";Expression={Get-RelativeUnixPath $_.FullName}}, Length |
    Format-Table -AutoSize

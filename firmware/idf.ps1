$ErrorActionPreference = 'Stop'

# Keep forwarding arbitrary idf.py arguments, including -D options.
$buildArgs = @($args)
$model = 's3-16m8m'
for ($i = 0; $i -lt $buildArgs.Count; $i++) {
    if ($buildArgs[$i] -eq '-Model') {
        if ($i + 1 -ge $buildArgs.Count) { throw 'Valeur manquante pour -Model.' }
        $model = $buildArgs[$i + 1]
        $buildArgs = @($buildArgs | Select-Object -First $i) + @($buildArgs | Select-Object -Skip ($i + 2))
        break
    }
}
if ($model -notin @('s3-16m8m', 's3-32m16m')) { throw 'Modèle inconnu : utilise s3-16m8m ou s3-32m16m.' }
$profileArgs = @('-D', "MODULOX_MODEL=$model")
if ($model -ne 's3-16m8m') {
    $profileArgs += @('-B', "build-$model", '-D', "SDKCONFIG=$PSScriptRoot/sdkconfig.$model")
}

$idfDirectory = if ($env:IDF_PATH) {
    $env:IDF_PATH
} else {
    Join-Path $env:USERPROFILE 'esp\v5.4.1\esp-idf'
}
$pythonDirectory = if ($env:IDF_PYTHON_ENV_PATH) {
    $env:IDF_PYTHON_ENV_PATH
} else {
    $toolsDirectory = if ($env:IDF_TOOLS_PATH) { $env:IDF_TOOLS_PATH } else {
        Join-Path $env:USERPROFILE '.espressif'
    }
    Join-Path $toolsDirectory 'python_env\idf5.4_py3.11_env'
}

$python = Join-Path $pythonDirectory 'Scripts\python.exe'
$export = Join-Path $idfDirectory 'export.ps1'
if (!(Test-Path -LiteralPath $python) -or !(Test-Path -LiteralPath $export)) {
    throw 'ESP-IDF ou son environnement Python est introuvable. Vérifie IDF_PATH et IDF_PYTHON_ENV_PATH.'
}

$env:IDF_PYTHON_ENV_PATH = $pythonDirectory
$env:Path = "$(Split-Path -Parent $python);$env:Path"
. $export

Push-Location -LiteralPath $PSScriptRoot
try {
    & $python (Join-Path $idfDirectory 'tools\idf.py') @profileArgs @buildArgs
    $idfExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $idfExitCode

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Enter-WristFoundryIdf {
    $script:WristFoundryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

    # The official Windows offline installer keeps tools and its Python
    # environment here. A plain PowerShell session otherwise defaults to
    # $HOME\.espressif and cannot find the installed virtual environment.
    if (Test-Path -LiteralPath 'C:\Espressif\python_env') {
        $env:IDF_TOOLS_PATH = 'C:\Espressif'
        $installedPython = 'C:\Espressif\tools\idf-python\3.11.2'
        $installedEnv = 'C:\Espressif\python_env\idf5.5_py3.11_env'
        if ((Test-Path -LiteralPath (Join-Path $installedPython 'python.exe')) -and
            (Test-Path -LiteralPath (Join-Path $installedEnv 'Scripts\python.exe'))) {
            $env:IDF_PYTHON_ENV_PATH = $installedEnv
            $env:Path = "$installedPython;$env:Path"
        }
    }

    $candidates = @()
    if ($env:IDF_PATH) {
        $candidates += $env:IDF_PATH
    }
    $candidates += 'C:\Espressif\frameworks\esp-idf-v5.5.5'

    $frameworkRoot = 'C:\Espressif\frameworks'
    if (Test-Path -LiteralPath $frameworkRoot) {
        $candidates += Get-ChildItem -LiteralPath $frameworkRoot -Directory -Filter 'esp-idf-v5.5*' |
            Sort-Object Name -Descending |
            Select-Object -ExpandProperty FullName
    }

    $idfPath = $candidates |
        Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $_ 'export.ps1')) } |
        Select-Object -First 1

    if (-not $idfPath) {
        throw 'ESP-IDF 5.5.x was not found. Install the official Espressif ESP-IDF 5.5.5 tools package first.'
    }

    $exportScript = Join-Path $idfPath 'export.ps1'
    . $exportScript
    Set-Location -LiteralPath $script:WristFoundryRoot

    Write-Host "WristFoundry root: $script:WristFoundryRoot"
    Write-Host "ESP-IDF: $env:IDF_PATH"
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)] [string] $Command,
        [Parameter(ValueFromRemainingArguments)] [string[]] $Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

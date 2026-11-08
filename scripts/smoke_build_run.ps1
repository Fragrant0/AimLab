[CmdletBinding()]
param(
    [ValidateRange(1, 300)]
    [int]$RunSeconds = 15,
    [string[]]$DependencyIncludeDirectories = @($env:AIMLAB_INCLUDE_DIRS),
    [string[]]$DependencyLibraryDirectories = @($env:AIMLAB_LIBRARY_DIRS),
    [string]$MSBuildPath
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $PSScriptRoot "build.ps1"
$exePath = Join-Path $repoRoot "x64\Debug\opengl_try.exe"
$logDir = Join-Path $repoRoot "tmp"
$logPath = Join-Path $logDir "smoke_run.log"
$errPath = Join-Path $logDir "smoke_run.err.log"

if (!(Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir | Out-Null
}

Write-Host "[smoke] Building Debug x64..."
$originalPath = $env:PATH
$process = $null
Push-Location $repoRoot
try {
    & $buildScript -Configuration Debug `
        -DependencyIncludeDirectories $DependencyIncludeDirectories `
        -DependencyLibraryDirectories $DependencyLibraryDirectories `
        -MSBuildPath $MSBuildPath

    if (!(Test-Path $exePath)) {
        throw "Executable not found: $exePath"
    }

    Remove-Item -LiteralPath $logPath, $errPath -ErrorAction SilentlyContinue

    $env:PATH = "$(Join-Path $repoRoot 'DLL');$originalPath"
    Write-Host "[smoke] Running for $RunSeconds seconds..."
    $process = Start-Process -FilePath $exePath `
                             -WorkingDirectory $repoRoot `
                             -RedirectStandardOutput $logPath `
                             -RedirectStandardError $errPath `
                             -WindowStyle Hidden `
                             -PassThru

    if ($process.WaitForExit($RunSeconds * 1000)) {
        throw "Smoke process exited early (exit code $($process.ExitCode)). Inspect $logPath and $errPath."
    }

    Stop-Process -Id $process.Id -Force
    $process.WaitForExit()

    $log = ""
    if (Test-Path $logPath) {
        $log = Get-Content -LiteralPath $logPath -Raw
    }

    $requiredMarkers = @(
        "[MapManager] Loaded",
        "[IBL] Precomputation complete.",
        "[Model] Loaded resources/objects/gun/gun.obj",
        "[FontRenderer] Initialized"
    )

    foreach ($marker in $requiredMarkers) {
        if (!$log.Contains($marker)) {
            throw "Smoke run did not emit expected marker: $marker"
        }
    }

    $maps = Get-Content -LiteralPath (Join-Path $repoRoot "resources\maps_config.json") -Raw | ConvertFrom-Json
    $propModels = @($maps.maps | ForEach-Object { $_.props } | ForEach-Object { $_.model } | Where-Object { $_ })
    if ($propModels.Count -gt 0) {
        $loadedProp = $propModels | Where-Object { $log.Contains("[Model] Loaded $_") } | Select-Object -First 1
        if (!$loadedProp) {
            throw "Smoke run did not load any configured scene model. Inspect $logPath."
        }
    }

    $stderr = if (Test-Path -LiteralPath $errPath) { Get-Content -LiteralPath $errPath -Raw } else { "" }
    $errors = ($log + "`n" + $stderr) -split "\r?\n" | Where-Object {
        $_ -match "ERROR::|\[FATAL\]|failed to|not found:|framebuffer.*(?:incomplete|not complete)"
    }
    if ($errors) {
        throw "Smoke run reported errors:`n$($errors -join "`n")"
    }

    Write-Host "[smoke] Passed. Log: $logPath"
}
finally {
    if ($process -and !$process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    $env:PATH = $originalPath
    Pop-Location
}

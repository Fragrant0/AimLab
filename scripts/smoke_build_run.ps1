param(
    [int]$RunSeconds = 15
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $repoRoot "vibe_build.bat"
$exePath = Join-Path $repoRoot "x64\Debug\opengl_try.exe"
$logDir = Join-Path $repoRoot "tmp"
$logPath = Join-Path $logDir "smoke_run.log"
$errPath = Join-Path $logDir "smoke_run.err.log"

if (!(Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir | Out-Null
}

Write-Host "[smoke] Building Debug x64..."
Push-Location $repoRoot
try {
    & cmd /c "set PATH=& `"$buildScript`""
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    if (!(Test-Path $exePath)) {
        throw "Executable not found: $exePath"
    }

    Remove-Item -LiteralPath $logPath, $errPath -ErrorAction SilentlyContinue

    Write-Host "[smoke] Running for $RunSeconds seconds..."
    $process = Start-Process -FilePath $exePath `
                             -WorkingDirectory $repoRoot `
                             -RedirectStandardOutput $logPath `
                             -RedirectStandardError $errPath `
                             -PassThru

    Start-Sleep -Seconds $RunSeconds

    if (!$process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }

    $log = ""
    if (Test-Path $logPath) {
        $log = Get-Content -LiteralPath $logPath -Raw
    }

    $requiredMarkers = @(
        "[MapManager] Loaded",
        "[IBL] Precomputation complete.",
        "[Model] Loaded resources/objects/avocado/Avocado.gltf",
        "[Model] Loaded resources/objects/gun/gun.obj",
        "[FontRenderer] Initialized"
    )

    foreach ($marker in $requiredMarkers) {
        if (!$log.Contains($marker)) {
            throw "Smoke run did not emit expected marker: $marker"
        }
    }

    Write-Host "[smoke] Passed. Log: $logPath"
}
finally {
    Pop-Location
}

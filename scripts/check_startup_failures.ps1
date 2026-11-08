[CmdletBinding()]
param(
    [string]$ExecutablePath,
    [ValidateRange(1, 60)]
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if (!$ExecutablePath) {
    $ExecutablePath = Join-Path $repoRoot "x64\Debug\opengl_try.exe"
}
if (!(Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
    throw "Executable not found: $ExecutablePath. Run scripts/build.ps1 first."
}
$ExecutablePath = (Resolve-Path -LiteralPath $ExecutablePath).ProviderPath
$fixtureRoot = Join-Path $repoRoot ("tmp\startup-failures-" + [guid]::NewGuid().ToString("N"))
$fixtureShaders = Join-Path $fixtureRoot "shaders"
$fixtureResources = Join-Path $fixtureRoot "resources"
$utf8 = New-Object System.Text.UTF8Encoding($false)

# Use physical copies only. The small flat scene still initializes every render stage.
New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "shaders") -Destination $fixtureRoot -Recurse
foreach ($directory in @("objects", "textures\ground", "textures\skybox_hdr")) {
    New-Item -ItemType Directory -Path (Join-Path $fixtureResources $directory) -Force | Out-Null
}
Copy-Item -LiteralPath (Join-Path $repoRoot "resources\objects\gun") `
    -Destination (Join-Path $fixtureResources "objects") -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot "resources\fonts") -Destination $fixtureResources -Recurse
foreach ($asset in @("textures\ground\sand.jpg", "textures\skybox_hdr\underwater_hdr.hdr")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "resources\$asset") `
        -Destination (Join-Path $fixtureResources $asset)
}
$map = @{
    name = "Startup Fixture"
    skybox_type = "hdr"
    skybox_hdr = "resources/textures/skybox_hdr/underwater_hdr.hdr"
    floor_texture = "resources/textures/ground/sand.jpg"
    floor_texture_name = "fixture_ground"
    terrain = "flat"
    ecology_enabled = $false
    player_start = @(0.0, 1.5, 3.0)
    props = @()
}
[System.IO.File]::WriteAllText((Join-Path $fixtureResources "maps_config.json"),
    (@{ maps = @($map) } | ConvertTo-Json -Depth 5), $utf8)

function Invoke-CaptureCase([string]$Name) {
    $outputPath = Join-Path $fixtureRoot "$Name.ppm"
    $stdoutPath = Join-Path $fixtureRoot "$Name.stdout.log"
    $stderrPath = Join-Path $fixtureRoot "$Name.stderr.log"
    $process = $null
    try {
        $process = Start-Process -FilePath $ExecutablePath `
            -ArgumentList @("--capture", "0", "$Name.ppm") `
            -WorkingDirectory $fixtureRoot `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath `
            -WindowStyle Hidden -PassThru
        # Windows PowerShell may otherwise release the native handle before ExitCode is read.
        $processHandle = $process.Handle
        if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
            throw "Case '$Name' timed out after $TimeoutSeconds seconds. Logs: $fixtureRoot"
        }
        $process.WaitForExit()
        $exitCode = $process.ExitCode
        if ($null -eq $exitCode) {
            throw "Case '$Name' did not expose an exit code (process handle $processHandle). Logs: $fixtureRoot"
        }
        $stdout = [System.IO.File]::ReadAllText($stdoutPath)
        $stderr = [System.IO.File]::ReadAllText($stderrPath)
        return [pscustomobject]@{
            ExitCode = $exitCode
            Log = $stdout + "`n" + $stderr
            OutputPath = $outputPath
        }
    }
    finally {
        if ($process -and !$process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
        if ($process) { $process.Dispose() }
    }
}

$invalidVertex = "#version 330 core`nvoid main() { gl_Position = vec4(fixture_unknown_identifier); }`n"
$invalidFragment = "#version 330 core`nout vec4 FragColor;`nvoid main() { FragColor = vec4(fixture_unknown_identifier); }`n"
$linkVertex = "#version 330 core`nlayout(location = 0) in vec3 aPos;`nout vec3 fixtureColor;`nvoid main() { fixtureColor = aPos; gl_Position = vec4(aPos, 1.0); }`n"
$linkFragment = "#version 330 core`nin vec2 fixtureColor;`nout vec4 FragColor;`nvoid main() { FragColor = vec4(fixtureColor, 0.0, 1.0); }`n"
$cases = @(
    @{ Name = "missing-vertex"; Files = @{ "pbr_model.vs" = $null }; Expected = "[Shader] VERTEX file read failed (shaders/pbr_model.vs)" },
    @{ Name = "missing-fragment"; Files = @{ "pbr_model.fs" = $null }; Expected = "[Shader] FRAGMENT file read failed (shaders/pbr_model.fs)" },
    @{ Name = "invalid-vertex"; Files = @{ "pbr_model.vs" = $invalidVertex }; Expected = "[Shader] VERTEX compile failed (shaders/pbr_model.vs)" },
    @{ Name = "invalid-fragment"; Files = @{ "pbr_model.fs" = $invalidFragment }; Expected = "[Shader] FRAGMENT compile failed (shaders/pbr_model.fs)" },
    @{ Name = "link-mismatch"; Files = @{ "pbr_model.vs" = $linkVertex; "pbr_model.fs" = $linkFragment }; Expected = "[Shader] PROGRAM link failed (shaders/pbr_model.vs + shaders/pbr_model.fs)" },
    @{ Name = "late-ibl"; Files = @{ "prefilter.fs" = $invalidFragment }; Expected = "[Shader] FRAGMENT compile failed (shaders/prefilter.fs)" },
    @{ Name = "late-font"; Files = @{ "font.fs" = $null }; Expected = "[Shader] FRAGMENT file read failed (shaders/font.fs)" },
    @{ Name = "late-post-process"; Files = @{ "post_process.fs" = $null }; Expected = "[Shader] FRAGMENT file read failed (shaders/post_process.fs)" }
)

$originalPath = $env:PATH
try {
    $env:PATH = "$(Join-Path $repoRoot 'DLL');$originalPath"
    Write-Host "[startup] Fixture and retained logs: $fixtureRoot"
    $baseline = Invoke-CaptureCase "baseline"
    if ($baseline.ExitCode -ne 0 -or !$baseline.Log.Contains("[Capture] Saved") -or
        !(Test-Path -LiteralPath $baseline.OutputPath) -or
        (Get-Item -LiteralPath $baseline.OutputPath).Length -lt 100 -or
        $baseline.Log -match "\[FATAL\]|ERROR::|\[Capture\] OpenGL error") {
        throw "Positive baseline failed (exit $($baseline.ExitCode)); inspect $fixtureRoot before interpreting failure cases."
    }
    Write-Host "[startup] PASS baseline (exit 0)"

    foreach ($case in $cases) {
        $backups = @{}
        try {
            foreach ($fileName in $case.Files.Keys) {
                $filePath = Join-Path $fixtureShaders $fileName
                $backups[$fileName] = [System.IO.File]::ReadAllBytes($filePath)
                if ($null -eq $case.Files[$fileName]) {
                    Remove-Item -LiteralPath $filePath
                }
                else {
                    [System.IO.File]::WriteAllText($filePath, $case.Files[$fileName], $utf8)
                }
            }
            $result = Invoke-CaptureCase $case.Name
            if ($result.ExitCode -ne 1 -or !$result.Log.Contains("[FATAL]") -or
                !$result.Log.Contains($case.Expected) -or $result.Log.Contains("[Capture] Saved") -or
                (Test-Path -LiteralPath $result.OutputPath)) {
                throw "Case '$($case.Name)' failed its exit/diagnostic assertions (exit $($result.ExitCode)); expected '$($case.Expected)'. Logs: $fixtureRoot"
            }
            Write-Host "[startup] PASS $($case.Name) (exit $($result.ExitCode))"
        }
        finally {
            foreach ($fileName in $backups.Keys) {
                [System.IO.File]::WriteAllBytes((Join-Path $fixtureShaders $fileName), $backups[$fileName])
            }
        }
    }
    Write-Host "[startup] Passed baseline and $($cases.Count) failure cases. Fixtures and logs retained: $fixtureRoot"
}
finally {
    $env:PATH = $originalPath
}

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

function New-MapConfiguration([hashtable]$Overrides) {
    $candidateMap = @{}
    foreach ($key in $map.Keys) { $candidateMap[$key] = $map[$key] }
    foreach ($key in $Overrides.Keys) { $candidateMap[$key] = $Overrides[$key] }
    return (@{ maps = @($candidateMap) } | ConvertTo-Json -Depth 8)
}

$baselineConfiguration = New-MapConfiguration @{}
$configurationCases = @(
    @{ Name = "missing-json"; Configuration = $null; Expected = "[MapManager] Could not open JSON config: resources/maps_config.json" },
    @{ Name = "malformed-json"; Configuration = '{"maps": ['; Expected = "[MapManager] Could not parse JSON config resources/maps_config.json" },
    @{ Name = "trailing-json"; Configuration = $baselineConfiguration + " trailing garbage"; Expected = "[MapManager] Could not parse JSON config resources/maps_config.json" },
    @{ Name = "empty-maps"; Configuration = '{"maps": []}'; Expected = "resources/maps_config.json.maps: expected a non-empty maps array" },
    @{ Name = "non-array-maps"; Configuration = '{"maps": {}}'; Expected = "resources/maps_config.json.maps: expected an array" },
    @{ Name = "bad-map-type"; Configuration = '{"maps": [42]}'; Expected = "maps[0]: expected a map object" },
    @{ Name = "unknown-terrain"; Configuration = (New-MapConfiguration @{ terrain = "fixture-unknown" }); Expected = "maps[0].terrain: expected 'flat' or 'heightmap'" },
    @{ Name = "unknown-skybox"; Configuration = (New-MapConfiguration @{ skybox_type = "fixture-unknown" }); Expected = "maps[0].skybox_type: expected 'hdr' or 'cubemap'" },
    @{ Name = "zero-direction"; Configuration = (New-MapConfiguration @{ lighting = @{ main_light = @{ direction = @(0.0, 0.0, 0.0) } } }); Expected = "maps[0].lighting.main_light.direction: must be a non-zero direction vector" },
    @{ Name = "float-overflow"; Configuration = (New-MapConfiguration @{ post_process = @{ exposure = 1e100 } }); Expected = "maps[0].post_process.exposure: expected a finite number representable as float" },
    @{ Name = "heightmap-grid-zero"; Configuration = (New-MapConfiguration @{ terrain = "heightmap"; heightmap = @{ path = "unused-heightmap.png"; grid_size = 0; size = 20.0 } }); Expected = "maps[0].heightmap.grid_size: must be an integer in [1, 2048]" },
    @{ Name = "heightmap-grid-overbudget"; Configuration = (New-MapConfiguration @{ terrain = "heightmap"; heightmap = @{ path = "unused-heightmap.png"; grid_size = 2049; size = 20.0 } }); Expected = "maps[0].heightmap.grid_size: must be an integer in [1, 2048]" },
    @{ Name = "heightmap-grid-fraction"; Configuration = (New-MapConfiguration @{ terrain = "heightmap"; heightmap = @{ path = "unused-heightmap.png"; grid_size = 1.5; size = 20.0 } }); Expected = "maps[0].heightmap.grid_size: must be an integer in [1, 2048]" },
    @{ Name = "heightmap-size-zero"; Configuration = (New-MapConfiguration @{ terrain = "heightmap"; heightmap = @{ path = "unused-heightmap.png"; grid_size = 16; size = 0.0 } }); Expected = "maps[0].heightmap.size: must be greater than zero" },
    @{ Name = "zero-scale"; Configuration = (New-MapConfiguration @{ props = @(@{ model = "resources/objects/gun/gun.obj"; scale = @(1.0, 0.0, 1.0) }) }); Expected = "maps[0].props[0].scale: all scale components must be non-zero" },
    @{ Name = "missing-ground"; Configuration = (New-MapConfiguration @{ floor_texture = "resources/textures/ground/fixture_missing.jpg" }); Expected = "[ResourceManager] Failed to load image 'resources/textures/ground/fixture_missing.jpg'" },
    @{ Name = "missing-hdr"; Configuration = (New-MapConfiguration @{ skybox_hdr = "resources/textures/skybox_hdr/fixture_missing.hdr" }); Expected = "[ResourceManager] Failed to load image 'resources/textures/skybox_hdr/fixture_missing.hdr'" },
    @{ Name = "missing-heightmap"; Configuration = (New-MapConfiguration @{ terrain = "heightmap"; heightmap = @{ path = "resources/textures/fixture_missing.png"; grid_size = 16; size = 20.0 } }); Expected = "[Terrain] Could not load heightmap: resources/textures/fixture_missing.png" },
    @{ Name = "missing-prop"; Configuration = (New-MapConfiguration @{ props = @(@{ model = "resources/objects/fixture_missing.obj" }) }); Expected = "Assimp import failed for 'resources/objects/fixture_missing.obj'" }
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
    $configurationPath = Join-Path $fixtureResources "maps_config.json"
    $configurationBackup = [System.IO.File]::ReadAllBytes($configurationPath)
    foreach ($case in $configurationCases) {
        try {
            if ($null -eq $case.Configuration) {
                Remove-Item -LiteralPath $configurationPath
            }
            else {
                [System.IO.File]::WriteAllText($configurationPath, $case.Configuration, $utf8)
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
            [System.IO.File]::WriteAllBytes($configurationPath, $configurationBackup)
        }
    }
    Write-Host "[startup] Passed baseline and $($cases.Count + $configurationCases.Count) failure cases. Fixtures and logs retained: $fixtureRoot"
}
finally {
    $env:PATH = $originalPath
}

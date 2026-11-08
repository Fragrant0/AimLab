[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [ValidateRange(10, 600)]
    [int]$TimeoutSeconds = 180,
    [string[]]$DependencyIncludeDirectories = @($env:AIMLAB_INCLUDE_DIRS),
    [string[]]$DependencyLibraryDirectories = @($env:AIMLAB_LIBRARY_DIRS),
    [string]$MSBuildPath
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$executablePath = Join-Path $repoRoot "tmp\map-loading-tests\map_loading_tests.exe"
if (!$SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Configuration Debug `
        -ProjectPath "tests\map_loading_tests.vcxproj" `
        -DependencyIncludeDirectories $DependencyIncludeDirectories `
        -DependencyLibraryDirectories $DependencyLibraryDirectories `
        -MSBuildPath $MSBuildPath
}
if (!(Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "Test executable not found: $executablePath. Run without -SkipBuild first."
}

$fixtureRoot = Join-Path $repoRoot ("tmp\map-loading-fixture-" + [guid]::NewGuid().ToString("N"))
$fixtureResources = Join-Path $fixtureRoot "resources"
$utf8 = New-Object System.Text.UTF8Encoding($false)
New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "shaders") -Destination $fixtureRoot -Recurse
foreach ($directory in @("objects", "objects\valid", "objects\broken-material", "textures\ground", "textures\skybox_hdr")) {
    New-Item -ItemType Directory -Path (Join-Path $fixtureResources $directory) -Force | Out-Null
}
# Physical copies keep shader/material discovery isolated from the working tree.
Copy-Item -LiteralPath (Join-Path $repoRoot "resources\objects\gun") `
    -Destination (Join-Path $fixtureResources "objects") -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot "resources\fonts") -Destination $fixtureResources -Recurse
foreach ($asset in @("textures\ground\sand.jpg", "textures\skybox_hdr\underwater_hdr.hdr")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "resources\$asset") `
        -Destination (Join-Path $fixtureResources $asset)
}
[System.IO.File]::WriteAllText((Join-Path $fixtureResources "textures\ground\corrupt.jpg"),
    "This is not an image.", $utf8)
[System.IO.File]::WriteAllText((Join-Path $fixtureResources "objects\corrupt.obj"),
    "This is not an OBJ mesh.", $utf8)
$triangle = "v -0.5 0 0`nv 0.5 0 0`nv 0 1 0`nvt 0 0`nvt 1 0`nvt 0.5 1`nvn 0 0 1`nf 1/1/1 2/2/1 3/3/1`n"
[System.IO.File]::WriteAllText((Join-Path $fixtureResources "objects\valid\triangle.obj"), $triangle, $utf8)
$brokenTriangle = "mtllib broken.mtl`nusemtl missing_texture`n" + $triangle
[System.IO.File]::WriteAllText((Join-Path $fixtureResources "objects\broken-material\triangle.obj"), $brokenTriangle, $utf8)
[System.IO.File]::WriteAllText((Join-Path $fixtureResources "objects\broken-material\broken.mtl"),
    "newmtl missing_texture`nKd 1 1 1`nmap_Kd definitely_missing_color.png`n", $utf8)

function New-FixtureMap([string]$Name, [double[]]$Spawn) {
    return @{
        name = $Name
        skybox_type = "hdr"
        skybox_hdr = "resources/textures/skybox_hdr/underwater_hdr.hdr"
        floor_texture = "resources/textures/ground/sand.jpg"
        floor_texture_name = "fixture_ground"
        terrain = "flat"
        ecology_enabled = $false
        player_start = $Spawn
        props = @()
    }
}

$replacement = New-FixtureMap "Replacement Fixture" @(2.0, 1.5, 4.0)
$baseline = New-FixtureMap "Baseline Fixture" @(0.0, 1.5, 3.0)
$missingFloor = New-FixtureMap "Missing Floor Fixture" @(10.0, 1.5, 10.0)
$missingFloor.floor_texture = "resources/textures/ground/definitely_missing.jpg"
$corruptFloor = New-FixtureMap "Corrupt Floor Fixture" @(11.0, 1.5, 11.0)
$corruptFloor.floor_texture = "resources/textures/ground/corrupt.jpg"
$missingHDR = New-FixtureMap "Missing HDR Fixture" @(12.0, 1.5, 12.0)
$missingHDR.skybox_hdr = "resources/textures/skybox_hdr/definitely_missing.hdr"
$missingModel = New-FixtureMap "Missing Model Fixture" @(13.0, 1.5, 13.0)
$missingModel.props = @(
    @{ model = "resources/objects/valid/triangle.obj" },
    @{ model = "resources/objects/definitely_missing.obj" }
)
$corruptModel = New-FixtureMap "Corrupt Model Fixture" @(14.0, 1.5, 14.0)
$corruptModel.props = @(@{ model = "resources/objects/corrupt.obj" })
$missingMaterialTexture = New-FixtureMap "Missing Material Texture Fixture" @(15.0, 1.5, 15.0)
$missingMaterialTexture.props = @(@{ model = "resources/objects/broken-material/triangle.obj" })
$missingHeightmap = New-FixtureMap "Missing Heightmap Fixture" @(16.0, 1.5, 16.0)
$missingHeightmap.terrain = "heightmap"
$missingHeightmap.heightmap = @{
    path = "resources/textures/heightmaps/definitely_missing.png"
    grid_size = 8
    size = 20.0
    height_scale = 2.0
}
$maps = @($replacement, $baseline, $missingFloor, $corruptFloor, $missingHDR,
    $missingModel, $corruptModel, $missingMaterialTexture, $missingHeightmap)
[System.IO.File]::WriteAllText((Join-Path $fixtureResources "maps_config.json"),
    (@{ maps = $maps } | ConvertTo-Json -Depth 8), $utf8)

$stdoutPath = Join-Path $fixtureRoot "map-loading.stdout.log"
$stderrPath = Join-Path $fixtureRoot "map-loading.stderr.log"
$originalPath = $env:PATH
$process = $null
try {
    $env:PATH = "$(Join-Path $repoRoot 'DLL');$originalPath"
    Write-Host "[map-loading] Fixture and retained logs: $fixtureRoot"
    $process = Start-Process -FilePath $executablePath -WorkingDirectory $fixtureRoot `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath `
        -WindowStyle Hidden -PassThru
    $processHandle = $process.Handle
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        throw "Map loading tests timed out after $TimeoutSeconds seconds. Logs: $fixtureRoot"
    }
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    if ($null -eq $exitCode) {
        throw "Map loading tests did not expose an exit code (process handle $processHandle). Logs: $fixtureRoot"
    }
    $stdout = [System.IO.File]::ReadAllText($stdoutPath)
    $stderr = [System.IO.File]::ReadAllText($stderrPath)
    if ($exitCode -ne 0 -or !$stdout.Contains("[map-loading] ALL PASS:") -or
        ($stdout + "`n" + $stderr) -match "\[map-loading\] FAIL:|\[FATAL\]") {
        throw "Map loading tests failed (exit $exitCode).`n$stderr`nLogs: $fixtureRoot"
    }
    $stdout -split "\r?\n" | Where-Object { $_ -like "[[]map-loading[]]*" } | ForEach-Object { Write-Host $_ }
    Write-Host "[map-loading] Passed. Fixtures and logs retained: $fixtureRoot"
}
finally {
    if ($process -and !$process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    if ($process) { $process.Dispose() }
    $env:PATH = $originalPath
}

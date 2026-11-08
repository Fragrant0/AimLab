[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string[]]$DependencyIncludeDirectories = @($env:AIMLAB_INCLUDE_DIRS),
    [string[]]$DependencyLibraryDirectories = @($env:AIMLAB_LIBRARY_DIRS),
    [string]$MSBuildPath,
    [string]$ProjectPath = "opengl_try.sln"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if (![System.IO.Path]::IsPathRooted($ProjectPath)) {
    $ProjectPath = Join-Path $repoRoot $ProjectPath
}
if (!(Test-Path -LiteralPath $ProjectPath -PathType Leaf)) {
    throw "Build project does not exist: $ProjectPath"
}
$ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).ProviderPath

function Resolve-DependencyDirectories([string[]]$Directories) {
    $resolved = foreach ($entry in $Directories) {
        foreach ($directory in ($entry -split ";")) {
            if (![string]::IsNullOrWhiteSpace($directory)) {
                if (!(Test-Path -LiteralPath $directory -PathType Container)) {
                    throw "Dependency directory does not exist: $directory"
                }
                (Resolve-Path -LiteralPath $directory).ProviderPath
            }
        }
    }
    return ($resolved -join ";")
}

if (!$MSBuildPath) {
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        $MSBuildPath = $command.Source
    }
    else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path -LiteralPath $vswhere) {
            $MSBuildPath = & $vswhere -latest -products * -version "[17.0,18.0)" `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -find "MSBuild\Current\Bin\MSBuild.exe" | Select-Object -First 1
        }
    }
}

if (!$MSBuildPath -or !(Test-Path -LiteralPath $MSBuildPath -PathType Leaf)) {
    throw "MSBuild was not found. Install Visual Studio 2022 with Desktop development with C++, or pass -MSBuildPath."
}

$includeDirectories = Resolve-DependencyDirectories $DependencyIncludeDirectories
$libraryDirectories = Resolve-DependencyDirectories $DependencyLibraryDirectories
$arguments = @(
    $ProjectPath,
    "/m", "/nologo", "/verbosity:minimal",
    "/p:Configuration=$Configuration", "/p:Platform=x64", "/p:PreferredToolArchitecture=x64"
)

Write-Host "[build] Building $Configuration x64 with $MSBuildPath"
$originalPath = $env:PATH
$originalBuildIncludeDirectories = $env:AimLabDependencyIncludeDirs
$originalBuildLibraryDirectories = $env:AimLabDependencyLibraryDirs
try {
    # Environment properties preserve directory lists without MSBuild CLI escaping.
    $env:AimLabDependencyIncludeDirs = $includeDirectories
    $env:AimLabDependencyLibraryDirs = $libraryDirectories
    # Put the selected VS installation's matching compiler/PDB tools first.
    $toolsDirectory = (Get-Item -LiteralPath $MSBuildPath).Directory
    $vcvars = $null
    while ($toolsDirectory) {
        $candidate = Join-Path $toolsDirectory.FullName "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path -LiteralPath $candidate) {
            $vcvars = $candidate
            break
        }
        $toolsDirectory = $toolsDirectory.Parent
    }
    if ($vcvars) {
        $initialize = 'call "{0}" >nul 2>&1 && set PATH' -f $vcvars
        $pathOutput = & "$env:SystemRoot\System32\cmd.exe" /d /s /c $initialize
        if ($LASTEXITCODE -ne 0) {
            throw "Could not initialize the Visual Studio x64 environment: $vcvars"
        }
        foreach ($line in $pathOutput) {
            if ($line -match "^Path=(.*)$") {
                $env:PATH = $Matches[1]
            }
        }
    }

    & $MSBuildPath @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE. Check the compiler/linker diagnostics above; configure dependencies using the script parameters or your Visual Studio property sheet."
    }
}
finally {
    $env:PATH = $originalPath
    $env:AimLabDependencyIncludeDirs = $originalBuildIncludeDirectories
    $env:AimLabDependencyLibraryDirs = $originalBuildLibraryDirectories
}

Write-Host "[build] Succeeded: $ProjectPath ($Configuration x64)"

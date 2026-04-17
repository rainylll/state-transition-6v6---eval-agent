param(
    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64",
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild,
    [switch]$SkipRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Stage {
    param([string]$Message)
    Write-Host "`n==== $Message ====" -ForegroundColor Cyan
}

function Write-Detail {
    param([string]$Message)
    Write-Host $Message -ForegroundColor DarkGray
}

function Get-TextLineCount {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        return 0
    }

    return ([System.IO.File]::ReadLines($Path) | Measure-Object).Count
}

function Get-VsWherePath {
    $vsWhereCandidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    return $vsWhereCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

function Resolve-VisualStudioGenerator {
    $vsWhere = Get-VsWherePath
    if (-not $vsWhere) {
        throw "Cannot find vswhere.exe. Please install Visual Studio Build Tools (C++ workload)."
    }

    $installPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installPath) {
        throw "No Visual Studio installation with VC tools found."
    }

    if ($installPath -match "\\2022\\") {
        $generator = "Visual Studio 17 2022"
    }
    elseif ($installPath -match "\\2019\\") {
        $generator = "Visual Studio 16 2019"
    }
    else {
        throw "Unsupported Visual Studio installation for CMake generator resolution: $installPath"
    }

    return [PSCustomObject]@{
        Generator   = $generator
        InstallPath = $installPath
    }
}

function Invoke-CheckedCommand {
    param(
        [string]$Description,
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $RepoRoot "build\encounter-runner"
$RunnerExe = Join-Path $BuildDir "bin\$Configuration\EncounterBatchRunner.exe"
$TasksSource = Join-Path $RepoRoot "agent_mvp\data_real\raw\simulation_tasks.jsonl"
$TasksLocal = Join-Path $RepoRoot "simulation_tasks.jsonl"
$EpisodesLocal = Join-Path $RepoRoot "episodes.jsonl"
$RolloutsLocal = Join-Path $RepoRoot "rollouts.jsonl"
$EpisodesOutDir = Join-Path $RepoRoot "agent_mvp\data_real\raw"
$EpisodesOut = Join-Path $EpisodesOutDir "episodes.jsonl"
$RolloutsOut = Join-Path $EpisodesOutDir "rollouts.jsonl"
$ReplayDir = Join-Path $RepoRoot "agent_mvp\data_real\replays"
$ReplayCount = 10

Write-Stage "Runner configuration"
Write-Detail "Repo root: $RepoRoot"
Write-Detail "Build dir: $BuildDir"
Write-Detail "Runner path: $RunnerExe"
Write-Detail "Configuration: $Configuration"
Write-Detail "Architecture: $Arch"
Write-Detail "SkipBuild: $SkipBuild"
Write-Detail "SkipRun: $SkipRun"
Write-Detail "Replay dir: $ReplayDir"
Write-Detail "Replay files per run: $ReplayCount"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake not found in PATH. Please install CMake or open this script from an environment that provides it."
}

if (-not $SkipBuild) {
    $vsInfo = Resolve-VisualStudioGenerator
    $cmakeArch = if ($Arch -eq "x86") { "Win32" } else { "x64" }

    Write-Stage "Configure EncounterBatchRunner (CMake)"
    Write-Host "Using Visual Studio generator: $($vsInfo.Generator)" -ForegroundColor DarkGray
    Write-Host "Using Visual Studio install: $($vsInfo.InstallPath)" -ForegroundColor DarkGray

    Invoke-CheckedCommand `
        -Description "cmake configure" `
        -FilePath "cmake" `
        -Arguments @(
            "-S", $RepoRoot,
            "-B", $BuildDir,
            "-G", $vsInfo.Generator,
            "-A", $cmakeArch
        )

    Write-Stage "Build EncounterBatchRunner"

    Invoke-CheckedCommand `
        -Description "cmake build" `
        -FilePath "cmake" `
        -Arguments @(
            "--build", $BuildDir,
            "--config", $Configuration,
            "--target", "EncounterBatchRunner",
            "--",
            "/nologo",
            "/clp:ErrorsOnly;Summary"
        )

    if (-not (Test-Path $RunnerExe)) {
        throw "Runner executable not found after build: $RunnerExe"
    }

    Write-Host "Build succeeded: $RunnerExe" -ForegroundColor Green
} else {
    Write-Stage "Skip build"
    Write-Detail "Using existing runner binary: $RunnerExe"
}

if (-not $SkipRun) {
    Write-Stage "Prepare simulation input"
    if (-not (Test-Path $TasksSource)) {
        throw "Input tasks not found: $TasksSource"
    }

    if (-not (Test-Path $RunnerExe)) {
        throw "Runner executable not found: $RunnerExe"
    }

    if (Test-Path $EpisodesLocal) {
        Remove-Item -LiteralPath $EpisodesLocal -Force
    }
    if (Test-Path $RolloutsLocal) {
        Remove-Item -LiteralPath $RolloutsLocal -Force
    }

    Copy-Item -LiteralPath $TasksSource -Destination $TasksLocal -Force
    New-Item -ItemType Directory -Path $ReplayDir -Force | Out-Null
    Get-ChildItem -Path $ReplayDir -Filter *.acmi -File -ErrorAction SilentlyContinue | Remove-Item -Force
    $taskCount = Get-TextLineCount -Path $TasksSource
    Write-Detail "Task source: $TasksSource"
    Write-Detail "Task count detected: $taskCount"
    Write-Detail "Local runner input: $TasksLocal"
    Write-Detail "Episodes output: $EpisodesOut"
    Write-Detail "Rollouts output: $RolloutsOut"
    Write-Detail "Replay output dir: $ReplayDir"

    Write-Stage "Run EncounterBatchRunner"
    $runStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Push-Location $RepoRoot
    try {
        & $RunnerExe
        if ($LASTEXITCODE -ne 0) {
            throw "Runner exited with code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
    $runStopwatch.Stop()
    Write-Host ("Runner finished in {0:N2}s" -f $runStopwatch.Elapsed.TotalSeconds) -ForegroundColor Green

    if (-not (Test-Path $EpisodesLocal)) {
        throw "Runner finished but episodes.jsonl was not produced at $EpisodesLocal"
    }
    if (-not (Test-Path $RolloutsLocal)) {
        throw "Runner finished but rollouts.jsonl was not produced at $RolloutsLocal"
    }

    Write-Stage "Archive episodes.jsonl"
    New-Item -ItemType Directory -Path $EpisodesOutDir -Force | Out-Null
    Move-Item -LiteralPath $EpisodesLocal -Destination $EpisodesOut -Force
    Write-Host "Episodes archived to: $EpisodesOut" -ForegroundColor Green
    Write-Detail "Episode line count: $(Get-TextLineCount -Path $EpisodesOut)"
    Move-Item -LiteralPath $RolloutsLocal -Destination $RolloutsOut -Force
    Write-Host "Rollouts archived to: $RolloutsOut" -ForegroundColor Green
    Write-Detail "Rollout line count: $(Get-TextLineCount -Path $RolloutsOut)"
    Write-Detail "Replay file count: $((Get-ChildItem -Path $ReplayDir -Filter *.acmi -File -ErrorAction SilentlyContinue | Measure-Object).Count)"
} else {
    Write-Stage "Skip run"
    Write-Detail "Build-only mode completed. Runner was not executed."
}

Write-Stage "Done"

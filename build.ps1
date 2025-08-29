# ReShade 64-bit Build Script
# This script builds only the 64-bit version of ReShade without examples

param(
    [string]$Configuration = "Release",
    [switch]$Clean,
    [switch]$Rebuild,
    [switch]$Verbose,
    [switch]$SkipSubmodules,
    [switch]$SkipPythonCheck,
    [switch]$Help
)

# Show help if requested
if ($Help) {
    Write-Host @"
ReShade 64-bit Build Script

Usage: .\build.ps1 [options]

Options:
    -Configuration <config>    Build configuration (Debug, Release, Debug App, Release App)
    -Clean                     Clean build outputs before building
    -Rebuild                   Clean and rebuild all projects
    -Verbose                   Enable verbose MSBuild output
    -SkipSubmodules           Skip git submodule initialization
    -SkipPythonCheck         Skip Python dependency check
    -Help                      Show this help message

Examples:
    .\build.ps1                                    # Build Release|64-bit
    .\build.ps1 -Configuration Debug               # Build Debug|64-bit
    .\build.ps1 -Configuration "Debug|64-bit"      # Same as above
    .\build.ps1 -Clean -Rebuild                    # Clean and rebuild
    .\build.ps1 -Verbose                          # Build with detailed output

Valid Configurations:
    - Debug App, Release App (Application builds)
    - Debug, Release (DLL builds)

Note: This script builds ONLY the 64-bit version of ReShade.
Note: You can use either "Debug" or "Debug|64-bit" format.
"@ -ForegroundColor Cyan
    exit 0
}

Write-Host "=== ReShade 64-bit Build Script ===" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Yellow
Write-Host "Platform: 64-bit (x64)" -ForegroundColor Yellow
Write-Host "===============================" -ForegroundColor Cyan

# Function to validate configuration
function Test-SolutionConfiguration {
    param([string]$Configuration)
    
    Write-Host "Validating solution configuration..." -ForegroundColor Yellow
    
    # Strip platform suffix if user provided it (e.g., "Debug|64-bit" -> "Debug")
    $baseConfig = $Configuration -replace '\|64-bit$', ''
    
    # Valid configurations for 64-bit builds
    $validConfigs = @(
        "Debug App", "Debug",
        "Release App", "Release"
    )
    
    if ($baseConfig -notin $validConfigs) {
        Write-Error "Invalid configuration '$Configuration'. Valid options are: $($validConfigs -join ', ') or $($validConfigs -join '|64-bit, ')|64-bit"
        return $false
    }
    
    Write-Host "Configuration '$baseConfig|64-bit' is valid" -ForegroundColor Green
    return $baseConfig
}

# Function to check Python dependencies
function Test-PythonDependencies {
    if ($SkipPythonCheck) {
        Write-Host "Skipping Python dependency check as requested" -ForegroundColor Yellow
        return $true
    }
    
    Write-Host "Checking Python dependencies..." -ForegroundColor Yellow
    
    # Check if Python is available
    try {
        $pythonVersion = python --version 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Python not found. The 'glad' dependency may fail to build."
            Write-Host "You can install Python from: https://www.python.org/downloads/" -ForegroundColor Yellow
            return $false
        }
        Write-Host "Found Python: $pythonVersion" -ForegroundColor Green
        
        # Check if pip is available
        try {
            $pipVersion = pip --version 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "pip not found. The 'glad' dependency may fail to build."
                return $false
            }
            Write-Host "Found pip: $pipVersion" -ForegroundColor Green
        } catch {
            Write-Warning "pip not found. The 'glad' dependency may fail to build."
            return $false
        }
        
        # Check if required packages are installed
        $requiredPackages = @("Jinja2", "glad")
        foreach ($package in $requiredPackages) {
            try {
                $result = python -c "import $package" 2>&1
                if ($LASTEXITCODE -ne 0) {
                    Write-Host "Installing $package..." -ForegroundColor Yellow
                    pip install $package
                    if ($LASTEXITCODE -ne 0) {
                        Write-Warning "Failed to install $package. The 'glad' dependency may fail to build."
                        return $false
                    }
                } else {
                    Write-Host "$package is available" -ForegroundColor Green
                }
            } catch {
                Write-Warning "Failed to check $package. The 'glad' dependency may fail to build."
                return $false
            }
        }
        
    } catch {
        Write-Warning "Python dependency check failed: $($_.Exception.Message)"
        return $false
    }
    
    Write-Host "Python dependencies are ready" -ForegroundColor Green
    return $true
}

# Function to find MSBuild
function Find-MSBuild {
    Write-Host "Searching for MSBuild..." -ForegroundColor Yellow
    
    # Try to use vswhere to find Visual Studio installations
    $vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswherePath) {
        try {
            $vsPath = & $vswherePath -latest -products * -requires Microsoft.VisualStudio.Component.Roslyn.Compiler -property installationPath
            if ($vsPath) {
                $msbuildPath = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
                if (Test-Path $msbuildPath) {
                    Write-Host "Found MSBuild at: $msbuildPath" -ForegroundColor Green
                    return $msbuildPath
                }
            }
        } catch {
            Write-Host "vswhere failed, falling back to manual search..." -ForegroundColor Yellow
        }
    }
    
    # Fallback to manual search
    $possiblePaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\MSBuild.exe"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            Write-Host "Found MSBuild at: $path" -ForegroundColor Green
            return $path
        }
    }
    
    Write-Error "MSBuild not found. Please install Visual Studio or Build Tools."
    Write-Host "You can download Visual Studio Build Tools from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022" -ForegroundColor Yellow
    return $null
}

# Function to check if git submodules are initialized
function Test-Submodules {
    if ($SkipSubmodules) {
        Write-Host "Skipping submodule check as requested" -ForegroundColor Yellow
        return $true
    }
    
    Write-Host "Checking git submodules..." -ForegroundColor Yellow
    
    # Check if this is a git repository
    if (-not (Test-Path ".git")) {
        Write-Host "Not a git repository, skipping submodule check" -ForegroundColor Yellow
        return $true
    }
    
    # Required submodules for ReShade
    $submodulePaths = @(
        "deps\imgui",
        "deps\minhook", 
        "deps\stb",
        "deps\glad",
        "deps\fpng",
        "deps\spirv",
        "deps\vma",
        "deps\d3d12",
        "deps\openxr",
        "deps\utfcpp"
    )
    
    $missingSubmodules = @()
    foreach ($path in $submodulePaths) {
        if (-not (Test-Path $path)) {
            $missingSubmodules += $path
        }
    }
    
    if ($missingSubmodules.Count -gt 0) {
        Write-Host "Missing submodules: $($missingSubmodules -join ', ')" -ForegroundColor Yellow
        Write-Host "Initializing git submodules..." -ForegroundColor Yellow
        
        try {
            git submodule update --init --recursive
            if ($LASTEXITCODE -ne 0) {
                Write-Error "Failed to initialize git submodules"
                return $false
            }
        } catch {
            Write-Error "Failed to execute git command: $($_.Exception.Message)"
            return $false
        }
    }
    
    Write-Host "Git submodules are ready" -ForegroundColor Green
    return $true
}

# Function to check build prerequisites
function Test-Prerequisites {
    Write-Host "Checking build prerequisites..." -ForegroundColor Yellow
    
    # Check Windows SDK
    $sdkPath = "${env:ProgramFiles(x86)}\Windows Kits\10\Include"
    if (-not (Test-Path $sdkPath)) {
        Write-Warning "Windows SDK not found. This may cause build issues."
        Write-Host "You can download Windows SDK from: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/" -ForegroundColor Yellow
    } else {
        Write-Host "Windows SDK found" -ForegroundColor Green
    }
    
    # Check if we're running as administrator (helpful for some builds)
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
    if ($isAdmin) {
        Write-Host "Running as Administrator" -ForegroundColor Green
    } else {
        Write-Host "Not running as Administrator (this is usually fine)" -ForegroundColor Yellow
    }
    
    return $true
}

# Main build function - builds only 64-bit projects
function Build-ReShade64 {
    param(
        [string]$MSBuildPath,
        [string]$Configuration
    )
    
    Write-Host "Building ReShade 64-bit..." -ForegroundColor Yellow
    
    # Build arguments for 64-bit only
    # Note: MSBuild needs separate Configuration and Platform parameters
    # This maps to the solution configuration "Configuration|64-bit"
    $buildArgs = @(
        "ReShade.sln",
        "/p:Configuration=$Configuration",
        "/p:Platform=64-bit",
        "/m",  # Use multiple processes
        "/nologo",
        "/clp:ErrorsOnly"  # Only show errors by default
    )
    
    if ($Clean) {
        $buildArgs += "/t:Clean"
    }
    
    if ($Rebuild) {
        $buildArgs += "/t:Rebuild"
    }
    
    if ($Verbose) {
        $buildArgs += "/v:detailed"
        $buildArgs = $buildArgs | Where-Object { $_ -ne "/clp:ErrorsOnly" }  # Remove error-only filter for verbose
    }
    
    # Build command
    Write-Host "Executing MSBuild with arguments: $($buildArgs -join ' ')" -ForegroundColor Gray
    
    # Execute build
    $startTime = Get-Date
    $process = Start-Process -FilePath $MSBuildPath -ArgumentList $buildArgs -NoNewWindow -PassThru -Wait
    $endTime = Get-Date
    $duration = $endTime - $startTime
    
    if ($process.ExitCode -eq 0) {
        Write-Host "Build completed successfully in $($duration.ToString('mm\:ss'))!" -ForegroundColor Green
        return $true
    } else {
        Write-Error "Build failed with exit code: $($process.ExitCode)"
        return $false
    }
}

# Function to show build results
function Show-BuildResults {
    param([string]$Configuration)
    
    Write-Host "`n=== Build Results ===" -ForegroundColor Cyan
    
    # Output directory for 64-bit builds
    $outputDir = "bin\x64\$Configuration"
    
    if (Test-Path $outputDir) {
        Write-Host "Output directory: $outputDir" -ForegroundColor Green
        
        # List output files
        $outputFiles = Get-ChildItem -Path $outputDir -Recurse -File | Where-Object { 
            $_.Extension -match "\.(dll|exe|pdb|lib|exp|ilk)$" 
        }
        
        if ($outputFiles) {
            Write-Host "`nGenerated files:" -ForegroundColor Yellow
            foreach ($file in $outputFiles) {
                $size = [math]::Round($file.Length / 1KB, 2)
                Write-Host "  $($file.Name) ($size KB)" -ForegroundColor Gray
            }
        }
        
        # Show key ReShade files
        Write-Host "`nKey ReShade files:" -ForegroundColor Yellow
        $keyFiles = @("ReShade64.dll", "ReShade64.pdb", "ReShadeFX64.lib", "fxc.exe", "inject.exe")
        foreach ($keyFile in $keyFiles) {
            $filePath = Join-Path $outputDir $keyFile
            if (Test-Path $filePath) {
                $size = [math]::Round((Get-Item $filePath).Length / 1KB, 2)
                Write-Host "  ✓ $keyFile ($size KB)" -ForegroundColor Green
            } else {
                Write-Host "  ✗ $keyFile (not found)" -ForegroundColor Red
            }
        }
    } else {
        Write-Warning "Output directory not found: $outputDir"
    }
    
    Write-Host "=====================" -ForegroundColor Cyan
}

# Main execution
try {
    # Check if we're in the right directory
    if (-not (Test-Path "ReShade.sln")) {
        Write-Error "ReShade.sln not found. Please run this script from the ReShade root directory."
        exit 1
    }
    
    # Validate configuration and get base config
    $baseConfig = Test-SolutionConfiguration -Configuration $Configuration
    if (-not $baseConfig) {
        exit 1
    }
    
    # Check prerequisites
    if (-not (Test-Prerequisites)) {
        exit 1
    }
    
    # Check Python dependencies
    if (-not (Test-PythonDependencies)) {
        Write-Warning "Python dependencies check failed. The 'glad' dependency may fail to build."
        $continue = Read-Host "Continue with build anyway? (y/N)"
        if ($continue -ne "y" -and $continue -ne "Y") {
            exit 1
        }
    }
    
    # Find MSBuild
    $msbuildPath = Find-MSBuild
    if (-not $msbuildPath) {
        exit 1
    }
    
    # Check submodules
    if (-not (Test-Submodules)) {
        exit 1
    }
    
    # Build the project (64-bit only)
    $success = Build-ReShade64 -MSBuildPath $msbuildPath -Configuration $baseConfig
    
    if ($success) {
        Show-BuildResults -Configuration $baseConfig
        Write-Host "`nBuild completed successfully!" -ForegroundColor Green
        Write-Host "ReShade 64-bit has been built and is ready for use." -ForegroundColor Cyan
        Write-Host "`nNext steps:" -ForegroundColor Yellow
        Write-Host "  - Copy ReShade64.dll to your game directory" -ForegroundColor Gray
        Write-Host "  - Use inject.exe to inject into running processes" -ForegroundColor Gray
        Write-Host "  - fxc.exe can compile .fx shader files" -ForegroundColor Gray
    } else {
        Write-Host "`nBuild failed!" -ForegroundColor Red
        Write-Host "Check the error messages above for details." -ForegroundColor Yellow
        Write-Host "Common issues:" -ForegroundColor Yellow
        Write-Host "  - Missing Python dependencies (try: pip install Jinja2 glad)" -ForegroundColor Gray
        Write-Host "  - Missing Visual Studio Build Tools" -ForegroundColor Gray
        Write-Host "  - Missing Windows SDK" -ForegroundColor Gray
        Write-Host "  - Git submodules not initialized" -ForegroundColor Gray
        exit 1
    }
    
} catch {
    Write-Error "An error occurred: $($_.Exception.Message)"
    Write-Host "Stack trace: $($_.ScriptStackTrace)" -ForegroundColor Red
    exit 1
}

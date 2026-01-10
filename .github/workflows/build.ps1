#*****************************************************************************#
#    Copyright (C) 2026 Julian Xhokaxhiu                                      #
#                                                                             #
#    This file is part of timewarp                                            #
#                                                                             #
#    timewarp is free software: you can redistribute it and\or modify         #
#    it under the terms of the GNU General Public License as published by     #
#    the Free Software Foundation, either version 3 of the License            #
#                                                                             #
#    timewarp is distributed in the hope that it will be useful,              #
#    but WITHOUT ANY WARRANTY; without even the implied warranty of           #
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            #
#    GNU General Public License for more details.                             #
#*****************************************************************************#

Set-StrictMode -Version Latest

$vcpkgRoot = ".\vcpkg"
$vcpkgBaseline = [string](jq --arg baseline "builtin-baseline" -r '.[$baseline]' vcpkg.json)
$vcpkgOriginUrl = &"git" -C $vcpkgRoot remote get-url origin
$vcpkgTagName = &"git" -C $vcpkgRoot describe --exact-match --tags

$releasePath = [string](jq -r '.configurePresets[0].binaryDir' CMakePresets.json).Replace('${sourceDir}/', '')

Write-Output "--------------------------------------------------"
Write-Output "BUILD CONFIGURATION: $env:_RELEASE_CONFIGURATION"
Write-Output "RELEASE VERSION: $env:_RELEASE_VERSION"
Write-Output "VCPKG ORIGIN: $vcpkgOriginUrl"
Write-Output "VCPKG TAG: $vcpkgTagName"
Write-Output "VCPKG BASELINE: $vcpkgBaseline"
Write-Output "--------------------------------------------------"

# Load vcvarsall environment
$vcvarspath = &"${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -prerelease -latest -property InstallationPath
cmd.exe /c "call `"$vcvarspath\VC\Auxiliary\Build\vcvarsall.bat`" ${env:_RELEASE_ARCHITECTURE} && set > %temp%\vcvars.txt"
Get-Content "$env:temp\vcvars.txt" | Foreach-Object {
  if ($_ -match "^(.*?)=(.*)$") {
    Set-Content "env:\$($matches[1])" $matches[2]
  }
}

# Unset VCPKG_ROOT if set
[Environment]::SetEnvironmentVariable('VCPKG_ROOT','')

# Add Github Packages registry
nuget sources add -Name github -Source "https://nuget.pkg.github.com/julianxhokaxhiu/index.json" -Username ${env:GITHUB_REPOSITORY_OWNER} -Password ${env:GITHUB_PACKAGES_PAT} -StorePasswordInClearText
nuget setApiKey ${env:GITHUB_PACKAGES_PAT} -Source "https://nuget.pkg.github.com/julianxhokaxhiu/index.json"
nuget sources list

# Vcpkg setup
cmd.exe /c "call $vcpkgRoot\bootstrap-vcpkg.bat"

vcpkg integrate install

# Start the build
cmake --preset "${env:_RELEASE_CONFIGURATION}-${env:_RELEASE_ARCHITECTURE}" -D_DLL_VERSION="$env:_BUILD_VERSION"
cmake --build --preset "${env:_RELEASE_CONFIGURATION}-${env:_RELEASE_ARCHITECTURE}"

# Start the packaging
mkdir .dist\pkg | Out-Null
Copy-Item -R "$releasePath\bin\*" .dist\pkg
Move-Item .dist\pkg\${env:_RELEASE_NAME}.dll .dist\pkg\dxgi.dll

7z a ".\.dist\${env:_RELEASE_NAME}-${env:_RELEASE_ARCHITECTURE}-${env:_RELEASE_VERSION}.zip" ".\.dist\pkg\*"

Remove-Item -Recurse -Force .dist\pkg

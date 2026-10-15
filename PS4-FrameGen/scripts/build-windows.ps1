$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$target = if ($env:FRAMEGEN_HOST_BUILD_DIR) { $env:FRAMEGEN_HOST_BUILD_DIR } else { 'build-v03' }
cmake -S . -B $target -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $target --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $target -C Release --output-on-failure
exit $LASTEXITCODE

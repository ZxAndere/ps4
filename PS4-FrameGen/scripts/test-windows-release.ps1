$ErrorActionPreference = 'Stop'
# Put this script beside the delivered EXEs and DLL. All tests run locally in temporary directories.
foreach ($name in 'framegen_tests.exe', 'framegen_runtime_tests.exe', 'framegen_bridge_tests.exe') {
    & (Join-Path $PSScriptRoot $name)
    if ($LASTEXITCODE -ne 0) { throw "$name failed with $LASTEXITCODE" }
}
Write-Output 'Windows test executables completed. This does not verify PS4 runtime compatibility.'

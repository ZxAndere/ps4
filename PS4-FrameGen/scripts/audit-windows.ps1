$tools = 'cmake', 'cl', 'clang++', 'ld.lld', 'ffmpeg', 'dotnet', 'git'
foreach ($tool in $tools) {
    $command = Get-Command $tool -ErrorAction SilentlyContinue
    [PSCustomObject]@{ Tool = $tool; Available = [bool]$command }
}
[PSCustomObject]@{ Tool = 'OO_PS4_TOOLCHAIN binding'; Available = [bool]$env:OO_PS4_TOOLCHAIN }
if ($env:OO_PS4_TOOLCHAIN) {
    foreach ($relative in 'include/orbis/VideoOut.h', 'lib/libc.a', 'lib/libc++.a', 'bin/windows/create-fself.exe', 'bin/windows/PkgTool.Core.exe') {
        [PSCustomObject]@{ Tool = $relative; Available = (Test-Path (Join-Path $env:OO_PS4_TOOLCHAIN $relative)) }
    }
}

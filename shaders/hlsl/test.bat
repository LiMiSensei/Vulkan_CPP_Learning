@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem  Compile shaders\hlsl\*.hlsl into SPIR-V with dxc
rem  Output: vert.spv (vertex), frag.spv (fragment)
rem  Requires: dxc.exe shipped with the Vulkan SDK
rem
rem  NOTE: keep this file pure ASCII. cmd.exe parses .bat files with the
rem  OEM code page (936/GBK on Chinese Windows), so non-ASCII comments get
rem  mis-decoded and can swallow the next ASCII character, turning a plain
rem  "rem ..." line into a bogus command. English-only text avoids that.
rem ============================================================

set "DXC=C:\VulkanSDK\1.4.357.0\Bin\dxc.exe"
if not exist "%DXC%" (
    where dxc >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] dxc.exe not found at "%DXC%" nor in PATH
        echo         Please fix the DXC path at the top of this script
        pause
        exit /b 1
    )
    set "DXC=dxc"
)

rem Use pushd, not "cd /d": when this script lives on a UNC path
rem (\\machine\share\...), cmd.exe cannot use it as the working directory,
rem but pushd maps a temporary drive letter for us.
pushd "%~dp0"

set "FAILED=0"

rem -T vs_6_0   vertex shader target
rem -E main     entry point name
rem -spirv      emit SPIR-V (Vulkan); without it dxc emits DXIL
rem Do NOT add -fvk-invert-y: the original glsl does not flip Y, and adding
rem it would render the image upside down.
echo === Compiling vert.hlsl (vs_6_0) ===
"%DXC%" -T vs_6_0 -E main -spirv -fspv-entrypoint-name=main -Fo "vert.spv" "vert.hlsl"
if errorlevel 1 (
    echo [FAILED] vert.hlsl
    set "FAILED=1"
) else (
    echo [OK] vert.spv
)

echo.
rem -T ps_6_0   pixel/fragment shader target
echo === Compiling frag.hlsl (ps_6_0) ===
"%DXC%" -T ps_6_0 -E main -spirv -fspv-entrypoint-name=main -Fo "frag.spv" "frag.hlsl"
if errorlevel 1 (
    echo [FAILED] frag.hlsl
    set "FAILED=1"
) else (
    echo [OK] frag.spv
)

echo.
if "!FAILED!"=="1" (
    echo === Result: FAILED ===
) else (
    echo === Result: ALL OK ===
)

popd
pause

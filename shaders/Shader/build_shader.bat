@echo off
setlocal

rem ============================================================
rem  Compile shaders\Shader\shader.hlsl into SPIR-V with dxc
rem    entry VSMain -> vert.spv   (vertex  , vs_6_0)
rem    entry PSMain -> frag.spv   (fragment, ps_6_0)
rem  Requires: dxc.exe shipped with the Vulkan SDK
rem
rem  NOTE: keep this file pure ASCII. cmd.exe parses .bat with the OEM
rem  code page (936/GBK on Chinese Windows); non-ASCII comments can be
rem  mis-decoded and swallow the next ASCII character, turning a plain
rem  "rem ..." line into a bogus command. English-only text avoids that.
rem ============================================================

set "DXC=C:\VulkanSDK\1.4.357.0\Bin\dxc.exe"
if not exist "%DXC%" (
    where dxc >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] dxc.exe not found at "%DXC%" nor in PATH
        pause
        exit /b 1
    )
    set "DXC=dxc"
)

rem Use pushd, not "cd /d": this script lives on a UNC path
rem (\\legion\share\...). cmd.exe cannot use a UNC path as the current
rem directory at all - it would fall back to C:\Windows and the relative
rem file names below would resolve against the wrong folder. pushd maps
rem a temporary drive letter for us.
pushd "%~dp0"

set "FAILED=0"

rem -spirv       emit SPIR-V (Vulkan); without it dxc emits DXIL
rem -E VSMain    pick the vertex entry point out of shader.hlsl
rem -fspv-entrypoint-name=main  the C++ side loads the module with the
rem              entry point name "main", so rename it in the SPIR-V
rem Do NOT add -fvk-invert-y: the original GLSL does not flip Y.
echo === vert: shader.hlsl  -E VSMain -> vert.spv ===
"%DXC%" -T vs_6_0 -E VSMain -spirv -fspv-entrypoint-name=main -Fo vert.spv shader.hlsl
if errorlevel 1 (
    echo [FAILED] vert.spv
    set "FAILED=1"
) else (
    echo [OK] vert.spv
)

echo.
echo === frag: shader.hlsl  -E PSMain -> frag.spv ===
"%DXC%" -T ps_6_0 -E PSMain -spirv -fspv-entrypoint-name=main -Fo frag.spv shader.hlsl
if errorlevel 1 (
    echo [FAILED] frag.spv
    set "FAILED=1"
) else (
    echo [OK] frag.spv
)

echo.
if "%FAILED%"=="1" (
    echo === Result: FAILED ===
) else (
    echo === Result: ALL OK ===
)

popd
pause
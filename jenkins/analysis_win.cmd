@REM Copyright (c) Microsoft. All rights reserved.
@REM Licensed under the MIT license. See LICENSE file in the project root for full license information.

@setlocal EnableExtensions EnableDelayedExpansion
@echo off

set build-root=%~dp0..
rem // resolve to fully qualified path
for %%i in ("%build-root%") do set build-root=%%~fi

set sdk-root=%build-root%

set build_folder=%build-root%\cmake\mem_win

if EXIST %build_folder% (
    rmdir /s/q %build_folder%
    rem no error checking
)

mkdir %build_folder%
pushd %build_folder%

REM -- C --
cmake %sdk-root% -Dmemory_trace:BOOL=ON -Duse_prov_client:BOOL=ON -Dsdk_analysis:BOOL=ON -Dskip_samples:BOOL=ON
if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!
popd

msbuild /m %build_folder%\azure_iot_sdks.sln "/p:Configuration=Release"
if not !ERRORLEVEL!==0 exit /b !ERRORLEVEL!

call %build_folder%\sdk_analysis\memory_analysis\release\memory_analysis.exe
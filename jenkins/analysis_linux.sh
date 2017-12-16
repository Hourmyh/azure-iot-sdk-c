#!/bin/bash
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE file in the project root for full license information.

build_root=$(cd "$(dirname "$0")/.." && pwd)
cd $build_root

build_folder=$build_root"/cmake/mem_linux"

# -- C --
rm -r -f $build_folder
mkdir -p $build_folder
pushd $build_folder
cmake -DCMAKE_BUILD_TYPE=Release -Dmemory_trace:BOOL=ON -Duse_prov_client:BOOL=ON -Dsdk_analysis:BOOL=ON -Dskip_samples:BOOL=ON $build_root

CORES=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)

make --jobs=$CORES

popd

# Run the memory analysis
pushd $build_folder/sdk_analysis/memory_analysis

./memory_analysis

popd

[ $? -eq 0 ] || exit $?


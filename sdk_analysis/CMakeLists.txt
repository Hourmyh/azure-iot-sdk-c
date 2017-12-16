#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#this is CMakeLists for testtools. It does nothing, except loads other folders

function(add_analysis_directory whatIsBuilding)
    add_subdirectory(${whatIsBuilding})

    set_target_properties(${whatIsBuilding}
               PROPERTIES
               FOLDER "SDK_analysis")
endfunction()

add_analysis_directory(memory_analysis)

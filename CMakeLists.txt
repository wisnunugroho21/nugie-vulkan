cmake_minimum_required(VERSION 3.24.0)

include(CMake/CommonMacros.txt)

SETUP_APP(Nugie_Engine "Nugie Engine")

add_subdirectory(libraries/lightweightvk)
target_link_libraries(${PROJECT_NAME} PRIVATE LVKLibrary)

add_subdirectory(libraries/glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm)

add_subdirectory(libraries/assimp)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp)

target_include_directories(${PROJECT_NAME} PRIVATE libraries/stb)

add_subdirectory(libraries/ktx-software)
target_link_libraries(${PROJECT_NAME} PRIVATE ktx)
target_include_directories(${PROJECT_NAME} PRIVATE libraries/ktx-software/lib)

add_subdirectory(libraries/meshoptimizer)
target_link_libraries(${PROJECT_NAME} PRIVATE meshoptimizer)

add_subdirectory(libraries/taskflow)
target_link_libraries(${PROJECT_NAME} PRIVATE Taskflow)
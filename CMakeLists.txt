cmake_minimum_required(VERSION 4.3)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

add_executable(tut1 tut1.cpp)
add_executable(tut2 tut2.cpp)
add_executable(matrix-mul matrix-mul.cpp)
add_executable(matrix-mul-cpu matrix-mul-cpu.cpp)
add_executable(matrix-mul-tiled matrix-mul-tiled.cpp)
add_executable(MNIST-Kernel MNIST-Kernel.cpp)

target_link_libraries(tut1 Vulkan::Vulkan)
target_link_libraries(tut2 Vulkan::Vulkan)
target_link_libraries(matrix-mul Vulkan::Vulkan)
target_link_libraries(matrix-mul-tiled Vulkan::Vulkan)
target_link_libraries(MNIST-Kernel Vulkan::Vulkan)

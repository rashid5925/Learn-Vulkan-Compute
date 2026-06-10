cmake_minimum_required(VERSION 4.3)
project(VulkanTest)

find_package(Vulkan REQUIRED)

add_executable(tut1 tut1.cpp)
add_executable(tut2 tut2.cpp)
add_executable(matrix-mul matrix-mul.cpp)
add_executable(matrix-mul-cpu matrix-mul-cpu.cpp)

target_link_libraries(tut1 Vulkan::Vulkan)
target_link_libraries(tut2 Vulkan::Vulkan)
target_link_libraries(matrix-mul Vulkan::Vulkan)

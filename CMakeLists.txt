cmake_minimum_required(VERSION 4.3)
project(VulkanTest)

find_package(Vulkan REQUIRED)

add_executable(tut1 tut1.cpp)
target_link_libraries(tut1 Vulkan::Vulkan)

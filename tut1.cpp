#include <iostream>
#include <assert.h>
#include <vulkan/vulkan.hpp>

int main() {    
    VkResult res;

    VkInstance instance;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Arr Add";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = nullptr;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    res = vkCreateInstance(&createInfo, nullptr, &instance);
    assert(res == VK_SUCCESS);

    uint32_t physicalDevicesCount = 0;
    res = vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, nullptr);
    assert(res == VK_SUCCESS);
    VkPhysicalDevice physicalDevices[physicalDevicesCount]; 
    res = vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, physicalDevices);
    std::cout << "Number of physical devices: " << physicalDevicesCount << std::endl;
    assert(res == VK_SUCCESS);

    VkPhysicalDevice physicalDevice = physicalDevices[0];
    VkPhysicalDeviceProperties deviceProperties = vk::PhysicalDevice(physicalDevice).getProperties();
    VkPhysicalDeviceFeatures deviceFeatures = vk::PhysicalDevice(physicalDevice).getFeatures();

    std::cout << "Physical Device apiVersion: " << deviceProperties.apiVersion << std::endl;
    std::cout << "Physical Device driverVersion: " << deviceProperties.driverVersion << std::endl;
    std::cout << "Physical Device deviceName: " << deviceProperties.deviceName << std::endl;
    std::cout << "Physical Device VkPhysicalDeviceType: " << deviceProperties.deviceType << std::endl;

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.00f;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    // typedef struct VkDeviceCreateInfo {
    //     VkStructureType                    sType;
    //     const void*                        pNext;
    //     VkDeviceCreateFlags                flags;
    //     uint32_t                           queueCreateInfoCount;
    //     const VkDeviceQueueCreateInfo*     pQueueCreateInfos;
    //     // enabledLayerCount is deprecated and should not be used
    //     uint32_t                           enabledLayerCount;
    //     // ppEnabledLayerNames is deprecated and should not be used
    //     const char* const*                 ppEnabledLayerNames;
    //     uint32_t                           enabledExtensionCount;
    //     const char* const*                 ppEnabledExtensionNames;
    //     const VkPhysicalDeviceFeatures*    pEnabledFeatures;
    // } VkDeviceCreateInfo;
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.ppEnabledExtensionNames = nullptr;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    VkDevice device;
    res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    assert(res == VK_SUCCESS);
        
    
    std::cout << "OK: " << res << std::endl;
    return 0;
}

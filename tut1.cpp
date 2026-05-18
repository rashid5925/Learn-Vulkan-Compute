#include <iostream>
#include <fstream>
#include <stdexcept>
#include <assert.h>
#include <vulkan/vulkan.hpp>

#define N 25

float dataA[N] = { 123, 456, 789, 213, 546, 879, 312, 645, 978, 321, 654, 987, 132, 465, 798, 231, 564, 897, 312, 645, 978, 321, 654, 987, 132 };
float dataB[N] = { 123, 456, 789, 213, 546, 879, 312, 645, 978, 321, 654, 987, 132, 465, 798, 231, 564, 897, 312, 645, 978, 321, 654, 987, 132 };
float results[N] = { 0 };
int bufferSize = 3 * N * sizeof(float);

std::vector<char> readSPV(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

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
    
    std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
    res = vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, physicalDevices.data());
    std::cout << "Number of physical devices: " << physicalDevicesCount << std::endl;
    assert(res == VK_SUCCESS);

    VkPhysicalDevice physicalDevice = physicalDevices[0];
    VkPhysicalDeviceProperties deviceProperties = vk::PhysicalDevice(physicalDevice).getProperties();
    VkPhysicalDeviceFeatures deviceFeatures = vk::PhysicalDevice(physicalDevice).getFeatures();

    std::cout << "Physical Device apiVersion: " << deviceProperties.apiVersion << std::endl;
    std::cout << "Physical Device driverVersion: " << deviceProperties.driverVersion << std::endl;
    std::cout << "Physical Device deviceName: " << deviceProperties.deviceName << std::endl;
    std::cout << "Physical Device VkPhysicalDeviceType: " << deviceProperties.deviceType << std::endl;

    uint32_t queueFamilyPropertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    std::cout << "Queue Family Count: " << queueFamilyPropertyCount << std::endl;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());
    uint32_t queueFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i) {
        if (
            queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
            queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT
        ) {
            queueFamilyIndex = i;
            break;
        }
    }

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.00f;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
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

    VkQueue queue{};
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    
    // typedef struct VkBufferCreateInfo {
    //     VkStructureType        sType;
    //     const void*            pNext;
    //     VkBufferCreateFlags    flags;
    //     VkDeviceSize           size;
    //     VkBufferUsageFlags     usage;
    //     VkSharingMode          sharingMode;
    //     uint32_t               queueFamilyIndexCount;
    //     const uint32_t*        pQueueFamilyIndices;
    // } VkBufferCreateInfo;
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uint32_t size = bufferSize;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    res = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer);
    assert(res == VK_SUCCESS);

    VkMemoryRequirements memoryRequiremnts{};
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequiremnts);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.allocationSize = memoryRequiremnts.size;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    memoryAllocateInfo.memoryTypeIndex = -1;
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if (
            (memoryRequiremnts.memoryTypeBits & (1 << i)) &&
            ((memoryProperties.memoryTypes[i].propertyFlags & required) == required)
        ) {
            memoryAllocateInfo.memoryTypeIndex = i;
            break;
        }
    }
    VkDeviceMemory memory{};
    res = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &memory);
    assert(res == VK_SUCCESS);

    res = vkBindBufferMemory(device, buffer, memory, 0);

    // typedef struct VkDescriptorSetLayoutBinding {
    //     uint32_t              binding;
    //     VkDescriptorType      descriptorType;
    //     uint32_t              descriptorCount;
    //     VkShaderStageFlags    stageFlags;
    //     const VkSampler*      pImmutableSamplers;
    // } VkDescriptorSetLayoutBinding;
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding1{};
    descriptorSetLayoutBinding1.binding = 0;
    descriptorSetLayoutBinding1.descriptorCount = 1;
    descriptorSetLayoutBinding1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBinding1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding2{};
    descriptorSetLayoutBinding2.binding = 1;
    descriptorSetLayoutBinding2.descriptorCount = 1;
    descriptorSetLayoutBinding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBinding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding3{};
    descriptorSetLayoutBinding3.binding = 2;
    descriptorSetLayoutBinding3.descriptorCount = 1;
    descriptorSetLayoutBinding3.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBinding3.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[3] = {
        descriptorSetLayoutBinding1,
        descriptorSetLayoutBinding2,
        descriptorSetLayoutBinding3,
    };


    // typedef struct VkDescriptorSetLayoutCreateInfo {
    //     VkStructureType                        sType;
    //     const void*                            pNext;
        // VkDescriptorSetLayoutCreateFlags       flags;
    //     uint32_t                               bindingCount;
    //     const VkDescriptorSetLayoutBinding*    pBindings;
    // } VkDescriptorSetLayoutCreateInfo;
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 3;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;

    VkDescriptorSetLayout descriptorSetLayout{};
    res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
    assert(res == VK_SUCCESS);

    VkDescriptorPoolSize descriptorPoolSize{};
    descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorPoolSize.descriptorCount = 3;
    
    // typedef struct VkDescriptorPoolCreateInfo {
    //     VkStructureType                sType;
    //     const void*                    pNext;
    //     VkDescriptorPoolCreateFlags    flags;
    //     uint32_t                       maxSets;
    //     uint32_t                       poolSizeCount;
    //     const VkDescriptorPoolSize*    pPoolSizes;
    // } VkDescriptorPoolCreateInfo;
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
    
    VkDescriptorPool descriptorPool{};
    res = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
    assert(res == VK_SUCCESS);

    VkDescriptorSet descriptorSet{};

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; 
    descriptorSetAllocateInfo.descriptorPool = descriptorPool; 
    descriptorSetAllocateInfo.descriptorSetCount = 1; 
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

    // typedef struct VkDescriptorBufferInfo {
    //     VkBuffer        buffer;
    //     VkDeviceSize    offset;
    //     VkDeviceSize    range;
    // } VkDescriptorBufferInfo;
    VkDescriptorBufferInfo descriptorBufferInfo1;
    descriptorBufferInfo1.buffer = buffer;
    descriptorBufferInfo1.range = 100;
    descriptorBufferInfo1.offset = 0;

    VkDescriptorBufferInfo descriptorBufferInfo2;
    descriptorBufferInfo2.buffer = buffer;
    descriptorBufferInfo2.range = 100;
    descriptorBufferInfo2.offset = 100;

    VkDescriptorBufferInfo descriptorBufferInfo3;
    descriptorBufferInfo3.buffer = buffer;
    descriptorBufferInfo3.range = 100;
    descriptorBufferInfo3.offset = 200;

    std::vector<VkDescriptorBufferInfo> descriptorBufferInfos {
        descriptorBufferInfo1,
        descriptorBufferInfo2,
        descriptorBufferInfo3,
    };

    res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
    assert(res == VK_SUCCESS);

    std::vector<VkWriteDescriptorSet> writeDescriptorSet(3, VkWriteDescriptorSet{});
    for (uint32_t i; i < writeDescriptorSet.size(); ++i) {
        writeDescriptorSet[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet[i].dstSet = descriptorSet; 
        writeDescriptorSet[i].dstBinding = i; 
        writeDescriptorSet[i].descriptorCount = 1; 
        writeDescriptorSet[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
        writeDescriptorSet[i].pBufferInfo = &descriptorBufferInfos[i];
    }

    vkUpdateDescriptorSets(device, 3, writeDescriptorSet.data(), 0, NULL);

    void* mappedData;
    vkMapMemory(device, memory, 0, size, 0, &mappedData);
    memcpy(mappedData, dataA, N * sizeof(float)); 
    memcpy((char*)mappedData + N * sizeof(float), dataB, N * sizeof(float));
    vkUnmapMemory(device, memory);

    auto computeShaderCode = readSPV("arr-add.spv");

    VkShaderModule shaderModule{};
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = computeShaderCode.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(computeShaderCode.data());
    res = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);
    assert(res == VK_SUCCESS);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
    shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfo.module = shaderModule;
    shaderStageCreateInfo.pName = "main";

    // typedef struct VkPipelineLayoutCreateInfo {
    //     VkStructureType                 sType;
    //     const void*                     pNext;
    //     VkPipelineLayoutCreateFlags     flags;
    //     uint32_t                        setLayoutCount;
    //     const VkDescriptorSetLayout*    pSetLayouts;
    //     uint32_t                        pushConstantRangeCount;
    //     const VkPushConstantRange*      pPushConstantRanges;
    // } VkPipelineLayoutCreateInfo;
    VkPipelineLayout pipelineLayout{};
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
    assert(res == VK_SUCCESS);

    // typedef struct VkComputePipelineCreateInfo {
    //     VkStructureType                    sType;
    //     const void*                        pNext;
    //     VkPipelineCreateFlags              flags;
    //     VkPipelineShaderStageCreateInfo    stage;
    //     VkPipelineLayout                   layout;
    //     VkPipeline                         basePipelineHandle;
    //     int32_t                            basePipelineIndex;
    // } VkComputePipelineCreateInfo;
    VkPipeline pipeline{};
    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.stage = shaderStageCreateInfo;
    computePipelineCreateInfo.layout = pipelineLayout;
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    res = vkCreateComputePipelines(device, NULL, 1, &computePipelineCreateInfo, nullptr, &pipeline);
    assert(res == VK_SUCCESS);

    VkCommandPool commandPool{};
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    res = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
    assert(res == VK_SUCCESS);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.commandBufferCount = 1;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    VkCommandBuffer commandBuffer{};
    res = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    assert(res == VK_SUCCESS);

    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    assert(res == VK_SUCCESS);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

    vkCmdDispatch(commandBuffer, N, 1, 1);
    res = vkEndCommandBuffer(commandBuffer);
    assert(res == VK_SUCCESS);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence fence{};
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    res = vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
    assert(res == VK_SUCCESS);

    res = vkQueueSubmit(queue, 1, &submitInfo, fence);
    assert(res == VK_SUCCESS);

    res = vkWaitForFences(device, 1, &fence, VK_TRUE, 100000000000);
    assert(res == VK_SUCCESS);

    vkDestroyFence(device, fence, nullptr);

    std::cout << "OK: " << res << std::endl;

    void* mappedMemory = NULL;
    vkMapMemory(device, memory, 0, bufferSize, 0, &mappedMemory);
    float* mappedResults = (float*)((char*)mappedMemory + 2 * N * sizeof(float));

    std::cout << "Result: " << std::endl;
    for (uint32_t i = 0; i < N; ++i) {
        results[i] = mappedResults[i];
        std::cout << results[i] << ", ";
    }
    std::cout << std::endl;

    return 0;
}

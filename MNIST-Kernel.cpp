#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <assert.h>
#include <vulkan/vulkan.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

struct SpecializationData {
    uint32_t T1r;
    uint32_t T1c;
    uint32_t T2r;
    uint32_t T2c;
};

struct SpecializationDataRelu {
    uint32_t LW1r;
    uint32_t LW1c;
};

std::vector<float> readModelBin(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        std::cerr << "Failed to open file!" << std::endl;
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    size_t element_size = sizeof(float); 
    size_t num_elements = size / element_size;

    std::vector<float> data(num_elements);

    if (file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cout << "Successfully read " << num_elements << " elements." << std::endl;
    } else {
        std::cerr << "Failed to read file." << std::endl;
    }

    file.close();
    return data;
}

std::vector<float> readImage(const char* image_path) {
    int width, height, channels;
    unsigned char* raw_pixels = stbi_load(image_path, &width, &height, &channels, 1);
    if (!raw_pixels) {
        std::cerr << "Error: Could not load image!" << std::endl;
        return {};
    }

    int target_width = 28;
    int target_height = 28;

    std::vector<float> input_tensor(target_height * target_width);

    for (int y = 0; y < target_height; ++y) {
        for (int x = 0; x < target_width; ++x) {
            
            float src_x_float = (x + 0.5f) * ((float)width / target_width) - 0.5f;
            float src_y_float = (y + 0.5f) * ((float)height / target_height) - 0.5f;

            int x1 = std::max(0, std::min((int)floor(src_x_float), width - 1));
            int y1 = std::max(0, std::min((int)floor(src_y_float), height - 1));
            int x2 = std::max(0, std::min(x1 + 1, width - 1));
            int y2 = std::max(0, std::min(y1 + 1, height - 1));

            float dx = src_x_float - x1;
            float dy = src_y_float - y1;

            float p11 = raw_pixels[y1 * width + x1];
            float p21 = raw_pixels[y1 * width + x2];
            float p12 = raw_pixels[y2 * width + x1];
            float p22 = raw_pixels[y2 * width + x2];

            float i1 = p11 * (1.0f - dx) + p21 * dx;
            float i2 = p12 * (1.0f - dx) + p22 * dx;
            float final_pixel_val = i1 * (1.0f - dy) + i2 * dy;

            int target_idx = (y * target_width) + x;

            input_tensor[target_idx] = final_pixel_val / 255.0f;
        }
    }

    stbi_image_free(raw_pixels);

    std::cout << "Total elements in tensor of image: " << input_tensor.size() << std::endl;

    return input_tensor;
}

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

void initializeVulkan(VkInstance &instance, VkApplicationInfo &appInfo, VkInstanceCreateInfo &createInfo, VkPhysicalDevice &physicalDevice, VkPhysicalDeviceProperties &deviceProperties, VkPhysicalDeviceFeatures &deviceFeatures, uint32_t *queueFamilyIndex, VkResult &res) {
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MNIST Kernel";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

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

    physicalDevice = physicalDevices[0];
    
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    std::cout << "Physical Device apiVersion: " << deviceProperties.apiVersion << std::endl;
    std::cout << "Physical Device driverVersion: " << deviceProperties.driverVersion << std::endl;
    std::cout << "Physical Device deviceName: " << deviceProperties.deviceName << std::endl;
    std::cout << "Physical Device VkPhysicalDeviceType: " << deviceProperties.deviceType << std::endl;

    uint32_t queueFamilyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    std::cout << "Queue Family Count: " << queueFamilyPropertyCount << std::endl;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());
    for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i) {
        if (
            queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
            queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT
        ) {
            *queueFamilyIndex = i;
            break;
        }
    }

}

void createDevice(VkDeviceQueueCreateInfo& queueCreateInfo, VkDeviceCreateInfo& deviceCreateInfo, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkPhysicalDeviceFeatures deviceFeatures, VkDevice& device, VkResult res) {
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.00f;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.ppEnabledExtensionNames = nullptr;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    assert(res == VK_SUCCESS);
}

void createBuffer(VkBufferCreateInfo& bufferCreateInfo, VkBufferUsageFlags bufferUsage, VkBuffer& stagingBuffer, VkDevice device, uint32_t bufferSize, VkResult res) {
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = bufferUsage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    res = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer);
    assert(res == VK_SUCCESS);
}

void createMemory(VkDevice device, VkPhysicalDevice& physicalDevice, VkBuffer &stagingBuffer, VkDeviceMemory& stagingMemory, VkResult res) {
    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memoryRequirements);

    VkMemoryAllocateInfo localMemoryAllocateInfo{};
    localMemoryAllocateInfo.allocationSize = memoryRequirements.size;
    localMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    localMemoryAllocateInfo.memoryTypeIndex = -1;
    VkPhysicalDeviceMemoryProperties localMemoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &localMemoryProperties);
    VkMemoryPropertyFlags localRequired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < localMemoryProperties.memoryTypeCount; ++i) {
        if (
            (memoryRequirements.memoryTypeBits & (1 << i)) &&
            ((localMemoryProperties.memoryTypes[i].propertyFlags & localRequired) == localRequired)
        ) {
            localMemoryAllocateInfo.memoryTypeIndex = i;
            break;
        }
    }

    res = vkAllocateMemory(device, &localMemoryAllocateInfo, nullptr, &stagingMemory);
    assert(res == VK_SUCCESS);
}

void createDescriptorLayoutBinding(VkDescriptorSetLayoutBinding& descriptorSetLayoutBinding, uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags) {
    descriptorSetLayoutBinding.binding = binding;
    descriptorSetLayoutBinding.descriptorCount = 1;
    descriptorSetLayoutBinding.descriptorType = descriptorType;
    descriptorSetLayoutBinding.stageFlags = stageFlags;
}

void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayoutBinding* descriptorSetLayoutBindings, uint32_t bindingCount, VkDescriptorSetLayout& descriptorSetLayout, VkResult res) {
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;

    res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
    assert(res == VK_SUCCESS);
}

void createDescriptorPool(VkDevice device, VkDescriptorPool& descriptorPool, uint32_t descriptorCount, VkResult res) {
    VkDescriptorPoolSize descriptorPoolSize{};
    descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorPoolSize.descriptorCount = descriptorCount;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;

    res = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
    assert(res == VK_SUCCESS);
}

void createDescriptorBufferInfo(VkDescriptorBufferInfo& descriptorBufferInfo, VkBuffer buffer, size_t range, size_t offset) {
    descriptorBufferInfo.buffer = buffer;
    descriptorBufferInfo.range = range;
    descriptorBufferInfo.offset = offset;
}

void createSpecializationMapEntries(VkSpecializationInfo* specializationInfoL, SpecializationData& specDataL, VkSpecializationMapEntry (&specializationMapEntriesL)[4], uint32_t T1r, uint32_t T1c, uint32_t T2r, uint32_t T2c)
{
    specDataL.T1r = T1r;
    specDataL.T1c = T1c;
    specDataL.T2r = T2r;
    specDataL.T2c = T2c;

    specializationMapEntriesL[0] = { 0, offsetof(SpecializationData, T1r), sizeof(uint32_t) };
    specializationMapEntriesL[1] = { 1, offsetof(SpecializationData, T1c), sizeof(uint32_t) };
    specializationMapEntriesL[2] = { 2, offsetof(SpecializationData, T2r),  sizeof(uint32_t) };
    specializationMapEntriesL[3] = { 3, offsetof(SpecializationData, T2c),  sizeof(uint32_t) };

    specializationInfoL->mapEntryCount = 4;
    specializationInfoL->pMapEntries   = specializationMapEntriesL;
    specializationInfoL->dataSize      = sizeof(SpecializationData);
    specializationInfoL->pData         = &specDataL;
}

void createSpecializationMapEntriesRelu(VkSpecializationInfo* specializationInfoL, SpecializationDataRelu& specDataL, VkSpecializationMapEntry (&specializationMapEntriesL)[2], uint32_t LW1r, uint32_t LW1c)
{
    specDataL.LW1r = LW1r;
    specDataL.LW1c = LW1c;

    specializationMapEntriesL[0] = { 0, offsetof(SpecializationDataRelu, LW1r), sizeof(uint32_t) };
    specializationMapEntriesL[1] = { 1, offsetof(SpecializationDataRelu, LW1c), sizeof(uint32_t) };

    specializationInfoL->mapEntryCount = 2;
    specializationInfoL->pMapEntries   = specializationMapEntriesL;
    specializationInfoL->dataSize      = sizeof(SpecializationDataRelu);
    specializationInfoL->pData         = &specDataL;
}

void initializeBufferMemDesc(
    VkDevice device, 
    VkPhysicalDevice physicalDevice, 
    VkQueue queue,
    uint32_t queueFamilyIndex,
    uint32_t bufferSize,
    VkResult res, 
    const std::vector<float>& linear1_w, 
    const std::vector<float>& input_tensor, 
    const std::vector<float>& linear1_b,
    const std::vector<float>& linear2_w, 
    const std::vector<float>& linear2_b,
    uint32_t outputSizeL1,
    uint32_t outputSizeL2,
    VkBuffer& buffer,        
    VkDeviceMemory& memory   
) {
    VkBufferCreateInfo localBufferCreateInfo{};
    VkBuffer stagingBuffer{};
    VkDeviceMemory stagingMemory{};
    createBuffer(localBufferCreateInfo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, device, bufferSize, res);
    createMemory(device, physicalDevice, stagingBuffer, stagingMemory, res);
    res = vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
    assert(res == VK_SUCCESS);

    VkBufferCreateInfo bufferCreateInfo{};
    createBuffer(bufferCreateInfo, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, buffer, device, bufferSize, res);
    createMemory(device, physicalDevice, buffer, memory, res);
    res = vkBindBufferMemory(device, buffer, memory, 0);
    assert(res == VK_SUCCESS);

    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &mappedData);

    size_t offset = 0;
    memcpy((char*)mappedData + offset, linear1_w.data(), linear1_w.size() * sizeof(float));
    
    offset += linear1_w.size() * sizeof(float);
    memcpy((char*)mappedData + offset, input_tensor.data(), input_tensor.size() * sizeof(float));
    
    offset += input_tensor.size() * sizeof(float);
    offset += outputSizeL1 * sizeof(float); 
    memcpy((char*)mappedData + offset, linear1_b.data(), linear1_b.size() * sizeof(float));
    
    offset += linear1_b.size() * sizeof(float);
    memcpy((char*)mappedData + offset, linear2_w.data(), linear2_w.size() * sizeof(float));
    
    offset += linear2_w.size() * sizeof(float);
    memcpy((char*)mappedData + offset, linear2_b.data(), linear2_b.size() * sizeof(float));
    
    vkUnmapMemory(device, stagingMemory);

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    VkCommandPool commandPool{};
    res = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
    assert(res == VK_SUCCESS);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer{};
    res = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    assert(res == VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    assert(res == VK_SUCCESS);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

    res = vkEndCommandBuffer(commandBuffer);
    assert(res == VK_SUCCESS);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence{};
    res = vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
    assert(res == VK_SUCCESS);

    res = vkQueueSubmit(queue, 1, &submitInfo, fence);
    assert(res == VK_SUCCESS);
    res = vkWaitForFences(device, 1, &fence, VK_TRUE, 100000000000);
    assert(res == VK_SUCCESS);

    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
}

void createDescriptorSetLinear(
    VkDevice device, 
    VkDescriptorSetLayout &descriptorSetLayout, 
    VkDescriptorPool descriptorPool, 
    VkDescriptorSet& descriptorSet, 
    VkDescriptorSetAllocateInfo &descriptorSetAllocateInfo,
    VkResult res
) {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingT1{};
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingT2{};
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingOutput{};

    createDescriptorLayoutBinding(descriptorSetLayoutBindingT1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    createDescriptorLayoutBinding(descriptorSetLayoutBindingT2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    createDescriptorLayoutBinding(descriptorSetLayoutBindingOutput, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingsL[3] = {
        descriptorSetLayoutBindingT1,
        descriptorSetLayoutBindingT2,
        descriptorSetLayoutBindingOutput
    };

    createDescriptorSetLayout(device, descriptorSetLayoutBindingsL, 3, descriptorSetLayout, res);

    createDescriptorPool(device, descriptorPool, 3,res);

    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; 
    descriptorSetAllocateInfo.descriptorPool = descriptorPool; 
    descriptorSetAllocateInfo.descriptorSetCount = 1; 
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
}

void createDescriptorSetRelu(
    VkDevice device, 
    VkDescriptorSetLayout &descriptorSetLayout, 
    VkDescriptorPool descriptorPool, 
    VkDescriptorSet& descriptorSet, 
    VkDescriptorSetAllocateInfo &descriptorSetAllocateInfo,
    VkResult res
) {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingT1{};

    createDescriptorLayoutBinding(descriptorSetLayoutBindingT1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingsL[1] = {
        descriptorSetLayoutBindingT1,
    };

    createDescriptorSetLayout(device, descriptorSetLayoutBindingsL, 1, descriptorSetLayout, res);

    createDescriptorPool(device, descriptorPool, 1, res);

    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; 
    descriptorSetAllocateInfo.descriptorPool = descriptorPool; 
    descriptorSetAllocateInfo.descriptorSetCount = 1; 
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
}

void updateDescriptorSetLinear(
    VkDevice device,
    VkDescriptorSet descriptorSet,
    VkBuffer buffer,
    uint32_t t1Size, uint32_t t1Offset,
    uint32_t t2Size, uint32_t t2Offset,
    uint32_t outputSize, uint32_t outputOffset
) {
    VkDescriptorBufferInfo bufferInfos[3];
    createDescriptorBufferInfo(bufferInfos[0], buffer, t1Size * sizeof(float), t1Offset * sizeof(float));
    createDescriptorBufferInfo(bufferInfos[1], buffer, t2Size * sizeof(float), t2Offset * sizeof(float));
    createDescriptorBufferInfo(bufferInfos[2], buffer, outputSize * sizeof(float), outputOffset * sizeof(float));

    VkWriteDescriptorSet writes[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
}

void updateDescriptorSetRelu(
    VkDevice device,
    VkDescriptorSet descriptorSet,
    VkBuffer buffer,
    uint32_t t1Size, uint32_t t1Offset
) {
    VkDescriptorBufferInfo bufferInfoT1;
    createDescriptorBufferInfo(bufferInfoT1, buffer, t1Size * sizeof(float), t1Offset * sizeof(float));

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfoT1;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void linearOperation(
    const std::string& shader_file_name,
    VkBuffer &buffer, 
    VkDeviceMemory &memory, 
    VkBuffer &stagingBuffer, 
    VkDevice device, 
    const uint32_t bufferSize, 
    VkPhysicalDevice physicalDevice, 
    VkQueue queue, 
    VkDescriptorSetLayout descriptorSetLayout, 
    VkDescriptorSet& descriptorSet, 
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo,
    uint32_t queueFamilyIndex, 
    VkResult res, 
    uint32_t t1Size, 
    uint32_t t1Offset,
    uint32_t t2Size,
    uint32_t t2Offset,
    uint32_t outputSize,
    uint32_t outputOffset,
    uint32_t T1r, 
    uint32_t T1c, 
    uint32_t T2r, 
    uint32_t T2c
) {
    updateDescriptorSetLinear(device, descriptorSet, buffer, t1Size, t1Offset, t2Size, t2Offset, outputSize, outputOffset);

    auto computeShaderCodeL = readSPV(shader_file_name);

    VkShaderModule shaderModuleL{};
    VkShaderModuleCreateInfo shaderModuleCreateInfoL{};
    shaderModuleCreateInfoL.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfoL.codeSize = computeShaderCodeL.size();
    shaderModuleCreateInfoL.pCode = reinterpret_cast<const uint32_t*>(computeShaderCodeL.data());
    res = vkCreateShaderModule(device, &shaderModuleCreateInfoL, nullptr, &shaderModuleL);
    assert(res == VK_SUCCESS);

    SpecializationData specDataL{};
    VkSpecializationMapEntry specializationMapEntriesL[4]{};
    VkSpecializationInfo specializationInfoL{};
    createSpecializationMapEntries(&specializationInfoL, specDataL, specializationMapEntriesL, T1r, T1c, T2r, T2c);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfoL{};
    shaderStageCreateInfoL.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfoL.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfoL.module = shaderModuleL;
    shaderStageCreateInfoL.pName = "main";
    shaderStageCreateInfoL.pSpecializationInfo = &specializationInfoL;

    VkPipelineLayout pipelineLayoutL{};
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayoutL);
    assert(res == VK_SUCCESS);

    VkPipeline pipeline{};
    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.stage = shaderStageCreateInfoL;
    computePipelineCreateInfo.layout = pipelineLayoutL;
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    res = vkCreateComputePipelines(device, NULL, 1, &computePipelineCreateInfo, nullptr, &pipeline);
    assert(res == VK_SUCCESS);

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    VkCommandPool commandPool{};
    res = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
    assert(res == VK_SUCCESS);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer{};
    res = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    assert(res == VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    assert(res == VK_SUCCESS);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutL, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdDispatch(commandBuffer, (T2c + 15) / 16, (T1r + 15) / 16, 1);
    
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
}

void relu(
    const std::string& shader_file_name,
    VkBuffer &buffer, 
    VkDeviceMemory &memory, 
    VkBuffer &stagingBuffer, 
    VkDevice device, 
    const uint32_t bufferSize, 
    VkPhysicalDevice physicalDevice, 
    VkQueue queue, 
    VkDescriptorSetLayout descriptorSetLayout, 
    VkDescriptorSet& descriptorSet, 
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo,
    uint32_t queueFamilyIndex, 
    VkResult res, 
    uint32_t t1Size, 
    uint32_t t1Offset,
    uint32_t T1r, 
    uint32_t T1c
) {
    updateDescriptorSetRelu(device, descriptorSet, buffer, t1Size, t1Offset);

    auto computeShaderCodeL = readSPV(shader_file_name);

    VkShaderModule shaderModuleL{};
    VkShaderModuleCreateInfo shaderModuleCreateInfoL{};
    shaderModuleCreateInfoL.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfoL.codeSize = computeShaderCodeL.size();
    shaderModuleCreateInfoL.pCode = reinterpret_cast<const uint32_t*>(computeShaderCodeL.data());
    res = vkCreateShaderModule(device, &shaderModuleCreateInfoL, nullptr, &shaderModuleL);
    assert(res == VK_SUCCESS);

    SpecializationDataRelu specDataL{};
    VkSpecializationMapEntry specializationMapEntriesL[2]{};
    VkSpecializationInfo specializationInfoL{};
    createSpecializationMapEntriesRelu(&specializationInfoL, specDataL, specializationMapEntriesL, T1r, T1c);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfoL{};
    shaderStageCreateInfoL.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfoL.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfoL.module = shaderModuleL;
    shaderStageCreateInfoL.pName = "main";
    shaderStageCreateInfoL.pSpecializationInfo = &specializationInfoL;

    VkPipelineLayout pipelineLayoutL{};
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayoutL);
    assert(res == VK_SUCCESS);

    VkPipeline pipeline{};
    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.stage = shaderStageCreateInfoL;
    computePipelineCreateInfo.layout = pipelineLayoutL;
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    res = vkCreateComputePipelines(device, NULL, 1, &computePipelineCreateInfo, nullptr, &pipeline);
    assert(res == VK_SUCCESS);

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    VkCommandPool commandPool{};
    res = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
    assert(res == VK_SUCCESS);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer{};
    res = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    assert(res == VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    assert(res == VK_SUCCESS);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutL, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdDispatch(commandBuffer, (T1r + 15) / 16, 1, 1);
    
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
}

int main(int argc, char* argv[]) {    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <image_file_path>\n";
        return 1; 
    }

    std::string linear1_w_filename = "../nn_models/linear1_w.bin";
    std::string linear1_b_filename = "../nn_models/linear1_b.bin";
    std::string linear2_w_filename = "../nn_models/linear2_w.bin";
    std::string linear2_b_filename = "../nn_models/linear2_b.bin";
    const char* image_filename = argv[1];
    std::vector<float> linear1_w = readModelBin(linear1_w_filename);
    std::vector<float> linear1_b = readModelBin(linear1_b_filename);
    std::vector<float> linear2_w = readModelBin(linear2_w_filename);
    std::vector<float> linear2_b = readModelBin(linear2_b_filename);
    std::vector<float> input_tensor = readImage(image_filename);
    if (linear1_w.empty() || linear1_b.empty() || linear2_w.empty() || linear2_b.empty() || input_tensor.empty()) {
        std::cerr << "One or more model files or input tensor are empty or file error occurred. Exiting.\n";
        return 1;
    }
    std::cout << "Successfully read linear weights and biases and input tensor of sizes: " 
        << "LW1: " << linear1_w.size() << ", " 
        << "LB1: " << linear1_b.size() << ", " 
        << "LW2: " << linear2_w.size() << ", " 
        << "LB2: " << linear2_b.size() << ", " 
        << "Input: " << input_tensor.size() << std::endl;
        
    const uint INr = 784;
    const uint INc = 1;

    const uint LW1r = 128;
    const uint LW1c = 784;

    const uint LB1r = 128;
    const uint LB1c = 1;

    const uint LO1r = 128;
    const uint LO1c = 1;

    const uint LW2r = 10;
    const uint LW2c = 128;

    const uint LB2r = 10;
    const uint LB2c = 1;

    const uint LO2r = 10;
    const uint LO2c = 1;

    VkResult res;
    VkInstance instance;
    VkApplicationInfo appInfo{};
    VkInstanceCreateInfo createInfo{};
    VkPhysicalDeviceProperties deviceProperties{};
    VkPhysicalDeviceFeatures deviceFeatures{};
    VkPhysicalDevice physicalDevice{};
    uint32_t queueFamilyIndex = 0;
    initializeVulkan(instance, appInfo, createInfo, physicalDevice, deviceProperties, deviceFeatures, &queueFamilyIndex, res);

    VkDeviceQueueCreateInfo queueCreateInfo{};    
    VkDeviceCreateInfo deviceCreateInfo{};
    VkDevice device;
    createDevice(queueCreateInfo, deviceCreateInfo, physicalDevice, queueFamilyIndex, deviceFeatures, device, res);

    VkQueue queue{};
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    VkBuffer stagingBuffer{};

    const uint outputSizeL1 = LW1r * INc;
    const uint outputSizeL2 = LW2r * LO1c;

    // (linear1_w.size() + input_tensor.size() + outputSizeL1 + linear1_b.size() + linear2_w.size() + linear2_b.size() + outputSizeL2)
    const uint32_t bufferSize = (linear1_w.size() + input_tensor.size() + linear1_b.size() + outputSizeL1 + linear2_w.size() + linear2_b.size() + outputSizeL2) * sizeof(float);

    initializeBufferMemDesc(
        device, 
        physicalDevice, 
        queue, 
        queueFamilyIndex,
        bufferSize,
        res,
        linear1_w, 
        input_tensor, 
        linear1_b,
        linear2_w, 
        linear2_b,
        outputSizeL1,
        outputSizeL2,
        buffer, 
        memory
    );

    VkDescriptorSetLayout descriptorSetLayoutLinear{};
    VkDescriptorPool descriptorPoolLinear{};
    VkDescriptorSet descriptorSetLinear{};
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfoLinear{};

    createDescriptorSetLinear(
        device, 
        descriptorSetLayoutLinear, 
        descriptorPoolLinear, 
        descriptorSetLinear, 
        descriptorSetAllocateInfoLinear,
        res
    );  

    res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfoLinear, &descriptorSetLinear);
    assert(res == VK_SUCCESS);

    VkDescriptorSetLayout descriptorSetLayoutRelu{};
    VkDescriptorPool descriptorPoolRelu{};
    VkDescriptorSet descriptorSetRelu{};
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfoRelu{};

    createDescriptorSetRelu(
        device, 
        descriptorSetLayoutRelu, 
        descriptorPoolRelu, 
        descriptorSetRelu, 
        descriptorSetAllocateInfoRelu,
        res
    );
    
    res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfoRelu, &descriptorSetRelu);
    assert(res == VK_SUCCESS);

    uint32_t offsetLinear1W = 0;
    uint32_t offsetInput = offsetLinear1W + linear1_w.size();
    uint32_t offsetOutputL1 = offsetInput + input_tensor.size();
    uint32_t offsetLinear1B = offsetOutputL1 + outputSizeL1;
    uint32_t offsetLinear2W = offsetLinear1B + linear1_b.size();
    uint32_t offsetLinear2B = offsetLinear2W + linear2_w.size();
    uint32_t offsetOutputL2 = offsetLinear2B + linear2_b.size();
    
    // linear 1
    linearOperation(
        "MNIST-linear_w.spv",
        buffer, 
        memory, 
        stagingBuffer, 
        device, 
        bufferSize, 
        physicalDevice, 
        queue, 
        descriptorSetLayoutLinear,
        descriptorSetLinear,
        descriptorSetAllocateInfoLinear,
        queueFamilyIndex, 
        res, 
        linear1_w.size(),
        offsetLinear1W,
        input_tensor.size(),
        offsetInput,
        outputSizeL1,
        offsetOutputL1,
        LW1r, 
        LW1c, 
        INr, 
        INc
    );

    // bias 1
    linearOperation(
        "MNIST-linear_b.spv",
        buffer, 
        memory, 
        stagingBuffer, 
        device, 
        bufferSize, 
        physicalDevice, 
        queue, 
        descriptorSetLayoutLinear,
        descriptorSetLinear,
        descriptorSetAllocateInfoLinear,
        queueFamilyIndex, 
        res, 
        linear1_b.size(),
        offsetLinear1B,
        outputSizeL1,
        offsetOutputL1,
        outputSizeL1,
        offsetOutputL1,
        LW1r, 
        LW1c, 
        LO1r, 
        LO1c
    );

    // relu 1
    relu(
        "MNIST-relu.spv",
        buffer, 
        memory, 
        stagingBuffer, 
        device, 
        bufferSize, 
        physicalDevice, 
        queue, 
        descriptorSetLayoutRelu,
        descriptorSetRelu,
        descriptorSetAllocateInfoRelu,
        queueFamilyIndex, 
        res, 
        outputSizeL1,
        offsetOutputL1,
        LO1r, 
        LO1c
    );

    // linear 2
    linearOperation(
        "MNIST-linear_w.spv",
        buffer, 
        memory, 
        stagingBuffer, 
        device, 
        bufferSize, 
        physicalDevice, 
        queue, 
        descriptorSetLayoutLinear,
        descriptorSetLinear,
        descriptorSetAllocateInfoLinear,
        queueFamilyIndex, 
        res, 
        linear2_w.size(),
        offsetLinear2W,
        outputSizeL1,
        offsetOutputL1,
        outputSizeL2,
        offsetOutputL2,
        LW2r, 
        LW2c, 
        LO1r, 
        LO1c
    );

    // bias 2
    linearOperation(
        "MNIST-linear_b.spv",
        buffer, 
        memory, 
        stagingBuffer, 
        device, 
        bufferSize, 
        physicalDevice, 
        queue, 
        descriptorSetLayoutLinear,
        descriptorSetLinear,
        descriptorSetAllocateInfoLinear,
        queueFamilyIndex, 
        res, 
        linear2_b.size(),
        offsetLinear2B,
        outputSizeL2,
        offsetOutputL2,
        outputSizeL2,
        offsetOutputL2,
        LW2r, 
        LW2c, 
        LB2r, 
        LB2c
    );

    std::cout << "OK: " << res << std::endl;

    void* mappedMemory = NULL;
    vkMapMemory(device, memory, 0, bufferSize, 0, &mappedMemory);
    float* mappedResults = (float*)((char*)mappedMemory + (offsetOutputL2 * sizeof(float)));

    std::cout << "Result values: " << std::endl;
    for (uint32_t i = 0; i < LO2r * LO2c; ++i) {
        std::cout << mappedResults[i] << ", ";
    }
    std::cout << std::endl;

    uint predicted_label = 0;
    float max_value = mappedResults[0];
    for (uint32_t i = 1; i < LO2r * LO2c; ++i) {
        if (mappedResults[i] > max_value) {
            max_value = mappedResults[i];
            predicted_label = i;
        }
    }
    std::cout << "Predicted label: " << predicted_label << std::endl;

    return 0;
}

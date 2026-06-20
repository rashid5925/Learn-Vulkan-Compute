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
    uint32_t LW1r;
    uint32_t LW1c;
    uint32_t INr;
    uint32_t INc;
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

void createSpecializationMapEntries(VkSpecializationInfo* specializationInfoL, SpecializationData& specDataL, VkSpecializationMapEntry (&specializationMapEntriesL)[4], uint32_t LW1r, uint32_t LW1c, uint32_t INr, uint32_t INc)
{
    specDataL.LW1r = LW1r;
    specDataL.LW1c = LW1c;
    specDataL.INr = INr;
    specDataL.INc = INc;

    specializationMapEntriesL[0] = { 0, offsetof(SpecializationData, LW1r), sizeof(uint32_t) };
    specializationMapEntriesL[1] = { 1, offsetof(SpecializationData, LW1c), sizeof(uint32_t) };
    specializationMapEntriesL[2] = { 2, offsetof(SpecializationData, INr),  sizeof(uint32_t) };
    specializationMapEntriesL[3] = { 3, offsetof(SpecializationData, INc),  sizeof(uint32_t) };

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

std::vector<float> linearW(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex, VkResult res, const std::vector<float>& linear1_w, const std::vector<float>& input_tensor, uint32_t LW1r, uint32_t LW1c, uint32_t INr, uint32_t INc) {
    const uint outputSize = LW1r * INc;
    const uint32_t bufferSize = (linear1_w.size() + input_tensor.size() + outputSize) * sizeof(float);

    VkBufferCreateInfo localBufferCreateInfo{};
    VkBuffer stagingBuffer{};
    VkDeviceMemory stagingMemory{};
    createBuffer(localBufferCreateInfo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, device, bufferSize, res);
    createMemory(device, physicalDevice, stagingBuffer, stagingMemory, res);

    res = vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
    assert(res == VK_SUCCESS);

    VkBufferCreateInfo bufferCreateInfo{};
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    createBuffer(bufferCreateInfo, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, buffer, device, bufferSize, res);
    createMemory(device, physicalDevice, buffer, memory, res);

    res = vkBindBufferMemory(device, buffer, memory, 0);
    assert(res == VK_SUCCESS);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingLW1{};
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingT{};
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingOutput{};

    createDescriptorLayoutBinding(descriptorSetLayoutBindingLW1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    createDescriptorLayoutBinding(descriptorSetLayoutBindingT, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    createDescriptorLayoutBinding(descriptorSetLayoutBindingOutput, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingsL[3] = {
        descriptorSetLayoutBindingLW1,
        descriptorSetLayoutBindingT,
        descriptorSetLayoutBindingOutput,
    };

    VkDescriptorSetLayout descriptorSetLayout{};
    createDescriptorSetLayout(device, descriptorSetLayoutBindingsL, 3, descriptorSetLayout, res);

    VkDescriptorPool descriptorPool{};
    createDescriptorPool(device, descriptorPool, 3,res);

    VkDescriptorSet descriptorSet{};

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; 
    descriptorSetAllocateInfo.descriptorPool = descriptorPool; 
    descriptorSetAllocateInfo.descriptorSetCount = 1; 
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;


    VkDescriptorBufferInfo descriptorBufferInfoLW1;
    VkDescriptorBufferInfo descriptorBufferInfoInputTensor;
    VkDescriptorBufferInfo descriptorBufferInfoOutput;
    createDescriptorBufferInfo(descriptorBufferInfoLW1, buffer, linear1_w.size() * sizeof(float), 0);
    createDescriptorBufferInfo(descriptorBufferInfoInputTensor, buffer, input_tensor.size() * sizeof(float), linear1_w.size() * sizeof(float));
    createDescriptorBufferInfo(descriptorBufferInfoOutput, buffer, outputSize * sizeof(float), (input_tensor.size() + linear1_w.size()) * sizeof(float));

    std::vector<VkDescriptorBufferInfo> descriptorBufferInfosL {
        descriptorBufferInfoLW1,
        descriptorBufferInfoInputTensor,
        descriptorBufferInfoOutput,
    };

    res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
    assert(res == VK_SUCCESS);

    std::vector<VkWriteDescriptorSet> writeDescriptorSetL(3, VkWriteDescriptorSet{});
    for (uint32_t i = 0; i < writeDescriptorSetL.size(); ++i) {
        writeDescriptorSetL[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSetL[i].dstSet = descriptorSet; 
        writeDescriptorSetL[i].dstBinding = i; 
        writeDescriptorSetL[i].descriptorCount = 1; 
        writeDescriptorSetL[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
        writeDescriptorSetL[i].pBufferInfo = &descriptorBufferInfosL[i];
    }

    vkUpdateDescriptorSets(device, 3, writeDescriptorSetL.data(), 0, NULL);

    // 2. Map and transfer the flat buffer smoothly
    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &mappedData);
    memcpy(mappedData, linear1_w.data(), linear1_w.size() * sizeof(float));
    memcpy((char*)mappedData + linear1_w.size() * sizeof(float), input_tensor.data(), input_tensor.size() * sizeof(float));
    vkUnmapMemory(device, stagingMemory);

    auto computeShaderCodeL = readSPV("MNIST-linear_w.spv");

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
    createSpecializationMapEntries(&specializationInfoL, specDataL, specializationMapEntriesL, LW1r, LW1c, INr, INc);

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

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutL, 0, 1, &descriptorSet, 0, NULL);

    vkCmdDispatch(commandBuffer, (INc + 15) / 16, (LW1r + 15) / 16, 1);
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

    void* mappedMemory = NULL;
    vkMapMemory(device, memory, 0, bufferSize, 0, &mappedMemory);
    float* mappedResults = (float*)((char*)mappedMemory + ((linear1_w.size() + input_tensor.size()) * sizeof(float)));

    std::vector<float> result(mappedResults, mappedResults + outputSize);
    
    return result;
}

std::vector<float> linearB(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex, VkResult res, const std::vector<float>& linear1_b, const std::vector<float>& linear1_o, uint32_t LB1r, uint32_t LB1c, uint32_t LOr, uint32_t LOc) {
    const uint outputSize = LOr * LOc;
    const uint32_t bufferSize = (linear1_b.size() + linear1_o.size() + outputSize) * sizeof(float);

    VkBufferCreateInfo localBufferCreateInfo{};
    VkBuffer stagingBuffer{};
    VkDeviceMemory stagingMemory{};
    createBuffer(localBufferCreateInfo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, device, bufferSize, res);
    createMemory(device, physicalDevice, stagingBuffer, stagingMemory, res);

    res = vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
    assert(res == VK_SUCCESS);

    VkBufferCreateInfo bufferCreateInfo{};
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    createBuffer(bufferCreateInfo, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, buffer, device, bufferSize, res);
    createMemory(device, physicalDevice, buffer, memory, res);

    res = vkBindBufferMemory(device, buffer, memory, 0);
    assert(res == VK_SUCCESS);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingLB1{};
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingT{};
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingOutput{};

    createDescriptorLayoutBinding(descriptorSetLayoutBindingLB1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    createDescriptorLayoutBinding(descriptorSetLayoutBindingT, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    createDescriptorLayoutBinding(descriptorSetLayoutBindingOutput, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingsL[3] = {
        descriptorSetLayoutBindingLB1,
        descriptorSetLayoutBindingT,
        descriptorSetLayoutBindingOutput,
    };

    VkDescriptorSetLayout descriptorSetLayout{};
    createDescriptorSetLayout(device, descriptorSetLayoutBindingsL, 3, descriptorSetLayout, res);

    VkDescriptorPool descriptorPool{};
    createDescriptorPool(device, descriptorPool, 3, res);

    VkDescriptorSet descriptorSet{};

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; 
    descriptorSetAllocateInfo.descriptorPool = descriptorPool; 
    descriptorSetAllocateInfo.descriptorSetCount = 1; 
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorBufferInfo descriptorBufferInfoLB1;
    VkDescriptorBufferInfo descriptorBufferInfoLO;
    VkDescriptorBufferInfo descriptorBufferInfoOutput;
    createDescriptorBufferInfo(descriptorBufferInfoLB1, buffer, linear1_b.size() * sizeof(float), 0);
    createDescriptorBufferInfo(descriptorBufferInfoLO, buffer, outputSize * sizeof(float), linear1_b.size() * sizeof(float));
    createDescriptorBufferInfo(descriptorBufferInfoOutput, buffer, outputSize * sizeof(float), (linear1_o.size() + linear1_b.size()) * sizeof(float));

    std::vector<VkDescriptorBufferInfo> descriptorBufferInfosL {
        descriptorBufferInfoLB1,
        descriptorBufferInfoLO,
        descriptorBufferInfoOutput,
    };

    res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
    assert(res == VK_SUCCESS);

    std::vector<VkWriteDescriptorSet> writeDescriptorSetL(3, VkWriteDescriptorSet{});
    for (uint32_t i = 0; i < writeDescriptorSetL.size(); ++i) {
        writeDescriptorSetL[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSetL[i].dstSet = descriptorSet; 
        writeDescriptorSetL[i].dstBinding = i; 
        writeDescriptorSetL[i].descriptorCount = 1; 
        writeDescriptorSetL[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
        writeDescriptorSetL[i].pBufferInfo = &descriptorBufferInfosL[i];
    }

    vkUpdateDescriptorSets(device, 3, writeDescriptorSetL.data(), 0, NULL);

    // 2. Map and transfer the flat buffer smoothly
    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &mappedData);
    memcpy(mappedData, linear1_b.data(), linear1_b.size() * sizeof(float));
    memcpy((char*)mappedData + linear1_b.size() * sizeof(float), linear1_o.data(), linear1_o.size() * sizeof(float));
    vkUnmapMemory(device, stagingMemory);

    auto computeShaderCodeL = readSPV("MNIST-linear1_b.spv");

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
    createSpecializationMapEntries(&specializationInfoL, specDataL, specializationMapEntriesL, LB1r, LB1c, LOr, LOc);

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

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutL, 0, 1, &descriptorSet, 0, NULL);

    vkCmdDispatch(commandBuffer, (LOc + 15) / 16, (LB1r + 15) / 16, 1);
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

    void* mappedMemory = NULL;
    vkMapMemory(device, memory, 0, bufferSize, 0, &mappedMemory);
    float* mappedResults = (float*)((char*)mappedMemory + ((linear1_b.size() + linear1_o.size()) * sizeof(float)));

    std::vector<float> result(mappedResults, mappedResults + outputSize);
    
    return result;
}

std::vector<float> relu(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex, VkResult res, const std::vector<float>& linear1_o, uint32_t LOr, uint32_t LOc) {
    const uint outputSize = linear1_o.size();
    const uint32_t bufferSize = linear1_o.size() * sizeof(float);

    VkBufferCreateInfo localBufferCreateInfo{};
    VkBuffer stagingBuffer{};
    VkDeviceMemory stagingMemory{};
    createBuffer(localBufferCreateInfo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, device, bufferSize, res);
    createMemory(device, physicalDevice, stagingBuffer, stagingMemory, res);

    res = vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
    assert(res == VK_SUCCESS);

    VkBufferCreateInfo bufferCreateInfo{};
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    createBuffer(bufferCreateInfo, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, buffer, device, bufferSize, res);
    createMemory(device, physicalDevice, buffer, memory, res);

    res = vkBindBufferMemory(device, buffer, memory, 0);
    assert(res == VK_SUCCESS);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingRelu{};

    createDescriptorLayoutBinding(descriptorSetLayoutBindingRelu, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingsRelu[1] = {
        descriptorSetLayoutBindingRelu,
    };

    VkDescriptorSetLayout descriptorSetLayout{};
    createDescriptorSetLayout(device, descriptorSetLayoutBindingsRelu, 1, descriptorSetLayout, res);

    VkDescriptorPool descriptorPool{};
    createDescriptorPool(device, descriptorPool, 1, res);

    VkDescriptorSet descriptorSet{};

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; 
    descriptorSetAllocateInfo.descriptorPool = descriptorPool; 
    descriptorSetAllocateInfo.descriptorSetCount = 1; 
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorBufferInfo descriptorBufferInfoRelu;
    createDescriptorBufferInfo(descriptorBufferInfoRelu, buffer, linear1_o.size() * sizeof(float), 0);

    std::vector<VkDescriptorBufferInfo> descriptorBufferInfosRelu {
        descriptorBufferInfoRelu,
    };

    res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
    assert(res == VK_SUCCESS);

    std::vector<VkWriteDescriptorSet> writeDescriptorSetRelu(1, VkWriteDescriptorSet{});
    for (uint32_t i = 0; i < writeDescriptorSetRelu.size(); ++i) {
        writeDescriptorSetRelu[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSetRelu[i].dstSet = descriptorSet; 
        writeDescriptorSetRelu[i].dstBinding = i; 
        writeDescriptorSetRelu[i].descriptorCount = 1; 
        writeDescriptorSetRelu[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
        writeDescriptorSetRelu[i].pBufferInfo = &descriptorBufferInfosRelu[i];
    }

    vkUpdateDescriptorSets(device, 1, writeDescriptorSetRelu.data(), 0, NULL);

    // 2. Map and transfer the flat buffer smoothly
    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &mappedData);
    memcpy(mappedData, linear1_o.data(), linear1_o.size() * sizeof(float));
    vkUnmapMemory(device, stagingMemory);

    auto computeShaderCodeRelu = readSPV("MNIST-ReLU.spv");

    VkShaderModule shaderModuleRelu{};
    VkShaderModuleCreateInfo shaderModuleCreateInfoRelu{};
    shaderModuleCreateInfoRelu.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfoRelu.codeSize = computeShaderCodeRelu.size();
    shaderModuleCreateInfoRelu.pCode = reinterpret_cast<const uint32_t*>(computeShaderCodeRelu.data());
    res = vkCreateShaderModule(device, &shaderModuleCreateInfoRelu, nullptr, &shaderModuleRelu);
    assert(res == VK_SUCCESS);

    SpecializationDataRelu specDataRelu{};
    VkSpecializationMapEntry specializationMapEntriesRelu[2]{};
    VkSpecializationInfo specializationInfoRelu{};
    createSpecializationMapEntriesRelu(&specializationInfoRelu, specDataRelu, specializationMapEntriesRelu, LOr, LOc);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfoRelu{};
    shaderStageCreateInfoRelu.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfoRelu.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfoRelu.module = shaderModuleRelu;
    shaderStageCreateInfoRelu.pName = "main";
    shaderStageCreateInfoRelu.pSpecializationInfo = &specializationInfoRelu;

    VkPipelineLayout pipelineLayoutRelu{};
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayoutRelu);
    assert(res == VK_SUCCESS);

    VkPipeline pipeline{};
    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.stage = shaderStageCreateInfoRelu;
    computePipelineCreateInfo.layout = pipelineLayoutRelu;
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

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutRelu, 0, 1, &descriptorSet, 0, NULL);

    vkCmdDispatch(commandBuffer, (LOr + 15) / 16, 1, 1);
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

    void* mappedMemory = NULL;
    vkMapMemory(device, memory, 0, bufferSize, 0, &mappedMemory);
    float* mappedResults = (float*)((char*)mappedMemory);

    std::vector<float> result(mappedResults, mappedResults + outputSize);

    return result;
}


int main(int argc, char* argv[]) {    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <image_file_path>\n";
        return 1; 
    }

    const uint N = 10;
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

    std::vector<float> flatOutput(N, 0.0f);

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
    
    std::vector<float> linear1_o = linearW(device, physicalDevice, queue, queueFamilyIndex, res, linear1_w, input_tensor, LW1r, LW1c, INr, INc);
    linear1_o = linearB(device, physicalDevice, queue, queueFamilyIndex, res, linear1_b, linear1_o, LB1r, LB1c, LO1r, LO1c);
    linear1_o = relu(device, physicalDevice, queue, queueFamilyIndex, res, linear1_o, LO1r, LO1c);

    std::vector<float> linear2_o = linearW(device, physicalDevice, queue, queueFamilyIndex, res, linear2_w, linear1_o, LW2r, LW2c, LO1r, LO1c);
    linear2_o = linearB(device, physicalDevice, queue, queueFamilyIndex, res, linear2_b, linear2_o, LB2r, LB2c, LB2r, LB2c);

    std::cout << "OK: " << res << std::endl;

    std::cout << "Result values: " << std::endl;
    for (uint32_t i = 0; i < linear2_o.size(); ++i) {
        std::cout << linear2_o[i] << ", ";
    }
    std::cout << std::endl;

    uint predicted_label = 0;
    float max_value = linear2_o[0];
    for (uint32_t i = 1; i < linear2_o.size(); ++i) {
        if (linear2_o[i] > max_value) {
            max_value = linear2_o[i];
            predicted_label = i;
        }
    }
    std::cout << "Predicted label: " << predicted_label << std::endl;

    return 0;
}

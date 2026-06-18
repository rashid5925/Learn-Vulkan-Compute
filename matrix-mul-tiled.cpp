#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <assert.h>
#include <vulkan/vulkan.hpp>

// const size_t N = 10;
// float dataA[N][N] = {
//     {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f},
//     {11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f, 20.0f},
//     {21.0f, 22.0f, 23.0f, 24.0f, 25.0f, 26.0f, 27.0f, 28.0f, 29.0f, 30.0f},
//     {31.0f, 32.0f, 33.0f, 34.0f, 35.0f, 36.0f, 37.0f, 38.0f, 39.0f, 40.0f},
//     {41.0f, 42.0f, 43.0f, 44.0f, 45.0f, 46.0f, 47.0f, 48.0f, 49.0f, 50.0f},
//     {51.0f, 52.0f, 53.0f, 54.0f, 55.0f, 56.0f, 57.0f, 58.0f, 59.0f, 60.0f},
//     {61.0f, 62.0f, 63.0f, 64.0f, 65.0f, 66.0f, 67.0f, 68.0f, 69.0f, 70.0f},
//     {71.0f, 72.0f, 73.0f, 74.0f, 75.0f, 76.0f, 77.0f, 78.0f, 79.0f, 80.0f},
//     {81.0f, 82.0f, 83.0f, 84.0f, 85.0f, 86.0f, 87.0f, 88.0f, 89.0f, 90.0f},
//     {91.0f, 92.0f, 93.0f, 94.0f, 95.0f, 96.0f, 97.0f, 98.0f, 99.0f, 100.0f},
// };
// float dataB[N][N] = {
//     {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f},
//     {11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f, 20.0f},
//     {21.0f, 22.0f, 23.0f, 24.0f, 25.0f, 26.0f, 27.0f, 28.0f, 29.0f, 30.0f},
//     {31.0f, 32.0f, 33.0f, 34.0f, 35.0f, 36.0f, 37.0f, 38.0f, 39.0f, 40.0f},
//     {41.0f, 42.0f, 43.0f, 44.0f, 45.0f, 46.0f, 47.0f, 48.0f, 49.0f, 50.0f},
//     {51.0f, 52.0f, 53.0f, 54.0f, 55.0f, 56.0f, 57.0f, 58.0f, 59.0f, 60.0f},
//     {61.0f, 62.0f, 63.0f, 64.0f, 65.0f, 66.0f, 67.0f, 68.0f, 69.0f, 70.0f},
//     {71.0f, 72.0f, 73.0f, 74.0f, 75.0f, 76.0f, 77.0f, 78.0f, 79.0f, 80.0f},
//     {81.0f, 82.0f, 83.0f, 84.0f, 85.0f, 86.0f, 87.0f, 88.0f, 89.0f, 90.0f},
//     {91.0f, 92.0f, 93.0f, 94.0f, 95.0f, 96.0f, 97.0f, 98.0f, 99.0f, 100.0f},
// };
// float results[N][N];
// int bufferSize = ((N * N) + (N * N) + (N * N)) * sizeof(float);

std::vector<float> readCSV(const std::string& filename, size_t& outN) {
    std::vector<float> flatMatrix;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for reading.\n";
        outN = 0;
        return flatMatrix;
    }

    std::string line;
    size_t rowCount = 0;
    size_t colCount = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue; 

        std::stringstream ss(line);
        std::string value;
        size_t currentCw = 0;

        while (std::getline(ss, value, ',')) {
            try {
                flatMatrix.push_back(std::stof(value)); 
                currentCw++;
            } catch (const std::exception& e) {
                std::cerr << "Warning: Skipping invalid token '" << value << "'\n";
            }
        }

        if (currentCw > 0) {
            if (rowCount == 0) {
                colCount = currentCw; 
            }
            rowCount++;
        }
    }

    file.close();

    if (rowCount != colCount) {
        std::cerr << "Warning: Matrix is not square! Rows: " << rowCount << ", Cols: " << colCount << "\n";
    }

    outN = rowCount;
    return flatMatrix;
}

bool writeCSV(const std::string& filename, float* mappedMemory, size_t rows, size_t cols) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing.\n";
        return false;
    }

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            file << mappedMemory[i * cols + j];
            if (j < cols - 1) {
                file << ",";
            }
        }
        file << "\n";
    }

    file.close();
    return true;
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

int main() {    
    std::string inputFilename = "../datasets/input_matrix.csv";
    std::string outputFilename = "../datasets/output_matrix.csv";
    std::cout << "Reading matrix from " << inputFilename << "...\n";
    size_t N;
    std::vector<float> flatInput = readCSV(inputFilename, N);
    if (flatInput.empty()) {
        std::cerr << "Matrix is empty or file error occurred. Exiting.\n";
        return 1;
    }
    std::cout << "Successfully read a matrix of size: " << N << "x" << N << "\n";

    const int bufferSize = ((N * N) + (N * N) + (N * N)) * sizeof(float);

    std::vector<float> flatOutput(N * N, 0.0f);

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
    VkBufferCreateInfo localBufferCreateInfo{};
    localBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uint32_t size = bufferSize;
    localBufferCreateInfo.size = size;
    localBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    localBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer{};
    res = vkCreateBuffer(device, &localBufferCreateInfo, nullptr, &stagingBuffer);
    assert(res == VK_SUCCESS);

    VkMemoryRequirements localMemoryRequiremnts{};
    vkGetBufferMemoryRequirements(device, stagingBuffer, &localMemoryRequiremnts);

    VkMemoryAllocateInfo localMemoryAllocateInfo{};
    localMemoryAllocateInfo.allocationSize = localMemoryRequiremnts.size;
    localMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    localMemoryAllocateInfo.memoryTypeIndex = -1;
    VkPhysicalDeviceMemoryProperties localMemoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &localMemoryProperties);
    VkMemoryPropertyFlags localRequired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < localMemoryProperties.memoryTypeCount; ++i) {
        if (
            (localMemoryRequiremnts.memoryTypeBits & (1 << i)) &&
            ((localMemoryProperties.memoryTypes[i].propertyFlags & localRequired) == localRequired)
        ) {
            localMemoryAllocateInfo.memoryTypeIndex = i;
            break;
        }
    }
    VkDeviceMemory stagingMemory{};
    res = vkAllocateMemory(device, &localMemoryAllocateInfo, nullptr, &stagingMemory);
    assert(res == VK_SUCCESS);

    res = vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer buffer{};
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
    VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
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
    descriptorBufferInfo1.range = N * N * sizeof(float);
    descriptorBufferInfo1.offset = N * N * sizeof(float) * 0;

    VkDescriptorBufferInfo descriptorBufferInfo2;
    descriptorBufferInfo2.buffer = buffer;
    descriptorBufferInfo2.range = N * N * sizeof(float);
    descriptorBufferInfo2.offset = N * N * sizeof(float) * 1;

    VkDescriptorBufferInfo descriptorBufferInfo3;
    descriptorBufferInfo3.buffer = buffer;
    descriptorBufferInfo3.range = N * N * sizeof(float);
    descriptorBufferInfo3.offset = N * N * sizeof(float) * 2;

    std::vector<VkDescriptorBufferInfo> descriptorBufferInfos {
        descriptorBufferInfo1,
        descriptorBufferInfo2,
        descriptorBufferInfo3,
    };

    res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
    assert(res == VK_SUCCESS);

    std::vector<VkWriteDescriptorSet> writeDescriptorSet(3, VkWriteDescriptorSet{});
    for (uint32_t i = 0; i < writeDescriptorSet.size(); ++i) {
        writeDescriptorSet[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet[i].dstSet = descriptorSet; 
        writeDescriptorSet[i].dstBinding = i; 
        writeDescriptorSet[i].descriptorCount = 1; 
        writeDescriptorSet[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
        writeDescriptorSet[i].pBufferInfo = &descriptorBufferInfos[i];
    }

    vkUpdateDescriptorSets(device, 3, writeDescriptorSet.data(), 0, NULL);

    // 2. Map and transfer the flat buffer smoothly
    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mappedData);
    size_t matrixBytes = N * N * sizeof(float);
    memcpy(mappedData, flatInput.data(), matrixBytes);
    memcpy((char*)mappedData + matrixBytes, flatInput.data(), matrixBytes);
    vkUnmapMemory(device, stagingMemory);

    auto computeShaderCode = readSPV("matrix-mul-tiled.spv");

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

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

    vkCmdDispatch(commandBuffer, (N + 15) / 16, (N + 15) / 16, 1);
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
    float* mappedResults = (float*)((char*)mappedMemory + (2 * N * N * sizeof(float)));

    writeCSV(outputFilename, mappedResults, N, N);
    // std::cout << "Result: " << std::endl;
    // for (uint32_t i = 0; i < N; ++i) {
    //     for (uint32_t j = 0; j < N; ++j) {
    //         outputMatrix[i][j] = mappedResults[i * N + j];
    //         std::cout << outputMatrix[i][j] << ", ";
    //     }
    //     std::cout << std::endl;
    // }

    std::cout << std::endl;

    return 0;
}

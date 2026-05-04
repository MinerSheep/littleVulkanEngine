#include "lve_pipeline.hpp"

// std
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace lve {

LvePipeline::LvePipeline(
    LveDevice& device,
    const std::string& vertFilepath,
    const std::string& fragFilepath,
    const PipelineConfigInfo& configInfo) 
      : lveDevice(device)
    {
      createGraphicsPipeline(vertFilepath, fragFilepath, configInfo);
    }

    LvePipeline::~LvePipeline() {}

    PipelineConfigInfo LvePipeline::defaultPipelineConfigInfo(uint32_t width, uint32_t height) {
      PipelineConfigInfo pipelineInfo{};

      return pipelineInfo;
    }

    std::vector<char> LvePipeline::readFile(const std::string& filename) {
      // ate means ???, binary means that there wont be any file conversions
      // ate means when the file is opened, we seek to the end immediately - good for size
      // binary means we won't have unwanted text conversions
      std::ifstream file{filename, std::ios::ate | std::ios::binary};

      if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
      }

      size_t fileSize = (size_t)file.tellg();
      std::vector<char> buffer(fileSize);

      // seek to start of file
      file.seekg(0);
      file.read(buffer.data(), fileSize);

      file.close();

      return buffer;
    }

void LvePipeline::createGraphicsPipeline(
    const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo) {
  auto vertCode = readFile(vertFilepath);
  auto fragCode = readFile(fragFilepath);

  std::cout << "Vertex Shader Code Size: " << vertCode.size() << '\n';
  std::cout << "Fragment Shader Code Size: " << fragCode.size() << '\n';

  // more implementation to come I think
}

void LvePipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) 
{
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();

  // pass in our compiled vec<char> as a uint32 ptr - important to note this wouldnt be valid in c because char and int are diff sizes
  // reinterpret cast handles this issue
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  if (vkCreateShaderModule(lveDevice.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS)
    throw std::runtime_error("Failed to create shader module");
}

}  // namespace lve

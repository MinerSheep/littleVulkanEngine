#pragma once

#include <lve_device.hpp>

#include <string>
#include <vector>

namespace lve {
  // we want our app to be able to configure the pipeline and share the config info
  struct PipelineConfigInfo {};

class LvePipeline {
 public:
  LvePipeline(LveDevice &device, const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo);
  ~LvePipeline();

  // delete copy constructors
  LvePipeline(const LvePipeline& other) = delete;
  void operator=(const LvePipeline& other) = delete;

  // WRONG move operation, not copy
  LvePipeline(LvePipeline && other) = delete;

  static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t width, uint32_t height);

 private:
  static std::vector<char> readFile(const std::string& filename);

  void createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo);

  // shader code compiled to vec<char> and a shader module
  void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

  LveDevice& lveDevice;  // dangerous: if device is released before our pipeline is, dereferencing this would be unsafe
  VkPipeline graphicsPipeline;
  VkShaderModule vertShaderModule;
  VkShaderModule fragShaderModule;
};
}  // namespace lve

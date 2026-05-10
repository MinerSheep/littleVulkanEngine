#pragma once

#include <lve_device.hpp>

#include <string>
#include <vector>

namespace lve {
  // we want our app to be able to configure the pipeline and share the config info
  struct PipelineConfigInfo {
  PipelineConfigInfo(const PipelineConfigInfo&) = delete;
  PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

  VkPipelineViewportStateCreateInfo viewportInfo;
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo; // <-
  VkPipelineRasterizationStateCreateInfo rasterizationInfo;
  VkPipelineMultisampleStateCreateInfo multisampleInfo;
  VkPipelineColorBlendAttachmentState colorBlendAttachment;
  VkPipelineColorBlendStateCreateInfo colorBlendInfo;
  VkPipelineDepthStencilStateCreateInfo depthStencilInfo;

  std::vector<VkDynamicState> dynamicStateEnables;
  VkPipelineDynamicStateCreateInfo dynamicStateInfo;

  // Externally set
  VkPipelineLayout pipelineLayout = nullptr;
  VkRenderPass renderPass = nullptr;
  uint32_t subpass = 0;
};

class LvePipeline {
 public:
  LvePipeline(LveDevice &device, const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo);
  ~LvePipeline();

  // delete copy constructors
  LvePipeline(const LvePipeline& other) = delete;
  LvePipeline& operator=(const LvePipeline& other) = delete;

  // WRONG move operation, not copy
  LvePipeline(LvePipeline && other) = delete;

  void bind(VkCommandBuffer commandBuffer);

  static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);

 private:
  static std::vector<char> readFile(const std::string& filename);

  void createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo);

  // shader code compiled to vec<char> and a shader module
  void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

  LveDevice& lveDevice;  // dangerous: if device is released before our pipeline is, dereferencing this would be unsafe
  // However device should always outlive the pipeline

  VkPipeline graphicsPipeline;
  VkShaderModule vertShaderModule;
  VkShaderModule fragShaderModule;
};
}  // namespace lve

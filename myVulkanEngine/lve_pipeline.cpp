#include "lve_pipeline.hpp"

#include "lve_model.hpp"

// std
#include <assert.h>

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
    : lveDevice(device) {
  createGraphicsPipeline(vertFilepath, fragFilepath, configInfo);
}

LvePipeline::~LvePipeline() {
  // If no pipeline layout or render pass, it will crash now!
  vkDestroyShaderModule(lveDevice.device(), fragShaderModule, nullptr);
  vkDestroyShaderModule(lveDevice.device(), vertShaderModule, nullptr);

  vkDestroyPipeline(lveDevice.device(), graphicsPipeline, nullptr);
}

void LvePipeline::bind(VkCommandBuffer commandBuffer) {
  // Need to signal that this is a graphics pipeline we are binding to
  // (Other pipelines include COMPUTE and RAY TRACE pipelines)
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
}

void LvePipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo) {
  // Triangle list means every 3 vertices in the vertex shader will become a triangle
  // Triangle strip means every 3 consecutive vertices becomes a triangle { 1, 2, 3, 4 } -> { 1, 2,
  // 3 } & { 2, 3, 4 }
  configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

  // viewport info (viewport + scissor)
  configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  configInfo.viewportInfo.viewportCount = 1;
  configInfo.viewportInfo.pViewports = nullptr;
  configInfo.viewportInfo.scissorCount = 1;
  configInfo.viewportInfo.pScissors = nullptr;
  

  // The viewport is the transformation between our pipeline output & target image
  // Because vertex shader uses [-1, 1] for [-width, width] therefore this does that transformation

  // Scissor doesn't describe the view, however it does cut the view

  // Rasterization stage, break up triangle into fragments for each pixel overlap
  configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  configInfo.rasterizationInfo.depthClampEnable =
      VK_FALSE;  // forces z component to be [0, 1], 0 = behind camera, 1 = too far to see
  configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
  configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;  // fill triangle
  configInfo.rasterizationInfo.lineWidth = 1.0f;
  configInfo.rasterizationInfo.cullMode =
      VK_CULL_MODE_NONE;  // cull gives triangles front & back sides
  configInfo.rasterizationInfo.frontFace =
      VK_FRONT_FACE_CLOCKWISE;  // specifies which vertice direction is front facing
  configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
  configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;  // Optional
  configInfo.rasterizationInfo.depthBiasClamp = 0.0f;           // Optional
  configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;     // Optional

  // Edge handling, breaks grid to 4 pieces, each piece contained shades the whole pixel by 25%
  configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
  configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  configInfo.multisampleInfo.minSampleShading = 1.0f;           // Optional
  configInfo.multisampleInfo.pSampleMask = nullptr;             // Optional
  configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;  // Optional
  configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;       // Optional

  // Color blending - mix frag shader and frame buffer values
  configInfo.colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_A_BIT;
  configInfo.colorBlendAttachment.blendEnable = VK_FALSE;
  configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
  configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
  configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
  configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
  configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
  configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

  configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
  configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;  // Optional
  configInfo.colorBlendInfo.attachmentCount = 1;
  configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
  configInfo.colorBlendInfo.blendConstants[0] = 0.0f;  // Optional
  configInfo.colorBlendInfo.blendConstants[1] = 0.0f;  // Optional
  configInfo.colorBlendInfo.blendConstants[2] = 0.0f;  // Optional
  configInfo.colorBlendInfo.blendConstants[3] = 0.0f;  // Optional

  // Depth testing stores a value for every pixel - basically the z value of every fragment shader
  // Only updates the color and depth buffers for pixels where the cloud is the closest fragment
  configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
  configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
  configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
  configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
  configInfo.depthStencilInfo.minDepthBounds = 0.0f;  // Optional
  configInfo.depthStencilInfo.maxDepthBounds = 1.0f;  // Optional
  configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
  configInfo.depthStencilInfo.front = {};  // Optional
  configInfo.depthStencilInfo.back = {};   // Optional

  // Expect dynamic scissor and viewport later
  configInfo.dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
  configInfo.dynamicStateInfo.dynamicStateCount =
      static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
  configInfo.dynamicStateInfo.flags = 0;
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
    const std::string& vertFilepath,
    const std::string& fragFilepath,
    const PipelineConfigInfo& configInfo) {
  auto vertCode = readFile(vertFilepath);
  auto fragCode = readFile(fragFilepath);

  // std::cout << "Vertex Shader Code Size: " << vertCode.size() << '\n';
  // std::cout << "Fragment Shader Code Size: " << fragCode.size() << '\n';

  assert(
      configInfo.pipelineLayout != VK_NULL_HANDLE &&
      "Cannot create graphics pipeline without pipeline layout");
  assert(
      configInfo.renderPass != VK_NULL_HANDLE &&
      "Cannot create graphics pipeline without render pass");

  // more implementation to come I think
  createShaderModule(vertCode, &vertShaderModule);
  createShaderModule(fragCode, &fragShaderModule);

  VkPipelineShaderStageCreateInfo shaderStages[2];
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;  // this stage is for vertex shader
  shaderStages[0].module = vertShaderModule;
  shaderStages[0].pName = "main";
  shaderStages[0].flags = 0;
  shaderStages[0].pNext = nullptr;
  shaderStages[0].pSpecializationInfo = nullptr;

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // this stage is for vertex shader
  shaderStages[1].module = fragShaderModule;
  shaderStages[1].pName = "main";
  shaderStages[1].flags = 0;
  shaderStages[1].pNext = nullptr;
  shaderStages[1].pSpecializationInfo = nullptr;

  auto bindingDescriptions = LveModel::Vertex::getBindingDescriptions();
  auto attributeDescriptions = LveModel::Vertex::getAttributeDescriptions();
  // How to interpret vertex info - now adding vertex buffers tutorial 06
  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
  vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();


  // put it all together - this is tedious, however, now the pipeline is easily reproduceable
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
  pipelineInfo.pViewportState = &configInfo.viewportInfo;
  pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
  pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
  pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
  pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
  pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;  // optional - used for further configuration

  pipelineInfo.layout = configInfo.pipelineLayout;
  pipelineInfo.renderPass = configInfo.renderPass;
  pipelineInfo.subpass = configInfo.subpass;

  // These can optimize performance by pulling from an existing graphics pipeline
  pipelineInfo.basePipelineIndex = -1;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(
          lveDevice.device(),
          VK_NULL_HANDLE,
          1,
          &pipelineInfo,
          nullptr,
          &graphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create graphics pipeline");
  }
}

void LvePipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();

  // pass in our compiled vec<char> as a uint32 ptr - important to note this wouldnt be valid in c
  // because char and int are diff sizes reinterpret cast handles this issue
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  if (vkCreateShaderModule(lveDevice.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS)
    throw std::runtime_error("Failed to create shader module");
}

}  // namespace lve

#include "lve_descriptors.hpp"

// std
#include <cassert>
#include <stdexcept>

namespace lve {

// *************** Descriptor Set Layout Builder *********************

LveDescriptorSetLayout::Builder &LveDescriptorSetLayout::Builder::addBinding(
    uint32_t binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags,
    uint32_t count) {
  // Check that this is a new binding
  assert(bindings.count(binding) == 0 && "Binding already in use");

  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = descriptorType;
  layoutBinding.descriptorCount = count;
  layoutBinding.stageFlags = stageFlags;

  bindings[binding] = layoutBinding;

  return *this;
}

std::unique_ptr<LveDescriptorSetLayout> LveDescriptorSetLayout::Builder::build() const {
  return std::make_unique<LveDescriptorSetLayout>(lveDevice, bindings);
}

// *************** Descriptor Set Layout *********************

LveDescriptorSetLayout::LveDescriptorSetLayout(
    LveDevice &lveDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
    : lveDevice{lveDevice}, bindings{bindings} {

  // Vector of SetLayoutBindings
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
  for (auto kv : bindings) {
    setLayoutBindings.push_back(kv.second);
  }

  // CreateInfo
  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
  descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
  descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

  if (vkCreateDescriptorSetLayout(
          lveDevice.device(),
          &descriptorSetLayoutInfo,
          nullptr,
          &descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

LveDescriptorSetLayout::~LveDescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(lveDevice.device(), descriptorSetLayout, nullptr);
}

// *************** Descriptor Pool Builder *********************

LveDescriptorPool::Builder &LveDescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType, uint32_t count) {
  poolSizes.push_back({descriptorType, count});
  return *this;
}

LveDescriptorPool::Builder &LveDescriptorPool::Builder::setPoolFlags(
    VkDescriptorPoolCreateFlags flags) {
  poolFlags = flags;
  return *this;
}
LveDescriptorPool::Builder &LveDescriptorPool::Builder::setMaxSets(uint32_t count) {
  maxSets = count;
  return *this;
}

std::unique_ptr<LveDescriptorPool> LveDescriptorPool::Builder::build() const {
  return std::make_unique<LveDescriptorPool>(lveDevice, maxSets, poolFlags, poolSizes);
}

// *************** Descriptor Pool *********************

LveDescriptorPool::LveDescriptorPool(
    LveDevice &lveDevice,
    uint32_t maxSets,
    VkDescriptorPoolCreateFlags poolFlags,
    const std::vector<VkDescriptorPoolSize> &poolSizes)
    : lveDevice{lveDevice} {

  // CreateInfo
  VkDescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  descriptorPoolInfo.pPoolSizes = poolSizes.data();
  descriptorPoolInfo.maxSets = maxSets;
  descriptorPoolInfo.flags = poolFlags;

  if (vkCreateDescriptorPool(lveDevice.device(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

LveDescriptorPool::~LveDescriptorPool() {
  vkDestroyDescriptorPool(lveDevice.device(), descriptorPool, nullptr);
}

// https://vkguide.dev/docs/extra-chapter/abstracting_descriptors/
bool LveDescriptorPool::allocateDescriptorSet(
    const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const {

  // CreateInfo
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.pSetLayouts = &descriptorSetLayout;
  allocInfo.descriptorSetCount = 1;

  // Might want to create a "DescriptorPoolManager" class that handles the case when the Pool is full
  // with no descriptors remaining.
  // In that case, build a new pool whenever old pool fills up.
  if (vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, &descriptor) != VK_SUCCESS) {
    return false;
  }
  return true;
}

void LveDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
  vkFreeDescriptorSets(
      lveDevice.device(),
      descriptorPool,
      static_cast<uint32_t>(descriptors.size()),
      descriptors.data());
}

void LveDescriptorPool::resetPool() {
  vkResetDescriptorPool(lveDevice.device(), descriptorPool, 0);
}

// *************** Descriptor Writer *********************

LveDescriptorWriter::LveDescriptorWriter(LveDescriptorSetLayout &setLayout, LveDescriptorPool &pool)
    : setLayout{setLayout}, pool{pool} {}

LveDescriptorWriter &LveDescriptorWriter::writeBuffer(
    uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto &bindingDescription = setLayout.bindings[binding];

  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pBufferInfo = bufferInfo;
  write.descriptorCount = 1;

  writes.push_back(write);
  return *this;
}

LveDescriptorWriter &LveDescriptorWriter::writeImage(
    uint32_t binding, VkDescriptorImageInfo *imageInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto &bindingDescription = setLayout.bindings[binding];

  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pImageInfo = imageInfo;
  write.descriptorCount = 1;

  writes.push_back(write);
  return *this;
}

bool LveDescriptorWriter::build(VkDescriptorSet &set) {
  bool success = pool.allocateDescriptorSet(setLayout.getDescriptorSetLayout(), set);
  if (!success) {
    return false;
  }
  overwrite(set);
  return true;
}

// Tells all descriptor sets to write to the newly created set from BUILD
void LveDescriptorWriter::overwrite(VkDescriptorSet &set) {
  for (auto &write : writes) {
    write.dstSet = set;
  }
  vkUpdateDescriptorSets(pool.lveDevice.device(), writes.size(), writes.data(), 0, nullptr);
}

}  // namespace lve
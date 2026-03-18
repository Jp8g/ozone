#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

typedef uint32_t oz_window;

typedef struct oz_swapchain {
	VkSwapchainKHR vkSwapchain;
	VkImage* images;
	VkImageView* imageViews;
	VkExtent2D extents;
	uint32_t imageCount;
} oz_swapchain;

typedef struct oz_pipeline_create_info {
	VkDevice device;
	VkFormat format;
	VkShaderModule* shaderModules;
	VkShaderStageFlagBits* shaderStages;
	VkPushConstantRange* pushConstantRanges;
	VkDescriptorSetLayout* descriptorSetLayouts;
	uint32_t descriptorSetLayoutCount;
	uint32_t pushConstantRangeCount;
	uint8_t shaderCount;
} oz_pipeline_create_info;

typedef struct oz_pipeline {
	VkPipeline vkPipeline;
	VkPipelineCache pipelineCache;
} oz_pipeline;

typedef struct oz_gfx_context {
	VkDevice device;
	VkQueue graphicsQueue;
	VkFence inFlightFence;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkInstance vkInstance;
	VkSurfaceKHR vkSurface;
	VkPhysicalDevice physicalDevice;
	oz_swapchain* swapchain;
	oz_pipeline* pipelines;
	uint16_t pipelineCount;
} oz_gfx_context;

VkShaderModule oz_gfx_create_shader_module(VkDevice device, const char* path);
oz_gfx_context* oz_gfx_init(oz_window window);
VkFormat oz_gfx_get_format(oz_gfx_context* gfxContext);
uint32_t oz_gfx_find_memory_type(uint32_t typeFilter, VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties, VkMemoryPropertyFlags properties);
void oz_gfx_create_pipeline(oz_pipeline_create_info* pPipelineCreateInfo, oz_pipeline* pPipeline, VkPipelineLayout* pPipelineLayout);
void oz_gfx_create_swapchain(oz_gfx_context* gfxContext, VkFormat format, uint32_t width, uint32_t height, oz_swapchain* swapchain);
void oz_gfx_record_cmd_buffer(oz_gfx_context* gfxContext, uint32_t imageIndex, void (*record_cmd_buffer)(oz_gfx_context*));
void oz_gfx_render_frame(oz_gfx_context* gfxContext, void (*record_cmd_buffer)(oz_gfx_context*));
void oz_gfx_shutdown(oz_gfx_context* gfxContext);
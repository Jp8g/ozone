#include <fcntl.h>
#include <ozone/graphics.h>
#include <ozone/window.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

struct oz_gfx_context {
	VkDevice device;
	uint32_t swapChainImageCount;
	VkSwapchainKHR swapchain;
	VkImage* swapchainImages;
	VkImageView* swapchainImageViews;
	VkQueue graphicsQueue;
	VkFence inFlightFence;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkExtent2D extent;
	VkPipeline pipeline;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkInstance vkInstance;
	VkSurfaceKHR vkSurface;
};

char* oz_get_vk_driver_files() {
	const char* cacheHome = NULL;
	const char* home = NULL;
	FILE* cacheFile = NULL;
	char* path = NULL;

	if ((cacheHome = getenv("XDG_CACHE_HOME")) != NULL) {
		path = malloc(strlen(cacheHome) + sizeof("/ozone/vk_driver_cache.bin"));
		memcpy(path, cacheHome, strlen(cacheHome));
		memcpy(path + strlen(cacheHome), "/ozone", sizeof("/ozone"));
		mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
		memcpy(path + strlen(cacheHome) + sizeof("/ozone") - 1, "/vk_driver_cache.bin", sizeof("/vk_driver_cache.bin"));
	}
	else if ((home = getenv("HOME")) != NULL) {
		path = malloc(strlen(home) + sizeof("/.cache/ozone/vk_driver_cache.bin"));
		memcpy(path, home, strlen(home));
		memcpy(path + strlen(home), "/.cache/ozone", sizeof("/.cache/ozone"));
		mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
		memcpy(path + strlen(home) + sizeof("/.cache/ozone") - 1, "/vk_driver_cache.bin", sizeof("/vk_driver_cache.bin"));
	}

	cacheFile = fopen(path, "w+b");
	printf("Found cache: %s\n", path);
	free(path);
	
	if (!cacheFile) return NULL;
	fclose(cacheFile);

	DIR* dir = opendir("/sys/class/drm/");
	struct dirent* entry;

	if(!dir) {
		printf("Failed to open dir /sys/class/drm/\n");
		return NULL;
	}

	char cardPath[(sizeof("/sys/class/drm/") - 1) + sizeof("card") + 3 + (sizeof("/device/device"))] = "/sys/class/drm/";
	uint32_t nameLen = 0;

	while ((entry = readdir(dir))) {
		struct stat statBuffer;

		nameLen = strlen(entry->d_name);
		
		if (nameLen > sizeof("card") + 3) continue;

		memset(cardPath + (sizeof("/sys/class/drm/") - 1) + nameLen, '\0', sizeof(cardPath) - (sizeof("/sys/class/drm/") - 1) - nameLen);
		memcpy(cardPath + (sizeof("/sys/class/drm/") - 1), entry->d_name, nameLen + 1);

		if (stat(cardPath, &statBuffer) == 0 && S_ISDIR(statBuffer.st_mode) && strncmp(entry->d_name, "card", 4) == 0) {
			const char* ptr = entry->d_name + 4;

			if (*ptr == '\0') continue;

			bool valid = true;
			while (*ptr != '\0') {
				if (*ptr < '0' || *ptr > '9') {
					valid = false;
					break;
				}
				ptr++;
			}

			if (!valid) continue;

			printf("Found card %s in /sys/class/drm\n", entry->d_name);

			uint deviceID, vendorID;

			memcpy(cardPath + (sizeof("/sys/class/drm/") - 1) + nameLen, "/device/device", sizeof("/device/device"));
			FILE* deviceFile = fopen(cardPath, "r");
			if (!deviceFile) continue;
			fscanf(deviceFile, "%x", &deviceID);
			fclose(deviceFile);

			memcpy(cardPath + (sizeof("/sys/class/drm/") - 1) + nameLen, "/device/vendor", sizeof("/device/vendor"));
			FILE* vendorFile = fopen(cardPath, "r");
			if (!vendorFile) continue;
			fscanf(vendorFile, "%x", &vendorID);
			fclose(vendorFile);

			
		}
	}

	return (void*)1;
}

VkPhysicalDevice oz_get_physical_device(VkInstance vkInstance) {
	if (oz_get_vk_driver_files() == NULL) printf("Failed to get driver files\n");
	uint32_t deviceCount = 0;
	if (vkEnumeratePhysicalDevices(vkInstance, &deviceCount, NULL) != VK_SUCCESS || deviceCount == 0) {
		printf("no devices?\n");
		return NULL;
	}

	VkPhysicalDevice* physicalDevices = malloc(deviceCount * sizeof(VkPhysicalDevice));

	if (vkEnumeratePhysicalDevices(vkInstance, &deviceCount, physicalDevices) != VK_SUCCESS) {
		printf("failed to enumerate physical devices\n");
		return NULL;
	}

	VkPhysicalDevice physicalDevice = physicalDevices[0];

	for (uint32_t i = 0; i < deviceCount; i++) {
		physicalDevice = physicalDevices[i];

		VkPhysicalDeviceProperties2 physicalDeviceProperties;
		//VkPhysicalDeviceFeatures physicalDeviceFeatures;
		vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties);
		//vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

		uint64_t major = VK_VERSION_MAJOR(physicalDeviceProperties.properties.apiVersion);
		uint64_t minor = VK_VERSION_MINOR(physicalDeviceProperties.properties.apiVersion);
		//uint64_t patch = VK_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion);

		if (major == 1 && minor >= 3) {
			free(physicalDevices);
			return physicalDevice;
		}
	}

	free(physicalDevices);
	printf("No suitable device found");
	return NULL;
}

uint32_t oz_get_graphics_queue_family_index(VkPhysicalDevice physicalDevice, VkSurfaceKHR vkSurface) {
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

	VkQueueFamilyProperties* queueFamilyProperties = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties);

	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, vkSurface, &presentSupport);

			if (presentSupport) {
				free(queueFamilyProperties);
				return i;
			}
		}
	}

	free(queueFamilyProperties);
	printf("Device does not support graphics queue family\n");
	return UINT32_MAX;
}

VkShaderModule oz_create_shader_module(VkDevice device, const char* path) {
	FILE* file = fopen(path, "rb");
	if (!file) {
		printf("Failed to open file: %s\n", path);
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	uint64_t length = ftell(file);
	fseek(file, 0, SEEK_SET);

	uint32_t* code = malloc(length);
	fread(code, 1, length, file);
	fclose(file);

	VkShaderModule shaderModule;
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = length,
		.pCode = code,
	};

	if (vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, &shaderModule) != VK_SUCCESS) {
		printf("Failed to create shader module\n");
		return NULL;
	}

	free(code);
	return shaderModule;
}

oz_gfx_context* oz_graphics_system_init(oz_window window) {
	oz_gfx_context* gfxContext = malloc(sizeof(oz_gfx_context));

	VkApplicationInfo applicationInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Ozone",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "Ozone",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_3,
	};

	const char* instanceExtensionNames[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
	};

	VkInstanceCreateInfo instanceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &applicationInfo,
		.enabledExtensionCount = 2,
		.ppEnabledExtensionNames = instanceExtensionNames,
	};

	setenv("VK_DRIVER_FILES", "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json", 1);



    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

	if (vkCreateInstance(&instanceCreateInfo, NULL, &gfxContext->vkInstance) != VK_SUCCESS) {
		printf("couldnt create instance\n");
		return NULL;
	}

    clock_gettime(CLOCK_MONOTONIC, &end);

	float diff = (float)(end.tv_sec - start.tv_sec) * 1000.0f + (float)(end.tv_nsec - start.tv_nsec) / 1000000.0f;
	printf("vkCreateInstance: %f ms\n", diff);

	VkWaylandSurfaceCreateInfoKHR waylandSurfaceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
		.display = oz_get_wayland_display(),
		.surface = oz_get_wayland_surface(window),
	};

	vkCreateWaylandSurfaceKHR(gfxContext->vkInstance, &waylandSurfaceCreateInfo, NULL, &gfxContext->vkSurface);

	VkPhysicalDevice physicalDevice = oz_get_physical_device(gfxContext->vkInstance);
	if (!physicalDevice) {
		printf("ERR\n");
		return NULL;
	}

	uint32_t graphicsQueueFamilyIndex = oz_get_graphics_queue_family_index(physicalDevice, gfxContext->vkSurface);
	if (graphicsQueueFamilyIndex == UINT32_MAX) {
		printf("ERR\n");
		return NULL;
	}

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = graphicsQueueFamilyIndex,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};

	VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.dynamicRendering = VK_TRUE,
	};

	const char* deviceExtensionNames[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	VkDeviceCreateInfo deviceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pQueueCreateInfos = &deviceQueueCreateInfo,
		.queueCreateInfoCount = 1,
		.pNext = &physicalDeviceVulkan13Features,

		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = deviceExtensionNames,
	};

	vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &gfxContext->device);

	vkGetDeviceQueue(gfxContext->device, graphicsQueueFamilyIndex, 0, &gfxContext->graphicsQueue);

	uint32_t surfaceFormatCount = 0;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, gfxContext->vkSurface, &surfaceFormatCount, NULL) != VK_SUCCESS || surfaceFormatCount == 0) {
		printf("Failed to get formats\n");
		return NULL;
	}

	VkSurfaceFormatKHR* surfaceFormats = malloc(surfaceFormatCount * sizeof(VkSurfaceFormatKHR));
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, gfxContext->vkSurface, &surfaceFormatCount, surfaceFormats) != VK_SUCCESS) {
		printf("Failed to get formats\n");
		return NULL;
	}

	VkFormat format = 0;

	for (uint32_t i = 0; i < surfaceFormatCount; i++) {

		if (surfaceFormats[i].format == VK_FORMAT_B8G8R8_SRGB) {
			format = VK_FORMAT_B8G8R8_SRGB;
			break;
		}

		if (surfaceFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
			format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		}
	}

	if (!format) format = surfaceFormats[0].format;

	VkSurfaceFormatKHR surfaceFormat = {format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	gfxContext->extent.width = oz_window_get_width(window);
	gfxContext->extent.height = oz_window_get_height(window);

	uint32_t imageCount = 3;
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, gfxContext->vkSurface, &surfaceCapabilities);

	if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
		imageCount = surfaceCapabilities.maxImageCount;
	}
	else if (surfaceCapabilities.minImageCount != 0 && imageCount < surfaceCapabilities.minImageCount) {
		imageCount = surfaceCapabilities.minImageCount;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = gfxContext->vkSurface,
		.minImageCount = imageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = gfxContext->extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = presentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE,
	};

	if (vkCreateSwapchainKHR(gfxContext->device, &swapChainCreateInfo, NULL, &gfxContext->swapchain) != VK_SUCCESS) {
		printf("Failed to create swapchain\n");
		return NULL;
	}

	if (vkGetSwapchainImagesKHR(gfxContext->device, gfxContext->swapchain, &gfxContext->swapChainImageCount, NULL) != VK_SUCCESS || gfxContext->swapChainImageCount == 0) {
		printf("Failed to get swapchain images\n");
		return NULL;
	}

	gfxContext->swapchainImages = malloc(gfxContext->swapChainImageCount * sizeof(VkImage));

	if (vkGetSwapchainImagesKHR(gfxContext->device, gfxContext->swapchain, &gfxContext->swapChainImageCount, gfxContext->swapchainImages) != VK_SUCCESS) {
		printf("Failed to get swapchain images\n");
		return NULL;
	}

	gfxContext->swapchainImageViews = malloc(gfxContext->swapChainImageCount * sizeof(VkImageView));

	for (uint32_t i = 0; i < gfxContext->swapChainImageCount; i++) {
		VkImageViewCreateInfo swapchainImageViewCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = gfxContext->swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surfaceFormat.format,
			.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel = 0,
			.subresourceRange.levelCount = 1,
			.subresourceRange.baseArrayLayer = 0,
			.subresourceRange.layerCount = 1,
		};

		vkCreateImageView(gfxContext->device, &swapchainImageViewCreateInfo, NULL, &gfxContext->swapchainImageViews[i]);
	}

	VkSemaphoreCreateInfo semaphoreCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};
	VkFenceCreateInfo fenceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	vkCreateSemaphore(gfxContext->device, &semaphoreCreateInfo, NULL, &gfxContext->imageAvailableSemaphore);
	vkCreateSemaphore(gfxContext->device, &semaphoreCreateInfo, NULL, &gfxContext->renderFinishedSemaphore);
	vkCreateFence(gfxContext->device, &fenceCreateInfo, NULL, &gfxContext->inFlightFence);

	VkCommandPoolCreateInfo commandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = graphicsQueueFamilyIndex,
	};

	if (vkCreateCommandPool(gfxContext->device, &commandPoolCreateInfo, NULL, &gfxContext->commandPool) != VK_SUCCESS) {
		printf("Failed to create command pool\n");
		return NULL;
	}

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = gfxContext->commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	if (vkAllocateCommandBuffers(gfxContext->device, &commandBufferAllocateInfo, &gfxContext->commandBuffer) != VK_SUCCESS) {
		printf("Failed to allocate command buffers\n");
		return NULL;
	}

	VkPipelineCache pipelineCache;
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
	};

	if (vkCreatePipelineCache(gfxContext->device, &pipelineCacheCreateInfo, NULL, &pipelineCache) != VK_SUCCESS) {
		printf("Failed to create pipeline cache\n");
		return NULL;
	}

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &format,
	};

	VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	};

	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pDynamicStates = dynamicStates,
		.dynamicStateCount = 2,
	};

	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.lineWidth = 1.0f,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
	};

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
		.blendEnable = false,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &pipelineColorBlendAttachmentState,
	};

	VkShaderModule vertModule = oz_create_shader_module(gfxContext->device, "vert.spv");
	if (!vertModule) return NULL;

	VkShaderModule fragModule = oz_create_shader_module(gfxContext->device, "frag.spv");
	if (!fragModule) return NULL;

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertModule,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragModule,
			.pName = "main",
		},
	};

	VkPipelineLayout pipelineLayout;
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pushConstantRangeCount = 0,
	};

	if (vkCreatePipelineLayout(gfxContext->device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout) != VK_SUCCESS) {
		printf("Failed to create pipeline layout\n");
		return NULL;
	}

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.layout = pipelineLayout,
		.stageCount = 2,
		.pStages = pipelineShaderStageCreateInfos,
		.pVertexInputState = &pipelineVertexInputStateCreateInfo,
		.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo,
		.pViewportState = &pipelineViewportStateCreateInfo,
		.pRasterizationState = &pipelineRasterizationStateCreateInfo,
		.pMultisampleState = &pipelineMultisampleStateCreateInfo,
		.pColorBlendState = &pipelineColorBlendStateCreateInfo,
		.pDynamicState = &pipelineDynamicStateCreateInfo,
		.pNext = &pipelineRenderingCreateInfo,
	};

	if (vkCreateGraphicsPipelines(gfxContext->device, pipelineCache, 1, &graphicsPipelineCreateInfo, NULL, &gfxContext->pipeline) != VK_SUCCESS) {
		printf("Failed to create graphics pipelines\n");
		return NULL;
	}

	vkDestroyPipelineLayout(gfxContext->device, pipelineLayout, NULL);
	vkDestroyShaderModule(gfxContext->device, vertModule, NULL);
	vkDestroyShaderModule(gfxContext->device, fragModule, NULL);

	return gfxContext;
}

void oz_record_cmd_buffer(oz_gfx_context* gfxContext, uint32_t imageIndex) {

	vkResetCommandBuffer(gfxContext->commandBuffer, 0);

	VkCommandBufferBeginInfo commandBufferBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};

	vkBeginCommandBuffer(gfxContext->commandBuffer, &commandBufferBeginInfo);

	VkImageMemoryBarrier renderBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = gfxContext->swapchainImages[imageIndex],
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    vkCmdPipelineBarrier(gfxContext->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &renderBarrier);

	VkRenderingAttachmentInfo renderingColorAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = gfxContext->swapchainImageViews[imageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}}
	};

	VkRenderingInfo renderingInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, gfxContext->extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &renderingColorAttachmentInfo,
	};

	vkCmdBeginRendering(gfxContext->commandBuffer, &renderingInfo);
	vkCmdBindPipeline(gfxContext->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxContext->pipeline);
	VkViewport viewport = {0.0f, 0.0f, (float)gfxContext->extent.width, (float)gfxContext->extent.height, 0.0f, 1.0f};
	vkCmdSetViewport(gfxContext->commandBuffer, 0, 1, &viewport);
	VkRect2D scissor = {{0, 0}, gfxContext->extent};
	vkCmdSetScissor(gfxContext->commandBuffer, 0, 1, &scissor);
	vkCmdDraw(gfxContext->commandBuffer, 3, 1, 0, 0);
	vkCmdEndRendering(gfxContext->commandBuffer);

	VkImageMemoryBarrier presentBarrier = renderBarrier;
	presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	presentBarrier.dstAccessMask = 0;
	vkCmdPipelineBarrier(gfxContext->commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &presentBarrier);

	vkEndCommandBuffer(gfxContext->commandBuffer);
}

void oz_render_frame(oz_gfx_context* gfxContext) {
	vkWaitForFences(gfxContext->device, 1, &gfxContext->inFlightFence, VK_FALSE, UINT64_MAX);
	vkResetFences(gfxContext->device, 1, &gfxContext->inFlightFence);

	uint32_t imageIndex = UINT32_MAX;
	if (vkAcquireNextImageKHR(gfxContext->device, gfxContext->swapchain, UINT64_MAX, gfxContext->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex) != VK_SUCCESS || imageIndex == UINT32_MAX) {
		printf("Failed to acquire next image index\n");
		return;
	}

	oz_record_cmd_buffer(gfxContext, imageIndex);

	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &gfxContext->imageAvailableSemaphore,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &gfxContext->commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &gfxContext->renderFinishedSemaphore,
	};

	vkQueueSubmit(gfxContext->graphicsQueue, 1, &submitInfo, gfxContext->inFlightFence);

	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &gfxContext->renderFinishedSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &gfxContext->swapchain,
		.pImageIndices = &imageIndex,
	};

	vkQueuePresentKHR(gfxContext->graphicsQueue, &presentInfo);
}

void oz_graphics_system_shutdown(oz_gfx_context* gfxContext) {
	vkDeviceWaitIdle(gfxContext->device);
	vkDestroyPipeline(gfxContext->device, gfxContext->pipeline, NULL);

	vkDestroyFence(gfxContext->device, gfxContext->inFlightFence, NULL);
	vkDestroySemaphore(gfxContext->device, gfxContext->imageAvailableSemaphore, NULL);
	vkDestroySemaphore(gfxContext->device, gfxContext->renderFinishedSemaphore, NULL);

	for (uint32_t i = 0; i < gfxContext->swapChainImageCount; i++) {
		vkDestroyImageView(gfxContext->device, gfxContext->swapchainImageViews[i], NULL);
	}

	vkDestroyCommandPool(gfxContext->device, gfxContext->commandPool, NULL);
	vkDestroySwapchainKHR(gfxContext->device, gfxContext->swapchain, NULL);
	vkDestroyDevice(gfxContext->device, NULL);
	vkDestroySurfaceKHR(gfxContext->vkInstance, gfxContext->vkSurface, NULL);
	vkDestroyInstance(gfxContext->vkInstance, NULL);

	free(gfxContext->swapchainImages);
	free(gfxContext->swapchainImageViews);
	free(gfxContext);
}
#include <fcntl.h>
#include <linux/limits.h>
#include <ozone/gpu/gfx/gfx.h>
#include <ozone/window/window.h>
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

struct timespec start_vk, end_vk;
bool timer_init_vk = false;

void timer_vk(const char* input) {
    if (!timer_init_vk) {
        clock_gettime(CLOCK_MONOTONIC, &start_vk);
        timer_init_vk = true;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_vk);
    printf("%s: %fms\n", input, (float)(end_vk.tv_sec - start_vk.tv_sec) * 1000.0f + (float)(end_vk.tv_nsec - start_vk.tv_nsec) / 1000000.0f);

    start_vk = end_vk;
}

char* oz_get_vk_driver_files(void** freeptr) {
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

	uint16_t cacheCardCount = UINT16_MAX;
	uint16_t cacheICDPathLength = UINT16_MAX;
	char* cacheICDPath = NULL;

	cacheFile = fopen(path, "rb");
	if (!cacheFile) return NULL;
	free(path);

	uint64_t cacheFileSize = UINT64_MAX; 

	if (fseek(cacheFile, 0, SEEK_END) == 0) {
		cacheFileSize = ftell(cacheFile);
		rewind(cacheFile);
	}

	uint8_t* cacheData = NULL;
	if (cacheFileSize != UINT64_MAX) {
		cacheData = malloc(cacheFileSize * sizeof(uint8_t));
		fread(cacheData, 1, cacheFileSize, cacheFile);
	}
	else {
		return NULL;
	}

	if (cacheData) {
		if (cacheFileSize >= 4) {
			cacheCardCount = ((uint16_t*)cacheData)[0];
			if (cacheCardCount == 0) {

				printf("cache has 0 cardcount\n");
				free(cacheData);
				return NULL;
			}
			cacheICDPathLength = ((uint16_t*)cacheData)[2];
			if (cacheICDPathLength == 0) {
				printf("cachedata icd path length is 0\n");
				free(cacheData);
				return NULL;
			}
			cacheICDPath = (char*)&cacheData[4 + (cacheCardCount * 12)];
			if (cacheICDPath == NULL) {
				printf("cachedata icd path is null\n");
				free(cacheData);
				return NULL;
			}
		}
		else {
			printf("cachedata filesize is invalid\n");
			free(cacheData);
			return NULL;
		}
	}
	else {
		printf("no cachedata\n");
		return NULL;
	}
	
	fclose(cacheFile);

	DIR* dir = opendir("/sys/class/drm/");
	struct dirent* entry;

	if(!dir) {
		printf("Failed to open dir /sys/class/drm/\n");
		return NULL;
	}

	char cardPath[(sizeof("/sys/class/drm/") - 1) + sizeof("card") + 3 + (sizeof("/device/device"))] = "/sys/class/drm/";
	uint32_t nameLen = 0;
	uint16_t cardCount = 0;

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

			cardCount++;
			if (cardCount > cacheCardCount) {
				printf("higher card count? (yikes)");
				return NULL;
			}

			uint16_t idx = 0;

			while (true) {
				if (idx == cacheCardCount) {
					printf("a device has been added? (yikes)");
					return NULL;
				}

				if (strcmp((char*)&(cacheData + 4)[idx * 12], entry->d_name + 4) == 0) {
					break;
				}

				idx++;
			}

			uint32_t deviceID, vendorID;

			memcpy(cardPath + (sizeof("/sys/class/drm/") - 1) + nameLen, "/device/device", sizeof("/device/device"));
			FILE* deviceFile = fopen(cardPath, "r");
			if (!deviceFile) return NULL;
			fscanf(deviceFile, "%x", &deviceID);
			fclose(deviceFile);

			if (deviceID != ((uint32_t*)cacheData + 1)[idx * 3 + 1]) {
				printf("deviceid changed? (yikes) %x, %x, %s\n", ((uint32_t*)cacheData + 1)[idx * 3 + 1], deviceID, cardPath);
				return NULL;
			}

			memcpy(cardPath + (sizeof("/sys/class/drm/") - 1) + nameLen, "/device/vendor", sizeof("/device/vendor"));
			FILE* vendorFile = fopen(cardPath, "r");
			if (!vendorFile) return NULL;
			fscanf(vendorFile, "%x", &vendorID);
			fclose(vendorFile);

			if (vendorID != ((uint32_t*)cacheData + 1)[idx * 3 + 2]) {
				printf("vendorid changed? (yikes)");
				return NULL;
			}
		}
	}

	closedir(dir);
	*freeptr = cacheData;
	return cacheICDPath;
}

void oz_set_vk_driver_files_cache(VkPhysicalDevice* physicalDevice, const char* icdPath) {
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

	cacheFile = fopen(path, "wb");
	if (!cacheFile) return;
	free(path);

	DIR* dir = opendir("/sys/class/drm/");
	struct dirent* entry;

	if(!dir) {
		printf("Failed to open dir /sys/class/drm/\n");
		return;
	}

	uint16_t icdPathSize = strlen(icdPath) + 1;

	char cardPath[(sizeof("/sys/class/drm/") - 1) + sizeof("card") + 3 + (sizeof("/device/device"))] = "/sys/class/drm/";
	uint32_t nameLen = 0;
	uint16_t cardCount = 0;
	uint16_t cacheDataCapacity = 64 + icdPathSize;
	uint16_t cacheDataSize = 4 + icdPathSize;
	uint8_t* cacheData = malloc(cacheDataCapacity);

	VkPhysicalDeviceDriverProperties physicalDeviceDriverProperties = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES
	};

	VkPhysicalDeviceProperties2 physicalDeviceProperties = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
	    .pNext = &physicalDeviceDriverProperties
	};

	vkGetPhysicalDeviceProperties2(*physicalDevice, &physicalDeviceProperties);

	while ((entry = readdir(dir))) {
		struct stat statBuffer;

		nameLen = strlen(entry->d_name);
		
		if (nameLen > strlen("card") + 3 || nameLen < strlen("card") + 1) continue;

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

			cacheDataSize = 4 + (cardCount * 12) + 12 + icdPathSize;
			if (cacheDataSize > cacheDataCapacity) {
				cacheDataCapacity *= 2;
				cacheData = realloc(cacheData, cacheDataCapacity);
				if (!cacheData) return;
			}

			((char*)cacheData)[4 + (cardCount * 12) + 0] = *(entry->d_name + 4 + 0);
			((char*)cacheData)[4 + (cardCount * 12) + 1] = '\0';
			((char*)cacheData)[4 + (cardCount * 12) + 2] = '\0';
			((char*)cacheData)[4 + (cardCount * 12) + 3] = '\0';

			if (nameLen > strlen("card") + 1) {
				((char*)cacheData)[4 + (cardCount * 12) + 1] = *(entry->d_name + 4 + 1);
			}

			if (nameLen > strlen("card") + 2) {
				((char*)cacheData)[4 + (cardCount * 12) + 2] = *(entry->d_name + 4 + 2);
			}

			uint32_t deviceID, vendorID;

			memcpy(cardPath + (sizeof("/sys/class/drm/") - 1) + nameLen, "/device/device", sizeof("/device/device"));
			FILE* deviceFile = fopen(cardPath, "r");
			if (!deviceFile) continue;
			fscanf(deviceFile, "%x", &deviceID);
			fclose(deviceFile);

			((uint32_t*)cacheData)[1 + (cardCount * 3) + 1] = deviceID;

			memcpy(cardPath + (sizeof("/sys/class/drm/") - 1) + nameLen, "/device/vendor", sizeof("/device/vendor"));
			FILE* vendorFile = fopen(cardPath, "r");
			if (!vendorFile) continue;
			fscanf(vendorFile, "%x", &vendorID);
			fclose(vendorFile);

			((uint32_t*)cacheData)[1 + (cardCount * 3) + 2] = vendorID;

			cardCount++;
		}
	}

	closedir(dir);

	((uint16_t*)cacheData)[0] = cardCount;
	((uint16_t*)cacheData)[1] = icdPathSize;

	strcpy(&((char*)cacheData)[4 + (cardCount * 12)], icdPath);

	fwrite(cacheData, sizeof(uint8_t), cacheDataSize, cacheFile);
	fclose(cacheFile);
}

VkPhysicalDevice oz_get_physical_device(VkInstance* vkInstances, uint16_t vkInstanceCount, VkInstance* deviceInstance) {
	uint32_t deviceCount = 0;

	for (uint16_t i = 0; i < vkInstanceCount; i++) {
		if (vkInstances[i] == NULL) continue;

		uint32_t instanceDeviceCount = 0;

		if (vkEnumeratePhysicalDevices(vkInstances[i], &instanceDeviceCount, NULL) != VK_SUCCESS) {
			vkDestroyInstance(vkInstances[i], NULL);
			vkInstances[i] = NULL;
			continue;
		}

		deviceCount += instanceDeviceCount;
	}
	timer_vk("[VK] vkEnumeratePhysicalDevices");

	VkPhysicalDevice* physicalDevices = malloc(deviceCount * sizeof(VkPhysicalDevice));

	uint32_t prevInstanceDeviceCount = 0;
	VkPhysicalDevice physicalDevice = NULL;

	for (uint32_t i = 0; i < vkInstanceCount; i++) {
		if (vkInstances[i] == NULL) continue;

		if (vkEnumeratePhysicalDevices(vkInstances[i], &deviceCount, &physicalDevices[prevInstanceDeviceCount]) != VK_SUCCESS) {
			printf("failed to enumerate physical devices\n");
			return NULL;
		}

		for (uint32_t i = 0; i < deviceCount; i++) {
			physicalDevice = physicalDevices[i];

			VkPhysicalDeviceProperties physicalDeviceProperties;

			//VkPhysicalDeviceFeatures physicalDeviceFeatures;
			vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
			//vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

			uint64_t major = VK_VERSION_MAJOR(physicalDeviceProperties.apiVersion);
			uint64_t minor = VK_VERSION_MINOR(physicalDeviceProperties.apiVersion);
			//uint64_t patch = VK_VERSION_PATCH(physicalDeviceProperties.properties.apiVersion);

			if (major == 1 && minor >= 3) {
				if (deviceInstance != NULL) *deviceInstance = vkInstances[i];
				free(physicalDevices);
				timer_vk("[VK] select physical device");
				return physicalDevice;
			}
		}
	}

	if (deviceInstance != NULL) *deviceInstance = NULL;
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

VkShaderModule oz_gfx_create_shader_module(VkDevice device, const char* path) {
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

VkFormat oz_gfx_get_format(oz_gfx_context* gfxContext) {
	uint32_t surfaceFormatCount = 0;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(gfxContext->physicalDevice, gfxContext->vkSurface, &surfaceFormatCount, NULL) != VK_SUCCESS || surfaceFormatCount == 0) {
		printf("Failed to get formats\n");
		return VK_FORMAT_UNDEFINED;
	}

	VkSurfaceFormatKHR* surfaceFormats = malloc(surfaceFormatCount * sizeof(VkSurfaceFormatKHR));
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(gfxContext->physicalDevice, gfxContext->vkSurface, &surfaceFormatCount, surfaceFormats) != VK_SUCCESS) {
		printf("Failed to get formats\n");
		return VK_FORMAT_UNDEFINED;
	}

	for (uint32_t i = 0; i < surfaceFormatCount; i++) {

		if (surfaceFormats[i].format == VK_FORMAT_B8G8R8_SRGB) {
			free(surfaceFormats);
			return VK_FORMAT_B8G8R8_SRGB;
		}

		if (surfaceFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
			free(surfaceFormats);
			return VK_FORMAT_R8G8B8A8_UNORM;
		}
	}

	VkFormat format = surfaceFormats[0].format;
	free(surfaceFormats);

	return format;
}

oz_gfx_context* oz_gfx_init(oz_window window) {
	timer_vk("[VK] start");
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

	const char* instanceLayerNames[] = {
		"VK_LAYER_KHRONOS_validation",
	};

	VkInstanceCreateInfo instanceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &applicationInfo,
		.enabledExtensionCount = 2,
		.ppEnabledExtensionNames = instanceExtensionNames,
		#ifdef OZONE_ENABLE_VK_VALIDATION
		.enabledLayerCount = 1,
		.ppEnabledLayerNames = instanceLayerNames,
		#endif
	};

	void* freeptr = NULL;
	char* icdPath = oz_get_vk_driver_files(&freeptr);
	timer_vk("[VK] get_vk_driver_files");

	if (icdPath) {
		setenv("VK_DRIVER_FILES", icdPath, 1);
		if (vkCreateInstance(&instanceCreateInfo, NULL, &gfxContext->vkInstance) != VK_SUCCESS) {
			printf("Failed to create Instance\n");
		}
		timer_vk("[VK] vkCreateInstance");

		gfxContext->physicalDevice = oz_get_physical_device(&gfxContext->vkInstance, 1, NULL);
	} else {
	    DIR* dir;
	    const char* dir_name = "/usr/share/vulkan/icd.d/";

	    uint32_t instanceCount = 0;
	    uint32_t instanceCapacity = 0;
	    VkInstance* instances = NULL;

	    char** paths = malloc(1 * sizeof(char*));

	    if ((dir = opendir(dir_name))) {
		    struct dirent* entry;

		    while ((entry = readdir(dir))) {
		    	if (strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0) continue;

		    	unsigned long entry_name_len = strlen(entry->d_name);
		    	unsigned long dir_name_len = strlen(dir_name);

		    	char path[PATH_MAX];
		    	memcpy(path, dir_name, dir_name_len);
		    	memcpy(path + dir_name_len, entry->d_name, entry_name_len + 1);

		    	setenv("VK_DRIVER_FILES", path, 1);

		    	if (instanceCount >= instanceCapacity) {
		    		instanceCapacity = instanceCapacity == 0 ? 1 : instanceCapacity * 2;

		    		instances = realloc(instances, instanceCapacity * sizeof(VkInstance));
		    		paths = realloc(paths, instanceCapacity * sizeof(char*));
		    	}

		    	if (vkCreateInstance(&instanceCreateInfo, NULL, &instances[instanceCount]) == VK_SUCCESS) {
		    		paths[instanceCount] = malloc((strlen(path) + 1) * sizeof(char));
		    		strcpy(paths[instanceCount], path);
		    		instanceCount++;
		    		printf("Success!\n");
		    	}
		    	else {
		    		instances[instanceCount] = NULL;
		    		printf("Failure!\n");
		    	}
		    }
	    }

		timer_vk("[VK] vkCreateInstance's");
	    closedir(dir);

	    gfxContext->physicalDevice = oz_get_physical_device(instances, instanceCount, &gfxContext->vkInstance);
		timer_vk("[VK] get_physical_device");
		if (!gfxContext->physicalDevice) {
			printf("ERR\n");
			return NULL;
		}

		oz_set_vk_driver_files_cache(&gfxContext->physicalDevice, icdPath);
		timer_vk("[VK] set_vk_driver_files_cache");

	    for (uint32_t i = 0; i < instanceCount; i++) {
	    	if (instances[i] == gfxContext->vkInstance) {
	    		icdPath = paths[i];
	    		continue;
	    	}

	    	if (instances[i] != NULL) vkDestroyInstance(instances[i], NULL);
	    	free(paths[i]);
	    }

	    free(instances);
	    free(paths);
		timer_vk("[VK] vkDestroyInstance's");
	}

	if (freeptr != NULL) free(freeptr);

	VkWaylandSurfaceCreateInfoKHR waylandSurfaceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
		.display = oz_window_get_wayland_display(),
		.surface = oz_window_get_wayland_surface(window),
	};

	vkCreateWaylandSurfaceKHR(gfxContext->vkInstance, &waylandSurfaceCreateInfo, NULL, &gfxContext->vkSurface);

	uint32_t graphicsQueueFamilyIndex = oz_get_graphics_queue_family_index(gfxContext->physicalDevice, gfxContext->vkSurface);
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

	vkCreateDevice(gfxContext->physicalDevice, &deviceCreateInfo, NULL, &gfxContext->device);

	vkGetDeviceQueue(gfxContext->device, graphicsQueueFamilyIndex, 0, &gfxContext->graphicsQueue);

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

	
	timer_vk("[VK] everything else");

	return gfxContext;
}

uint32_t oz_gfx_find_memory_type(uint32_t typeFilter, VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    printf("Failed to find memory type");
    return UINT32_MAX;
}

void oz_gfx_create_pipeline(oz_pipeline_create_info* pPipelineCreateInfo, oz_pipeline* pPipeline, VkPipelineLayout* pPipelineLayout) {
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
	};

	if (vkCreatePipelineCache(pPipelineCreateInfo->device, &pipelineCacheCreateInfo, NULL, &pPipeline->pipelineCache) != VK_SUCCESS) {
		printf("Failed to create pipeline cache\n");
		return;
	}

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &pPipelineCreateInfo->format,
	};

	VkVertexInputBindingDescription vertexInputBindingDescription = {
		.binding = 0,
		.stride = 3 * sizeof(float),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription vertexInputAttributeDescription = {
		.binding = 0,
		.location = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = 0,
	};

	VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pVertexBindingDescriptions = &vertexInputBindingDescription,
		.vertexBindingDescriptionCount = 1,
		.pVertexAttributeDescriptions = &vertexInputAttributeDescription,
		.vertexAttributeDescriptionCount = 1,
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
		.cullMode = VK_CULL_MODE_NONE,
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

	VkPipelineShaderStageCreateInfo* pipelineShaderStageCreateInfos = malloc(pPipelineCreateInfo->shaderCount * sizeof(VkPipelineShaderStageCreateInfo));

	for (uint8_t i = 0; i < pPipelineCreateInfo->shaderCount; i++) {
		pipelineShaderStageCreateInfos[i] = (VkPipelineShaderStageCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = pPipelineCreateInfo->shaderStages[i],
			.module = pPipelineCreateInfo->shaderModules[i],
			.pName = "main",
		};
	}

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = pPipelineCreateInfo->descriptorSetLayoutCount,
		.pSetLayouts = pPipelineCreateInfo->descriptorSetLayouts,
		.pushConstantRangeCount = pPipelineCreateInfo->pushConstantRangeCount,
		.pPushConstantRanges = pPipelineCreateInfo->pushConstantRanges,
	};

	if (vkCreatePipelineLayout(pPipelineCreateInfo->device, &pipelineLayoutCreateInfo, NULL, pPipelineLayout) != VK_SUCCESS) {
		printf("Failed to create pipeline layout\n");
		return;
	}

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.layout = *pPipelineLayout,
		.stageCount = pPipelineCreateInfo->shaderCount,
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

	if (vkCreateGraphicsPipelines(pPipelineCreateInfo->device, pPipeline->pipelineCache, 1, &graphicsPipelineCreateInfo, NULL, &pPipeline->vkPipeline) != VK_SUCCESS) {
		printf("Failed to create graphics pipelines\n");
		return;
	}

	free(pipelineShaderStageCreateInfos);

	for (uint8_t i = 0; i < pPipelineCreateInfo->shaderCount; i++) {
		vkDestroyShaderModule(pPipelineCreateInfo->device, pPipelineCreateInfo->shaderModules[i], NULL);
	}
}

void oz_gfx_create_swapchain(oz_gfx_context* gfxContext, VkFormat format, uint32_t width, uint32_t height, oz_swapchain* swapchain) {
	VkSurfaceFormatKHR surfaceFormat = {format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

	uint32_t imageCount = 3;
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gfxContext->physicalDevice, gfxContext->vkSurface, &surfaceCapabilities);

	if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
		imageCount = surfaceCapabilities.maxImageCount;
	}
	else if (surfaceCapabilities.minImageCount != 0 && imageCount < surfaceCapabilities.minImageCount) {
		imageCount = surfaceCapabilities.minImageCount;
	}

	swapchain->extents = (VkExtent2D){.width = width, .height = height};

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = gfxContext->vkSurface,
		.minImageCount = imageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = swapchain->extents,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = presentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = swapchain->vkSwapchain,
	};

	if (vkCreateSwapchainKHR(gfxContext->device, &swapChainCreateInfo, NULL, &swapchain->vkSwapchain) != VK_SUCCESS) {
		printf("Failed to create swapchain\n");
		return;
	}

	if (vkGetSwapchainImagesKHR(gfxContext->device, swapchain->vkSwapchain, &swapchain->imageCount, NULL) != VK_SUCCESS || swapchain->imageCount == 0) {
		printf("Failed to get swapchain images\n");
		return;
	}

	swapchain->images = malloc(swapchain->imageCount * sizeof(VkImage));

	if (vkGetSwapchainImagesKHR(gfxContext->device, swapchain->vkSwapchain, &swapchain->imageCount, swapchain->images) != VK_SUCCESS) {
		printf("Failed to get swapchain images\n");
		return;
	}

	swapchain->imageViews = malloc(swapchain->imageCount * sizeof(VkImageView));

	for (uint32_t i = 0; i < swapchain->imageCount; i++) {
		VkImageViewCreateInfo swapchainImageViewCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain->images[i],
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

		vkCreateImageView(gfxContext->device, &swapchainImageViewCreateInfo, NULL, &swapchain->imageViews[i]);
	}

	return;
}

void oz_gfx_record_cmd_buffer(oz_gfx_context* gfxContext, uint32_t imageIndex, void (*record_cmd_buffer)(oz_gfx_context*)) {

	vkResetCommandBuffer(gfxContext->commandBuffer, 0);

	VkCommandBufferBeginInfo commandBufferBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};

	vkBeginCommandBuffer(gfxContext->commandBuffer, &commandBufferBeginInfo);

	VkImageMemoryBarrier renderBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = gfxContext->swapchain->images[imageIndex],
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    vkCmdPipelineBarrier(gfxContext->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &renderBarrier);

	VkRenderingAttachmentInfo renderingColorAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = gfxContext->swapchain->imageViews[imageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}}
	};

	VkRenderingInfo renderingInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, gfxContext->swapchain->extents},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &renderingColorAttachmentInfo,
	};
	
	vkCmdBeginRendering(gfxContext->commandBuffer, &renderingInfo);

	record_cmd_buffer(gfxContext);

	vkCmdEndRendering(gfxContext->commandBuffer);

	VkImageMemoryBarrier presentBarrier = renderBarrier;
	presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	presentBarrier.dstAccessMask = 0;
	vkCmdPipelineBarrier(gfxContext->commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &presentBarrier);

	vkEndCommandBuffer(gfxContext->commandBuffer);
}

void oz_gfx_render_frame(oz_gfx_context* gfxContext, void (*record_cmd_buffer)(oz_gfx_context*)) {
	vkWaitForFences(gfxContext->device, 1, &gfxContext->inFlightFence, VK_FALSE, UINT64_MAX);
	vkResetFences(gfxContext->device, 1, &gfxContext->inFlightFence);

	uint32_t imageIndex = UINT32_MAX;
	if (vkAcquireNextImageKHR(gfxContext->device, gfxContext->swapchain->vkSwapchain, UINT64_MAX, gfxContext->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex) != VK_SUCCESS || imageIndex == UINT32_MAX) {
		printf("Failed to acquire next image index\n");
		return;
	}

	oz_gfx_record_cmd_buffer(gfxContext, imageIndex, record_cmd_buffer);

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
		.pSwapchains = &gfxContext->swapchain->vkSwapchain,
		.pImageIndices = &imageIndex,
	};

	vkQueuePresentKHR(gfxContext->graphicsQueue, &presentInfo);
}

void oz_gfx_shutdown(oz_gfx_context* gfxContext) {
	vkDeviceWaitIdle(gfxContext->device);

	for (uint16_t i = 0; i < gfxContext->pipelineCount; i++) {
		vkDestroyPipeline(gfxContext->device, gfxContext->pipelines[i].vkPipeline, NULL);
		vkDestroyPipelineCache(gfxContext->device, gfxContext->pipelines[i].pipelineCache, NULL);
	}

	vkDestroyFence(gfxContext->device, gfxContext->inFlightFence, NULL);
	vkDestroySemaphore(gfxContext->device, gfxContext->imageAvailableSemaphore, NULL);
	vkDestroySemaphore(gfxContext->device, gfxContext->renderFinishedSemaphore, NULL);

	for (uint32_t i = 0; i < gfxContext->swapchain->imageCount; i++) {
		vkDestroyImageView(gfxContext->device, gfxContext->swapchain->imageViews[i], NULL);
	}

	vkDestroyCommandPool(gfxContext->device, gfxContext->commandPool, NULL);
	vkDestroySwapchainKHR(gfxContext->device, gfxContext->swapchain->vkSwapchain, NULL);
	vkDestroyDevice(gfxContext->device, NULL);
	vkDestroySurfaceKHR(gfxContext->vkInstance, gfxContext->vkSurface, NULL);
	vkDestroyInstance(gfxContext->vkInstance, NULL);

	free(gfxContext->swapchain->images);
	free(gfxContext->swapchain->imageViews);
	free(gfxContext->swapchain);
	free(gfxContext);
}
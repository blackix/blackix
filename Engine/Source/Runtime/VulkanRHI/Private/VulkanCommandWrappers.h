// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommandWrappers.h: Wrap all Vulkan API functions so we can add our own 'layers'
=============================================================================*/

#pragma once 

#if VULKAN_ENABLE_CUSTOM_LAYER
	#define  VULKAN_LAYER_BODY		;
	#define	VULKAN_EXTERN_EXPORT	VULKANRHI_API
#else
	#define VULKAN_LAYER_BODY		{}
	#define	VULKAN_EXTERN_EXPORT
#endif

struct FWrapLayer
{
	// Pass in VK_RESULT_MAX_ENUM for Prolog calls; for Epilog use a different value or the actual Result if available

	static void CreateInstance(VkResult Result, const VkInstanceCreateInfo* CreateInfo, VkInstance* Instance) VULKAN_LAYER_BODY
	static void EnumeratePhysicalDevices(VkResult Result, VkInstance Instance, uint32* PhysicalDeviceCount, VkPhysicalDevice* PhysicalDevices) VULKAN_LAYER_BODY
	static void DestroyInstance(VkResult Result, VkInstance Instance) VULKAN_LAYER_BODY
	static void GetInstanceProcAddr(VkResult Result, VkInstance Instance, const char* Name, PFN_vkVoidFunction VoidFunction) VULKAN_LAYER_BODY
	VULKAN_EXTERN_EXPORT static void EnumerateInstanceExtensionProperties(VkResult Result, const char* LayerName, uint32* PropertyCount, VkExtensionProperties* Properties) VULKAN_LAYER_BODY
	static void EnumerateInstanceLayerProperties(VkResult Result, uint32* PropertyCount, VkLayerProperties* Properties) VULKAN_LAYER_BODY
	static void AllocateMemory(VkResult Result, VkDevice Device, const VkMemoryAllocateInfo* AllocateInfo, VkDeviceMemory* Memory) VULKAN_LAYER_BODY
	static void FlushMappedMemoryRanges(VkResult Result, VkDevice Device, uint32 MemoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) VULKAN_LAYER_BODY
	static void InvalidateMappedMemoryRanges(VkResult Result, VkDevice Device, uint32 MemoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) VULKAN_LAYER_BODY
	static void GetBufferMemoryRequirements(VkResult Result, VkDevice Device, VkBuffer Buffer, VkMemoryRequirements* MemoryRequirements) VULKAN_LAYER_BODY
	static void GetImageMemoryRequirements(VkResult Result, VkDevice Device, VkImage Image, VkMemoryRequirements* MemoryRequirements) VULKAN_LAYER_BODY
	static void CreateFence(VkResult Result, VkDevice Device, const VkFenceCreateInfo* CreateInfo, VkFence* Fence) VULKAN_LAYER_BODY
	static void ResetFences(VkResult Result, VkDevice Device, uint32 FenceCount, const VkFence* Fences) VULKAN_LAYER_BODY
	static void WaitForFences(VkResult Result, VkDevice Device, uint32 FenceCount, const VkFence* Fences, VkBool32 bWaitAll, uint64_t Timeout) VULKAN_LAYER_BODY
	static void CreateSemaphore(VkResult Result, VkDevice Device, const VkSemaphoreCreateInfo* CreateInfo, VkSemaphore* Semaphore) VULKAN_LAYER_BODY
	static void CreateQueryPool(VkResult Result, VkDevice Device, const VkQueryPoolCreateInfo* CreateInfo, VkQueryPool* QueryPool) VULKAN_LAYER_BODY
	static void CreateBuffer(VkResult Result, VkDevice Device, const VkBufferCreateInfo* CreateInfo, VkBuffer* Buffer) VULKAN_LAYER_BODY
	static void CreateBufferView(VkResult Result, VkDevice Device, const VkBufferViewCreateInfo* CreateInfo, VkBufferView* BufferView) VULKAN_LAYER_BODY
	VULKAN_EXTERN_EXPORT static void CreateImage(VkResult Result, VkDevice Device, const VkImageCreateInfo* CreateInfo, VkImage* Image) VULKAN_LAYER_BODY
	static void DestroyImage(VkResult Result, VkDevice Device, VkImage Image) VULKAN_LAYER_BODY
	static void GetImageSubresourceLayout(VkResult Result, VkDevice Device, VkImage Image, const VkImageSubresource* Subresource, VkSubresourceLayout* Layout) VULKAN_LAYER_BODY
	static void CreateImageView(VkResult Result, VkDevice Device, const VkImageViewCreateInfo* CreateInfo, VkImageView* ImageView) VULKAN_LAYER_BODY
	static void CreateShaderModule(VkResult Result, VkDevice Device, const VkShaderModuleCreateInfo* CreateInfo, VkShaderModule* ShaderModule) VULKAN_LAYER_BODY
	static void CreatePipelineCache(VkResult Result, VkDevice Device, const VkPipelineCacheCreateInfo* CreateInfo, VkPipelineCache* PipelineCache) VULKAN_LAYER_BODY
	static void CreateGraphicsPipelines(VkResult Result, VkDevice Device, VkPipelineCache PipelineCache, uint32 CreateInfoCount, const VkGraphicsPipelineCreateInfo* CreateInfos, VkPipeline* Pipelines) VULKAN_LAYER_BODY
	static void CreatePipelineLayout(VkResult Result, VkDevice Device, const VkPipelineLayoutCreateInfo* CreateInfo, VkPipelineLayout* PipelineLayout) VULKAN_LAYER_BODY
	static void CreateSampler(VkResult Result, VkDevice Device, const VkSamplerCreateInfo* CreateInfo, VkSampler* Sampler) VULKAN_LAYER_BODY
	static void CreateDescriptorSetLayout(VkResult Result, VkDevice Device, const VkDescriptorSetLayoutCreateInfo* CreateInfo, VkDescriptorSetLayout* SetLayout) VULKAN_LAYER_BODY
	static void CreateDescriptorPool(VkResult Result, VkDevice Device, const VkDescriptorPoolCreateInfo* CreateInfo, VkDescriptorPool* DescriptorPool) VULKAN_LAYER_BODY
	static void AllocateDescriptorSets(VkResult Result, VkDevice Device, const VkDescriptorSetAllocateInfo* AllocateInfo, VkDescriptorSet* DescriptorSets) VULKAN_LAYER_BODY
	static void FreeDescriptorSets(VkResult Result, VkDevice Device, VkDescriptorPool DescriptorPool, uint32 DescriptorSetCount, const VkDescriptorSet* DescriptorSets) VULKAN_LAYER_BODY
	static void UpdateDescriptorSets(VkResult Result, VkDevice Device, uint32 DescriptorWriteCount, const VkWriteDescriptorSet* DescriptorWrites, uint32 DescriptorCopyCount, const VkCopyDescriptorSet* DescriptorCopies) VULKAN_LAYER_BODY
	static void CreateFramebuffer(VkResult Result, VkDevice Device, const VkFramebufferCreateInfo* CreateInfo, VkFramebuffer* Framebuffer) VULKAN_LAYER_BODY
	static void CreateRenderPass(VkResult Result, VkDevice Device, const VkRenderPassCreateInfo* CreateInfo, VkRenderPass* RenderPass) VULKAN_LAYER_BODY
	static void CreateCommandPool(VkResult Result, VkDevice Device, const VkCommandPoolCreateInfo* CreateInfo, VkCommandPool* CommandPool) VULKAN_LAYER_BODY
	static void BeginCommandBuffer(VkResult Result, VkCommandBuffer CommandBuffer, const VkCommandBufferBeginInfo* BeginInfo) VULKAN_LAYER_BODY
	static void EndCommandBuffer(VkResult Result, VkCommandBuffer CommandBuffer) VULKAN_LAYER_BODY
	static void BindDescriptorSets(VkResult Result, VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipelineLayout Layout, uint32 FirstSet, uint32 DescriptorSetCount, const VkDescriptorSet* DescriptorSets, uint32 DynamicOffsetCount, const uint32* DynamicOffsets) VULKAN_LAYER_BODY
	static void BindVertexBuffers(VkResult Result, VkCommandBuffer CommandBuffer, uint32 FirstBinding, uint32 BindingCount, const VkBuffer* Buffers, const VkDeviceSize* pOffsets) VULKAN_LAYER_BODY
	static void BindIndexBuffer(VkResult Result, VkCommandBuffer CommandBuffer, VkBuffer IndexBuffer, VkDeviceSize Offset, VkIndexType IndexType) VULKAN_LAYER_BODY
	static void CopyBuffer(VkResult Result, VkCommandBuffer CommandBuffer, VkBuffer SrcBuffer, VkBuffer DstBuffer, uint32 RegionCount, const VkBufferCopy* Regions) VULKAN_LAYER_BODY
	static void CopyBufferToImage(VkResult Result, VkCommandBuffer CommandBuffer, VkBuffer SrcBuffer, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkBufferImageCopy* Regions) VULKAN_LAYER_BODY
	VULKAN_EXTERN_EXPORT static void BlitImage(VkResult Result, VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageBlit* Regions, VkFilter Filter) VULKAN_LAYER_BODY
	static void CopyImageToBuffer(VkResult Result, VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkBuffer DstBuffer, uint32 RegionCount, const VkBufferImageCopy* Regions) VULKAN_LAYER_BODY
	VULKAN_EXTERN_EXPORT static void ClearColorImage(VkResult Result, VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout ImageLayout, const VkClearColorValue* ColorValue, uint32 RangeCount, const VkImageSubresourceRange* Ranges) VULKAN_LAYER_BODY
	static void ClearDepthStencilImage(VkResult Result, VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout ImageLayout, const VkClearDepthStencilValue* DepthStencil, uint32 RangeCount, const VkImageSubresourceRange* Ranges) VULKAN_LAYER_BODY
	static void ClearAttachments(VkResult Result, VkCommandBuffer CommandBuffer, uint32 AttachmentCount, const VkClearAttachment* Attachments, uint32 RectCount, const VkClearRect* Rects) VULKAN_LAYER_BODY
	static void ResolveImage(VkResult Result, VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageResolve* Regions) VULKAN_LAYER_BODY
	static void WaitEvents(VkResult Result, VkCommandBuffer CommandBuffer, uint32 EventCount, const VkEvent* Events, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, uint32 MemoryBarrierCount,
			const VkMemoryBarrier* pMemoryBarriers, uint32 BufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32 ImageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) VULKAN_LAYER_BODY
	static void PipelineBarrier(VkResult Result, VkCommandBuffer CommandBuffer, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, VkDependencyFlags DependencyFlags, uint32 MemoryBarrierCount, const VkMemoryBarrier* MemoryBarriers, uint32 BufferMemoryBarrierCount, const VkBufferMemoryBarrier* BufferMemoryBarriers, uint32 ImageMemoryBarrierCount, const VkImageMemoryBarrier* ImageMemoryBarriers) VULKAN_LAYER_BODY
	static void BeginRenderPass(VkResult Result, VkCommandBuffer CommandBuffer, const VkRenderPassBeginInfo* RenderPassBegin, VkSubpassContents Contents) VULKAN_LAYER_BODY
	static void EndRenderPass(VkResult Result, VkCommandBuffer CommandBuffer) VULKAN_LAYER_BODY
	static void NextSubpass(VkResult Result, VkCommandBuffer CommandBuffer, VkSubpassContents Contents) VULKAN_LAYER_BODY
	static void QueuePresent(VkResult Result, VkQueue Queue, const VkPresentInfoKHR* PresentInfo) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceMemoryProperties(VkResult Result, VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceMemoryProperties* Properties) VULKAN_LAYER_BODY
	static void CreateDevice(VkResult Result, VkPhysicalDevice PhysicalDevice, const VkDeviceCreateInfo* CreateInfo, VkDevice* Device) VULKAN_LAYER_BODY
	static void QueueSubmit(VkResult Result, VkQueue Queue, uint32 SubmitCount, const VkSubmitInfo* Submits, VkFence Fence) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceFeatures(VkResult Result, VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceFeatures* Features) VULKAN_LAYER_BODY
	static void GetSwapChainImagesKHR(VkResult Result, VkDevice Device, VkSwapchainKHR Swapchain, uint32_t* SwapchainImageCount, VkImage* SwapchainImages) VULKAN_LAYER_BODY
	static void GetDeviceQueue(VkResult Result, VkDevice Device, uint32 QueueFamilyIndex, uint32 QueueIndex, VkQueue* Queue) VULKAN_LAYER_BODY
	static void DeviceWaitIdle(VkResult Result, VkDevice Device) VULKAN_LAYER_BODY
	static void MapMemory(VkResult Result, VkDevice Device, VkDeviceMemory Memory, VkDeviceSize Offset, VkDeviceSize Size, VkMemoryMapFlags Flags, void** Data) VULKAN_LAYER_BODY
	static void UnmapMemory(VkResult Result, VkDevice Device, VkDeviceMemory Memory) VULKAN_LAYER_BODY
	static void BindBufferMemory(VkResult Result, VkDevice Device, VkBuffer Buffer, VkDeviceMemory Memory, VkDeviceSize MemoryOffset) VULKAN_LAYER_BODY
	VULKAN_EXTERN_EXPORT static void BindImageMemory(VkResult Result, VkDevice Device, VkImage Image, VkDeviceMemory Memory, VkDeviceSize MemoryOffset) VULKAN_LAYER_BODY
	static void GetFenceStatus(VkResult Result, VkDevice Device, VkFence Fence) VULKAN_LAYER_BODY
	static void GetQueryPoolResults(VkResult Result, VkDevice Device, VkQueryPool QueryPool, uint32 FirstQuery, uint32 QueryCount, size_t DataSize, void* Data, VkDeviceSize Stride, VkQueryResultFlags Flags) VULKAN_LAYER_BODY
	static void CreateComputePipelines(VkResult Result, VkDevice Device, VkPipelineCache PipelineCache, uint32 CreateInfoCount, const VkComputePipelineCreateInfo* CreateInfos, VkPipeline* Pipelines) VULKAN_LAYER_BODY
	static void AllocateCommandBuffers(VkResult Result, VkDevice Device, const VkCommandBufferAllocateInfo* AllocateInfo, VkCommandBuffer* CommandBuffers) VULKAN_LAYER_BODY
	static void CreateSwapchainKHR(VkResult Result, VkDevice Device, const VkSwapchainCreateInfoKHR* CreateInfo, VkSwapchainKHR* Swapchain) VULKAN_LAYER_BODY
	static void AcquireNextImageKHR(VkResult Result, VkDevice Device, VkSwapchainKHR Swapchain, uint64_t Timeout, VkSemaphore Semaphore, VkFence Fence, uint32_t* ImageIndex) VULKAN_LAYER_BODY
	static void FreeMemory(VkResult Result, VkDevice Device, VkDeviceMemory Memory) VULKAN_LAYER_BODY
	static void DestroyFence(VkResult Result, VkDevice Device, VkFence Fence) VULKAN_LAYER_BODY
	static void DestroySemaphore(VkResult Result, VkDevice Device, VkSemaphore Semaphore) VULKAN_LAYER_BODY
	static void CreateEvent(VkResult Result, VkDevice Device, const VkEventCreateInfo* CreateInfo, VkEvent* Event) VULKAN_LAYER_BODY
	static void DestroyEvent(VkResult Result, VkDevice Device, VkEvent Event) VULKAN_LAYER_BODY
	static void DestroyBuffer(VkResult Result, VkDevice Device, VkBuffer Buffer) VULKAN_LAYER_BODY
	static void DestroyBufferView(VkResult Result, VkDevice Device, VkBufferView BufferView) VULKAN_LAYER_BODY
	static void DestroyImageView(VkResult Result, VkDevice Device, VkImageView ImageView) VULKAN_LAYER_BODY
	static void DestroyQueryPool(VkResult Result, VkDevice Device, VkQueryPool QueryPool) VULKAN_LAYER_BODY
	static void DestroyPipeline(VkResult Result, VkDevice Device, VkPipeline Pipeline) VULKAN_LAYER_BODY
	static void DestroyPipelineLayout(VkResult Result, VkDevice Device, VkPipelineLayout PipelineLayout) VULKAN_LAYER_BODY
	static void DestroyPipelineCache(VkResult Result, VkDevice Device, VkPipelineCache PipelineCache) VULKAN_LAYER_BODY
	static void DestroySampler(VkResult Result, VkDevice Device, VkSampler Sampler) VULKAN_LAYER_BODY
	static void DestroyShaderModule(VkResult Result, VkDevice Device, VkShaderModule ShaderModule) VULKAN_LAYER_BODY
	static void DestroyFramebuffer(VkResult Result, VkDevice Device, VkFramebuffer Framebuffer) VULKAN_LAYER_BODY
	static void DestroyRenderPass(VkResult Result, VkDevice Device, VkRenderPass RenderPass) VULKAN_LAYER_BODY
	static void DestroyCommandPool(VkResult Result, VkDevice Device, VkCommandPool CommandPool) VULKAN_LAYER_BODY
	static void DestroyDevice(VkResult Result, VkDevice Device) VULKAN_LAYER_BODY
	static void DestroySurfaceKHR(VkResult Result, VkInstance Instance, VkSurfaceKHR SurfaceKHR) VULKAN_LAYER_BODY
	static void DestroyDescriptorSetLayout(VkResult Result, VkDevice Device, VkDescriptorSetLayout DescriptorSetLayout) VULKAN_LAYER_BODY
	static void DestroyDescriptorPool(VkResult Result, VkDevice Device, VkDescriptorPool DescriptorPool) VULKAN_LAYER_BODY
	static void ResetDescriptorPool(VkResult Result, VkDevice Device, VkDescriptorPool DescriptorPool, VkDescriptorPoolResetFlags Flags) VULKAN_LAYER_BODY
	static void DestroySwapchainKHR(VkResult Result, VkDevice Device, VkSwapchainKHR Swapchain) VULKAN_LAYER_BODY
	static void FreeCommandBuffers(VkResult Result, VkDevice Device, VkCommandPool CommandPool, uint32 CommandBufferCount, const VkCommandBuffer* CommandBuffers) VULKAN_LAYER_BODY
	static void GetPipelineCacheData(VkResult Result, VkDevice Device, VkPipelineCache PipelineCache, size_t* DataSize, void* Data) VULKAN_LAYER_BODY
	static void MergePipelineCaches(VkResult Result, VkDevice Device, VkPipelineCache DestCache, uint32 SourceCacheCount, const VkPipelineCache* SrcCaches) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceQueueFamilyProperties(VkResult Result, VkPhysicalDevice PhysicalDevice, uint32* QueueFamilyPropertyCount, VkQueueFamilyProperties* QueueFamilyProperties) VULKAN_LAYER_BODY
	static void QueueWaitIdle(VkResult Result, VkQueue Queue) VULKAN_LAYER_BODY
	static void Draw(VkResult Result, VkCommandBuffer CommandBuffer, uint32 VertexCount, uint32 InstanceCount, uint32 FirstVertex, uint32 FirstInstance) VULKAN_LAYER_BODY
	static void DrawIndexed(VkResult Result, VkCommandBuffer CommandBuffer, uint32 IndexCount, uint32 InstanceCount, uint32 FirstIndex, int32_t VertexOffset, uint32 FirstInstance) VULKAN_LAYER_BODY
	static void DrawIndirect(VkResult Result, VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset, uint32 DrawCount, uint32 Stride) VULKAN_LAYER_BODY
	static void DrawIndexedIndirect(VkResult Result, VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset, uint32 DrawCount, uint32 Stride) VULKAN_LAYER_BODY
	static void Dispatch(VkResult Result, VkCommandBuffer CommandBuffer, uint32 X, uint32 Y, uint32 Z) VULKAN_LAYER_BODY
	static void DispatchIndirect(VkResult Result, VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset) VULKAN_LAYER_BODY
	static void CopyImage(VkResult Result, VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageCopy* Regions) VULKAN_LAYER_BODY
	static void BeginQuery(VkResult Result, VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 Query, VkQueryControlFlags Flags) VULKAN_LAYER_BODY
	static void EndQuery(VkResult Result, VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 Query) VULKAN_LAYER_BODY
#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
	static void GetImageMemoryRequirements2KHR(VkResult Result, VkDevice Device, const VkImageMemoryRequirementsInfo2KHR* Info, VkMemoryRequirements2KHR* MemoryRequirements) VULKAN_LAYER_BODY
#endif
	static void ResetQueryPool(VkResult Result, VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 FirstQuery, uint32 QueryCount) VULKAN_LAYER_BODY
	static void WriteTimestamp(VkResult Result, VkCommandBuffer CommandBuffer, VkPipelineStageFlagBits PipelineStage, VkQueryPool QueryPool, uint32 Query) VULKAN_LAYER_BODY
	static void BindPipeline(VkResult Result, VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipeline Pipeline) VULKAN_LAYER_BODY
	static void ResetCommandBuffer(VkResult Result, VkCommandBuffer CommandBuffer, VkCommandBufferResetFlags Flags) VULKAN_LAYER_BODY
	static void SetViewport(VkResult Result, VkCommandBuffer CommandBuffer, uint32 FirstViewport, uint32 ViewportCount, const VkViewport* Viewports) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceFormatProperties(VkResult Result, VkPhysicalDevice PhysicalDevice, VkFormat Format, VkFormatProperties* FormatProperties) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceProperties(VkResult Result, VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceProperties* FormatProperties) VULKAN_LAYER_BODY
	static void SetScissor(VkResult Result, VkCommandBuffer CommandBuffer, uint32 FirstScissor, uint32 ScissorCount, const VkRect2D* Scissors) VULKAN_LAYER_BODY
	static void SetLineWidth(VkResult Result, VkCommandBuffer CommandBuffer, float LineWidth) VULKAN_LAYER_BODY
#if VULKAN_HAS_PHYSICAL_DEVICE_PROPERTIES2
	static void GetPhysicalDeviceProperties2KHR(VkResult Result, VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceProperties2KHR* Properties) VULKAN_LAYER_BODY
#endif
	static void SetDepthBias(VkResult Result, VkCommandBuffer CommandBuffer, float DepthBiasConstantFactor, float DepthBiasClamp, float DepthBiasSlopeFactor) VULKAN_LAYER_BODY
	static void SetBlendConstants(VkResult Result, VkCommandBuffer CommandBuffer, const float BlendConstants[4]) VULKAN_LAYER_BODY
	static void SetDepthBounds(VkResult Result, VkCommandBuffer CommandBuffer, float MinDepthBounds, float MaxDepthBounds) VULKAN_LAYER_BODY
	static void SetStencilCompareMask(VkResult Result, VkCommandBuffer CommandBuffer, VkStencilFaceFlags FaceMask, uint32 CompareMask) VULKAN_LAYER_BODY
	static void SetStencilWriteMask(VkResult Result, VkCommandBuffer CommandBuffer, VkStencilFaceFlags FaceMask, uint32 WriteMask) VULKAN_LAYER_BODY
	static void SetStencilReference(VkResult Result, VkCommandBuffer CommandBuffer, VkStencilFaceFlags FaceMask, uint32 Reference) VULKAN_LAYER_BODY
	static void FillBuffer(VkResult Result, VkCommandBuffer CommandBuffer, VkBuffer DstBuffer, VkDeviceSize DstOffset, VkDeviceSize Size, uint32 Data) VULKAN_LAYER_BODY
	static void SetEvent(VkResult Result, VkCommandBuffer CommandBuffer, VkEvent Event, VkPipelineStageFlags StageMask) VULKAN_LAYER_BODY
	static void ResetEvent(VkResult Result, VkCommandBuffer CommandBuffer, VkEvent Event, VkPipelineStageFlags StageMask) VULKAN_LAYER_BODY
	static void CopyQueryPoolResults(VkResult Result, VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 FirstQuery, uint32 QueryCount, VkBuffer DstBuffer, VkDeviceSize DstOffset, VkDeviceSize Stride, VkQueryResultFlags Flags) VULKAN_LAYER_BODY
	static void GetDeviceProcAddr(VkResult Result, VkDevice Device, const char* Name, PFN_vkVoidFunction VoidFunction) VULKAN_LAYER_BODY
	VULKAN_EXTERN_EXPORT static void EnumerateDeviceExtensionProperties(VkResult Result, VkPhysicalDevice PhysicalDevice, const char* LayerName, uint32* PropertyCount, VkExtensionProperties* Properties) VULKAN_LAYER_BODY
	static void EnumerateDeviceLayerProperties(VkResult Result, VkPhysicalDevice PhysicalDevice, uint32* PropertyCount, VkLayerProperties* Properties) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceSurfaceCapabilitiesKHR(VkResult Result, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkSurfaceCapabilitiesKHR* SurfaceCapabilities) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceSurfaceFormatsKHR(VkResult Result, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, uint32_t* SurfaceFormatCountPtr, VkSurfaceFormatKHR* SurfaceFormats) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceSurfaceSupportKHR(VkResult Result, VkPhysicalDevice PhysicalDevice, uint32_t QueueFamilyIndex, VkSurfaceKHR Surface, VkBool32* SupportedPtr) VULKAN_LAYER_BODY
	static void GetPhysicalDeviceSurfacePresentModesKHR(VkResult Result, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, uint32_t* PresentModeCountPtr, VkPresentModeKHR* PresentModesPtr) VULKAN_LAYER_BODY
#if VULKAN_USE_CREATE_ANDROID_SURFACE
	static void CreateAndroidSurfaceKHR(VkResult Result, VkInstance Instance, const VkAndroidSurfaceCreateInfoKHR* CreateInfo, VkSurfaceKHR* Surface) VULKAN_LAYER_BODY
#endif
#if VULKAN_USE_CREATE_WIN32_SURFACE
	static void CreateWin32SurfaceKHR(VkResult Result, VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, VkSurfaceKHR* pSurface);
#endif
};

#undef VULKAN_LAYER_BODY

namespace VulkanRHI
{
#if VULKAN_ENABLE_DUMP_LAYER
	void FlushDebugWrapperLog();
#else
	#define FlushDebugWrapperLog()
#endif

	FORCEINLINE_DEBUGGABLE VkResult  vkCreateInstance(const VkInstanceCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkInstance* Instance)
	{
		FWrapLayer::CreateInstance(VK_RESULT_MAX_ENUM, CreateInfo, Instance);
		VkResult Result = VULKANAPINAMESPACE::vkCreateInstance(CreateInfo, Allocator, Instance);
		FWrapLayer::CreateInstance(Result, CreateInfo, Instance);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyInstance(VkInstance Instance, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyInstance(VK_RESULT_MAX_ENUM, Instance);
		VULKANAPINAMESPACE::vkDestroyInstance(Instance, Allocator);
		FWrapLayer::DestroyInstance(VK_SUCCESS, Instance);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumeratePhysicalDevices(VkInstance Instance, uint32* PhysicalDeviceCount, VkPhysicalDevice* PhysicalDevices)
	{
		FWrapLayer::EnumeratePhysicalDevices(VK_RESULT_MAX_ENUM, Instance, PhysicalDeviceCount, PhysicalDevices);
		VkResult Result = VULKANAPINAMESPACE::vkEnumeratePhysicalDevices(Instance, PhysicalDeviceCount, PhysicalDevices);
		FWrapLayer::EnumeratePhysicalDevices(Result, Instance, PhysicalDeviceCount, PhysicalDevices);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceFeatures(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceFeatures* Features)
	{
		FWrapLayer::GetPhysicalDeviceFeatures(VK_RESULT_MAX_ENUM, PhysicalDevice, Features);
		VULKANAPINAMESPACE::vkGetPhysicalDeviceFeatures(PhysicalDevice, Features);
		FWrapLayer::GetPhysicalDeviceFeatures(VK_SUCCESS, PhysicalDevice, Features);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkFormatProperties* FormatProperties)
	{
		FWrapLayer::GetPhysicalDeviceFormatProperties(VK_RESULT_MAX_ENUM, PhysicalDevice, Format, FormatProperties);
		VULKANAPINAMESPACE::vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Format, FormatProperties);
		FWrapLayer::GetPhysicalDeviceFormatProperties(VK_SUCCESS, PhysicalDevice, Format, FormatProperties);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE VkResult  vkGetPhysicalDeviceImageFormatProperties(
		VkPhysicalDevice                            physicalDevice,
		VkFormat                                    format,
		VkImageType                                 type,
		VkImageTiling                               tiling,
		VkImageUsageFlags                           usage,
		VkImageCreateFlags                          flags,
		VkImageFormatProperties*                    pImageFormatProperties);
#endif
	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceProperties(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceProperties* Properties)
	{
		FWrapLayer::GetPhysicalDeviceProperties(VK_RESULT_MAX_ENUM, PhysicalDevice, Properties);
		VULKANAPINAMESPACE::vkGetPhysicalDeviceProperties(PhysicalDevice, Properties);
		FWrapLayer::GetPhysicalDeviceProperties(VK_SUCCESS, PhysicalDevice, Properties);
	}

#if VULKAN_HAS_PHYSICAL_DEVICE_PROPERTIES2
	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceProperties2KHR* Properties)
	{
		FWrapLayer::GetPhysicalDeviceProperties2KHR(VK_RESULT_MAX_ENUM, PhysicalDevice, Properties);
		if (VULKANAPINAMESPACE::vkGetPhysicalDeviceProperties2KHR != nullptr)
		{
			VULKANAPINAMESPACE::vkGetPhysicalDeviceProperties2KHR(PhysicalDevice, Properties);
		}
		FWrapLayer::GetPhysicalDeviceProperties2KHR(VK_SUCCESS, PhysicalDevice, Properties);
	}
#endif

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice PhysicalDevice, uint32* QueueFamilyPropertyCount, VkQueueFamilyProperties* QueueFamilyProperties)
	{
		FWrapLayer::GetPhysicalDeviceQueueFamilyProperties(VK_RESULT_MAX_ENUM, PhysicalDevice, QueueFamilyPropertyCount, QueueFamilyProperties);
		VULKANAPINAMESPACE::vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, QueueFamilyPropertyCount, QueueFamilyProperties);
		FWrapLayer::GetPhysicalDeviceQueueFamilyProperties(VK_SUCCESS, PhysicalDevice, QueueFamilyPropertyCount, QueueFamilyProperties);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceMemoryProperties* MemoryProperties)
	{
		FWrapLayer::GetPhysicalDeviceMemoryProperties(VK_RESULT_MAX_ENUM, PhysicalDevice, MemoryProperties);
		VULKANAPINAMESPACE::vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, MemoryProperties);
		FWrapLayer::GetPhysicalDeviceMemoryProperties(VK_SUCCESS, PhysicalDevice, MemoryProperties);
	}

	static FORCEINLINE_DEBUGGABLE PFN_vkVoidFunction  vkGetInstanceProcAddr(VkInstance Instance, const char* Name)
	{
		FWrapLayer::GetInstanceProcAddr(VK_RESULT_MAX_ENUM, Instance, Name, nullptr);
		PFN_vkVoidFunction Function = VULKANAPINAMESPACE::vkGetInstanceProcAddr(Instance, Name);
		FWrapLayer::GetInstanceProcAddr(VK_SUCCESS, Instance, Name, Function);
		return Function;
	}

	static FORCEINLINE_DEBUGGABLE PFN_vkVoidFunction  vkGetDeviceProcAddr(VkDevice Device, const char* Name)
	{
		FWrapLayer::GetDeviceProcAddr(VK_RESULT_MAX_ENUM, Device, Name, nullptr);
		PFN_vkVoidFunction Function = VULKANAPINAMESPACE::vkGetDeviceProcAddr(Device, Name);
		FWrapLayer::GetDeviceProcAddr(VK_SUCCESS, Device, Name, Function);
		return Function;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateDevice(VkPhysicalDevice PhysicalDevice, const VkDeviceCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkDevice* Device)
	{
		FWrapLayer::CreateDevice(VK_RESULT_MAX_ENUM, PhysicalDevice, CreateInfo, Device);
		VkResult Result = VULKANAPINAMESPACE::vkCreateDevice(PhysicalDevice, CreateInfo, Allocator, Device);
		FWrapLayer::CreateDevice(Result, PhysicalDevice, CreateInfo, Device);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyDevice(VkDevice Device, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyDevice(VK_RESULT_MAX_ENUM, Device);
		VULKANAPINAMESPACE::vkDestroyDevice(Device, Allocator);
		FWrapLayer::DestroyDevice(VK_SUCCESS, Device);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumerateInstanceExtensionProperties(const char* LayerName, uint32* PropertyCount, VkExtensionProperties* Properties)
	{
		FWrapLayer::EnumerateInstanceExtensionProperties(VK_RESULT_MAX_ENUM, LayerName, PropertyCount, Properties);
		VkResult Result = VULKANAPINAMESPACE::vkEnumerateInstanceExtensionProperties(LayerName, PropertyCount, Properties);
		//FWrapLayer::EnumerateInstanceExtensionProperties(VK_SUCCESS, LayerName, PropertyCount, Properties);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumerateDeviceExtensionProperties(VkPhysicalDevice PhysicalDevice, const char* LayerName, uint32* PropertyCount, VkExtensionProperties* Properties)
	{
		FWrapLayer::EnumerateDeviceExtensionProperties(VK_RESULT_MAX_ENUM, PhysicalDevice, LayerName, PropertyCount, Properties);
		VkResult Result = VULKANAPINAMESPACE::vkEnumerateDeviceExtensionProperties(PhysicalDevice, LayerName, PropertyCount, Properties);
		FWrapLayer::EnumerateDeviceExtensionProperties(VK_SUCCESS, PhysicalDevice, LayerName, PropertyCount, Properties);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumerateInstanceLayerProperties(uint32* PropertyCount, VkLayerProperties* Properties)
	{
		FWrapLayer::EnumerateInstanceLayerProperties(VK_RESULT_MAX_ENUM, PropertyCount, Properties);
		VkResult Result = VULKANAPINAMESPACE::vkEnumerateInstanceLayerProperties(PropertyCount, Properties);
		FWrapLayer::EnumerateInstanceLayerProperties(VK_SUCCESS, PropertyCount, Properties);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumerateDeviceLayerProperties(VkPhysicalDevice PhysicalDevice, uint32* PropertyCount, VkLayerProperties* Properties)
	{
		FWrapLayer::EnumerateDeviceLayerProperties(VK_RESULT_MAX_ENUM, PhysicalDevice, PropertyCount, Properties);
		VkResult Result = VULKANAPINAMESPACE::vkEnumerateDeviceLayerProperties(PhysicalDevice, PropertyCount, Properties);
		FWrapLayer::EnumerateDeviceLayerProperties(VK_SUCCESS, PhysicalDevice, PropertyCount, Properties);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetDeviceQueue(VkDevice Device, uint32 QueueFamilyIndex, uint32 QueueIndex, VkQueue* Queue)
	{
		FWrapLayer::GetDeviceQueue(VK_RESULT_MAX_ENUM, Device, QueueFamilyIndex, QueueIndex, Queue);
		VULKANAPINAMESPACE::vkGetDeviceQueue(Device, QueueFamilyIndex, QueueIndex, Queue);
		FWrapLayer::GetDeviceQueue(VK_SUCCESS, Device, QueueFamilyIndex, QueueIndex, Queue);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkQueueSubmit(VkQueue Queue, uint32 SubmitCount, const VkSubmitInfo* Submits, VkFence Fence)
	{
		FWrapLayer::QueueSubmit(VK_RESULT_MAX_ENUM, Queue, SubmitCount, Submits, Fence);
		VkResult Result = VULKANAPINAMESPACE::vkQueueSubmit(Queue, SubmitCount, Submits, Fence);
		FWrapLayer::QueueSubmit(Result, Queue, SubmitCount, Submits, Fence);
		return Result;
	}
	static FORCEINLINE_DEBUGGABLE VkResult  vkQueueWaitIdle(VkQueue Queue)
	{
		FWrapLayer::QueueWaitIdle(VK_RESULT_MAX_ENUM, Queue);
		VkResult Result = VULKANAPINAMESPACE::vkQueueWaitIdle(Queue);
		FWrapLayer::QueueWaitIdle(Result, Queue);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkDeviceWaitIdle(VkDevice Device)
	{
		FWrapLayer::DeviceWaitIdle(VK_RESULT_MAX_ENUM, Device);
		VkResult Result = VULKANAPINAMESPACE::vkDeviceWaitIdle(Device);
		FWrapLayer::DeviceWaitIdle(VK_SUCCESS, Device);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkAllocateMemory(VkDevice Device, const VkMemoryAllocateInfo* AllocateInfo, const VkAllocationCallbacks* Allocator, VkDeviceMemory* Memory)
	{
		FWrapLayer::AllocateMemory(VK_RESULT_MAX_ENUM, Device, AllocateInfo, Memory);
		VkResult Result = VULKANAPINAMESPACE::vkAllocateMemory(Device, AllocateInfo, Allocator, Memory);
		FWrapLayer::AllocateMemory(Result, Device, AllocateInfo, Memory);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkFreeMemory(VkDevice Device, VkDeviceMemory Memory, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::FreeMemory(VK_RESULT_MAX_ENUM, Device, Memory);
		VULKANAPINAMESPACE::vkFreeMemory(Device, Memory, Allocator);
		FWrapLayer::FreeMemory(VK_SUCCESS, Device, Memory);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkMapMemory(VkDevice Device, VkDeviceMemory Memory, VkDeviceSize Offset, VkDeviceSize Size, VkMemoryMapFlags Flags, void** Data)
	{
		FWrapLayer::MapMemory(VK_RESULT_MAX_ENUM, Device, Memory, Offset, Size, Flags, Data);
		VkResult Result = VULKANAPINAMESPACE::vkMapMemory(Device, Memory, Offset, Size, Flags, Data);
		FWrapLayer::MapMemory(Result, Device, Memory, Offset, Size, Flags, Data);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkUnmapMemory(VkDevice Device, VkDeviceMemory Memory)
	{
		FWrapLayer::UnmapMemory(VK_RESULT_MAX_ENUM, Device, Memory);
		VULKANAPINAMESPACE::vkUnmapMemory(Device, Memory);
		FWrapLayer::UnmapMemory(VK_SUCCESS, Device, Memory);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkFlushMappedMemoryRanges(VkDevice Device, uint32 MemoryRangeCount, const VkMappedMemoryRange* MemoryRanges)
	{
		FWrapLayer::FlushMappedMemoryRanges(VK_RESULT_MAX_ENUM, Device, MemoryRangeCount, MemoryRanges);
		VkResult Result = VULKANAPINAMESPACE::vkFlushMappedMemoryRanges(Device, MemoryRangeCount, MemoryRanges);
		FWrapLayer::FlushMappedMemoryRanges(Result, Device, MemoryRangeCount, MemoryRanges);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkInvalidateMappedMemoryRanges(VkDevice Device, uint32 MemoryRangeCount, const VkMappedMemoryRange* MemoryRanges)
	{
		FWrapLayer::InvalidateMappedMemoryRanges(VK_RESULT_MAX_ENUM, Device, MemoryRangeCount, MemoryRanges);
		VkResult Result = VULKANAPINAMESPACE::vkInvalidateMappedMemoryRanges(Device, MemoryRangeCount, MemoryRanges);
		FWrapLayer::InvalidateMappedMemoryRanges(Result, Device, MemoryRangeCount, MemoryRanges);
		return Result;
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkGetDeviceMemoryCommitment(
		VkDevice                                    Device,
		VkDeviceMemory                              Memory,
		VkDeviceSize*                               pCommittedMemoryInBytes);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkBindBufferMemory(VkDevice Device, VkBuffer Buffer, VkDeviceMemory Memory, VkDeviceSize MemoryOffset)
	{
		FWrapLayer::BindBufferMemory(VK_RESULT_MAX_ENUM, Device, Buffer, Memory, MemoryOffset);
		VkResult Result = VULKANAPINAMESPACE::vkBindBufferMemory(Device, Buffer, Memory, MemoryOffset);
		FWrapLayer::BindBufferMemory(Result, Device, Buffer, Memory, MemoryOffset);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkBindImageMemory(VkDevice Device, VkImage Image, VkDeviceMemory Memory, VkDeviceSize MemoryOffset)
	{
		FWrapLayer::BindImageMemory(VK_RESULT_MAX_ENUM, Device, Image, Memory, MemoryOffset);
		VkResult Result = VULKANAPINAMESPACE::vkBindImageMemory(Device, Image, Memory, MemoryOffset);
		FWrapLayer::BindImageMemory(Result, Device, Image, Memory, MemoryOffset);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetBufferMemoryRequirements(VkDevice Device, VkBuffer Buffer, VkMemoryRequirements* MemoryRequirements)
	{
		FWrapLayer::GetBufferMemoryRequirements(VK_RESULT_MAX_ENUM, Device, Buffer, MemoryRequirements);
		VULKANAPINAMESPACE::vkGetBufferMemoryRequirements(Device, Buffer, MemoryRequirements);
		FWrapLayer::GetBufferMemoryRequirements(VK_SUCCESS, Device, Buffer, MemoryRequirements);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetImageMemoryRequirements(VkDevice Device, VkImage Image, VkMemoryRequirements* MemoryRequirements)
	{
		FWrapLayer::GetImageMemoryRequirements(VK_RESULT_MAX_ENUM, Device, Image, MemoryRequirements);
		VULKANAPINAMESPACE::vkGetImageMemoryRequirements(Device, Image, MemoryRequirements);
		FWrapLayer::GetImageMemoryRequirements(VK_SUCCESS, Device, Image, MemoryRequirements);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE void  vkGetImageSparseMemoryRequirements(
		VkDevice                                    Device,
		VkImage                                     Image,
		uint32*                                   pSparseMemoryRequirementCount,
		VkSparseImageMemoryRequirements*            pSparseMemoryRequirements);

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceSparseImageFormatProperties(
		VkPhysicalDevice                            physicalDevice,
		VkFormat                                    format,
		VkImageType                                 type,
		VkSampleCountFlagBits                       samples,
		VkImageUsageFlags                           usage,
		VkImageTiling                               tiling,
		uint32*                                   pPropertyCount,
		VkSparseImageFormatProperties*              pProperties);

	static FORCEINLINE_DEBUGGABLE VkResult  vkQueueBindSparse(
		VkQueue                                     queue,
		uint32                                    bindInfoCount,
		const VkBindSparseInfo*                     pBindInfo,
		VkFence                                     Fence);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateFence(VkDevice Device, const VkFenceCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkFence* Fence)
	{
		FWrapLayer::CreateFence(VK_RESULT_MAX_ENUM, Device, CreateInfo, Fence);
		VkResult Result = VULKANAPINAMESPACE::vkCreateFence(Device, CreateInfo, Allocator, Fence);
		FWrapLayer::CreateFence(Result, Device, CreateInfo, Fence);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyFence(VkDevice Device, VkFence Fence, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyFence(VK_RESULT_MAX_ENUM, Device, Fence);
		VULKANAPINAMESPACE::vkDestroyFence(Device, Fence, Allocator);
		FWrapLayer::DestroyFence(VK_SUCCESS, Device, Fence);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkResetFences(VkDevice Device, uint32 FenceCount, const VkFence* Fences)
	{
		FWrapLayer::ResetFences(VK_RESULT_MAX_ENUM, Device, FenceCount, Fences);
		VkResult Result = VULKANAPINAMESPACE::vkResetFences(Device, FenceCount, Fences);
		FWrapLayer::ResetFences(Result, Device, FenceCount, Fences);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkGetFenceStatus(VkDevice Device, VkFence Fence)
	{
		FWrapLayer::GetFenceStatus(VK_RESULT_MAX_ENUM, Device, Fence);
		VkResult Result = VULKANAPINAMESPACE::vkGetFenceStatus(Device, Fence);
		FWrapLayer::GetFenceStatus(Result, Device, Fence);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkWaitForFences(VkDevice Device, uint32 FenceCount, const VkFence* Fences, VkBool32 bWaitAll, uint64_t Timeout)
	{
		FWrapLayer::WaitForFences(VK_RESULT_MAX_ENUM, Device, FenceCount, Fences, bWaitAll, Timeout);
		VkResult Result = VULKANAPINAMESPACE::vkWaitForFences(Device, FenceCount, Fences, bWaitAll, Timeout);
		FWrapLayer::WaitForFences(Result, Device, FenceCount, Fences, bWaitAll, Timeout);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateSemaphore(VkDevice Device, const VkSemaphoreCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkSemaphore* Semaphore)
	{
		FWrapLayer::CreateSemaphore(VK_RESULT_MAX_ENUM, Device, CreateInfo, Semaphore);
		VkResult Result = VULKANAPINAMESPACE::vkCreateSemaphore(Device, CreateInfo, Allocator, Semaphore);
		FWrapLayer::CreateSemaphore(Result, Device, CreateInfo, Semaphore);
		return Result;
	}


	static FORCEINLINE_DEBUGGABLE void  vkDestroySemaphore(VkDevice Device, VkSemaphore Semaphore, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroySemaphore(VK_RESULT_MAX_ENUM, Device, Semaphore);
		VULKANAPINAMESPACE::vkDestroySemaphore(Device, Semaphore, Allocator);
		FWrapLayer::DestroySemaphore(VK_SUCCESS, Device, Semaphore);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateEvent(VkDevice Device, const VkEventCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkEvent* Event)
	{
		FWrapLayer::CreateEvent(VK_RESULT_MAX_ENUM, Device, CreateInfo, Event);
		VkResult Result = VULKANAPINAMESPACE::vkCreateEvent(Device, CreateInfo, Allocator, Event);
		FWrapLayer::CreateEvent(Result, Device, CreateInfo, Event);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyEvent(VkDevice Device, VkEvent Event, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyEvent(VK_RESULT_MAX_ENUM, Device, Event);
		VULKANAPINAMESPACE::vkDestroyEvent(Device, Event, Allocator);
		FWrapLayer::DestroyEvent(VK_SUCCESS, Device, Event);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE VkResult  vkGetEventStatus(
		VkDevice                                    Device,
		VkEvent                                     event);

	static FORCEINLINE_DEBUGGABLE VkResult  vkSetEvent(
		VkDevice                                    Device,
		VkEvent                                     event);

	static FORCEINLINE_DEBUGGABLE VkResult  vkResetEvent(
		VkDevice                                    Device,
		VkEvent                                     event);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateQueryPool(VkDevice Device, const VkQueryPoolCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkQueryPool* QueryPool)
	{
		FWrapLayer::CreateQueryPool(VK_RESULT_MAX_ENUM, Device, CreateInfo, QueryPool);
		VkResult Result = VULKANAPINAMESPACE::vkCreateQueryPool(Device, CreateInfo, Allocator, QueryPool);
		FWrapLayer::CreateQueryPool(Result, Device, CreateInfo, QueryPool);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyQueryPool(VkDevice Device, VkQueryPool QueryPool, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyQueryPool(VK_RESULT_MAX_ENUM, Device, QueryPool);
		VULKANAPINAMESPACE::vkDestroyQueryPool(Device, QueryPool, Allocator);
		FWrapLayer::DestroyQueryPool(VK_SUCCESS, Device, QueryPool);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkGetQueryPoolResults(VkDevice Device, VkQueryPool QueryPool,
		uint32 FirstQuery, uint32 QueryCount, size_t DataSize, void* Data, VkDeviceSize Stride, VkQueryResultFlags Flags)
	{
		FWrapLayer::GetQueryPoolResults(VK_RESULT_MAX_ENUM, Device, QueryPool, FirstQuery, QueryCount, DataSize, Data, Stride, Flags);
		VkResult Result = VULKANAPINAMESPACE::vkGetQueryPoolResults(Device, QueryPool, FirstQuery, QueryCount, DataSize, Data, Stride, Flags);
		FWrapLayer::GetQueryPoolResults(Result, Device, QueryPool, FirstQuery, QueryCount, DataSize, Data, Stride, Flags);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateBuffer(VkDevice Device, const VkBufferCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkBuffer* Buffer)
	{
		FWrapLayer::CreateBuffer(VK_RESULT_MAX_ENUM, Device, CreateInfo, Buffer);
		VkResult Result = VULKANAPINAMESPACE::vkCreateBuffer(Device, CreateInfo, Allocator, Buffer);
		FWrapLayer::CreateBuffer(Result, Device, CreateInfo, Buffer);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyBuffer(VkDevice Device, VkBuffer Buffer, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyBuffer(VK_RESULT_MAX_ENUM, Device, Buffer);
		VULKANAPINAMESPACE::vkDestroyBuffer(Device, Buffer, Allocator);
		FWrapLayer::DestroyBuffer(VK_SUCCESS, Device, Buffer);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateBufferView(VkDevice Device, const VkBufferViewCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkBufferView* View)
	{
		FWrapLayer::CreateBufferView(VK_RESULT_MAX_ENUM, Device, CreateInfo, View);
		VkResult Result = VULKANAPINAMESPACE::vkCreateBufferView(Device, CreateInfo, Allocator, View);
		FWrapLayer::CreateBufferView(Result, Device, CreateInfo, View);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyBufferView(VkDevice Device, VkBufferView BufferView, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyBufferView(VK_RESULT_MAX_ENUM, Device, BufferView);
		VULKANAPINAMESPACE::vkDestroyBufferView(Device, BufferView, Allocator);
		FWrapLayer::DestroyBufferView(VK_RESULT_MAX_ENUM, Device, BufferView);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateImage(VkDevice Device, const VkImageCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkImage* Image)
	{
		FWrapLayer::CreateImage(VK_RESULT_MAX_ENUM, Device, CreateInfo, Image);
		VkResult Result = VULKANAPINAMESPACE::vkCreateImage(Device, CreateInfo, Allocator, Image);
		FWrapLayer::CreateImage(Result, Device, CreateInfo, Image);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyImage(VkDevice Device, VkImage Image, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyImage(VK_RESULT_MAX_ENUM, Device, Image);
		VULKANAPINAMESPACE::vkDestroyImage(Device, Image, Allocator);
		FWrapLayer::DestroyImage(VK_SUCCESS, Device, Image);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetImageSubresourceLayout(VkDevice Device, VkImage Image, const VkImageSubresource* Subresource, VkSubresourceLayout* Layout)
	{
		FWrapLayer::GetImageSubresourceLayout(VK_RESULT_MAX_ENUM, Device, Image, Subresource, Layout);
		VULKANAPINAMESPACE::vkGetImageSubresourceLayout(Device, Image, Subresource, Layout);
		FWrapLayer::GetImageSubresourceLayout(VK_SUCCESS, Device, Image, Subresource, Layout);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateImageView(VkDevice Device, const VkImageViewCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkImageView* View)
	{
		FWrapLayer::CreateImageView(VK_RESULT_MAX_ENUM, Device, CreateInfo, View);
		VkResult Result = VULKANAPINAMESPACE::vkCreateImageView(Device, CreateInfo, Allocator, View);
		FWrapLayer::CreateImageView(Result, Device, CreateInfo, View);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyImageView(VkDevice Device, VkImageView ImageView, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyImageView(VK_RESULT_MAX_ENUM, Device, ImageView);
		VULKANAPINAMESPACE::vkDestroyImageView(Device, ImageView, Allocator);
		FWrapLayer::DestroyImageView(VK_SUCCESS, Device, ImageView);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateShaderModule(VkDevice Device, const VkShaderModuleCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkShaderModule* ShaderModule)
	{
		FWrapLayer::CreateShaderModule(VK_RESULT_MAX_ENUM, Device, CreateInfo, ShaderModule);
		VkResult Result = VULKANAPINAMESPACE::vkCreateShaderModule(Device, CreateInfo, Allocator, ShaderModule);
		FWrapLayer::CreateShaderModule(Result, Device, CreateInfo, ShaderModule);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyShaderModule(VkDevice Device, VkShaderModule ShaderModule, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyShaderModule(VK_RESULT_MAX_ENUM, Device, ShaderModule);
		VULKANAPINAMESPACE::vkDestroyShaderModule(Device, ShaderModule, Allocator);
		FWrapLayer::DestroyShaderModule(VK_SUCCESS, Device, ShaderModule);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreatePipelineCache(VkDevice Device, const VkPipelineCacheCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkPipelineCache* PipelineCache)
	{
		FWrapLayer::CreatePipelineCache(VK_RESULT_MAX_ENUM, Device, CreateInfo, PipelineCache);
		VkResult Result = VULKANAPINAMESPACE::vkCreatePipelineCache(Device, CreateInfo, Allocator, PipelineCache);
		FWrapLayer::CreatePipelineCache(Result, Device, CreateInfo, PipelineCache);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyPipelineCache(VkDevice Device, VkPipelineCache PipelineCache, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyPipelineCache(VK_RESULT_MAX_ENUM, Device, PipelineCache);
		VULKANAPINAMESPACE::vkDestroyPipelineCache(Device, PipelineCache, Allocator);
		FWrapLayer::DestroyPipelineCache(VK_SUCCESS, Device, PipelineCache);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkGetPipelineCacheData(VkDevice Device, VkPipelineCache PipelineCache, size_t* DataSize, void* Data)
	{
		FWrapLayer::GetPipelineCacheData(VK_RESULT_MAX_ENUM, Device, PipelineCache, DataSize, Data);
		VkResult Result = VULKANAPINAMESPACE::vkGetPipelineCacheData(Device, PipelineCache, DataSize, Data);
		FWrapLayer::GetPipelineCacheData(Result, Device, PipelineCache, DataSize, Data);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkMergePipelineCaches(VkDevice Device, VkPipelineCache DestCache, uint32 SourceCacheCount, const VkPipelineCache* SrcCaches)
	{
		FWrapLayer::MergePipelineCaches(VK_RESULT_MAX_ENUM, Device, DestCache, SourceCacheCount, SrcCaches);
		VkResult Result = VULKANAPINAMESPACE::vkMergePipelineCaches(Device, DestCache, SourceCacheCount, SrcCaches);
		FWrapLayer::MergePipelineCaches(Result, Device, DestCache, SourceCacheCount, SrcCaches);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateGraphicsPipelines(VkDevice Device, VkPipelineCache PipelineCache, uint32 CreateInfoCount, const VkGraphicsPipelineCreateInfo* CreateInfos, const VkAllocationCallbacks* Allocator, VkPipeline* Pipelines)
	{
		FWrapLayer::CreateGraphicsPipelines(VK_RESULT_MAX_ENUM, Device, PipelineCache, CreateInfoCount, CreateInfos, Pipelines);
		VkResult Result = VULKANAPINAMESPACE::vkCreateGraphicsPipelines(Device, PipelineCache, CreateInfoCount, CreateInfos, Allocator, Pipelines);
		FWrapLayer::CreateGraphicsPipelines(Result, Device, PipelineCache, CreateInfoCount, CreateInfos, Pipelines);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateComputePipelines(VkDevice Device, VkPipelineCache PipelineCache, uint32 CreateInfoCount, const VkComputePipelineCreateInfo* CreateInfos, const VkAllocationCallbacks* Allocator, VkPipeline* Pipelines)
	{
		FWrapLayer::CreateComputePipelines(VK_RESULT_MAX_ENUM, Device, PipelineCache, CreateInfoCount, CreateInfos, Pipelines);
		VkResult Result = VULKANAPINAMESPACE::vkCreateComputePipelines(Device, PipelineCache, CreateInfoCount, CreateInfos, Allocator, Pipelines);
		FWrapLayer::CreateComputePipelines(Result, Device, PipelineCache, CreateInfoCount, CreateInfos, Pipelines);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyPipeline(VkDevice Device, VkPipeline Pipeline, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyPipeline(VK_RESULT_MAX_ENUM, Device, Pipeline);
		VULKANAPINAMESPACE::vkDestroyPipeline(Device, Pipeline, Allocator);
		FWrapLayer::DestroyPipeline(VK_SUCCESS, Device, Pipeline);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreatePipelineLayout(VkDevice Device, const VkPipelineLayoutCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkPipelineLayout* PipelineLayout)
	{
		FWrapLayer::CreatePipelineLayout(VK_RESULT_MAX_ENUM, Device, CreateInfo, PipelineLayout);
		VkResult Result = VULKANAPINAMESPACE::vkCreatePipelineLayout(Device, CreateInfo, Allocator, PipelineLayout);
		FWrapLayer::CreatePipelineLayout(Result, Device, CreateInfo, PipelineLayout);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyPipelineLayout(VkDevice Device, VkPipelineLayout PipelineLayout, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyPipelineLayout(VK_RESULT_MAX_ENUM, Device, PipelineLayout);
		VULKANAPINAMESPACE::vkDestroyPipelineLayout(Device, PipelineLayout, Allocator);
		FWrapLayer::DestroyPipelineLayout(VK_SUCCESS, Device, PipelineLayout);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateSampler(VkDevice Device, const VkSamplerCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkSampler* Sampler)
	{
		FWrapLayer::CreateSampler(VK_RESULT_MAX_ENUM, Device, CreateInfo, Sampler);
		VkResult Result = VULKANAPINAMESPACE::vkCreateSampler(Device, CreateInfo, Allocator, Sampler);
		FWrapLayer::CreateSampler(Result, Device, CreateInfo, Sampler);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroySampler(VkDevice Device, VkSampler Sampler, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroySampler(VK_RESULT_MAX_ENUM, Device, Sampler);
		VULKANAPINAMESPACE::vkDestroySampler(Device, Sampler, Allocator);
		FWrapLayer::DestroySampler(VK_RESULT_MAX_ENUM, Device, Sampler);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateDescriptorSetLayout(VkDevice Device, const VkDescriptorSetLayoutCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkDescriptorSetLayout* SetLayout)
	{
		FWrapLayer::CreateDescriptorSetLayout(VK_RESULT_MAX_ENUM, Device, CreateInfo, SetLayout);
		VkResult Result = VULKANAPINAMESPACE::vkCreateDescriptorSetLayout(Device, CreateInfo, Allocator, SetLayout);
		FWrapLayer::CreateDescriptorSetLayout(Result, Device, CreateInfo, SetLayout);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyDescriptorSetLayout(VkDevice Device, VkDescriptorSetLayout DescriptorSetLayout, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyDescriptorSetLayout(VK_RESULT_MAX_ENUM, Device, DescriptorSetLayout);
		VULKANAPINAMESPACE::vkDestroyDescriptorSetLayout(Device, DescriptorSetLayout, Allocator);
		FWrapLayer::DestroyDescriptorSetLayout(VK_SUCCESS, Device, DescriptorSetLayout);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateDescriptorPool(VkDevice Device, const VkDescriptorPoolCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkDescriptorPool* DescriptorPool)
	{
		FWrapLayer::CreateDescriptorPool(VK_RESULT_MAX_ENUM, Device, CreateInfo, DescriptorPool);
		VkResult Result = VULKANAPINAMESPACE::vkCreateDescriptorPool(Device, CreateInfo, Allocator, DescriptorPool);
		FWrapLayer::CreateDescriptorPool(Result, Device, CreateInfo, DescriptorPool);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyDescriptorPool(VkDevice Device, VkDescriptorPool DescriptorPool, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyDescriptorPool(VK_RESULT_MAX_ENUM, Device, DescriptorPool);
		VULKANAPINAMESPACE::vkDestroyDescriptorPool(Device, DescriptorPool, Allocator);
		FWrapLayer::DestroyDescriptorPool(VK_SUCCESS, Device, DescriptorPool);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkResetDescriptorPool(VkDevice Device, VkDescriptorPool DescriptorPool, VkDescriptorPoolResetFlags Flags)
	{
		FWrapLayer::ResetDescriptorPool(VK_RESULT_MAX_ENUM, Device, DescriptorPool, Flags);
		VkResult Result = VULKANAPINAMESPACE::vkResetDescriptorPool(Device, DescriptorPool, Flags);
		FWrapLayer::ResetDescriptorPool(Result, Device, DescriptorPool, Flags);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkAllocateDescriptorSets(VkDevice Device, const VkDescriptorSetAllocateInfo* AllocateInfo, VkDescriptorSet* DescriptorSets)
	{
		FWrapLayer::AllocateDescriptorSets(VK_RESULT_MAX_ENUM, Device, AllocateInfo, DescriptorSets);
		VkResult Result = VULKANAPINAMESPACE::vkAllocateDescriptorSets(Device, AllocateInfo, DescriptorSets);
		FWrapLayer::AllocateDescriptorSets(Result, Device, AllocateInfo, DescriptorSets);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkFreeDescriptorSets(VkDevice Device, VkDescriptorPool DescriptorPool, uint32 DescriptorSetCount, const VkDescriptorSet* DescriptorSets)
	{
		FWrapLayer::FreeDescriptorSets(VK_RESULT_MAX_ENUM, Device, DescriptorPool, DescriptorSetCount, DescriptorSets);
		VkResult Result = VULKANAPINAMESPACE::vkFreeDescriptorSets(Device, DescriptorPool, DescriptorSetCount, DescriptorSets);
		FWrapLayer::FreeDescriptorSets(Result, Device, DescriptorPool, DescriptorSetCount, DescriptorSets);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkUpdateDescriptorSets(VkDevice Device, uint32 DescriptorWriteCount, const VkWriteDescriptorSet* DescriptorWrites, uint32 DescriptorCopyCount, const VkCopyDescriptorSet* DescriptorCopies)
	{
		FWrapLayer::UpdateDescriptorSets(VK_RESULT_MAX_ENUM, Device, DescriptorWriteCount, DescriptorWrites, DescriptorCopyCount, DescriptorCopies);
		VULKANAPINAMESPACE::vkUpdateDescriptorSets(Device, DescriptorWriteCount, DescriptorWrites, DescriptorCopyCount, DescriptorCopies);
		FWrapLayer::UpdateDescriptorSets(VK_SUCCESS, Device, DescriptorWriteCount, DescriptorWrites, DescriptorCopyCount, DescriptorCopies);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateFramebuffer(VkDevice Device, const VkFramebufferCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkFramebuffer* Framebuffer)
	{
		FWrapLayer::CreateFramebuffer(VK_RESULT_MAX_ENUM, Device, CreateInfo, Framebuffer);
		VkResult Result = VULKANAPINAMESPACE::vkCreateFramebuffer(Device, CreateInfo, Allocator, Framebuffer);
		FWrapLayer::CreateFramebuffer(Result, Device, CreateInfo, Framebuffer);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyFramebuffer(VkDevice Device, VkFramebuffer Framebuffer, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyFramebuffer(VK_RESULT_MAX_ENUM, Device, Framebuffer);
		VULKANAPINAMESPACE::vkDestroyFramebuffer(Device, Framebuffer, Allocator);
		FWrapLayer::DestroyFramebuffer(VK_SUCCESS, Device, Framebuffer);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateRenderPass(VkDevice Device, const VkRenderPassCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkRenderPass* RenderPass)
	{
		FWrapLayer::CreateRenderPass(VK_RESULT_MAX_ENUM, Device, CreateInfo, RenderPass);
		VkResult Result = VULKANAPINAMESPACE::vkCreateRenderPass(Device, CreateInfo, Allocator, RenderPass);
		FWrapLayer::CreateRenderPass(Result, Device, CreateInfo, RenderPass);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyRenderPass(VkDevice Device, VkRenderPass RenderPass, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyRenderPass(VK_RESULT_MAX_ENUM, Device, RenderPass);
		VULKANAPINAMESPACE::vkDestroyRenderPass(Device, RenderPass, Allocator);
		FWrapLayer::DestroyRenderPass(VK_SUCCESS, Device, RenderPass);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkGetRenderAreaGranularity(
		VkDevice                                    Device,
		VkRenderPass                                renderPass,
		VkExtent2D*                                 pGranularity);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateCommandPool(VkDevice Device, const VkCommandPoolCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkCommandPool* CommandPool)
	{
		FWrapLayer::CreateCommandPool(VK_RESULT_MAX_ENUM, Device, CreateInfo, CommandPool);
		VkResult Result = VULKANAPINAMESPACE::vkCreateCommandPool(Device, CreateInfo, Allocator, CommandPool);
		FWrapLayer::CreateCommandPool(Result, Device, CreateInfo, CommandPool);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyCommandPool(VkDevice Device, VkCommandPool CommandPool, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroyCommandPool(VK_RESULT_MAX_ENUM, Device, CommandPool);
		VULKANAPINAMESPACE::vkDestroyCommandPool(Device, CommandPool, Allocator);
		FWrapLayer::DestroyCommandPool(VK_SUCCESS, Device, CommandPool);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE VkResult  vkResetCommandPool(
		VkDevice                                    Device,
		VkCommandPool                               commandPool,
		VkCommandPoolResetFlags                     flags);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkAllocateCommandBuffers(VkDevice Device, const VkCommandBufferAllocateInfo* AllocateInfo, VkCommandBuffer* CommandBuffers)
	{
		FWrapLayer::AllocateCommandBuffers(VK_RESULT_MAX_ENUM, Device, AllocateInfo, CommandBuffers);
		VkResult Result = VULKANAPINAMESPACE::vkAllocateCommandBuffers(Device, AllocateInfo, CommandBuffers);
		FWrapLayer::AllocateCommandBuffers(Result, Device, AllocateInfo, CommandBuffers);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkFreeCommandBuffers(VkDevice Device, VkCommandPool CommandPool, uint32 CommandBufferCount, const VkCommandBuffer* CommandBuffers)
	{
		FWrapLayer::FreeCommandBuffers(VK_RESULT_MAX_ENUM, Device, CommandPool, CommandBufferCount, CommandBuffers);
		VULKANAPINAMESPACE::vkFreeCommandBuffers(Device, CommandPool, CommandBufferCount, CommandBuffers);
		FWrapLayer::FreeCommandBuffers(VK_SUCCESS, Device, CommandPool, CommandBufferCount, CommandBuffers);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkBeginCommandBuffer(VkCommandBuffer CommandBuffer, const VkCommandBufferBeginInfo* BeginInfo)
	{
		FWrapLayer::BeginCommandBuffer(VK_RESULT_MAX_ENUM, CommandBuffer, BeginInfo);
		VkResult Result = VULKANAPINAMESPACE::vkBeginCommandBuffer(CommandBuffer, BeginInfo);
		FWrapLayer::BeginCommandBuffer(Result, CommandBuffer, BeginInfo);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEndCommandBuffer(VkCommandBuffer CommandBuffer)
	{
		FWrapLayer::EndCommandBuffer(VK_RESULT_MAX_ENUM, CommandBuffer);
		VkResult Result = VULKANAPINAMESPACE::vkEndCommandBuffer(CommandBuffer);
		FWrapLayer::EndCommandBuffer(Result, CommandBuffer);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkResetCommandBuffer(VkCommandBuffer CommandBuffer, VkCommandBufferResetFlags Flags)
	{
		FWrapLayer::ResetCommandBuffer(VK_RESULT_MAX_ENUM, CommandBuffer, Flags);
		VkResult Result = VULKANAPINAMESPACE::vkResetCommandBuffer(CommandBuffer, Flags);
		FWrapLayer::ResetCommandBuffer(Result, CommandBuffer, Flags);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBindPipeline(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipeline Pipeline)
	{
		FWrapLayer::BindPipeline(VK_RESULT_MAX_ENUM, CommandBuffer, PipelineBindPoint, Pipeline);
		VULKANAPINAMESPACE::vkCmdBindPipeline(CommandBuffer, PipelineBindPoint, Pipeline);
		FWrapLayer::BindPipeline(VK_SUCCESS, CommandBuffer, PipelineBindPoint, Pipeline);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetViewport(VkCommandBuffer CommandBuffer, uint32 FirstViewport, uint32 ViewportCount, const VkViewport* Viewports)
	{
		FWrapLayer::SetViewport(VK_RESULT_MAX_ENUM, CommandBuffer, FirstViewport, ViewportCount, Viewports);
		VULKANAPINAMESPACE::vkCmdSetViewport(CommandBuffer, FirstViewport, ViewportCount, Viewports);
		FWrapLayer::SetViewport(VK_SUCCESS, CommandBuffer, FirstViewport, ViewportCount, Viewports);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetScissor(VkCommandBuffer CommandBuffer, uint32 FirstScissor, uint32 ScissorCount, const VkRect2D* Scissors)
	{
		FWrapLayer::SetScissor(VK_RESULT_MAX_ENUM, CommandBuffer, FirstScissor, ScissorCount, Scissors);
		VULKANAPINAMESPACE::vkCmdSetScissor(CommandBuffer, FirstScissor, ScissorCount, Scissors);
		FWrapLayer::SetScissor(VK_SUCCESS, CommandBuffer, FirstScissor, ScissorCount, Scissors);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetLineWidth(VkCommandBuffer CommandBuffer, float LineWidth)
	{
		FWrapLayer::SetLineWidth(VK_RESULT_MAX_ENUM, CommandBuffer, LineWidth);
		VULKANAPINAMESPACE::vkCmdSetLineWidth(CommandBuffer, LineWidth);
		FWrapLayer::SetLineWidth(VK_SUCCESS, CommandBuffer, LineWidth);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetDepthBias(VkCommandBuffer CommandBuffer, float DepthBiasConstantFactor, float DepthBiasClamp, float DepthBiasSlopeFactor)
	{
		FWrapLayer::SetDepthBias(VK_RESULT_MAX_ENUM, CommandBuffer, DepthBiasConstantFactor, DepthBiasClamp, DepthBiasSlopeFactor);
		VULKANAPINAMESPACE::vkCmdSetDepthBias(CommandBuffer, DepthBiasConstantFactor, DepthBiasClamp, DepthBiasSlopeFactor);
		FWrapLayer::SetDepthBias(VK_SUCCESS, CommandBuffer, DepthBiasConstantFactor, DepthBiasClamp, DepthBiasSlopeFactor);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetBlendConstants(VkCommandBuffer CommandBuffer, const float BlendConstants[4])
	{
		FWrapLayer::SetBlendConstants(VK_RESULT_MAX_ENUM, CommandBuffer, BlendConstants);
		VULKANAPINAMESPACE::vkCmdSetBlendConstants(CommandBuffer, BlendConstants);
		FWrapLayer::SetBlendConstants(VK_SUCCESS, CommandBuffer, BlendConstants);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetDepthBounds(VkCommandBuffer CommandBuffer, float MinDepthBounds, float MaxDepthBounds)
	{
		FWrapLayer::SetDepthBounds(VK_RESULT_MAX_ENUM, CommandBuffer, MinDepthBounds, MaxDepthBounds);
		VULKANAPINAMESPACE::vkCmdSetDepthBounds(CommandBuffer, MinDepthBounds, MaxDepthBounds);
		FWrapLayer::SetDepthBounds(VK_SUCCESS, CommandBuffer, MinDepthBounds, MaxDepthBounds);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetStencilCompareMask(VkCommandBuffer CommandBuffer, VkStencilFaceFlags FaceMask, uint32 CompareMask)
	{
		FWrapLayer::SetStencilCompareMask(VK_RESULT_MAX_ENUM, CommandBuffer, FaceMask, CompareMask);
		VULKANAPINAMESPACE::vkCmdSetStencilCompareMask(CommandBuffer, FaceMask, CompareMask);
		FWrapLayer::SetStencilCompareMask(VK_SUCCESS, CommandBuffer, FaceMask, CompareMask);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetStencilWriteMask(VkCommandBuffer CommandBuffer, VkStencilFaceFlags FaceMask, uint32 WriteMask)
	{
		FWrapLayer::SetStencilWriteMask(VK_RESULT_MAX_ENUM, CommandBuffer, FaceMask, WriteMask);
		VULKANAPINAMESPACE::vkCmdSetStencilWriteMask(CommandBuffer, FaceMask, WriteMask);
		FWrapLayer::SetStencilWriteMask(VK_SUCCESS, CommandBuffer, FaceMask, WriteMask);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetStencilReference(VkCommandBuffer CommandBuffer, VkStencilFaceFlags FaceMask, uint32 Reference)
	{
		FWrapLayer::SetStencilReference(VK_RESULT_MAX_ENUM, CommandBuffer, FaceMask, Reference);
		VULKANAPINAMESPACE::vkCmdSetStencilReference(CommandBuffer, FaceMask, Reference);
		FWrapLayer::SetStencilReference(VK_SUCCESS, CommandBuffer, FaceMask, Reference);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBindDescriptorSets(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipelineLayout Layout, uint32 FirstSet, uint32 DescriptorSetCount, const VkDescriptorSet* DescriptorSets, uint32 DynamicOffsetCount, const uint32* DynamicOffsets)
	{
		FWrapLayer::BindDescriptorSets(VK_RESULT_MAX_ENUM, CommandBuffer, PipelineBindPoint, Layout, FirstSet, DescriptorSetCount, DescriptorSets, DynamicOffsetCount, DynamicOffsets);
		VULKANAPINAMESPACE::vkCmdBindDescriptorSets(CommandBuffer, PipelineBindPoint, Layout, FirstSet, DescriptorSetCount, DescriptorSets, DynamicOffsetCount, DynamicOffsets);
		FWrapLayer::BindDescriptorSets(VK_SUCCESS, CommandBuffer, PipelineBindPoint, Layout, FirstSet, DescriptorSetCount, DescriptorSets, DynamicOffsetCount, DynamicOffsets);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBindIndexBuffer(VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset, VkIndexType IndexType)
	{
		FWrapLayer::BindIndexBuffer(VK_RESULT_MAX_ENUM, CommandBuffer, Buffer, Offset, IndexType);
		VULKANAPINAMESPACE::vkCmdBindIndexBuffer(CommandBuffer, Buffer, Offset, IndexType);
		FWrapLayer::BindIndexBuffer(VK_SUCCESS, CommandBuffer, Buffer, Offset, IndexType);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBindVertexBuffers(VkCommandBuffer CommandBuffer, uint32 FirstBinding, uint32 BindingCount, const VkBuffer* Buffers, const VkDeviceSize* Offsets)
	{
		FWrapLayer::BindVertexBuffers(VK_RESULT_MAX_ENUM, CommandBuffer, FirstBinding, BindingCount, Buffers, Offsets);
		VULKANAPINAMESPACE::vkCmdBindVertexBuffers(CommandBuffer, FirstBinding, BindingCount, Buffers, Offsets);
		FWrapLayer::BindVertexBuffers(VK_SUCCESS, CommandBuffer, FirstBinding, BindingCount, Buffers, Offsets);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDraw(VkCommandBuffer CommandBuffer, uint32 VertexCount, uint32 InstanceCount, uint32 FirstVertex, uint32 FirstInstance)
	{
		FWrapLayer::Draw(VK_RESULT_MAX_ENUM, CommandBuffer, VertexCount, InstanceCount, FirstVertex, FirstInstance);
		VULKANAPINAMESPACE::vkCmdDraw(CommandBuffer, VertexCount, InstanceCount, FirstVertex, FirstInstance);
		FWrapLayer::Draw(VK_SUCCESS, CommandBuffer, VertexCount, InstanceCount, FirstVertex, FirstInstance);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDrawIndexed(VkCommandBuffer CommandBuffer, uint32 IndexCount, uint32 InstanceCount, uint32 FirstIndex, int32_t VertexOffset, uint32 FirstInstance)
	{
		FWrapLayer::DrawIndexed(VK_RESULT_MAX_ENUM, CommandBuffer, IndexCount, InstanceCount, FirstIndex, VertexOffset, FirstInstance);
		VULKANAPINAMESPACE::vkCmdDrawIndexed(CommandBuffer, IndexCount, InstanceCount, FirstIndex, VertexOffset, FirstInstance);
		FWrapLayer::DrawIndexed(VK_SUCCESS, CommandBuffer, IndexCount, InstanceCount, FirstIndex, VertexOffset, FirstInstance);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDrawIndirect(VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset, uint32 DrawCount, uint32 Stride)
	{
		FWrapLayer::DrawIndirect(VK_RESULT_MAX_ENUM, CommandBuffer, Buffer, Offset, DrawCount, Stride);
		VULKANAPINAMESPACE::vkCmdDrawIndirect(CommandBuffer, Buffer, Offset, DrawCount, Stride);
		FWrapLayer::DrawIndirect(VK_SUCCESS, CommandBuffer, Buffer, Offset, DrawCount, Stride);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDrawIndexedIndirect(VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset, uint32 DrawCount, uint32 Stride)
	{
		FWrapLayer::DrawIndexedIndirect(VK_RESULT_MAX_ENUM, CommandBuffer, Buffer, Offset, DrawCount, Stride);
		VULKANAPINAMESPACE::vkCmdDrawIndexedIndirect(CommandBuffer, Buffer, Offset, DrawCount, Stride);
		FWrapLayer::DrawIndexedIndirect(VK_SUCCESS, CommandBuffer, Buffer, Offset, DrawCount, Stride);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDispatch(VkCommandBuffer CommandBuffer, uint32 X, uint32 Y, uint32 Z)
	{
		FWrapLayer::Dispatch(VK_RESULT_MAX_ENUM, CommandBuffer, X, Y, Z);
		VULKANAPINAMESPACE::vkCmdDispatch(CommandBuffer, X, Y, Z);
		FWrapLayer::Dispatch(VK_SUCCESS, CommandBuffer, X, Y, Z);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDispatchIndirect(VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset)
	{
		FWrapLayer::DispatchIndirect(VK_RESULT_MAX_ENUM, CommandBuffer, Buffer, Offset);
		VULKANAPINAMESPACE::vkCmdDispatchIndirect(CommandBuffer, Buffer, Offset);
		FWrapLayer::DispatchIndirect(VK_SUCCESS, CommandBuffer, Buffer, Offset);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyBuffer(VkCommandBuffer CommandBuffer, VkBuffer SrcBuffer, VkBuffer DstBuffer, uint32 RegionCount, const VkBufferCopy* Regions)
	{
		FWrapLayer::CopyBuffer(VK_RESULT_MAX_ENUM, CommandBuffer, SrcBuffer, DstBuffer, RegionCount, Regions);
		VULKANAPINAMESPACE::vkCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, RegionCount, Regions);
		FWrapLayer::CopyBuffer(VK_SUCCESS, CommandBuffer, SrcBuffer, DstBuffer, RegionCount, Regions);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyImage(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageCopy* Regions)
	{
		FWrapLayer::CopyImage(VK_RESULT_MAX_ENUM, CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);
		VULKANAPINAMESPACE::vkCmdCopyImage(CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);
		FWrapLayer::CopyImage(VK_SUCCESS, CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBlitImage(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageBlit* Regions, VkFilter Filter)
	{
		FWrapLayer::BlitImage(VK_RESULT_MAX_ENUM, CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions, Filter);
		VULKANAPINAMESPACE::vkCmdBlitImage(CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions, Filter);
		FWrapLayer::BlitImage(VK_SUCCESS, CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions, Filter);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyBufferToImage(VkCommandBuffer CommandBuffer, VkBuffer SrcBuffer, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkBufferImageCopy* Regions)
	{
		FWrapLayer::CopyBufferToImage(VK_RESULT_MAX_ENUM, CommandBuffer, SrcBuffer, DstImage, DstImageLayout, RegionCount, Regions);
		VULKANAPINAMESPACE::vkCmdCopyBufferToImage(CommandBuffer, SrcBuffer, DstImage, DstImageLayout, RegionCount, Regions);
		FWrapLayer::CopyBufferToImage(VK_SUCCESS, CommandBuffer, SrcBuffer, DstImage, DstImageLayout, RegionCount, Regions);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyImageToBuffer(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkBuffer DstBuffer, uint32 RegionCount, const VkBufferImageCopy* Regions)
	{
		FWrapLayer::CopyImageToBuffer(VK_RESULT_MAX_ENUM, CommandBuffer, SrcImage, SrcImageLayout, DstBuffer, RegionCount, Regions);
		VULKANAPINAMESPACE::vkCmdCopyImageToBuffer(CommandBuffer, SrcImage, SrcImageLayout, DstBuffer, RegionCount, Regions);
		FWrapLayer::CopyImageToBuffer(VK_SUCCESS, CommandBuffer, SrcImage, SrcImageLayout, DstBuffer, RegionCount, Regions);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdUpdateBuffer(
		VkCommandBuffer                             commandBuffer,
		VkBuffer                                    dstBuffer,
		VkDeviceSize                                dstOffset,
		VkDeviceSize                                dataSize,
		const uint32*                             pData);
#endif

	static FORCEINLINE_DEBUGGABLE void  vkCmdFillBuffer(VkCommandBuffer CommandBuffer, VkBuffer DstBuffer, VkDeviceSize DstOffset, VkDeviceSize Size, uint32 Data)
	{
		FWrapLayer::FillBuffer(VK_RESULT_MAX_ENUM, CommandBuffer, DstBuffer, DstOffset, Size, Data);
		VULKANAPINAMESPACE::vkCmdFillBuffer(CommandBuffer, DstBuffer, DstOffset, Size, Data);
		FWrapLayer::FillBuffer(VK_SUCCESS, CommandBuffer, DstBuffer, DstOffset, Size, Data);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdClearColorImage(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout ImageLayout, const VkClearColorValue* Color, uint32 RangeCount, const VkImageSubresourceRange* Ranges)
	{
		FWrapLayer::ClearColorImage(VK_RESULT_MAX_ENUM, CommandBuffer, Image, ImageLayout, Color, RangeCount, Ranges);
		VULKANAPINAMESPACE::vkCmdClearColorImage(CommandBuffer, Image, ImageLayout, Color, RangeCount, Ranges);
		FWrapLayer::ClearColorImage(VK_SUCCESS, CommandBuffer, Image, ImageLayout, Color, RangeCount, Ranges);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdClearDepthStencilImage(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout ImageLayout, const VkClearDepthStencilValue* DepthStencil, uint32 RangeCount, const VkImageSubresourceRange* Ranges)
	{
		FWrapLayer::ClearDepthStencilImage(VK_RESULT_MAX_ENUM, CommandBuffer, Image, ImageLayout, DepthStencil, RangeCount, Ranges);
		VULKANAPINAMESPACE::vkCmdClearDepthStencilImage(CommandBuffer, Image, ImageLayout, DepthStencil, RangeCount, Ranges);
		FWrapLayer::ClearDepthStencilImage(VK_SUCCESS, CommandBuffer, Image, ImageLayout, DepthStencil, RangeCount, Ranges);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdClearAttachments(VkCommandBuffer CommandBuffer, uint32 AttachmentCount, const VkClearAttachment* Attachments, uint32 RectCount, const VkClearRect* Rects)
	{
		FWrapLayer::ClearAttachments(VK_RESULT_MAX_ENUM, CommandBuffer, AttachmentCount, Attachments, RectCount, Rects);
		VULKANAPINAMESPACE::vkCmdClearAttachments(CommandBuffer, AttachmentCount, Attachments, RectCount, Rects);
		FWrapLayer::ClearAttachments(VK_SUCCESS, CommandBuffer, AttachmentCount, Attachments, RectCount, Rects);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdResolveImage(
		VkCommandBuffer CommandBuffer,
		VkImage SrcImage, VkImageLayout SrcImageLayout,
		VkImage DstImage, VkImageLayout DstImageLayout,
		uint32 RegionCount, const VkImageResolve* Regions)
	{
		FWrapLayer::ResolveImage(VK_RESULT_MAX_ENUM, CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);
		VULKANAPINAMESPACE::vkCmdResolveImage(CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);
		FWrapLayer::ResolveImage(VK_SUCCESS, CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetEvent(VkCommandBuffer CommandBuffer, VkEvent Event, VkPipelineStageFlags StageMask)
	{
		FWrapLayer::SetEvent(VK_RESULT_MAX_ENUM, CommandBuffer, Event, StageMask);
		VULKANAPINAMESPACE::vkCmdSetEvent(CommandBuffer, Event, StageMask);
		FWrapLayer::SetEvent(VK_SUCCESS, CommandBuffer, Event, StageMask);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdResetEvent(VkCommandBuffer CommandBuffer, VkEvent Event, VkPipelineStageFlags StageMask)
	{
		FWrapLayer::ResetEvent(VK_RESULT_MAX_ENUM, CommandBuffer, Event, StageMask);
		VULKANAPINAMESPACE::vkCmdResetEvent(CommandBuffer, Event, StageMask);
		FWrapLayer::ResetEvent(VK_SUCCESS, CommandBuffer, Event, StageMask);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdWaitEvents(VkCommandBuffer CommandBuffer, uint32 EventCount, const VkEvent* Events,
		VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask,
		uint32 MemoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers,
		uint32 BufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers,
		uint32 ImageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
	{
		FWrapLayer::WaitEvents(VK_RESULT_MAX_ENUM, CommandBuffer, EventCount, Events, SrcStageMask, DstStageMask,
			MemoryBarrierCount, pMemoryBarriers, BufferMemoryBarrierCount, pBufferMemoryBarriers, ImageMemoryBarrierCount, pImageMemoryBarriers);
		VULKANAPINAMESPACE::vkCmdWaitEvents(CommandBuffer, EventCount, Events, SrcStageMask, DstStageMask, MemoryBarrierCount, pMemoryBarriers,
			BufferMemoryBarrierCount, pBufferMemoryBarriers, ImageMemoryBarrierCount, pImageMemoryBarriers);
		FWrapLayer::WaitEvents(VK_SUCCESS, CommandBuffer, EventCount, Events, SrcStageMask, DstStageMask,
			MemoryBarrierCount, pMemoryBarriers, BufferMemoryBarrierCount, pBufferMemoryBarriers, ImageMemoryBarrierCount, pImageMemoryBarriers);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdPipelineBarrier(
		VkCommandBuffer CommandBuffer, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, VkDependencyFlags DependencyFlags,
		uint32 MemoryBarrierCount, const VkMemoryBarrier* MemoryBarriers,
		uint32 BufferMemoryBarrierCount, const VkBufferMemoryBarrier* BufferMemoryBarriers,
		uint32 ImageMemoryBarrierCount, const VkImageMemoryBarrier* ImageMemoryBarriers)
	{
		FWrapLayer::PipelineBarrier(VK_RESULT_MAX_ENUM, CommandBuffer, SrcStageMask, DstStageMask, DependencyFlags, MemoryBarrierCount, MemoryBarriers, BufferMemoryBarrierCount, BufferMemoryBarriers, ImageMemoryBarrierCount, ImageMemoryBarriers);
		VULKANAPINAMESPACE::vkCmdPipelineBarrier(CommandBuffer, SrcStageMask, DstStageMask, DependencyFlags, MemoryBarrierCount, MemoryBarriers, BufferMemoryBarrierCount, BufferMemoryBarriers, ImageMemoryBarrierCount, ImageMemoryBarriers);
		FWrapLayer::PipelineBarrier(VK_SUCCESS, CommandBuffer, SrcStageMask, DstStageMask, DependencyFlags, MemoryBarrierCount, MemoryBarriers, BufferMemoryBarrierCount, BufferMemoryBarriers, ImageMemoryBarrierCount, ImageMemoryBarriers);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBeginQuery(VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 Query, VkQueryControlFlags Flags)
	{
		FWrapLayer::BeginQuery(VK_RESULT_MAX_ENUM, CommandBuffer, QueryPool, Query, Flags);
		VULKANAPINAMESPACE::vkCmdBeginQuery(CommandBuffer, QueryPool, Query, Flags);
		FWrapLayer::BeginQuery(VK_SUCCESS, CommandBuffer, QueryPool, Query, Flags);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdEndQuery(VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 Query)
	{
		FWrapLayer::EndQuery(VK_RESULT_MAX_ENUM, CommandBuffer, QueryPool, Query);
		VULKANAPINAMESPACE::vkCmdEndQuery(CommandBuffer, QueryPool, Query);
		FWrapLayer::EndQuery(VK_SUCCESS, CommandBuffer, QueryPool, Query);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdResetQueryPool(VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 FirstQuery, uint32 QueryCount)
	{
		FWrapLayer::ResetQueryPool(VK_RESULT_MAX_ENUM, CommandBuffer, QueryPool, FirstQuery, QueryCount);
		VULKANAPINAMESPACE::vkCmdResetQueryPool(CommandBuffer, QueryPool, FirstQuery, QueryCount);
		FWrapLayer::ResetQueryPool(VK_SUCCESS, CommandBuffer, QueryPool, FirstQuery, QueryCount);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdWriteTimestamp(VkCommandBuffer CommandBuffer, VkPipelineStageFlagBits PipelineStage, VkQueryPool QueryPool, uint32 Query)
	{
		FWrapLayer::WriteTimestamp(VK_RESULT_MAX_ENUM, CommandBuffer, PipelineStage, QueryPool, Query);
		VULKANAPINAMESPACE::vkCmdWriteTimestamp(CommandBuffer, PipelineStage, QueryPool, Query);
		FWrapLayer::WriteTimestamp(VK_SUCCESS, CommandBuffer, PipelineStage, QueryPool, Query);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyQueryPoolResults(VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 FirstQuery, uint32 QueryCount,
		VkBuffer DstBuffer, VkDeviceSize DstOffset, VkDeviceSize Stride, VkQueryResultFlags Flags)
	{
		FWrapLayer::CopyQueryPoolResults(VK_RESULT_MAX_ENUM, CommandBuffer, QueryPool, FirstQuery, QueryCount, DstBuffer, DstOffset, Stride, Flags);
		VULKANAPINAMESPACE::vkCmdCopyQueryPoolResults(CommandBuffer, QueryPool, FirstQuery, QueryCount, DstBuffer, DstOffset, Stride, Flags);
		FWrapLayer::CopyQueryPoolResults(VK_SUCCESS, CommandBuffer, QueryPool, FirstQuery, QueryCount, DstBuffer, DstOffset, Stride, Flags);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdPushConstants(
		VkCommandBuffer                             commandBuffer,
		VkPipelineLayout                            layout,
		VkShaderStageFlags                          stageFlags,
		uint32                                    offset,
		uint32                                    size,
		const void*                                 pValues);
#endif
	static FORCEINLINE_DEBUGGABLE void  vkCmdBeginRenderPass(
		VkCommandBuffer CommandBuffer,
		const VkRenderPassBeginInfo* RenderPassBegin,
		VkSubpassContents Contents)
	{
		FWrapLayer::BeginRenderPass(VK_RESULT_MAX_ENUM, CommandBuffer, RenderPassBegin, Contents);
		VULKANAPINAMESPACE::vkCmdBeginRenderPass(CommandBuffer, RenderPassBegin, Contents);
		FWrapLayer::BeginRenderPass(VK_SUCCESS, CommandBuffer, RenderPassBegin, Contents);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdNextSubpass(VkCommandBuffer CommandBuffer, VkSubpassContents Contents)
	{
		FWrapLayer::NextSubpass(VK_RESULT_MAX_ENUM, CommandBuffer, Contents);
		VULKANAPINAMESPACE::vkCmdNextSubpass(CommandBuffer, Contents);
		FWrapLayer::NextSubpass(VK_SUCCESS, CommandBuffer, Contents);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdEndRenderPass(VkCommandBuffer CommandBuffer)
	{
		FWrapLayer::EndRenderPass(VK_RESULT_MAX_ENUM, CommandBuffer);
		VULKANAPINAMESPACE::vkCmdEndRenderPass(CommandBuffer);
		FWrapLayer::EndRenderPass(VK_SUCCESS, CommandBuffer);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdExecuteCommands(
		VkCommandBuffer                             commandBuffer,
		uint32                                    commandBufferCount,
		const VkCommandBuffer*                      pCommandBuffers);
#endif

	static FORCEINLINE_DEBUGGABLE VkResult vkCreateSwapchainKHR(VkDevice Device, const VkSwapchainCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSwapchainKHR* Swapchain)
	{
		FWrapLayer::CreateSwapchainKHR(VK_RESULT_MAX_ENUM, Device, CreateInfo, Swapchain);
		VkResult Result = VULKANAPINAMESPACE::vkCreateSwapchainKHR(Device, CreateInfo, Allocator, Swapchain);
		FWrapLayer::CreateSwapchainKHR(Result, Device, CreateInfo, Swapchain);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void vkDestroySwapchainKHR(VkDevice Device, VkSwapchainKHR Swapchain, const VkAllocationCallbacks* Allocator)
	{
		FWrapLayer::DestroySwapchainKHR(VK_RESULT_MAX_ENUM, Device, Swapchain);
		VULKANAPINAMESPACE::vkDestroySwapchainKHR(Device, Swapchain, Allocator);
		FWrapLayer::DestroySwapchainKHR(VK_SUCCESS, Device, Swapchain);
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetSwapchainImagesKHR(VkDevice Device, VkSwapchainKHR Swapchain, uint32_t* SwapchainImageCount, VkImage* SwapchainImages)
	{
		FWrapLayer::GetSwapChainImagesKHR(VK_RESULT_MAX_ENUM, Device, Swapchain, SwapchainImageCount, SwapchainImages);
		VkResult Result = VULKANAPINAMESPACE::vkGetSwapchainImagesKHR(Device, Swapchain, SwapchainImageCount, SwapchainImages);
		FWrapLayer::GetSwapChainImagesKHR(Result, Device, Swapchain, SwapchainImageCount, SwapchainImages);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkAcquireNextImageKHR(VkDevice Device, VkSwapchainKHR Swapchain, uint64_t Timeout, VkSemaphore Semaphore, VkFence Fence, uint32_t* ImageIndex)
	{
		FWrapLayer::AcquireNextImageKHR(VK_RESULT_MAX_ENUM, Device, Swapchain, Timeout, Semaphore, Fence, ImageIndex);
		VkResult Result = VULKANAPINAMESPACE::vkAcquireNextImageKHR(Device, Swapchain, Timeout, Semaphore, Fence, ImageIndex);
		FWrapLayer::AcquireNextImageKHR(Result, Device, Swapchain, Timeout, Semaphore, Fence, ImageIndex);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkQueuePresentKHR(VkQueue Queue, const VkPresentInfoKHR* PresentInfo)
	{
		FWrapLayer::QueuePresent(VK_RESULT_MAX_ENUM, Queue, PresentInfo);
		VkResult Result = VULKANAPINAMESPACE::vkQueuePresentKHR(Queue, PresentInfo);
		FWrapLayer::QueuePresent(Result, Queue, PresentInfo);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkSurfaceCapabilitiesKHR* SurfaceCapabilities)
	{
		FWrapLayer::GetPhysicalDeviceSurfaceCapabilitiesKHR(VK_RESULT_MAX_ENUM, PhysicalDevice, Surface, SurfaceCapabilities);
		VkResult Result = VULKANAPINAMESPACE::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, SurfaceCapabilities);
		FWrapLayer::GetPhysicalDeviceSurfaceCapabilitiesKHR(VK_SUCCESS, PhysicalDevice, Surface, SurfaceCapabilities);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, uint32_t* SurfaceFormatCountPtr, VkSurfaceFormatKHR* SurfaceFormats)
	{
		FWrapLayer::GetPhysicalDeviceSurfaceFormatsKHR(VK_RESULT_MAX_ENUM, PhysicalDevice, Surface, SurfaceFormatCountPtr, SurfaceFormats);
		VkResult Result = VULKANAPINAMESPACE::vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, SurfaceFormatCountPtr, SurfaceFormats);
		FWrapLayer::GetPhysicalDeviceSurfaceFormatsKHR(VK_SUCCESS, PhysicalDevice, Surface, SurfaceFormatCountPtr, SurfaceFormats);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice PhysicalDevice, uint32_t QueueFamilyIndex,	VkSurfaceKHR Surface, VkBool32* SupportedPtr)
	{
		FWrapLayer::GetPhysicalDeviceSurfaceSupportKHR(VK_RESULT_MAX_ENUM, PhysicalDevice, QueueFamilyIndex, Surface, SupportedPtr);
		VkResult Result = VULKANAPINAMESPACE::vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, QueueFamilyIndex, Surface, SupportedPtr);
		FWrapLayer::GetPhysicalDeviceSurfaceSupportKHR(VK_SUCCESS, PhysicalDevice, QueueFamilyIndex, Surface, SupportedPtr);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, uint32_t* PresentModeCountPtr, VkPresentModeKHR* PresentModesPtr)
	{
		FWrapLayer::GetPhysicalDeviceSurfacePresentModesKHR(VK_RESULT_MAX_ENUM, PhysicalDevice, Surface, PresentModeCountPtr, PresentModesPtr);
		VkResult Result = VULKANAPINAMESPACE::vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, PresentModeCountPtr, PresentModesPtr);
		FWrapLayer::GetPhysicalDeviceSurfacePresentModesKHR(VK_SUCCESS, PhysicalDevice, Surface, PresentModeCountPtr, PresentModesPtr);
		return Result;
	}

#if VULKAN_USE_CREATE_WIN32_SURFACE
	FORCEINLINE_DEBUGGABLE VkResult vkCreateWin32SurfaceKHR(VkInstance Instance, const VkWin32SurfaceCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSurfaceKHR* Surface)
	{
		FWrapLayer::CreateWin32SurfaceKHR(VK_RESULT_MAX_ENUM, Instance, CreateInfo, Surface);
		VkResult Result = VULKANAPINAMESPACE::vkCreateWin32SurfaceKHR(Instance, CreateInfo, Allocator, Surface);
		FWrapLayer::CreateWin32SurfaceKHR(Result, Instance, CreateInfo, Surface);
		return Result;
	}
#endif

#if VULKAN_USE_CREATE_ANDROID_SURFACE
	static FORCEINLINE_DEBUGGABLE VkResult vkCreateAndroidSurfaceKHR(VkInstance Instance, const VkAndroidSurfaceCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSurfaceKHR* Surface)
	{
		FWrapLayer::CreateAndroidSurfaceKHR(VK_RESULT_MAX_ENUM, Instance, CreateInfo, Surface);
		VkResult Result = VULKANAPINAMESPACE::vkCreateAndroidSurfaceKHR(Instance, CreateInfo, Allocator, Surface);
		FWrapLayer::CreateAndroidSurfaceKHR(Result, Instance, CreateInfo, Surface);
		return Result;
	}
#endif

	static FORCEINLINE_DEBUGGABLE void vkDestroySurfaceKHR(VkInstance Instance, VkSurfaceKHR Surface, const VkAllocationCallbacks* pAllocator)
	{
		FWrapLayer::DestroySurfaceKHR(VK_RESULT_MAX_ENUM, Instance, Surface);
		VULKANAPINAMESPACE::vkDestroySurfaceKHR(Instance, Surface, pAllocator);
		FWrapLayer::DestroySurfaceKHR(VK_SUCCESS, Instance, Surface);
	}

#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
	static FORCEINLINE_DEBUGGABLE void vkGetImageMemoryRequirements2KHR(VkDevice Device, const VkImageMemoryRequirementsInfo2KHR* Info, VkMemoryRequirements2KHR* MemoryRequirements)
	{
		FWrapLayer::GetImageMemoryRequirements2KHR(VK_RESULT_MAX_ENUM, Device, Info, MemoryRequirements);
		VULKANAPINAMESPACE::vkGetImageMemoryRequirements2KHR(Device, Info, MemoryRequirements);
		FWrapLayer::GetImageMemoryRequirements2KHR(VK_SUCCESS, Device, Info, MemoryRequirements);
	}
#endif

#if VULKAN_ENABLE_IMAGE_TRACKING_LAYER
	void BindDebugLabelName(VkImage Image, const TCHAR* Name);
#endif

#if VULKAN_ENABLE_DUMP_LAYER
	void DumpLayerPushMarker(const TCHAR* Name);
	void DumpLayerPopMarker();
	void PrintfBegin(const FString& String);
#endif
}

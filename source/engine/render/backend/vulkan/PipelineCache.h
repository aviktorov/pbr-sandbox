#pragma once

#include <unordered_map>
#include <volk.h>

namespace render::backend::vulkan
{
	class PipelineLayoutCache;
	class Device;
	class Context;
	struct RenderPrimitive;

	/*
	 */
	class PipelineCache
	{
	public:
		PipelineCache(const Device *device, PipelineLayoutCache *layout_cache)
			: device(device), layout_cache(layout_cache) { }
		~PipelineCache();

		VkPipeline fetch(const Context *context, const RenderPrimitive *primitive);
		void clear();

	private:
		uint64_t getHash(VkPipelineLayout layout, const Context *context, const RenderPrimitive *primitive) const;

	private:
		const Device *device {nullptr};
		PipelineLayoutCache *layout_cache {nullptr};

		std::unordered_map<uint64_t, VkPipeline> cache;
	};
}
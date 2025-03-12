//#ifndef VKBUFFER_HPP_3517C9FB_83A0_42F4_BC81_15F390CB83E0
//#define VKBUFFER_HPP_3517C9FB_83A0_42F4_BC81_15F390CB83E0
//// SOLUTION_TAGS: vulkan-(ex-[^123]|cw-.)
//
//#include <volk/volk.h>
//#include <vk_mem_alloc.h>
//
//#include <utility>
//
//#include <cassert>
//
//#include "allocator.hpp"
//
//namespace labutils
//{
//	class Buffer
//	{
//		public:
//			Buffer() noexcept, ~Buffer();
//
//			explicit Buffer( VmaAllocator, VkBuffer = VK_NULL_HANDLE, VmaAllocation = VK_NULL_HANDLE ) noexcept;
//
//			Buffer( Buffer const& ) = delete;
//			Buffer& operator= (Buffer const&) = delete;
//
//			Buffer( Buffer&& ) noexcept;
//			Buffer& operator = (Buffer&&) noexcept;
//
//		public:
//			VkBuffer buffer = VK_NULL_HANDLE;
//			VmaAllocation allocation = VK_NULL_HANDLE;
//
//		private:
//			VmaAllocator mAllocator = VK_NULL_HANDLE;
//	};
//
//	Buffer create_buffer( Allocator const&, VkDeviceSize, VkBufferUsageFlags, VmaAllocationCreateFlags, VmaMemoryUsage = VMA_MEMORY_USAGE_AUTO );
//}
//
//#endif // VKBUFFER_HPP_3517C9FB_83A0_42F4_BC81_15F390CB83E0

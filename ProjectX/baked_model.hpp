#ifndef BAKED_MODEL_HPP_7D7BFF3A_1743_43DF_8D4F_D67D80FD8282
#define BAKED_MODEL_HPP_7D7BFF3A_1743_43DF_8D4F_D67D80FD8282

#include <string>
#include <vector>

#include <cstdint>
#include <volk/volk.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <volk/volk.h>
#include <array>

/* Baked file format:
 *
 *  1. Header:
 *    - 16*char: file magic = "\0\0COMP5892Mmesh"
 *    - 16*char: variant = "default" (changes later)
 *
 *  2. Textures
 *    - 1*uint32_t: U = number of (unique) textures
 *    - repeat U times:
 *      - string: path to texture
 *      - 1*uint8_t: texture color space (see ETextureSpace)
 *      - 1*uint8_t: number of channels in texture
 *
 *  3. Material information
 *    - 1*uint32_t: M = number of materials
 *    - repeat M times:
 *      - uint32_t: base color texture index
 *      - uint32_t: roughness texture index
 *      - uint32_t: metalness texture index
 *      - uint32_t: alpha mask texture index; set to 0xffffffff if not available
 *      - uint32_t: normal map texture index; set to 0xffffffff if not available
 *      - uint32_t: emissive texture index;
 *
 *  4. Mesh data
 *    - 1*uint32_t: M = number of meshes
 *    - repeat M times:
 *      - uint32_t : material index
 *      - uint32_t : V = number of vertices
 *      - uint32_t : I = number of indices
 *      - repeat V times: vec3 position
 *      - repeat V times: vec3 normal
 *      - repeat V times: vec2 texture coordinate
 *      - repeat I times: uint32_t index
 *
 * Strings are stored as
 *   - 1*uint32_t: N = length of string in chars, including terminating \0
 *   - repeat N times: char in string
 *
 * See cw2-bake/main.cpp (specifically write_model_data_()) for additional
 * information.
 *
 *
 * My suggestion for loading the data into Vulkan is as follows:
 *
 * - Create and load textures. This gives a list of Images (which includes a
 *   VkImage + VmaAllocation) and VkImageViews. We only need to keep these
 *   around -- place them in a vector.
 *
 * - Create a Descriptor Set Layout for material information only. Initially,
 *   this would include three textures (base color, metalness, roughness).
 *
 * - Create a Descriptor Set for each material. You can easily get the
 *   VkImageViews from the list in the first step by the index in the
 *   BaseMaterialInfo. This also avoids loading duplicates of textures if they
 *   are reused across multiple materials.
 *
 * - Upload mesh data. In my reference solution, I created separate VkBuffers
 *   for each mesh (one for each attribute and one for the indices).
 */

#include "Image.hpp"
#include "Buffer.hpp"
#include <glm/glm.hpp>

namespace vk
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec2 tex;
		glm::vec3 normal;
		std::array<uint8_t, 3> quaternion;

		static VkVertexInputBindingDescription GetBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescrip{};
			bindingDescrip.binding = 0;
			bindingDescrip.stride = sizeof(Vertex);
			bindingDescrip.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescrip;
		}

		static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 4> attributes = {};

			attributes[0].binding = 0;
			attributes[0].location = 0;
			attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributes[0].offset = offsetof(Vertex, pos);

			attributes[1].binding = 0;
			attributes[1].location = 1;
			attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
			attributes[1].offset = offsetof(Vertex, tex);

			attributes[2].binding = 0;
			attributes[2].location = 2;
			attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributes[2].offset = offsetof(Vertex, normal);

			attributes[3].binding = 0;
			attributes[3].location = 3;
			attributes[3].format = VK_FORMAT_R8G8B8_UINT;
			attributes[3].offset = offsetof(Vertex, quaternion);

			return attributes;
		}

		bool operator==(const Vertex& other) const
		{
			return pos == other.pos && tex == other.tex && normal == other.normal;
		}

	};
}


enum class ETextureSpace : std::uint8_t
{
	unorm = 0,
	srgb = 1
};

struct BakedTextureInfo
{
	std::string path;
	ETextureSpace space;
	std::uint8_t channels;
};

struct BakedMaterialInfo
{
	std::uint32_t baseColorTextureId;
	std::uint32_t roughnessTextureId;
	std::uint32_t metalnessTextureId;
	std::uint32_t alphaMaskTextureId; // May be set to 0xffffffff if no alpha mask
	std::uint32_t normalMapTextureId; // May be set to 0xffffffff if no normal map
	std::uint32_t emissiveTextureId; // May be set to 0xffffffff if no emissive map

	// The emissive map can be ignored in Assignment 1.2. It is only required 
	// in parts of Assignment 2.2.
};

struct BakedMeshData
{
	std::uint32_t materialId; // Material index to get texture for this mesh 
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> texcoords;
	std::vector<glm::vec3> normals;
	std::vector<std::uint32_t> indices;
	std::vector<std::array<uint8_t, 3>> compressedTBN;

	std::vector<vk::Vertex> vertexData;

	vk::Buffer vertexBuffer;
	vk::Buffer indexBuffer;
};

//model.materials[mesh.materialID].baseColortextureid
struct BakedModel
{
	std::vector<BakedTextureInfo> textures;
	std::vector<BakedMaterialInfo> materials; // Each material had an texture ID into textures array
	std::vector<BakedMeshData> meshes;
	std::vector<vk::Image> loadedTextures;
};

BakedModel load_baked_model( char const* aModelPath );
#endif // BAKED_MODEL_HPP_7D7BFF3A_1743_43DF_8D4F_D67D80FD8282


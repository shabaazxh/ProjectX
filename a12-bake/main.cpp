#include <iterator>
#include <vector>
#include <typeinfo>
#include <exception>
#include <filesystem>
#include <system_error>
#include <unordered_map>

#include <cstdio>
#include <cstring>

#include <tgen.h>
#include <glm/glm.hpp>

#include "index_mesh.hpp"
#include "input_model.hpp"
#include "load_model_obj.hpp"

#include "../labutils/error.hpp"
namespace lut = labutils;
#include <iostream>
#include <glm/gtc/quaternion.hpp>

namespace
{
	// constants
	/* File "magic". The first 16 bytes of our custom file are equal to this
	 * magic value. This allows us to check whether a certain file is
	 * (probably) of the right type. Having a file magic is relatively common
	 * practice -- you can find a list of such magic sequences e.g. here:
     * https://en.wikipedia.org/wiki/List_of_file_signatures
	 *
	 * When picking a signature there are a few considerations. For example,
	 * including non-printable characters (e.g. the \0) early keeps the file
	 * from being misidentified as text.
	 */
	 constexpr char kFileMagic[16] = "\0\0COMP5892Mmesh";

	/* Note: change the file variant if you change the file format! 
	 *
	 * Suggestion: use 'uid-tag'. For example, I would use "scsmbil-tan" to
	 * indicate that this is a custom format by myself (=scsmbil) with
	 * additional tangent space information.
	 */
	constexpr char kFileVariant[16] = "default-a12";

	/* Fallback texture for RGBA 1111 and Grayscale 1
	 */
	constexpr char kTextureFallbackR1[] = "assets-src/a12/r1.png";
	constexpr char kTextureFallbackRGBA1111[] = "assets-src/a12/rgba1111.png";
	constexpr char kTextureFallbackRGB000[] = "assets-src/a12/rgb000.png";

	// types
	struct TextureInfo_
	{
		std::uint32_t uniqueId;
		std::uint8_t space;
		std::uint8_t channels;
		std::string newPath;
	};

	// local functions:
	void process_model_(
		char const* aOutput,
		char const* aInputOBJ,
		glm::mat4x4 const& aStaticTransform = glm::mat4x4( 1.f ) //TODO
	);


	InputModel normalize_( InputModel );


	void write_model_data_(
		FILE*,
		InputModel const&,
		std::vector<IndexedMesh> const&,
		std::unordered_map<std::string,TextureInfo_> const&
	);

	std::vector<IndexedMesh> index_meshes_(
		InputModel const&,
		float aErrorTolerance = 1e-5f
	);

	std::unordered_map<std::string,TextureInfo_> find_unique_textures_(
		InputModel const&
	);

	std::unordered_map<std::string,TextureInfo_> new_paths_(
		std::unordered_map<std::string,TextureInfo_>,
		std::filesystem::path const& aTexDir
	);

}


int main() try
{
#	if !defined(NDEBUG)
	std::printf( "Suggest running this in release mode (it appears to be running in debug)\n" );
	std::printf( "Especially under VisualStudio/MSVC, the debug build seems very slow.\n" );
	/* A few notes:
	 * 
	 * I have not profiled this at all. The following are based on previous
	 * experience(s).
	 *
	 * - ZStd benefits immensely from compiler optimizations.
	 * 
	 * - Under MSVC, std::unordered_set performs quite badly in debug mode. This
	 *   may be further related to other debug-related options (e.g., extended
	 *   iterator checking...).
	 * 
	 *   Normally, I avoid unordered_set here, and instead rely on one of the many
	 *   high quality flat_set implementations. They tend to be faster from the 
	 *   get go and perform more equally under different compilers. 
	 * 
	 * - NDEBUG is the standard macro to control the behaviour of assert(). When 
	 *   NDEBUG is defined, assert() will "do nothing" (they're expanded to an 
	 *   empty statement). This is typically desirable in a release build, but not
	 *   necessary or guaranteed. (Indeed, the premake sets NDEBUG explicitly for
	 *   this project -- this is why the check above works. But don't rely on this
	 *   blindly.)
	 * 
	 * - The VisualStudio interactive debugger's heap profiler (the thing that 
	 *   shows you the memory usage graph) carries a measurable overhead as well.
	 *
	 * The binary .comp5892mesh should be unchanged between debug and release
	 * builds, so you can safely use the release build to create the file once,
	 * even while debugging the main A12 program.
	 */
#	endif
	process_model_(
		"assets/suntemple.comp5892mesh",
		"assets-src/a12/suntemple.obj-zstd"
	);

	return 0;
}
catch( std::exception const& eErr )
{
	std::fprintf( stderr, "Top-level exception [%s]:\n%s\nBye.\n", typeid(eErr).name(), eErr.what() );
	return 1;
}

namespace
{
	void process_model_( char const* aOutput, char const* aInputOBJ, glm::mat4x4 const& aStaticTransform )
	{
		static constexpr std::size_t vertexSize = sizeof(float)*(3+3+2);

		// Figure out output paths
		std::filesystem::path const outname( aOutput );
		std::filesystem::path const rootdir = outname.parent_path();
		std::filesystem::path const basename = outname.stem();
		std::filesystem::path const texdir = basename.string() + "-tex";

		// Load input model
		auto const model = normalize_( load_compressed_wavefront_obj( aInputOBJ ) );

		// This is now what it's read in from the obj model
		std::size_t inputVerts = 0;
		for( auto const& imesh : model.meshes )
			inputVerts += imesh.vertexCount;

		std::printf( "%s: %zu meshes, %zu materials\n", aInputOBJ, model.meshes.size(), model.materials.size() );
		std::printf( " - triangle soup vertices: %zu => %zu kB\n", inputVerts, inputVerts*vertexSize/1024 );

		// Index meshes
		auto const indexed = index_meshes_( model );

		std::size_t outputVerts = 0, outputIndices = 0;
		for( auto const& mesh : indexed )
		{
			outputVerts += mesh.vert.size();
			outputIndices += mesh.indices.size();
		}

		std::printf( " - indexed vertices: %zu with %zu indices => %zu kB\n", outputVerts, outputIndices, (outputVerts*vertexSize + outputIndices*sizeof(std::uint32_t))/1024 );

		// Find list of unique textures
		auto const textures = new_paths_( find_unique_textures_( model ), texdir );

		std::printf( " - unique textures: %zu\n", textures.size() );

		// Ensure output directory exists
		std::filesystem::create_directories( rootdir );

		// Output mesh data
		auto mainpath = rootdir / basename;
		mainpath.replace_extension( "comp5892mesh_new_packed" );

		FILE* fof = std::fopen( mainpath.string().c_str(), "wb" );
		if( !fof )
			throw lut::Error( "Unable to open '%s' for writing", mainpath.string().c_str() );

		try
		{
			write_model_data_( fof, model, indexed, textures );
		}
		catch( ... )
		{
			std::fclose( fof );
			throw;
		}

		std::fclose( fof );

		// Copy textures
		std::filesystem::create_directories( rootdir / texdir );

		std::size_t errors = 0;
		for( auto const& entry : textures )
		{
			auto const dest = rootdir / entry.second.newPath;

			std::error_code ec;
			bool ret = std::filesystem::copy_file( 
				entry.first,
				dest,
				std::filesystem::copy_options::none,
				ec
			);

			if( !ret )
			{
				++errors;
				std::fprintf( stderr, "copy_file(): '%s' failed: %s (%s)\n", dest.string().c_str(), ec.message().c_str(), ec.category().name() );
			}
		}

		auto const total = textures.size();
		std::printf( "Copied %zu textures out of %zu.\n", total-errors, total );
		if( errors )
		{
			std::fprintf( stderr, "Some copies reported an error. Currently, the code will never overwrite existing files. The errors likely just indicate that the file was copied previously. Remove old files manually, if necessary.\n" );
		}
	}
}

namespace
{
	InputModel normalize_( InputModel aModel )
	{
		for( auto& mat : aModel.materials )
		{
			if( mat.baseColorTexturePath.empty() )
				mat.baseColorTexturePath = kTextureFallbackRGBA1111;
			if( mat.roughnessTexturePath.empty() )
				mat.roughnessTexturePath = kTextureFallbackR1;
			if( mat.metalnessTexturePath.empty() )
				mat.metalnessTexturePath = kTextureFallbackR1;
			if( mat.emissiveTexturePath.empty() )
				mat.emissiveTexturePath = kTextureFallbackRGB000;
		}

		return aModel; // This should use the move constructor implicitly.
	}
}

namespace
{
	void checked_write_( FILE* aOut, std::size_t aBytes, void const* aData )
	{
		auto const ret = std::fwrite( aData, 1, aBytes, aOut );

		if( ret != aBytes )
			throw lut::Error( "fwrite() failed: %zu instead of %zu", ret, aBytes );
	}

	void write_string_( FILE* aOut, char const* aString )
	{
		// Write a string
		// Format:
		//  - uint32_t : N = length of string in bytes, including terminating '\0'
		//  - N x char : string
		std::uint32_t const length = std::uint32_t(std::strlen(aString)+1);
		checked_write_( aOut, sizeof(std::uint32_t), &length );

		checked_write_( aOut, length, aString );
	}

	void write_model_data_( FILE* aOut, InputModel const& aModel, std::vector<IndexedMesh> const& aIndexedMeshes, std::unordered_map<std::string,TextureInfo_> const& aTextures )
	{
		// Write header
		// Format:
		//   - char[16] : file magic
		//   - char[16] : file variant ID
		checked_write_( aOut, sizeof(char)*16, kFileMagic );
		checked_write_( aOut, sizeof(char)*16, kFileVariant );
		
		// Write list of unique textures
		// Format:
		//  - unit32_t : U = number of unique textures
		//  - repeat U times:
		//    - string : path to texture 
		//    - uint8_t : texture color space (0 = unorm, 1 = srgb)
		//    - uint8_t : number of channels in texture
		std::vector<TextureInfo_ const*> orderedUnqiue( aTextures.size() );
		for( auto const& tex : aTextures )
		{
			assert( !orderedUnqiue[tex.second.uniqueId] );
			orderedUnqiue[tex.second.uniqueId] = &tex.second;
		}

		std::uint32_t const textureCount = std::uint32_t(orderedUnqiue.size());
		checked_write_( aOut, sizeof(textureCount), &textureCount );

		for( auto const& tex : orderedUnqiue )
		{
			assert( tex );
			write_string_( aOut, tex->newPath.c_str() );

			std::uint8_t space = tex->space;
			checked_write_( aOut, sizeof(space), &space );

			std::uint8_t channels = tex->channels;
			checked_write_( aOut, sizeof(channels), &channels );
		}

		// Write material information
		// Format:
		//  - uint32_t : M = number of materials
		//  - repeat M times:
		//    - uin32_t : base color texture index
		//    - uin32_t : roughness texture index
		//    - uin32_t : metalness texture index
		//    - uin32_t : alphaMask texture index (or 0xffffffff if none)
		//    - uin32_t : normalMap texture index (or 0xffffffff if none)
		//    - uin32_t : emissive texture index
		std::uint32_t const materialCount = std::uint32_t(aModel.materials.size());
		checked_write_( aOut, sizeof(materialCount), &materialCount );

		for( auto const& mat : aModel.materials )
		{
			auto const write_tex_ = [&] (std::string const& aTexturePath ) {
				if( aTexturePath.empty() )
				{
					static constexpr std::uint32_t sentinel = ~std::uint32_t(0);
					checked_write_( aOut, sizeof(std::uint32_t), &sentinel );
					return;
				}

				auto const it = aTextures.find( aTexturePath );
				assert( aTextures.end() != it );

				checked_write_( aOut, sizeof(std::uint32_t), &it->second.uniqueId );
			};

			write_tex_( mat.baseColorTexturePath );
			write_tex_( mat.roughnessTexturePath );
			write_tex_( mat.metalnessTexturePath );
			write_tex_( mat.alphaMaskTexturePath );
			write_tex_( mat.normalMapTexturePath );
			write_tex_( mat.emissiveTexturePath );
		}

		// Write mesh data
		// Format:
		//  - uint32_t : M = number of meshes
		//  - repeat M times:
		//    - uint32_t : material index
		//    - uint32_t : V = number of vertices
		//    - uint32_t : I = number of indices
		//    - repeat V times: vec3 position
		//    - repeat V times: vec3 normal
		//    - repeat V times: vec2 texture coordinate
		//    - repeat I times: uint32_t index

		std::uint32_t const meshCount = std::uint32_t(aModel.meshes.size());
		checked_write_( aOut, sizeof(meshCount), &meshCount );

		assert( aModel.meshes.size() == aIndexedMeshes.size() );
		for( std::size_t i = 0; i < aModel.meshes.size(); ++i )
		{
			auto const& mmesh = aModel.meshes[i];

			std::uint32_t materialIndex = std::uint32_t(mmesh.materialIndex);
			checked_write_( aOut, sizeof(materialIndex), &materialIndex );

			auto const& imesh = aIndexedMeshes[i];

			std::uint32_t vertexCount = std::uint32_t(imesh.vert.size());
			checked_write_( aOut, sizeof(vertexCount), &vertexCount );
			std::uint32_t indexCount = std::uint32_t(imesh.indices.size());
			checked_write_( aOut, sizeof(indexCount), &indexCount );

			checked_write_( aOut, sizeof(glm::vec3)*vertexCount, imesh.vert.data() );
			checked_write_( aOut, sizeof(glm::vec3)*vertexCount, imesh.norm.data() );
			checked_write_( aOut, sizeof(glm::vec2)*vertexCount, imesh.text.data() );
			
			// Output packed quaternion
			checked_write_(aOut, 3 * sizeof(uint8_t) * vertexCount, imesh.quaternions.data());

			// indices
			checked_write_( aOut, sizeof(std::uint32_t)*indexCount, imesh.indices.data() );
		}
	}
}

namespace
{
	uint8_t packFloatTo8Bit(float f)
	{
		float normalized = (f + 1.0f) / 2.0f; // normalize to 0-1
		uint8_t packed = static_cast<uint8_t>(normalized * 255.0f);
		return packed;
	}

	std::vector<IndexedMesh> index_meshes_( InputModel const& aModel, float aErrorTolerance )
	{
		std::vector<IndexedMesh> indexed;

		for( auto const& imesh : aModel.meshes )
		{
			auto const endIndex = imesh.vertexStartIndex + imesh.vertexCount;

			TriangleSoup soup;

			soup.vert.reserve( imesh.vertexCount );
			for( std::size_t i = imesh.vertexStartIndex; i < endIndex; ++i )
				soup.vert.emplace_back( aModel.positions[i] );

			soup.text.reserve( imesh.vertexCount );
			for( std::size_t i = imesh.vertexStartIndex; i < endIndex; ++i )
				soup.text.emplace_back( aModel.texcoords[i] );

			soup.norm.reserve( imesh.vertexCount );
			for( std::size_t i = imesh.vertexStartIndex; i < endIndex; ++i )
				soup.norm.emplace_back( aModel.normals[i] );

			indexed.emplace_back( make_indexed_mesh( soup, aErrorTolerance ) );
		}

		// So now we would have all indexed meshes 
		// so we can loop over the indexed meshes (meshes with indices) and compute tangent 
		// for each mesh

		for (auto& indexedMesh : indexed)
		{
			// Compute tangent 
			std::vector<glm::vec4> bitangents   (indexedMesh.indices.size(), glm::vec4(0.0));
			std::vector<glm::vec4> tangents     (indexedMesh.indices.size(), glm::vec4(0.0));
			std::vector<glm::quat> tbnQuaternion(indexedMesh.indices.size());

			for (size_t i = 0; i < indexedMesh.indices.size(); i += 3)
			{
				glm::vec3& v0 = indexedMesh.vert[indexedMesh.indices[i + 0]];
				glm::vec3& v1 = indexedMesh.vert[indexedMesh.indices[i + 1]];
				glm::vec3& v2 = indexedMesh.vert[indexedMesh.indices[i + 2]];

				glm::vec2& v0_tex = indexedMesh.text[indexedMesh.indices[i + 0]];
				glm::vec2& v1_tex = indexedMesh.text[indexedMesh.indices[i + 1]];
				glm::vec2& v2_tex = indexedMesh.text[indexedMesh.indices[i + 2]];

				glm::vec3 edge1 = v1 - v0;
				glm::vec3 edge2 = v2 - v0;

				glm::vec2 deltaUV1 = v1_tex - v0_tex;
				glm::vec2 deltaUV2 = v2_tex - v0_tex;

				float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

				glm::vec4 tangent = glm::vec4(1.0);
				glm::vec4 bitangent = glm::vec4(1.0);

				tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
				tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
				tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

				bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
				bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
				bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

				// Normalize the tangent and bitangent vectors
				tangent = glm::normalize(tangent);
				bitangent = glm::normalize(bitangent);

				tangents[indexedMesh.indices[i + 0]] += tangent;
				tangents[indexedMesh.indices[i + 1]] += tangent;
				tangents[indexedMesh.indices[i + 2]] += tangent;

				bitangents[indexedMesh.indices[i + 0]] += bitangent;
				bitangents[indexedMesh.indices[i + 1]] += bitangent;
				bitangents[indexedMesh.indices[i + 2]] += bitangent;
			}

			// Pack the TBN
			indexedMesh.quaternions.resize(indexedMesh.vert.size());
			for (size_t i = 0; i < indexedMesh.vert.size(); ++i) {

				glm::vec4  n = glm::vec4(indexedMesh.norm[i], 0.0);
				glm::vec4& t = tangents[i];
				glm::vec4& b = bitangents[i];

				t = glm::normalize(t);
				b = glm::normalize(b);
				n = glm::normalize(n);
				
				// Construct the TBN matrix. 4x4 used to store the quaternion to use glm::quat_cast
				glm::mat4 tbn = glm::mat4(
					t,
					b,
					n,
					glm::vec4(0,0,0,1)
				);

				// quat_cast already returns a normalized quaternion 
				glm::quat quaternion = glm::quat_cast(tbn);

				//// Store the quaternion into 3 components (24-bits), we will reconstruct w in the shader
				float qx = quaternion.x;
				float qy = quaternion.y;
				float qz = quaternion.z;

				//// Convert the floats to 8 bits
				uint8_t xPacked = packFloatTo8Bit(qx);
				uint8_t yPacked = packFloatTo8Bit(qy);
				uint8_t zPacked = packFloatTo8Bit(qz);
				
				indexedMesh.quaternions[i][0] = xPacked;
				indexedMesh.quaternions[i][1] = yPacked;
				indexedMesh.quaternions[i][2] = zPacked;
			}
		}


		return indexed;
	}
}

namespace
{
	std::unordered_map<std::string,TextureInfo_> find_unique_textures_( InputModel const& aModel )
	{
		std::unordered_map<std::string,TextureInfo_> unique;

		std::uint32_t texid = 0;
		auto const add_unique_ = [&] (std::string const& aPath, std::uint8_t aSpace, std::uint8_t aChannels)
		{
			if( aPath.empty() )
				return;

			TextureInfo_ info{};
			info.uniqueId = texid;
			info.space = aSpace;
			info.channels = aChannels;

			auto const [it, isNew] = unique.emplace( std::make_pair(aPath,info) );

			if( isNew )
				++texid;
		};

		for( auto const& mat : aModel.materials )
		{
			add_unique_( mat.baseColorTexturePath, 1, 4 );
			add_unique_( mat.roughnessTexturePath, 0, 1 ); 
			add_unique_( mat.metalnessTexturePath, 0, 1 ); 
			add_unique_( mat.alphaMaskTexturePath, 1, 4 );  // assume == baseColor
			add_unique_( mat.normalMapTexturePath, 0, 3 );  // xyz only
			add_unique_( mat.emissiveTexturePath, 1, 4 ); 
		}

		return unique;
	}

	std::unordered_map<std::string,TextureInfo_> new_paths_( std::unordered_map<std::string,TextureInfo_> aTextures, std::filesystem::path const& aTexDir )
	{
		for( auto& entry : aTextures )
		{
			std::filesystem::path const originalPath( entry.first );
			auto const filename = originalPath.filename();
			auto const newpath = aTexDir / filename;
		
			auto& info = entry.second;
			info.newPath = newpath.string();
		}

		// Note: aTextures is still local to the function, so there is no need
		// to explicitly std::move() it. However, since it is passed in as an
		// argument, NRVO is unlikely to occur.
		return aTextures; 
	}
}




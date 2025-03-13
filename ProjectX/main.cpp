#include <tuple>
#include <chrono>
#include <limits>
#include <vector>
#include <stdexcept>

#include <cstdio>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <volk/volk.h>

#if !defined(GLM_FORCE_RADIANS)
#	define GLM_FORCE_RADIANS
#endif
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "baked_model.hpp"

#include <iostream>

#include "Utils.hpp"
#include "Context.hpp"
#include "Engine.hpp"

int main() try
{
	vk::Engine engine;

	if (!engine.Initialize())
	{
		std::cout << "Failed to initialize engine. " << std::endl;
		return 0;
	}

	engine.Run();

	return 0;
}
catch( std::exception const& eErr )
{
	std::fprintf( stderr, "\n" );
	std::fprintf( stderr, "Error: %s\n", eErr.what() );
	return 1;
}


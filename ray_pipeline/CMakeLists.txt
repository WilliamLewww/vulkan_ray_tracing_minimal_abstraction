cmake_minimum_required (VERSION 3.19)
project(vulkan_ray_tracing_minimal_abstraction)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

file(GLOB SHADERS "src/shaders/*")

add_executable(application src/main.c)
include_directories(application include)
include_directories(application ${Vulkan_INCLUDE_DIRS})
include_directories(application ${GLFW_INCLUDE_DIRS})
target_link_libraries(application ${Vulkan_LIBRARIES})
target_link_libraries(application glfw)
target_link_libraries(application m)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
foreach(SHADER ${SHADERS})
	get_filename_component(SHADER_NAME ${SHADER} NAME)

	add_custom_command(
		OUTPUT shaders/${SHADER_NAME}.spv
		COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} --target-env=vulkan1.2 -o shaders/${SHADER_NAME}.spv
		DEPENDS ${SHADER}
	)
	target_sources(application PRIVATE shaders/${SHADER_NAME}.spv)
endforeach()

add_custom_target(copy_resources
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
)

add_dependencies(application copy_resources)

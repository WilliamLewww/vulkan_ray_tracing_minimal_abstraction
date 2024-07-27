cmake_minimum_required (VERSION 3.21)
project(vulkan_ray_tracing_minimal_abstraction)

find_package(Vulkan REQUIRED)

if(DEFINED VALIDATION_ENABLED)
  add_compile_definitions(VALIDATION_ENABLED=1)
endif()

file(GLOB SHADERS 
  "src/shader.rchit" 
  "src/shader.rgen" 
  "src/shader.rmiss" 
  "src/shader_shadow.rmiss")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_executable(application src/main.cpp)
  add_compile_definitions(PLATFORM_LINUX=1)

  find_package(X11)
  target_link_libraries(triangle X11)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  add_executable(application WIN32 src/main.cpp)
  add_compile_definitions(PLATFORM_WINDOWS=1)
endif()

set_property(TARGET application PROPERTY CXX_STANDARD 20)
include_directories(application include)
include_directories(application ${Vulkan_INCLUDE_DIRS})
target_link_libraries(application ${Vulkan_LIBRARIES})

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
foreach(SHADER ${SHADERS})
	get_filename_component(SHADER_NAME ${SHADER} NAME)

	add_custom_command(
		OUTPUT shaders/${SHADER_NAME}.spv
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} --target-env vulkan1.2 -o
        shaders/${SHADER_NAME}.spv ${SHADER}
		DEPENDS ${SHADER}
	)
	target_sources(application PRIVATE shaders/${SHADER_NAME}.spv)
endforeach()

add_custom_target(copy_resources
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
)

add_dependencies(application copy_resources)

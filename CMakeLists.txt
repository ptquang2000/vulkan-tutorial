cmake_minimum_required (VERSION 3.29)

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

project (HelloTriangle)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package (glfw3 REQUIRED)
find_package (Vulkan REQUIRED)

add_library(VulkanCppModule INTERFACE)
add_library(Vulkan::cppm ALIAS VulkanCppModule)
target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
target_compile_definitions(VulkanCppModule
        INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

add_executable(HelloTriangle main.cpp)

target_link_libraries(HelloTriangle PUBLIC glfw Vulkan::cppm)

add_custom_command(
	TARGET HelloTriangle POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E create_symlink
	${CMAKE_BINARY_DIR}/compile_commands.json
	${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
)

function (add_slang_shader_target TARGET)	
	cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})

	set(SHADER_DIR ${CMAKE_CURRENT_LIST_DIR}/Vulkan-Tutorial/attachments)
	set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)
  	set(ENTRY_POINTS -entry vertMain -entry fragMain)
	set(SHADER_BINARIES "")

  	add_custom_command(
		OUTPUT ${SHADER_BINARY_DIR}
          	COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
		COMMENT "Create directory: ${SHADER_BINARY_DIR}"
  	)

	foreach(SHADER_FILENAME ${SHADER_SOURCES})
		set(SHADER_SOURCE "${SHADER_DIR}/${SHADER_FILENAME}")
		string(REPLACE "slang" "spv" SHADER_BINARY  ${SHADER_FILENAME})

  		add_custom_command(
			OUTPUT  ${SHADER_BINARY_DIR}/${SHADER_BINARY}
			COMMAND slangc ${SHADER_SOURCE} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADER_BINARY}
			WORKING_DIRECTORY ${SHADER_BINARY_DIR}
			DEPENDS ${SHADER_DIR} ${SHADER_SOURCE}
			COMMENT "Compiling Slang Shader: ${SHADER_SOURCE}"
        	  	VERBATIM
		)

		list(APPEND SHADER_BINARIES "${SHADER_BINARY_DIR}/${SHADER_BINARY}")
	endforeach()

	add_custom_target(${TARGET} DEPENDS ${SHADER_BINARIES})
endfunction()

add_slang_shader_target(HelloTriangleShaders SOURCES 22_shader_ubo.slang 27_shader_depth.slang)

function(add_texture_target TARGET)	
	cmake_parse_arguments ("TEXTURE" "" "" "SOURCES" ${ARGN})

	set(TEXTURE_DIR ${CMAKE_CURRENT_LIST_DIR}/Vulkan-Tutorial/images)
	set(TEXTURE_BINARY_DIR ${CMAKE_BINARY_DIR}/textures)
	set(TEXTURE_BINARIES "")

  	add_custom_command(
		OUTPUT ${TEXTURE_BINARY_DIR}
          	COMMAND ${CMAKE_COMMAND} -E make_directory ${TEXTURE_BINARY_DIR}
		COMMENT "Create directory: ${TEXTURE_BINARY_DIR}"
  	)

	foreach(TEXTURE_FILENAME ${TEXTURE_SOURCES})
		set(TEXTURE_SOURCE "${TEXTURE_DIR}/${TEXTURE_FILENAME}")

  		add_custom_command(
			OUTPUT  ${TEXTURE_BINARY_DIR}/${TEXTURE_FILENAME}
			COMMAND ${CMAKE_COMMAND} -E copy ${TEXTURE_SOURCE} ${TEXTURE_BINARY_DIR}
			WORKING_DIRECTORY ${TEXTURE_BINARY_DIR}
			DEPENDS ${TEXTURE_DIR} ${TEXTURE_SOURCE}
			COMMENT "Copy texture: ${TEXTURE_SOURCE}"
        	  	VERBATIM
		)

		list(APPEND TEXTURE_BINARIES "${TEXTURE_BINARY_DIR}/${TEXTURE_FILENAME}")
	endforeach()

	add_custom_target(${TARGET} DEPENDS ${TEXTURE_BINARIES})
endfunction()

add_texture_target(HelloTriangleTextures SOURCES texture.jpg triangle.png)

add_dependencies(HelloTriangle HelloTriangleShaders HelloTriangleTextures)

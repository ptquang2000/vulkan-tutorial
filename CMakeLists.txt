cmake_minimum_required (VERSION 3.24)

project (HelloTriangle)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
	set(ARCH "x86_64")
else()
	set(ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

include(FetchContent)
if (WIN32)
	FetchContent_Declare(glfw
		URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip
	)
	FetchContent_Declare(glm
		GIT_REPOSITORY	https://github.com/g-truc/glm.git
		GIT_TAG 	8d1fd52e5ab5590e2c81768ace50c72bae28f2ed
	)
	message(FATAL_ERROR "Missing fetch content for Vulkan")

	set(CMAKE_C_COMPILER cl)
	set(CMAKE_CXX_COMPILER cl)
elseif(UNIX)
	FetchContent_Declare(vulkan
		URL https://sdk.lunarg.com/sdk/download/1.4.321.1/linux/vulkansdk-linux-x86_64-1.4.321.1.tar.xz
	)
	FetchContent_Declare(glfw
		URL https://mirror.pkgbuild.com/extra/os/x86_64/glfw-1:3.4-1-x86_64.pkg.tar.zst 
	)
	FetchContent_Declare(glm
		URL https://mirror.moson.org/arch/extra/os/x86_64/glm-1.0.3-1-x86_64.pkg.tar.zst
	)
	FetchContent_MakeAvailable(vulkan glfw glm)

	set(ENV{VULKAN_SDK} "${vulkan_SOURCE_DIR}/${ARCH}")
	set(LD_LIBRARY_PATH "$ENV{VULKAN_SDK}/lib")
	set(VK_ADD_LAYER_PATH "$ENV{VULKAN_SDK}/share/vulkan/explicit_layer.d")
	message(STATUS "VK_ADD_LAYER_PATH: ${VK_ADD_LAYER_PATH}")
	add_library(glfw SHARED IMPORTED)
	set_target_properties(glfw PROPERTIES
		IMPORTED_LOCATION ${glfw_SOURCE_DIR}/usr/lib/libglfw.so
		INTERFACE_INCLUDE_DIRECTORIES ${glfw_SOURCE_DIR}/usr/include
	)
	add_library(glm STATIC IMPORTED)
	set_target_properties(glm PROPERTIES
		IMPORTED_LOCATION ${glm_SOURCE_DIR}/usr/lib/libglm.a
		INTERFACE_INCLUDE_DIRECTORIES ${glm_SOURCE_DIR}/usr/include
	)

	set(CMAKE_C_COMPILER clang)
	set(CMAKE_CXX_COMPILER clang++)
else()
	message(FATAL_ERROR "Unsupported platform")
endif()

find_package(Vulkan REQUIRED)
target_compile_definitions(Vulkan::Vulkan
        INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan glfw glm)

add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
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
			COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${LD_LIBRARY_PATH} $ENV{VULKAN_SDK}/bin/slangc ${SHADER_SOURCE} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADER_BINARY}
			WORKING_DIRECTORY ${SHADER_BINARY_DIR}
			DEPENDS ${SHADER_DIR} ${SHADER_SOURCE}
			COMMENT "Compiling Slang Shader: ${SHADER_SOURCE}"
			VERBATIM
		)

		list(APPEND SHADER_BINARIES "${SHADER_BINARY_DIR}/${SHADER_BINARY}")
	endforeach()

	add_custom_target(${TARGET} DEPENDS ${SHADER_BINARIES})
endfunction()

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


function(add_models_target TARGET)	
	cmake_parse_arguments ("MODEL" "" "" "SOURCES" ${ARGN})

	set(MODEL_DIR ${CMAKE_CURRENT_LIST_DIR}/Vulkan-Tutorial/attachments/assets)
	set(MODEL_BINARY_DIR ${CMAKE_BINARY_DIR}/models)
	set(MODEL_BINARIES "")

  	add_custom_command(
		OUTPUT ${MODEL_BINARY_DIR}
          	COMMAND ${CMAKE_COMMAND} -E make_directory ${MODEL_BINARY_DIR}
		COMMENT "Create directory: ${MODEL_BINARY_DIR}"
  	)

	foreach(MODEL_FILENAME ${MODEL_SOURCES})
		set(MODEL_SOURCE "${MODEL_DIR}/${MODEL_FILENAME}")

  		add_custom_command(
			OUTPUT  ${MODEL_BINARY_DIR}/${MODEL_FILENAME}
			COMMAND ${CMAKE_COMMAND} -E copy ${MODEL_SOURCE} ${MODEL_BINARY_DIR}
			WORKING_DIRECTORY ${MODEL_BINARY_DIR}
			DEPENDS ${MODEL_DIR} ${MODEL_SOURCE}
			COMMENT "Copy texture: ${MODEL_SOURCE}"
        	  	VERBATIM
		)

		list(APPEND MODEL_BINARIES "${MODEL_BINARY_DIR}/${MODEL_FILENAME}")
	endforeach()

	add_custom_target(${TARGET} DEPENDS ${MODEL_BINARIES})
endfunction()

add_slang_shader_target(${PROJECT_NAME}Shaders SOURCES 22_shader_ubo.slang 27_shader_depth.slang)
add_texture_target(${PROJECT_NAME}Textures SOURCES texture.jpg triangle.png)
add_models_target(${PROJECT_NAME}Models SOURCES viking_room.obj viking_room.png)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}Shaders ${PROJECT_NAME}Textures ${PROJECT_NAME}Models)

add_custom_target(RUN ALL
	DEPENDS ${PROJECT_NAME}
	COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${LD_LIBRARY_PATH} VK_ADD_LAYER_PATH=${VK_ADD_LAYER_PATH} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}
	COMMENT "Running ${PROJECT_NAME}"
)

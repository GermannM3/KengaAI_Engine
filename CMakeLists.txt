cmake_minimum_required(VERSION 3.20)
project(KengaEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get git hash for version injection
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE KENGA_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT KENGA_GIT_HASH)
    set(KENGA_GIT_HASH "unknown")
endif()

# Release: full optimization, NDEBUG (Vulkan validation layers disabled in VulkanContext)
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# vcpkg: передай -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

# tinygltf и stb — header-only, без Config.cmake
if(NOT DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed")
endif()
find_path(TINYGLTF_INCLUDE_DIR tiny_gltf.h
    HINTS "${VCPKG_INSTALLED_DIR}/x64-windows/include"
    NO_DEFAULT_PATH
)
find_path(STB_INCLUDE_DIR stb_image.h
    HINTS "${VCPKG_INSTALLED_DIR}/x64-windows/include"
    PATH_SUFFIXES stb
    NO_DEFAULT_PATH
)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h
    HINTS "${VCPKG_INSTALLED_DIR}/x64-windows/include"
    NO_DEFAULT_PATH
)
if(NOT TINYGLTF_INCLUDE_DIR)
    message(FATAL_ERROR "tiny_gltf.h not found. Install tinygltf via vcpkg.")
endif()
if(NOT STB_INCLUDE_DIR)
    message(FATAL_ERROR "stb_image.h not found. Install stb via vcpkg.")
endif()
if(NOT MINIAUDIO_INCLUDE_DIR)
    message(FATAL_ERROR "miniaudio.h not found. Install miniaudio via vcpkg.")
endif()

set(CORE_SOURCES
    src/core/Application.cpp
    src/core/Input.cpp
    src/core/InputSystem.cpp
    src/core/LogManager.cpp
    src/core/Profiler.cpp
    src/core/Settings.cpp
    src/core/Time.cpp
)

set(ECS_SOURCES
    src/ecs/Registry.cpp
    src/ecs/SystemManager.cpp
    src/ecs/Systems/CameraSystem.cpp
    src/ecs/Systems/MovementSystem.cpp
    src/ecs/Systems/RotationSystem.cpp
)

set(AUDIO_SOURCES
    src/audio/AudioSystem.cpp
)

set(EDITOR_SOURCES
    src/editor/AssetBrowser.cpp
    src/editor/EditorSystem.cpp
    src/editor/SceneHierarchy.cpp
    src/editor/PropertyEditor.cpp
)

set(ASSET_SOURCES
    src/assets/SkyboxLoader.cpp
)

set(PARTICLE_SOURCES
    src/particles/ParticleSystem.cpp
    src/particles/GpuParticleSystem.cpp
)

set(SERIALIZATION_SOURCES
    src/serialization/SceneSerializer.cpp
    src/serialization/PrefabManager.cpp
)

set(SCRIPTING_SOURCES
    src/scripting/LuaSystem.cpp
)

set(ANIMATION_SOURCES
    src/animation/AnimationSystem.cpp
)

set(GAME_SOURCES
    src/game/GameSystem.cpp
)

set(PHYSICS_SOURCES
    src/physics/PhysicsSystem.cpp
)

set(RENDERING_SOURCES
    src/rendering/FrustumCuller.cpp
    src/rendering/GltfMesh.cpp
    src/rendering/PbrPipeline.cpp
    src/rendering/PostProcess.cpp
    src/rendering/ShadowMap.cpp
    src/rendering/PbrVertex.cpp
    src/rendering/RenderPass.cpp
    src/rendering/Renderer.cpp
    src/rendering/ShaderCompiler.cpp
    src/rendering/Swapchain.cpp
    src/rendering/Texture.cpp
    src/rendering/Vertex.cpp
    src/rendering/VfxPasses.cpp
    src/rendering/VulkanContext.cpp
)

add_executable(KengaEngine src/main.cpp ${CORE_SOURCES} ${ECS_SOURCES} ${AUDIO_SOURCES} ${EDITOR_SOURCES} ${PARTICLE_SOURCES} ${SERIALIZATION_SOURCES} ${SCRIPTING_SOURCES} ${ANIMATION_SOURCES} ${GAME_SOURCES} ${ASSET_SOURCES} ${PHYSICS_SOURCES} ${RENDERING_SOURCES})
target_include_directories(KengaEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${TINYGLTF_INCLUDE_DIR} ${STB_INCLUDE_DIR} ${MINIAUDIO_INCLUDE_DIR} ${BULLET_INCLUDE_DIR} ${LUA_INCLUDE_DIR})
if(MSVC)
    target_compile_options(KengaEngine PRIVATE /FS)
endif()
target_compile_definitions(KengaEngine PRIVATE
    KENGA_VERSION="${PROJECT_VERSION}"
    KENGA_GIT_HASH="${KENGA_GIT_HASH}"
)
target_link_libraries(KengaEngine PRIVATE glfw glm::glm unofficial::shaderc::shaderc spdlog::spdlog Vulkan::Vulkan imgui::imgui nlohmann_json::nlohmann_json ${BULLET_LIBRARIES} ${LUA_LIBRARIES} sol2::sol2)

# Copy shaders and assets next to executable for runtime loading
add_custom_command(TARGET KengaEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:KengaEngine>/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:KengaEngine>/assets
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/simple.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/simple.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/pbr.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/pbr.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/shadow.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/shadow.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/fullscreen.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/bloom_threshold.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/bloom_blur.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/tonemap.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/skybox.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/skybox.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/equirect_to_cube.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/irradiance_convolve.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/prefilter_env.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/brdf_lut.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/god_rays.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/fog.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/ssr.frag
        $<TARGET_FILE_DIR:KengaEngine>/shaders/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/cube.gltf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/cube.bin
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/sky.hdr
        $<TARGET_FILE_DIR:KengaEngine>/assets/
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:KengaEngine>/assets/sounds
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/sounds/step.wav
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/sounds/impact.wav
        $<TARGET_FILE_DIR:KengaEngine>/assets/sounds/
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:KengaEngine>/assets/prefabs
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/prefabs/cube.json
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/prefabs/spark_emitter.json
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/prefabs/point_light.json
        $<TARGET_FILE_DIR:KengaEngine>/assets/prefabs/
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:KengaEngine>/shaders/particles
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/particles/particle.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/shaders/particles/particle.frag
        $<TARGET_FILE_DIR:KengaEngine>/shaders/particles/
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:KengaEngine>/assets/scripts
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts
        $<TARGET_FILE_DIR:KengaEngine>/assets/scripts
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:KengaEngine>/assets/models
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/models/CesiumMan.glb
        $<TARGET_FILE_DIR:KengaEngine>/assets/models/
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:KengaEngine>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/settings.json
        $<TARGET_FILE_DIR:KengaEngine>/config/settings.json
)

# --- Unit tests (RELEASE_PLAN.md M1) ---
find_package(GTest CONFIG REQUIRED)
enable_testing()
add_executable(KengaEngineTests
    tests/test_main.cpp
    tests/test_registry.cpp
    tests/test_physics.cpp
    tests/test_serialization.cpp
    tests/test_scripting.cpp
    tests/stub_physics_deps.cpp
    src/ecs/Registry.cpp
    src/core/LogManager.cpp
    src/physics/PhysicsSystem.cpp
    src/serialization/SceneSerializer.cpp
    src/serialization/PrefabManager.cpp
    src/scripting/LuaSystem.cpp
)
target_include_directories(KengaEngineTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${BULLET_INCLUDE_DIR}
    ${MINIAUDIO_INCLUDE_DIR}
    ${LUA_INCLUDE_DIR}
    ${TINYGLTF_INCLUDE_DIR}
    ${STB_INCLUDE_DIR}
)
target_link_libraries(KengaEngineTests PRIVATE
    GTest::gtest spdlog::spdlog glm::glm nlohmann_json::nlohmann_json
    ${BULLET_LIBRARIES}
    Vulkan::Vulkan
    ${LUA_LIBRARIES}
    sol2::sol2
)
add_test(NAME KengaEngineTests COMMAND KengaEngineTests)

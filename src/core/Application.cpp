/**
 * @file Application.cpp
 * @brief Реализация Application с fixed timestep game loop
 *
 * Gaffer on Games "Fix Your Timestep" pattern.
 */

#include "core/Application.h"
#include "core/Input.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"
#include "core/Time.h"
#include "core/Settings.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include "ecs/SystemManager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "audio/AudioSystem.h"
#include "core/InputSystem.h"
#include "editor/EditorSystem.h"
#include "particles/ParticleSystem.h"
#include "particles/GpuParticleSystem.h"
#include "ecs/Systems/CameraSystem.h"
#include "ecs/Systems/MovementSystem.h"
#include "ecs/Systems/RotationSystem.h"
#include "physics/PhysicsSystem.h"
#include "scripting/LuaSystem.h"
#include "animation/AnimationSystem.h"
#include "rendering/Renderer.h"
#include "rendering/GltfMesh.h"
#include "rendering/VfxPasses.h"
#include "core/Profiler.h"
#include "serialization/SceneSerializer.h"
#include "serialization/PrefabManager.h"
#include "game/GameSystem.h"

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cstddef>

#include <exception>
#include <iterator>
#include <vector>
#include <stdexcept>

namespace kenga {

Application::Application()
{
    init();
}

Application::~Application()
{
    shutdown();
}

BOOL WINAPI Application::signal_handler(DWORD ctrl_type)
{
    switch (ctrl_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        KNG_INFO("Received signal {}, shutting down gracefully...", static_cast<int>(ctrl_type));
        // Note: we can't access the Application instance here safely from a signal handler.
        // The flag is set via a global atomic that the main loop polls.
        return TRUE;
    default:
        return FALSE;
    }
}

void Application::init()
{
    // Register Windows signal handler for graceful shutdown
    SetConsoleCtrlHandler(signal_handler, TRUE);

    // Load runtime configuration
    m_settings.load("config/settings.json");

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    if (m_settings.fullscreen) {
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    }
    m_window = glfwCreateWindow(m_settings.window_width, m_settings.window_height,
                                "Kenga Engine v" KENGA_VERSION " (" KENGA_GIT_HASH ")",
                                m_settings.fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
    if (m_window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSwapInterval(m_settings.vsync ? 1 : 0);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(1280, 720, "Kenga Engine v" KENGA_VERSION " (" KENGA_GIT_HASH ")", nullptr, nullptr);
    if (m_window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    KNG_INFO("Kenga Engine v" KENGA_VERSION " (" KENGA_GIT_HASH ") starting...");

    Input::set_window(m_window);
    // Start with normal cursor (editor mode is on by default)
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetKeyCallback(m_window, [](GLFWwindow* w, int k, int s, int a, int m) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
        if (app) app->on_key(w, k, s, a, m);
    });
    glfwSetCursorPosCallback(m_window, Input::mouse_callback);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* win, int w, int h) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(win));
        if (app) {
            app->on_framebuffer_resize(w, h);
        }
    });

    m_registry = std::make_unique<Registry>();
    m_system_manager = std::make_unique<SystemManager>();
    m_system_manager->register_system(std::make_unique<InputSystem>());
    m_system_manager->register_system(std::make_unique<MovementSystem>());
    m_system_manager->register_system(std::make_unique<RotationSystem>());
    m_system_manager->register_system(std::make_unique<CameraSystem>());

    // Audio system (miniaudio)
    auto audio_uptr = std::make_unique<AudioSystem>();
    audio_uptr->init();
    m_system_manager->register_system(std::move(audio_uptr));
    auto* audio_sys = m_system_manager->get_system<AudioSystem>();

    // Physics system (Bullet3)
    auto physics_uptr = std::make_unique<PhysicsSystem>();
    physics_uptr->init();
    m_system_manager->register_system(std::move(physics_uptr));
    auto* physics_sys = m_system_manager->get_system<PhysicsSystem>();
    if (physics_sys && audio_sys) {
        physics_sys->set_audio_system(audio_sys);
    }

    // Editor system
    m_system_manager->register_system(std::make_unique<EditorSystem>());
    auto* editor_sys = m_system_manager->get_system<EditorSystem>();
    if (physics_sys && editor_sys) {
        physics_sys->set_editor_system(editor_sys);
    }

    // Lua scripting system
    {
        auto lua_uptr = std::make_unique<LuaSystem>();
        lua_uptr->init(*m_registry);
        m_system_manager->register_system(std::move(lua_uptr));
        KNG_INFO("LuaSystem registered");
    }

    // Animation system (skeletal)
    m_system_manager->register_system(std::make_unique<AnimationSystem>());
    KNG_INFO("AnimationSystem registered");

    // Game system (demo: win/lose, HUD)
    m_system_manager->register_system(std::make_unique<GameSystem>());
    auto* game_sys = m_system_manager->get_system<GameSystem>();

    // Particle system
    m_system_manager->register_system(std::make_unique<ParticleSystem>());
    auto* particle_sys = m_system_manager->get_system<ParticleSystem>();
    if (physics_sys && particle_sys) {
        physics_sys->set_particle_system(particle_sys);
    }

    m_camera_entity = m_registry->create_entity();
    m_registry->add_component<Camera>(m_camera_entity,
                                      Camera{glm::mat4(1.0f), glm::mat4(1.0f),
                                             glm::vec3(0.0f, 5.0f, -15.0f),
                                             glm::normalize(glm::vec3(0.0f, -0.3f, 1.0f)),
                                             glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f});
    // Player component for demo FPS
    m_registry->add_component<Player>(m_camera_entity, Player{100.0f, 100.0f, 30, 30});

    // Wire GameSystem and LuaSystem to player (camera) entity
    if (game_sys) game_sys->set_camera_entity(m_camera_entity);
    auto* lua_sys = m_system_manager->get_system<LuaSystem>();
    if (lua_sys) lua_sys->set_player_entity(m_camera_entity);

    // --- Static ground plane (Bullet + ECS) ---
    const Entity plane_entity = m_registry->create_entity();
    m_registry->add_component<Position>(plane_entity, Position{0.0f, -0.1f, 0.0f});
    m_registry->add_component<Scale>(plane_entity, Scale{50.0f, 0.1f, 50.0f});
    {
        auto* ground_shape = new btBoxShape(btVector3(50.0f, 0.1f, 50.0f));
        btTransform ground_tf;
        ground_tf.setIdentity();
        ground_tf.setOrigin(btVector3(0.0f, -0.1f, 0.0f));
        btRigidBody::btRigidBodyConstructionInfo rb_info(
            0.0f, new btDefaultMotionState(ground_tf), ground_shape);
        auto* ground_body = new btRigidBody(rb_info);
        physics_sys->world()->addRigidBody(ground_body);
        m_registry->add_component<RigidBody>(plane_entity,
                                             RigidBody{ground_body, ground_shape, 0.0f, true});
        KNG_INFO("Created static ground plane rigid body");
    }

    // --- Arena walls (4 static boxes around 50x50 floor) ---
    {
        const float hw = 25.0f;
        const float hh = 5.0f;
        const float hd = 0.5f;
        auto add_wall = [&](float px, float py, float pz, float sx, float sy, float sz) {
            const Entity wall = m_registry->create_entity();
            m_registry->add_component<Position>(wall, Position{px, py, pz});
            m_registry->add_component<Rotation>(wall, Rotation{0.0f, 0.0f});
            m_registry->add_component<Orientation>(wall, Orientation{});
            m_registry->add_component<Scale>(wall, Scale{sx, sy, sz});
            auto* shape = new btBoxShape(btVector3(sx * 0.5f, sy * 0.5f, sz * 0.5f));
            btTransform tf;
            tf.setIdentity();
            tf.setOrigin(btVector3(px, py, pz));
            btRigidBody::btRigidBodyConstructionInfo rb_info(
                0.0f, new btDefaultMotionState(tf), shape);
            auto* body = new btRigidBody(rb_info);
            physics_sys->world()->addRigidBody(body);
            m_registry->add_component<RigidBody>(wall, RigidBody{body, shape, 0.0f, true});
        };
        add_wall(0.0f, hh, hw + hd, hw * 2.0f, hh * 2.0f, hd * 2.0f);   // north
        add_wall(0.0f, hh, -hw - hd, hw * 2.0f, hh * 2.0f, hd * 2.0f);  // south
        add_wall(hw + hd, hh, 0.0f, hd * 2.0f, hh * 2.0f, hw * 2.0f);   // east
        add_wall(-hw - hd, hh, 0.0f, hd * 2.0f, hh * 2.0f, hw * 2.0f);  // west
        KNG_INFO("Created 4 arena walls");
    }

    // --- First cube entity (used by renderer as "the cube") ---
    const Entity cube_entity = m_registry->create_entity();
    m_registry->add_component<Position>(cube_entity, Position{0.0f, 8.0f, 0.0f});
    m_registry->add_component<Rotation>(cube_entity, Rotation{0.0f, 0.0f});
    m_registry->add_component<Orientation>(cube_entity, Orientation{});
    m_registry->add_component<Scale>(cube_entity, Scale{1.0f, 1.0f, 1.0f});
    {
        auto* cube_shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
        btTransform cube_tf;
        cube_tf.setIdentity();
        cube_tf.setOrigin(btVector3(0.0f, 8.0f, 0.0f));
        btScalar mass = 1.0f;
        btVector3 inertia(0, 0, 0);
        cube_shape->calculateLocalInertia(mass, inertia);
        btRigidBody::btRigidBodyConstructionInfo rb_info(
            mass, new btDefaultMotionState(cube_tf), cube_shape, inertia);
        auto* body = new btRigidBody(rb_info);
        physics_sys->world()->addRigidBody(body);
        m_registry->add_component<RigidBody>(cube_entity,
                                             RigidBody{body, cube_shape, mass, false});
        KNG_INFO("Created dynamic cube 0 at (0, 8, 0)");
    }

    // --- 4 more dynamic cubes ---
    for (int i = 1; i < 5; ++i) {
        const Entity e = m_registry->create_entity();
        const float x = static_cast<float>(i) * 3.0f - 6.0f;
        const float y = 5.0f + static_cast<float>(i) * 2.0f;
        m_registry->add_component<Position>(e, Position{x, y, 0.0f});
        m_registry->add_component<Rotation>(e, Rotation{0.0f, 0.0f});
        m_registry->add_component<Orientation>(e, Orientation{});
        m_registry->add_component<Scale>(e, Scale{1.0f, 1.0f, 1.0f});

        auto* cube_shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
        btTransform tf;
        tf.setIdentity();
        tf.setOrigin(btVector3(x, y, 0.0f));
        btScalar mass = 1.0f;
        btVector3 inertia(0, 0, 0);
        cube_shape->calculateLocalInertia(mass, inertia);
        btRigidBody::btRigidBodyConstructionInfo rb_info(
            mass, new btDefaultMotionState(tf), cube_shape, inertia);
        auto* body = new btRigidBody(rb_info);
        physics_sys->world()->addRigidBody(body);
        m_registry->add_component<RigidBody>(e,
                                             RigidBody{body, cube_shape, mass, false});
        KNG_INFO("Created dynamic cube {} at ({:.1f}, {:.1f}, 0)", i, x, y);
    }

    Camera& cam = m_registry->get_component<Camera>(m_camera_entity);
    cam.position = glm::vec3(0.0f, 8.0f, -18.0f);
    cam.front = glm::normalize(glm::vec3(0.0f, -0.2f, 1.0f));
    cam.up = glm::vec3(0.0f, 1.0f, 0.0f);
    KNG_INFO("Camera set to ({:.1f}, {:.1f}, {:.1f})",
             cam.position.x, cam.position.y, cam.position.z);

    m_renderer = create_vulkan_renderer();
    m_renderer->init(m_window);
    init_imgui();
    m_renderer->set_scene(m_registry.get(), m_camera_entity, cube_entity, plane_entity);

    // Create light entities
    const Entity dir_light = m_registry->create_entity();
    m_registry->add_component<Light>(dir_light, Light{
        LightType::directional,
        glm::vec3(1.0f, 0.95f, 0.9f), // warm white
        2.5f,                          // higher intensity for visible PBR
        glm::vec3(-0.5f, -1.0f, -0.5f),
        50.0f, 0.9f, 0.8f
    });

    const Entity point_light = m_registry->create_entity();
    m_registry->add_component<Position>(point_light, Position{3.0f, 2.0f, 0.0f});
    m_registry->add_component<Light>(point_light, Light{
        LightType::point,
        glm::vec3(0.2f, 0.5f, 1.0f), // blue
        2.0f,
        glm::vec3(0.0f, -1.0f, 0.0f),
        15.0f, 0.9f, 0.8f
    });

    const Entity spot_light = m_registry->create_entity();
    m_registry->add_component<Position>(spot_light, Position{-3.0f, 4.0f, 2.0f});
    m_registry->add_component<Light>(spot_light, Light{
        LightType::spot,
        glm::vec3(1.0f, 0.3f, 0.1f), // orange
        3.0f,
        glm::vec3(0.3f, -1.0f, -0.2f),
        20.0f,
        0.95f,  // inner cutoff (cos ~18 deg)
        0.85f   // outer cutoff (cos ~32 deg)
    });
    KNG_INFO("Created 3 lights: directional, point, spot");

    auto* input_sys = m_system_manager->get_system<InputSystem>();
    if (input_sys) {
        input_sys->set_camera_entity(m_camera_entity);
        if (audio_sys) {
            input_sys->set_audio_system(audio_sys);
        }
        if (editor_sys) {
            input_sys->set_editor_system(editor_sys);
        }
        if (physics_sys) {
            input_sys->set_physics_system(physics_sys);
        }
        if (particle_sys) {
            input_sys->set_particle_system(particle_sys);
        }
    }

    // Wire GPU particle system (owned by renderer) to physics and input
    auto* gpu_ps = m_renderer->get_gpu_particle_system();
    if (gpu_ps) {
        if (physics_sys) physics_sys->set_gpu_particle_system(gpu_ps);
        if (input_sys) input_sys->set_gpu_particle_system(gpu_ps);
    }

    // Wire GameSystem and restart callback for demo game
    if (input_sys && game_sys) {
        input_sys->set_game_system(game_sys);
        input_sys->set_restart_callback([this]() { restart_demo(); });
    }

    KNG_INFO("Camera input active");

    KNG_INFO("Created camera, ground plane, 5 physics cubes, 3 lights");

    // Scripted cube — patrols left-right via Lua
    {
        const Entity scripted = m_registry->create_entity();
        m_registry->add_component<Position>(scripted, Position{0.0f, 3.0f, -5.0f});
        m_registry->add_component<Scale>(scripted, Scale{1.0f, 1.0f, 1.0f});
        m_registry->add_component<Script>(scripted, Script{"assets/scripts/patrol.lua"});
        KNG_INFO("Created scripted entity {} with patrol.lua", static_cast<unsigned>(scripted));
    }

    // Animated character — CesiumMan with skeletal animation
    {
        auto mesh = std::make_shared<GltfMesh>();
        // Try multiple paths for the model file
        const char* model_paths[] = {
            "assets/models/CesiumMan.glb",
            "Debug/assets/models/CesiumMan.glb",
            "Release/assets/models/CesiumMan.glb",
            "../assets/models/CesiumMan.glb",
            "../../assets/models/CesiumMan.glb",
        };
        bool loaded = false;
        for (const auto* p : model_paths) {
            if (mesh->load_from_file(p)) { loaded = true; break; }
        }
        if (loaded && mesh->has_skeleton) {
            const Entity anim_entity = m_registry->create_entity();
            m_registry->add_component<Position>(anim_entity, Position{3.0f, 0.0f, 0.0f});
            m_registry->add_component<Rotation>(anim_entity, Rotation{0.0f, 0.0f});
            m_registry->add_component<Scale>(anim_entity, Scale{1.0f, 1.0f, 1.0f});

            SkinnedMesh sm;
            sm.mesh = mesh;
            sm.current_clip = 0;
            sm.playing = true;
            sm.loop = true;
            sm.speed = 1.0f;
            m_registry->add_component<SkinnedMesh>(anim_entity, std::move(sm));
            KNG_INFO("Created animated CesiumMan entity {} ({} joints, {} anims)",
                     static_cast<unsigned>(anim_entity),
                     mesh->skeleton.size(), mesh->animations.size());
        } else {
            KNG_WARN("CesiumMan.glb not found or has no skeleton — skipping animated entity");
        }
    }

    // Demo: wave 1 only (next waves spawned when current wave cleared)
    spawn_wave(1);
    m_last_spawned_wave = 1;

    // Stress test: spawn extra cubes with LOD (reduced for demo)
    {
        constexpr int stress_count = 10;
        int spawned = 0;
        for (int i = 0; i < stress_count; ++i) {
            const float x = static_cast<float>((i % 10) - 5) * 3.0f;
            const float z = static_cast<float>((i / 10) - 2) * 3.0f;
            const float y = 8.0f + static_cast<float>(i) * 0.5f; // stagger heights so they fall

            const Entity e = m_registry->create_entity();
            m_registry->add_component<Position>(e, Position{x, y, z});
            m_registry->add_component<Rotation>(e, Rotation{0.0f, 0.0f});
            m_registry->add_component<Scale>(e, Scale{0.5f, 0.5f, 0.5f});
            m_registry->add_component<LodInfo>(e, LodInfo{30.0f, 60.0f, 0, true});

            if (physics_sys) {
                m_registry->add_component<RigidBody>(e, RigidBody{nullptr, nullptr, 1.0f, false});
            }
            ++spawned;
        }
        KNG_INFO("Stress test: spawned {} extra cubes with LOD", spawned);
    }

    // Initialize prefab manager and load default prefabs
    m_prefab_manager = std::make_unique<PrefabManager>();
    m_prefab_manager->load_prefab("Cube", "assets/prefabs/cube.json");
    m_prefab_manager->load_prefab("SparkEmitter", "assets/prefabs/spark_emitter.json");
    m_prefab_manager->load_prefab("PointLight", "assets/prefabs/point_light.json");

    // Wire prefab manager and physics to editor for save/load/spawn
    if (editor_sys) {
        editor_sys->set_prefab_manager(m_prefab_manager.get());
        editor_sys->set_physics_system(physics_sys);
    }

    KNG_INFO("Application initialized: 1280x720");
    KNG_INFO("Vulkan initialized successfully");
}

void Application::restart_demo()
{
    auto* game_sys = m_system_manager->get_system<GameSystem>();
    auto* physics_sys = m_system_manager->get_system<PhysicsSystem>();
    if (!game_sys || !physics_sys) return;

    // Collect and destroy all enemy entities
    std::vector<Entity> to_destroy;
    for (const Entity e : m_registry->view<Enemy>()) {
        to_destroy.push_back(e);
    }
    for (Entity e : to_destroy) {
        physics_sys->remove_rigid_body(*m_registry, e);
        m_registry->destroy_entity(e);
    }

    game_sys->restart(*m_registry);
    m_last_spawned_wave = 0; // Will trigger spawn_wave(1) in update()
    KNG_INFO("Demo restarted");
}

void Application::spawn_wave(int wave_num)
{
    auto* physics_sys = m_system_manager->get_system<PhysicsSystem>();
    if (!physics_sys) return;

    // Arena spawn positions
    static const glm::vec3 spawn_pool[] = {
        {5.0f, 1.0f, 0.0f},   {-5.0f, 1.0f, 5.0f},   {0.0f, 1.0f, 10.0f},
        {8.0f, 1.0f, -5.0f},  {-3.0f, 1.0f, -8.0f},  {6.0f, 1.0f, 6.0f},
        {-7.0f, 1.0f, -4.0f}, {4.0f, 1.0f, -10.0f},  {-4.0f, 1.0f, 2.0f},
        {10.0f, 1.0f, 0.0f},
    };
    constexpr size_t pool_size = std::size(spawn_pool);

    // Progressive difficulty: more enemies, mixed types
    const int base_count = 2 + wave_num;
    const int count = std::min(base_count, 10);
    const size_t offset = (static_cast<size_t>(wave_num - 1) * 3) % pool_size;

    for (int i = 0; i < count; ++i) {
        const glm::vec3 pos = spawn_pool[(offset + static_cast<size_t>(i)) % pool_size];
        const Entity e = m_registry->create_entity();
        m_registry->add_component<Position>(e, Position{pos.x, pos.y, pos.z});
        m_registry->add_component<Rotation>(e, Rotation{0.0f, 0.0f});
        m_registry->add_component<Scale>(e, Scale{0.8f, 0.8f, 0.8f});

        // Mix enemy types based on wave number
        std::string script;
        float hp, attack_range, attack_damage;
        float scale = 0.8f;

        if (wave_num >= 7 && i == 0) {
            // Heavy enemy: tanky, slow, high damage (appears wave 7+)
            script = "assets/scripts/enemy_heavy.lua";
            hp = 120.0f + wave_num * 10.0f;
            attack_range = 3.0f;
            attack_damage = 25.0f;
            scale = 1.2f;
        } else if (wave_num >= 3 && (i % 3 == 0)) {
            // Fast enemy: weak, fast, low damage (appears wave 3+)
            script = "assets/scripts/enemy_fast.lua";
            hp = 25.0f + wave_num * 2.0f;
            attack_range = 2.0f;
            attack_damage = 8.0f;
            scale = 0.6f;
        } else {
            // Normal enemy: balanced
            script = "assets/scripts/enemy_chase.lua";
            hp = 50.0f + wave_num * 5.0f;
            attack_range = 2.5f;
            attack_damage = 15.0f;
        }

        m_registry->add_component<Scale>(e, Scale{scale, scale, scale});
        m_registry->add_component<Enemy>(e, Enemy{hp, hp, attack_range, attack_damage, 0.0f});
        m_registry->add_component<Script>(e, Script{script});
        physics_sys->create_rigid_body(*m_registry, e, 1.0f, false, pos);
    }
    KNG_INFO("Spawned wave {} ({} enemies)", wave_num, count);
}

void Application::shutdown()
{
    // Shutdown audio before physics (audio references may depend on live objects)
    if (m_system_manager) {
        auto* audio_sys = m_system_manager->get_system<AudioSystem>();
        if (audio_sys) {
            audio_sys->shutdown();
        }
    }

    // Shutdown physics before renderer (Bullet objects must be cleaned up)
    if (m_system_manager) {
        auto* physics_sys = m_system_manager->get_system<PhysicsSystem>();
        if (physics_sys) {
            physics_sys->shutdown();
        }
    }

    if (m_renderer) {
        m_renderer->get_vk_device(); // wait idle before imgui cleanup
        vkDeviceWaitIdle(m_renderer->get_vk_device());
        shutdown_imgui();
        m_renderer->shutdown();
        m_renderer.reset();
    }
    m_system_manager.reset();
    m_registry.reset();
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    KNG_INFO("Application shutdown");
}

void Application::run()
{
    constexpr double fixed_dt = Time::fixed_dt();
    double accumulator = 0.0;
    double previous_time = Time::get_time();

    KNG_DEBUG("Entering main loop (fixed_dt = {})", fixed_dt);

    try {
    while (!glfwWindowShouldClose(m_window)) {
        auto& prof = get_profiler();
        prof.begin(Profiler::Section::total_frame);

        const double current_time = Time::get_time();
        const double frame_time = current_time - previous_time;
        previous_time = current_time;

        prof.update_fps(frame_time);

        accumulator += frame_time;

        while (accumulator >= fixed_dt) {
            prof.begin(Profiler::Section::physics);
            fixed_update(fixed_dt);
            prof.end(Profiler::Section::physics);
            accumulator -= fixed_dt;
        }

        Time::set_delta_time(frame_time);
        update(frame_time);

        const double alpha = accumulator / fixed_dt;
        prof.begin(Profiler::Section::imgui);
        render(alpha);
        prof.end(Profiler::Section::imgui);

        glfwPollEvents();
        prof.end(Profiler::Section::total_frame);

        ++m_frame_count;
        m_fps_accumulator += frame_time;

        if (m_fps_accumulator >= 2.0) {
            const double fps = static_cast<double>(m_frame_count) / m_fps_accumulator;
            KNG_INFO("FPS: {:.1f} (frames: {}, elapsed: {:.2f}s)", fps, m_frame_count,
                     m_fps_accumulator);
            m_frame_count = 0;
            m_fps_last_log = current_time;
            m_fps_accumulator = 0.0;
        }
    }
    } catch (const std::exception& e) {
        KNG_CRITICAL("Exception in main loop: {}", e.what());
        shutdown();
        throw;
    }
}

void Application::fixed_update(double dt)
{
    if (m_paused) return;
    m_system_manager->update_fixed(*m_registry, dt);

    // Update GPU particle system (not registered as ISystem, owned by renderer)
    auto* gpu_ps = m_renderer->get_gpu_particle_system();
    if (gpu_ps) {
        gpu_ps->fixed_update(*m_registry, dt);
    }

    ++m_fixed_update_count;
    if (m_fixed_update_count >= 60) {
        m_fixed_update_count = 0;
        int logged = 0;
        for (const Entity e : m_registry->view<Position, Velocity>()) {
            if (logged >= 3) {
                break;
            }
            const auto& pos = m_registry->get_component<Position>(e);
            if (m_registry->has_component<Rotation>(e)) {
                const auto& rot = m_registry->get_component<Rotation>(e);
                KNG_INFO("Entity {} pos=({:.2f}, {:.2f}, {:.2f}) angle={:.2f}",
                         static_cast<unsigned>(e), pos.x, pos.y, pos.z, rot.angle);
            } else {
                KNG_INFO("Entity {} moved to ({:.2f}, {:.2f}, {:.2f})", static_cast<unsigned>(e),
                         pos.x, pos.y, pos.z);
            }
            ++logged;
        }
    }
}

void Application::update(double dt)
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    if (width > 0 && height > 0) {
        auto* cam_sys = m_system_manager->get_system<CameraSystem>();
        if (cam_sys) {
            cam_sys->set_aspect(static_cast<float>(width) / static_cast<float>(height));
        }
    }
    // Determine if fly camera is active (not UI mode, not editor edit mode)
    bool fly_mode = true;
    auto* input_sys = m_system_manager->get_system<InputSystem>();
    auto* editor_sys = m_system_manager->get_system<EditorSystem>();
    if (input_sys && input_sys->get_ui_mode()) {
        fly_mode = false;
    }
    if (editor_sys && editor_sys->is_editor_visible() && !editor_sys->is_play_mode()) {
        fly_mode = false;
    }
    if (m_paused) {
        fly_mode = false;
    }
    Input::update_mouse_delta(fly_mode);
    // Freeze gameplay when paused
    if (!m_paused) {
        m_system_manager->update_variable(*m_registry, dt);

        // Spawn next wave when current wave cleared (Arena One / M2)
        auto* game_sys = m_system_manager->get_system<GameSystem>();
        if (game_sys && game_sys->state() == GameState::playing &&
            game_sys->current_wave() > m_last_spawned_wave &&
            game_sys->current_wave() <= game_sys->waves_total()) {
            spawn_wave(game_sys->current_wave());
            m_last_spawned_wave = game_sys->current_wave();
        }

        // Play victory/defeat sounds
        static GameState last_state = GameState::menu;
        if (game_sys->state() != last_state) {
            auto* audio_sys = m_system_manager->get_system<AudioSystem>();
            if (audio_sys && audio_sys->is_initialized()) {
                if (game_sys->state() == GameState::won) {
                    audio_sys->play_victory();
                } else if (game_sys->state() == GameState::lost) {
                    audio_sys->play_defeat();
                }
            }
            last_state = game_sys->state();
        }
    }
    (void)dt;
}

void Application::render(double alpha)
{
    // Show cursor when paused so user can click Resume / Back to Editor
    if (m_paused && m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    auto* input_sys = m_system_manager->get_system<InputSystem>();
    if (input_sys) {
        m_renderer->set_wireframe(input_sys->get_wireframe());
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto* editor_sys = m_system_manager->get_system<EditorSystem>();
    const bool in_play_mode = editor_sys && editor_sys->is_play_mode();

    // Debug Panel — hidden in Play mode for clean game view
    if (!in_play_mode) {
    ImGui::SetNextWindowPos(ImVec2(250.0f, ImGui::GetIO().DisplaySize.y - 250.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400.0f, 250.0f), ImGuiCond_Once);
    ImGui::Begin("Debug Panel");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    if (m_registry && m_registry->is_valid(m_camera_entity) &&
        m_registry->has_component<Camera>(m_camera_entity)) {
        Camera& cam = m_registry->get_component<Camera>(m_camera_entity);
        ImGui::Text("Camera pos: %.2f %.2f %.2f", cam.position.x, cam.position.y, cam.position.z);
        ImGui::Text("Camera front: %.2f %.2f %.2f", cam.front.x, cam.front.y, cam.front.z);

        if (ImGui::Button("Reset Camera")) {
            cam.position = glm::vec3(0.0f, 5.0f, -15.0f);
            cam.front = glm::normalize(glm::vec3(0.0f, -0.3f, 1.0f));
            KNG_INFO("Camera reset from ImGui");
        }
    }

    static bool wireframe_ui = false;
    if (ImGui::Checkbox("Wireframe", &wireframe_ui)) {
        m_renderer->set_wireframe(wireframe_ui);
    }

    static float light_dir[3] = {-0.5f, -1.0f, -0.5f};
    if (ImGui::SliderFloat3("Light Direction", light_dir, -1.0f, 1.0f)) {
        m_renderer->set_light_direction(glm::vec3(light_dir[0], light_dir[1], light_dir[2]));
    }

    ImGui::Separator();
    static bool skybox_enabled = true;
    if (ImGui::Checkbox("Enable Skybox", &skybox_enabled)) {
        m_renderer->set_skybox_enabled(skybox_enabled);
    }
    static float ibl_intensity = 1.0f;
    if (ImGui::SliderFloat("IBL Intensity", &ibl_intensity, 0.0f, 3.0f)) {
        m_renderer->set_ibl_intensity(ibl_intensity);
    }

    // VFX controls
    ImGui::Separator();
    {
        auto* vfx = m_renderer->get_vfx_settings();
        if (vfx) {
            if (ImGui::CollapsingHeader("VFX Settings")) {
                ImGui::Checkbox("God Rays", &vfx->god_rays_enabled);
                if (vfx->god_rays_enabled) {
                    ImGui::SliderFloat("Rays Density", &vfx->god_rays_density, 0.1f, 3.0f);
                    ImGui::SliderFloat("Rays Weight", &vfx->god_rays_weight, 0.001f, 0.1f);
                    ImGui::SliderFloat("Rays Decay", &vfx->god_rays_decay, 0.9f, 1.0f);
                    ImGui::SliderFloat("Rays Exposure", &vfx->god_rays_exposure, 0.01f, 1.0f);
                    ImGui::SliderInt("Rays Samples", &vfx->god_rays_samples, 10, 100);
                    ImGui::SliderFloat2("Light Pos (screen)", &vfx->light_screen_pos.x, 0.0f, 1.0f);
                }

                ImGui::Checkbox("Volumetric Fog", &vfx->fog_enabled);
                if (vfx->fog_enabled) {
                    ImGui::ColorEdit3("Fog Color", &vfx->fog_color.x);
                    ImGui::SliderFloat("Fog Density", &vfx->fog_density, 0.001f, 0.2f);
                    ImGui::SliderFloat("Fog Start", &vfx->fog_start, 0.0f, 50.0f);
                    ImGui::SliderFloat("Fog End", &vfx->fog_end, 10.0f, 500.0f);
                    ImGui::SliderFloat("Fog Height", &vfx->fog_height, -10.0f, 20.0f);
                    ImGui::SliderFloat("Fog Height Falloff", &vfx->fog_height_falloff, 0.0f, 1.0f);
                }

                ImGui::Checkbox("SSR (expensive)", &vfx->ssr_enabled);
                if (vfx->ssr_enabled) {
                    ImGui::SliderFloat("SSR Step", &vfx->ssr_step_size, 0.1f, 2.0f);
                    ImGui::SliderFloat("SSR Intensity", &vfx->ssr_intensity, 0.0f, 1.0f);
                    ImGui::SliderFloat("SSR Thickness", &vfx->ssr_thickness, 0.1f, 5.0f);
                    ImGui::SliderInt("SSR Steps", &vfx->ssr_max_steps, 8, 128);
                }
            }
        }
    }

    // Audio controls
    ImGui::Separator();
    ImGui::Text("Audio");
    {
        auto* audio_sys = m_system_manager->get_system<AudioSystem>();
        if (audio_sys && audio_sys->is_initialized()) {
            if (ImGui::Button("Play Step")) {
                audio_sys->play_step();
            }
            ImGui::SameLine();
            if (ImGui::Button("Play Impact")) {
                audio_sys->play_impact();
            }
        } else {
            ImGui::TextDisabled("AudioSystem not initialized");
        }
    }

    // Particle info
    ImGui::Separator();
    {
        auto* p_sys = m_system_manager->get_system<ParticleSystem>();
        auto* gpu_ps = m_renderer->get_gpu_particle_system();
        if (gpu_ps) {
            ImGui::Text("GPU Particles: %d / %d", gpu_ps->alive_count(), GpuParticleSystem::MAX_PARTICLES);
            // Test burst buttons
            static int burst_type = 0;
            const char* burst_names[] = {"Sparks", "Smoke", "Fire", "Trail"};
            ImGui::Combo("Burst Type", &burst_type, burst_names, 4);
            if (ImGui::Button("Test Burst")) {
                // Emit at camera position + forward offset
                if (m_registry && m_registry->is_valid(m_camera_entity) &&
                    m_registry->has_component<Camera>(m_camera_entity)) {
                    const Camera& cam = m_registry->get_component<Camera>(m_camera_entity);
                    glm::vec3 spawn = cam.position + cam.front * 5.0f;
                    gpu_ps->emit_burst_typed(spawn, 30, burst_type);
                }
            }
        }
        if (p_sys) {
            ImGui::Text("CPU Particles: %d / %d", p_sys->alive_count(), ParticleSystem::MAX_PARTICLES);
        }
    }

    // Editor controls
    ImGui::Separator();
    {
        auto* editor_sys = m_system_manager->get_system<EditorSystem>();
        if (editor_sys) {
            bool show_ed = editor_sys->is_editor_visible();
            if (ImGui::Checkbox("Editor Mode", &show_ed)) {
                editor_sys->set_editor_visible(show_ed);
            }
            bool play = editor_sys->is_play_mode();
            if (ImGui::Checkbox("Play Mode", &play)) {
                editor_sys->set_play_mode(play);
            }
        }
    }

    ImGui::Text("Tab - toggle cursor (UI / fly camera)");

    // Profiler overlay
    ImGui::Separator();
    ImGui::Text("Profiler");
    {
        const auto& prof = get_profiler();
        ImGui::Text("FPS: %.1f", prof.fps());
        ImGui::Text("Frame: %.2f ms", prof.get_ms(Profiler::Section::total_frame));
        ImGui::Text("  Physics:  %.2f ms", prof.get_ms(Profiler::Section::physics));
        ImGui::Text("  Render:   %.2f ms",
            prof.get_ms(Profiler::Section::render_record) +
            prof.get_ms(Profiler::Section::render_submit));
        ImGui::Text("    Record: %.2f ms", prof.get_ms(Profiler::Section::render_record));
        ImGui::Text("    Submit: %.2f ms", prof.get_ms(Profiler::Section::render_submit));
        ImGui::Text("  ImGui:    %.2f ms", prof.get_ms(Profiler::Section::imgui));
        ImGui::Text("Draw calls: %d  Culled: %d", prof.draw_calls, prof.culled_count);

        // Entity count
        int total_entities = 0;
        if (m_registry) {
            for (const Entity e : m_registry->view<Position>()) {
                (void)e;
                ++total_entities;
            }
        }
        ImGui::Text("Entities: %d", total_entities);
    }

    ImGui::End();
    }

    // Editor panels (hierarchy, properties, toolbar) — hidden in Play mode
    if (!in_play_mode && editor_sys && m_registry) {
        editor_sys->draw_ui(*m_registry);
    }

    // Pause overlay — when in Play mode and paused
    if (in_play_mode && m_paused) {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(display);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.6f));
        ImGui::Begin("##PauseOverlay", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 80, display.y * 0.5f - 60));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::SetWindowFontScale(2.0f);
        ImGui::Text("PAUSED");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 60, display.y * 0.5f));
        if (ImGui::Button("Resume", ImVec2(120, 30))) {
            m_paused = false;
        }
        ImGui::SetCursorScreenPos(ImVec2(display.x * 0.5f - 60, display.y * 0.5f + 40));
        if (ImGui::Button("Back to Editor", ImVec2(120, 30))) {
            m_paused = false;
            editor_sys->set_play_mode(false);
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    // Draw particles overlay
    {
        auto* p_sys = m_system_manager->get_system<ParticleSystem>();
        if (p_sys && m_registry && m_registry->is_valid(m_camera_entity) &&
            m_registry->has_component<Camera>(m_camera_entity)) {
            const Camera& cam = m_registry->get_component<Camera>(m_camera_entity);
            const glm::mat4 vp = cam.proj * cam.view;
            int w = 0, h = 0;
            glfwGetFramebufferSize(m_window, &w, &h);
            p_sys->draw_particles(vp, static_cast<float>(w), static_cast<float>(h));
        }
    }

    // Game HUD (health, ammo, crosshair, win/lose overlay)
    {
        auto* game_sys = m_system_manager->get_system<GameSystem>();
        if (game_sys && m_registry) {
            if (game_sys->state() == GameState::menu) {
                game_sys->draw_menu();
            } else {
                game_sys->draw_hud(*m_registry);
            }
        }
    }

    ImGui::Render();

    m_renderer->render(alpha);
}

void Application::on_key(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (action != GLFW_PRESS || key != GLFW_KEY_ESCAPE) return;

    auto* editor_sys = m_system_manager->get_system<EditorSystem>();
    if (editor_sys && editor_sys->is_play_mode()) {
        m_paused = !m_paused;
        return;
    }
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void Application::on_framebuffer_resize(int width, int height)
{
    if (m_renderer) {
        m_renderer->on_resize(width, height);
    }
    if (width > 0 && height > 0) {
        auto* cam_sys = m_system_manager->get_system<CameraSystem>();
        if (cam_sys) {
            cam_sys->set_aspect(static_cast<float>(width) / static_cast<float>(height));
        }
    }
}

VkDescriptorPool Application::create_imgui_descriptor_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(m_renderer->get_vk_device(), &pool_info, nullptr, &pool);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool: " + std::to_string(static_cast<int>(result)));
    }
    return pool;
}

void Application::init_imgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    m_imgui_pool = create_imgui_descriptor_pool();

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_renderer->get_vk_instance();
    init_info.PhysicalDevice = m_renderer->get_vk_physical_device();
    init_info.Device = m_renderer->get_vk_device();
    init_info.QueueFamily = m_renderer->get_graphics_queue_family();
    init_info.Queue = m_renderer->get_vk_graphics_queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imgui_pool;
    init_info.RenderPass = m_renderer->get_vk_imgui_render_pass();
    init_info.MinImageCount = 2;
    init_info.ImageCount = m_renderer->get_swapchain_image_count();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    KNG_INFO("ImGui initialized (Vulkan + GLFW)");
}

void Application::shutdown_imgui()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_imgui_pool != VK_NULL_HANDLE && m_renderer) {
        vkDestroyDescriptorPool(m_renderer->get_vk_device(), m_imgui_pool, nullptr);
        m_imgui_pool = VK_NULL_HANDLE;
    }
    KNG_INFO("ImGui shutdown");
}

} // namespace kenga

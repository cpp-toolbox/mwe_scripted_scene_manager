#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "graphics/animated_texture_atlas/animated_texture_atlas.hpp"
#include "graphics/batcher/generated/batcher.hpp"
#include "graphics/fps_camera/fps_camera.hpp"
#include "graphics/scripted_scene_manager/scripted_scene_manager.hpp"
#include "graphics/vertex_geometry/vertex_geometry.hpp"
#include "graphics/window/window.hpp"
#include "graphics/shader_cache/shader_cache.hpp"
#include "graphics/particle_emitter/particle_emitter.hpp"
#include "sound_system/sound_system.hpp"
#include "utility/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "graphics/texture_packer/texture_packer.hpp"
#include "graphics/texture_packer_model_loading/texture_packer_model_loading.hpp"
#include "utility/model_loading/model_loading.hpp"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#include <cstdio>
#include <cstdlib>

#include <functional>
#include <iostream>
#include <random>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

unsigned int SCREEN_WIDTH = 800;
unsigned int SCREEN_HEIGHT = 800;

class SmokeParticleEmitter {
  public:
    ParticleEmitter particle_emitter;

    SmokeParticleEmitter(unsigned int max_particles, Transform initial_transform)
        : particle_emitter(life_span_lambda(), initial_velocity_lambda(), velocity_change_lambda(), scaling_lambda(),
                           rotation_lambda(), spawn_delay_lambda(), max_particles, initial_transform) {}

  private:
    static std::function<float()> life_span_lambda() {
        return []() -> float {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(1.0f, 3.0f);
            return dist(rng);
        };
    }

    static std::function<glm::vec3()> initial_velocity_lambda() {
        return []() -> glm::vec3 {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> horizontal_dist(-0.5f, 0.5f); // Small lateral variance
            /*std::uniform_real_distribution<float> upward_dist(0.75f, 0.9f);     // upward push rever back to this*/
            std::uniform_real_distribution<float> upward_dist(2.0f, 3.0f); // upward push

            // Initial upward push with slight lateral drift
            float dx = horizontal_dist(rng);
            float dy = upward_dist(rng); // Main upward velocity
            float dz = horizontal_dist(rng);

            return glm::vec3(dx, dy, dz);
            /*return glm::vec3(0, 0, 0);*/
        };
    }

    static std::function<glm::vec3(float, float)> velocity_change_lambda() {
        return [](float life_percentage, float delta_time) -> glm::vec3 {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> horizontal_dist(-0.010f, 0.010f); // Small lateral variance
            std::uniform_real_distribution<float> vertical_dist(0.2f, 0.3f);

            float accel_x = horizontal_dist(rng);
            float accel_y = vertical_dist(rng);
            float accel_z = horizontal_dist(rng);

            glm::vec3 smoke_push_down = -glm::vec3(accel_x, accel_y, accel_z) * delta_time;
            return smoke_push_down;
        };
    }

    static std::function<float(float)> scaling_lambda() {
        return [](float life_percentage) -> float { return std::max(life_percentage * 1.2f, 0.0f); };
    }

    static std::function<float(float)> rotation_lambda() {
        return [](float life_percentage) -> float { return life_percentage / 5.0f; };
    }

    static std::function<float()> spawn_delay_lambda() {
        return []() -> float { return 0.1f; };
    }
};

static void error_callback(int error, const char *description) { fprintf(stderr, "Error: %s\n", description); }

void setVec3(GLint unif_loc, const glm::vec3 &value) { glUniform3fv(unif_loc, 1, &value[0]); }
void setFloat(GLint unif_loc, float value) { glUniform1f(unif_loc, value); }

// Light attribute structure
struct PointLightAttributes {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

// NOTE we baked in the specular and diffuse into the lights but in reality this is material based
// need to restructure this later
void set_shader_light_data(FPSCamera &camera, ShaderCache &shader_cache, bool is_flame_active) {
    ShaderProgramInfo shader_info =
        shader_cache.get_shader_program(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_MULTIPLE_LIGHTS);

    shader_cache.use_shader_program(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_MULTIPLE_LIGHTS);

    GLint location = glGetUniformLocation(shader_info.id, "view_pos");
    if (location != -1) {
        setVec3(location, camera.transform.position);
    } else {
        std::cerr << "Warning: Uniform 'view_pos' not found!" << std::endl;
    }

    // Set directional light
    auto set_dir_light = [&](const glm::vec3 &direction, const glm::vec3 &ambient, const glm::vec3 &diffuse,
                             const glm::vec3 &specular) {
        setVec3(glGetUniformLocation(shader_info.id, "dir_light.direction"), direction);
        setVec3(glGetUniformLocation(shader_info.id, "dir_light.ambient"), ambient);
        setVec3(glGetUniformLocation(shader_info.id, "dir_light.diffuse"), diffuse);
        setVec3(glGetUniformLocation(shader_info.id, "dir_light.specular"), specular);
    };
    set_dir_light({-0.2f, -1.0f, -0.3f}, {0.1f, 0.1f, 0.1f}, {0.8f, 0.8f, 0.8f}, {1.0f, 1.0f, 1.0f});

    // Point light data
    std::vector<PointLightAttributes> point_lights = {
        {{0, 0, 0}, {0.52f, 0.12f, 0.32f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, 1.0f, 0.09f, 0.032f},
        {{2, -2, 2}, {0.02f, 0.02f, 0.02f}, {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.4f}, 1.0f, 0.09f, 0.032f},
        {{2, 2, -2}, {0.02f, 0.02f, 0.02f}, {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.4f}, 1.0f, 0.09f, 0.032f},
        {{-2, 2, 2}, {0.02f, 0.02f, 0.02f}, {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.4f}, 1.0f, 0.09f, 0.032f},
    };

    // Enhanced flickering effect
    float current_time = static_cast<float>(glfwGetTime());
    float flicker_factor = (sin(current_time * 7.0f) + sin(current_time * 13.0f)) * 0.5f + 0.5f; // Combined sine waves
    float flame_intensity = is_flame_active ? (0.6f + 0.4f * flicker_factor) : 0.0f;

    for (size_t i = 0; i < point_lights.size(); ++i) {
        auto light = point_lights[i];
        std::string base = "point_lights[" + std::to_string(i) + "].";

        // Adjust flame light intensity
        if (is_flame_active && i == 0) {
            light.diffuse *= flame_intensity;
            light.specular *= flame_intensity;
        }

        // Adjust flame light intensity
        if (!is_flame_active && i == 0) {
            light.diffuse *= 0;
            light.specular *= 0;
        }

        setVec3(glGetUniformLocation(shader_info.id, (base + "position").c_str()), light.position);
        setVec3(glGetUniformLocation(shader_info.id, (base + "ambient").c_str()), light.ambient);
        setVec3(glGetUniformLocation(shader_info.id, (base + "diffuse").c_str()), light.diffuse);
        setVec3(glGetUniformLocation(shader_info.id, (base + "specular").c_str()), light.specular);
        setFloat(glGetUniformLocation(shader_info.id, (base + "constant").c_str()), light.constant);
        setFloat(glGetUniformLocation(shader_info.id, (base + "linear").c_str()), light.linear);
        setFloat(glGetUniformLocation(shader_info.id, (base + "quadratic").c_str()), light.quadratic);
    }

    // Set spot light
    auto set_spot_light = [&](const glm::vec3 &position, const glm::vec3 &direction, const glm::vec3 &ambient,
                              const glm::vec3 &diffuse, const glm::vec3 &specular, float constant, float linear,
                              float quadratic, float cutoff, float outer_cutoff) {
        setVec3(glGetUniformLocation(shader_info.id, "spot_light.position"), position);
        setVec3(glGetUniformLocation(shader_info.id, "spot_light.direction"), direction);
        setVec3(glGetUniformLocation(shader_info.id, "spot_light.ambient"), ambient);
        setVec3(glGetUniformLocation(shader_info.id, "spot_light.diffuse"), diffuse);
        setVec3(glGetUniformLocation(shader_info.id, "spot_light.specular"), specular);
        setFloat(glGetUniformLocation(shader_info.id, "spot_light.constant"), constant);
        setFloat(glGetUniformLocation(shader_info.id, "spot_light.linear"), linear);
        setFloat(glGetUniformLocation(shader_info.id, "spot_light.quadratic"), quadratic);
        setFloat(glGetUniformLocation(shader_info.id, "spot_light.cut_off"), cutoff);
        setFloat(glGetUniformLocation(shader_info.id, "spot_light.outer_cut_off"), outer_cutoff);
    };

    set_spot_light(camera.transform.position, camera.transform.compute_forward_vector(),
                   {0.1f, 0.1f, 0.1f}, // ambient light (soft overall lighting)
                   {1.0f, 1.0f, 1.0f}, // diffuse light (intense light from the spotlight)
                   {1.0f, 1.0f, 1.0f}, // specular light (highlighted areas with shininess)
                   1.0f, 0.09f, 0.032f, glm::cos(glm::radians(12.5f)), glm::cos(glm::radians(15.0f)));
}

// Wrapper that automatically creates a lambda for member functions
template <typename T, typename R, typename... Args> auto wrap_member_function(T &obj, R (T::*f)(Args...)) {
    // Return a std::function that wraps the member function in a lambda
    return std::function<R(Args...)>{[&obj, f](Args &&...args) { return (obj.*f)(std::forward<Args>(args)...); }};
}

int main() {

    bool flame_active = false;
    std::vector<glm::vec3> flame_vertices = generate_rectangle_vertices(0, 2, 3, 3);
    std::vector<unsigned int> flame_indices = generate_rectangle_indices();
    auto flame_normals = generate_rectangle_normals();

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("mwe_shader_cache_logs.txt", true);
    file_sink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

    LiveInputState live_input_state;

    GLFWwindow *window =
        initialize_glfw_glad_and_return_window(SCREEN_WIDTH, SCREEN_HEIGHT, "glfw window", false, false, false);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    FPSCamera camera(glm::vec3(0, 0, 3), 50, SCREEN_WIDTH, SCREEN_HEIGHT, 90, 0.1, 50);
    std::function<void(unsigned int)> char_callback = [](unsigned int _) {};
    std::function<void(int, int, int, int)> key_callback = [](int _, int _1, int _2, int _3) {};
    std::function<void(double, double)> mouse_pos_callback = wrap_member_function(camera, &FPSCamera::mouse_callback);
    std::function<void(int, int, int)> mouse_button_callback = [](int _, int _1, int _2) {};
    GLFWLambdaCallbackManager glcm(window, char_callback, key_callback, mouse_pos_callback, mouse_button_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Hide and capture the mouse

    std::vector<ShaderType> requested_shaders = {
        ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_MULTIPLE_LIGHTS};
    ShaderCache shader_cache(requested_shaders, sinks);
    Batcher batcher(shader_cache);

    /*TexturePacker texture_packer("assets/packed_textures/packed_texture.json",*/
    /*                             {"assets/packed_textures/container_0_atlas_visualization.png",*/
    /*                              "assets/packed_textures/container_1_atlas_visualization.png"});*/

    TexturePacker texture_packer(
        "assets/packed_textures/packed_texture.json",
        {"assets/packed_textures/packed_texture_0.png", "assets/packed_textures/packed_texture_1.png"});

    AnimatedTextureAtlas animated_texture_atlas("", "assets/images/flame.png", 50.0, texture_packer);

    /*AnimatedTextureAtlas animated_texture_atlas("", "assets/images/flame.png", 500.0, texture_packer);*/

    auto room = parse_model_into_ivpnts("assets/room/room.obj", true);
    auto packed_room = convert_ivpnt_to_ivpntp(room, texture_packer);

    glfwSwapInterval(0);

    GLuint ltw_matrices_gl_name;
    glm::mat4 ltw_matrices[1024];

    // initialize all matrices to identity matrices
    for (int i = 0; i < 1024; ++i) {
        ltw_matrices[i] = glm::mat4(1.0f);
    }

    glGenBuffers(1, &ltw_matrices_gl_name);
    glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ltw_matrices), ltw_matrices, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ltw_matrices_gl_name);

    std::unordered_map<SoundType, std::string> sound_type_to_file = {
        {SoundType::SOUND_1, "assets/sounds/Flick_noflame.mp3"},
        {SoundType::SOUND_2, "assets/sounds/Flick_withflame.mp3"},
    };

    SoundSystem sound_system(100, sound_type_to_file);

    std::vector<glm::vec2> packed_tex_coords_last_tick{};
    int curr_obj_id = 1000;
    int flame_obj_id = 1000;

    Transform spe_transform;
    spe_transform.position = glm::vec3(0, 2, 0);
    spe_transform.scale = glm::vec3(3, 3, 3);
    SmokeParticleEmitter spe(1000, spe_transform);
    ScriptedSceneManager scripted_scene_manager("assets/scene_script.json");

    std::function<void(double, const json &, const json &)> scripted_events =
        [&](double ms_curr_time, const json &curr_state, const json &prev_state) {
            if (curr_state["flame.draw"]) {
                flame_active = true;
                spe.particle_emitter.resume_emitting_particles();
                std::vector<glm::vec2> packed_tex_coords =
                    animated_texture_atlas.get_texture_coordinates_of_current_animation_frame(ms_curr_time);

                /*auto packed_tex_coords =*/
                /*    texture_packer.get_packed_texture_coordinates("assets/images/alphabet.png",
                 * atlas_texture_coordinates);*/

                bool new_coords = false;
                if (packed_tex_coords != packed_tex_coords_last_tick) {
                    new_coords = true;
                    curr_obj_id += 1;
                }
                packed_tex_coords_last_tick = packed_tex_coords;

                const std::vector<int> packed_texture_indices(4, 0);
                std::vector<unsigned int> ltw_mat_idxs(4, flame_obj_id);

                batcher.texture_packer_cwl_v_transformation_ubos_1024_multiple_lights_shader_batcher.queue_draw(
                    curr_obj_id, flame_indices, flame_vertices, ltw_mat_idxs, packed_texture_indices, packed_tex_coords,
                    flame_normals);
            }

            if (!curr_state["flame.draw"]) {
                flame_active = false;
                spe.particle_emitter.stop_emitting_particles();
            }

            if (curr_state["flick.play"] && !prev_state["flick.play"]) {
                sound_system.queue_sound(SoundType::SOUND_2, glm::vec3(0.0));
                sound_system.play_all_sounds();
            }
        };

    auto smoke_vertices = generate_square_vertices(0, 0, 0.5);
    auto smoke_indices = generate_rectangle_indices();
    std::vector<glm::vec2> smoke_local_uvs = generate_rectangle_texture_coordinates();
    auto smoke_texture_coordinates =
        texture_packer.get_packed_texture_coordinates("assets/images/smoke_64px.png", smoke_local_uvs);
    auto smoke_pt_idx = texture_packer.get_packed_texture_index_of_texture("assets/images/smoke_64px.png");
    std::vector<int> smoke_pt_idxs(4, smoke_pt_idx); // 4 because square

    int width, height;

    double previous_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double current_time = glfwGetTime();
        double delta_time = current_time - previous_time;
        previous_time = current_time;

        glfwGetFramebufferSize(window, &width, &height);

        glViewport(0, 0, width, height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1, 0.1, 0.1, 1.0);

        // pass uniforms
        camera.process_input(window, delta_time);

        glm::mat4 projection = camera.get_projection_matrix();
        glm::mat4 view = camera.get_view_matrix();

        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_MULTIPLE_LIGHTS,
                                 ShaderUniformVariable::CAMERA_TO_CLIP, projection);
        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_MULTIPLE_LIGHTS,
                                 ShaderUniformVariable::WORLD_TO_CAMERA, view);

        spe.particle_emitter.update(delta_time, projection * view);
        auto particles = spe.particle_emitter.get_particles_sorted_by_distance();

        set_shader_light_data(camera, shader_cache, flame_active);

        int count = 1000;
        for (auto &ivptp : packed_room) {
            std::vector<unsigned int> ltw_indices(ivptp.xyz_positions.size(), 1000);
            std::vector<int> ptis(ivptp.xyz_positions.size(), ivptp.packed_texture_index);
            batcher.texture_packer_cwl_v_transformation_ubos_1024_multiple_lights_shader_batcher.queue_draw(
                count, ivptp.indices, ivptp.xyz_positions, ltw_indices, ptis, ivptp.packed_texture_coordinates,
                ivptp.normals);
            count++;
        }

        // run scripted events
        double ms_curr_time = glfwGetTime() * 1000.0;
        scripted_scene_manager.run_scripted_events(ms_curr_time, scripted_events);

        for (size_t i = 0; i < particles.size(); ++i) {
            auto &curr_particle = particles[i];

            //  compute the up vector (assuming we want it to be along the y-axis)
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 forward = camera.transform.compute_forward_vector();

            glm::vec3 right = glm::normalize(glm::cross(up, forward));

            up = glm::normalize(glm::cross(forward, right));

            // this makes it billboarded
            glm::mat4 rotation_matrix = glm::mat4(1.0f);
            rotation_matrix[0] = glm::vec4(right, 0.0f);
            rotation_matrix[1] = glm::vec4(up, 0.0f);
            rotation_matrix[2] = glm::vec4(-forward, 0.0f); // We negate the direction for correct facing

            // I think this is bad.
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), curr_particle.transform.position);
            transform *= rotation_matrix;
            transform = glm::scale(transform, curr_particle.transform.scale);
            transform = glm::scale(transform, curr_particle.emitter_transform.scale);

            ltw_matrices[i] = transform;

            if (curr_particle.is_alive()) {

                /*auto nv =*/
                /*    generate_rectangle_vertices_3d(particle.transform.position,
                 * camera.transform.compute_right_vector(),*/
                /*                                   camera.transform.compute_up_vector(), 1, 1);*/

                std::vector<unsigned int> smoke_ltw_mat_idxs(4, i);
                batcher.texture_packer_cwl_v_transformation_ubos_1024_multiple_lights_shader_batcher.queue_draw(
                    i, smoke_indices, smoke_vertices, smoke_ltw_mat_idxs, smoke_pt_idxs, smoke_texture_coordinates,
                    flame_normals);
            }
        }

        // load in the matrices
        glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ltw_matrices), ltw_matrices);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        batcher.texture_packer_cwl_v_transformation_ubos_1024_multiple_lights_shader_batcher.draw_everything();
        // -------------------

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}

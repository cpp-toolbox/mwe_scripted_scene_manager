#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "graphics/animated_texture_atlas/animated_texture_atlas.hpp"
#include "graphics/batcher/generated/batcher.hpp"
#include "graphics/fps_camera/fps_camera.hpp"
#include "graphics/scripted_scene_manager/scripted_scene_manager.hpp"
#include "graphics/window/window.hpp"
#include "graphics/shader_cache/shader_cache.hpp"
#include "graphics/particle_emitter/particle_emitter.hpp"
#include "sound_system/sound_system.hpp"
#include "utility/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "graphics/texture_packer/texture_packer.hpp"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#include <cstdio>
#include <cstdlib>

#include <functional>
#include <iostream>
#include <random>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

unsigned int SCREEN_WIDTH = 640;
unsigned int SCREEN_HEIGHT = 480;

class SmokeParticleEmitter {
  public:
    ParticleEmitter particle_emitter;

    SmokeParticleEmitter(unsigned int max_particles)
        : particle_emitter(life_span_lambda(), initial_velocity_lambda(), velocity_change_lambda(), scaling_lambda(),
                           rotation_lambda(), spawn_delay_lambda(), max_particles) {}

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
            std::uniform_real_distribution<float> upward_dist(1.0f, 2.0f);      // Strong upward push

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
        return [](float life_percentage) -> float { return std::max(life_percentage * 2.0f, 0.0f); };
    }

    static std::function<float(float)> rotation_lambda() {
        return [](float life_percentage) -> float { return life_percentage / 5.0f; };
    }

    static std::function<float()> spawn_delay_lambda() {
        return []() -> float {
            return 0.01f; // Spawn a new particle every 0.05 seconds
        };
    }
};

static void error_callback(int error, const char *description) { fprintf(stderr, "Error: %s\n", description); }

// Wrapper that automatically creates a lambda for member functions
template <typename T, typename R, typename... Args> auto wrap_member_function(T &obj, R (T::*f)(Args...)) {
    // Return a std::function that wraps the member function in a lambda
    return std::function<R(Args...)>{[&obj, f](Args &&...args) { return (obj.*f)(std::forward<Args>(args)...); }};
}

int main() {
    std::vector<glm::vec3> flame_vertices = {
        {0.5f, 0.5f, 0.0f},   // top right
        {0.5f, -0.5f, 0.0f},  // bottom right
        {-0.5f, -0.5f, 0.0f}, // bottom left
        {-0.5f, 0.5f, 0.0f}   // top left
    };
    std::vector<unsigned int> flame_indices = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

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

    std::vector<ShaderType> requested_shaders = {ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024};
    ShaderCache shader_cache(requested_shaders, sinks);
    Batcher batcher(shader_cache);
    AnimatedTextureAtlas animated_texture_atlas("assets/images/alphabet.json", "assets/images/alphabet.png", 500.0);

    TexturePacker texture_packer("assets/packed_textures/packed_texture.json",
                                 {"assets/packed_textures/packed_texture_0.png"});

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

    ScriptedSceneManager scripted_scene_manager("assets/scene_script.json");
    std::function<void(double, const json &, const json &)> scripted_events = [&](double ms_curr_time,
                                                                                  const json &curr_state,
                                                                                  const json &prev_state) {
        if (curr_state["flame.draw"]) {
            std::vector<glm::vec2> atlas_texture_coordinates =
                animated_texture_atlas.get_texture_coordinates_of_sprite(ms_curr_time);
            auto packed_tex_coords =
                texture_packer.get_packed_texture_coordinates("assets/images/alphabet.png", atlas_texture_coordinates);

            bool new_coords = false;
            if (packed_tex_coords != packed_tex_coords_last_tick) {
                new_coords = true;
                curr_obj_id += 1;
            }
            packed_tex_coords_last_tick = packed_tex_coords;

            const std::vector<int> packed_texture_indices(4, 0);
            std::vector<unsigned int> ltw_mat_idxs(4, flame_obj_id);

            batcher.texture_packer_cwl_v_transformation_ubos_1024_shader_batcher.queue_draw(
                curr_obj_id, flame_indices, flame_vertices, ltw_mat_idxs, packed_texture_indices, packed_tex_coords);
        }

        if (curr_state["flick.play"] && !prev_state["flick.play"]) {
            sound_system.queue_sound(SoundType::SOUND_2, glm::vec3(0.0));
            sound_system.play_all_sounds();
        }
    };

    SmokeParticleEmitter spe(1000);
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
        glClearColor(0.5, 0.5, 0.5, 1.0);

        // pass uniforms
        camera.process_input(window, delta_time);

        glm::mat4 projection = camera.get_projection_matrix();
        glm::mat4 view = camera.get_view_matrix();

        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024,
                                 ShaderUniformVariable::CAMERA_TO_CLIP, projection);
        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024,
                                 ShaderUniformVariable::WORLD_TO_CAMERA, view);

        // run scripted events
        // -------------------
        double ms_curr_time = glfwGetTime() * 1000.0;
        scripted_scene_manager.run_scripted_events(ms_curr_time, scripted_events);

        spe.particle_emitter.update(delta_time, projection * view);
        auto particles = spe.particle_emitter.get_particles_sorted_by_distance();

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

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), curr_particle.transform.position);
            transform *= rotation_matrix;
            transform = glm::scale(transform, curr_particle.transform.scale);

            ltw_matrices[i] = transform;

            if (curr_particle.is_alive()) {

                /*auto nv =*/
                /*    generate_rectangle_vertices_3d(particle.transform.position,
                 * camera.transform.compute_right_vector(),*/
                /*                                   camera.transform.compute_up_vector(), 1, 1);*/

                std::vector<unsigned int> smoke_ltw_mat_idxs(4, i);
                batcher.texture_packer_cwl_v_transformation_ubos_1024_shader_batcher.queue_draw(
                    i, smoke_indices, smoke_vertices, smoke_ltw_mat_idxs, smoke_pt_idxs, smoke_texture_coordinates);
            }
        }

        // load in the matrices
        glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ltw_matrices), ltw_matrices);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        batcher.texture_packer_cwl_v_transformation_ubos_1024_shader_batcher.draw_everything();
        // -------------------

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
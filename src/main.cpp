#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "graphics/animated_texture_atlas/animated_texture_atlas.hpp"
#include "graphics/batcher/generated/batcher.hpp"
#include "graphics/fps_camera/fps_camera.hpp"
#include "graphics/scripted_scene_manager/scripted_scene_manager.hpp"
#include "graphics/window/window.hpp"
#include "graphics/shader_cache/shader_cache.hpp"
#include "sound_system/sound_system.hpp"
#include "utility/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "graphics/texture_packer/texture_packer.hpp"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#include <cstdio>
#include <cstdlib>

#include <functional>
#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

unsigned int SCREEN_WIDTH = 640;
unsigned int SCREEN_HEIGHT = 480;

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
    int curr_obj_id = 0;
    int flame_obj_id = 0;

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

    int width, height;

    double previous_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double current_time = glfwGetTime();
        double delta_time = current_time - previous_time;
        previous_time = current_time;

        glfwGetFramebufferSize(window, &width, &height);

        glViewport(0, 0, width, height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
        batcher.texture_packer_cwl_v_transformation_ubos_1024_shader_batcher.draw_everything();
        // -------------------

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
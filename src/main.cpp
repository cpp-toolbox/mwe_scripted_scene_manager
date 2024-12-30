#include <fmt/core.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/detail/qualifier.hpp>

#include "graphics/animated_texture_atlas/animated_texture_atlas.hpp"
#include "graphics/batcher/generated/batcher.hpp"
#include "graphics/fps_camera/fps_camera.hpp"
#include "graphics/scripted_events/scripted_scene_manager.hpp"
#include "graphics/vertex_geometry/vertex_geometry.hpp"
#include "graphics/window/window.hpp"
#include "graphics/shader_cache/shader_cache.hpp"
#include "graphics/particle_emitter/particle_emitter.hpp"
#include "graphics/texture_packer/texture_packer.hpp"
#include "graphics/texture_packer_model_loading/texture_packer_model_loading.hpp"

#include "sound_system/sound_system.hpp"

#include "utility/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "utility/model_loading/model_loading.hpp"
#include "utility/rigged_model_loading/rigged_model_loading.hpp"
#include "utility/unique_id_generator/unique_id_generator.hpp"

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

#include <atomic>
#include <iostream>

class BlowingSmokeParticleEmitter {
  public:
    ParticleEmitter particle_emitter;

    BlowingSmokeParticleEmitter(unsigned int max_particles, Transform initial_transform)
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
            std::uniform_real_distribution<float> x_dist(0.05, 0.1f); // minor variance
            std::uniform_real_distribution<float> z_dist(0.3, 0.5f);  // blowing
            std::uniform_real_distribution<float> y_dist(0.5f, 0.1f); // upward push

            // Initial upward push with slight lateral drift
            float dx = x_dist(rng);
            float dy = y_dist(rng); // Main upward velocity
            float dz = z_dist(rng);

            return glm::vec3(dx, dy, dz);
            /*return glm::vec3(0, 0, 0);*/
        };
    }

    static std::function<glm::vec3(float, float)> velocity_change_lambda() {
        return [](float life_percentage, float delta_time) -> glm::vec3 {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> horizontal_dist(-0.0025f, 0.0025f); // Small lateral variance
            std::uniform_real_distribution<float> vertical_dist(0.05f, 0.1f);

            float accel_x = horizontal_dist(rng);
            float accel_y = vertical_dist(rng);
            accel_y = 0;
            float accel_z = horizontal_dist(rng);

            glm::vec3 smoke_push_down = -glm::vec3(accel_x, accel_y, accel_z) * delta_time;
            return smoke_push_down;
        };
    }

    static std::function<float(float)> scaling_lambda() {
        return [](float life_percentage) -> float { return life_percentage * 0.1; };
    }

    static std::function<float(float)> rotation_lambda() {
        return [](float life_percentage) -> float { return life_percentage / 5.0f; };
    }

    static std::function<float()> spawn_delay_lambda() {
        return []() -> float { return 0.05f; };
    }
};

class CigaretteSmokeParticleEmitter {
  public:
    ParticleEmitter particle_emitter;

    CigaretteSmokeParticleEmitter(unsigned int max_particles, Transform initial_transform)
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
            std::uniform_real_distribution<float> horizontal_dist(-0.10f, 0.10f); // Small lateral variance
            /*std::uniform_real_distribution<float> upward_dist(0.75f, 0.9f);     // upward push rever back to this*/
            std::uniform_real_distribution<float> upward_dist(0.1f, 0.2f); // upward push

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
            std::uniform_real_distribution<float> horizontal_dist(-0.0025f, 0.0025f); // Small lateral variance
            std::uniform_real_distribution<float> vertical_dist(0.05f, 0.1f);

            float accel_x = horizontal_dist(rng);
            float accel_y = vertical_dist(rng);
            accel_y = 0;
            float accel_z = horizontal_dist(rng);

            glm::vec3 smoke_push_down = -glm::vec3(accel_x, accel_y, accel_z) * delta_time;
            return smoke_push_down;
        };
    }

    static std::function<float(float)> scaling_lambda() {
        return [](float life_percentage) -> float { return life_percentage * 0.1; };
    }

    static std::function<float(float)> rotation_lambda() {
        return [](float life_percentage) -> float { return life_percentage / 5.0f; };
    }

    static std::function<float()> spawn_delay_lambda() {
        return []() -> float { return 0.10f; };
    }
};

static void error_callback(int error, const char *description) { fprintf(stderr, "Error: %s\n", description); }

void setVec3(GLint unif_loc, const glm::vec3 &value) { glUniform3fv(unif_loc, 1, &value[0]); }
void setFloat(GLint unif_loc, float value) { glUniform1f(unif_loc, value); }

// Light attribute structure
struct PointLightAttributes {
    glm::vec3 position = glm::vec3(0, 0, 0);
    glm::vec3 ambient = glm::vec3(0, 0, 0);
    glm::vec3 diffuse = glm::vec3(0, 0, 0);
    glm::vec3 specular = glm::vec3(0, 0, 0);
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
};

// NOTE we baked in the specular and diffuse into the lights but in reality this is material based
// need to restructure this later
void set_shader_light_data(FPSCamera &camera, ShaderCache &shader_cache, bool is_flame_active,
                           glm::vec3 flame_light_pos) {
    ShaderProgramInfo shader_info = shader_cache.get_shader_program(
        ShaderType::
            TEXTURE_PACKER_RIGGED_AND_ANIMATED_CWL_V_TRANSFORMATION_UBOS_1024_WITH_TEXTURES_AND_MULTIPLE_LIGHTS);

    shader_cache.use_shader_program(
        ShaderType::
            TEXTURE_PACKER_RIGGED_AND_ANIMATED_CWL_V_TRANSFORMATION_UBOS_1024_WITH_TEXTURES_AND_MULTIPLE_LIGHTS);

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
        {flame_light_pos, {0.52f, 0.32f, 0.32f}, {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.4f}, 8.0f, 8.0f, 8.0f},
        {{.8, -.8, .8}, {0.00f, 0.00f, 0.00f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.09f, 0.032f},
        {{.8, .8, -.8}, {0.00f, 0.00f, 0.00f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.09f, 0.032f},
        {{-.8, .8, .8}, {0.00f, 0.00f, 0.00f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.09f, 0.032f},
    };
    PointLightAttributes empty_light = PointLightAttributes();

    // Enhanced flickering effect
    float current_time = static_cast<float>(glfwGetTime());
    float flicker_factor = (sin(current_time * 7.0f) + sin(current_time * 13.0f)) * 0.5f + 0.5f; // Combined sine waves
    float flame_intensity = is_flame_active ? (0.1f + 0.4f * flicker_factor) : 0.0f;

    for (size_t i = 0; i < point_lights.size(); ++i) {
        PointLightAttributes light_data;
        std::string base = "point_lights[" + std::to_string(i) + "].";

        if (i == 0) {
            if (not is_flame_active) {
                light_data = empty_light;
            } else {
                light_data = point_lights[i];
            }
        } else {
            light_data = point_lights[i];
        }

        setVec3(glGetUniformLocation(shader_info.id, (base + "position").c_str()), light_data.position);
        setVec3(glGetUniformLocation(shader_info.id, (base + "ambient").c_str()), light_data.ambient);
        setVec3(glGetUniformLocation(shader_info.id, (base + "diffuse").c_str()), light_data.diffuse);
        setVec3(glGetUniformLocation(shader_info.id, (base + "specular").c_str()), light_data.specular);
        setFloat(glGetUniformLocation(shader_info.id, (base + "constant").c_str()), light_data.constant);
        setFloat(glGetUniformLocation(shader_info.id, (base + "linear").c_str()), light_data.linear);
        setFloat(glGetUniformLocation(shader_info.id, (base + "quadratic").c_str()), light_data.quadratic);
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
                   /*{0.1f, 0.1f, 0.1f}, // ambient light (soft overall lighting)*/
                   /*{0.5f, 0.5f, 0.5f}, // diffuse light (intense light from the spotlight)*/
                   /*{0.5f, 0.5f, 0.5f}, // specular light (highlighted areas with shininess)*/
                   // temporarily turning this off
                   {0.0f, 0.0f, 0.0f}, // ambient light (soft overall lighting)
                   {0.0f, 0.0f, 0.0f}, // diffuse light (intense light from the spotlight)
                   {0.0f, 0.0f, 0.0f}, // specular light (highlighted areas with shininess)
                   1.0f, 0.09f, 0.032f, glm::cos(glm::radians(12.5f)), glm::cos(glm::radians(15.0f)));
}

// Wrapper that automatically creates a lambda for member functions
template <typename T, typename R, typename... Args> auto wrap_member_function(T &obj, R (T::*f)(Args...)) {
    // Return a std::function that wraps the member function in a lambda
    return std::function<R(Args...)>{[&obj, f](Args &&...args) { return (obj.*f)(std::forward<Args>(args)...); }};
}

void draw_packed_object(std::vector<IVPNTexturePacked> &packed_object, Transform &object_transform,
                        glm::mat4 *ltw_matrices, Batcher &batcher) {
    int ltw_mat_idx = packed_object[0].id;
    ltw_matrices[ltw_mat_idx] = object_transform.get_transform_matrix();
    for (auto &ivptp : packed_object) {
        // hopefully the matrix at this index is an identity
        std::vector<unsigned int> ltw_indices(ivptp.xyz_positions.size(), ltw_mat_idx);
        std::vector<int> ptis(ivptp.xyz_positions.size(), ivptp.packed_texture_index);
        std::vector<glm::ivec4> blank_bone_ids(ivptp.xyz_positions.size(), glm::ivec4(0, 0, 0, 0));
        std::vector<glm::vec4> blank_bone_weights(ivptp.xyz_positions.size(), glm::vec4(0, 0, 0, 0));

        batcher
            .texture_packer_rigged_and_animated_cwl_v_transformation_ubos_1024_with_textures_and_multiple_lights_shader_batcher
            .queue_draw(ivptp.id, ivptp.indices, ltw_indices, blank_bone_ids, blank_bone_weights, ptis,
                        ivptp.packed_texture_coordinates, ivptp.normals, ivptp.xyz_positions);
    }
}

// so that you can attach an item to a bone and keep it attached while animations still play
glm::mat4 get_the_transform_to_attach_an_object_to_a_bone(std::string bone_name, Transform &bone_origin_offset,
                                                          RecIvpntRiggedCollector &rirc) {

    int bone_index = rirc.bone_name_to_unique_index[bone_name];
    BoneInfo bone_info = rirc.bone_unique_idx_to_info[bone_index];
    auto the_transform_that_translates_the_origin_to_the_bones_origin =
        glm::inverse(bone_info.local_space_to_bone_space_in_bind_pose_transformation);

    // put it in the right spot, then git it some translation
    the_transform_that_translates_the_origin_to_the_bones_origin =
        bone_origin_offset.get_transform_matrix() * the_transform_that_translates_the_origin_to_the_bones_origin;
    // then animate it which will work because the emitter is relative to the mesh in bind pose now.
    auto animated_transform = bone_info.local_space_animated_transform_upto_this_bone *
                              the_transform_that_translates_the_origin_to_the_bones_origin;

    return animated_transform;
}

int main() {

    unsigned int flame_id = UniqueIDGenerator::generate();
    bool flame_active = false;
    bool cigarette_light_active = false;
    std::vector<glm::vec3> flame_vertices = generate_rectangle_vertices(0, 0, .03, .03);
    std::vector<unsigned int> flame_indices = generate_rectangle_indices();
    auto flame_normals = generate_rectangle_normals();

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("mwe_shader_cache_logs.txt", true);
    file_sink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

    LiveInputState live_input_state;

    GLFWwindow *window =
        initialize_glfw_glad_and_return_window(SCREEN_WIDTH, SCREEN_HEIGHT, "cpp-tbx demo", true, true, false, true);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    FPSCamera camera(glm::vec3(0, 0, 0), 50, SCREEN_WIDTH, SCREEN_HEIGHT, 90, 0.1, 50);
    std::function<void(unsigned int)> char_callback = [](unsigned int _) {};
    std::function<void(int, int, int, int)> key_callback = [](int _, int _1, int _2, int _3) {};
    std::function<void(double, double)> mouse_pos_callback = wrap_member_function(camera, &FPSCamera::mouse_callback);
    std::function<void(int, int, int)> mouse_button_callback = [](int _, int _1, int _2) {};
    GLFWLambdaCallbackManager glcm(window, char_callback, key_callback, mouse_pos_callback, mouse_button_callback);

    std::vector<ShaderType> requested_shaders = {
        ShaderType::
            TEXTURE_PACKER_RIGGED_AND_ANIMATED_CWL_V_TRANSFORMATION_UBOS_1024_WITH_TEXTURES_AND_MULTIPLE_LIGHTS};
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

    RecIvpntRiggedCollector rirc;

    Transform crosshair_transform = Transform();
    crosshair_transform.scale = glm::vec3(.01, .01, .01);
    auto crosshair = parse_model_into_ivpnts("assets/crosshair/3d_crosshair.obj", false);
    std::vector<IVPNTexturePacked> packed_crosshair = convert_ivpnt_to_ivpntp(crosshair, texture_packer);

    auto lightbulb = parse_model_into_ivpnts("assets/lightbulb/lightbulb.obj", false);
    // we have four point lights atm
    std::vector<IVPNTexturePacked> packed_lightbulb_1 = convert_ivpnt_to_ivpntp(lightbulb, texture_packer);
    std::vector<IVPNTexturePacked> packed_lightbulb_2 = convert_ivpnt_to_ivpntp(lightbulb, texture_packer);
    std::vector<IVPNTexturePacked> packed_lightbulb_3 = convert_ivpnt_to_ivpntp(lightbulb, texture_packer);
    std::vector<IVPNTexturePacked> packed_lightbulb_4 = convert_ivpnt_to_ivpntp(lightbulb, texture_packer);

    /*{{0, 0, 0}, {0.52f, 0.32f, 0.32f}, {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.4f}, 1.0f, 0.09f, 0.032f},*/
    /*{{2, -2, 2}, {0.02f, 0.02f, 0.02f}, {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.4f}, 1.0f, 0.09f, 0.032f},*/
    /*{{2, 2, -2}, {0.02f, 0.02f, 0.02f}, {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.4f}, 1.0f, 0.09f, 0.032f},*/
    /*{{-2, 2, 2}, {0.02f, 0.02f, 0.02f}, {0.1f, 0.1f, 0.1f}, {0.4f, 0.4f, 0.4f}, 1.0f, 0.09f, 0.032f},*/
    auto lightbulb_1_transform = Transform();
    lightbulb_1_transform.position = glm::vec3(0, 0, 0);

    auto lightbulb_2_transform = Transform();
    lightbulb_2_transform.position = glm::vec3(.8, -.8, .8);

    auto lightbulb_3_transform = Transform();
    lightbulb_3_transform.position = glm::vec3(.8, .8, -.8);

    auto lightbulb_4_transform = Transform();
    lightbulb_4_transform.position = glm::vec3(-.8, .8, .8);

    /*std::vector<IVPNTRigged> smoke_ivpntrs = rirc.parse_model_into_ivpntrs("assets/test/test.fbx");*/
    std::vector<IVPNTRigged> smoke_ivpntrs = rirc.parse_model_into_ivpntrs("assets/smoking/smoking.fbx");
    std::vector<IVPNTPRigged> smoke_ivptprs = convert_ivpnt_to_ivpntpr(smoke_ivpntrs, texture_packer);

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
        {SoundType::LIGHTER_FAIL, "assets/sounds/lighter_fail.mp3"},
        {SoundType::LIGHTER_SUCCESS, "assets/sounds/lighter_success.mp3"},
        {SoundType::GRAB, "assets/sounds/grab.wav"},
        {SoundType::KNIFE_GRAB, "assets/sounds/knife_grab.wav"},
        {SoundType::STAB, "assets/sounds/stab.wav"},
        {SoundType::CIGARETTE_BURN, "assets/sounds/cigarette_burning.wav"},
        {SoundType::EXHALE, "assets/sounds/exhale.wav"},
        {SoundType::AMBIENT, "assets/sounds/ambient.wav"},
        {SoundType::WOOSH, "assets/sounds/woosh.wav"},
    };

    SoundSystem sound_system(100, sound_type_to_file);

    sound_system.queue_sound(SoundType::AMBIENT, glm::vec3(0, 0, 0));

    std::vector<glm::vec2> packed_tex_coords_last_tick{};
    int curr_obj_id = 1000;
    int flame_obj_id = 1000;

    Transform spe_transform;
    spe_transform.position = glm::vec3(0, 0, 0);
    spe_transform.scale = glm::vec3(.2, .2, .2);
    CigaretteSmokeParticleEmitter cs_pe(300, spe_transform);
    BlowingSmokeParticleEmitter bs_pe(300, spe_transform);
    // turn off at first
    bs_pe.particle_emitter.stop_emitting_particles();
    cs_pe.particle_emitter.stop_emitting_particles();
    ScriptedEvent scripted_event("assets/smoking/smoking_event.json");

    std::vector<glm::ivec4> smoke_bone_ids(4, glm::ivec4(0, 0, 0, 0));   // 4 because square
    std::vector<glm::vec4> smoke_bone_weights(4, glm::vec4(0, 0, 0, 0)); // 4 because square

    std::unordered_map<std::string, std::function<void(bool, bool)>> event_callbacks = {
        {"grab_pack",
         [&](bool first_call, bool last_call) { sound_system.queue_sound(SoundType::GRAB, glm::vec3(0.0)); }},
        {"grab_lighter",
         [&](bool first_call, bool last_call) { sound_system.queue_sound(SoundType::GRAB, glm::vec3(0.0)); }},
        {"ligher_flick_fail",
         [&](bool first_call, bool last_call) { sound_system.queue_sound(SoundType::LIGHTER_FAIL, glm::vec3(0.0)); }},
        {"lighter_flick_success",
         [&](bool first_call, bool last_call) {
             sound_system.queue_sound(SoundType::LIGHTER_SUCCESS, glm::vec3(0.0));
         }},
        {"lighter_flame",
         [&](bool first_call, bool last_call) {
             if (first_call) {
                 flame_active = true;
             }
             if (last_call) {
                 flame_active = false;
             }
         }},
        {"grab_knife",
         [&](bool first_call, bool last_call) { sound_system.queue_sound(SoundType::KNIFE_GRAB, glm::vec3(0.0)); }},
        {"knife_throw",
         [&](bool first_call, bool last_call) { sound_system.queue_sound(SoundType::WOOSH, glm::vec3(0.0)); }},
        {"couch_stab",
         [&](bool first_call, bool last_call) { sound_system.queue_sound(SoundType::STAB, glm::vec3(0.0)); }},
        {"inhale",
         [&](bool first_call, bool last_call) {
             if (first_call) {
                 sound_system.queue_sound(SoundType::CIGARETTE_BURN, glm::vec3(0.0));
                 cigarette_light_active = true;
                 cs_pe.particle_emitter.stop_emitting_particles();
             }

             if (last_call) {
                 cigarette_light_active = false;
                 cs_pe.particle_emitter.resume_emitting_particles();
                 return;
             }
         }},
        {"exhale",
         [&](bool first_call, bool last_call) {
             if (first_call) {
                 sound_system.queue_sound(SoundType::EXHALE, glm::vec3(0.0));
                 bs_pe.particle_emitter.resume_emitting_particles();
             }

             if (last_call) {
                 bs_pe.particle_emitter.stop_emitting_particles();
             }
         }},
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

        shader_cache.set_uniform(
            ShaderType::
                TEXTURE_PACKER_RIGGED_AND_ANIMATED_CWL_V_TRANSFORMATION_UBOS_1024_WITH_TEXTURES_AND_MULTIPLE_LIGHTS,
            ShaderUniformVariable::CAMERA_TO_CLIP, projection);
        shader_cache.set_uniform(
            ShaderType::
                TEXTURE_PACKER_RIGGED_AND_ANIMATED_CWL_V_TRANSFORMATION_UBOS_1024_WITH_TEXTURES_AND_MULTIPLE_LIGHTS,
            ShaderUniformVariable::WORLD_TO_CAMERA, view);

        cs_pe.particle_emitter.update(delta_time, projection * view);
        auto cs_particles = cs_pe.particle_emitter.get_particles_sorted_by_distance();

        bs_pe.particle_emitter.update(delta_time, projection * view);
        auto bs_particles = bs_pe.particle_emitter.get_particles_sorted_by_distance();

        // VVV CIG

        auto custom_transform = Transform();
        custom_transform.position = glm::vec3(.05, 0, -.05);
        auto smoke_emitter_at_cig_tip_transform =
            get_the_transform_to_attach_an_object_to_a_bone("cig_root", custom_transform, rirc);

        cs_pe.particle_emitter.transform.set_transform_matrix(smoke_emitter_at_cig_tip_transform);
        ltw_matrices[0] = smoke_emitter_at_cig_tip_transform * crosshair_transform.get_transform_matrix();

        glm::vec4 cig_light_pos = smoke_emitter_at_cig_tip_transform * glm::vec4(.05, 0, -.05, 1);
        glm::vec3 cig_light_pos_3d = glm::vec3(cig_light_pos);

        // ^^^ CIG

        // VVV MOUTH
        custom_transform = Transform();
        custom_transform.position = glm::vec3(0, 0.02, .08);
        auto smoke_emitter_at_mouth_transform =
            get_the_transform_to_attach_an_object_to_a_bone("head", custom_transform, rirc);

        bs_pe.particle_emitter.transform.set_transform_matrix(smoke_emitter_at_mouth_transform);
        /*ltw_matrices[1] = smoke_emitter_at_mouth_transform * crosshair_transform.get_transform_matrix();*/
        ltw_matrices[1] = smoke_emitter_at_mouth_transform * crosshair_transform.get_transform_matrix();
        // ^^^ MOUTH
        //
        // VVV LIGHTER
        custom_transform = Transform();
        custom_transform.position = glm::vec3(-.02, 0, .05);
        auto lighter_transform =
            get_the_transform_to_attach_an_object_to_a_bone("lighter_root", custom_transform, rirc);

        /*ltw_matrices[packed_crosshair[0].id] = lighter_transform * crosshair_transform.get_transform_matrix();*/
        /*ltw_matrices[1] = lighter_transform * crosshair_transform.get_transform_matrix();*/
        ltw_matrices[1] = lighter_transform;

        glm::vec4 lighter_flame_pos = lighter_transform * glm::vec4(-.02, 0, .02, 1);
        glm::vec3 lighter_flame_pos_3d = glm::vec3(lighter_flame_pos);
        // ^^^ LIGHTER

        if (flame_active) {
            set_shader_light_data(camera, shader_cache, true, lighter_flame_pos_3d);
        } else if (cigarette_light_active) {
            set_shader_light_data(camera, shader_cache, true, cig_light_pos_3d);
        } else {
            set_shader_light_data(camera, shader_cache, false, glm::vec3(0));
        }

        /*draw_packed_object(packed_crosshair, crosshair_transform, ltw_matrices, batcher);*/
        /*for (auto &ivptp : packed_crosshair) {*/
        /*    // hopefully the matrix at this index is an identity*/
        /*    std::vector<unsigned int> ltw_indices(ivptp.xyz_positions.size(), 1);*/
        /*    std::vector<int> ptis(ivptp.xyz_positions.size(), ivptp.packed_texture_index);*/
        /*    std::vector<glm::ivec4> blank_bone_ids(ivptp.xyz_positions.size(), glm::ivec4(0, 0, 0, 0));*/
        /*    std::vector<glm::vec4> blank_bone_weights(ivptp.xyz_positions.size(), glm::vec4(0, 0, 0, 0));*/
        /**/
        /*    batcher*/
        /*        .texture_packer_rigged_and_animated_cwl_v_transformation_ubos_1024_with_textures_and_multiple_lights_shader_batcher*/
        /*        .queue_draw(ivptp.id, ivptp.indices, ltw_indices, blank_bone_ids, blank_bone_weights, ptis,*/
        /*                    ivptp.packed_texture_coordinates, ivptp.normals, ivptp.xyz_positions);*/
        /*}*/

        /*draw_packed_object(packed_lightbulb_1, lightbulb_1_transform, ltw_matrices, batcher);*/
        /*draw_packed_object(packed_lightbulb_2, lightbulb_2_transform, ltw_matrices, batcher);*/
        /*draw_packed_object(packed_lightbulb_3, lightbulb_3_transform, ltw_matrices, batcher);*/
        /*draw_packed_object(packed_lightbulb_4, lightbulb_4_transform, ltw_matrices, batcher);*/

        /*for (auto &ivptp : packed_lightbulb) {*/
        /*    // hopefully the matrix at this index is an identity*/
        /*    std::vector<unsigned int> ltw_indices(ivptp.xyz_positions.size(), 0);*/
        /*    std::vector<int> ptis(ivptp.xyz_positions.size(), ivptp.packed_texture_index);*/
        /*    std::vector<glm::ivec4> blank_bone_ids(ivptp.xyz_positions.size(), glm::ivec4(0, 0, 0, 0));*/
        /*    std::vector<glm::vec4> blank_bone_weights(ivptp.xyz_positions.size(), glm::vec4(0, 0, 0, 0));*/
        /**/
        /*    batcher*/
        /*        .texture_packer_rigged_and_animated_cwl_v_transformation_ubos_1024_with_textures_and_multiple_lights_shader_batcher*/
        /*        .queue_draw(ivptp.id, ivptp.indices, ltw_indices, blank_bone_ids, blank_bone_weights, ptis,*/
        /*                    ivptp.packed_texture_coordinates, ivptp.normals, ivptp.xyz_positions);*/
        /*}*/

        // run scripted events

        std::vector<glm::mat4> bone_transformations;
        float animation_time_sec = glfwGetTime();
        rirc.set_bone_transforms(animation_time_sec, bone_transformations);

        const unsigned int MAX_BONES_TO_BE_USED = 100;
        ShaderProgramInfo shader_info = shader_cache.get_shader_program(
            ShaderType::
                TEXTURE_PACKER_RIGGED_AND_ANIMATED_CWL_V_TRANSFORMATION_UBOS_1024_WITH_TEXTURES_AND_MULTIPLE_LIGHTS);
        GLint location = glGetUniformLocation(
            shader_info.id, shader_cache.get_uniform_name(ShaderUniformVariable::BONE_ANIMATION_TRANSFORMS).c_str());
        glUniformMatrix4fv(location, MAX_BONES_TO_BE_USED, GL_FALSE, glm::value_ptr(bone_transformations[0]));

        for (auto &ivptr : smoke_ivptprs) {
            // Populate bone_indices and bone_weights
            std::vector<glm::ivec4> bone_indices;
            std::vector<glm::vec4> bone_weights;

            for (const auto &vertex_bone_data : ivptr.bone_data) {
                glm::ivec4 indices(static_cast<int>(vertex_bone_data.indices_of_bones_that_affect_this_vertex[0]),
                                   static_cast<int>(vertex_bone_data.indices_of_bones_that_affect_this_vertex[1]),
                                   static_cast<int>(vertex_bone_data.indices_of_bones_that_affect_this_vertex[2]),
                                   static_cast<int>(vertex_bone_data.indices_of_bones_that_affect_this_vertex[3]));

                glm::vec4 weights(vertex_bone_data.weight_value_of_this_vertex_wrt_bone[0],
                                  vertex_bone_data.weight_value_of_this_vertex_wrt_bone[1],
                                  vertex_bone_data.weight_value_of_this_vertex_wrt_bone[2],
                                  vertex_bone_data.weight_value_of_this_vertex_wrt_bone[3]);

                bone_indices.push_back(indices);
                bone_weights.push_back(weights);
            }

            std::vector<int> packed_texture_indices(ivptr.xyz_positions.size(), ivptr.packed_texture_index);

            std::vector<unsigned int> ltw_indices(ivptr.xyz_positions.size(), ivptr.id);
            batcher
                .texture_packer_rigged_and_animated_cwl_v_transformation_ubos_1024_with_textures_and_multiple_lights_shader_batcher
                .queue_draw(ivptr.id, ivptr.indices, ltw_indices, bone_indices, bone_weights, packed_texture_indices,
                            ivptr.packed_texture_coordinates, ivptr.normals, ivptr.xyz_positions);
        }

        for (size_t i = 0; i < cs_particles.size(); ++i) {

            auto &curr_particle = cs_particles[i];

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

            // temporary
            ltw_matrices[curr_particle.id] = transform;

            if (curr_particle.is_alive()) {

                auto nv = generate_rectangle_vertices_3d(curr_particle.transform.position,
                                                         camera.transform.compute_right_vector(),
                                                         camera.transform.compute_up_vector(), 1, 1);

                std::vector<unsigned int> smoke_ltw_mat_idxs(4, curr_particle.id);
                batcher
                    .texture_packer_rigged_and_animated_cwl_v_transformation_ubos_1024_with_textures_and_multiple_lights_shader_batcher
                    .queue_draw(curr_particle.id, smoke_indices, smoke_ltw_mat_idxs, smoke_bone_ids, smoke_bone_weights,
                                smoke_pt_idxs, smoke_texture_coordinates, flame_normals, smoke_vertices);
            }
        }

        for (size_t i = 0; i < bs_particles.size(); ++i) {

            auto &curr_particle = bs_particles[i];

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

            // temporary
            ltw_matrices[curr_particle.id] = transform;

            if (curr_particle.is_alive()) {

                auto nv = generate_rectangle_vertices_3d(curr_particle.transform.position,
                                                         camera.transform.compute_right_vector(),
                                                         camera.transform.compute_up_vector(), 1, 1);

                std::vector<unsigned int> smoke_ltw_mat_idxs(4, curr_particle.id);
                batcher
                    .texture_packer_rigged_and_animated_cwl_v_transformation_ubos_1024_with_textures_and_multiple_lights_shader_batcher
                    .queue_draw(curr_particle.id, smoke_indices, smoke_ltw_mat_idxs, smoke_bone_ids, smoke_bone_weights,
                                smoke_pt_idxs, smoke_texture_coordinates, flame_normals, smoke_vertices);
            }
        }

        /*if (flame_active) {*/
        if (true) {
            double ms_curr_time = glfwGetTime() * 1000;

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
            std::vector<unsigned int> ltw_mat_idxs(4, 1);

            batcher
                .texture_packer_rigged_and_animated_cwl_v_transformation_ubos_1024_with_textures_and_multiple_lights_shader_batcher
                .queue_draw(curr_obj_id, flame_indices, ltw_mat_idxs, smoke_bone_ids, smoke_bone_weights,
                            packed_texture_indices, packed_tex_coords, flame_normals, flame_vertices);
        }

        double curr_time_sec = glfwGetTime();
        scripted_event.run_scripted_events(curr_time_sec, event_callbacks);

        batcher
            .texture_packer_rigged_and_animated_cwl_v_transformation_ubos_1024_with_textures_and_multiple_lights_shader_batcher
            .draw_everything();

        sound_system.play_all_sounds();

        // load in the matrices
        glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ltw_matrices), ltw_matrices);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        /*batcher.texture_packer_cwl_v_transformation_ubos_1024_multiple_lights_shader_batcher.draw_everything();*/
        // -------------------

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}

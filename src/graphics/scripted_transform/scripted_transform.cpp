#include "scripted_transform.hpp"

ScriptedTransform::ScriptedTransform(std::vector<ScriptedTransformKeyframe> keyframes, double ms_start_time,
                                     double ms_end_time, double tau_position, double tau_rotation, double tau_scale)
    : transform{}, keyframes{keyframes}, coef_matrices_position{}, coef_matrices_rotation{}, coef_matrices_scale{},
      cummulative_arc_lengths_position{}, cummulative_arc_lengths_rotation{}, cummulative_arc_lengths_scale{},
      ms_start_time{ms_start_time}, ms_end_time{ms_end_time} {

    if (keyframes.size() < 4) {
        throw std::runtime_error("ScriptedTransform needs at least 4 control points!");
    }

    // clang-format off
    glm::mat4 basis_matrix_position = glm::mat4(               0,                1,                    0,             0,
                                                   -tau_position,                0,         tau_position,             0,
                                                2 * tau_position, tau_position - 3, 3 - 2 * tau_position, -tau_position,
                                                   -tau_position, 2 - tau_position,     tau_position - 2,  tau_position);  // arguments are given as column-major!
    glm::mat4 basis_matrix_rotation = glm::mat4(               0,                1,                    0,             0,
                                                   -tau_rotation,                0,         tau_rotation,             0,
                                                2 * tau_rotation, tau_rotation - 3, 3 - 2 * tau_rotation, -tau_rotation,
                                                   -tau_rotation, 2 - tau_rotation,     tau_rotation - 2,  tau_rotation);
    glm::mat4 basis_matrix_scale = glm::mat4(            0,             1,                 0,          0,
                                                -tau_scale,             0,         tau_scale,          0,
                                             2 * tau_scale, tau_scale - 3, 3 - 2 * tau_scale, -tau_scale,
                                                -tau_scale, 2 - tau_scale,     tau_scale - 2,  tau_scale);
    // clang-format on

    double cummulative_arc_length_position = 0.0;
    double cummulative_arc_length_rotation = 0.0;
    double cummulative_arc_length_scale = 0.0;
    cummulative_arc_lengths_position.push_back(cummulative_arc_length_position);
    cummulative_arc_lengths_rotation.push_back(cummulative_arc_length_rotation);
    cummulative_arc_lengths_scale.push_back(cummulative_arc_length_scale);

    for (int i = 1; i < keyframes.size() - 2; i++) {
        // clang-format off
        glm::mat4x3 control_matrix_position = glm::mat4x3(keyframes[i - 1].position.x, keyframes[i - 1].position.y, keyframes[i - 1].position.z,
                                                          keyframes[i + 0].position.x, keyframes[i + 0].position.y, keyframes[i + 0].position.z,
                                                          keyframes[i + 1].position.x, keyframes[i + 1].position.y, keyframes[i + 1].position.z,
                                                          keyframes[i + 2].position.x, keyframes[i + 2].position.y, keyframes[i + 2].position.z);
        glm::mat4x3 control_matrix_rotation = glm::mat4x3(keyframes[i - 1].rotation.x, keyframes[i - 1].rotation.y, keyframes[i - 1].rotation.z,
                                                          keyframes[i + 0].rotation.x, keyframes[i + 0].rotation.y, keyframes[i + 0].rotation.z,
                                                          keyframes[i + 1].rotation.x, keyframes[i + 1].rotation.y, keyframes[i + 1].rotation.z,
                                                          keyframes[i + 2].rotation.x, keyframes[i + 2].rotation.y, keyframes[i + 2].rotation.z);
        glm::mat4x3 control_matrix_scale = glm::mat4x3(keyframes[i - 1].scale.x, keyframes[i - 1].scale.y, keyframes[i - 1].scale.z,
                                                       keyframes[i + 0].scale.x, keyframes[i + 0].scale.y, keyframes[i + 0].scale.z,
                                                       keyframes[i + 1].scale.x, keyframes[i + 1].scale.y, keyframes[i + 1].scale.z,
                                                       keyframes[i + 2].scale.x, keyframes[i + 2].scale.y, keyframes[i + 2].scale.z);
        // clang-format on

        glm::mat4x3 coef_matrix_position = control_matrix_position * basis_matrix_position;
        glm::mat4x3 coef_matrix_rotation = control_matrix_rotation * basis_matrix_rotation;
        glm::mat4x3 coef_matrix_scale = control_matrix_scale * basis_matrix_scale;
        coef_matrices_position.push_back(coef_matrix_position);
        coef_matrices_rotation.push_back(coef_matrix_rotation);
        coef_matrices_scale.push_back(coef_matrix_scale);

        double t = 0.0;
        double dt = 0.01;
        while (t < 1.0) {
            double dxdt = 3 * coef_matrix_position[0][3] * t * t + 2 * coef_matrix_position[0][2] * t + coef_matrix_position[0][1];
            double dydt = 3 * coef_matrix_position[1][3] * t * t + 2 * coef_matrix_position[1][2] * t + coef_matrix_position[1][1];
            double dzdt = 3 * coef_matrix_position[2][3] * t * t + 2 * coef_matrix_position[2][2] * t + coef_matrix_position[2][1];

            cummulative_arc_length_position += glm::sqrt(dxdt * dxdt + dydt * dydt + dzdt * dzdt) * dt;

            t += dt;
        }

        cummulative_arc_lengths_position.push_back(cummulative_arc_length_position);
    }
}

void ScriptedTransform::update(double ms_curr_time) {
    if (ms_curr_time <= ms_start_time) {
        transform.position = keyframes[1].position;
        transform.rotation = keyframes[1].rotation;
        transform.scale = keyframes[1].scale;
        return;
    }

    if (ms_curr_time >= ms_end_time) {
        transform.position = keyframes[keyframes.size() - 2].position;
        transform.rotation = keyframes[keyframes.size() - 2].rotation;
        transform.scale = keyframes[keyframes.size() - 2].scale;
        return;
    }

    double total_arc_length = cummulative_arc_lengths_position[cummulative_arc_lengths_position.size() - 1];
    double curr_arc_length = total_arc_length * (ms_curr_time - ms_start_time) / (ms_end_time - ms_start_time);

    int i = 0;
    while (cummulative_arc_lengths_position[i + 1] < curr_arc_length)
        i++;

    float t =
        (curr_arc_length - cummulative_arc_lengths_position[i]) / (cummulative_arc_lengths_position[i + 1] - cummulative_arc_lengths_position[i]);

    transform.position = coef_matrices_position[i] * glm::vec4(1, t, t * t, t * t * t);
    transform.rotation = coef_matrices_rotation[i] * glm::vec4(1, t, t * t, t * t * t); // keyframes[i + 1].rotation + t * (keyframes[i + 2].rotation - keyframes[i + 1].rotation);
    transform.scale = coef_matrices_scale[i] * glm::vec4(1, t, t * t, t * t * t); // keyframes[i + 1].scale + t * (keyframes[i + 2].scale - keyframes[i + 1].scale);
}

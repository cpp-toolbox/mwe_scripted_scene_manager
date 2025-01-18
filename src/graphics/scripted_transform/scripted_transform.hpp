#ifndef SCRIPTED_TRANSFORM_HPP
#define SCRIPTED_TRANSFORM_HPP

#include "sbpt_generated_includes.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

struct ScriptedTransformKeyframe {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

class ScriptedTransform {
  public:
    ScriptedTransform(std::vector<ScriptedTransformKeyframe> keyframes, double ms_start_time, double ms_end_time,
                      double tau_position = 0.5, double tau_rotation = 0.5, double tau_scale = 0.5);

    /**
     * transform.position: Catmull-Rom interpolation
     * transform.rotation: linear interpolation
     * transform.scale: linear interpolation
     */
    void update(double ms_curr_time);

    Transform transform;

  private:
    std::vector<ScriptedTransformKeyframe> keyframes;
    std::vector<glm::mat4x3> coef_matrices_position;
    std::vector<glm::mat4x3> coef_matrices_rotation;
    std::vector<glm::mat4x3> coef_matrices_scale;
    std::vector<double> cummulative_arc_lengths_position; // arc length traversed until i-th control point
    std::vector<double> cummulative_arc_lengths_rotation; // arc length traversed until i-th control point
    std::vector<double> cummulative_arc_lengths_scale;    // arc length traversed until i-th control point
    double ms_start_time;
    double ms_end_time;
};

#endif // SCRIPTED_TRANSFORM_HPP

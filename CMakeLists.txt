cmake_minimum_required(VERSION 3.10)
project(mwe_scripted_scene_manager)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE SOURCES "src/*.cpp")

# Add the main executable
add_executable(${PROJECT_NAME} ${SOURCES})

find_package(glad)
find_package(glfw3)
find_package(stb)
find_package(glm)
find_package(nlohmann_json)
find_package(spdlog)
find_package(SndFile)
find_package(OpenAL)
target_link_libraries(${PROJECT_NAME} glad::glad glfw stb::stb glm::glm nlohmann_json::nlohmann_json spdlog::spdlog SndFile::sndfile OpenAL::OpenAL)

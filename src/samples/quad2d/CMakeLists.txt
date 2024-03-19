if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    include_directories(${CMAKE_SOURCE_DIR}/external/glfw/include)
    link_directories(${CMAKE_SOURCE_DIR}/external/glfw)
else()
    find_package(glfw3 REQUIRED)
    include_directories(${GLFW_INCLUDE_DIRS})
endif()

set(RENDER_SOURCE
        ../../render/quad_renderer.cpp
        quad2d_render.cpp
        render_init.cpp
        update.cpp
        draw.cpp
        present.cpp)

add_executable(quad_renderer main.cpp ../../utils/glfw_window.cpp ${VK_UTILS_SRC} ${RENDER_SOURCE} ${IMGUI_SRC})

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set_target_properties(quad_renderer PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

    target_link_libraries(quad_renderer PRIVATE project_options
                          etna glfw3 project_warnings)
else()
    target_link_libraries(quad_renderer PRIVATE project_options
                          etna glfw project_warnings)
endif()
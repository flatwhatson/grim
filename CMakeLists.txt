cmake_minimum_required(VERSION 3.10)

project(grim CXX)

option(USE_CCACHE "Use compiler cache" on)

########################################
# Compiler configuration

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
  set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g")
endif()

if(USE_CCACHE)
  find_program(CCACHE_COMMAND ccache)
  if(CCACHE_COMMAND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_COMMAND})
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_COMMAND})
  else()
    message(WARNING "Couldn't find ccache executable")
  endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

########################################
# Dependencies

find_package(PkgConfig REQUIRED)
pkg_search_module(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_search_module(Guile REQUIRED IMPORTED_TARGET guile-3.0)

find_package(GLEW REQUIRED)
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

set(imgui_dir "${CMAKE_SOURCE_DIR}/external/imgui")
file(GLOB imgui_sources "${imgui_dir}/*.cpp")
list(APPEND imgui_sources "${imgui_dir}/examples/imgui_impl_sdl.cpp")
list(APPEND imgui_sources "${imgui_dir}/examples/imgui_impl_opengl3.cpp")

add_library(imgui STATIC ${imgui_sources})
target_include_directories(imgui PUBLIC ${imgui_dir})
target_link_libraries(imgui PkgConfig::SDL2 OpenGL::GL)

########################################
# Targets

set(grim_dir "${CMAKE_SOURCE_DIR}/src")
file(GLOB grim_sources "${grim_dir}/*.cpp")

add_executable(grim ${grim_sources})
target_include_directories(grim PUBLIC ${grim_dir})
target_link_libraries(grim imgui PkgConfig::Guile GLEW::GLEW)

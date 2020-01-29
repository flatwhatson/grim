#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#include <SDL.h>
#include <GL/glew.h>
#include <libguile.h>

#include <cstdio>
#include <memory>
#include <deque>

static const int gl_major_version = 3;
static const int gl_minor_version = 0;
static const char* glsl_version = "#version 130";

static const char* window_title = "grim";
static const int window_width = 1280;
static const int window_height = 720;

static const ImVec4 background_color = { 0.45f, 0.55f, 0.60f, 1.00f };
static const ImVec4 input_color = { 0xcc/255.0f, 0x66/255.0f, 0x66/255.0f, 1.00f };
static const ImVec4 output_color = { 0xb5/255.0f, 0xbd/255.0f, 0x68/255.0f, 1.00f };

////////////////////////////////////////

struct free_deleter {
  template<class T>
  void operator()(T* ptr) {
    std::free(ptr);
  }
};

template<class T>
using unique_c_ptr = std::unique_ptr<T, free_deleter>;
using unique_c_str = unique_c_ptr<char>;

enum class entry_type {
  input,
  output,
};

struct entry {
  entry(entry_type type, unique_c_str&& text)
      : type(type), text(std::move(text)) {}

  entry_type type;
  unique_c_str text;
};

static std::deque<entry> history;
static std::string user_input;

////////////////////////////////////////

SCM grim_scm_read(const char* input) {
  SCM string = scm_from_locale_string(input);
  SCM port = scm_open_input_string(string);
  SCM expr = scm_read(port);
  scm_close_input_port(port);
  return expr;
}

unique_c_str grim_scm_write(SCM expr) {
  SCM port = scm_open_output_string();
  scm_write(expr, port);
  SCM string = scm_get_output_string(port);
  scm_close_output_port(port);
  char* output = scm_to_locale_string(string);
  return unique_c_str(output);
}

void grim_scm_eval(const char* input) {
  SCM expr = grim_scm_read(input);
  history.emplace_back(entry_type::input, grim_scm_write(expr));
  SCM result = scm_eval(expr, scm_current_module());
  history.emplace_back(entry_type::output, grim_scm_write(result));
}

////////////////////////////////////////

void init_scheme() {
  // TODO
}

void run_frame() {
  ImGui::Begin("Hello, scheme!");

  for (auto& entry : history) {
    switch (entry.type) {
      case entry_type::input:
        ImGui::TextColored(input_color, "%s", entry.text.get());
        break;
      case entry_type::output:
        ImGui::TextColored(output_color, "%s", entry.text.get());
        break;
    }
  }

  if (ImGui::InputText("<<<", &user_input, ImGuiInputTextFlags_EnterReturnsTrue)) {
    grim_scm_eval(user_input.c_str());
    user_input.clear();
  }

  ImGui::End();
}

int real_main(int /*argc*/, char** /*argv*/) {
  init_scheme();

  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    fprintf(stderr, "Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

  if (!window) {
    fprintf(stderr, "Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_major_version);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_minor_version);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);

  if (!gl_context) {
    fprintf(stderr, "Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);

  auto glew_result = glewInit();
  if (glew_result != GLEW_OK) {
    fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_result));
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  //ImGui::StyleColorsClassic();
  //ImGui::StyleColorsLight();

  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  bool done = false;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
        done = true;
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    run_frame();

    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(background_color.x, background_color.y, background_color.z, background_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

void main2(void*, int argc, char** argv) {
  exit(real_main(argc, argv));
}

int main(int argc, char** argv) {
  scm_boot_guile(argc, argv, main2, nullptr);
}

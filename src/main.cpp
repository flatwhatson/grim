#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#include <SDL.h>
#include <GL/glew.h>
#include <libguile.h>

#include <cstdio>
#include <deque>
#include <functional>
#include <string>
#include <string_view>

static const int gl_major_version = 3;
static const int gl_minor_version = 0;
static const char* glsl_version = "#version 130";

static const char* window_title = "grim";
static const int window_width = 1280;
static const int window_height = 720;

static const ImVec4 background_color = { 0.45f, 0.55f, 0.60f, 1.00f };
static const ImVec4 error_color = ImColor(0xcc, 0x66, 0x66);
static const ImVec4 output_color = ImColor(0xb5, 0xbd, 0x68);

////////////////////////////////////////

enum class entry_type {
  input,
  output,
  error,
};

struct entry {
  entry(entry_type type, std::string&& text)
      : type(type), text(std::move(text)) {}

  entry_type type;
  std::string text;
};

static std::deque<entry> history;
static std::string user_input;

////////////////////////////////////////

using grim_scm_t_thunk   = std::function<void()>;
using grim_scm_t_handler = std::function<void(SCM, SCM)>;

SCM grim_scm_wrap_thunk(void* data) {
  (*static_cast<grim_scm_t_thunk*>(data))();
  return SCM_UNSPECIFIED;
}

SCM grim_scm_wrap_handler(void* data, SCM tag, SCM args) {
  (*static_cast<grim_scm_t_handler*>(data))(tag, args);
  return SCM_UNSPECIFIED;
}

SCM grim_scm_try_catch(const grim_scm_t_thunk& thunk, const grim_scm_t_handler& handler) {
  return scm_c_catch(SCM_BOOL_T,
      grim_scm_wrap_thunk, const_cast<void*>(static_cast<const void*>(&thunk)),
      grim_scm_wrap_handler, const_cast<void*>(static_cast<const void*>(&handler)),
      nullptr, nullptr);
}

std::string grim_scm_get_output_string(SCM port) {
  SCM string = scm_get_output_string(port);
  scm_close_output_port(port);
  auto outptr = scm_to_locale_string(string);
  auto outstr = std::string(outptr);
  free(outptr);
  return outstr;
}

SCM grim_scm_read(std::string_view input) {
  SCM string = scm_from_locale_stringn(input.data(), input.size());
  SCM port = scm_open_input_string(string);
  SCM expr = scm_read(port);
  scm_close_input_port(port);
  return expr;
}

std::string grim_scm_write(SCM expr) {
  SCM port = scm_open_output_string();
  scm_write(expr, port);
  return grim_scm_get_output_string(port);
}

std::string grim_scm_print_exception(SCM tag, SCM args) {
  SCM port = scm_open_output_string();
  scm_print_exception(port, SCM_BOOL_F, tag, args);
  return grim_scm_get_output_string(port);
}

void grim_scm_eval(std::string_view input) {
  history.emplace_back(entry_type::input, std::string(input));
  grim_scm_try_catch([&] {
    SCM expr = grim_scm_read(input);
    SCM result = scm_eval(expr, scm_current_module());
    history.emplace_back(entry_type::output, grim_scm_write(result));
  }, [&](SCM tag, SCM args) {
    history.emplace_back(entry_type::error, grim_scm_print_exception(tag, args));
  });
}

////////////////////////////////////////

void init_scheme() {
  // TODO
}

void run_frame() {
  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  ImGui::Begin("Scheme Console");

  const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
  ImGui::BeginChild("History", ImVec2(0, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

  for (auto& entry : history) {
    const ImVec4* color = nullptr;
    const char* format = nullptr;

    switch (entry.type) {
      case entry_type::input:
        break;
      case entry_type::output:
        color = &output_color;
        break;
      case entry_type::error:
        color = &error_color;
        break;
    }

    ImGui::PushTextWrapPos();
    if (color)
      ImGui::PushStyleColor(ImGuiCol_Text, *color);

    if (format) {
      ImGui::Text(format, entry.text.c_str());
    } else {
      ImGui::TextUnformatted(entry.text.data(), entry.text.data() + entry.text.size());
    }

    ImGui::PopTextWrapPos();
    if (color)
      ImGui::PopStyleColor();
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();
  ImGui::Separator();

  bool reclaim_focus = false;
  if (ImGui::InputText("Input", &user_input, ImGuiInputTextFlags_EnterReturnsTrue)) {
    grim_scm_eval(user_input);
    user_input.clear();
    reclaim_focus = true;
  }

  ImGui::SetItemDefaultFocus();
  if (reclaim_focus)
    ImGui::SetKeyboardFocusHere(-1);

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

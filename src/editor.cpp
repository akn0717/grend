#include <grend/main_logic.hpp>

#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include <imgui/examples/imgui_impl_opengl3.h>

using namespace grendx;

void game_editor::update_models(engine *renderer) {
	const gl_manager& glman = renderer->get_glman();
	edit_model = glman.cooked_models.begin();
}

void game_editor::handle_editor_input(engine *renderer,
                                      context& ctx,
                                      SDL_Event& ev)
{
	const gl_manager& glman = renderer->get_glman();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplSDL2_ProcessEvent(&ev);

	if (!io.WantCaptureKeyboard) {
		if (ev.type == SDL_KEYDOWN) {
			switch (ev.key.keysym.sym) {
				case SDLK_w: cam.velocity.z =  movement_speed; break;
				case SDLK_s: cam.velocity.z = -movement_speed; break;
				case SDLK_a: cam.velocity.x =  movement_speed; break;
				case SDLK_d: cam.velocity.x = -movement_speed; break;
				case SDLK_q: cam.velocity.y =  movement_speed; break;
				case SDLK_e: cam.velocity.y = -movement_speed; break;

				//case SDLK_m: in_edit_mode = !in_edit_mode; break;
				case SDLK_m: mode = mode::Inactive; break;
				case SDLK_i: load_map(); break;
				case SDLK_o: save_map(); break;

				case SDLK_g:
					//edit_rotation -= M_PI/4;
					edit_transform *= glm::rotate((float)-M_PI/4.f, glm::vec3(1, 0, 0));
					break;

				case SDLK_h:
					//edit_rotation -= M_PI/4;
					edit_transform *= glm::rotate((float)M_PI/4.f, glm::vec3(1, 0, 0));
					break;

				case SDLK_z:
					//edit_rotation -= M_PI/4;
					edit_transform *= glm::rotate((float)-M_PI/4.f, glm::vec3(0, 1, 0));
					break;

				case SDLK_c:
					//edit_rotation += M_PI/4;
					edit_transform *= glm::rotate((float)M_PI/4.f, glm::vec3(0, 1, 0));
					break;

				case SDLK_x:
					// flip horizontally (along the X axis)
					edit_transform *= glm::scale(glm::vec3(-1, 1, 1));
					edit_inverted = !edit_inverted;
					break;

				case SDLK_b:
					// flip horizontally (along the Z axis)
					edit_transform *= glm::scale(glm::vec3( 1, 1, -1));
					edit_inverted = !edit_inverted;
					break;

				case SDLK_v:
					// flip vertically
					edit_transform *= glm::scale(glm::vec3( 1, -1, 1));
					edit_inverted = !edit_inverted;
					break;

				case SDLK_j:
					// scale down
					edit_transform *= glm::scale(glm::vec3(0.9));
					break;

				case SDLK_k:
					// scale up
					edit_transform *= glm::scale(glm::vec3(1/0.9));
					break;

				case SDLK_r:
					if (edit_model == glman.cooked_models.begin()) {
					    edit_model = glman.cooked_models.end();
					}
					edit_model--;
					break;

				case SDLK_f:
					edit_model++;
					if (edit_model == glman.cooked_models.end()) {
					    edit_model = glman.cooked_models.begin();
					}
					break;

				case SDLK_DELETE:
					// undo, basically
					if (!dynamic_models.empty()) {
					    dynamic_models.pop_back();
					}
					break;
			}
		}

		else if (ev.type == SDL_KEYUP) {
			switch (ev.key.keysym.sym) {
				case SDLK_w:
				case SDLK_s:
					cam.velocity.z = 0;
					break;

				case SDLK_a:
				case SDLK_d:
					cam.velocity.x = 0;
					break;

				case SDLK_q:
				case SDLK_e:
					cam.velocity.y = 0;
					break;
			}
		}
	}

	if (!io.WantCaptureMouse) {
		int x, y;
		Uint32 buttons = SDL_GetMouseState(&x, &y); (void)buttons;

		if (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
			// TODO: generic functions to do SO(3) rotation (does glm do it?
			//       probably, also maybe should be quarternions...)
			int win_x, win_y;
			SDL_GetWindowSize(ctx.window, &win_x, &win_y);

			x = (x > 0)? x : win_x/2;
			y = (x > 0)? y : win_y/2;

			// TODO: get window resolution
			float center_x = (float)win_x / 2;
			float center_y = (float)win_y / 2;

			float rel_x = ((float)x - center_x) / center_x;
			float rel_y = ((float)y - center_y) / center_y;

			cam.set_direction(glm::vec3(
				movement_speed * sin(rel_x*2*M_PI),
				movement_speed * sin(-rel_y*M_PI/2.f),
				movement_speed * -cos(rel_x*2*M_PI)
			));
		}

		if (ev.type == SDL_MOUSEWHEEL) {
			edit_distance -= ev.wheel.y/10.f /* scroll sensitivity */;
		}

		else if (ev.type == SDL_MOUSEBUTTONDOWN) {
			if (ev.button.button == SDL_BUTTON_LEFT) {
				dynamic_models.push_back({
					edit_model->first,
					edit_position,
					edit_transform,
					edit_inverted,
				});
			}
		}

		// TODO: snap to increments
		auto align = [&] (float x) { return floor(x * fidelity)/fidelity; };
		edit_position = glm::vec3(
			align(cam.direction.x*edit_distance + cam.position.x),
			align(cam.direction.y*edit_distance + cam.position.y),
			align(cam.direction.z*edit_distance + cam.position.z));
	}
}

void game_editor::logic(context& ctx, float delta) {
	// TODO: should time-dependent position updates be part of the camera
	//       class? (probably)
	cam.position += cam.velocity.z*cam.direction*delta;
	cam.position += cam.velocity.y*cam.up*delta;
	cam.position += cam.velocity.x*cam.right*delta;
}

void game_editor::menubar(void) {
	static bool demo_window = false;

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open", "CTRL+O")) {}
			if (ImGui::MenuItem("Save", "CTRL+S")) {}
			if (ImGui::MenuItem("Save As", "Shift+CTRL+S")) {}
			if (ImGui::MenuItem("Close", "CTRL+O")) {}

			ImGui::Separator();
			if (ImGui::BeginMenu("Import")) {
				if (ImGui::MenuItem("Import .obj model")) {}
				if (ImGui::MenuItem("Import .gltf models")) {}
				if (ImGui::MenuItem("Import .gltf scene")) {}
				if (ImGui::MenuItem("Import .map scene")) {}
				if (ImGui::MenuItem("Load .gltf scene as current")) {}
				ImGui::EndMenu();
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Reload shaders")) {}

			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "CTRL+Q")) {}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
			if (ImGui::MenuItem("Redo", "CTRL+Y")) {}

			ImGui::Separator();
			if (ImGui::MenuItem("Reset+Regenerate physics", "CTRL+P")) {}

			ImGui::Separator();
			if (ImGui::MenuItem("asdf", "CTRL+V")) {}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Options")) {
			ImGui::SliderFloat("Snap increment", &fidelity,
			                  0.5f, 50.f, "%.1f");
			ImGui::SliderFloat("Movement speed", &movement_speed,
			                  1.f, 100.f, "%.1f");
			ImGui::SliderFloat("Exposure (tonemapping)", &exposure, 0.1, 10.f);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Tools")) {
			if (ImGui::MenuItem("Objects editor", "o"))
				show_map_window = true;
			if (ImGui::MenuItem("Lights editor", "l"))
				show_lights_window = true;

			if (ImGui::MenuItem("Material editor", "CTRL+M")) {}

			ImGui::Separator();
			if (ImGui::MenuItem("Add object", "r")) {}
			if (ImGui::MenuItem("Scale object", "s")) {}

			ImGui::Separator();
			if (ImGui::MenuItem("Bake lighting", "CTRL+B")) {}
			if (ImGui::MenuItem("Generate environment light probes", "CTRL+L")) {}
			if (ImGui::MenuItem("Generate IBL cubemaps", "Shift-CTRL+L")) {}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug")) {
			ImGui::MenuItem("Dear ImGui demo window", NULL, &demo_window);
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (demo_window) {
		ImGui::ShowDemoWindow(&demo_window);
	}
}

// TODO: rename 'engine' to 'renderer' or something
void game_editor::render_imgui(engine *renderer, context& ctx) {
	if (mode != mode::Inactive) {
		menubar();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}

void game_editor::map_window(engine *renderer, context& ctx) {
	glm::vec3 dummy;

	ImGui::Begin("Objects", &show_map_window);
	ImGui::Text("Object properties");

	ImGui::Columns(4, NULL, false);
	for (auto& s : {"X", "Y", "Z"}) {
		ImGui::Text(s);
		ImGui::NextColumn();
	}
	ImGui::Columns(1);

	ImGui::InputFloat3("position", glm::value_ptr(edit_position));
	ImGui::InputFloat3("scale", glm::value_ptr(dummy));
	ImGui::InputFloat3("rotation (degrees)", glm::value_ptr(dummy));
	ImGui::End();
}

void game_editor::lights_window(engine *renderer, context& ctx) {
	ImGui::Begin("Lights", &show_lights_window);
	ImGui::Columns(2);

	for (int i = 0; i < renderer->active_lights; i++) {
		std::string name = "Light " + std::to_string(i);

		if (ImGui::Selectable(name.c_str(), selected_light == i)) {
			// TODO: maybe not this
			selected_light = i;
		}

		ImGui::NextColumn();
		ImGui::InputFloat3("position", glm::value_ptr(renderer->lights[i].position));

		if (selected_light == i) {
			ImGui::ColorEdit4("color", glm::value_ptr(renderer->lights[i].diffuse));
			ImGui::SliderFloat("intensity", &renderer->lights[i].intensity,
			                   0.f, 1000.f);
			ImGui::SliderFloat("radius", &renderer->lights[i].radius,
			                   0.01f, 10.f);
			ImGui::SliderFloat("directional", &renderer->lights[i].position.w,
			                   0.f, 1.f);

			renderer->lights[i].changed = true;
		}
		ImGui::NextColumn();


	}
	ImGui::Columns(1);
	ImGui::Separator();

	if (ImGui::Button("Add Light")) {

	}

	ImGui::SameLine();
	if (ImGui::Button("Delete Light")) {

	}

	ImGui::End();
}

void game_editor::map_models(engine *renderer, context& ctx) {
	for (auto& v : dynamic_models) {
		glm::mat4 trans = glm::translate(v.position) * v.transform;

		glFrontFace(v.inverted? GL_CW : GL_CCW);
		renderer->dqueue_draw_model(v.name, trans);
		DO_ERROR_CHECK();
	}
}

void game_editor::render_editor(engine *renderer, context& ctx) {
	if (mode != mode::Inactive) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(ctx.window);
		ImGui::NewFrame();

		if (show_map_window) {
			glm::mat4 trans = glm::translate(edit_position) * edit_transform;

			glFrontFace(edit_inverted? GL_CW : GL_CCW);
			renderer->draw_model_lines(edit_model->first, trans);
			renderer->draw_model(edit_model->first, trans);
			DO_ERROR_CHECK();

			map_window(renderer, ctx);
		}

		if (show_lights_window) {
			for (unsigned i = 0; i < renderer->active_lights; i++) {
				// TODO: don't directly access lights here
				auto& v = renderer->lights[i];

				glm::mat4 trans =
					glm::translate(glm::vec3(v.position))
					* glm::scale(glm::vec3(renderer->lights[i].radius));

				renderer->dqueue_draw_model("smoothsphere", trans);
			}

			lights_window(renderer, ctx);
		}
	}

	map_models(renderer, ctx);
}

void game_editor::load_map(std::string name) {
	std::ifstream foo(name);
	std::cerr << "loading map " << name << std::endl;

	if (!foo.good()) {
		std::cerr << "couldn't open save file" << name << std::endl;
		return;
	}

	std::string line;
	while (std::getline(foo, line)) {
		auto statement = split_string(line, '\t');
		if (line[0] == '#' || line[0] == '\n') {
			continue;
		}

		if (statement[0] == "entity" && statement.size() >= 5) {
			auto posvec = split_string(statement[2], ',');
			auto matvec = split_string(statement[3], ',');

			editor_entry v;
			v.name = statement[1];
			v.position = glm::vec3(std::stof(posvec[0]), std::stof(posvec[1]), std::stof(posvec[2]));
			v.inverted = std::stoi(statement[4]);

			for (unsigned i = 0; i < 16; i++) {
				v.transform[i/4][i%4] = std::stof(matvec[i]);
			}

			dynamic_models.push_back(v);
			std::cerr << "# loaded a " << v.name << std::endl;
		}
	}
}

void game_editor::save_map(std::string name) {
	std::ofstream foo(name);
	std::cerr << "saving map " << name << std::endl;

	if (!foo.good()) {
		std::cerr << "couldn't open save file" << name << std::endl;
		return;
	}

	foo << "### test scene save file" << std::endl;

	/*
	for (std::string& lib : libraries) {
		foo << "library\t" << lib << std::endl;
	}
	*/

	for (auto& v : dynamic_models) {
		foo << "entity\t" << v.name << "\t"
			<< v.position.x << "," << v.position.y << "," << v.position.z << "\t";

		for (unsigned y = 0; y < 4; y++) {
			for (unsigned x = 0; x < 4; x++) {
				foo << v.transform[y][x] << ",";
			}
		}

		foo << "\t" << v.inverted;
		foo << std::endl;
	}
}

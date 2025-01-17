#include <grend/gameEditor.hpp>
#include <grend/utility.hpp>
#include <grend/fileDialog.hpp>

#include <grend/ecs/ecs.hpp>
#include <grend/ecs/serializer.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <fstream>
#include <string.h>

using namespace grendx;

static char searchBuffer[0x1000] = "";
static fileDialog export_entity_dialog("Export entity");

static void drawSelectableLabel(const char *txt) {
	if (ImGui::Selectable(txt)) {
		if (*searchBuffer) {
			// add a space if the search buffer already has contents
			strncat(searchBuffer, " ", sizeof(searchBuffer) - 1);
		}

		strncat(searchBuffer, txt, sizeof(searchBuffer) - 1);
	}
}

static void drawJson(nlohmann::json& value, const std::string& path = ".") {
	static ImGuiTreeNodeFlags flags
		= ImGuiTreeNodeFlags_DefaultOpen;
		/*
		= ImGuiTreeNodeFlags_Bullet
		| ImGuiTreeNodeFlags_DefaultOpen
		*/
		/*
		| ImGuiTreeNodeFlags_OpenOnArrow
		| ImGuiTreeNodeFlags_OpenOnDoubleClick
		| ImGuiTreeNodeFlags_SpanAvailWidth
		*/
		;

	if (value.is_array()) {
		for (unsigned i = 0; i < value.size(); i++) {
			std::string name = "[" + std::to_string(i) + "]";

			if (ImGui::TreeNodeEx(name.c_str(), flags)) {
				drawJson(value[i], path + name);
				ImGui::TreePop();
			}
		}
	}

	else if (value.is_object()) {
		for (auto& [name, em] : value.items()) {
			std::string temp = path + ":" + name;
			if (ImGui::TreeNodeEx(temp.c_str(), flags)) {
				drawJson(em, path + ":" + name);
				ImGui::TreePop();
			}
		}
	}

	else if (value.is_number_float()) {
		auto ptr = value.get_ptr<nlohmann::json::number_float_t*>();
		float p = *ptr;

		ImGui::SameLine();
		ImGui::SliderFloat("float", &p, 0.f, 10.f);

		*ptr = p;
	}

	else if (value.is_string()) {
		if (ImGui::TreeNodeEx(path.c_str(), flags)) {
			ImGui::SameLine();
			ImGui::Text("%s", value.get_ptr<std::string*>()->c_str());

			static char namebuf[128];
			ImGui::InputText("##edit", namebuf, sizeof(namebuf));

			ImGui::SameLine();
			if (ImGui::Button("OK")) {
				value = namebuf;
			}
			ImGui::TreePop();
		}
	}

	else if (value.is_null()) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), "<null>");
	}
}

void gameEditor::addEntityWindow(gameMain *game) {
#if 0
	static char comboBuf[0x1000];
	static const size_t bufsize = sizeof(comboBuf) - 1;

	*comboBuf = '\0'; // reset every frame

	ImGui::Begin("Add entity", &showAddEntityWindow, ImGuiWindowFlags_MenuBar);
	std::vector<std::string> names;

	size_t n = 0;
	for (auto& [name, _] : game->factories->factories) {
		names.push_back(name);

		if (n + name.length() + 2 < bufsize) {
			strncpy(comboBuf + n, name.c_str(), bufsize - n);
			n += name.length() + 1;
			// add second null terminator in case this is the last entry
			comboBuf[n + 1] = '\0';
		}
	}

	// make sure the combo string is null-terminated (man I don't miss C strings)
	// this is one big thing I'm not a fan of in imgui -
	// std::initializer_list would still be pretty cheap for the cases where you
	// want to specify a list of constants in code, std::vector& would be _much_
	// cheaper for cases like this where you're dynamically generating some list
	// of options...
	// why not use a more c++-ish style, it's basically a c library written in c++
	comboBuf[bufsize] = '\0';
	comboBuf[bufsize - 1] = '\0';

	static int baseIndex = -1;
	static std::vector<int> componentIndexes;
	static nlohmann::json curjson;
	static bool syncToCursor = true;

	// TODO
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("Templates")) {
			if (ImGui::MenuItem("Save as new template")) {

				static char templateSave[256] = "Template";
				ImGui::InputText("## template save", templateSave, sizeof(templateSave));
				ImGui::SameLine();
				if (ImGui::Button("Save as template")) {
					// TODO: Save as template implementation
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Entity properties")) {
			ImGui::Checkbox("Sync node properties to cursor", &syncToCursor);
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	static int curidx = 0;
	ImGui::Combo("Base entity", &curidx, comboBuf);
	ImGui::Separator();

	if (curidx != baseIndex) {
		curjson = game->factories->properties(names[curidx]);
		baseIndex = curidx;
		// XXX
		curjson["entity-type"] = names[curidx];
	}

	float footer =
		2.f * (ImGui::GetStyle().ItemSpacing.y
		       + ImGui::GetFrameHeightWithSpacing());
	float sidebuttons = ImGui::GetStyle().ItemSpacing.x
		+ ImGui::GetFrameHeightWithSpacing();

	ImGui::BeginChild("components", ImVec2(0, -footer));
	ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), "Properties");
	ImGui::Separator();
	drawJson(curjson);
	ImGui::EndChild();

	ImGui::Separator();
	static int addidx = 0;
	ImGui::Combo("##Component Type", &addidx, comboBuf);
	ImGui::SameLine();
	if (ImGui::Button("Add component")) {
		auto props = game->factories->properties(names[addidx]);

		curjson["components"].push_back({
			names[addidx],
			props
		});
	}

	if (syncToCursor) {
		// TODO: could have function to serialize any TRS
		curjson["node"] = {
			{"position",
				{
					cursorBuf.position.x,
					cursorBuf.position.y,
					cursorBuf.position.z,
				}},
			{"rotation",
				{
					cursorBuf.rotation.w,
					cursorBuf.rotation.x,
					cursorBuf.rotation.y,
					cursorBuf.rotation.z,
				}},
			{"scale",
				{
					cursorBuf.scale.x,
					cursorBuf.scale.y,
					cursorBuf.scale.z,
				}},
		};
	}

	ImGui::Separator();

	static bool attach = false;
	ImGui::Checkbox("Attach to selected node", &attach);

	ImGui::SameLine();
	if (ImGui::Button("Create")) {
		ecs::entity *ent = game->factories->build(game->entities.get(), curjson);

		if (ent) {
			if (attach && selectedNode) {
				setNode("entity-original-node", selectedNode, ent->node);
				ent->node = selectedNode;
			}

			game->entities->add(ent);
			selectedNode = ent->node;
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel")) {
		componentIndexes.clear();
		baseIndex = 0;
		showAddEntityWindow = false;
	}

	ImGui::End();
#endif
}

static void handle_prompts(gameEditor *editor, gameMain *game) {
	if (export_entity_dialog.promptFilename()) {
		auto factories = game->services.resolve<ecs::serializer>();
		auto entities  = game->services.resolve<ecs::entityManager>();

		const std::string& name = export_entity_dialog.selection;

		if (!editor->selectedEntity) {
			std::cerr << "No entity selected, can't export to "
				<< name << std::endl;
			return;
		}

		std::cerr << "Exporting entity to " << name << std::endl;

		std::ofstream out(name);
		auto curjson = factories->serialize(entities, editor->selectedEntity);

		out << curjson.dump();

		export_entity_dialog.clear();
	}
}

void gameEditor::entitySelectWindow(gameMain *game) {
	auto entities  = game->services.resolve<ecs::entityManager>();
	auto factories = game->services.resolve<ecs::serializer>();

	ImGui::Begin("Entities", &showEntitySelectWindow);
	if (ImGui::Button("Clear")) {
		*searchBuffer = '\0';
	}
	ImGui::SameLine();
	ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
	auto tags = split_string(searchBuffer);
	std::vector<const char *> tagchars;

	for (const auto& v : tags) {
		//tagchars.push_back(v.c_str());
		if (const char *re = remangle(v)) {
			tagchars.push_back(re);
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("New entity")) {
		//showAddEntityWindow = true;
		ImGui::OpenPopup("new_entity_popup");
	}

	if (ImGui::BeginPopup("new_entity_popup")) {
		for (const auto& [name, _] : entities->components) {
			if (ImGui::Selectable(demangle(name).c_str())) {
				nlohmann::json j = {
					{"entity-type", demangle(name)},
					{"entity-properties", {
						{"position", {0,0,0}},
						{"rotation", {1,0,0,0}},
						{"scale",    {1,1,1}}
					}},
					{"components", {}},
				};

				ecs::entity *ent = factories->build(entities, j);
				entities->add(ent);
			}
			//drawSelectableLabel(demangle(name).c_str());
		}
		ImGui::EndPopup();
	}

	ImGui::Separator();

	ImGui::Columns(3);

	ImGui::BeginChild("components");
	ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), "Active components");
	ImGui::Separator();

	ImGui::BeginChild("componentList");
	for (const auto& [name, _] : entities->components) {
		drawSelectableLabel(demangle(name).c_str());
	}

	ImGui::EndChild();
	ImGui::EndChild();
	ImGui::NextColumn();

	ImGui::BeginChild("matchingEnts", ImVec2(0, 0), false, 0);
	ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), "Matching entities");
	ImGui::Separator();

	ImGui::BeginChild("entityList", ImVec2(0, 0), false, 0);
	for (auto& ent : entities->entities) {
		if (*searchBuffer && !entities->hasComponents(ent, tagchars)) {
			// entity doesn't have the searched tags, filtered out
			// TODO: wait, why am I not using the search interface here?
			continue;
		}

		std::string entstr = "entity #" + std::to_string((uintptr_t)ent);
		std::string contextstr = entstr + ":context";
		std::string popupstr = entstr + ":popup";

		if (entities->condemned.count(ent)) {
			entstr = "[deleted] " + entstr;
		}

		if (ImGui::Selectable(entstr.c_str(), ent == selectedEntity)) {
			selectedNode      = ent->node;
			selectedEntity    = ent;
		}

		if (ImGui::BeginPopupContextItem(contextstr.c_str())) {
			ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), "Action");
			ImGui::Separator();

			if (ImGui::Selectable("Delete")) {
				entities->remove(selectedEntity);
				selectedEntity = nullptr;
			}

			if (ImGui::Selectable("Duplicate")) { /* TODO */ }
			ImGui::EndPopup();
		}

		auto& components = entities->getEntityComponents(ent);
		std::set<std::string> seen;
		ImGui::Separator();
		ImGui::Indent(16.f);
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), "Attached components:");
		ImGui::Columns(2);

		std::string sectionName = entstr + ":components";
		ImGui::TreePush(sectionName.c_str());

		for (auto& [name, comp] : components) {
			if (!seen.count(name)) {
				if (ImGui::Selectable(demangle(name).c_str())) {
					// TODO: need a way to serialize a specific component,
					//       avoid recreating an entire entity
					//selectedComponent = comp;
					selectedEntity = ent;
				}

				ImGui::NextColumn();
				seen.insert(name);
			}
		}

		ImGui::TreePop();
		ImGui::Columns(1);

		std::string asdf = entstr + ":sec";
		ImGui::TreePush(asdf.c_str());
		if (ImGui::Button("Attach")) {
			ImGui::OpenPopup(popupstr.c_str());
		}

		ImGui::SameLine();
		if (ImGui::Button("Save")) {
			selectedEntity = ent;
			export_entity_dialog.show();
		}

		if (ImGui::BeginPopup(popupstr.c_str())) {
			for (const auto& [name, _] : entities->components) {
				if (ImGui::Selectable(demangle(name).c_str())) {
					nlohmann::json j = {demangle(name), {}};

					factories->build(entities, ent, j);
				}
			}
			ImGui::EndPopup();
		}
		ImGui::TreePop();

		ImGui::Unindent(16.f);
		ImGui::Separator();
	}

	ImGui::EndChild();
	ImGui::EndChild();
	ImGui::NextColumn();

	ImGui::BeginChild("editComponent");
	ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), "Active components");
	ImGui::Separator();

	ImGui::BeginChild("editView");
	static nlohmann::json curjson;
	static ecs::entity* curcomp = nullptr;

	if (selectedEntity != curcomp && selectedEntity) {
		curjson = factories->serialize(entities, selectedEntity);
		curcomp = selectedEntity;
	}

	drawJson(curjson);

	if (ImGui::Button("Apply")) {
		ecs::entity *ent = factories->build(entities, curjson);

		entities->remove(selectedEntity);
		entities->clearFreedEntities();
		entities->add(ent);

		selectedEntity = ent;
	}

	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::Columns(1);
	ImGui::End();

	handle_prompts(this, game);
}

#include <grend/gameEditor.hpp>
#include <grend/controllers.hpp>
#include <grend/loadScene.hpp>
#include <grend/renderContext.hpp>

// XXX: for updateEntityTransforms()
#include <grend/ecs/rigidBody.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl.h>

using namespace grendx;

void gameEditor::handleSelectObject(gameMain *game) {
	int x, y;
	int win_x, win_y;
	Uint32 buttons = SDL_GetMouseState(&x, &y); (void)buttons;
	SDL_GetWindowSize(game->ctx.window, &win_x, &win_y);

	float fx = x/(1.f*win_x);
	float fy = y/(1.f*win_y);

	auto rend = game->services.resolve<renderContext>();

	uint32_t clickidx = rend->framebuffer->index(fx, fy);
	std::cerr << "clicked object: " << clickidx << std::endl;
	std::cerr << "ent object: " << clickidx-10 << std::endl;
	std::cerr << "clickables: " << clickState.size() << std::endl;

	if (clickidx - 10 < clickState.size()) {
		auto& [_, ent] = clickState[clickidx - 10];
		selectedNode = ent->node;
		selectedEntity = ent;

		std::cerr << "selected entity: " << clickidx-10
			<< "@" << (void*)ent
			<< std::endl;

	} else if (clickidx && selectedNode) {
		clickedX = (x*1.f / win_x);
		clickedY = ((win_y - y)*1.f / win_y);

		if (clickidx > 0 && clickidx <= 6) {
			transformBuf = selectedNode->getTransformTRS();
			std::cerr << "It's a UI model" << std::endl;

		} else {
			// TODO: store, look up entity IDs as render IDs
			//selectedNode = getNonModel(clicked);
		}

		switch (clickidx) {
			case 1: setMode(MoveX); break;
			case 2: setMode(MoveY); break;
			case 3: setMode(MoveZ); break;
			case 4: setMode(RotateX); break;
			case 5: setMode(RotateY); break;
			case 6: setMode(RotateZ); break;
			default: break;
		}

		if (selectedNode) {
			clickDepth = glm::distance(selectedNode->getTransformTRS().position,
					cam->position());
		} else {
			clickDepth = 0.f;
		}

	} else {
		selectedNode   = nullptr;
		selectedEntity = nullptr;
	}
}

static bool imguiWantsKeyboard(void) {
	ImGuiIO& io = ImGui::GetIO();

	return io.WantCaptureKeyboard;
}

static bool imguiWantsMouse(void) {
	ImGuiIO& io = ImGui::GetIO();

	return io.WantCaptureMouse;
}


static void handleAddNode(gameEditor *editor,
                          std::string name,
                          sceneNode::ptr obj)
{
	assert(editor->selectedNode != nullptr);

	obj->setTransform(editor->cursorBuf);
	setNode(name, editor->selectedNode, obj);
	editor->selectedNode = obj;
	editor->runCallbacks(obj, gameEditor::editAction::Added);
};

template <class T>
static bindFunc makeClicker(gameEditor *editor,
                            gameMain *game,
                            std::string name)
{
	return [=] (const SDL_Event& ev, unsigned flags) {
		if (editor->selectedNode
		    && ev.type == SDL_MOUSEBUTTONDOWN
			&& ev.button.button == SDL_BUTTON_LEFT)
		{
			auto ptr = std::make_shared<T>();
			std::string nodename = name + std::to_string(ptr->id);
			handleAddNode(editor, nodename, ptr);
			return (int)gameEditor::mode::View;
		}

		return MODAL_NO_CHANGE;
	};
}

void gameEditor::loadInputBindings(gameMain *game) {
	inputBinds.bind(MODAL_ALL_MODES, resizeInputHandler(game, post));

	// camera movement (on key press)
	inputBinds.bind(MODAL_ALL_MODES,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				auto v = cam->velocity();

				switch (ev.key.keysym.sym) {
					case SDLK_w: v.z =  movementSpeed; break;
					case SDLK_s: v.z = -movementSpeed; break;
					case SDLK_a: v.x =  movementSpeed; break;
					case SDLK_d: v.x = -movementSpeed; break;
					case SDLK_q: v.y =  movementSpeed; break;
					case SDLK_e: v.y = -movementSpeed; break;
					default: break;
				}

				cam->setVelocity(v);
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// Return back to View mode from any other mode
	inputBinds.bind(MODAL_ALL_MODES,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				if (ev.key.keysym.sym == SDLK_ESCAPE) {
					return (int)mode::View;
				}
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// reload shaders
	inputBinds.bind(mode::View,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN
			    && ev.key.keysym.sym == SDLK_r
			    && flags & bindFlags::Control)
			{
				reloadShaders(game);
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// camera movement (on key up)
	inputBinds.bind(MODAL_ALL_MODES,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYUP) {
				auto v = cam->velocity();

				switch (ev.key.keysym.sym) {
					case SDLK_w:
					case SDLK_s: v.z = 0; break;
					case SDLK_a:
					case SDLK_d: v.x = 0; break;
					case SDLK_q:
					case SDLK_e: v.y = 0; break;
				}

				cam->setVelocity(v);
			}

			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// map editing
	inputBinds.bind(mode::View,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				auto state = game->services.resolve<gameState>();

				switch (ev.key.keysym.sym) {
					case SDLK_i:
						if (auto node = loadMapCompiled()) {
							clear(game);
							selectedNode = state->rootnode = *node;
							runCallbacks(selectedNode, editAction::NewScene);

						} else printError(node);

						break;

					case SDLK_o: saveMap(game, state->rootnode); break;
					case SDLK_DELETE: selectedNode = unlink(selectedNode); break;
				}
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// clone keybind
	inputBinds.bind(mode::View,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN
			    && ev.key.keysym.sym == SDLK_d
			    && flags & bindFlags::Shift
			    && selectedNode && !selectedNode->parent.expired())
			{
				sceneNode::ptr temp = clone(selectedNode);
				std::string name = "cloned " + std::to_string(temp->id);
				setNode(name, selectedNode->parent.lock(), temp);
				runCallbacks(selectedNode, editAction::Added);
				return (int)mode::MoveSomething;
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// camera movement (set direction)
	inputBinds.bind(MODAL_ALL_MODES,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			int x, y;
			Uint32 buttons = SDL_GetMouseState(&x, &y); (void)buttons;

			if (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
				int win_x, win_y;
				SDL_GetWindowSize(game->ctx.window, &win_x, &win_y);

				x = (x > 0)? x : win_x/2;
				y = (x > 0)? y : win_y/2;

				float center_x = (float)win_x / 2;
				float center_y = (float)win_y / 2;

				float rel_x = ((float)x - center_x) / center_x;
				float rel_y = ((float)y - center_y) / center_y;

				cam->setDirection(glm::vec3(
					sin(rel_x*2*M_PI),
					sin(-rel_y*M_PI/2.f),
					-cos(rel_x*2*M_PI)
				));
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsMouse);

	// scroll wheel for cursor placement
	inputBinds.bind(MODAL_ALL_MODES,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_MOUSEWHEEL) {
				editDistance -= ev.wheel.y/1.f /* scroll sensitivity */;
			}

			return MODAL_NO_CHANGE;
		},
		imguiWantsMouse);

	// camera movement (set direction)
	inputBinds.bind(mode::View,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_MOUSEBUTTONDOWN
			    && ev.button.button == SDL_BUTTON_LEFT)
			{
				auto state = game->services.resolve<gameState>();

				if (flags & bindFlags::Control) {
					// TODO: need like a keymapping system
					// TODO: need more abstracted object/entity/we
					//       selection/delection
					selectedNode   = state->rootnode;
					selectedEntity = nullptr;

				} else {
					handleSelectObject(game);
				}
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsMouse);

	// handle menubar keybinds
	inputBinds.bind(mode::View,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				switch (ev.key.keysym.sym) {
					case SDLK_l: return (int)mode::AddSomething;
					case SDLK_g: return (int)mode::MoveSomething;
					case SDLK_r: return (int)mode::RotateSomething;
					case SDLK_f: return (int)mode::ScaleSomething;
				}
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// handle add keybinds
	inputBinds.bind(mode::AddSomething,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				switch (ev.key.keysym.sym) {
					case SDLK_o: return (int)mode::AddObject;
					case SDLK_p: return (int)mode::AddPointLight;
					case SDLK_s: return (int)mode::AddSpotLight;
					case SDLK_d: return (int)mode::AddDirectionalLight;
					case SDLK_r: return (int)mode::AddReflectionProbe;
					default:     return (int)mode::View;
				}
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// handle move keybinds
	inputBinds.bind(mode::MoveSomething,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				transformBuf = selectedNode->getTransformTRS();

				switch (ev.key.keysym.sym) {
					case SDLK_x: return (int)mode::MoveX;
					case SDLK_y: return (int)mode::MoveY;
					case SDLK_z: return (int)mode::MoveZ;
					default:     return (int)mode::View;
				}
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// rotate keybinds
	inputBinds.bind(mode::RotateSomething,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				transformBuf = selectedNode->getTransformTRS();

				switch (ev.key.keysym.sym) {
					case SDLK_x: return (int)mode::RotateX;
					case SDLK_y: return (int)mode::RotateY;
					case SDLK_z: return (int)mode::RotateZ;
					default:     return (int)mode::View;
				}
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	// scale keybinds
	inputBinds.bind(mode::ScaleSomething,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				transformBuf = selectedNode->getTransformTRS();

				switch (ev.key.keysym.sym) {
					case SDLK_x: return (int)mode::ScaleX;
					case SDLK_y: return (int)mode::ScaleY;
					case SDLK_z: return (int)mode::ScaleZ;
					default:     return (int)mode::View;
				}
			}
			return MODAL_NO_CHANGE;
		},
		imguiWantsKeyboard);

	inputBinds.bind(mode::View,
		[&, game] (const SDL_Event& ev, unsigned flags) {
			int ret = MODAL_NO_CHANGE;

			if (selectedNode == nullptr
			    || selectedNode->type != sceneNode::objType::ReflectionProbe)
			{
				// only reflection probes have editable bounding boxes for the
				// time being
				return ret;
			}

			if (ev.type == SDL_KEYDOWN) {
				if (flags & bindFlags::Shift) {
					switch (ev.key.keysym.sym) {
						case SDLK_x: ret = mode::MoveAABBPosX; break;
						case SDLK_y: ret = mode::MoveAABBPosY; break;
						case SDLK_z: ret = mode::MoveAABBPosZ; break;
						default: break;
					}

				} else {
					switch (ev.key.keysym.sym) {
						case SDLK_x: ret = mode::MoveAABBNegX; break;
						case SDLK_y: ret = mode::MoveAABBNegY; break;
						case SDLK_z: ret = mode::MoveAABBNegZ; break;
						default: break;
					}
				}
			}

			if (ret != MODAL_NO_CHANGE) {
				sceneReflectionProbe::ptr probe =
					std::dynamic_pointer_cast<sceneReflectionProbe>(selectedNode);

				// XXX: put bounding box in position/scale transform...
				// TODO: don't do that
				transformBuf.position = probe->boundingBox.min;
				transformBuf.scale    = probe->boundingBox.max;

				// XXX: no mouse click, pretend there was a click at the center
				//      for movement purposes
				clickedX = clickedY = 0.5;
			}

			return ret;
		},
		imguiWantsKeyboard);

	inputBinds.bind(mode::AddObject,
		makeClicker<sceneNode>(this, game, "object E"),
		imguiWantsMouse);

	inputBinds.bind(mode::AddPointLight,
		makeClicker<sceneLightPoint>(this, game, "point light E"),
		imguiWantsMouse);

	inputBinds.bind(mode::AddSpotLight,
		makeClicker<sceneLightSpot>(this, game, "spot light E"),
		imguiWantsMouse);

	inputBinds.bind(mode::AddDirectionalLight,
		makeClicker<sceneLightDirectional>(this, game, "directional light E"),
		imguiWantsMouse);

	inputBinds.bind(mode::AddReflectionProbe,
		makeClicker<sceneReflectionProbe>(this, game, "reflection probe E"),
		imguiWantsMouse);

	inputBinds.bind(mode::AddIrradianceProbe,
		makeClicker<sceneIrradianceProbe>(this, game, "irradiance probe E"),
		imguiWantsMouse);

	auto releaseMove = [&, game] (const SDL_Event& ev, unsigned flags) {
		if (ev.type == SDL_MOUSEBUTTONUP
		    && ev.button.button == SDL_BUTTON_LEFT)
		{
			invalidateLightMaps(selectedNode);
			return (int)mode::View;
		} else {
			return MODAL_NO_CHANGE;
		}
	};
	
	inputBinds.bind(mode::MoveX, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::MoveY, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::MoveZ, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::RotateX, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::RotateY, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::RotateZ, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::ScaleSomething, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::ScaleX, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::ScaleY, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::ScaleZ, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::MoveAABBPosX, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::MoveAABBPosY, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::MoveAABBPosZ, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::MoveAABBNegX, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::MoveAABBNegY, releaseMove, imguiWantsMouse);
	inputBinds.bind(mode::MoveAABBNegZ, releaseMove, imguiWantsMouse);
}

void gameEditor::handleEvent(gameMain *game, const SDL_Event& ev)
{
	ImGui_ImplSDL2_ProcessEvent(&ev);
	setMode((enum mode)inputBinds.dispatch(ev));
}

// TODO: move to utility
#define TAU  (6.2831853)
#define TAUF (6.2831853f)

template <typename T>
static T sign(T x) {
	if (x >= 0) {
		return 1;
	} else {
		return -1;
	}
}

void gameEditor::updateSelected(const TRS& updated) {
	selectedNode->setTransform(updated);

	if (selectedEntity) {
		std::cerr << "got here, updating entity" << std::endl;
		updateEntityTransforms(selectedEntity->manager, selectedEntity, updated);
	}

	runCallbacks(selectedNode, editAction::Moved);
}

void gameEditor::handleMoveRotate(gameMain *game) {
	int x, y;
	int win_x, win_y;
	Uint32 buttons = SDL_GetMouseState(&x, &y); (void)buttons;
	SDL_GetWindowSize(game->ctx.window, &win_x, &win_y);

	float adj_x = x / (1.f*win_x);
	float adj_y = (win_y - y) / (1.f * win_y);
	float amount_x = adj_x - clickedX;
	float amount_y = adj_y - clickedY;
	float amount = amount_x + amount_y;
	float reversed_x;
	float reversed_y;

	glm::mat3 rot;
	glm::vec3 dir;
	float rad;

	TRS selectedTransform = selectedNode->getTransformTRS();
	glm::vec4 screenuv = cam->worldToScreenPosition(transformBuf.position);
	glm::vec2 screenpos = glm::vec2(screenuv.x*win_x, screenuv.y*win_y);
	glm::vec2 normed, clicknorm;

	clicknorm = glm::vec2(clickedX*win_x - screenpos.x,
	                      clickedY*win_y - screenpos.y);
	normed = glm::vec2(x - screenpos.x, (win_y-y) - screenpos.y);

	switch (mode) {
		case mode::MoveX:
			rot = glm::mat3_cast(transformBuf.rotation);
			dir = rot * glm::vec3(1, 0, 0);
			reversed_x = sign(glm::dot(dir, -cam->right()));
			reversed_y = sign(glm::dot(dir, cam->up()));

			selectedTransform.position =
				transformBuf.position
				+ dir*clickDepth*amount_x*reversed_x
				+ dir*clickDepth*amount_y*reversed_y
				;
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Moved);
			break;

		case mode::MoveY:
			rot = glm::mat3_cast(transformBuf.rotation);
			dir = rot * glm::vec3(0, 1, 0);
			reversed_x = sign(glm::dot(dir, -cam->right()));
			reversed_y = sign(glm::dot(dir, cam->up()));

			selectedTransform.position =
				transformBuf.position
				+ dir*clickDepth*amount_x*reversed_x
				+ dir*clickDepth*amount_y*reversed_y
				;
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Moved);
			break;

		case mode::MoveZ:
			rot = glm::mat3_cast(transformBuf.rotation);
			dir = rot * glm::vec3(0, 0, 1);
			reversed_x = sign(glm::dot(dir, -cam->right()));
			reversed_y = sign(glm::dot(dir, cam->up()));

			selectedTransform.position =
				transformBuf.position
				+ dir*clickDepth*amount_x*reversed_x
				+ dir*clickDepth*amount_y*reversed_y
				;
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Moved);
			break;

		// TODO: need to split rotation spinner in seperate quadrant meshes
		//       so that this can pick the right movement direction
		//       for the spinner
		case mode::RotateX:
			rot = glm::mat3_cast(transformBuf.rotation);
			dir = rot * glm::vec3(1, 0, 0);
			reversed_x = glm::sign(glm::dot(dir, cam->direction()));

			rad = atan2(normed.x, normed.y) - atan2(clicknorm.x, clicknorm.y);
			selectedTransform.rotation =
				glm::quat(glm::rotate(transformBuf.rotation,
				                      reversed_x*rad,
				                      glm::vec3(1, 0, 0)));
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Rotated);
			break;

		case mode::RotateY:
			rot = glm::mat3_cast(transformBuf.rotation);
			dir = rot * glm::vec3(0, 1, 0);
			reversed_x = glm::sign(glm::dot(dir, cam->direction()));

			rad = atan2(normed.x, normed.y) - atan2(clicknorm.x, clicknorm.y);
			selectedTransform.rotation =
				glm::quat(glm::rotate(transformBuf.rotation,
				                      reversed_x*rad,
				                      glm::vec3(0, 1, 0)));
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Rotated);
			break;

		case mode::RotateZ:
			rot = glm::mat3_cast(transformBuf.rotation);
			dir = rot * glm::vec3(0, 0, 1);
			reversed_x = glm::sign(glm::dot(dir, cam->direction()));

			rad = atan2(normed.x, normed.y) - atan2(clicknorm.x, clicknorm.y);
			selectedTransform.rotation =
				glm::quat(glm::rotate(transformBuf.rotation,
				                      reversed_x*rad,
				                      glm::vec3(0, 0, 1)));
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Rotated);
			break;

		// scale, unlike the others, has a mouse handler for the select mode,
		// similar to blender
		case mode::ScaleSomething:
			selectedTransform.scale =
				transformBuf.scale + glm::vec3(TAUF*amount);
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Scaled);
			break;

		case mode::ScaleX:
			selectedTransform.scale =
				transformBuf.scale + glm::vec3(TAUF*amount, 0, 0);
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Scaled);
			break;

		case mode::ScaleY:
			selectedTransform.scale =
				transformBuf.scale + glm::vec3(0, TAUF*amount, 0);
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Scaled);
			break;

		case mode::ScaleZ:
			selectedTransform.scale =
				transformBuf.scale + glm::vec3(0, 0, TAUF*amount);
			updateSelected(selectedTransform);
			//selectedNode->setTransform(selectedTransform);
			//runCallbacks(selectedNode, editAction::Scaled);
			break;

		default:
			break;
	}

	if (selectedNode->type == sceneNode::objType::ReflectionProbe) {
		sceneReflectionProbe::ptr probe =
			std::dynamic_pointer_cast<sceneReflectionProbe>(selectedNode);

		float reversed_x = sign(glm::dot(glm::vec3(1, 0, 0), -cam->right()));
		float reversed_y = sign(glm::dot(glm::vec3(0, 1, 0),  cam->up()));
		float reversed_z = sign(glm::dot(glm::vec3(0, 0, 1), -cam->right()));
		float depth = glm::distance(probe->getTransformTRS().position, cam->position());

		switch (mode) {
			case mode::MoveAABBPosX:
				probe->boundingBox.max =
					transformBuf.scale
						+ glm::vec3(1, 0, 0)*depth*amount_x*reversed_x
						+ glm::vec3(1, 0, 0)*depth*amount_y*reversed_y;
				break;

			case mode::MoveAABBPosY:
				probe->boundingBox.max =
					transformBuf.scale
						+ glm::vec3(0, 1, 0)*depth*amount_x*reversed_x
						+ glm::vec3(0, 1, 0)*depth*amount_y*reversed_y;
				break;

			case mode::MoveAABBPosZ:
				probe->boundingBox.max =
					transformBuf.scale
						+ glm::vec3(0, 0, 1)*depth*amount_x*reversed_z
						+ glm::vec3(0, 0, 1)*depth*amount_y*reversed_y*reversed_z;
				break;

			case mode::MoveAABBNegX:
				probe->boundingBox.min =
					transformBuf.position
						+ glm::vec3(1, 0, 0)*depth*amount_x*reversed_x
						+ glm::vec3(1, 0, 0)*depth*amount_y*reversed_y;
				break;

			case mode::MoveAABBNegY:
				probe->boundingBox.min =
					transformBuf.position
						+ glm::vec3(0, 1, 0)*depth*amount_x*reversed_x
						+ glm::vec3(0, 1, 0)*depth*amount_y*reversed_y;
				break;

			case mode::MoveAABBNegZ:
				probe->boundingBox.min =
					transformBuf.position
						+ glm::vec3(0, 0, 1)*depth*amount_x*reversed_z
						+ glm::vec3(0, 0, 1)*depth*amount_y*reversed_y*reversed_z;
				break;

			default:
				break;
		}
	}
}

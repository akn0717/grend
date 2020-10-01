#include <grend/gameMainDevWindow.hpp>
#include <grend/gameObject.hpp>
#include <grend/playerView.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>

#include <iostream>
#include <memory>

using namespace grendx;

// TODO: library functions to handle most of this
void addCameraWeapon(gameView::ptr view) {
	playerView::ptr player = std::dynamic_pointer_cast<playerView>(view);
	gameObject::ptr testweapon = std::make_shared<gameObject>();

	auto [objs, models] =
		load_gltf_scene(GR_PREFIX "assets/obj/Mossberg-lowres/shotgun.gltf");

	compile_models(models);
	bind_cooked_meshes();
	objs->transform.scale = glm::vec3(2.0f);
	objs->transform.position = glm::vec3(-0.3, 1.1, 1.25);
	objs->transform.rotation = glm::quat(glm::vec3(0, 3.f*M_PI/2.f, 0));
	setNode("weapon", player->cameraObj, objs);
}

int main(int argc, char *argv[]) {
	std::cerr << "entering main()" << std::endl;
	std::cerr << "started SDL context" << std::endl;
	std::cerr << "have game state" << std::endl;

	gameMain *game = new gameMainDevWindow();
	gameView::ptr player = std::make_shared<playerView>(game);
	game->setView(player);
	addCameraWeapon(player);

	auto foo = openSpatialLoop(GR_PREFIX "assets/sfx/Bit Bit Loop.ogg");
	foo->worldPosition = glm::vec3(-10, 0, -5);
	game->audio->add(foo);

	auto bar = openSpatialLoop(GR_PREFIX "assets/sfx/Meditating Beat.ogg");
	bar->worldPosition = glm::vec3(0, 0, -5);
	game->audio->add(bar);

	game->run();
	return 0;
}

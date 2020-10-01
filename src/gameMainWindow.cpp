#include <grend/gameMainWindow.hpp>

using namespace grendx;

gameMainWindow::gameMainWindow() : gameMain("grend") {
	phys   = physics::ptr(new imp_physics());
	state  = game_state::ptr(new game_state());
	rend   = renderer::ptr(new renderer(ctx));
	audio  = audioMixer::ptr(new audioMixer(ctx));
}

void gameMainWindow::handleInput(void) {
	SDL_Event ev;

	while (SDL_PollEvent(&ev)) {
		if (ev.type == SDL_QUIT) {
			running = false;
			return;
		}

		view->handleInput(this, ev);
	}
}

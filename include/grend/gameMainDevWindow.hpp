#pragma once

#include <grend/gameState.hpp>   // TODO: rename to gameState.h
#include <grend/engine.hpp>      // TODO: rename to renderer.h
#include <grend/game_editor.hpp> // TODO: gameEditor
#include <grend/imp_physics.hpp>
#include <grend/gameMain.hpp>
#include <grend/gameView.hpp>
#include <grend/playerView.hpp>
#include <grend/timers.hpp>
#include <grend/modalSDLInput.hpp>
#include <memory>

namespace grendx {

// development instance with editor, a production instance would only
// need a player view
class gameMainDevWindow : public gameMain {
	public:
		enum modes {
			Editor,
			Player,
		};

		gameMainDevWindow();
		virtual void handleInput(void);

		gameView::ptr player;
		gameView::ptr editor;
		modalSDLInput input;
};

// namespace grendx
}

#pragma once

#include <grend/gameObject.hpp>
#include <grend/gameModel.hpp>
#include <grend/glManager.hpp>

#include <vector>

namespace grendx {

class renderFramebuffer {
	public:
		typedef std::shared_ptr<renderFramebuffer> ptr;
		typedef std::weak_ptr<renderFramebuffer> weakptr;

		// TODO: Have like 3 different conventions for this scattered through
		//       the codebase, need to just pick one
		renderFramebuffer(int Width, int Height);
		renderFramebuffer(Framebuffer::ptr fb, int Width, int Height);

		void clear(void);
		void setSize(int Width, int Height);
		gameMesh::ptr index(float x, float y);
		gameMesh::ptr index(unsigned idx);

		Framebuffer::ptr framebuffer;
		Texture::ptr color;
		Texture::ptr depth;

		int width = -1, height = -1;
		struct {
			float x, y;
			float min_x, min_y;
		} scale = {
			1.0, 1.0,
			0.5, 0.5,
		};

		std::vector<gameMesh::ptr> drawn_meshes;
};

// namespace grendx;
}
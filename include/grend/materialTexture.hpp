#pragma once

#include <grend/glmIncludes.hpp>
#include <grend/openglIncludes.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace grendx {

// TODO: need a material cache, having a way to lazily load/unload texture
//       would be a very good thing
class materialTexture {
	public:
		typedef std::shared_ptr<materialTexture> ptr;
		typedef std::weak_ptr<materialTexture> weakptr;

		materialTexture() { };
		materialTexture(std::string filename);
		void load_texture(std::string filename);
		bool loaded(void) const { return channels != 0; };

		int width = 0, height = 0;
		int channels = 0;
		size_t size;
		std::vector<uint8_t> pixels;

		// XXX: could use GL enums directly here, seems like that might
		//      be tying it too closely to the OpenGL api though... idk,
		//      if this is too unwieldy can always remove it later
		enum filter {
			// only 'nearest' and 'linear' valid for mag filter
			// can throw away mipmap specification on the opengl side,
			// if that that's specified for a mag filter
			Nearest,
			Linear,

			NearestMipmaps, // not used as mode
			NearestMipmapNearest,
			NearestMipmapLinear,

			LinearMipmaps,  // not used as mode
			LinearMipmapNearest,
			LinearMipmapLinear,
		};

		enum wrap {
			ClampToEdge,
			MirroredRepeat,
			Repeat,
		};

		enum imageType {
			Plain,
			Etc2Compressed,
			VecTex,
		};

		// higher quality defaults, tweakable settings can be done in loading
		enum filter minFilter = filter::LinearMipmapLinear;
		enum filter magFilter = filter::Linear;

		enum wrap wrapS = wrap::Repeat;
		enum wrap wrapT = wrap::Repeat;

		enum imageType type = imageType::Plain;
};

// namespace grendx
}

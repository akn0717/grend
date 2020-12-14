#pragma once

#include <grend/glmIncludes.hpp>
#include <grend/openglIncludes.hpp>
#include <grend/sdlContext.hpp>
// TODO: move, for lights
#include <grend/quadtree.hpp>
#include <grend/animation.hpp>
#include <grend/boundingBox.hpp>

#include <memory>
#include <map>
#include <vector>
#include <utility>
#include <string>
#include <stdint.h>

#include <iostream>
#include <sstream>

namespace grendx {

size_t allocateObjID(void);

// forward declaration, physics.hpp include at end
class physicsObject;

// TODO: start switching things to camelCase
class gameObject {
	public:
		// used for type checking, dynamically-typed tree here
		enum objType {
			None,
			Import,          // Imported model, indicates where to stop saving
			                 // + where to load from
			Model,           // generic model, static/dynamic/etc subclasses
			Mesh,            // meshes that make up a model
			Skin,            // Skinning info
			Particles,       // particle system
			Light,           // Light object, has Point/Spot/etc subclasses
			ReflectionProbe, // Full reflection probe
			IrradianceProbe, // diffuse light probe
			Camera,          // TODO: camera position marker
		} type = objType::None;

		typedef std::shared_ptr<gameObject> ptr;
		typedef std::weak_ptr<gameObject> weakptr;

		gameObject(enum objType t = objType::None) : type(t) {};
		~gameObject();

		// handlers for basic input events
		virtual void onLeftClick() {
			// default handlers just call upwards
			std::cerr << "left click " << idString() << std::endl;
			if (auto p = parent.lock()) { p->onLeftClick(); }
		};

		virtual void onMiddleClick() {
			std::cerr << "middle click " << idString() << std::endl;
			if (auto p = parent.lock()) { p->onMiddleClick(); }
		};

		virtual void onRightClick() {
			std::cerr << "right click " << idString() << std::endl;
			if (auto p = parent.lock()) { p->onRightClick(); }
		};

		virtual void onHover() {
			std::cerr << "hover " << idString() << std::endl;
			if (auto p = parent.lock()) { p->onHover(); }
		};

		virtual std::string typeString(void) {
			return "gameObject";
		}

		virtual std::string idString(void) {
			std::stringstream strm;
			strm << "[" << typeString() << " : 0x" << std::hex << this <<  "]";
			return strm.str();
		}

		virtual glm::mat4 getTransform(float delta);

		// setNode isn't a member function, since it needs to be able to set
		// the shared pointer parent
		gameObject::ptr getNode(std::string name);
		void removeNode(std::string name);
		bool hasNode(std::string name);

		// used for routing click events
		size_t id = allocateObjID();
		// TODO: bounding box/radius

		// transform relative to parent
		TRS transform;
		std::vector<animationChannel::ptr> animations;
		bool visible = true;

		gameObject::weakptr parent;
		std::map<std::string, gameObject::ptr> nodes;
		// for unlinking when the object is removed
		std::shared_ptr<physicsObject> physObj;
		GLenum face_order  = GL_CCW;
};

static inline
void setNode(std::string name, gameObject::ptr obj, gameObject::ptr sub) {
	assert(obj != nullptr && sub != nullptr);

	obj->nodes[name] = sub;
	sub->parent = obj;
}

// TODO: just realized these overload syscalls, should that be avoided?
//       in principle it's fine, might be confusing in code though
gameObject::ptr unlink(gameObject::ptr node);
gameObject::ptr clone(gameObject::ptr node);     // shallow copy
gameObject::ptr duplicate(gameObject::ptr node); // deep copy

class gameImport : public gameObject {
	public:
		typedef std::shared_ptr<gameImport> ptr;
		typedef std::weak_ptr<gameImport>   weakptr;

		gameImport(std::string path)
			: gameObject(objType::Import), sourceFile(path) {}

		virtual std::string typeString(void) {
			return "Imported file";
		}

		virtual std::string idString(void) {
			std::stringstream strm;
			strm << "[" << typeString() << " : " << sourceFile <<  "]";
			return strm.str();
		}

		std::string sourceFile;
};

class gameSkin : public gameObject {
	public:
		typedef std::shared_ptr<gameSkin> ptr;
		typedef std::weak_ptr<gameSkin>   weakptr;

		gameSkin() : gameObject(objType::Skin) {}

		virtual std::string typeString(void) {
			return "Skin";
		}

		std::vector<glm::mat4> inverseBind;
		// keep internal pointers to joints, same nodes as in the tree
		std::vector<gameObject::ptr> joints;
};

// forward declaration defined in glManager.hpp
// ugh, this is becoming a maze of forward declarations...
class Buffer;

class gameParticles : public gameObject {
	public:
		typedef std::shared_ptr<gameParticles> ptr;
		typedef std::weak_ptr<gameParticles>   weakptr;

		gameParticles(unsigned _maxInstances = 256);

		virtual std::string typeString(void) {
			return "Particle system";
		}

		void update(void);
		void syncBuffer(void);

		std::vector<glm::mat4> positions;
		// approximate bounding sphere for instances in this object,
		// used for culling
		float radius;
		bool synced = false;
		unsigned activeInstances;
		unsigned maxInstances;

		std::shared_ptr<Buffer> ubuffer = nullptr;
};

class gameLight : public gameObject {
	public:
		typedef std::shared_ptr<gameLight> ptr;
		typedef std::weak_ptr<gameLight> weakptr;

		enum lightTypes {
			None,
			Point,
			Spot,
			Directional,
			Rectangular,  // TODO:
			Area,         // TODO:
			BoundedPoint, // TODO:
		} lightType;

		gameLight(enum lightTypes t)
			: gameObject(objType::Light), lightType(t) {};

		virtual std::string typeString(void) {
			return "Light (abstract)";
		}

		virtual float extent(float threshold=0.03) = 0;
		// TODO: 'within(threshold, pos)' to test if a light affects a given point

		// TODO: why not store things in std140 format to avoid packing?
		glm::vec4 diffuse = glm::vec4(1);
		float     intensity = 50.0;
		bool      casts_shadows = false;
		bool changed   = true;
		bool is_static = true;
		bool have_map  = false;
};

class gameLightPoint : public gameLight {
	public:
		typedef std::shared_ptr<gameLightPoint> ptr;
		typedef std::weak_ptr<gameLightPoint> weakptr;

		gameLightPoint() : gameLight(lightTypes::Point) {};

		virtual std::string typeString(void) {
			return "Point light";
		}

		virtual float extent(float threshold=0.03);

		float radius = 1.0f;
		// TODO: maybe abstract atlas textures more
		quadtree::node_id shadowmap[6];
};

class gameLightSpot : public gameLight {
	public:
		typedef std::shared_ptr<gameLightSpot> ptr;
		typedef std::weak_ptr<gameLightSpot> weakptr;

		gameLightSpot() : gameLight(lightTypes::Spot) {};

		virtual std::string typeString(void) {
			return "Spot light";
		}

		virtual float extent(float threshold=0.03);

		float radius = 1.0;
		float angle = 3.1415/4.0;

		// TODO: maybe abstract atlas textures more
		quadtree::node_id shadowmap;
};

class gameLightDirectional : public gameLight {
	public:
		typedef std::shared_ptr<gameLightDirectional> ptr;
		typedef std::weak_ptr<gameLightDirectional> weakptr;

		gameLightDirectional() : gameLight(lightTypes::Directional) {};

		virtual std::string typeString(void) {
			return "Directional light";
		}

		virtual float extent(float threshold=0.03);

		// TODO: maybe abstract atlas textures more
		quadtree::node_id shadowmap;
};

class gameReflectionProbe : public gameObject {
	public:
		typedef std::shared_ptr<gameReflectionProbe> ptr;
		typedef std::weak_ptr<gameReflectionProbe> weakptr;

		virtual std::string typeString(void) {
			return "Reflection probe";
		}

		gameReflectionProbe() : gameObject(objType::ReflectionProbe) {};
		quadtree::node_id faces[5][6];
		// bounding box for parallax correction
		AABB boundingBox = {
			.min = glm::vec3(-1),
			.max = glm::vec3(1),
		};

		bool changed = true;
		bool is_static = true;
		bool have_map = false;
		bool have_convolved = false;
};

class gameIrradianceProbe : public gameObject {
	public:
		typedef std::shared_ptr<gameIrradianceProbe> ptr;
		typedef std::weak_ptr<gameIrradianceProbe> weakptr;

		virtual std::string typeString(void) {
			return "Irradiance probe";
		}

		gameIrradianceProbe() : gameObject(objType::IrradianceProbe) {
			source = std::make_shared<gameReflectionProbe>();
		};

		gameReflectionProbe::ptr source;
		quadtree::node_id faces[6];
		quadtree::node_id coefficients[6];

		AABB boundingBox = {
			.min = glm::vec3(-1),
			.max = glm::vec3(1),
		};

		bool changed = true;
		bool is_static = true;
		bool have_map = false;
};

// namespace grendx
}

#include <grend/physics.hpp>
#include <grend/glManager.hpp>

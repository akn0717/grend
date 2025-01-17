#include <grend/sceneNode.hpp>
#include <grend/glManager.hpp>
#include <grend/utility.hpp>
#include <math.h>

using namespace grendx;

sceneNode::~sceneNode() {
	// TODO: toggleable debug log, or profile events, etc
	//std::cerr << "Freeing a " << idString() << std::endl;
}

// XXX: "key functions", needed to do dynamic_cast across .so boundries
//      Requires that the function be a "non-inline, non-pure virtual function"
sceneImport::~sceneImport() {};
sceneSkin::~sceneSkin() {};
sceneParticles::~sceneParticles() {};
sceneBillboardParticles::~sceneBillboardParticles() {};
sceneLight::~sceneLight() {};
sceneLightPoint::~sceneLightPoint() {};
sceneLightSpot::~sceneLightSpot() {};
sceneLightDirectional::~sceneLightDirectional() {};
sceneReflectionProbe::~sceneReflectionProbe() {};
sceneIrradianceProbe::~sceneIrradianceProbe() {};

size_t grendx::allocateObjID(void) {
	static size_t counter = 0;
	return ++counter;
}

// TODO: should have this return a const ref
TRS sceneNode::getTransformTRS() {
	return transform;
}

// TODO: should have this return a const ref
TRS sceneNode::getOrigTransform() {
	return origTransform;
}

// TODO: also const ref
glm::mat4 sceneNode::getTransformMatrix() {
	if (updated) {
		cachedTransformMatrix = transform.getTransform();
		updated = false;
		// note that queueCache.updated isn't changed here
	}

	return cachedTransformMatrix;
}

void sceneNode::setTransform(const TRS& t) {
	if (isDefault) {
		origTransform = t;
	}

	updated = true;
	queueCache.updated = true;
	isDefault = false;
	transform = t;
}

void sceneNode::setPosition(const glm::vec3& position) {
	TRS temp = getTransformTRS();
	temp.position = position;
	setTransform(temp);
}

void sceneNode::setScale(const glm::vec3& scale) {
	TRS temp = getTransformTRS();
	temp.scale = scale;
	setTransform(temp);
}

void sceneNode::setRotation(const glm::quat& rotation) {
	TRS temp = getTransformTRS();
	temp.rotation = rotation;
	setTransform(temp);
}

sceneNode::ptr sceneNode::getNode(std::string name) {
	return hasNode(name)? nodes[name] : nullptr;
}

sceneNode::ptr grendx::unlink(sceneNode::ptr node) {
	if (node != nullptr) {
		if (auto p = node->parent.lock()) {
			for (auto& [key, ptr] : p->nodes) {
				if (node == ptr) {
					sceneNode::ptr ret = p;
					p->nodes.erase(key);
					node->parent.reset();
					return ret;
				}
			}
		}
	}

	return node;
}

sceneNode::ptr grendx::clone(sceneNode::ptr node) {
	sceneNode::ptr ret = std::make_shared<sceneNode>();

	ret->setTransform(node->getTransformTRS());

	for (auto& [name, ptr] : node->nodes) {
		setNode(name, ret, ptr);
	}

	return ret;
}

static animationMap::ptr copyAnimationMap(animationMap::ptr anim) {
	auto ret = std::make_shared<animationMap>();

	for (auto& p : *anim) {
		ret->insert(p);
	}

	return ret;
}

static animationCollection::ptr copyCollection(animationCollection::ptr anims) {
	if (!anims) return nullptr;

	auto ret = std::make_shared<animationCollection>();

	for (auto& [name, anim] : *anims) {
		if (anim == nullptr)
			continue;

		ret->insert({name, copyAnimationMap(anim)});
	}

	return ret;
}

template <typename T>
static int indexOf(const std::vector<T>& vec, const T& value) {
	for (int i = 0; i < vec.size(); i++) {
		if (vec[i] == value) {
			return i;
		}
	}

	return -1;
}

static sceneNode::ptr copySkinNodes(sceneSkin::ptr target,
                                     sceneSkin::ptr skin,
                                     sceneNode::ptr node)
{
	if (node == nullptr) {
		return node;
	}

	if (node->type != sceneNode::objType::None) {
		// should only have pure node types under skin nodes
		return node;
	}

	sceneNode::ptr ret = std::make_shared<sceneNode>();

	// copy generic sceneNode members
	// XXX: two set transforms to ensure origTransform is the same
	ret->setTransform(node->getOrigTransform());
	ret->setTransform(node->getTransformTRS());
	ret->visible         = node->visible;
	ret->animChannel     = node->animChannel;
	ret->extraProperties = node->extraProperties;

	int i = indexOf(skin->joints, node);
	if (i >= 0) {
		target->joints[i] = ret;
	}

	// copy sub-nodes
	for (auto& [name, ptr] : node->nodes) {
		setNode(name, ret, copySkinNodes(target, skin, ptr));
	}

	return ret;
}

static void copySkin(sceneSkin::ptr target, sceneSkin::ptr skin) {
	target->inverseBind = skin->inverseBind;
	target->transforms  = skin->transforms;

	target->joints.resize(skin->joints.size());

	for (auto& [name, ptr] : skin->nodes) {
		setNode(name, target, copySkinNodes(target, skin, ptr));
	}
}

sceneNode::ptr grendx::duplicate(sceneNode::ptr node) {
	// TODO: need to copy all attributes

	sceneNode::ptr ret;

	// copy specific per-type object members
	switch (node->type) {
		case sceneNode::objType::None:
			ret = std::make_shared<sceneNode>();
			break;

		case sceneNode::objType::Import:
			{
				auto foo = std::static_pointer_cast<sceneImport>(node);
				auto temp = std::make_shared<sceneImport>(foo->sourceFile);
				temp->animations = copyCollection(foo->animations);

				ret = temp;
				break;
			}

		case sceneNode::objType::Skin:
			{
				ret = std::make_shared<sceneSkin>();

				// small XXX: need to copy skin information after copying subnodes
				auto skin = std::static_pointer_cast<sceneSkin>(node);
				auto temp = std::static_pointer_cast<sceneSkin>(ret);

				copySkin(temp, skin);
				return ret;
			}
			break;

		// TODO: meshes/models, particles shouldn't be doCopy, but would it make
		//       sense to copy other types like lights, cameras?
		default:
			return node;
	}

	// copy generic sceneNode members
	// XXX: two set transforms to ensure origTransform is the same
	ret->setTransform(node->getOrigTransform());
	ret->setTransform(node->getTransformTRS());
	ret->visible         = node->visible;
	ret->animChannel     = node->animChannel;
	ret->parent          = node->parent;
	ret->extraProperties = node->extraProperties;

	// copy sub-nodes
	for (auto& [name, ptr] : node->nodes) {
		setNode(name, ret, duplicate(ptr));
	}


	return ret;
}

std::string grendx::getNodeName(sceneNode::ptr node) {
	if (node != nullptr) {
		if (auto p = node->parent.lock()) {
			for (auto& [key, ptr] : p->nodes) {
				if (node == ptr) {
					return key;
				}
			}
		}
	}

	return "";
}

void sceneNode::removeNode(std::string name) {
	auto it = nodes.find(name);

	if (it != nodes.end()) {
		nodes.erase(it);
	}
}

bool sceneNode::hasNode(std::string name) {
	return nodes.find(name) != nodes.end();
}

float sceneLightPoint::extent(float threshold) {
	return radius * (sqrt((intensity * diffuse.w)/threshold) - 1);
}

float sceneLightSpot::extent(float threshold) {
	return radius * (sqrt((intensity * diffuse.w)/threshold) - 1);
}

float sceneLightDirectional::extent(float threshold) {
	// infinite extent
	return HUGE_VALF;
}

// TODO: better name
static glm::mat4 lookup(std::map<sceneNode*, glm::mat4>& cache,
                        sceneNode *root,
                        sceneNode *ptr)
{
	auto it = cache.find(ptr);

	if (it == cache.end()) {
		sceneNode::ptr parent = ptr->parent.lock();
		glm::mat4 mat(1);

		if (ptr != root && parent) {
			mat = lookup(cache, root, parent.get()) * ptr->getTransformMatrix();
		}

		cache[ptr] = mat;
		return mat;

	} else {
		return it->second;
	}
}

void sceneSkin::sync(Program::ptr program) { 
	size_t numjoints = min(inverseBind.size(), 256);

	if (transforms.size() != inverseBind.size()) {
		transforms.resize(inverseBind.size());
	}

#if GLSL_VERSION >= 300
	// use UBOs on gles3, core profiles
	if (!ubuffer) {
		ubuffer = genBuffer(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
		//ubuffer->allocate(sizeof(GLfloat[16*numjoints]));
		ubuffer->allocate(sizeof(GLfloat[16*256]));
	}
#endif

	std::map<sceneNode*, glm::mat4> accumTransforms;

	for (unsigned i = 0; i < inverseBind.size(); i++) {
		if (!joints[i]) {
			transforms[i] = glm::mat4(1);
			continue;
		}

		glm::mat4 accum = lookup(accumTransforms, this, joints[i].get());
		transforms[i] = accum*inverseBind[i];
	}

#if GLSL_VERSION < 300
	// no UBOs on gles2
	for (unsigned i = 0; i < transforms.size(); i++) {
		std::string sloc = "joints["+std::to_string(i)+"]";
		if (!prog->set(sloc, transforms[i])) {
			std::cerr <<
				"NOTE: couldn't set joint matrix " << i
				<< ", too many joints/wrong shader?" << std::endl;
			break;
		}
	}
#else
	ubuffer->update(transforms.data(), 0, sizeof(GLfloat[16*numjoints]));
	program->setUniformBlock("jointTransforms", ubuffer, UBO_JOINTS);
#endif
}

void sceneParticles::syncBuffer(void) {
	if (!ubuffer) {
		ubuffer = genBuffer(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
		ubuffer->allocate(sizeof(GLfloat[16*256]));
	}

	if (!synced) {
		ubuffer->update(positions.data(), 0, sizeof(GLfloat[16*activeInstances]));
		synced = true;
	}
}

void sceneParticles::update(void) {
	// just set a flag indicating that the buffer isn't synced,
	// will get synced in the render loop somewhere
	// (need to do it this way since things will be updated in threads, and
	// can't do anything to opengl state from non-main threads)
	synced = false;
}

sceneParticles::sceneParticles(unsigned _maxInstances)
	: sceneNode(objType::Particles)
{
	positions.reserve(_maxInstances);
	positions.resize(positions.capacity());

	maxInstances = _maxInstances;
	activeInstances = 0;
};

void sceneBillboardParticles::syncBuffer(void) {
	if (!ubuffer) {
		ubuffer = genBuffer(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
		ubuffer->allocate(sizeof(GLfloat[4*1024]));
	}

	if (!synced) {
		ubuffer->update(positions.data(), 0, sizeof(GLfloat[4*activeInstances]));
		synced = true;
	}
}

void sceneBillboardParticles::update(void) {
	// just set a flag indicating that the buffer isn't synced,
	// will get synced in the render loop somewhere
	// (need to do it this way since things will be updated in threads, and
	// can't do anything to opengl state from non-main threads)
	synced = false;
}

sceneBillboardParticles::sceneBillboardParticles(unsigned _maxInstances)
	: sceneNode(objType::BillboardParticles)
{
	positions.reserve(_maxInstances);
	positions.resize(positions.capacity());

	maxInstances = _maxInstances;
	activeInstances = 0;
};

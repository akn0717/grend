#pragma once

#include <grend/physics.hpp>
#include <grend/ecs/ecs.hpp>

namespace grendx::ecs {

class rigidBody : public component {
	public:
		rigidBody(entityManager *manager, entity *ent, float _mass)
			: component(manager, ent)
		{
			manager->registerComponent(ent, "rigidBody", this);
			mass = _mass;
		}

		void registerCollisionQueue(std::shared_ptr<std::vector<collision>> queue) {
			if (phys) {
				phys->collisionQueue = queue;
			}
			// TODO: should show warning if there's no physics object
		}

		void syncPhysics(entity *ent) {
			if (ent && ent->node && phys) {
				ent->node->transform = phys->getTransform();
			}
		}

		physicsObject::ptr phys = nullptr;
		float mass;
};

class rigidBodySphere : public rigidBody {
	public:
		rigidBodySphere(entityManager *manager,
		                entity *ent,
		                glm::vec3 position,
		                float mass,
		                float radius)
			: rigidBody(manager, ent, mass)
		{
			manager->registerComponent(ent, "rigidBodySphere", this);
			phys = manager->engine->phys->addSphere(ent, position, mass, radius);
		}
};

class rigidBodyBox : public rigidBody {
	public:
		rigidBodyBox(entityManager *manager,
		             entity *ent,
		             glm::vec3 position,
		             float mass,
		             AABBExtent& box)
			: rigidBody(manager, ent, mass)
		{
			manager->registerComponent(ent, "rigidBodyBox", this);
			phys = manager->engine->phys->addBox(ent, position, mass, box);
		}
};

// namespace grendx::ecs
};

#include "boxSpawner.hpp"
#include <grend/geometryGeneration.hpp>

boxBullet::boxBullet(entityManager *manager, gameMain *game, glm::vec3 position)
	: projectile(manager, game, position)
{
	static gameObject::ptr model = nullptr;
	static gameLightPoint::ptr lit = std::make_shared<gameLightPoint>();

	manager->registerComponent(this, "boxBullet", this);

	if (!model) {
#if LOCAL_BUILD
		model = loadScene("test-assets/obj/smoothcube.glb");
#else
		gameModel::ptr mod = generate_cuboid(0.3, 0.3, 0.3);
		model = std::make_shared<gameObject>();
		compileModel("boxProjectile", mod);
		setNode("model", model, mod);
#endif
		bindCookedMeshes();

		lit->radius = 0.15;
		lit->intensity = 500;
	}


	setNode("model", node, model);
	setNode("light", node, lit);
}

void boxSpawner::handleInput(entityManager *manager, entity *ent, inputEvent& ev)
{
	if (ev.active && ev.type == inputEvent::types::primaryAction) {
		std::cerr << "boxSpawner::handleInput(): got here" << std::endl;
		glm::mat3 noderot = glm::mat3_cast(ent->node->transform.rotation);
		glm::vec3 playerrot = noderot*glm::vec3(0, 0, 1);

		//auto box = new boxBullet(manager, manager->engine, ent->node->transform.position + 3.f*ev.data);
		auto box = new boxBullet(manager, manager->engine, ent->node->transform.position + 2.f*playerrot);

		rigidBody *body;
		castEntityComponent(body, manager, box, "rigidBody");

		if (body) {
			body->phys->setVelocity(40.f * playerrot);
			manager->add(box);
		}
	}
}
#include "levelController.hpp"

void levelController::addInit(levelController::initFunc func) {
	initializers.push_back(func);
}

void levelController::addDestructor(destructFunc func) {
	destructors.push_back(func);
}

void levelController::addLoseCondition(loseFunc func) {
	loseConditions.push_back(func);
}

void levelController::addObjective(std::string description,
                                   levelController::objectiveFunc func)
{
	objectives[description] = func;
	objectivesCompleted[description] = false;
}

void levelController::reset(void) {
	for (auto& f : destructors) {
		f(game);
	}

	for (auto& f : initializers) {
		f(game);
	}
}

bool levelController::won(void) {
	bool allSatisfied = true;

	for (auto& [desc, f] : objectives) {
		bool completed = f(game);

		objectivesCompleted[desc] = completed;
		allSatisfied &= objectivesCompleted[desc];
	}

	return allSatisfied;
}

std::pair<bool, std::string> levelController::lost(void) {
	for (auto& f : loseConditions) {
		auto result = f(game);

		if (!result.first) {
			return result;
		}
	}

	return {false, ""};
}

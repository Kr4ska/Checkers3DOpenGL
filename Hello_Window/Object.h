#pragma once
#include "model.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

class Object
{
public:
	bool visible;
	std::string name;
	Model model;
	glm::vec3 position;

	void move(glm::vec3);
	void newPos(glm::vec3);
	Object(std::string, Model, glm::vec3, glm::vec3, float);
	~Object();

	void scaleModel(float scale) {
		model.setScale(scale);
	}

	void rotateModel(const glm::vec3& angles) {
		model.rotate(angles);
	}
};

void Object::move(glm::vec3 diretion) {
	model.move(diretion);
	position = model.position;
}

void Object::newPos(glm::vec3 new_pos) {
	glm::vec3 _stepOnNewPos = new_pos - position;
	position = new_pos;

	model.move(_stepOnNewPos);
}

Object::Object(std::string name_, Model model_, glm::vec3 position_, glm::vec3 rotate_ = glm::vec3{0.0f}, float scale_ = 1.0f) :name(name_), model(model_), position(position_), visible(true) {
	model.move(position);
	scaleModel(scale_);
	rotateModel(rotate_);
}

Object::~Object() {

}

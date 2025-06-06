#pragma once
#include "model.h"
#include "Object.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

class Checker : public Object
{
public:
	Checker(std::string, Model, glm::vec3);
	~Checker();

	void setKing();
	bool getKing();
	bool isWhite() const { return name.find("White") != std::string::npos; }
	void newPos(glm::vec3 new_pos) {
		Object::newPos(new_pos + glm::vec3(0.0f, king == true ? 1.0f : 0.0f, 0.0f));
	}
private:
	bool king;
};

Checker::Checker(std::string name_, Model model_, glm::vec3 position_ = { 0.0f, 0.0f, 0.0f }) :
										Object("Checker " + name_, model_, position_), king(false){}

Checker::~Checker()
{
}
void Checker::setKing() { king = true; rotateModel({ 0.0f, 0.0f, 180.0f }); move(glm::vec3(0.0, model.checkBox.height, 0.0f)); }
bool Checker::getKing() { return king; }
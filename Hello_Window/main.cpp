#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "model.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <vector>
#include <cmath>

using std::vector;

// --- Константы ---
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

// --- Камера ---
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
glm::mat4 projection, view;

// --- Тайминг ---
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- Модели ---
vector<Model*> arrModel;
Model* selectedModel = nullptr;
bool modelSelected = false;

// --- Состояние ввода ---
namespace Input {
    bool altPressedLastFrame = false;
    bool cursorLocked = true;
    glm::dvec2 cursorPosBeforeLock;
}

// --- Структура луча ---
struct Ray {
    glm::vec3 origin, direction, end;
};

// --- Прототипы ---
void framebuffer_size_callback(GLFWwindow*, int, int);
void cursorPositionCallback(GLFWwindow*, double, double);
void scrollCallback(GLFWwindow*, double, double);
void mouseCallback(GLFWwindow*, int, int, int);
void processInput(GLFWwindow*);
void keyCallback(GLFWwindow*, int, int, int, int);

// --- Генерация луча ---
Ray GeneratePickingRay(int mouseX, int mouseY, int screenWidth, int screenHeight,
    const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    float x = (2.0f * mouseX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / screenHeight;
    glm::mat4 invViewProj = inverse(projectionMatrix * viewMatrix);
    glm::vec4 nearPoint = invViewProj * glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farPoint = invViewProj * glm::vec4(x, y, 1.0f, 1.0f);

    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    return { glm::vec3(nearPoint), glm::normalize(glm::vec3(farPoint - nearPoint)), glm::vec3(farPoint) };
}

// --- Проверка пересечения луча с цилиндром ---
bool RayHitboxIntersection(const Ray& ray, const HitBox& hitbox, float& t) {
    glm::mat4 invModel = glm::translate(glm::mat4(1.0f), -hitbox.position);
    glm::vec3 localOrigin = invModel * glm::vec4(ray.origin, 1.0f);
    glm::vec3 localDir = invModel * glm::vec4(ray.direction, 0.0f);

    float a = localDir.x * localDir.x + localDir.z * localDir.z;
    float b = 2.0f * (localOrigin.x * localDir.x + localOrigin.z * localDir.z);
    float c = localOrigin.x * localOrigin.x + localOrigin.z * localOrigin.z - hitbox.radius * hitbox.radius;

    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0.0f) return false;

    float sqrtDisc = sqrt(discriminant);
    float t0 = (-b - sqrtDisc) / (2 * a);
    float t1 = (-b + sqrtDisc) / (2 * a);

    t = INFINITY;
    bool hit = false;

    for (float ti : {t0, t1}) {
        if (ti < 0.0f) continue;
        glm::vec3 point = localOrigin + localDir * ti;
        if (point.y >= 0.0f && point.y <= hitbox.height && ti < t) {
            t = ti;
            hit = true;
        }
    }

    auto checkCap = [&](float tCap, float yCap) {
        if (tCap <= 0.0f) return false;
        glm::vec3 point = localOrigin + localDir * tCap;
        return (point.x * point.x + point.z * point.z <= hitbox.radius * hitbox.radius);
        };

    float tBottom = -localOrigin.y / localDir.y;
    float tTop = (hitbox.height - localOrigin.y) / localDir.y;

    if (checkCap(tBottom, 0.0f) && tBottom < t) { t = tBottom; hit = true; }
    if (checkCap(tTop, hitbox.height) && tTop < t) { t = tTop; hit = true; }

    return hit;
}

// --- Главная функция ---
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGL for Ravesli.com!", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouseCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    stbi_set_flip_vertically_on_load(true);
    glEnable(GL_DEPTH_TEST);

    Shader shader("../6.multiple_lights.vs", "../6.multiple_lights.fs");
    Model model1("../resources/objects/shashka v4/shashka v4.obj");
    Model model2("../resources/objects/shashka v4/shashka v4.obj");
    model2.move(glm::vec3(5.0f));

    arrModel = { &model1, &model2 };

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.5f, 0.55f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.setVec3("viewPos", camera.Position);
        shader.setFloat("material.shininess", 32.0f);

        shader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
        shader.setVec3("dirLight.ambient", glm::vec3(0.05f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.4f));
        shader.setVec3("dirLight.specular", glm::vec3(0.5f));

        shader.setVec3("spotLight.position", camera.Position);
        shader.setVec3("spotLight.direction", camera.Front);
        shader.setVec3("spotLight.ambient", glm::vec3(0.0f));
        shader.setVec3("spotLight.diffuse", glm::vec3(1.0f));
        shader.setVec3("spotLight.specular", glm::vec3(1.0f));
        shader.setFloat("spotLight.constant", 1.0f);
        shader.setFloat("spotLight.linear", 0.09f);
        shader.setFloat("spotLight.quadratic", 0.032f);
        shader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
        shader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

        projection = glm::perspective(glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f);
        view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        for (auto model : arrModel)
            model->Draw(shader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
// Вспомогательные функции
namespace InputHelpers {

    // Меняет режим захвата курсора
    void ToggleCursorLock(GLFWwindow* window) {
        Input::cursorLocked = !Input::cursorLocked;
        if (Input::cursorLocked) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetCursorPos(window, Input::cursorPosBeforeLock.x, Input::cursorPosBeforeLock.y);
        }
        else {
            glfwGetCursorPos(window, &Input::cursorPosBeforeLock.x, &Input::cursorPosBeforeLock.y);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    // Проверяет, кликнули ли по модели, и сохраняет её в selectedModel
    void TrySelectModel(GLFWwindow* window, double x, double y) {
        float hitT;
        Ray ray = GeneratePickingRay((int)x, (int)y, SCR_WIDTH, SCR_HEIGHT, view, projection);
        for (Model* m : arrModel) {
            if (RayHitboxIntersection(ray, m->checkBox, hitT)) {
                selectedModel = m;
                modelSelected = true;
                return;
            }
        }
    }

    // Перемещает выбранную модель по стрелкам
    void MoveSelectedModel(int key) {
        const float speed = 0.05f;
        if (!modelSelected) return;

        glm::vec3 delta(0.0f);
        switch (key) {
        case GLFW_KEY_UP:    delta.z = -speed; break;
        case GLFW_KEY_DOWN:  delta.z = speed; break;
        case GLFW_KEY_LEFT:  delta.x = -speed; break;
        case GLFW_KEY_RIGHT: delta.x = speed; break;
        default: return;
        }
        selectedModel->move(delta);
    }

}

// --- CALLBACK на клик мыши ---
void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !Input::cursorLocked) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        InputHelpers::TrySelectModel(window, x, y);
    }
}

// --- CALLBACK на движение мыши (мышиный look) ---
void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    static bool initialized = false;
    if (!initialized) {
        glfwSetCursorPos(window, lastX, lastY);
        initialized = true;
        return;
    }

    if (!Input::cursorLocked) return;

    // вычисляем смещение от центра и отправляем в камеру
    double dx = xpos - lastX;
    double dy = lastY - ypos; // инвертируем Y, чтобы ↑ было +dy
    camera.ProcessMouseMovement((float)dx, (float)dy);

    // возвращаем курсор в центр
    glfwSetCursorPos(window, lastX, lastY);
}

// --- CALLBACK на колёсико мыши ---
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll((float)yoffset);
}

// --- CALLBACK на нажатие клавиш ---
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // ALT: переключить режим захвата курсора (обработка edge only)
    if (key == GLFW_KEY_LEFT_ALT && action == GLFW_PRESS && !Input::altPressedLastFrame) {
        InputHelpers::ToggleCursorLock(window);
        Input::altPressedLastFrame = true;
    }
    if (key == GLFW_KEY_LEFT_ALT && action == GLFW_RELEASE) {
        Input::altPressedLastFrame = false;
    }

    // ESC: либо отменить выбор модели, либо выйти
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (modelSelected) {
            modelSelected = false;
            selectedModel = nullptr;
        }
        else {
            glfwSetWindowShouldClose(window, true);
        }
    }

    // Передвижение выбранной модели по стрелкам
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        InputHelpers::MoveSelectedModel(key);
    }
}

// --- ОБРАБОТКА обычного WASD-движения камеры (вызывается в основном цикле) ---
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
}

// --- CALLBACK при изменении размера окна ---
void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

// Refactored OpenGL Application
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "shader.h"
#include "camera.h"
#include "model.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <vector>

using std::vector;

// Constants
static constexpr unsigned int SCR_WIDTH = 1600;
static constexpr unsigned int SCR_HEIGHT = 900;

// Ray structure for picking
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    glm::vec3 end;
};

class Application {
public:
    Application();
    ~Application();
    int run();

private:
    // Window and timing
    GLFWwindow* window_ = nullptr;
    double deltaTime_ = 0.0f;
    double lastFrame_ = 0.0f;

    // Camera & view/projection
    Camera camera_{ glm::vec3(0.0f, 0.0f, 3.0f) };
    glm::mat4 projection_;
    glm::mat4 view_;
    float lastX_ = SCR_WIDTH / 2.0f;
    float lastY_ = SCR_HEIGHT / 2.0f;
    bool firstMouse_ = true;

    // Selection & input state
    vector<Model*> models_;
    Model* selectedModel_ = nullptr;
    bool modelSelected_ = false;
    bool cursorLocked_ = true;
    bool altPressed_ = false;
    glm::dvec2 preLockPos_;

    // Shader
    Shader* shader_ = nullptr;

    // Initialization helpers
    bool initWindow();
    void setupCallbacks();
    void loadResources();

    // Main loop
    void processInput();
    void update();
    void render();

    // Callbacks handlers
    void onFramebufferSize(int width, int height);
    void onCursorMove(double xpos, double ypos);
    void onScroll(double yoffset);
    void onKey(int key, int scancode, int action, int mods);
    void onMouseButton(int button, int action);

    // Helpers
    Ray generateRay(int x, int y) const;
    bool testIntersection(const Ray& ray, const HitBox& box, float& t) const;
    void toggleCursorLock();
    void trySelect(double x, double y);
    void moveSelected(int key);
};

// Implementations

Application::Application() {
    if (!initWindow()) std::exit(EXIT_FAILURE);
    setupCallbacks();
    loadResources();
} 

Application::~Application() {
    delete shader_;
    delete selectedModel_;
    glfwTerminate();
}

//Основной цикл
int Application::run() {
    while (!glfwWindowShouldClose(window_)) {
        double current = glfwGetTime();
        deltaTime_ = current - lastFrame_;
        lastFrame_ = current;

        processInput();
        update();
        render();

        glfwSwapBuffers(window_);
        glfwPollEvents();
    }
    return 0;
}

//Инициализацию нужных переменных и глобальная настройка
bool Application::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Refactored OpenGL", nullptr, nullptr);
    if (!window_) { std::cerr << "GLFW window creation failed\n"; return false; }
    glfwMakeContextCurrent(window_);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }
    stbi_set_flip_vertically_on_load(true);
    glEnable(GL_DEPTH_TEST);

    return true;
}

//Установка CALLBACK's
void Application::setupCallbacks() {
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->onFramebufferSize(width, height); 
        });
    glfwSetCursorPosCallback(window_, [](GLFWwindow* w, double x, double y) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->onCursorMove(x, y);
        });
    glfwSetScrollCallback(window_, [](GLFWwindow* w, double, double y) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->onScroll(y);
        });
    glfwSetKeyCallback(window_, [](GLFWwindow* w, int k, int s, int a, int m) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->onKey(k, s, a, m);
        });
    glfwSetMouseButtonCallback(window_, [](GLFWwindow* w, int b, int a, int) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->onMouseButton(b, a);
        });
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

//Загрузка ресурсов
void Application::loadResources() {
    shader_ = new Shader("../6.multiple_lights.vs", "../6.multiple_lights.fs");

    shader_->use();
    shader_->setFloat("material.shininess", 32.0f);

    //Глобальное освещение(Солнечное)
    shader_->setVec3("dirLight.direction", -0.3f, -1.0f, 0.2f);
    shader_->setVec3("dirLight.ambient", glm::vec3(0.3f));
    shader_->setVec3("dirLight.diffuse", glm::vec3(0.8f));
    shader_->setVec3("dirLight.specular", glm::vec3(0.5f));

    //Локальное освещение (фонарик)
    shader_->setVec3("spotLight.ambient", glm::vec3(0.0f));
    shader_->setVec3("spotLight.diffuse", glm::vec3(1.0f));
    shader_->setVec3("spotLight.specular", glm::vec3(1.0f));
    shader_->setFloat("spotLight.constant", 1.0f);
    shader_->setFloat("spotLight.linear", 0.09f);
    shader_->setFloat("spotLight.quadratic", 0.032f);
    shader_->setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
    shader_->setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));
    

    models_.push_back(new Model("../resources/objects/shashka v4/shashka v4.obj"));
    models_.push_back(new Model("../resources/objects/shashka v4/shashka v4.obj"));

    models_[1]->move(glm::vec3( 5.0f, 0.0f, 0.0f ));
}

//Передвижение камеры на WASD
void Application::processInput() {
    if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) camera_.ProcessKeyboard(FORWARD, deltaTime_);
    if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) camera_.ProcessKeyboard(BACKWARD, deltaTime_);
    if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) camera_.ProcessKeyboard(LEFT, deltaTime_);
    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) camera_.ProcessKeyboard(RIGHT, deltaTime_);
}

//Обновление переменных на каждый кадр
void Application::update() {
    view_ = camera_.GetViewMatrix();
    projection_ = glm::perspective(glm::radians(camera_.Zoom), float(SCR_WIDTH) / SCR_HEIGHT, 0.1f, 100.0f);
}

//Основной редндер
void Application::render() {
    glClearColor(0.5f, 0.55f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader_->use();
    shader_->setVec3("viewPos", camera_.Position);

    // lights setup omitted
    shader_->setMat4("view", view_);
    shader_->setMat4("projection", projection_);

    shader_->setVec3("spotLight.position", camera_.Position);
    shader_->setVec3("spotLight.direction", camera_.Front);


    for (auto m : models_) m->Draw(*shader_);
}


void Application::onFramebufferSize(int w, int h) {
    glViewport(0, 0, w, h);
}

void Application::onCursorMove(double xpos, double ypos) {
    if (!cursorLocked_) return;
    if (firstMouse_) {
        lastX_ = float(xpos); lastY_ = float(ypos); firstMouse_ = false;
        return;
    }
    float dx = float(xpos) - lastX_;
    float dy = lastY_ - float(ypos);
    lastX_ = float(xpos); lastY_ = float(ypos);
    camera_.ProcessMouseMovement(dx, dy);
}

void Application::onScroll(double yoffset) {
    camera_.ProcessMouseScroll(float(yoffset));
}

void Application::onKey(int key, int, int action, int) {
    if (key == GLFW_KEY_LEFT_ALT && action == GLFW_PRESS && !altPressed_) {
        toggleCursorLock(); altPressed_ = true;
    }
    if (key == GLFW_KEY_LEFT_ALT && action == GLFW_RELEASE) altPressed_ = false;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (modelSelected_) { modelSelected_ = false; selectedModel_ = nullptr; }
        else glfwSetWindowShouldClose(window_, true);
    }
    if ((action == GLFW_PRESS) &&
        (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT || key == GLFW_KEY_UP || key == GLFW_KEY_DOWN)) {
        moveSelected(key);
    }
}

void Application::onMouseButton(int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !cursorLocked_) {
        double x, y; glfwGetCursorPos(window_, &x, &y);
        trySelect(x, y);
    }
}

Ray Application::generateRay(int mouseX, int mouseY) const {
    float x = (2.0f * mouseX) / SCR_WIDTH - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / SCR_HEIGHT;
    glm::mat4 invVP = glm::inverse(projection_ * view_);
    glm::vec4 nearP = invVP * glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farP = invVP * glm::vec4(x, y, 1.0f, 1.0f);
    nearP /= nearP.w; farP /= farP.w;
    return { glm::vec3(nearP), glm::normalize(glm::vec3(farP - nearP)), glm::vec3(farP) };
}

bool Application::testIntersection(const Ray& ray, const HitBox& box, float& t) const {
    glm::mat4 invModel = glm::translate(glm::mat4(1.0f), -box.position);
    glm::vec3 o = invModel * glm::vec4(ray.origin, 1.0f);
    glm::vec3 d = invModel * glm::vec4(ray.direction, 0.0f);
    float a = d.x * d.x + d.z * d.z;
    float b = 2.0f * (o.x * d.x + o.z * d.z);
    float c = o.x * o.x + o.z * o.z - box.radius * box.radius;
    float disc = b * b - 4 * a * c;
    if (disc < 0) return false;
    float sd = sqrt(disc);
    float t0 = (-b - sd) / (2 * a), t1 = (-b + sd) / (2 * a);
    t = INFINITY; bool hit = false;
    for (float ti : {t0, t1}) {
        if (ti < 0) continue;
        glm::vec3 p = o + d * ti;
        if (p.y >= 0 && p.y <= box.height && ti < t) { t = ti; hit = true; }
    }
    auto cap = [&](float tc) { glm::vec3 p = o + d * tc; return (p.x * p.x + p.z * p.z <= box.radius * box.radius); };
    float tb = -o.y / d.y, tt = (box.height - o.y) / d.y;
    if (tb > 0 && cap(tb) && tb < t) { t = tb; hit = true; }
    if (tt > 0 && cap(tt) && tt < t) { t = tt; hit = true; }
    return hit;
}

void Application::toggleCursorLock() {
    cursorLocked_ = !cursorLocked_;
    if (cursorLocked_) {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPos(window_, preLockPos_.x, preLockPos_.y);
    }
    else {
        glfwGetCursorPos(window_, &preLockPos_.x, &preLockPos_.y);
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Application::trySelect(double x, double y) {
    float nearest = 0;
    Ray ray = generateRay(int(x), int(y));
    for (auto m : models_) {
        float t;
        if (testIntersection(ray, m->checkBox, t)) {
            selectedModel_ = m;
            modelSelected_ = true;
            return;
        }
    }
}

void Application::moveSelected(int key) {
    if (!modelSelected_) return;
    const float speed = 0.05f;
    glm::vec3 d(0.0f);
    switch (key) {
    case GLFW_KEY_UP:    d.z = -speed; break;
    case GLFW_KEY_DOWN:  d.z = speed; break;
    case GLFW_KEY_LEFT:  d.x = -speed; break;
    case GLFW_KEY_RIGHT: d.x = speed; break;
    default: return;
    }
    selectedModel_->move(d);
}

int main() {
    Application app;
    return app.run();
}

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader_m.h"
#include "camera.h"
#include "model.h"


#include <iostream>
#include "stb_image.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void mouseCallback(GLFWwindow* window, int button, int action, int mods);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
// Константы
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

// Камера
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

glm::mat4 projection;
glm::mat4 view;

// Загрузка моделей
vector <Model*> arrModel;
Model* temp = nullptr;
bool modelEnabled = false;
// Тайминги
float deltaTime = 0.0f;
float lastFrame = 0.0f;

namespace Input {
    bool altPressedLastFrame = false;
    bool cursorLocked = true;
    glm::dvec2 cursorPosBeforeLock;
}

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    glm::vec3 end;
};

// Генерация луча из позиции курсора
Ray GeneratePickingRay(int mouseX, int mouseY, int screenWidth, int screenHeight,
    const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    // Нормализация координат мыши
    float x = (2.0f * mouseX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / screenHeight;

    // Матрицы преобразования
    glm::mat4 invViewProj = inverse(projectionMatrix * viewMatrix);

    // Ближняя и дальняя точки
    glm::vec4 nearPoint = invViewProj * glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farPoint = invViewProj * glm::vec4(x, y, 1.0f, 1.0f);

    // Перспективное деление
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    return {
        glm::vec3(nearPoint),
        glm::normalize(glm::vec3(farPoint - nearPoint)),
        glm::vec3(farPoint)
    };
}

// Проверка пересечения луча с цилиндром
bool RayHitboxIntersection(const Ray& ray, const BoundingBox& hitbox, float& t) {
    // Преобразование луча в локальное пространство цилиндра
    glm::mat4 invModel = inverse(glm::translate(glm::mat4(1.0f), hitbox.position));
    glm::vec3 localOrigin = invModel * glm::vec4(ray.origin, 1.0f);
    glm::vec3 localDir = invModel * glm::vec4(ray.direction, 0.0f);

    // Уравнение цилиндра (ось Y)
    float a = localDir.x * localDir.x + localDir.z * localDir.z;
    float b = 2.0f * (localOrigin.x * localDir.x + localOrigin.z * localDir.z);
    float c = localOrigin.x * localOrigin.x + localOrigin.z * localOrigin.z - hitbox.radius * hitbox.radius;

    // Решение квадратного уравнения
    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0.0f) return false;

    float sqrtDisc = sqrt(discriminant);
    float t0 = (-b - sqrtDisc) / (2 * a);
    float t1 = (-b + sqrtDisc) / (2 * a);

    // Поиск ближайшего пересечения в пределах высоты
    bool hit = false;
    t = INFINITY;

    for (float ti : {t0, t1}) {
        if (ti < 0.0f) continue;

        glm::vec3 point = localOrigin + localDir * ti;
        if (point.y >= 0.0f && point.y <= hitbox.height) {
            if (ti < t) {
                t = ti;
                hit = true;
            }
        }
    }

    // Проверка оснований
    if (hitbox.height > 0.0f) {
        // Нижнее основание
        float tBottom = (-localOrigin.y) / localDir.y;
        if (tBottom > 0.0f) {
            glm::vec3 bottomPoint = localOrigin + localDir * tBottom;
            if (bottomPoint.x * bottomPoint.x + bottomPoint.z * bottomPoint.z <= hitbox.radius * hitbox.radius) {
                if (tBottom < t) {
                    t = tBottom;
                    hit = true;
                }
            }
        }

        // Верхнее основание
        float tTop = (hitbox.height - localOrigin.y) / localDir.y;
        if (tTop > 0.0f) {
            glm::vec3 topPoint = localOrigin + localDir * tTop;
            if (topPoint.x * topPoint.x + topPoint.z * topPoint.z <= hitbox.radius * hitbox.radius) {
                if (tTop < t) {
                    t = tTop;
                    hit = true;
                }
            }
        }
    }

    return hit;
}

int main()
{
    // glfw: инициализация и конфигурирование
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw: создание окна
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGL for Ravesli.com!", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouseCallback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, keyCallback);

    // Сообщаем GLFW, чтобы он захватил наш курсор
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: загрузка всех указателей на OpenGL-функции
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Сообщаем stb_image.h, чтобы он перевернул загруженные текстуры относительно y-оси (до загрузки модели)
    stbi_set_flip_vertically_on_load(true);

    // Конфигурирование глобального состояния OpenGL
    glEnable(GL_DEPTH_TEST);

    // Компилирование нашей шейдерной программы
    Shader ourShader("../6.multiple_lights.vs", "../6.multiple_lights.fs");

    Model ourModel("../resources/objects/shashka v4/shashka v4.obj");
    arrModel.push_back(&ourModel);

    Model secondModel("../resources/objects/shashka v4/shashka v4.obj");
    secondModel.move(glm::vec3(5.0f));
    arrModel.push_back(&secondModel);
    // Отрисовка в режиме каркаса
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Цикл рендеринга
    while (!glfwWindowShouldClose(window))
    {
        // Логическая часть работы со временем для каждого кадра
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Обработка ввода
        processInput(window);

        // Рендеринг
        glClearColor(0.5f, 0.55f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Убеждаемся, что активировали шейдер прежде, чем настраивать uniform-переменные/объекты_рисования
                // Убеждаемся, что активировали шейдер прежде, чем настраивать uniform-переменные/объекты_рисования
        ourShader.use();
        ourShader.setVec3("viewPos", camera.Position);
        ourShader.setFloat("material.shininess", 32.0f);


        // Направленный свет
        ourShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
        ourShader.setVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
        ourShader.setVec3("dirLight.diffuse", 0.4f, 0.4f, 0.4f);
        ourShader.setVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);

        // Прожектор
        ourShader.setVec3("spotLight.position", camera.Position);
        ourShader.setVec3("spotLight.direction", camera.Front);
        ourShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
        ourShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
        ourShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.09);
        ourShader.setFloat("spotLight.quadratic", 0.032);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
        ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

        // Преобразования Вида/Проекции
        projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        secondModel.Draw(ourShader);
        ourModel.Draw(ourShader);

        // glfw: обмен содержимым front- и back- буферов. Отслеживание событий ввода/вывода (была ли нажата/отпущена кнопка, перемещен курсор мыши и т.п.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // glfw: завершение, освобождение всех выделенных ранее GLFW-реcурсов
    glfwTerminate();
    return 0;
}

void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
    if (!Input::cursorLocked){
        if (GLFW_MOUSE_BUTTON_LEFT == button && action == GLFW_PRESS) {
            double x, y;
            float f;
            glfwGetCursorPos(window, &x, &y);
            for (int i = 0; i < arrModel.size(); i++)
            {
                if (RayHitboxIntersection(GeneratePickingRay(x, y, SCR_WIDTH, SCR_HEIGHT, view, projection), arrModel[i]->checkBox, f)) {
                    temp = arrModel[i];
                    modelEnabled = true;
                }
            }
        }
    }
}

// Обработка всех событий ввода: запрос GLFW о нажатии/отпускании кнопки мыши в данном кадре и соответствующая обработка данных событий
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    bool altPressed = false;
    if (altPressed = (key == GLFW_KEY_LEFT_ALT && action == GLFW_PRESS)) {

        // Переключение режима только при изменении состояния
        if (!Input::altPressedLastFrame)
            Input::cursorLocked = !Input::cursorLocked;

        if (Input::cursorLocked) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

            // Возвращаем сохраненную позицию
            glfwSetCursorPos(window,
                Input::cursorPosBeforeLock.x,
                Input::cursorPosBeforeLock.y);
        }
        else {
            // Режим нормальный - показать курсор
            // Сохраняем текущую позицию перед разблокировкой
            glfwGetCursorPos(window,
                &Input::cursorPosBeforeLock.x,
                &Input::cursorPosBeforeLock.y);

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    Input::altPressedLastFrame = altPressed;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        if (modelEnabled)
        {
            modelEnabled = false;
            temp = nullptr;
        }
        else
            glfwSetWindowShouldClose(window, true);
    }
    if (modelEnabled) {
        float speed = 0.05f;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            temp->move(glm::vec3(0.0f, 0.0f, -speed));
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            temp->move(glm::vec3(0.0f, 0.0f, speed));
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            temp->move(glm::vec3(-speed, 0.0f, 0.0f));
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            temp->move(glm::vec3(speed, 0.0f, 0.0f));
    }

};

// glfw: всякий раз, когда изменяются размеры окна (пользователем или операционной системой), вызывается данная callback-функция
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // Убеждаемся, что окно просмотра соответствует новым размерам окна.
    // Обратите внимание, ширина и высота будут значительно больше, чем указано, на Retina-дисплеях
    glViewport(0, 0, width, height);
}

// glfw: всякий раз, когда перемещается мышь, вызывается данная callback-функция
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    static bool firstFrame = true;
    if (firstFrame) {
        glfwSetCursorPos(window, SCR_WIDTH / 2.0, SCR_HEIGHT / 2.0);
        firstFrame = false;
        return;
    }
    static bool lastFrameCursorLock = true;

    if (Input::cursorLocked) {
        // Вычисляем дельту движения
        glm::dvec2 delta = glm::dvec2(xpos - SCR_WIDTH / 2.0, ypos - SCR_HEIGHT / 2.0);

        if(lastFrameCursorLock)
            camera.ProcessMouseMovement(delta.x, -delta.y);
        lastFrameCursorLock = true;
        // Центрирование курсора
        glfwSetCursorPos(window, SCR_WIDTH / 2.0, SCR_HEIGHT / 2.0);
    }
    else {
        lastFrameCursorLock = false;
    }

}

// glfw: всякий раз, когда прокручивается колесико мыши, вызывается данная callback-функция
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}
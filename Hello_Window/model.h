#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mesh.h"
#include "shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

struct HitBox {
    glm::vec3 position; // Центр основания цилиндра
    float radius;
    float height;
};

class Model 
{
public:
    // Данные модели 
    HitBox checkBox;
    glm::vec3 position;
    float scale;
    glm::vec3 rotation;
    vector<Texture> textures_loaded; // (оптимизация) сохраняем все загруженные текстуры, чтобы убедиться, что они не загружены более одного раза
    vector<Mesh> meshes;
    string directory;
    bool gammaCorrection;
    // Конструктор в качестве аргумента использует путь к 3D-модели
    Model(string const& path, bool gamma = false, glm::vec3 position_ = { 0.0f, 0.0f, 0.0f }, float scale_ = 1.0f, glm::vec3 rotation_ = { 0.0f, 0.0f, 0.0f })
        : gammaCorrection(gamma), position(position_), scale(scale_), rotation(rotation_)
    {
        loadModel(path);
    }
    void setScale(float newScale) { scale *= newScale; checkBox.radius *= newScale; }
    void rotate(const glm::vec3& angles) { rotation += angles; }
    // Отрисовываем модель, а значит и все её меши
    void Draw(Shader shader) {
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, position);
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        modelMatrix = glm::scale(modelMatrix, glm::vec3(1.0f) * scale);
        shader.setMat4("model", modelMatrix);

        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

    void move(glm::vec3 direction) {
        position += direction;
        checkBox.position += direction;
    }
private:
    // Загружаем модель с помощью Assimp и сохраняем полученные меши в векторе meshes
    void loadModel(string const &path)
    {
        // Чтение файла с помощью Assimp
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // Проверка на ошибки
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // если НЕ 0
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
		
        // Получение пути к файлу
        directory = path.substr(0, path.find_last_of('/'));

        // Рекурсивная обработка корневого узла Assimp
        processNode(scene->mRootNode, scene);

        //Генерацию Хит-бокса исходя из модели (цилиндрическая)
        checkBox = generateHitBox();
    }

    // Рекурсивная обработка узла. Обрабатываем каждый отдельный меш, расположенный в узле, и повторяем этот процесс для своих дочерних углов (если таковы вообще имеются)
    void processNode(aiNode *node, const aiScene *scene)
    {
        // Обрабатываем каждый меш текущего узла
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // Узел содержит только индексы объектов в сцене.
            // Сцена же содержит все данные; узел - это лишь способ организации данных
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // После того, как мы обработали все меши (если таковые имелись), мы начинаем рекурсивно обрабатывать каждый из дочерних узлов
        for(unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }

    }

    HitBox generateHitBox() {
        HitBox hitbox;
        glm::vec3 min(FLT_MAX, FLT_MAX, FLT_MAX);
        glm::vec3 max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        // Обходим все меши в сцене/
        for (const auto mesh : meshes) {
            //const aiMesh* mesh = scene->mMeshes[m];

            // Обходим все вершины меша
            for (const auto& vertex : mesh.vertices) {

                // Обновляем минимальные и максимальные значения
                min.x = std::min(min.x, vertex.Position.x);
                min.y = std::min(min.y, vertex.Position.y);
                min.z = std::min(min.z, vertex.Position.z);

                max.x = std::max(max.x, vertex.Position.x);
                max.y = std::max(max.y, vertex.Position.y);
                max.z = std::max(max.z, vertex.Position.z);
            }
        }

        // Вычисляем радиус как максимальное расстояние по XZ-плоскости
        float extentX = (max.x - min.x) / 2.0f;
        float extentZ = (max.z - min.z) / 2.0f;
        hitbox.radius = std::max(extentX, extentZ);

        // Вычисляем высоту
        hitbox.height = max.y - min.y;

        // Центр основания цилиндра 
        hitbox.position = glm::vec3(
            (min.x + max.x) / 2.0f,
            min.y,
            (min.z + max.z) / 2.0f
        );

        return hitbox;
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        // Данные для заполнения
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // Цикл по всем вершинам меша
        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // мы объявляем промежуточный вектор, т.к. Assimp использует свой собственный векторный класс, который не преобразуется напрямую в тип glm::vec3, поэтому сначала мы передаем данные в этот промежуточный вектор типа glm::vec3
            
			// Координаты
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
			
            // Нормали
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
			
            // Текстурные координаты
            if(mesh->mTextureCoords[0]) // если меш содержит текстурные координаты
            {
                glm::vec2 vec;
				
                // Вершина может содержать до 8 различных текстурных координат. Мы предполагаем, что мы не будем использовать модели,
                // в которых вершина может содержать несколько текстурных координат, поэтому мы всегда берем первый набор (0)
                vec.x = mesh->mTextureCoords[0][i].x; 
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
			
            // Касательный вектор
            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;
            vertex.Tangent = vector;
			
            // Вектор бинормали
            vector.x = mesh->mBitangents[i].x;
            vector.y = mesh->mBitangents[i].y;
            vector.z = mesh->mBitangents[i].z;
            vertex.Bitangent = vector;
            vertices.push_back(vertex);
        }
        // Теперь проходимся по каждой грани меша (грань - это треугольник меша) и извлекаем соответствующие индексы вершин
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
			
            // Получаем все индексы граней и сохраняем их в векторе indices
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }
		
        // Обрабатываем материалы
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];  
		
        // Мы вводим соглашение об именах сэмплеров в шейдерах. Каждая диффузная текстура будет называться 'texture_diffuseN',
        // где N - порядковый номер от 1 до MAX_SAMPLER_NUMBER. 
        // Тоже самое относится и к другим текстурам:
        // диффузная - texture_diffuseN
        // отражения - texture_specularN
        // нормали - texture_normalN

        // 1. Диффузные карты
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		
        // 2. Карты отражения
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		
        // 3. Карты нормалей
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
		
        // 4. Карты высот
        std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        
        // Возвращаем меш-объект, созданный на основе полученных данных
        return Mesh(vertices, indices, textures);
    }
    
    // Проверяем все текстуры материалов заданного типа и загружам текстуры, если они еще не были загружены.
    // Необходимая информация возвращается в виде структуры Texture
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
			
            // Проверяем, не была ли текстура загружена ранее, и если - да, то пропускаем загрузку новой текстуры и переходим к следующей итерации
            bool skip = false;
            for(unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true; // текстура с тем же путем к файлу уже загружена, переходим к следующей (оптимизация)
                    break;
                }
            }
            if(!skip)
            {   // если текстура еще не была загружена, то загружаем её
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture); // сохраняем текстуру в массиве с уже загруженными текстурами, тем самым гарантируя, что у нас не появятся без необходимости дубликаты текстур
            }
        }
        return textures;
    }
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}


#endif


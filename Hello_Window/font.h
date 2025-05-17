#pragma once
#include <map>
#include "shader.h"
#include <string>
#include <stdexcept>
#include <iostream>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

class Font {
public:
    struct Character {
        unsigned int TextureID;
        glm::ivec2   Size;
        glm::ivec2   Bearing;
        unsigned int Advance;
    };

    Font(const std::string& fontPath, unsigned int fontSize) {
        if (!loadFont(fontPath, fontSize)) {
            throw std::runtime_error("Failed to load font: " + fontPath);
        }
        setupBuffers();
    }

    ~Font() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        for (auto& pair : Characters) {
            glDeleteTextures(1, &pair.second.TextureID);
        }
    }

    void RenderText(const std::string& text, float x, float y, float scale,
        const glm::vec3& color, const glm::mat4& projection, Shader& shader) {
        GLint prevShader;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevShader);
        shader.use();
        shader.setMat4("projection", projection);
        shader.setVec3("textColor", color);

        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        // Итерация по Unicode code points строки (упрощенно)
        const char* str = text.c_str();
        while (*str) {
            unsigned int codepoint = decodeUTF8(&str);

            auto it = Characters.find(codepoint);
            if (it == Characters.end()) {
                it = Characters.find(0x003F); // '?' в Unicode
                if (it == Characters.end()) continue;
            }

            const Character& ch = it->second;

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            float vertices[6][4] = {
                {xpos,     ypos + h, 0.0f, 0.0f},
                {xpos,     ypos,     0.0f, 1.0f},
                {xpos + w, ypos,     1.0f, 1.0f},
                {xpos,     ypos + h, 0.0f, 0.0f},
                {xpos + w, ypos,     1.0f, 1.0f},
                {xpos + w, ypos + h, 1.0f, 0.0f}
            };

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            x += (ch.Advance >> 6) * scale;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glUseProgram(prevShader);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glActiveTexture(GL_TEXTURE0);
    }

private:
    std::map<unsigned int, Character> Characters; // Ключ - Unicode code point
    unsigned int VAO, VBO;

    bool loadFont(const std::string& fontPath, unsigned int fontSize) {
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            std::cerr << "ERROR::FREETYPE: Failed to initialize library\n";
            return false;
        }

        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
            std::cerr << "ERROR::FREETYPE: Failed to load font: " << fontPath << "\n";
            FT_Done_FreeType(ft);
            return false;
        }

        FT_Set_Pixel_Sizes(face, 0, fontSize);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Загрузка ASCII (32-126)
        for (unsigned int c = 32; c < 127; ++c) {
            loadGlyph(face, c);
        }

        // Загрузка кириллицы (U+0400 - U+04FF)
        for (unsigned int c = 0x0400; c <= 0x04FF; ++c) {
            loadGlyph(face, c);
        }

        // Загрузка резервного символа '?' (U+003F)
        loadGlyph(face, 0x003F);

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        if (Characters.empty()) {
            std::cerr << "ERROR::FONT: No glyphs loaded\n";
            return false;
        }

        return true;
    }

    void loadGlyph(FT_Face face, unsigned int codepoint) {
        if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
            std::cerr << "WARNING::FREETYPE: Failed to load glyph: U+"
                << std::hex << codepoint << "\n";
            return;
        }

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RED, // Внутренний формат: RED
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED, // Формат данных
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );


        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Characters[codepoint] = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
    }

    void setupBuffers() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Декодирование UTF-8 (упрощенная версия)
    unsigned int decodeUTF8(const char** str) {
        unsigned int codepoint = 0;
        unsigned char byte = static_cast<unsigned char>(**str);
        if (byte < 0x80) {
            codepoint = byte;
            (*str)++;
        }
        else if ((byte & 0xE0) == 0xC0) {
            codepoint = ((byte & 0x1F) << 6) | (static_cast<unsigned char>(*(*str + 1)) & 0x3F);
            (*str) += 2;
        }
        else if ((byte & 0xF0) == 0xE0) {
            codepoint = ((byte & 0x0F) << 12) | ((static_cast<unsigned char>(*(*str + 1) & 0x3F) << 6) | (static_cast<unsigned char>(*(*str + 2)) & 0x3F));
            (*str) += 3;
        }
        else {
            // Неподдерживаемый символ
            (*str)++;
            return 0xFFFD; // Символ замены
        }
        return codepoint;
    }
};
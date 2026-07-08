#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#include <string>
#include <iostream>

#include <learnopengl/shader.h>

// Estructura que guarda todo lo necesario para dibujar un caracter individual
struct Character
{
    unsigned int TextureID;  // ID de la textura del glifo
    glm::ivec2   Size;       // Tamaño del glifo
    glm::ivec2   Bearing;    // Offset desde la línea base hasta la esquina superior-izquierda
    unsigned int Advance;    // Distancia horizontal hasta el siguiente glifo
};

// -----------------------------------------------------------------------------
// TextRenderer: dibuja texto 2D (HUD) encima de la escena 3D, y también
// rectángulos de fondo semi-transparentes (RenderQuad) para resaltar el texto.
// Se usa DESPUÉS de dibujar todos los modelos 3D del frame, ya que desactiva
// el depth test temporalmente para que el HUD quede siempre "al frente".
// -----------------------------------------------------------------------------
class TextRenderer
{
public:
    std::map<char, Character> Characters;
    Shader TextShader;
    Shader BoxShader;

    TextRenderer(unsigned int screenWidth, unsigned int screenHeight)
        : TextShader("shaders/text.vs", "shaders/text.fs"),
        BoxShader("shaders/box.vs", "shaders/box.fs")
    {
        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(screenWidth),
            0.0f, static_cast<float>(screenHeight));

        TextShader.use();
        TextShader.setMat4("projection", projection);

        BoxShader.use();
        BoxShader.setMat4("projection", projection);

        // --- Buffers para el texto (quad dinámico por glifo) ---
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // --- Buffers para el quad de fondo (rectángulo unitario reutilizable) ---
        float quadVertices[] = {
            0.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,

            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Carga una fuente .ttf y genera las texturas de los primeros 128 caracteres ASCII
    bool Load(const std::string& fontPath, unsigned int fontPixelSize)
    {
        FT_Library ft;
        if (FT_Init_FreeType(&ft))
        {
            std::cout << "ERROR::FREETYPE: No se pudo inicializar FreeType" << std::endl;
            return false;
        }

        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
        {
            std::cout << "ERROR::FREETYPE: No se pudo cargar la fuente: " << fontPath << std::endl;
            return false;
        }

        FT_Set_Pixel_Sizes(face, 0, fontPixelSize);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // desactiva el alineamiento de 4 bytes por defecto

        for (unsigned char c = 0; c < 128; c++)
        {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            {
                std::cout << "ERROR::FREETYPE: Fallo al cargar el glifo '" << c << "'" << std::endl;
                continue;
            }

            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RED,
                face->glyph->bitmap.width, face->glyph->bitmap.rows,
                0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer
            );

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Character character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<unsigned int>(face->glyph->advance.x)
            };
            Characters.insert(std::pair<char, Character>(c, character));
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return true;
    }

    // Dibuja un string en coordenadas de pantalla (origen abajo-izquierda, como OpenGL)
    void RenderText(const std::string& text, float x, float y, float scale, glm::vec3 color)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST); // el texto siempre debe verse encima de la escena 3D

        TextShader.use();
        TextShader.setVec3("textColor", color);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        for (char c : text)
        {
            Character ch = Characters[c];

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },

                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            x += (ch.Advance >> 6) * scale; // el advance viene en 1/64 de píxel
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    // Dibuja un rectángulo de color sólido (con transparencia) en coordenadas de pantalla.
    // x, y = esquina INFERIOR IZQUIERDA del rectángulo (mismo sistema que RenderText).
    // width, height = tamaño en píxeles.
    // color = rgb del rectángulo. alpha = opacidad (0.0 transparente, 1.0 opaco).
    // Útil como fondo detrás de texto para mejorar la legibilidad.
    void RenderQuad(float x, float y, float width, float height, glm::vec3 color, float alpha)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        BoxShader.use();
        BoxShader.setVec2("offset", glm::vec2(x, y));
        BoxShader.setVec2("size", glm::vec2(width, height));
        BoxShader.setVec4("boxColor", glm::vec4(color, alpha));

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    // Útil para centrar texto: calcula el ancho total en píxeles antes de dibujarlo
    float GetTextWidth(const std::string& text, float scale)
    {
        float width = 0.0f;
        for (char c : text)
            width += (Characters[c].Advance >> 6) * scale;
        return width;
    }

private:
    unsigned int VAO, VBO;
    unsigned int quadVAO, quadVBO;
};

#endif
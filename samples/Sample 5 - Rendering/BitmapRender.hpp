#pragma once
// GLM
#include "shader.hpp"

#include <Ultralight/Ultralight.h>

namespace render {
namespace bitmap {
// shader
static std::string vertexCode = R"(
#version 330 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 tex;
out vec2 TexCoord;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(pos, 0.0, 1.0);
    TexCoord = tex;
}
)";

static std::string fragmentCode = R"(
#version 330 core
in vec2 TexCoord;
out vec4 color;

uniform sampler2D image;

void main()
{
    color = texture(image, TexCoord);
}
)";

class BitmapRender {
  private:
    Shader shader;
    GLuint VAO, VBO, EBO;
    GLuint TextureID; // ID handle of the bitmap texture

    // VBO 定点顺序
    constexpr static GLuint VBOIndices[6] = {
        0, 1, 2, // 第一个三角形
        0, 2, 3  // 第二个三角形
    };

  public:
    BitmapRender() : TextureID(0), shader(vertexCode, fragmentCode) {
        // ctor
        //  Set OpenGL options
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Configure VAO/VBO/EBO for texture quads
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // opengl 中 分配 4 * 4 个 float 的内存(通过 EBO 实现定点复用)
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4 * 4, NULL, GL_DYNAMIC_DRAW);
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(VBOIndices), VBOIndices, GL_STATIC_DRAW);
        // 设置 location=0 pointer (position)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *)0);
        glEnableVertexAttribArray(0);
        // 设置 location=1 pointer (texture)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *)(2 * sizeof(GL_FLOAT)));
        glEnableVertexAttribArray(1);

        shader.use();
        // Disable byte-alignment restriction
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // init texture ID
        glGenTextures(1, &TextureID);
        glBindTexture(GL_TEXTURE_2D, TextureID);
        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    ~BitmapRender() {
        glDeleteTextures(1, &TextureID);
    }
    void Render(float leftBottomX, float leftBottomY, float rightTopX, float rightTopY) {
        // Activate corresponding render state
        shader.use();
        // 激活纹理序号 0
        glActiveTexture(GL_TEXTURE0);
        // 绑定 VAO 和 EBO
        glBindVertexArray(VAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

        // Update VBO for each character
        GLfloat vertices[4][4] = {// 顶点坐标 (x, y)       纹理坐标 (s, t)
                                  {leftBottomX, rightTopY, 0.0, 0.0},
                                  {leftBottomX, leftBottomY, 0.0, 1.0},
                                  {rightTopX, leftBottomY, 1.0, 1.0},
                                  {rightTopX, rightTopY, 1.0, 0.0}};

        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, TextureID);

        // 更新 VBO 缓冲区的内容
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        // Render quad
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // 解绑 VAO/VBO/EBO 和纹理
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Update(ultralight::RefPtr<ultralight::Bitmap> bitmap) {
        shader.use();

        GLfloat projection[4][4] = {{2.0f / bitmap->width(), 0, 0, 0},
                                    {0, 2.0f / bitmap->height(), 0, 0},
                                    {0, 0, -1.0, 0},
                                    {-1.0, -1.0, 0, 1.0}};
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, &projection[0][0]);

        void *pixels = bitmap->LockPixels();
        glBindTexture(GL_TEXTURE_2D, TextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width(), bitmap->height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels);
        bitmap->UnlockPixels();
    }
};

} // namespace bitmap
} // namespace render

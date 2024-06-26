#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <GL/glew.h>
#include <SDL_image.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "memory.h"
#include "ula.h"
#include "video.h"

namespace Video {
    const char *vertex_src = R"(
#version 120
varying vec2 TexCoord;
void main(){
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    TexCoord = vec2(gl_MultiTexCoord0);
}
)";
    const char *fragment_src = R"(
#version 120
#extension GL_EXT_gpu_shader4 : enable
varying vec2 TexCoord;
uniform sampler2D Screen;
//0.84615
const vec4 Palette[0x10] = vec4[0x10](
    vec4(0.0, 0.0, 0.0, 1.0), // 0 black
    vec4(0.0, 0.0, 0.8, 1.0), // 1 blue
    vec4(0.8, 0.0, 0.0, 1.0), // 2 red
    vec4(0.8, 0.0, 0.8, 1.0), // 3 mangenta
    vec4(0.0, 0.8, 0.0, 1.0), // 4 green
    vec4(0.0, 0.8, 0.8, 1.0), // 5 Cyan
    vec4(0.8, 0.8, 0.0, 1.0), // 6 Yellow
    vec4(0.8, 0.8, 0.8, 1.0), // 7 White
    vec4(0.0, 0.0, 0.0, 1.0), // 0 black
    vec4(0.0, 0.0, 1.0, 1.0), // 1 blue
    vec4(1.0, 0.0, 0.0, 1.0), // 2 red
    vec4(1.0, 0.0, 1.0, 1.0), // 3 mangenta
    vec4(0.0, 1.0, 0.0, 1.0), // 4 green
    vec4(0.0, 1.0, 1.0, 1.0), // 5 Cyan
    vec4(1.0, 1.0, 0.0, 1.0), // 6 Yellow
    vec4(1.0, 1.0, 1.0, 1.0) // 7 White
);
void main(){
    gl_FragColor = Palette[int(texture2D(Screen, TexCoord).r * 255.0)];
}
)";
    int viewport_width = -1;
    int viewport_height = -1;
    u16 *pbo_data = NULL;
    GLuint screen_texture = 0;
    GLuint palette_texture = 0;
    GLuint pbo = 0;
    GLuint vertex = 0;
    GLuint fragment = 0;
    GLuint program = 0;
    GLushort palette[0x100];

    void shader_error(const char *msg, GLuint shader){
        GLchar info[0x100];
        glGetShaderInfoLog(shader, sizeof(info), NULL, info);
        printf("%s: %s\n", msg, info);
        throw;
    }

    void setup(){
        GLint status;
        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &screen_texture);
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, ZX_SCREEN_WIDTH, ZX_SCREEN_HEIGHT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ZX_SCREEN_WIDTH, ZX_SCREEN_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glGenBuffers(1, &pbo);
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertex_src, NULL);
        glCompileShader(vertex);
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &status);
        if (!status)
            shader_error("Vertex shader", vertex);
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragment_src, NULL);
        glCompileShader(fragment);
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &status);
        if (!status)
            shader_error("Fragment shader", fragment);
        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (!status)
            shader_error("Link program", program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        vertex = fragment = 0;
    }

    void* update(){
        glUseProgram(program);
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, viewport_height);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(viewport_width, viewport_height);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(viewport_width, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glEnd();
        glUseProgram(0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, ZX_SCREEN_WIDTH * ZX_SCREEN_HEIGHT, NULL, GL_DYNAMIC_DRAW);
        return glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    }

    void frame(){
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ZX_SCREEN_WIDTH, ZX_SCREEN_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void set_filter(Filter filter){
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        switch (filter){
            case Nearest:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case Linear:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    void viewport_setup(int width, int height){
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        viewport_width = width;
        viewport_height = height;
    }

    void free(){
        if (screen_texture)
            glDeleteTextures(1, &screen_texture);
        if (pbo)
            glDeleteBuffers(1, &pbo);
        if (vertex)
            glDeleteShader(vertex);
        if (fragment)
            glDeleteShader(fragment);
        if (program)
            glDeleteProgram(program);
        screen_texture = pbo = vertex = fragment = program = 0;
    }
}

// The Seventh Night - milestone 1
// Window, forest scene from .glb files, 2.5D side camera with depth lanes,
// a lantern light that follows the player, fog, and a test "Will" control.
//
// Controls:  A/D move   Space jump   W/S change depth lane
//            Q/E lower/raise Will (lantern brightness)   Esc quit
//
// Run:  SeventhNight.exe path\to\Tree_1.glb path\to\Rock_1.glb ...
// Each model is placed in a row in the background lane so you can inspect it.

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Model.h"

static const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vPos;
out vec3 vNrm;
out vec2 vUV;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vPos = world.xyz;
    vNrm = mat3(transpose(inverse(uModel))) * aNrm;
    vUV = aUV;
    gl_Position = uProj * uView * world;
}
)";

static const char* kFragmentShader = R"(
#version 330 core
in vec3 vPos;
in vec3 vNrm;
in vec2 vUV;
uniform sampler2D uTex;
uniform int uHasTex;
uniform vec4 uColor;
uniform vec3 uLightPos;   // lantern position
uniform vec3 uCamPos;
uniform float uWill;      // 0..1, scales lantern brightness
out vec4 frag;
void main() {
    vec4 base = uColor;
    if (uHasTex == 1) base *= texture(uTex, vUV);
    vec3 n = normalize(vNrm);
    if (!gl_FrontFacing) n = -n;

    vec3 ambient = vec3(0.10, 0.13, 0.18);
    vec3 moon = vec3(0.15, 0.20, 0.30) * max(dot(n, normalize(vec3(-0.3, 1.0, 0.5))), 0.0);

    vec3 L = uLightPos - vPos;
    float d = length(L);
    L /= d;
    float att = 1.0 / (1.0 + 0.35 * d + 0.08 * d * d);
    vec3 lamp = vec3(1.0, 0.62, 0.28) * max(dot(n, L), 0.0) * att * 2.5 * uWill;

    vec3 col = base.rgb * (ambient + moon + lamp);

    float fd = length(uCamPos - vPos);
    float fog = 1.0 - exp(-pow(fd * 0.045, 1.6));
    col = mix(col, vec3(0.05, 0.07, 0.09), fog);
    frag = vec4(col, base.a);
}
)";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader error: %s\n", log);
    }
    return s;
}

static GLuint makeProgram() {
    GLuint vs = compile(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFragmentShader);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Link error: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

struct Player {
    glm::vec3 pos{0.0f, 0.0f, 0.0f};
    float velY = 0.0f;
    bool onGround = true;
    int lane = 1;  // 0 = foreground, 1 = middle, 2 = background
};

int main(int argc, char** argv) {
    if (!glfwInit()) {
        std::fprintf(stderr, "Could not start GLFW\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "The Seventh Night", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Could not create window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "Could not start GLEW\n");
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);  // simple and forgiving for placeholder shapes

    GLuint program = makeProgram();

    {  // scope: models are destroyed before the GL context goes away
    // Scene: ground, player placeholder, and any models given on the command line.
    Model ground = Model::groundQuad(80.0f, glm::vec4(0.12f, 0.17f, 0.12f, 1.0f));
    Model playerBox = Model::box(glm::vec3(0.6f, 1.7f, 0.4f), glm::vec4(0.25f, 0.45f, 0.85f, 1.0f));

    std::vector<Model> props;
    for (int i = 1; i < argc; ++i) {
        Model m;
        if (m.loadGLB(argv[i])) props.push_back(std::move(m));
    }

    const float laneZ[3] = {2.0f, 0.0f, -2.0f};
    Player player;
    float will = 1.0f;
    float camX = 0.0f;
    double last = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)std::min(now - last, 0.05);
        last = now;
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);

        // --- Input and movement ---
        float move = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += 1.0f;
        player.pos.x += move * 4.0f * dt;

        static bool wDown = false, sDown = false;
        bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        if (w && !wDown) player.lane = std::min(player.lane + 1, 2);
        if (s && !sDown) player.lane = std::max(player.lane - 1, 0);
        wDown = w;
        sDown = s;
        // Smoothly slide into the chosen lane.
        player.pos.z += (laneZ[player.lane] - player.pos.z) * std::min(1.0f, 10.0f * dt);

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && player.onGround) {
            player.velY = 6.5f;
            player.onGround = false;
        }
        player.velY -= 18.0f * dt;
        player.pos.y += player.velY * dt;
        if (player.pos.y <= 0.0f) {
            player.pos.y = 0.0f;
            player.velY = 0.0f;
            player.onGround = true;
        }

        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) will = std::max(0.0f, will - 0.6f * dt);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) will = std::min(1.0f, will + 0.6f * dt);

        // --- Camera: fixed side view that follows the player on x ---
        camX += (player.pos.x - camX) * std::min(1.0f, 4.0f * dt);
        glm::vec3 camPos(camX, 3.2f, 11.0f);
        glm::vec3 target(camX, 1.4f, 0.0f);
        glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        if (fbh == 0) fbh = 1;
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)fbw / fbh, 0.1f, 200.0f);

        // --- Draw ---
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.05f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(program, "uProj"), 1, GL_FALSE, &proj[0][0]);
        glUniform3fv(glGetUniformLocation(program, "uCamPos"), 1, &camPos[0]);
        glm::vec3 lantern = player.pos + glm::vec3(0.5f, 1.2f, 0.8f);
        glUniform3fv(glGetUniformLocation(program, "uLightPos"), 1, &lantern[0]);
        // A little flicker makes the lantern feel alive.
        float flicker = 0.92f + 0.08f * std::sin((float)now * 17.0f) * std::sin((float)now * 5.3f);
        glUniform1f(glGetUniformLocation(program, "uWill"), will * flicker);
        glUniform1i(glGetUniformLocation(program, "uTex"), 0);

        ground.draw(program, glm::mat4(1.0f));
        for (size_t i = 0; i < props.size(); ++i) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-8.0f + 5.0f * (float)i, 0.0f, laneZ[2] - 1.0f));
            props[i].draw(program, m);
        }
        playerBox.draw(program, glm::translate(glm::mat4(1.0f), player.pos));

        glfwSwapBuffers(window);
    }
    }  // end scope: Model destructors run here, while the GL context still exists

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

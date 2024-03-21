#include <cassert>
#include <iostream>
#include <string>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cstring>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <cmath>

class WaylandWindow
{
public:
    WaylandWindow(int width, int height) : width(width), height(height)
    {
        initWaylandDisplay();
        initEGLDisplay();
        initOpenGL();
    }

    ~WaylandWindow()
    {
        glDeleteProgram(programObject);
        wl_egl_window_destroy(wlEglWindow);
        eglDestroySurface(eglDisplay, eglSurface);
        eglTerminate(eglDisplay);
        wl_display_disconnect(wlDisplay);
    }

    void draw()
    {
        // 半径
        float circleRadius = 0.5f;

        // 頂点の数
        int numVertices = 4; // 四角形の頂点数

        // 頂点データ
        GLfloat vVertices[numVertices * 3]; // 3つの要素を持つnumVertices個の頂点

        // 頂点データを生成
        vVertices[0] = 0.0f;         // 左上 x座標
        vVertices[1] = circleRadius; // 左上 y座標
        vVertices[2] = 0.0f;         // 左上 z座標

        vVertices[3] = -circleRadius; // 左下 x座標
        vVertices[4] = 0.0f;          // 左下 y座標
        vVertices[5] = 0.0f;          // 左下 z座標

        vVertices[6] = 0.0f;          // 右下 x座標
        vVertices[7] = -circleRadius; // 右下 y座標
        vVertices[8] = 0.0f;          // 右下 z座標

        vVertices[9] = circleRadius; // 右上 x座標
        vVertices[10] = 0.0f;        // 右上 y座標
        vVertices[11] = 0.0f;        // 右上 z座標

        // 描画処理はそのままです
        glViewport(0, 0, width, height);
        glClearColor(0.9f, 0.9f, 0.9f, 0.5f); // 背景色を薄い灰色に設定（アルファ値0.5）
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(programObject);

        // ひし形を描画
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, numVertices);
    }

    void run()
    {
        draw();
        eglSwapBuffers(eglDisplay, eglSurface);
        while (wl_display_dispatch(wlDisplay) != -1)
        {
        }
    }

private:
    int width, height;
    wl_display *wlDisplay = nullptr;
    wl_surface *wlSurface = nullptr;
    wl_egl_window *wlEglWindow = nullptr;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    GLuint programObject = 0;

    struct WaylandGlobals
    {
        wl_compositor *compositor = nullptr;
        wl_shell *shell = nullptr;
    } globals;

    static void registryGlobal(void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
    {
        auto *globals = static_cast<WaylandGlobals *>(data);

        if (std::strcmp(interface, "wl_compositor") == 0)
        {
            globals->compositor = static_cast<wl_compositor *>(wl_registry_bind(registry, id, &wl_compositor_interface, 1));
        }
        else if (std::strcmp(interface, "wl_shell") == 0)
        {
            globals->shell = static_cast<wl_shell *>(wl_registry_bind(registry, id, &wl_shell_interface, 1));
        }
    }

    static const wl_registry_listener registryListener;

    void initWaylandDisplay()
    {
        std::cout << "Display接続" << std::endl;
        wlDisplay = wl_display_connect(nullptr);
        assert(wlDisplay);

        auto *registry = wl_display_get_registry(wlDisplay);
        wl_registry_add_listener(registry, &registryListener, &globals);

        wl_display_dispatch(wlDisplay);
        wl_display_roundtrip(wlDisplay);

        assert(globals.compositor && globals.shell);

        std::cout << "Surface生成" << std::endl;
        wlSurface = wl_compositor_create_surface(globals.compositor);
        assert(wlSurface);

        auto *shellSurface = wl_shell_get_shell_surface(globals.shell, wlSurface);
        wl_shell_surface_set_toplevel(shellSurface);

        wlEglWindow = wl_egl_window_create(wlSurface, width, height);
        assert(wlEglWindow);
    }

    void initEGLDisplay()
    {
        const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE};

        const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE};

        eglDisplay = eglGetDisplay((EGLNativeDisplayType)wlDisplay);
        assert(eglDisplay != EGL_NO_DISPLAY);
        assert(eglInitialize(eglDisplay, nullptr, nullptr) == EGL_TRUE);

        EGLConfig config;
        EGLint numConfig;
        assert(eglChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfig) == EGL_TRUE);
        assert(numConfig > 0);

        eglSurface = eglCreateWindowSurface(eglDisplay, config, (EGLNativeWindowType)wlEglWindow, nullptr);
        assert(eglSurface != EGL_NO_SURFACE);

        EGLContext eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
        assert(eglContext != EGL_NO_CONTEXT);

        assert(eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext) == EGL_TRUE);
    }

    void initOpenGL()
    {
        programObject = initProgramObject();
        assert(programObject != 0);
    }

    GLuint loadShader(GLenum type, const char *shaderSrc)
    {
        GLuint shader = glCreateShader(type);
        assert(shader);

        glShaderSource(shader, 1, &shaderSrc, nullptr);
        glCompileShader(shader);

        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        assert(compiled);

        return shader;
    }

    GLuint initProgramObject()
    {
        const char vShaderStr[] =
            "#version 300 es\n"
            "layout(location = 0) in vec4 vPosition;\n"
            "void main() {\n"
            "    gl_Position = vPosition;\n"
            "}\n";

        const char fShaderStr[] =
            "#version 300 es\n"
            "precision mediump float;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "    fragColor = vec4(0.0, 0.0, 1.0, 1.0);\n" // 青色に変更
            "}\n";

        GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vShaderStr);
        GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fShaderStr);

        GLuint program = glCreateProgram();
        assert(program);

        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);

        glLinkProgram(program);

        GLint linked;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        assert(linked);

        return program;
    }
};

const wl_registry_listener WaylandWindow::registryListener = {registryGlobal, nullptr};

int main(int argc, char **argv)
{
    int width = 320;
    int height = 320;
    WaylandWindow window(width, height);
    window.run();
    return 0;
}

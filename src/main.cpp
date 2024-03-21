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
        // 今回はWaylandに対してEGL+OpenGLを利用した。
        // もしEGLを利用せずにWayland環境で何かを描画したい場合、Waylandのプロトコルを直接利用する必要がある。
        // 例）WaylandクライアントはWaylandプロトコルを使用してサーフェスを作成し、そこに直接ピクセルデータを書き込むことで描画
        // しかし、上記ではOpenGLを使った3Dグラフィックスや高度な2Dグラフィックスのレンダリングには適していない。
        // 全てWaylandプロトコルで実装する方法はproitive.cppで検討
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

    // OpenGL ES を使用して単純な図形を描画
    void draw()
    {
        // 描画するひし形のサイズを制御するための半径を定義
        // この半径は、ひし形の頂点を計算する際に使用
        float circleRadius = 0.5f;

        // 頂点の数
        int numVertices = 4; // 四角形の頂点数

        // 頂点データ
        // 各頂点は3次元空間内の座標を持つため（x, y, z）、頂点データを格納するための配列を定義
        // 4つの頂点にそれぞれ3つの座標が必要なので、配列のサイズは12
        GLfloat vVertices[numVertices * 3];

        // 頂点データを生成
        vVertices[0] = 0.0f;         // 上 x座標
        vVertices[1] = circleRadius; // 上 y座標
        vVertices[2] = 0.0f;         // 上 z座標

        vVertices[3] = -circleRadius; // 左 x座標
        vVertices[4] = 0.0f;          // 左 y座標
        vVertices[5] = 0.0f;          // 左 z座標

        vVertices[6] = 0.0f;          // 下 x座標
        vVertices[7] = -circleRadius; // 下 y座標
        vVertices[8] = 0.0f;          // 下 z座標

        vVertices[9] = circleRadius; // 右 x座標
        vVertices[10] = 0.0f;        // 右 y座標
        vVertices[11] = 0.0f;        // 右 z座標

        // 描画領域のビューポートを設定
        // glViewport関数は、描画が行われるウィンドウのどの部分に表示されるかを定義する
        // ここでは、ビューポートをウィンドウ全体に設定
        glViewport(0, 0, width, height);

        // glClearColorで背景を塗りつぶす色を指定
        glClearColor(0.9f, 0.9f, 0.9f, 0.5f); // 背景色を薄い灰色に設定（アルファ値0.5）
        // glClearは実際にバッファをその色でクリアする
        glClear(GL_COLOR_BUFFER_BIT);

        // 画に使用するシェーダープログラムを指定
        // programObjectは、事前にコンパイルとリンクが完了したシェーダープログラムのID
        glUseProgram(programObject);

        // ひし形を描画
        // 第1引数（0）は、頂点属性のインデックスを指定。シェーダーで定義したlayout(location = 0) in vec4 vPosition;に対応している
        // 第2引数（3）は、頂点一つあたりのデータ数。3D座標（x, y, z）を使用しているため、3を指定
        // 第3引数（GL_FLOAT）は、データの型を指定。ここではGLfloat型を使用しているため、GL_FLOATを指定
        // 第4引数（GL_FALSE）はデータの正規化（0から1の間に収めるかどうか）を指定するが、ここでは正規化不要なのでGL_FALSEを指定
        // 第5引数（0）はストライドで、頂点データが配列に連続して格納されている場合は0を指定
        // 第6引数（vVertices）は、頂点データへのポインタを指定
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);

        // 特定の頂点属性配列を有効化。0は、有効にしたい属性のインデックスを示し、glVertexAttribPointerで設定したものど同様
        glEnableVertexAttribArray(0);

        // 頂点データを使用してプリミティブ（ここでは三角形）を描画
        // 第1引数（GL_TRIANGLE_FAN）は、描画するプリミティブのタイプを指定。GL_TRIANGLE_FANは、最初の頂点を中心として、残りの頂点がそれにファンのように連なる三角形を形成する
        // これにより、ひし形が2つの三角形から構成される
        // 第2引数（0）は、描画を開始する頂点のインデックスを指定
        // 第3引数（numVertices）は、描画に使用する頂点の数を指定。ここでは4つの頂点を使ってひし形を描画する
        glDrawArrays(GL_TRIANGLE_FAN, 0, numVertices);
    }

    void run()
    {
        // eglSwapBuffers関数は、ダブルバッファリングを使用している場合にバックバッファとフロントバッファを交換する機能を持つ。
        // この関数をコールすることで、前のdraw関数呼び出しによってバックバッファにレンダリングされた内容が画面に表示される
        // eglDisplayはEGLディスプレイコネクションを、eglSurfaceは描画が行われるサーフェスを示す
        eglSwapBuffers(eglDisplay, eglSurface);

        // Waylandディスプレイサーバーからのイベントを処理する
        // wl_display_dispatch関数はイベントキューを処理し、新しいイベントがあれば対応するリスナー関数を呼び出す
        // イベントが正常に処理された場合は0以上を、エラーが発生した場合は-1を返し、
        // 今回は、エラーが発生するまで（wl_display_dispatchが-1を返すまで）イベントを処理し続ける
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
        wl_shm *shm = nullptr;
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
        else if (strcmp(interface, "wl_shm") == 0)
        {
            globals->shm = static_cast<wl_shm *>(wl_registry_bind(registry, id, &wl_shm_interface, 1));
        }
    }

    static const wl_registry_listener registryListener;

    void initWaylandDisplay()
    {
        std::cout << "Waylandディスプレイサーバーへの接続" << std::endl;
        // NULLを指定すると、環境変数[WAYLAND_DISPLAY]に設定されているサーバー
        // 環境変数がなければ “wayland-0” という名前のサーバーに接続
        wlDisplay = wl_display_connect(nullptr);
        assert(wlDisplay);

        // ディスプレイサーバーとのI/Fで、何が使用可能なのかを取得
        // クライアントはレジストリからのイベントを購読し、それらのイベントが発生したときに特定のアクションを実行するためのコールバック関数（registryListener）を指定
        auto *registry = wl_display_get_registry(wlDisplay);
        wl_registry_add_listener(registry, &registryListener, &globals);

        // デフォルトのイベントキューに、イベントを配布（ディスパッチ）する
        wl_display_dispatch(wlDisplay);

        // 全リクエストがサーバーに送信され、サーバーからのすべてのイベントがアプリケーションによって処理されるまで「待機」
        wl_display_roundtrip(wlDisplay);

        // compositorは、クライアントが画面にウィンドウやサーフェスを描画するのに必要なオブジェクト
        // shellは、ウィンドウの管理やユーザーインターフェースの一部を担うオブジェクト
        assert(globals.compositor && globals.shell);

        std::cout << "Surface生成" << std::endl;
        // ウィンドウやウィジェットなど、画面に表示するための基本的な描画領域
        wlSurface = wl_compositor_create_surface(globals.compositor);
        assert(wlSurface);

        // サーフェスにウィンドウのような意味合いを与えるためのオブジェクトで、
        // サーフェスがトップレベルウィンドウかポップアップウィンドウかなどを管理
        auto *shellSurface = wl_shell_get_shell_surface(globals.shell, wlSurface);

        // トップレベルとして設定
        // 主にアプリケーションのメインインターフェースとして使われ、メニューバー、ツールバー、ステータスバー、コンテンツ表示エリアなどを含むこともある
        // またデスクトップ上で独立して存在し、通常、タイトルバー、最小化、最大化、閉じるボタンなどのウィンドウ管理機能が付随される。
        // （今回はなにも付随していない）
        wl_shell_surface_set_toplevel(shellSurface);
    }

    void initEGLDisplay()
    {
        // EGL:Embedded-System Graphics Library
        // EGLは描画が行われる環境（ウィンドウやディスプレイなど）とグラフィックスAPI（OpenGL等）の間の橋渡しをするAPI

        // wl_surfaceを基に、EGLを使用するためのウィンドウ（wl_egl_window）の作成
        // EGLは、OpenGL ESや他のグラフィックスAPIとネイティブウィンドウシステム間のインターフェイスを提供する
        // このwl_egl_windowオブジェクトは、後にEGLコンテキストやサーフェスを作成する際に使用される
        wlEglWindow = wl_egl_window_create(wlSurface, width, height);
        assert(wlEglWindow);

        // EGL設定の属性を定義
        const EGLint configAttribs[] = {
            // サーフェスタイプ
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            // カラーバッファサイズ
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            // レンダリングタイプ
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE};

        const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2, // OpenGL ES 2.0 を使用
            EGL_NONE};

        // WaylandディスプレイからEGLディスプレイコネクションを取得
        eglDisplay = eglGetDisplay((EGLNativeDisplayType)wlDisplay);
        assert(eglDisplay != EGL_NO_DISPLAY);                            // EGLディスプレイの取得が成功したことを確認
        assert(eglInitialize(eglDisplay, nullptr, nullptr) == EGL_TRUE); // EGLディスプレイを初期化し、成功したことを確認

        // 適切なEGLコンフィグを選択
        EGLConfig config;
        EGLint numConfig;
        assert(eglChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfig) == EGL_TRUE);
        assert(numConfig > 0);

        // EGLウィンドウサーフェスを作成
        // EGLウィンドウサーフェスは画面上のウィンドウや部分的な画面領域を表し、
        // OpenGL等のレンダリングAPIによって描画されたグラフィックスがここに表示される
        // → EGLウィンドウサーフェスは、レンダリングの結果が表示される「場所」
        // ちなみにウィンドウサーフェスは、バックバッファとフロントバッファの間で画像を交換する
        // ダブルバッファリングメカニズムをサポートしていることが一般的で、画像のちらつきを防ぎながらスムーズなアニメーションや描画が可能となる
        eglSurface = eglCreateWindowSurface(eglDisplay, config, (EGLNativeWindowType)wlEglWindow, nullptr);
        assert(eglSurface != EGL_NO_SURFACE);

        // EGLレンダリングコンテキストを作成
        // EGLレンダリングコンテキストはOpenGLのレンダリング状態、変数、設定を保持する
        // このコンテキストは、描画操作の現在の状態を表し、使用中のシェーダー、バインドされているテクスチャ、レンダリング設定などを含む
        // アプリケーションがグラフィックスAPIを使用してレンダリングを行うとき、
        // 全コマンドとリソースは特定のコンテキスト内で解釈され、実行されるため、
        // アプリケーションがレンダリングするには、有効なレンダリングコンテキストが必要
        // このコンテキストを介してグラフィックスハードウェアとやり取りする。
        // → EGLレンダリングコンテキストはレンダリングの「方法」や「状態」を保持するもの
        EGLContext eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
        assert(eglContext != EGL_NO_CONTEXT);

        // 作成したコンテキストとサーフェスをアクティブにし、成功したことを確認
        assert(eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext) == EGL_TRUE);
    }

    void initOpenGL()
    {
        // 初期のシェーダープログラムのIDを取得
        programObject = initProgramObject();
        assert(programObject != 0);
    }

    // OpenGL ESでシェーダーをロードし、コンパイルするための関数
    // シェーダーの種類（頂点シェーダーまたはフラグメントシェーダー）とシェーダーのソースコードを引数として受け取り、
    // コンパイルされたシェーダーのIDを返す
    GLuint loadShader(GLenum type, const char *shaderSrc)
    {
        // 指定されたタイプ（GL_VERTEX_SHADER または GL_FRAGMENT_SHADER）のシェーダーオブジェクトを作成し、
        // 作成されたシェーダーオブジェクトのIDを返す
        GLuint shader = glCreateShader(type);
        assert(shader);

        // glShaderSource関数によって、作成したシェーダーオブジェクトにソースコードを関連付ける
        // この関数はシェーダーオブジェクト、ソースコードの文字列数、ソースコードの文字列の配列、および各文字列の長さを指定する配列を引数として取る
        // ここでは、ソースコードが1つの文字列からなるため、文字列の数を1とする。
        // また最後の引数は各文字列の長さを指定する配列だが、nullptrを指定することで、文字列がnull終端であることを示す
        glShaderSource(shader, 1, &shaderSrc, nullptr);

        // シェーダーをコンパイル
        glCompileShader(shader);

        GLint compiled;
        // コンパイルが成功したかどうかをチェック
        // glGetShaderiv関数は、指定されたシェーダーオブジェクトの特定のパラメータの値を取得する
        // ここでは、シェーダーのコンパイル状態（GL_COMPILE_STATUS）を取得し、compiled変数に格納する
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        assert(compiled);

        // コンパイルされたシェーダーのIDを返却
        return shader;
    }

    GLuint initProgramObject()
    {
        // GLSL（OpenGL Shading Language）という言語を使用
        // 頂点シェーダーのGLSL
        // 頂点シェーダーは各頂点に適用されるシェーダーで、頂点の座標変換等を実施し、3Dモデルの各頂点に対して最初に実行されるシェーダー
        const char vShaderStr[] =
            "#version 300 es\n" // GLSLのバージョン(3.0)指定
            // vPositionという名前の4次元ベクトル（vec4）を定義
            // 頂点の位置を表すデータで、通常はx, y, zの座標とホモジニアス座標w（通常は1.0が設定）となる
            "layout(location = 0) in vec4 vPosition;\n"
            "void main() {\n"
            "    gl_Position = vPosition;\n" // 頂点位置を出力
            "}\n";

        // フラグメントシェーダーのGLSL
        // レンダリングされる各ピクセルに適用されるシェーダーで、ピクセルの最終色を計算する
        // テクスチャマッピング、ライティング、カラーブレンディングなどを行い、画面に表示される色や質感を決定する
        const char fShaderStr[] =
            "#version 300 es\n"          // GLSLのバージョン(3.0)指定
            "precision mediump float;\n" // 浮動小数点数の計算精度を指定。mediumpは中程度の精度
            // fragColorは4成分のベクトル（RGBA色）を保持し、レンダリングされるピクセルの色を示す
            // outキーワードは変数がシェーダーの出力であることを示す
            "out vec4 fragColor;\n"
            "void main() {\n"
            "    fragColor = vec4(0.0, 0.0, 1.0, 1.0);\n" // 青色を出力
            "}\n";

        // 頂点シェーダーをロードし、コンパイル
        GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vShaderStr);
        // フラグメントシェーダーをロードし、コンパイル
        GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fShaderStr);

        // 新しいシェーダープログラムのIDを生成
        // このIDは、シェーダーをリンクして最終的な実行可能なプログラムを作成するために使用する
        GLuint program = glCreateProgram();
        assert(program); // プログラムの作成が成功したことを確認

        // 先にコンパイルした頂点シェーダーとフラグメントシェーダーを
        // シェーダープログラムにアタッチ（関連付け）する。
        // これにより、これらのシェーダーはリンクのプロセスの一部となる
        glAttachShader(program, vertexShader);   // 頂点シェーダー
        glAttachShader(program, fragmentShader); // フラグメントシェーダー

        // アタッチされたシェーダーを使用してシェーダープログラムをリンク
        // リンクプロセスは、異なるシェーダーのコードを組み合わせて、最終的な実行可能なシェーダープログラムを作成する
        glLinkProgram(program);

        GLint linked;
        // シェーダープログラムが正常にリンクされたかどうかをチェックする。
        // リンクが成功すれば、linkedはGL_TRUEとなる
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        assert(linked);

        // プログラムのIDを返す
        return program;
    }
};

const wl_registry_listener WaylandWindow::registryListener = {registryGlobal, nullptr};
int main(int argc, char **argv)
{
    int width = 320;
    int height = 320;
    try
    {
        WaylandWindow window(width, height);
        window.draw();
        window.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#include <wayland-client.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

class WaylandWindow
{
public:
    WaylandWindow(int width, int height) : width(width), height(height)
    {
        initWaylandDisplay();
    }

    ~WaylandWindow()
    {
        if (shell_surface)
        {
            wl_shell_surface_destroy(shell_surface);
        }
        if (wlSurface)
        {
            wl_surface_destroy(wlSurface);
        }
        if (wlDisplay)
        {
            wl_display_disconnect(wlDisplay);
        }
    }

    void draw()
    {
        // 画像の1行あたりのバイト数（ストライド）を計算
        // 画像の幅（ピクセル単位）に4を掛けることで、1ピクセルあたり4バイト（32ビットカラーのABGRフォーマット）を使用
        const int stride = width * 4;
        // 画像全体のサイズ（バイト）の計算
        const int size = stride * height;

        // 画像全体のサイズと同じ大きさのファイルを作成し、そのファイルディスクリプタを取得
        int fd = create_anonymous_file(size);
        if (fd < 0)
        {
            throw std::runtime_error("Failed to create anonymous file");
        }

        // mmapを使用し、先ほど作成したファイルをプロセスのアドレス空間にマッピングし、そのアドレスをdataに格納する
        // MAP_SHAREDを指定することで、変更がファイル自体に書き戻され、他のマッピングしているプロセスと共有されるようになる
        auto *data = static_cast<uint32_t *>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (data == MAP_FAILED) // mmapに失敗した場合
        {
            close(fd);
            throw std::runtime_error("Failed to map memory");
        }

        // ABGR形式で各ピクセルに色を指定
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                uint32_t color = (x ^ y) & 0xff;
                data[y * width + x] = (255u << 24) | (color << 16) | (color << 8) | color; // ABGR
            }
        }

        // Waylandの共有メモリオブジェクトglobals.shmを使用し、共有メモリプールを作成
        wl_shm_pool *pool = wl_shm_create_pool(globals.shm, fd, size);

        // 共有メモリプールから、新しいバッファを作成
        // バッファは画像データの格納とWaylandサーフェスへの描画に使用される
        // パラメータはプール、オフセット（0）、幅、高さ、ストライド、ピクセルフォーマット（ARGB8888）
        wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

        // 作成したバッファをWaylandサーフェスにアタッチ
        // これにより、バッファの内容がサーフェスに表示される
        // オフセットを(0, 0)に指定することで、サーフェスの左上隅にバッファを配置
        wl_surface_attach(wlSurface, buffer, 0, 0);

        // サーフェスのどの部分が更新されたかをWaylandコンポジタに通知
        // 今回は、全サーフェスがダメージ（更新が必要）としてマーク
        wl_surface_damage(wlSurface, 0, 0, width, height);

        // サーフェスへの変更（バッファのアタッチやダメージの通知）をコミットし、Waylandコンポジタにこれらの変更を表示するよう指示
        wl_surface_commit(wlSurface);

        // Waylandディスプレイのバッファをフラッシュし、変更をコンポジタに即座に送信
        wl_display_flush(wlDisplay);

        // mmapでマッピングしたメモリ領域をアンマップ
        // メモリ領域へのアクセスが解除され、リソースが解放
        munmap(data, size);

        // ファイルディスクリプタをクローズ
        // ファイルのリソースが解放
        close(fd);

        // Waylandバッファを破棄
        // バッファに割り当てられていたリソースが解放
        wl_buffer_destroy(buffer);

        // 共有メモリプールを破棄
        // プールに関連付けられていたリソースが解放され、プールが使用していた共有メモリが解放
        wl_shm_pool_destroy(pool);
    }

    void run()
    {
        while (wl_display_dispatch(wlDisplay) != -1)
        {
        }
    }

private:
    int width, height;

    wl_display *wlDisplay = nullptr;
    wl_registry *registry = nullptr;
    wl_surface *wlSurface = nullptr;
    wl_shell_surface *shell_surface = nullptr;

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

    // 指定されたサイズの匿名ファイルを作成
    // そのファイルディスクリプタを返却
    int create_anonymous_file(off_t size)
    {
        std::string path_template = "/tmp/wayland-XXXXXX";
        std::vector<char> path(path_template.begin(), path_template.end());
        path.push_back('\0'); // 終端文字を追加
        int fd = mkstemp(path.data());

        if (fd < 0)
        {
            throw std::runtime_error("Failed to create temporary file");
        }

        // unlinkより、ファイルシステムから一時ファイルのエントリを削除
        // これにより、ファイルがオープンされている間はファイルが保持されるが、クローズされると自動的に削除される
        unlink(path.data());

        // fdで参照されるファイルを length バイトの長さになるように延長もしくは切り詰める
        if (ftruncate(fd, size) != 0)
        {
            close(fd);
            throw std::runtime_error("Failed to resize temporary file");
        }

        return fd;
    }
};

const wl_registry_listener WaylandWindow::registryListener = {
    WaylandWindow::registryGlobal,
    nullptr};

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

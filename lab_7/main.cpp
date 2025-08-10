#pragma clang diagnostic pushData
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"

#include <tchar.h>
#include <windows.h>
#include <windowsx.h>
#include <experimental/random>
#include <vector>
#include <fstream>
#include <atomic>
#include <iostream>

#define KEY_SHIFTED     0x8000
#define VK_Q           0x51
#define VK_S           0x53
#define VK_0           0x30
#define VK_9           0x39
#define MMAP_MODE       0
#define POSIX_MODE      1
#define FSTREAM_MODE    2
#define WIN_MODE        3

#define NONE_PLAYER     0
#define PLAYER_1        1
#define PLAYER_2        2
#define GAME_DRAW       3

namespace WinData {
    const UINT WM_FIELD_UPDATE = RegisterWindowMessage("WM_FIELD_UPDATE");
    const char *FILENAME = "settings";
    const TCHAR SZ_WIN_CLASS[] = _T("Win32SampleApp");
}


std::pair<UINT, std::string> GetThreadPriority(UINT d) {
    switch (d) {
        case 1:return {THREAD_PRIORITY_IDLE, "THREAD_PRIORITY: IDLE"};
        case 2:return {THREAD_PRIORITY_LOWEST, "THREAD_PRIORITY: LOWEST"};
        case 3:return {THREAD_PRIORITY_BELOW_NORMAL, "THREAD_PRIORITY: BELOW_NORMAL"};
        case 5:return {THREAD_PRIORITY_ABOVE_NORMAL, "THREAD_PRIORITY: ABOVE_NORMAL"};
        case 6:return {THREAD_PRIORITY_HIGHEST, "THREAD_PRIORITY: HIGHEST"};
        case 7:return {THREAD_PRIORITY_TIME_CRITICAL, "THREAD_PRIORITY: TIME_CRITICAL"};
        default:return {THREAD_PRIORITY_NORMAL, "THREAD_PRIORITY: NORMAL"};
    }
}

struct Colors {
    UINT r = 0;
    UINT g = 0;
    UINT b = 0;

    [[nodiscard]] unsigned int as_uint() const {
        return RGB(r, g, b);
    }
};

struct ColorHSV {
    UINT h = 0;
    UINT s = 0;
    UINT v = 0;

    [[nodiscard]] unsigned int as_uint() const {
        int h1, vmin, vinc, vdec, a, r, g, b;
        h1 = (h / 60) % 6;
        vmin = (100 - s) * v / 100;
        a = (v - vmin) * (h % 60) / 60;
        vinc = vmin + a;
        vdec = v - a;
        switch (h1) {
            case 0:
                r = v;
                g = vinc;
                b = vmin;
                break;
            case 1:
                r = vdec;
                g = v;
                b = vmin;
                break;
            case 2:
                r = vmin;
                g = v;
                b = vinc;
                break;
            case 3:
                r = vmin;
                g = vdec;
                b = v;
                break;
            case 4:
                r = vinc;
                g = vmin;
                b = v;
                break;
            case 5:
                r = v;
                g = vmin;
                b = vdec;
                break;
            default:
                break;
        }
        return RGB(r * 255 / 100, g * 255 / 100, b * 255 / 100);
    }
};

struct Settings {

    UINT N = 3;
    UINT w_width = 320;
    UINT w_height = 240;
    ColorHSV field_color = {0, 100, 100};
    Colors line_color = {255, 0, 0};
    int cur_delta = 0;
    UINT PLAYER_1_COLOR = RGB(255, 255, 255);
    UINT PLAYER_2_COLOR = RGB(0, 0, 0);

    static bool has_file();

    void save(int mode) const;

    void load(int mode);

    void mmapSave() const;

    void mmapLoad();

    void posixSave() const;

    void posixLoad();

    void fstreamSave() const;

    void fstreamLoad();

    void winSave() const;

    void winRead();
};

struct Game {
    UINT team = 0;
    std::vector<std::vector<UINT>> data;

    HANDLE h_map_file{};
    const wchar_t *shared_memory_name = L"MySharedMemory";
    int *lp_map_address{};
    int shared_memory_size{};

    RECT rect{0};
    HBRUSH h_brush;
    HPEN hpenP1;
    HPEN hpenP2;

    void initData(UINT team);

    void moveData(UINT x, UINT y);

    void pullData();

    void pushData();

    [[nodiscard]] HANDLE mySem() const;

    [[nodiscard]] HANDLE opSem() const;

    UINT check_win();

    void close() const;

};

HANDLE mutexBoard;
HANDLE semP1;
HANDLE semP2;

HANDLE draw_t;

std::atomic<bool> rendering(true);
std::atomic<bool> render_pause(true);

UINT rainbow_delta = 10;
UINT speed_freeze = 20;

Game game;
Settings settings;

void lineDraw(HDC hdc, UINT x1, UINT y1, UINT x2, UINT y2) {
    MoveToEx(hdc, x1, y1, nullptr);
    LineTo(hdc, x2, y2);
}

void boardDraw(HDC hdc) {

    HPEN hpen_l = CreatePen(PS_SOLID, 1, settings.line_color.as_uint());
    SelectObject(hdc, game.h_brush);
    SelectObject(hdc, hpen_l);
    for (int i = 1; i < settings.N; ++i) {
        lineDraw(hdc, i * game.rect.right / settings.N, 0, i * game.rect.right / settings.N, game.rect.bottom);
        lineDraw(hdc, 0, i * game.rect.bottom / settings.N, game.rect.right, i * game.rect.bottom / settings.N);
    }
    for (UINT i = 0; i < settings.N * settings.N; ++i) {
        UINT x = i % settings.N, y = i / settings.N;
        if (game.data[x][y] == PLAYER_1) {
            SelectObject(hdc, game.hpenP2);
            lineDraw(hdc, x * game.rect.right / settings.N, y * game.rect.bottom / settings.N,
                     (x + 1) * game.rect.right / settings.N, (y + 1) * game.rect.bottom / settings.N);
            lineDraw(hdc, (x + 1) * game.rect.right / settings.N, y * game.rect.bottom / settings.N,
                     x * game.rect.right / settings.N, (y + 1) * game.rect.bottom / settings.N);
        } else if (game.data[x][y] == PLAYER_2) {
            SelectObject(hdc, game.hpenP1);
            Ellipse(hdc, x * game.rect.right / settings.N, y * game.rect.bottom / settings.N,
                    ((x + 1) * game.rect.right / settings.N), (y + 1) * game.rect.bottom / settings.N);
        }
    }
    DeleteObject(hpen_l);
}

void run_notepad() {
    STARTUPINFO sInfo;
    PROCESS_INFORMATION pInfo;

    ZeroMemory(&sInfo, sizeof(STARTUPINFO));

    CreateProcess(_T("C:\\Windows\\Notepad.exe"),
                  nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sInfo, &pInfo);
}

DWORD WINAPI looper(LPVOID lpParam) {
    int i = 2;
    while (true) {
        i *= i;
    }
}

DWORD WINAPI render_proc(LPVOID lpParam) {
    CreateThread(NULL, 64 * 1024, looper, NULL, 0, NULL);
    HWND hwnd = *reinterpret_cast<HWND *>(lpParam);
    PAINTSTRUCT ps;
    HDC hdc, hdc_mem;
    HBITMAP hbm_mem, hbm_old;
    RECT rect = {0};

    hdc = GetDC(hwnd);
    GetClientRect(hwnd, &rect);
    hdc_mem = CreateCompatibleDC(hdc);
    hbm_mem = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    hbm_old = (HBITMAP) SelectObject(hdc_mem, hbm_mem);
    ReleaseDC(hwnd, hdc);

    rainbow_delta = settings.field_color.h;
    while (rendering) {
        if (game.rect.right != rect.right || game.rect.bottom != rect.bottom) {
            rect = game.rect;
            SelectObject(hdc_mem, hbm_old);
            DeleteObject(hbm_mem);
            hdc = GetDC(hwnd);
            hbm_mem = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            hbm_old = (HBITMAP) SelectObject(hdc_mem, hbm_mem);
            ReleaseDC(hwnd, hdc);
        }

        settings.field_color.h = (++rainbow_delta / speed_freeze) % 360;
        game.h_brush = CreateSolidBrush(settings.field_color.as_uint());
        DeleteBrush((HBRUSH) SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR) game.h_brush));

        FillRect(hdc_mem, &game.rect, game.h_brush);

        boardDraw(hdc_mem);

        InvalidateRect(hwnd, &game.rect, FALSE);
        hdc = BeginPaint(hwnd, &ps);
        BitBlt(hdc, 0, 0, rect.right, rect.bottom, hdc_mem, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        //sched_yield();
    }
    SelectObject(hdc_mem, hbm_old);
    DeleteObject(hbm_mem);
    DeleteDC(hdc_mem);
    return 0;
}

LRESULT CALLBACK window_keydown_proc(HWND hwnd, WPARAM wParam) {
    if (wParam == VK_ESCAPE || (GetAsyncKeyState(VK_CONTROL) & KEY_SHIFTED) && wParam == VK_Q) {
        PostQuitMessage(0);
    } else if (wParam == VK_RETURN) {
        WaitForSingleObject(mutexBoard, INFINITE);
        settings.field_color = {
                static_cast<UINT>(std::experimental::randint(0, 360)),
                static_cast<UINT>(std::experimental::randint(0, 100)),
                static_cast<UINT>(std::experimental::randint(0, 100))
        };
        rainbow_delta = settings.field_color.h * speed_freeze;
        game.h_brush = CreateSolidBrush(settings.field_color.as_uint());
        DeleteBrush((HBRUSH) SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR) game.h_brush));
        ReleaseMutex(mutexBoard);

    } else if ((GetAsyncKeyState(VK_SHIFT) & KEY_SHIFTED) && wParam == VK_S) {
        run_notepad();
    } else if (wParam == VK_SPACE) {
        render_pause = not render_pause;
        (render_pause ? ResumeThread : SuspendThread)(draw_t);
        std::cout << "play " << render_pause << std::endl;
    } else if (VK_0 <= wParam && wParam <= VK_9) {
        SetThreadPriority(draw_t, GetThreadPriority(wParam - 48).first);
    }
    return 0;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static UINT color;

    if (message == WinData::WM_FIELD_UPDATE) {
        WaitForSingleObject(mutexBoard, INFINITE);
        puts("field updated");
        game.pullData();
        ReleaseMutex(mutexBoard);
        return 0;
    }

    switch (message) {
        case WM_DESTROY:
            GetWindowRect(hwnd, &game.rect);
            settings.w_width = game.rect.right - game.rect.left;
            settings.w_height = game.rect.bottom - game.rect.top;
            rendering = false;
            PostQuitMessage(0);
            return 0;

        case WM_LBUTTONDOWN:
            game.moveData(LOWORD(lParam) * settings.N / game.rect.right,
                          HIWORD(lParam) * settings.N / game.rect.bottom);
            return 0;
        case WM_SIZE:
            GetClientRect(hwnd, &game.rect);
            return 0;
        case WM_KEYDOWN:
            return window_keydown_proc(hwnd, wParam);

        case WM_MOUSEWHEEL:
            settings.cur_delta += GET_WHEEL_DELTA_WPARAM(wParam) / 12;
            settings.cur_delta = (settings.cur_delta + 255 * 2) % (255 * 2);
            color = static_cast<UINT>(255 -
                                      (std::min(settings.cur_delta, 255) - std::max(settings.cur_delta - 255, 0)));
            settings.line_color = {color, color, color};
            return 0;
        default:
            break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

void mainloop(int team) {
    MSG message{};
    WNDCLASS wincl = {0};

    int nCmdShow = SW_SHOW;

    game.initData(team);

    HINSTANCE h_this_instance = GetModuleHandle(nullptr);

    mutexBoard = CreateMutex(NULL, FALSE, "BoardMutex");

    semP1 = CreateSemaphore(NULL, 1, 1, "player_1_move_sem");
    semP2 = CreateSemaphore(NULL, 0, 1, "player_2_move_sem");

    wincl.hInstance = h_this_instance;
    wincl.lpszClassName = WinData::SZ_WIN_CLASS;
    wincl.lpfnWndProc = window_proc;
    wincl.hbrBackground = game.h_brush;

    if (!RegisterClass(&wincl)) return;

    TCHAR SZ_WIN_NAME[] = "The Game: Player 0";
    if (game.team == PLAYER_1) {
        SZ_WIN_NAME[std::size(SZ_WIN_NAME) - 2] = '1';
    } else {
        SZ_WIN_NAME[std::size(SZ_WIN_NAME) - 2] = '2';
    }

    HWND hwnd = CreateWindow(
            WinData::SZ_WIN_CLASS,           /* Classname */
            SZ_WIN_NAME,            /* Title Text */
            WS_OVERLAPPEDWINDOW,    /* default window */
            CW_USEDEFAULT,          /* Windows decides the position */
            CW_USEDEFAULT,          /* where the window ends up on the screen */
            settings.w_width,       /* The programs w_width */
            settings.w_height,      /* and w_height in pixels */
            HWND_DESKTOP,           /* The window is a child-window to desktop */
            NULL,                   /* No menu */
            h_this_instance,        /* Program Instance handler */
            NULL                    /* No Window Creation data */
    );
    if (hwnd == NULL) return;

    ShowWindow(hwnd, nCmdShow);

    draw_t = CreateThread(NULL, 64 * 1024, render_proc, &hwnd, 0, NULL);
    BOOL b_message_ok;
    while ((b_message_ok = GetMessage(&message, nullptr, 0, 0)) != 0) {
        /* Yep, fuck logic: BOOL mb not only 1 or 0.
         * See msdn at https://msdn.microsoft.com/en-us/library/windows/desktop/ms644936(v=vs.85).aspx
         */
        if (b_message_ok == -1) {
            puts("Suddenly, GetMessage failed! You can call GetLastError() to see what happend");
            break;
        }
        /* Translate virtual-key message into character message */
        TranslateMessage(&message);
        /* Send message to window_proc */
        DispatchMessage(&message);
    }

    CloseHandle(draw_t);
    DestroyWindow(hwnd);
    UnregisterClass(WinData::SZ_WIN_CLASS, h_this_instance);

    game.close();
    puts("exit");
}

void Settings::save(int mode) const {
    switch (mode) {
        case MMAP_MODE:
            mmapSave();
            break;
        case POSIX_MODE:
            posixSave();
            break;
        case FSTREAM_MODE:
            fstreamSave();
            break;
        case WIN_MODE:
            winSave();
            break;
        default:
            break;
    }
};

void Settings::load(int mode) {
    switch (mode) {
        case MMAP_MODE:
            mmapLoad();
            break;
        case POSIX_MODE:
            posixLoad();
            break;
        case FSTREAM_MODE:
            fstreamLoad();
            break;
        case WIN_MODE:
            winRead();
            break;
        default:
            break;
    }
};

bool Settings::has_file() {
    DWORD fileAttributes = GetFileAttributes(WinData::FILENAME);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void Settings::mmapSave() const {
    WaitForSingleObject(mutexBoard, INFINITE);
    puts("mmapSave");
    auto h_file = CreateFile(
            WinData::FILENAME,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    auto mapping = CreateFileMapping(
            h_file,
            nullptr,
            PAGE_READWRITE,
            0,
            sizeof(Settings),
            nullptr);

    auto data = (Settings *) MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(Settings));

    memcpy(data, (void *) this, sizeof(Settings));

    UnmapViewOfFile(data);
    CloseHandle(mapping);
    CloseHandle(h_file);
    ReleaseMutex(mutexBoard);
}

void Settings::mmapLoad() {
    WaitForSingleObject(mutexBoard, INFINITE);
    puts("mmapLoad");
    auto h_file = CreateFile(
            WinData::FILENAME,
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    auto mapping = CreateFileMapping(
            h_file,
            nullptr,
            PAGE_READONLY,
            0,
            sizeof(Settings),
            nullptr);

    auto data = (Settings *) MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(Settings));

    memcpy((void *) this, data, sizeof(Settings));

    UnmapViewOfFile(data);
    CloseHandle(mapping);
    CloseHandle(h_file);
    ReleaseMutex(mutexBoard);
}

void Settings::posixSave() const {
    WaitForSingleObject(mutexBoard, INFINITE);
    puts("posixSave");
    auto fd = fopen(WinData::FILENAME, "wb");
    fwrite(this, sizeof(Settings), 1, fd);
    fclose(fd);
    ReleaseMutex(mutexBoard);
}

void Settings::posixLoad() {
    WaitForSingleObject(mutexBoard, INFINITE);
    puts("posixLoad");
    auto fd = fopen(WinData::FILENAME, "rb");
    fread(this, sizeof(Settings), 1, fd);
    fclose(fd);
    ReleaseMutex(mutexBoard);
}

void Settings::fstreamSave() const {
    WaitForSingleObject(mutexBoard, INFINITE);
    puts("fstreamSave");
    std::ofstream out(WinData::FILENAME);
    out.write((char *) this, sizeof(Settings));
    out.close();
    ReleaseMutex(mutexBoard);
}

void Settings::fstreamLoad() {
    WaitForSingleObject(mutexBoard, INFINITE);
    puts("fstreamLoad");
    std::ifstream in(WinData::FILENAME);
    in.read((char *) this, sizeof(Settings));
    in.close();
    ReleaseMutex(mutexBoard);
}

void Settings::winSave() const {
    WaitForSingleObject(mutexBoard, INFINITE);
    puts("winSave");
    auto h_file = CreateFile(WinData::FILENAME, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                             nullptr);
    WriteFile(h_file, (LPVOID) this, sizeof(Settings), nullptr, nullptr);
    CloseHandle(h_file);
    ReleaseMutex(mutexBoard);
}

void Settings::winRead() {
    WaitForSingleObject(mutexBoard, INFINITE);
    puts("win_load");
    auto h_file = CreateFile(WinData::FILENAME, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             nullptr);
    ReadFile(h_file, (LPVOID) this, sizeof(Settings), nullptr, nullptr);
    CloseHandle(h_file);
    ReleaseMutex(mutexBoard);
}

void Game::pullData() {
    WaitForSingleObject(mutexBoard, INFINITE);
    for (int i = 0; i < settings.N; ++i)
        memcpy(data[i].data(), lp_map_address + i * settings.N, sizeof(int) * settings.N);
    ReleaseMutex(mutexBoard);
}

void Game::pushData() {
    for (int i = 0; i < settings.N; ++i)
        memcpy(lp_map_address + i * settings.N, data[i].data(), sizeof(int) * settings.N);
    PostMessage(HWND_BROADCAST, WinData::WM_FIELD_UPDATE, 0, 0);
}

void Game::moveData(UINT x, UINT y) {
    if (WaitForSingleObject(mySem(), 1) == WAIT_TIMEOUT) {
        MessageBox(NULL, "The opponent's moveData is now!", "Ups!", MB_ICONSTOP | MB_OK);
        return;
    }
    WaitForSingleObject(mutexBoard, INFINITE);
    if (data[x][y] == NONE_PLAYER) {
        data[x][y] = team;
        pushData();
        UINT winner = game.check_win();
        std::cout << "Winner: " << winner << std::endl;
        if (winner == NONE_PLAYER) {
            ReleaseMutex(mutexBoard);
            ReleaseSemaphore(opSem(), 1, NULL);
        } else {
            char *msg;
            switch (winner) {
                case PLAYER_1:
                    msg = "Player 1 Win!";
                    break;
                case PLAYER_2:
                    msg = "Player 2 Win!";
                    break;
                default:
                    msg = "Drawn Game!";
                    break;
            }
            ReleaseMutex(mutexBoard);

            MessageBox(NULL, msg, "Game over!", MB_ICONMASK | MB_OK);

            WaitForSingleObject(mutexBoard, INFINITE);
            for (int i = 0; i < settings.N * settings.N; ++i) {
                data[i % settings.N][i / settings.N] = NONE_PLAYER;
            }
            pushData();
            ReleaseMutex(mutexBoard);
            ReleaseSemaphore(semP1, 1, NULL);
        }
    } else {
        ReleaseMutex(mutexBoard);
        ReleaseSemaphore(mySem(), 1, NULL);
        MessageBox(NULL, "The opponent's moveData is now!", "Ups!", MB_ICONSTOP | MB_OK);
    }
}

HANDLE Game::mySem() const {
    return team == PLAYER_1 ? semP1 : semP2;
}

HANDLE Game::opSem() const {
    return team != PLAYER_1 ? semP1 : semP2;
}

UINT Game::check_win() {
    if (data[0][0] != NONE_PLAYER) {
        bool all = true;
        for (int i = 1; i < settings.N; ++i) {
            if (data[0][0] != data[i][i]) {
                all = false;
                break;
            }
        }
        if (all) return data[0][0];
    }
    if (data[0][settings.N - 1] != NONE_PLAYER) {
        bool all = true;
        for (int i = 1; i < settings.N; ++i) {
            if (data[0][settings.N - 1] != data[i][settings.N - 1 - i]) {
                all = false;
                break;
            }
        }
        if (all) return data[0][settings.N - 1];
    }
    for (int x = 0; x < settings.N; ++x) {
        if (data[x][0] != NONE_PLAYER) {
            bool all = true;
            for (int y = 1; y < settings.N; ++y) {
                if (data[x][0] != data[x][y]) {
                    all = false;
                    break;
                }
            }
            if (all) return data[x][0];
        }
    }
    for (int y = 0; y < settings.N; ++y) {
        if (data[0][y] != NONE_PLAYER) {
            bool all = true;
            for (int x = 1; x < settings.N; ++x) {
                if (data[0][y] != data[x][y]) {
                    all = false;
                    break;
                }
            }
            if (all) return data[0][y];
        }
    }
    bool any = false;
    for (int i = 0; i < settings.N * settings.N; ++i) {
        if (data[i % settings.N][i / settings.N] == NONE_PLAYER) {
            any = true;
        }
    }
    return any ? NONE_PLAYER : GAME_DRAW;
}

void Game::initData(UINT _team) {
    this->team = _team;

    for (int i = 0; i < settings.N; ++i) { data.emplace_back(settings.N); } // Define game
    shared_memory_size = sizeof(UINT) * settings.N * settings.N;
    h_map_file = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                    nullptr,
                                    PAGE_READWRITE, 0, shared_memory_size,
                                    shared_memory_name);
    lp_map_address = (int *) MapViewOfFile(h_map_file, FILE_MAP_ALL_ACCESS, 0, 0, shared_memory_size);
    pullData();

    h_brush = CreateSolidBrush(settings.field_color.as_uint());
    hpenP1 = CreatePen(PS_SOLID, 1, settings.PLAYER_1_COLOR);
    hpenP2 = CreatePen(PS_SOLID, 1, settings.PLAYER_2_COLOR);
}

void Game::close() const {
    UnmapViewOfFile(lp_map_address);
    CloseHandle(h_map_file);

    DeleteObject(game.h_brush);
    DeleteObject(hpenP1);
    DeleteObject(hpenP2);
}

int main(int argc, char **argv) {
    int N = 0;
    int mode = WIN_MODE;
    int team = PLAYER_1;

    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "--mmap") {
            mode = MMAP_MODE;
        } else if (s == "--posix") {
            mode = POSIX_MODE;
        } else if (s == "--fstream") {
            mode = FSTREAM_MODE;
        } else if (s == "--winapi") {
            mode = WIN_MODE;

        } else if (s == "-p1") {
            team = PLAYER_1;
        } else if (s == "-p2") {
            team = PLAYER_2;
        } else {
            N = std::stoi(s);
        }
    }

    if (Settings::has_file()) settings.load(mode);
    if (N) {
        settings.N = N;
    }

    settings.save(mode);
    mainloop(team);

    settings.save(mode);

    return 0;
}

#pragma clang diagnostic pop

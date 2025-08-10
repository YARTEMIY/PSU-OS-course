#include <tchar.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <ctime>

#define KEY_SHIFTED     0x8000

#define MODE_MMAP           0
#define MODE_POSIX          1
#define MODE_FSTREAM        2
#define MODE_WINAPI         3

#define CIRCLE  1
#define CROSS   2

const char *FILENAME = "CONFIG";

HWND hwnd;
HBRUSH h_brush;
std::vector<std::vector<int>> board;

int width;
int height;

struct Color {
    int r = 0;
    int g = 0;
    int b = 0;
};

struct WindowSettings {

    int N = 3;
    int width = 320;
    int height = 240;
    Color boardColor = {0, 0, 255};
    Color lineColor = {255, 0, 0};
    int mode = MODE_WINAPI;

    void save() const {
        switch (mode) {
            case MODE_MMAP:
                mmapSave();
                break;
            case MODE_POSIX:
                posixSave();
                break;
            case MODE_FSTREAM:
                fstreamSave();
                break;
            case MODE_WINAPI:
                winSave();
                break;
            default:
                winSave();
                break;
        }
    };

    void load() {
        switch (mode) {
            case MODE_MMAP:
                mmapLoad();
                break;
            case MODE_POSIX:
                posixLoad();
                break;
            case MODE_FSTREAM:
                fstreamLoad();
                break;
            case MODE_WINAPI:
                winRead();
                break;
            default:
                winRead();
                break;
        }
    };

    static bool hasFile() {
        DWORD fileAttributes = GetFileAttributes(FILENAME);
        if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    void mmapSave() const {
        auto h_file = CreateFile(FILENAME, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);

        auto mapping = CreateFileMapping(h_file, nullptr, PAGE_READWRITE, 0, sizeof(WindowSettings), nullptr);

        auto data = (WindowSettings *) MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0,
                                                     sizeof(WindowSettings));

        memcpy(data, (void *) this, sizeof(WindowSettings));

        UnmapViewOfFile(data);
        CloseHandle(mapping);
        CloseHandle(h_file);
    }

    void mmapLoad() {
        auto h_file = CreateFile(
                FILENAME,
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
                sizeof(WindowSettings),
                nullptr);

        auto data = (WindowSettings *) MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(WindowSettings));

        memcpy((void *) this, data, sizeof(WindowSettings));

        UnmapViewOfFile(data);
        CloseHandle(mapping);
        CloseHandle(h_file);
    }

    void posixSave() const {
        auto fd = fopen(FILENAME, "wb");
        fwrite(this, sizeof(WindowSettings), 1, fd);
        fclose(fd);
    }

    void posixLoad() {
        auto fd = fopen(FILENAME, "rb");
        fread(this, sizeof(WindowSettings), 1, fd);
        fclose(fd);
    }

    void fstreamSave() const {
        std::ofstream out(FILENAME);
        out.write((char *) this, sizeof(WindowSettings));
        out.close();
    }

    void fstreamLoad() {
        std::ifstream in(FILENAME);
        in.read((char *) this, sizeof(WindowSettings));
        in.close();
    }

    void winSave() const {
        auto h_file = CreateFile(FILENAME, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        WriteFile(h_file, (LPVOID) this, sizeof(WindowSettings), nullptr, nullptr);
        CloseHandle(h_file);
    }

    void winRead() {
        auto h_file = CreateFile(FILENAME, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ReadFile(h_file, (LPVOID) this, sizeof(WindowSettings), nullptr, nullptr);
        CloseHandle(h_file);
    }

};

const TCHAR SZ_WIN_CLASS[] = _T("lab3_4");
const TCHAR SZ_WIN_NAME[] = _T("lab3_4");

WindowSettings settings;

bool drawLine(HDC hdc, UINT x1, UINT y1, UINT x2, UINT y2) {
    MoveToEx(hdc, x1, y1, nullptr);
    return LineTo(hdc, x2, y2);
}

void drawEllipse(HDC hdc, int x, int y) {
    RECT windowRect;
    GetClientRect(hwnd, &windowRect);


    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(102, 205, 170));
    HBRUSH brush = CreateSolidBrush(RGB(102, 205, 170));
    SelectObject(hdc, hPen);
    Ellipse(hdc, x * width / settings.N, y * height / settings.N,
            ((x + 1) * width / settings.N), (y + 1) * height / settings.N);
    SelectObject(hdc, brush);
    Ellipse(hdc, x * width / settings.N, y * height / settings.N,
            ((x + 1) * width / settings.N), (y + 1) * height / settings.N);
    DeleteObject(brush);
    DeleteObject(hPen);
}

void drawCross(HDC hdc, int x, int y) {
    RECT windowRect;
    GetClientRect(hwnd, &windowRect);


    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(220, 20, 60));

    SelectObject(hdc, hPen);
    drawLine(hdc, x * width / settings.N, y * height / settings.N,
             (x + 1) * width / settings.N, (y + 1) * height / settings.N);
    drawLine(hdc, (x + 1) * width / settings.N, y * height / settings.N,
             x * width / settings.N, (y + 1) * height / settings.N);

    DeleteObject(hPen);
}

void drawBoard(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    HPEN hpen_l = CreatePen(PS_SOLID, 1, RGB(settings.lineColor.r, settings.lineColor.g, settings.lineColor.b));
    SelectObject(hdc, h_brush);

    SelectObject(hdc, hpen_l);
    for (int i = 1; i < settings.N; ++i) {
        drawLine(hdc, i * width / settings.N, 0, i * width / settings.N, height);
        drawLine(hdc, 0, i * height / settings.N, width, i * height / settings.N);
    }
    for (int i = 0; i < settings.N * settings.N; ++i) {
        int x = i % settings.N;
        int y = i / settings.N;
        if (board[x][y] == CIRCLE) {
            drawEllipse(hdc, x, y);
        } else if (board[x][y] == CROSS) {
            drawCross(hdc, x, y);
        }
    }
    EndPaint(hwnd, &ps);

    DeleteObject(&ps);
    DeleteObject(hdc);
    DeleteObject(hpen_l);
}

void onMouseWheelFunc(HWND hwnd) {
    COLORREF linesColor = GetWindowLongPtr(hwnd, GWLP_USERDATA);

    int stepSize = 20;

    int newR = settings.lineColor.r + ((rand() % 2) ? stepSize : -stepSize);
    int newG = settings.lineColor.g + ((rand() % 2) ? stepSize : -stepSize);
    int newB = settings.lineColor.b + ((rand() % 2) ? stepSize : -stepSize);

    settings.lineColor.r = (newR >= 0 && newR <= 255) ? newR : settings.lineColor.r;
    settings.lineColor.g = (newG >= 0 && newG <= 255) ? newG : settings.lineColor.g;
    settings.lineColor.b = (newB >= 0 && newB <= 255) ? newB : settings.lineColor.b;

    InvalidateRect(hwnd, NULL, TRUE);
}


void runNotepad() {
    STARTUPINFO sInfo;
    PROCESS_INFORMATION pInfo;

    ZeroMemory(&sInfo, sizeof(STARTUPINFO));

    CreateProcess(_T("C:\\Windows\\Notepad.exe"),
                  nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sInfo, &pInfo);
}

LRESULT buttonKeyDownChecker(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (wParam == VK_RETURN) {
        settings.boardColor.r = rand() % 255 + 1;
        settings.boardColor.g = rand() % 255 + 1;
        settings.boardColor.b = rand() % 255 + 1;

        COLORREF newColor = RGB(settings.boardColor.r, settings.boardColor.g, settings.boardColor.b);

        DeleteObject((HBRUSH) SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR) CreateSolidBrush(newColor)));

        InvalidateRect(hwnd, NULL, TRUE);
    } else if (GetKeyState(VK_SHIFT) & KEY_SHIFTED && wParam == 'C') {
        runNotepad();
    } else if (wParam == VK_ESCAPE || (GetKeyState(VK_CONTROL) & KEY_SHIFTED) && wParam == 'Q') {
        PostQuitMessage(0);
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT:
            drawBoard(hwnd);
            break;
        case WM_DESTROY:
            RECT rect;
            GetWindowRect(hwnd, &rect);
            settings.width = rect.right - rect.left;
            settings.height = rect.bottom - rect.top;
            PostQuitMessage(0);
            break;

        case WM_SIZE:
            width = LOWORD(lParam);
            height = HIWORD(lParam);
            InvalidateRect(hwnd, nullptr, TRUE);
            break;

        case WM_LBUTTONDOWN:
            board[LOWORD(lParam) * settings.N / width][HIWORD(lParam) * settings.N / height] = 1;
            InvalidateRect(hwnd, nullptr, TRUE);
            break;

        case WM_RBUTTONDOWN:
            board[LOWORD(lParam) * settings.N / width][HIWORD(lParam) * settings.N / height] = 2;
            InvalidateRect(hwnd, nullptr, TRUE);
            break;

        case WM_KEYDOWN:
            buttonKeyDownChecker(hwnd, message, wParam, lParam);
        case WM_MOUSEWHEEL:
            onMouseWheelFunc(hwnd);
            break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

int loop() {
    for (int i = 0; i < settings.N; ++i) {
        board.emplace_back(settings.N);
    }

    BOOL b_message_ok;

    MSG message;
    WNDCLASS wincl = {0};

    int nCmdShow = SW_SHOW;

    HINSTANCE h_this_instance = GetModuleHandle(nullptr);

    wincl.hInstance = h_this_instance;
    wincl.lpszClassName = SZ_WIN_CLASS;
    wincl.lpfnWndProc = windowProc;

    h_brush = CreateSolidBrush(RGB(settings.boardColor.r, settings.boardColor.g, settings.boardColor.b));
    wincl.hbrBackground = h_brush;

    if (!RegisterClass(&wincl))
        return 0;

    hwnd = CreateWindow(
            SZ_WIN_CLASS,           /* Classname */
            SZ_WIN_NAME,            /* Title Text */
            WS_OVERLAPPEDWINDOW,    /* default window */
            CW_USEDEFAULT,          /* Windows decides the position */
            CW_USEDEFAULT,          /* where the window ends up on the screen */
            settings.width,       /* The programs width */
            settings.height,      /* and height in pixels */
            HWND_DESKTOP,           /* The window is a child-window to desktop */
            nullptr,                /* No menu */
            h_this_instance,        /* Program Instance handler */
            nullptr                 /* No Window Creation data */
    );

    ShowWindow(hwnd, nCmdShow);


    while ((b_message_ok = GetMessage(&message, nullptr, 0, 0)) != 0) {
        if (b_message_ok == -1) {
            puts("Suddenly, GetMessage failed! You can call GetLastError() to see what happend");
            break;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    DestroyWindow(hwnd);
    UnregisterClass(SZ_WIN_CLASS, h_this_instance);
    DeleteObject(h_brush);
}

int main(int argc, char **argv) {
    int N = 0;
    srand(time(nullptr));

    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "mmap") {
            settings.mode = MODE_MMAP;
        } else if (s == "posix") {
            settings.mode = MODE_POSIX;
        } else if (s == "fstream") {
            settings.mode = MODE_FSTREAM;
        } else if (s == "winapi") {
            settings.mode = MODE_WINAPI;
        } else {
            N = std::stoi(s);
        }
    }

    if (WindowSettings::hasFile()) {
        settings.load();
    }

    if (N) {
        settings.N = N;
    }

    loop();

    settings.save();

    return 0;
}
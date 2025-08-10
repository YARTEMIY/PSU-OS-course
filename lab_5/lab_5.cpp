#include <tchar.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <ctime>
#include <string>

#define KEY_SHIFTED     0x8000

#define MODE_MMAP           0
#define MODE_POSIX          1
#define MODE_FSTREAM        2
#define MODE_WINAPI         3

HWND hwnd;

int width;
int height;
HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 255));

const UINT WM_FIELD_UPDATE = RegisterWindowMessage(L"WM_FIELD_UPDATE");

struct Color {
    int r = 0;
    int g = 0;
    int b = 0;
};

struct WindowSettings {
    int N = 3;
    int w_width = 320;
    int w_height = 240;
    Color boardColor = { 0, 0, 255 };
    Color lineColor = { 255, 0, 0 };
    int mode = MODE_POSIX;

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
            mmapSave();
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
            mmapLoad();
            break;
        }
    };

    static bool hasFile() {
        DWORD fileAttributes = GetFileAttributes(L"CONFIG");

        return fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    void mmapSave() const {
        auto h_file = CreateFile(L"CONFIG", GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        auto mapping = CreateFileMapping(h_file, nullptr, PAGE_READWRITE, 0, sizeof(WindowSettings), nullptr);

        auto data = (WindowSettings*)MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0,
            sizeof(WindowSettings));

        memcpy(data, (void*)this, sizeof(WindowSettings));

        UnmapViewOfFile(data);
        CloseHandle(mapping);
        CloseHandle(h_file);
    }

    void mmapLoad() {
        auto h_file = CreateFile(
            L"CONFIG",
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

        auto data = (WindowSettings*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(WindowSettings));

        memcpy((void*)this, data, sizeof(WindowSettings));

        UnmapViewOfFile(data);
        CloseHandle(mapping);
        CloseHandle(h_file);
    }

    void posixSave() const {
        FILE* fd;
        errno_t error = fopen_s(&fd, "CONFIG", "wb");
        if (error == 0) {
            fwrite(this, sizeof(WindowSettings), 1, fd);
            fclose(fd);
        }
    }

    void posixLoad() {
        FILE* fd;
        errno_t error = fopen_s(&fd, "CONFIG", "rb");
        if (error == 0) {
            fread(this, sizeof(WindowSettings), 1, fd);
            fclose(fd);
        }
    }


    void fstreamSave() const {
        std::ofstream output_file("CONFIG");
        output_file.write((char*)this, sizeof(WindowSettings));
        output_file.close();
    }

    void fstreamLoad() {
        std::ifstream input_file("CONFIG");
        input_file.read((char*)this, sizeof(WindowSettings));
        input_file.close();
    }

    void winSave() const {
        auto file_handle = CreateFile(L"CONFIG", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
            nullptr);
        WriteFile(file_handle, (LPVOID)this, sizeof(WindowSettings), nullptr, nullptr);
        CloseHandle(file_handle);
    }

    void winRead() {
        auto file_handle = CreateFile(L"CONFIG", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
            nullptr);
        ReadFile(file_handle, (LPVOID) this, sizeof(WindowSettings), nullptr, nullptr);
        CloseHandle(file_handle);
    }


};

const TCHAR SZ_WIN_CLASS[] = _T("lab_5");
const TCHAR SZ_WIN_NAME[] = _T("lab_5");

WindowSettings settings;

struct Board {
    HANDLE h_map_file;
    std::vector<std::vector<UINT>> data;
    const wchar_t* shared_memory_name = L"MySharedMemory";
    int* lp_map_address;
    int shared_memory_size;

    void pullData() {
        for (int i = 0; i < settings.N; ++i)
            memcpy(data[i].data(), lp_map_address + i * settings.N, sizeof(int) * settings.N);
    }

    void pushData() {
        for (int i = 0; i < settings.N; ++i)
            memcpy(lp_map_address + i * settings.N, data[i].data(), sizeof(int) * settings.N);
        PostMessage(HWND_BROADCAST, WM_FIELD_UPDATE, 0, 0);

    }

    void initData() {
        for (int i = 0; i < settings.N; ++i) { data.emplace_back(settings.N); }
        shared_memory_size = sizeof(UINT) * settings.N * settings.N;
        h_map_file = CreateFileMappingW(INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE, 0, shared_memory_size,
            shared_memory_name);
        lp_map_address = (int*) MapViewOfFile(h_map_file, FILE_MAP_ALL_ACCESS, 0, 0, shared_memory_size);
        pullData();
    }

    void close() {
        UnmapViewOfFile(lp_map_address);
        CloseHandle(h_map_file);
    }
};

HBRUSH h_brush;
Board board;

bool drawLine(HDC hdc, UINT x1, UINT y1, UINT x2, UINT y2) {
    MoveToEx(hdc, x1, y1, nullptr);
    return LineTo(hdc, x2, y2);
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
        if (board.data[x][y] == 1) {
            RECT windowRect;
            GetClientRect(hwnd, &windowRect);

            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 0));
            HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);

            SelectObject(hdc, hPen);
            SelectObject(hdc, hBrush);
            Ellipse(hdc, x * width / settings.N, y * height / settings.N,
                ((x + 1) * width / settings.N), (y + 1) * height / settings.N);


            DeleteObject(hPen);
            DeleteObject(hBrush);
        }
        else if (board.data[x][y] == 2) {
            RECT windowRect;
            GetClientRect(hwnd, &windowRect);

            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 165, 0));

            SelectObject(hdc, hPen);
            drawLine(hdc, x * width / settings.N, y * height / settings.N,
                (x + 1) * width / settings.N, (y + 1) * height / settings.N);
            drawLine(hdc, (x + 1) * width / settings.N, y * height / settings.N,
                x * width / settings.N, (y + 1) * height / settings.N);

            DeleteObject(hPen);
        }
    }
    EndPaint(hwnd, &ps);

    DeleteObject(&ps);
    DeleteObject(hdc);
    DeleteObject(hpen_l);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RECT rect = { 0 };
    if (message == WM_FIELD_UPDATE) {
        board.pullData();
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    switch (message) {
    case WM_PAINT:
        drawBoard(hwnd);

        return 0;

    case WM_DESTROY:
        GetWindowRect(hwnd, &rect);
        settings.w_width = rect.right - rect.left;
        settings.w_height = rect.bottom - rect.top;
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:

        width = LOWORD(lParam);
        height = HIWORD(lParam);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_LBUTTONDOWN:
        board.data[LOWORD(lParam) * settings.N / width][HIWORD(lParam) * settings.N / height] = 1;
        board.pushData();
        return 0;

    case WM_RBUTTONDOWN:
        board.data[LOWORD(lParam) * settings.N / width][HIWORD(lParam) * settings.N / height] = 2;
        board.pushData();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || (GetAsyncKeyState(VK_CONTROL) & KEY_SHIFTED) && wParam == 0x51) {
            PostQuitMessage(0);
        }
        else if (wParam == VK_RETURN) {
            DeleteObject(hBrush);
            settings.boardColor.r = rand() % 255 + 1;
            settings.boardColor.g = rand() % 255 + 1;
            settings.boardColor.b = rand() % 255 + 1;
            hBrush = CreateSolidBrush(RGB(settings.boardColor.r, settings.boardColor.g, settings.boardColor.b));
            DeleteObject((HBRUSH)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush));
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if ((GetAsyncKeyState(VK_SHIFT) & KEY_SHIFTED) && wParam == 0x53) {
            STARTUPINFO sInfo;
            PROCESS_INFORMATION pInfo;

            ZeroMemory(&sInfo, sizeof(STARTUPINFO));

            CreateProcess(_T("C:\\Windows\\Notepad.exe"),
                nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sInfo, &pInfo);
        }
        else {
            break;
        }
        return 0;

    case WM_MOUSEWHEEL: {
        COLORREF linesColor = GetWindowLongPtr(hwnd, GWLP_USERDATA);

        int stepSize = 20;

        int newR = settings.lineColor.r + ((rand() % 2) ? stepSize : -stepSize);
        int newG = settings.lineColor.g + ((rand() % 2) ? stepSize : -stepSize);
        int newB = settings.lineColor.b + ((rand() % 2) ? stepSize : -stepSize);

        settings.lineColor.r = (newR >= 0 && newR <= 255) ? newR : settings.lineColor.r;
        settings.lineColor.g = (newG >= 0 && newG <= 255) ? newG : settings.lineColor.g;
        settings.lineColor.b = (newB >= 0 && newB <= 255) ? newB : settings.lineColor.b;

        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    default:
        break;

    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

int loop() {
    board.initData();

    BOOL b_message_ok;

    MSG message;
    WNDCLASS wincl = { 0 };

    int nCmdShow = SW_SHOW;

    HINSTANCE h_this_instance = GetModuleHandle(nullptr);

    wincl.hInstance = h_this_instance;
    wincl.lpszClassName = SZ_WIN_CLASS;
    wincl.lpfnWndProc = window_proc;

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
        settings.w_width,       /* The programs w_width */
        settings.w_height,      /* and w_height in pixels */
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
    board.close();
}

int main(int argc, char** argv) {
    int N = 0;
    srand(time(nullptr));

    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "mmap") {
            settings.mode = MODE_MMAP;
        }
        else if (s == "posix") {
            settings.mode = MODE_POSIX;
        }
        else if (s == "fstream") {
            settings.mode = MODE_FSTREAM;
        }
        else if (s == "winapi") {
            settings.mode = MODE_WINAPI;
        }
        else {
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

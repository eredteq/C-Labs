#include <windows.h>
#include <stdio.h>
#include <string>

// Макроси ідентифікаторів
#define BTN_START 1
#define BTN_READ 2
#define BTN_STOP 3
#define ID_INPUT 4
#define WM_NEW_DATA (WM_APP + 2)

// Глобальні змінні інтерфейсу
HWND hwndLog, hwndInput, hwndBtnStart, hwndBtnRead, hwndBtnStop, hwndLabel;
WNDPROC OriginalEditProc;

// Глобальні змінні для FileMapping та синхронізації
HANDLE hMapFile = NULL;
LPVOID pBuf = NULL;
HANDLE hEventDataReady = NULL;
HANDLE hMutex = NULL;
HANDLE hThread = NULL;
bool is_running = false;

const wchar_t* MapName = L"Local\\Lab6FileMapping";
const wchar_t* EventName = L"Local\\Lab6EventDataReady";
const wchar_t* MutexName = L"Local\\Lab6Mutex";
const DWORD MAP_SIZE = 2048;

// Оголошення функцій
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK InputEditProc(HWND, UINT, WPARAM, LPARAM);
void AppendLog(const wchar_t* text);
void StartServer(HWND hWnd);
void ReadFileMapping();
void StopServer();
void ProcessCommand(const wchar_t* cmd, HWND hWnd);
DWORD WINAPI WatchThread(LPVOID lpParam);

// Головна функція
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    WNDCLASSW wcex = { 0 };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"ServerClass";
    RegisterClassW(&wcex);

    HWND hWnd = CreateWindowW(L"ServerClass", L"Сервер", WS_OVERLAPPEDWINDOW,
        100, 100, 550, 450, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Додавання тексту у поле логів
void AppendLog(const wchar_t* text) {
    int len = GetWindowTextLengthW(hwndLog);
    SendMessageW(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hwndLog, EM_REPLACESEL, 0, (LPARAM)text);
}

// Потік автоматичного виявлення змін у проекції файлу (Завдання з зірочкою)
DWORD WINAPI WatchThread(LPVOID lpParam) {
    HWND hWnd = (HWND)lpParam;
    while (is_running) {
        DWORD dwWait = WaitForSingleObject(hEventDataReady, 500);
        if (!is_running) break;
        if (dwWait == WAIT_OBJECT_0) {
            PostMessageW(hWnd, WM_NEW_DATA, 0, 0);
        }
    }
    return 0;
}

// Створення проекції файлу та об'єктів синхронізації
void StartServer(HWND hWnd) {
    if (hMapFile != NULL) {
        AppendLog(L"Error: Проекцію файлу вже створено.\r\n");
        return;
    }

    // Створення об'єкта відображення файлу у пам'ять
    hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MAP_SIZE, MapName);
    if (hMapFile == NULL) {
        AppendLog(L"Error: Не вдалося створити об'єкт FileMapping.\r\n");
        return;
    }

    // Відображення об'єкта в адресний простір процесу
    pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, MAP_SIZE);
    if (pBuf == NULL) {
        AppendLog(L"Error: Подання проектованого файлу неможливе.\r\n");
        CloseHandle(hMapFile);
        hMapFile = NULL;
        return;
    }

    ZeroMemory(pBuf, MAP_SIZE);

    // Створення м'ютекса та події для синхронізації
    hMutex = CreateMutexW(NULL, FALSE, MutexName);
    hEventDataReady = CreateEventW(NULL, FALSE, FALSE, EventName);

    is_running = true;
    hThread = CreateThread(NULL, 0, WatchThread, hWnd, 0, NULL);

    AppendLog(L"Сервер FileMapping запущено. Очікування даних у спільній пам'яті...\r\n");
}

// Читання даних із проекції файлу
void ReadFileMapping() {
    if (hMapFile == NULL || pBuf == NULL) {
        AppendLog(L"Error: Проекцію файлу ще не відкрито.\r\n");
        return;
    }

    // Синхронізація доступу через м'ютекс
    if (hMutex) WaitForSingleObject(hMutex, INFINITE);

    wchar_t buffer[1024] = { 0 };
    wcscpy_s(buffer, 1024, (LPCWSTR)pBuf);

    if (hMutex) ReleaseMutex(hMutex);

    if (wcslen(buffer) > 0) {
        wchar_t logText[1200];
        swprintf_s(logText, 1200, L"\r\n-----------------------\r\nНові дані у проекції файлу:\r\n%s\r\n", buffer);
        AppendLog(logText);
    }
    else {
        AppendLog(L"Проекція файлу порожня.\r\n");
    }
}

// Закриття проекції файлу та звільнення ресурсів
void StopServer() {
    if (hMapFile == NULL) {
        AppendLog(L"Error: Сервер FileMapping вже зупинено або не був запущений.\r\n");
        return;
    }

    is_running = false;
    if (hEventDataReady) SetEvent(hEventDataReady);
    if (hThread) {
        WaitForSingleObject(hThread, 1000);
        CloseHandle(hThread);
        hThread = NULL;
    }

    if (pBuf) {
        UnmapViewOfFile(pBuf);
        pBuf = NULL;
    }
    if (hMapFile) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }
    if (hEventDataReady) {
        CloseHandle(hEventDataReady);
        hEventDataReady = NULL;
    }
    if (hMutex) {
        CloseHandle(hMutex);
        hMutex = NULL;
    }

    AppendLog(L"Сервер FileMapping зупинено. Проекцію файлу закрито.\r\n");
}

// Парсер текстових команд
void ProcessCommand(const wchar_t* cmd, HWND hWnd) {
    AppendLog(L">> ");
    AppendLog(cmd);
    AppendLog(L"\r\n");

    if (wcscmp(cmd, L"server startup") == 0 || wcscmp(cmd, L"server st") == 0) {
        StartServer(hWnd);
    }
    else if (wcscmp(cmd, L"filemapping read") == 0 || wcscmp(cmd, L"fm read") == 0) {
        ReadFileMapping();
    }
    else if (wcscmp(cmd, L"filemapping close") == 0 || wcscmp(cmd, L"fm cl") == 0 ||
        wcscmp(cmd, L"server shutdown") == 0 || wcscmp(cmd, L"server sd") == 0) {
        StopServer();
    }
    else if (wcscmp(cmd, L"help") == 0) {
        AppendLog(L"Доступні команди:\r\n");
        AppendLog(L"server startup - створити проекцію файлу. Скорочено: server st\r\n");
        AppendLog(L"filemapping read - примусово зчитати проекцію. Скорочено: fm read\r\n");
        AppendLog(L"filemapping close - закрити проекцію файлу. Скорочено: fm cl\r\n");
        AppendLog(L"help - показати цю довідку\r\n");
    }
    else {
        AppendLog(L"Невідома команда. Введіть 'help' для довідки.\r\n");
    }
}

// Підміна віконної процедури для поля вводу
LRESULT CALLBACK InputEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessageW(GetParent(hWnd), WM_APP + 1, 0, 0);
        return 0;
    }
    if (msg == WM_CHAR && wParam == VK_RETURN) {
        return 0;
    }
    return CallWindowProc(OriginalEditProc, hWnd, msg, wParam, lParam);
}

// Головна віконна процедура
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        hwndBtnStart = CreateWindowW(L"BUTTON", L"Створити проекцію", WS_CHILD | WS_VISIBLE, 10, 10, 140, 30, hWnd, (HMENU)BTN_START, NULL, NULL);
        hwndBtnRead = CreateWindowW(L"BUTTON", L"Зчитати проекцію", WS_CHILD | WS_VISIBLE, 160, 10, 140, 30, hWnd, (HMENU)BTN_READ, NULL, NULL);
        hwndBtnStop = CreateWindowW(L"BUTTON", L"Закрити проекцію", WS_CHILD | WS_VISIBLE, 310, 10, 140, 30, hWnd, (HMENU)BTN_STOP, NULL, NULL);

        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 50, 510, 300, hWnd, NULL, NULL, NULL);
        hwndLabel = CreateWindowW(L"STATIC", L">>", WS_CHILD | WS_VISIBLE, 10, 360, 20, 25, hWnd, NULL, NULL, NULL);
        hwndInput = CreateWindowW(L"EDIT", L"help", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            35, 360, 485, 25, hWnd, (HMENU)ID_INPUT, NULL, NULL);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnStart, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnRead, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnStop, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndInput, WM_SETFONT, (WPARAM)hFont, TRUE);

        OriginalEditProc = (WNDPROC)SetWindowLongPtr(hwndInput, GWLP_WNDPROC, (LONG_PTR)InputEditProc);

        AppendLog(L"Ініціалізація системи. Введіть 'help' для отримання списку команд\r\n");
        break;
    }
    case WM_NEW_DATA: {
        // Автоматичне читання при надходженні сигналу події від клієнта
        ReadFileMapping();
        break;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (hwndLog) {
            MoveWindow(hwndLog, 10, 50, width - 20, height - 100, TRUE);
            MoveWindow(hwndLabel, 10, height - 35, 20, 25, TRUE);
            MoveWindow(hwndInput, 35, height - 35, width - 45, 25, TRUE);
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 480;
        lpMMI->ptMinTrackSize.y = 350;
        break;
    }
    case WM_APP + 1: {
        wchar_t cmdBuf[256];
        GetWindowTextW(hwndInput, cmdBuf, 256);
        SetWindowTextW(hwndInput, L"");

        if (wcslen(cmdBuf) > 0) {
            ProcessCommand(cmdBuf, hWnd);
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == BTN_START) {
            StartServer(hWnd);
        }
        else if (LOWORD(wParam) == BTN_READ) {
            ReadFileMapping();
        }
        else if (LOWORD(wParam) == BTN_STOP) {
            StopServer();
        }
        break;
    case WM_DESTROY:
        if (hMapFile != NULL) StopServer();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
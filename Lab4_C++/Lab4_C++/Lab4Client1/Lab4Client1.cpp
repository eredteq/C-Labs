#include <windows.h>
#include <winsock.h>
#include <stdio.h>

#pragma comment(lib, "WS2_32.lib")

#define SERV_PORT 5000 
#define WSA_NETEVENT (WM_USER+1)
#define BTN_CONNECT 1
#define BTN_SEND 2

// Оголошення глобальних змінних інтерфейсу
HWND hwndLog, hwndBtnConnect, hwndBtnSend;

// Оголошення глобальних змінних мережі
SOCKET cln_socket = INVALID_SOCKET;
SOCKADDR_IN dest_sin;
bool is_connected = false;

// Оголошення функцій
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AppendLog(const wchar_t* text);

// Створенння вікна сервера
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    WNDCLASSW wcex = { 0 };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"Client1Class";
    RegisterClassW(&wcex);

    HWND hWnd = CreateWindowW(L"Client1Class", L"Клієнт 1", WS_OVERLAPPEDWINDOW,
        600, 100, 400, 300, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Функція додавання тексту у поле логів
void AppendLog(const wchar_t* text) {
    // Отримання довжини тексту
    int len = GetWindowTextLengthW(hwndLog);

    // Встановлення курсору в кінець
    SendMessageW(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);

    // Вставлення тексту
    SendMessageW(hwndLog, EM_REPLACESEL, 0, (LPARAM)text);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Ініціалізація бібліотеки сокетів
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        // Створення елементів інтерфейсу
        hwndBtnConnect = CreateWindowW(L"BUTTON", L"Підключитися", WS_CHILD | WS_VISIBLE, 10, 10, 120, 30, hWnd, (HMENU)BTN_CONNECT, NULL, NULL);
        hwndBtnSend = CreateWindowW(L"BUTTON", L"Відправити дані", WS_CHILD | WS_VISIBLE, 140, 10, 150, 30, hWnd, (HMENU)BTN_SEND, NULL, NULL);
        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 50, 350, 180, hWnd, NULL, NULL, NULL);

        // Встановлення шрифту
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnConnect, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnSend, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }

    case WM_SIZE: {
        // Масштабування поля логів
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (hwndLog) MoveWindow(hwndLog, 10, 50, width - 20, height - 60, TRUE);
        break;
    }

    case WM_GETMINMAXINFO: {
        // Обмеження мінімального розміру
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 350;
        lpMMI->ptMinTrackSize.y = 250;
        break;
    }

    case WM_COMMAND:
        // Обробка кнопки підключення/відключення
        if (LOWORD(wParam) == BTN_CONNECT) {
            if (!is_connected) {
                // Створення сокета
                cln_socket = socket(AF_INET, SOCK_STREAM, 0);
                dest_sin.sin_family = AF_INET;
                dest_sin.sin_port = htons(SERV_PORT);
                dest_sin.sin_addr.s_addr = inet_addr("127.0.0.1");

                // Спроба підключення
                if (connect(cln_socket, (PSOCKADDR)&dest_sin, sizeof(dest_sin)) != SOCKET_ERROR) {
                    WSAAsyncSelect(cln_socket, hWnd, WSA_NETEVENT, FD_READ | FD_CLOSE);
                    is_connected = true;
                    SetWindowTextW(hwndBtnConnect, L"Від'єднатися");
                    AppendLog(L"З'єднання з сервером було встановлено\r\n");
                }
                else {
                    if (cln_socket != INVALID_SOCKET) {
                        closesocket(cln_socket);
                        cln_socket = INVALID_SOCKET;
                    }
                    is_connected = false;
                    AppendLog(L"Помилка підключення. Сервер є недоступним\r\n");
                }
            }
            else {
                // Відключення
                closesocket(cln_socket);
                cln_socket = INVALID_SOCKET;
                is_connected = false;
                SetWindowTextW(hwndBtnConnect, L"Підключитися");
                AppendLog(L"Клієнт ініціалізував відключення від сервера\r\n");
            }
        }
        // Обробка кнопки відправки даних
        else if (LOWORD(wParam) == BTN_SEND) {
            if (!is_connected) {
                AppendLog(L"Помилка: Немає з'єднання з сервером.\r\n");
                return 0;
            }

            // Збір системних метрик
            int isServerOS = GetSystemMetrics(SM_SERVERR2);
            int monitorsCount = GetSystemMetrics(SM_CMONITORS);
            HDC hdc = GetDC(hWnd);
            int colorDepth = GetDeviceCaps(hdc, BITSPIXEL);
            ReleaseDC(hWnd, hdc);

            // Формування рядка
            wchar_t szBuf[256];
            swprintf_s(szBuf, 256, L"OS Server (SM_SERVERR2): %d\r\nКількість моніторів: %d\r\nГлибина кольору: %d bits",
                isServerOS, monitorsCount, colorDepth);

            // Відправка даних
            if (send(cln_socket, (char*)szBuf, wcslen(szBuf) * sizeof(wchar_t), 0) != SOCKET_ERROR) {
                AppendLog(L"Дані надіслано:\r\n");
                AppendLog(szBuf);
                AppendLog(L"\r\n\r\n");
            }
        }
        break;

    case WSA_NETEVENT:
        // Обробка відключення з боку сервера
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE) {
            AppendLog(L"З'єднання розірвано зі сторони серверу\r\n");
            closesocket(cln_socket);
            cln_socket = INVALID_SOCKET;
            is_connected = false;
            SetWindowTextW(hwndBtnConnect, L"Підключитися");
        }
        break;

    case WM_DESTROY:
        // Очищення ресурсів
        if (cln_socket != INVALID_SOCKET) {
            WSAAsyncSelect(cln_socket, NULL, 0, 0);
            closesocket(cln_socket);
            cln_socket = INVALID_SOCKET;
        }
        is_connected = false;
        WSACleanup();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

#include <windows.h>
#include <winsock.h>
#include <stdio.h>

#pragma comment(lib, "WS2_32.lib")

#define SERV_PORT 5000 
#define WSA_NETEVENT (WM_USER+1)
#define BTN_CONNECT 1
#define BTN_SEND 2

// Оголошення глобальних змінних інтерфейсу
HWND hwndLog, hwndInput, hwndBtnConnect, hwndBtnSend, hwndLabel;

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
    wcex.lpszClassName = L"Client3Class";
    RegisterClassW(&wcex);

    HWND hWnd = CreateWindowW(L"Client3Class", L"Клієнт 3", WS_OVERLAPPEDWINDOW,
        1050, 100, 400, 350, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Функція для додавання тексту у поле логів
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
        hwndLabel = CreateWindowW(L"STATIC", L"Введення тексту:", WS_CHILD | WS_VISIBLE, 10, 50, 100, 20, hWnd, NULL, NULL, NULL);
        hwndInput = CreateWindowW(L"EDIT", L"Hello, world!", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 70, 240, 25, hWnd, NULL, NULL, NULL);
        hwndBtnSend = CreateWindowW(L"BUTTON", L"Відправити", WS_CHILD | WS_VISIBLE, 260, 70, 100, 25, hWnd, (HMENU)BTN_SEND, NULL, NULL);
        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 110, 350, 180, hWnd, NULL, NULL, NULL);

        // Встановлення шрифту
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnConnect, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndInput, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnSend, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }

    case WM_SIZE: {
        // Масштабування елементів
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (hwndLog) {
            MoveWindow(hwndInput, 10, 70, width - 130, 25, TRUE);
            MoveWindow(hwndBtnSend, width - 110, 70, 100, 25, TRUE);
            MoveWindow(hwndLog, 10, 110, width - 20, height - 120, TRUE);
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        // Обмеження мінімального розміру
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 350;
        lpMMI->ptMinTrackSize.y = 300;
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
        // Обробка кнопки відправки тексту
        else if (LOWORD(wParam) == BTN_SEND) {
            if (!is_connected) {
                AppendLog(L"Error: відсутнє з'єднання з сервером.\r\n");
                return 0;
            }

            // Зчитування тексту з поля вводу
            wchar_t szBuf[256];
            GetWindowTextW(hwndInput, szBuf, 256);

            // Відправка даних
            if (send(cln_socket, (char*)szBuf, wcslen(szBuf) * sizeof(wchar_t), 0) != SOCKET_ERROR) {
                AppendLog(L"Ви: ");
                AppendLog(szBuf);
                AppendLog(L"\r\n");

                // Очищення поля вводу
                SetWindowTextW(hwndInput, L"");
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
        WSACleanup();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

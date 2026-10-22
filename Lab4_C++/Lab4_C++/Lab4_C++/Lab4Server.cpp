#include <windows.h>
#include <winsock.h>
#include <stdio.h>
#include <string>

#pragma comment(lib, "WS2_32.lib")

// Порт сервера
#define SERV_PORT 5000 
#define WSA_ACCEPT (WM_USER+0)
#define WSA_NETEVENT (WM_USER+1)

#define BTN_START 1
#define BTN_STOP 2
#define BTN_DISCONNECT 3
#define ID_INPUT 4

// Оголошення глобальних змінних для елементів інтерфейсу
HWND hwndLog, hwndInput, hwndBtnStart, hwndBtnStop, hwndBtnDisconnect, hwndLabel;

// Оголошення глобальних змінних для мережі
WSADATA wsaData;
SOCKET srv_socket = INVALID_SOCKET;

// Масив сокетів для клієнтів
SOCKET sock[3];

// Масив адрес для клієнтів
SOCKADDR_IN client_addrs[3];

// Змінна для збереження оригінальної процедури поля вводу
WNDPROC OriginalEditProc;

// Прапорець для відстеження режиму очікування ID клієнта
bool awaiting_disconnect_id = false;

// Оголошення функцій
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK InputEditProc(HWND, UINT, WPARAM, LPARAM);
void AppendLog(const wchar_t* text);
void StartServer(HWND hWnd);
void StopServer();
void DisconnectClient(int id);
void ProcessCommand(const wchar_t* cmd, HWND hWnd);

// Створенння вікна сервера
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

// Функція для додавання тексту у поле логів
void AppendLog(const wchar_t* text) {
    // Отримання довжини поточного тексту
    int len = GetWindowTextLengthW(hwndLog);

    // Встановлення курсору в кінець тексту
    SendMessageW(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);

    // Вставлення нового тексту
    SendMessageW(hwndLog, EM_REPLACESEL, 0, (LPARAM)text);
}

// Функція запуску сервера
void StartServer(HWND hWnd) {
    // Перевірка стану сервера
    if (srv_socket != INVALID_SOCKET) {
        AppendLog(L"Помилка: Сервер вже працює.\r\n");
        return;
    }

    // Створення сокета
    srv_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_socket == INVALID_SOCKET) {
        AppendLog(L"Помилка: Не вдалося створити сокет сервера.\r\n");
        return;
    }

    SOCKADDR_IN srv_address = {};
    srv_address.sin_family = AF_INET;
    srv_address.sin_port = htons(SERV_PORT);
    srv_address.sin_addr.s_addr = INADDR_ANY;

    // Зв'язування сокета з портом
    if (bind(srv_socket, (LPSOCKADDR)&srv_address, sizeof(srv_address)) == SOCKET_ERROR) {
        AppendLog(L"Помилка: Не вдалося зв'язати сокет із портом.\r\n");
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        return;
    }

    // Перехід у режим прослуховування
    if (listen(srv_socket, 4) == SOCKET_ERROR) {
        AppendLog(L"Помилка: Не вдалося запустити прослуховування.\r\n");
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        return;
    }

    // Налаштування асинхронного режиму
    if (WSAAsyncSelect(srv_socket, hWnd, WSA_ACCEPT, FD_ACCEPT) == SOCKET_ERROR) {
        AppendLog(L"Помилка: Не вдалося налаштувати асинхронні події.\r\n");
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        return;
    }

    AppendLog(L"Сервер запущено. Очікування підключень...\r\n");
}

// Функція зупинки сервера
void StopServer() {
    // Перевірка стану сервера
    if (srv_socket == INVALID_SOCKET) {
        AppendLog(L"Error: Сервер не запущено\r\n");
        return;
    }

    // Відключення всіх активних клієнтів
    for (int i = 0; i < 3; i++) {
        if (sock[i] != INVALID_SOCKET) {
            WSAAsyncSelect(sock[i], NULL, 0, 0);
            closesocket(sock[i]);
            sock[i] = INVALID_SOCKET;
        }
    }

    // Скасування асинхронних подій головного сокета
    WSAAsyncSelect(srv_socket, NULL, 0, 0);

    // Закриття головного сокета
    closesocket(srv_socket);
    srv_socket = INVALID_SOCKET;

    AppendLog(L"Сервер зупинено. Всіх клієнтів відключено\r\n");
}

// Функція примусового відключення конкретного клієнта
void DisconnectClient(int id) {
    // Перевірка коректності введеного ID
    if (id < 1 || id > 3) {
        AppendLog(L"Error: Недійсний номер клієнта\r\n");
        return;
    }

    // Обчислення індексу в масиві
    int index = id - 1;

    // Перевірка наявності підключення у слоті
    if (sock[index] == INVALID_SOCKET) {
        wchar_t msg[100];
        swprintf_s(msg, 100, L"Error: Слот %d порожній\r\n", id);
        AppendLog(msg);
        return;
    }

    // Скасування асинхронних подій клієнта
    WSAAsyncSelect(sock[index], NULL, 0, 0);

    // Закриття сокета клієнта
    closesocket(sock[index]);

    // Звільнення слота
    sock[index] = INVALID_SOCKET;

    // Виведення повідомлення про успішне відключення
    wchar_t msg[100];
    swprintf_s(msg, 100, L"Клієнта %d відключено зі сторони сервера\r\n", id);
    AppendLog(msg);
}

// Функція обробки текстових команд
void ProcessCommand(const wchar_t* cmd, HWND hWnd) {
    // Виведення введеної команди у лог
    AppendLog(L">> ");
    AppendLog(cmd);
    AppendLog(L"\r\n");

    // Перевірка режиму очікування введення ID
    if (awaiting_disconnect_id) {
        int id = _wtoi(cmd);
        if (id > 0) {
            DisconnectClient(id);
        }
        else {
            AppendLog(L"Error: Введено некоректне ID клієнта. Необхідно ввести дійсне ID\r\n");
        }
        // Скидання прапорця очікування
        awaiting_disconnect_id = false;
        return;
    }

    // Обробка команди запуску
    if (wcscmp(cmd, L"server startup") == 0 || wcscmp(cmd, L"server st") == 0) {
        StartServer(hWnd);
    }
    // Обробка команди зупинки
    else if (wcscmp(cmd, L"server shutdown") == 0 || wcscmp(cmd, L"server sd") == 0) {
        StopServer();
    }
    // Обробка команди виклику довідки
    else if (wcscmp(cmd, L"help") == 0) {
        AppendLog(L" Доступні команди:\r\n");
        AppendLog(L"server startup - запуск сервера. Скорочено: server st\r\n");
        AppendLog(L"server shutdown - вимкнення сервера. Скорочено: server sd\r\n");
        AppendLog(L"client remove [ID] - прибрати клієнта за його ID. Скорочено: client rm [ID]\r\n");
        AppendLog(L"help - відображення довідки\r\n");
    }
    // Обробка команди видалення клієнта
    else if (wcsncmp(cmd, L"client remove ", 14) == 0) {
        int id = 0;
        swscanf_s(cmd, L"client remove %d", &id);
        DisconnectClient(id);
    }
    // Обробка команди видалення клієнта
    else if (wcsncmp(cmd, L"client rm ", 10) == 0) {
        int id = 0;
        swscanf_s(cmd, L"client rm %d", &id);
        DisconnectClient(id);
    }
    // Обробка невідомої команди
    else {
        AppendLog(L"Невідома команда. Використайте 'help' для відображення довідки\r\n");
    }
}

// Віконна процедура для поля вводу
LRESULT CALLBACK InputEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Перевірка натискання Enter
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessageW(GetParent(hWnd), WM_APP + 1, 0, 0);
        return 0;
    }
    if (msg == WM_CHAR && wParam == VK_RETURN) {
        return 0;
    }
    // Виклик оригінальної процедури
    return CallWindowProc(OriginalEditProc, hWnd, msg, wParam, lParam);
}

// Обробка повідомлень вікна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Ініціалізація бібліотеки сокетів
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        // Ініціалізація масиву сокетів порожніми значеннями
        for (int i = 0; i < 3; i++) {
            sock[i] = INVALID_SOCKET;
        }

        // Кнопки керування
        hwndBtnStart = CreateWindowW(L"BUTTON", L"Запустити", WS_CHILD | WS_VISIBLE, 10, 10, 100, 30, hWnd, (HMENU)BTN_START, NULL, NULL);
        hwndBtnStop = CreateWindowW(L"BUTTON", L"Зупинити", WS_CHILD | WS_VISIBLE, 120, 10, 100, 30, hWnd, (HMENU)BTN_STOP, NULL, NULL);
        hwndBtnDisconnect = CreateWindowW(L"BUTTON", L"Від'єднати клієнта", WS_CHILD | WS_VISIBLE, 230, 10, 150, 30, hWnd, (HMENU)BTN_DISCONNECT, NULL, NULL);

        // Поле виведення логів
        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 50, 510, 300, hWnd, NULL, NULL, NULL);

        // Елементи терміналу
        hwndLabel = CreateWindowW(L"STATIC", L">>", WS_CHILD | WS_VISIBLE, 10, 360, 20, 25, hWnd, NULL, NULL, NULL);
        hwndInput = CreateWindowW(L"EDIT", L"help", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            35, 360, 485, 25, hWnd, (HMENU)ID_INPUT, NULL, NULL);

        // Встановлення шрифту для елементів
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnStart, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnStop, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnDisconnect, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndInput, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Підміна віконної процедури для поля вводу
        OriginalEditProc = (WNDPROC)SetWindowLongPtr(hwndInput, GWLP_WNDPROC, (LONG_PTR)InputEditProc);

        AppendLog(L"Ініціалізація системи. Введіть 'help' для отримання списку команд\r\n");
        break;
    }

    case WM_SIZE: {
        // Масштабування елементів при зміні розміру вікна
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
        // Встановлення мінімального розміру вікна
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 450;
        lpMMI->ptMinTrackSize.y = 350;
        break;
    }

    case WM_APP + 1: {
        // Обробка натискання Enter у полі вводу
        wchar_t cmdBuf[256];
        GetWindowTextW(hwndInput, cmdBuf, 256);

        // Очищення поля вводу
        SetWindowTextW(hwndInput, L"");

        // Перевірка на порожній рядок
        if (wcslen(cmdBuf) > 0) {
            ProcessCommand(cmdBuf, hWnd);
        }
        break;
    }

    case WM_COMMAND:
        // Обробка натискання кнопок
        if (LOWORD(wParam) == BTN_START) {
            StartServer(hWnd);
        }
        else if (LOWORD(wParam) == BTN_STOP) {
            StopServer();
        }
        else if (LOWORD(wParam) == BTN_DISCONNECT) {
            // Формування списку активних клієнтів
            AppendLog(L"Активні клієнти:\r\n");
            bool hasClients = false;
            for (int i = 0; i < 3; i++) {
                if (sock[i] != INVALID_SOCKET) {
                    wchar_t msg[50];
                    swprintf_s(msg, 50, L"Клієнт ID: %d\r\n", i + 1);
                    AppendLog(msg);
                    hasClients = true;
                }
            }

            // Перевірка наявності клієнтів
            if (!hasClients) {
                AppendLog(L"Відсутні підключені клієнти\r\n");
            }
            else {
                AppendLog(L"Необхідно ввести номер клієнта для відключення у поле команд та підтвердити:\r\n");
                // Встановлення прапорця очікування
                awaiting_disconnect_id = true;
                // Передача фокусу у поле вводу
                SetFocus(hwndInput);
            }
        }
        break;

    case WSA_ACCEPT: {
        // Перевірка наявності помилок при підключенні
        if (WSAGETSELECTERROR(lParam)) return 0;

        // Пошук найменшого вільного слота
        int freeSlot = -1;
        for (int i = 0; i < 3; i++) {
            if (sock[i] == INVALID_SOCKET) {
                freeSlot = i;
                break;
            }
        }

        // Перевірка наявності вільних місць
        if (freeSlot == -1) {
            // Тимчасове прийняття підключення для його закриття
            SOCKADDR_IN tempAddr;
            int tempLen = sizeof(tempAddr);
            SOCKET temp = accept(srv_socket, (SOCKADDR*)&tempAddr, &tempLen);
            closesocket(temp);
            AppendLog(L"Спроба підключення була відхилена: немає вільних слотів.\r\n");
            return 0;
        }

        // Прийняття клієнта у вільний слот
        int client_len = sizeof(client_addrs[freeSlot]);
        sock[freeSlot] = accept(srv_socket, (SOCKADDR*)&client_addrs[freeSlot], &client_len);

        // Налаштування асинхронного режиму для клієнта
        WSAAsyncSelect(sock[freeSlot], hWnd, WSA_NETEVENT, FD_READ | FD_CLOSE);

        // Виведення повідомлення про підключення
        wchar_t tempMsg[100];
        swprintf_s(tempMsg, 100, L"Клієнта підключено. Надано ID: %d\r\n", freeSlot + 1);
        AppendLog(tempMsg);
        break;
    }

    case WSA_NETEVENT: {
        // Визначення слота, від якого надійшла подія
        int Number = -1;
        for (int i = 0; i < 3; i++) {
            if (sock[i] == (SOCKET)wParam) {
                Number = i;
                break;
            }
        }

        // Перевірка коректності слота
        if (Number == -1) break;

        // Обробка отримання даних
        if (WSAGETSELECTEVENT(lParam) == FD_READ) {
            wchar_t wTemp[512];
            int rc = recv((SOCKET)wParam, (char*)wTemp, sizeof(wTemp) - sizeof(wchar_t), 0);
            if (rc > 0) {
                // Встановлення кінця рядка
                wTemp[rc / sizeof(wchar_t)] = L'\0';
                wchar_t log[1024];
                swprintf_s(log, 1024, L"\r\n Дані від Клієнта %d:\r\n%s\r\n", Number + 1, wTemp);
                AppendLog(log);
            }
        }

        // Обробка відключення клієнта
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE) {
            // Скасування асинхронних подій
            WSAAsyncSelect(sock[Number], hWnd, 0, 0);

            // Закриття сокета
            closesocket(sock[Number]);

            // Звільнення слота для нових підключень
            sock[Number] = INVALID_SOCKET;

            // Виведення повідомлення
            wchar_t tempMsg[100];
            swprintf_s(tempMsg, 100, L"Клієнт %d від'єднався. Слот сокету звільнено\r\n", Number + 1);
            AppendLog(tempMsg);
        }
        break;
    }

    case WM_DESTROY:
        // Очищення ресурсів перед виходом
        for (int i = 0; i < 3; i++) {
            if (sock[i] != INVALID_SOCKET) {
                WSAAsyncSelect(sock[i], NULL, 0, 0);
                closesocket(sock[i]);
                sock[i] = INVALID_SOCKET;
            }
        }
        if (srv_socket != INVALID_SOCKET) {
            WSAAsyncSelect(srv_socket, NULL, 0, 0);
            closesocket(srv_socket);
            srv_socket = INVALID_SOCKET;
        }
        WSACleanup();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

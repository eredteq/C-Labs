#include <windows.h>

// Глобальні змінні
HINSTANCE hInst;
WCHAR szTitle[] = L"GDI Paths - Штрихований прямокутник";
WCHAR szWindowClass[] = L"GDIPathWindowClass";

// Оголошення функцій
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW; // Перемальовувати вікно при зміні ширини/висоти
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = szWindowClass;

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int winWidth = 600;
    int winHeight = 500;

    // Створення вікна в центрі екрана
    HWND hWnd = CreateWindowW(
        szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        (screenWidth - winWidth) / 2, (screenHeight - winHeight) / 2,
        winWidth, winHeight, nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Отримання поточних розмірів клієнтської області вікна
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;

        int centerX = width / 2;
        int centerY = height / 2;

        // Розрахунок координат прямокутника (пропорційно до розмірів вікна)
        int rectWidth = width / 2;
        int rectHeight = height / 3;

        int left = centerX - rectWidth / 2;
        int top = centerY - rectHeight / 2;
        int right = left + rectWidth;
        int bottom = top + rectHeight;

        // Створення інструментів малювання
        HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
        HBRUSH hHatchBrush = CreateHatchBrush(HS_DIAGCROSS, RGB(255, 153, 51));

        // Вибір об'єктів у контекст пристрою
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hHatchBrush);

        // Налаштування фону під штрихуванням
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(255, 245, 200)); // Світло-жовтий фон

        // Побудова контуру прямокутника за допомогою GDI Path
        BeginPath(hdc);
        MoveToEx(hdc, left, top, nullptr);
        LineTo(hdc, right, top);
        LineTo(hdc, right, bottom);
        LineTo(hdc, left, bottom);
        CloseFigure(hdc);
        EndPath(hdc);

        // Отрисовка шляху: контур пером + заливка штрихованою кистю
        StrokeAndFillPath(hdc);

        // Відновлення стандартних об'єктів та очищення пам'яті
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);
        DeleteObject(hHatchBrush);

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
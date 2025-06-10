#include "PCH.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Windowsx.h>
#include <wrl/client.h> // NOTE: for ComPtr
namespace wrl = Microsoft::WRL;

constexpr const char* WINDOW_CLASS_NAME{ "vpl_window_class" };
constexpr const char* WINDOW_TITLE{ "VPL" };
constexpr DWORD WINDOW_STYLE{ WS_OVERLAPPEDWINDOW };
constexpr DWORD WINDOW_STYLE_EX{ WS_EX_OVERLAPPEDWINDOW };
constexpr int WINDOW_START_W{ 1280 };
constexpr int WINDOW_START_H{ 720 };

class Error : public std::runtime_error
{
public:
    Error(const char* file, int line, const std::string& msg)
        : std::runtime_error{ std::format("{}({}): {}\n{}", file, line, msg, std::stacktrace::current(1)) }
    {}
};

#if defined(_DEBUG)
#define Check(p) do { if (!(p)) __debugbreak(); } while (false)
#else
#define Check(p) do { if (!(p)) throw Error(__FILE__, __LINE__, "check failed: " #p); } while (false)
#endif

static LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result{};
    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    } break;
    default:
    {
        result = DefWindowProcA(hWnd, message, wParam, lParam);
    } break;
    }
    return result;
}

static void RegisterWin32WindowClass()
{
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure;
    //wc.cbClsExtra = ;
    //wc.cbWndExtra = ;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    //wc.hbrBackground = ;
    //wc.lpszMenuName = ;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm = LoadIconA(0, IDI_APPLICATION);

    Check(RegisterClassExA(&wc));
}

static HWND CreateWin32Window()
{
    RECT rect{ 0, 0, WINDOW_START_W, WINDOW_START_H };
    Check(AdjustWindowRectEx(&rect, WINDOW_STYLE, false, WINDOW_STYLE_EX));

    int window_w{ rect.right - rect.left };
    int window_h{ rect.bottom - rect.top };
    Check(window_w >= WINDOW_START_W && window_h >= WINDOW_START_H);

    HWND hwnd{};
    Check(hwnd = CreateWindowExA(
        WINDOW_STYLE_EX,
        WINDOW_CLASS_NAME, WINDOW_TITLE,
        WINDOW_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, window_w, window_h,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr
    ));

    ShowWindow(hwnd, SW_SHOW);

    return hwnd;
}

static void Entry()
{
    // win32 initialization
    Check(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE));
    RegisterWin32WindowClass();
    [[maybe_unused]] HWND window{ CreateWin32Window() };

    // main loop
    {
        MSG msg{};
        while (msg.message != WM_QUIT)
        {
            if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            else
            {
                // TODO: update logic here
            }
        }
    }
}

int main()
{
    try
    {
        Entry();
    }
    catch (const Error& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

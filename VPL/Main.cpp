// ----------------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------------

// Precompiled header
#include "PCH.h"

// Windows
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Windowsx.h>
#include <wrl/client.h> // for ComPtr
namespace wrl = Microsoft::WRL;

// D3D11
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

// ----------------------------------------------------------------------------
// Libraries
// ----------------------------------------------------------------------------

// D3D11
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")

// ----------------------------------------------------------------------------
// Program constants
// ----------------------------------------------------------------------------

constexpr const char* WINDOW_CLASS_NAME{ "vpl_window_class" };
constexpr const char* WINDOW_TITLE{ "VPL" };
constexpr DWORD WINDOW_STYLE{ WS_OVERLAPPEDWINDOW };
constexpr DWORD WINDOW_STYLE_EX{ WS_EX_OVERLAPPEDWINDOW };
constexpr int WINDOW_START_W{ 1280 };
constexpr int WINDOW_START_H{ 720 };
constexpr DXGI_FORMAT DEPTH_BUFFER_FORMAT{ DXGI_FORMAT_D32_FLOAT };

// ----------------------------------------------------------------------------
// Custom Assertions
// ----------------------------------------------------------------------------

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

#define CheckHR(hr) Check(SUCCEEDED(hr))

// ----------------------------------------------------------------------------
// Miscellaneous Utilities
// ----------------------------------------------------------------------------

static std::string StrFromWStr(const std::wstring& wstr)
{
    int buf_size{ WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr) };
    if (buf_size > 0)
    {
        std::unique_ptr<char[]> buf{ std::make_unique<char[]>(buf_size) };
        int written{ WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buf.get(), buf_size, nullptr, nullptr) };
        Check(written == buf_size);
        return buf.get();
    }
    else
    {
        return "";
    }
}

static std::wstring WStrFromStr(const std::string& wstr)
{
    int buf_size{ MultiByteToWideChar(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0) };
    if (buf_size > 0)
    {
        std::unique_ptr<wchar_t[]> buf{ std::make_unique<wchar_t[]>(buf_size) };
        int written{ MultiByteToWideChar(CP_UTF8, 0, wstr.c_str(), -1, buf.get(), buf_size) };
        Check(written == buf_size);
        return buf.get();
    }
    else
    {
        return L"";
    }
}

static std::string GetBytesStr(size_t bytes)
{
    const char* suffixes[]{ "B", "KB", "MB", "GB", "TB", "PB" };

    size_t i{};
    double val{ static_cast<double>(bytes) };
    while (val >= 1024.0 && i < std::size(suffixes) - 1)
    {
        val /= 1024.0;
        i++;
    }

    std::ostringstream out{};
    out << std::fixed << std::setprecision(2) << val << " " << suffixes[i];

    return out.str();
}

// ----------------------------------------------------------------------------
// Window Procedure
// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------
// Win32 API Helpers
// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------
// D3D11 (and DXGI) API Helpers
// ----------------------------------------------------------------------------

class FrameBuffer
{
public:
    FrameBuffer();
    FrameBuffer(ID3D11Device* d3d_dev, IDXGISwapChain1* swap_chain);
public:
    ID3D11RenderTargetView* BackBufferRTV() const noexcept { return m_back_buffer_rtv.Get(); }
    ID3D11DepthStencilView* DepthBufferDSV() const noexcept { return m_depth_buffer_dsv.Get(); }
private:
    wrl::ComPtr<ID3D11Texture2D> m_back_buffer;
    wrl::ComPtr<ID3D11RenderTargetView> m_back_buffer_rtv;
    wrl::ComPtr<ID3D11Texture2D> m_depth_buffer;
    wrl::ComPtr<ID3D11DepthStencilView> m_depth_buffer_dsv;
};

FrameBuffer::FrameBuffer()
    : m_back_buffer{}
    , m_back_buffer_rtv{}
    , m_depth_buffer{}
    , m_depth_buffer_dsv{}
{
}

FrameBuffer::FrameBuffer(ID3D11Device* d3d_dev, IDXGISwapChain1* swap_chain)
{
    // get swap chain back buffer handle
    CheckHR(swap_chain->GetBuffer(0, IID_PPV_ARGS(m_back_buffer.ReleaseAndGetAddressOf())));

    // create swap chain back buffer rtv
    CheckHR(d3d_dev->CreateRenderTargetView(m_back_buffer.Get(), nullptr, m_back_buffer_rtv.ReleaseAndGetAddressOf()));

    // get swap chain back buffer desc
    D3D11_TEXTURE2D_DESC buffer_desc{};
    m_back_buffer->GetDesc(&buffer_desc);

    // adapt the buffer desc for the depth buffer
    buffer_desc.Format = DEPTH_BUFFER_FORMAT;
    buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    // create depth buffer
    CheckHR(d3d_dev->CreateTexture2D(&buffer_desc, nullptr, m_depth_buffer.ReleaseAndGetAddressOf()));

    // create dsv from depth stencil buffer
    CheckHR(d3d_dev->CreateDepthStencilView(m_depth_buffer.Get(), nullptr, m_depth_buffer_dsv.ReleaseAndGetAddressOf()));
}

static void SetupDXGIInforQueue()
{
    #if defined(_DEBUG)
    wrl::ComPtr<IDXGIInfoQueue> queue{};
    CheckHR(DXGIGetDebugInterface1(0, IID_PPV_ARGS(queue.ReleaseAndGetAddressOf())));
    CheckHR(queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
    CheckHR(queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
    #endif
}

static wrl::ComPtr<ID3D11Device> CreateD3D11Device()
{

    UINT flags{};
    #if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif
    D3D_FEATURE_LEVEL requested_level{ D3D_FEATURE_LEVEL_11_1 };
    D3D_FEATURE_LEVEL supported_level{};

    wrl::ComPtr<ID3D11Device> d3d_dev{};
    CheckHR(D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        &requested_level, 1,
        D3D11_SDK_VERSION,
        d3d_dev.ReleaseAndGetAddressOf(),
        &supported_level,
        nullptr
    ));

    Check(requested_level == supported_level);

    return d3d_dev;
}

static void SetupD3D11InfoQueue([[maybe_unused]] ID3D11Device* d3d_dev)
{
    #if defined(_DEBUG)
    wrl::ComPtr<ID3D11InfoQueue> queue{};
    CheckHR(d3d_dev->QueryInterface(queue.ReleaseAndGetAddressOf()));
    CheckHR(queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true));
    CheckHR(queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true));
    #endif
}

static wrl::ComPtr<IDXGISwapChain1> CreateDXGISwapChain(HWND window, ID3D11Device* d3d_dev)
{
    wrl::ComPtr<IDXGIDevice> dxgi_dev{};
    CheckHR(d3d_dev->QueryInterface(dxgi_dev.ReleaseAndGetAddressOf()));
    wrl::ComPtr<IDXGIAdapter> dxgi_adapter{};
    CheckHR(dxgi_dev->GetAdapter(dxgi_adapter.ReleaseAndGetAddressOf()));

    DXGI_ADAPTER_DESC adapter_desc{};
    CheckHR(dxgi_adapter->GetDesc(&adapter_desc));

    std::cout << "adapter: " << StrFromWStr(adapter_desc.Description) << std::endl;
    std::cout << "VRAM: " << GetBytesStr(adapter_desc.DedicatedVideoMemory) << std::endl;
    std::cout << "dedicated RAM: " << GetBytesStr(adapter_desc.DedicatedSystemMemory) << std::endl;
    std::cout << "shared RAM: " << GetBytesStr(adapter_desc.SharedSystemMemory) << std::endl;

    wrl::ComPtr<IDXGIFactory2> dxgi_factory{};
    CheckHR(dxgi_adapter->GetParent(IID_PPV_ARGS(dxgi_factory.ReleaseAndGetAddressOf())));

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 0; // use window width
    desc.Height = 0; // use window height
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = false;
    desc.SampleDesc = { .Count = 1, .Quality = 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2; // double buffering
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;

    wrl::ComPtr<IDXGISwapChain1> swap_chain{};
    CheckHR(dxgi_factory->CreateSwapChainForHwnd(d3d_dev, window, &desc, nullptr, nullptr, swap_chain.ReleaseAndGetAddressOf()));

    // disable Alt+Enter changing monitor resolution to match window size
    CheckHR(dxgi_factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));

    return swap_chain;
}

// ----------------------------------------------------------------------------
// Application's Entry Point (may throw an exception)
// ----------------------------------------------------------------------------

static void Entry()
{
    // win32 initialization
    Check(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE));
    RegisterWin32WindowClass();
    [[maybe_unused]] HWND window{ CreateWin32Window() };

    // d3d11 initialization
    SetupDXGIInforQueue();
    wrl::ComPtr<ID3D11Device> d3d_dev{ CreateD3D11Device() };
    wrl::ComPtr<ID3D11DeviceContext> d3d_ctx{};
    d3d_dev->GetImmediateContext(d3d_ctx.ReleaseAndGetAddressOf());
    SetupD3D11InfoQueue(d3d_dev.Get());
    wrl::ComPtr<IDXGISwapChain1> swap_chain{ CreateDXGISwapChain(window, d3d_dev.Get()) };

    // frame buffer
    FrameBuffer frame_buffer{ d3d_dev.Get(), swap_chain.Get() };

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
                // update
                {
                    // TODO
                }

                // rendering
                {
                    float clear_color[4]{ 0.1f, 0.2f, 0.6f, 1.0f };
                    d3d_ctx->ClearRenderTargetView(frame_buffer.BackBufferRTV(), clear_color);
                    d3d_ctx->ClearDepthStencilView(frame_buffer.DepthBufferDSV(), D3D11_CLEAR_DEPTH, 1.0f, 0);
                }

                // signal the swapchain to present the current back buffer
                CheckHR(swap_chain->Present(1, 0)); // use vsync
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------

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

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

// Math library
#include "SimpleMath.h"
using Matrix = DirectX::SimpleMath::Matrix;
using Vector2 = DirectX::SimpleMath::Vector2;
using Vector3 = DirectX::SimpleMath::Vector3;
using Vector4 = DirectX::SimpleMath::Vector4;
using Quaternion = DirectX::SimpleMath::Quaternion;

// ImGui
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Shaders bytecode
#include "VS.h"
#include "PSFlat.h"

// Constant buffers
#define float2 Vector2
#define float3 Vector3
#define float4 Vector4
#define matrix Matrix
#include "ConstantBuffers.hlsli"
#undef float2
#undef float3
#undef float4
#undef matrix

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
constexpr int WINDOW_MIN_W{ 8 };
constexpr int WINDOW_MIN_H{ 8 };
constexpr DXGI_FORMAT DEPTH_BUFFER_FORMAT{ DXGI_FORMAT_D32_FLOAT };
constexpr float CAMERA_START_YAW_DEG{ -90.0f };
constexpr float CAMERA_START_PITCH_DEG{ 0.0f };
constexpr float CAMERA_MIN_PITCH_DEG{ -89.0f };
constexpr float CAMERA_MAX_PITCH_DEG{ +89.0f };
constexpr float CAMERA_FOV_DEG{ 45.0f };
constexpr float CAMERA_NEAR_PLANE{ 0.01f };
constexpr float CAMERA_FAR_PLANE{ 100.0f };
constexpr float CAMERA_MOVE_SPEED{ 10.0f };
constexpr float CAMERA_MOVE_SPEED_MULTIPLIER{ 2.0f };
constexpr float MOUSE_SENSITIVITY{ 5.0f };

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
#define Crash(msg) __debugbreak()
#else
#define Crash(msg) throw Error(__FILE__, __LINE__, msg)
#endif

#define Check(p) do { if (!(p)) Crash("check failed: " #p); } while (false)
#define CheckHR(hr) Check(SUCCEEDED(hr))

#define Unreachable() Crash("unreachable code path")

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

static bool s_did_resize{};

// keyboard state
static bool s_keyboard[0xFF]{}; // read here for details: https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

// mouse state
static struct
{
    bool left, right;
    struct { int x, y; } current;
    struct { int x, y; } previous;
    int dx, dy;
} s_mouse{};

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result{};
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
    {
        result = 1;
    }
    else
    {
        switch (msg)
        {
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            ImGuiIO& io{ ImGui::GetIO() };
            if (!io.WantCaptureKeyboard)
            {
                s_keyboard[wparam] = (msg == WM_KEYDOWN);
            }
        } break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        {
            ImGuiIO& io{ ImGui::GetIO() };
            if (!io.WantCaptureMouse)
            {
                s_mouse.left = (msg == WM_LBUTTONDOWN);
            }
        } break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        {
            ImGuiIO& io{ ImGui::GetIO() };
            if (!io.WantCaptureMouse)
            {
                s_mouse.right = (msg == WM_RBUTTONDOWN);
            }
        } break;
        case WM_MOUSEMOVE:
        {
            ImGuiIO& io{ ImGui::GetIO() };
            if (!io.WantCaptureMouse)
            {
                s_mouse.current.x = GET_X_LPARAM(lparam);
                s_mouse.current.y = GET_Y_LPARAM(lparam);
            }
        } break;
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;
        case WM_SIZE:
        {
            s_did_resize = true;
        } break;
        default:
        {
            result = DefWindowProcA(hwnd, msg, wparam, lparam);
        } break;
        }
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

LARGE_INTEGER GetWin32PerformanceCounter()
{
    LARGE_INTEGER perf{};
    Check(QueryPerformanceCounter(&perf));
    return perf;
}

LARGE_INTEGER GetWin32PerformanceFrequency()
{
    LARGE_INTEGER freq{};
    Check(QueryPerformanceFrequency(&freq));
    return freq;
}

float GetElapsedSec(LARGE_INTEGER t0, LARGE_INTEGER t1, LARGE_INTEGER frequency)
{
    float elapsed_sec{ static_cast<float>(t1.QuadPart - t0.QuadPart) / static_cast<float>(frequency.QuadPart) };
    return elapsed_sec;
}

// ----------------------------------------------------------------------------
// D3D11 (and DXGI) API Helpers
// ----------------------------------------------------------------------------

class FrameBuffer
{
public:
    FrameBuffer(ID3D11Device* d3d_dev, IDXGISwapChain1* swap_chain);
    FrameBuffer();
    ~FrameBuffer() = default;
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&&) noexcept = default;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer& operator=(FrameBuffer&&) noexcept = default;
public:
    ID3D11RenderTargetView* BackBufferRTV() const noexcept { return m_back_buffer_rtv.Get(); }
    ID3D11DepthStencilView* DepthBufferDSV() const noexcept { return m_depth_buffer_dsv.Get(); }
private:
    wrl::ComPtr<ID3D11Texture2D> m_back_buffer;
    wrl::ComPtr<ID3D11RenderTargetView> m_back_buffer_rtv;
    wrl::ComPtr<ID3D11Texture2D> m_depth_buffer;
    wrl::ComPtr<ID3D11DepthStencilView> m_depth_buffer_dsv;
};

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

FrameBuffer::FrameBuffer()
    : m_back_buffer{}
    , m_back_buffer_rtv{}
    , m_depth_buffer{}
    , m_depth_buffer_dsv{}
{
}

class Mesh
{
public:
    static Mesh Quad(ID3D11Device* d3d_dev);
    static Mesh Cube(ID3D11Device* d3d_dev);
public:
    Mesh(ID3D11Device* d3d_dev, UINT vertex_count, UINT vertex_size, const void* vertices, UINT index_count, UINT index_size, const void* indices);
    ~Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(const Mesh&) = delete;
    Mesh& operator=(Mesh&&) noexcept = default;
public:
    ID3D11Buffer* const* Vertices() const noexcept { return m_vertices.GetAddressOf(); }
    ID3D11Buffer* Indices() const noexcept { return m_indices.Get(); }
    UINT VertexCount() const noexcept { return m_vertex_count; }
    UINT IndexCount() const noexcept { return m_index_count; }
    const UINT* Stride() const noexcept { return &m_stride; }
    DXGI_FORMAT IndexFormat() const noexcept { return m_index_format; }
    const UINT* Offset() const noexcept { return &m_offset; }
private:
    wrl::ComPtr<ID3D11Buffer> m_vertices;
    wrl::ComPtr<ID3D11Buffer> m_indices;
    UINT m_vertex_count;
    UINT m_index_count;
    UINT m_stride;
    DXGI_FORMAT m_index_format;
    UINT m_offset;
};

Mesh Mesh::Quad(ID3D11Device* d3d_dev)
{
    struct Vertex
    {
        Vector3 position;
        Vector3 normal;
    };

    /*
        Local space quad vertex positions (we are on z = 0)
        We suppose the quad's normal is (0, 0, 1)

        (-0.5, +0.5)            (+0.5, +0.5)
                    +----------+
                    |          |
                    |          |
                    |          |
                    |          |
                    +----------+
        (-0.5, -0.5)            (+0.5, -0.5)

    */

    Vertex vertices[]
    {
        { { +0.5f, +0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { { -0.5f, +0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { { +0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
    };

    UINT indices[]
    {
        0, 1, 2,
        2, 3, 0,
    };

    return { d3d_dev, _countof(vertices), sizeof(*vertices), vertices, _countof(indices), sizeof(*indices), indices };
}

Mesh Mesh::Cube(ID3D11Device* d3d_dev)
{
    struct Vertex
    {
        Vector3 position;
        Vector3 normal;
    };

    Vertex vertices[]
    {
        // front face (Z+)
        { { -0.5f, -0.5f, +0.5f }, { 0.0f, 0.0f, 1.0f } },
        { { +0.5f, -0.5f, +0.5f }, { 0.0f, 0.0f, 1.0f } },
        { { +0.5f, +0.5f, +0.5f }, { 0.0f, 0.0f, 1.0f } },
        { { -0.5f, +0.5f, +0.5f }, { 0.0f, 0.0f, 1.0f } },

        // back face (Z-)
        { { +0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f } },
        { { -0.5f, +0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f } },
        { { +0.5f, +0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f } },

        // left face (X-)
        { { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f } },
        { { -0.5f, -0.5f, +0.5f }, { -1.0f, 0.0f, 0.0f } },
        { { -0.5f, +0.5f, +0.5f }, { -1.0f, 0.0f, 0.0f } },
        { { -0.5f, +0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f } },

        // right face (X+)
        { { +0.5f, -0.5f, +0.5f }, { +1.0f, 0.0f, 0.0f } },
        { { +0.5f, -0.5f, -0.5f }, { +1.0f, 0.0f, 0.0f } },
        { { +0.5f, +0.5f, -0.5f }, { +1.0f, 0.0f, 0.0f } },
        { { +0.5f, +0.5f, +0.5f }, { +1.0f, 0.0f, 0.0f } },

        // top face (Y+)
        { { -0.5f, +0.5f, +0.5f }, { 0.0f, +1.0f, 0.0f } },
        { { +0.5f, +0.5f, +0.5f }, { 0.0f, +1.0f, 0.0f } },
        { { +0.5f, +0.5f, -0.5f }, { 0.0f, +1.0f, 0.0f } },
        { { -0.5f, +0.5f, -0.5f }, { 0.0f, +1.0f, 0.0f } },

        // bottom face (Y-)
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f } },
        { { +0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f } },
        { { +0.5f, -0.5f, +0.5f }, { 0.0f, -1.0f, 0.0f } },
        { { -0.5f, -0.5f, +0.5f }, { 0.0f, -1.0f, 0.0f } },
    };

    UINT indices[]
    {
        // front
        0, 1, 2,
        0, 2, 3,

        // back
        4, 5, 6,
        4, 6, 7,

        // left
        8, 9,10,
        8,10,11,

        // right
        12,13,14,
        12,14,15,

        // top 
        16,17,18,
        16,18,19,

        // bottom
        20,21,22,
        20,22,23
    };

    return { d3d_dev, _countof(vertices), sizeof(*vertices), vertices, _countof(indices), sizeof(*indices), indices };
}

Mesh::Mesh(ID3D11Device* d3d_dev, UINT vertex_count, UINT vertex_size, const void* vertices, UINT index_count, UINT index_size, const void* indices)
    : m_vertices{}
    , m_indices{}
    , m_vertex_count{ vertex_count }
    , m_index_count{ index_count }
    , m_stride{ vertex_size }
    , m_index_format{}
    , m_offset{}
{
    Check(vertex_count > 0);
    Check(index_count > 0);
    Check(vertex_size > 0);
    Check(index_size > 0 && (index_size == 2 || index_size == 4));

    // set index format based on index stride
    switch (index_size)
    {
    case 2: { m_index_format = DXGI_FORMAT_R16_UINT; } break;
    case 4: { m_index_format = DXGI_FORMAT_R32_UINT; } break;
    default: { Unreachable(); } break;
    }

    // upload vertices to the GPU
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = m_vertex_count * vertex_size;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = vertices;
        data.SysMemPitch = 0;
        data.SysMemSlicePitch = 0;
        CheckHR(d3d_dev->CreateBuffer(&desc, &data, m_vertices.ReleaseAndGetAddressOf()));
    }

    // upload indices to the GPU
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = m_index_count * index_size;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = indices;
        data.SysMemPitch = 0;
        data.SysMemSlicePitch = 0;
        CheckHR(d3d_dev->CreateBuffer(&desc, &data, m_indices.ReleaseAndGetAddressOf()));
    }
}

class SubresourceMap
{
public:
    SubresourceMap(ID3D11DeviceContext* d3d_ctx, ID3D11Resource* res, UINT subres_idx, D3D11_MAP map_type, UINT map_flags);
    ~SubresourceMap();
    SubresourceMap(const SubresourceMap&) = delete;
    SubresourceMap(SubresourceMap&&) noexcept = delete;
    SubresourceMap& operator=(const SubresourceMap&) = delete;
    SubresourceMap& operator=(SubresourceMap&&) noexcept = delete;
public:
    void* Data() { return m_mapped_subres.pData; }
private:
    ID3D11DeviceContext* m_d3d_ctx;
    ID3D11Resource* m_res;
    UINT m_subres_idx;
    D3D11_MAPPED_SUBRESOURCE m_mapped_subres;
};

SubresourceMap::SubresourceMap(ID3D11DeviceContext* d3d_ctx, ID3D11Resource* res, UINT subres_idx, D3D11_MAP map_type, UINT map_flags)
    : m_d3d_ctx{ d3d_ctx }
    , m_res{ res }
    , m_subres_idx{ subres_idx }
    , m_mapped_subres{}
{
    CheckHR(m_d3d_ctx->Map(m_res, m_subres_idx, map_type, map_flags, &m_mapped_subres));
}

SubresourceMap::~SubresourceMap()
{
    m_d3d_ctx->Unmap(m_res, m_subres_idx);
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
    D3D_FEATURE_LEVEL requested_level{ D3D_FEATURE_LEVEL_11_0 };
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
// ImGui API Helpers
// ----------------------------------------------------------------------------

class ImGuiHandle
{
public:
    ImGuiHandle(HWND window, ID3D11Device* d3d_dev, ID3D11DeviceContext* d3d_ctx);
    ~ImGuiHandle();
    ImGuiHandle(const ImGuiHandle&) = delete;
    ImGuiHandle(ImGuiHandle&&) noexcept = delete;
    ImGuiHandle& operator=(const ImGuiHandle&) = delete;
    ImGuiHandle& operator=(ImGuiHandle&&) noexcept = delete;
};

ImGuiHandle::ImGuiHandle(HWND window, ID3D11Device* d3d_dev, ID3D11DeviceContext* d3d_ctx)
{
    // setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // setup ImGui style
    ImGui::StyleColorsDark();

    // setup platform/renderer backends
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(d3d_dev, d3d_ctx);
}

ImGuiHandle::~ImGuiHandle()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

static void StartNewImGuiFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

static void RenderImGuiFrame(ID3D11DeviceContext* d3d_ctx, ID3D11RenderTargetView* rtv)
{
    ImGui::Render();
    d3d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// ----------------------------------------------------------------------------
// Camera
// ----------------------------------------------------------------------------

struct Camera
{
    Vector3 eye;
    float yaw_deg;
    float pitch_deg;
    float fov_deg;
    float near_plane;
    float far_plane;
    Vector3 target;
};

// ----------------------------------------------------------------------------
// Application's Entry Point (may throw an exception)
// ----------------------------------------------------------------------------

static void Entry()
{
    // win32 initialization
    Check(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE));
    RegisterWin32WindowClass();
    HWND window{ CreateWin32Window() };

    // d3d11 initialization
    SetupDXGIInforQueue();
    wrl::ComPtr<ID3D11Device> d3d_dev{ CreateD3D11Device() };
    wrl::ComPtr<ID3D11DeviceContext> d3d_ctx{};
    d3d_dev->GetImmediateContext(d3d_ctx.ReleaseAndGetAddressOf());
    SetupD3D11InfoQueue(d3d_dev.Get());
    wrl::ComPtr<IDXGISwapChain1> swap_chain{ CreateDXGISwapChain(window, d3d_dev.Get()) };

    // frame buffer
    FrameBuffer frame_buffer{ d3d_dev.Get(), swap_chain.Get() };

    // viewprot
    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // shaders
    wrl::ComPtr<ID3D11VertexShader> vs{};
    CheckHR(d3d_dev->CreateVertexShader(VS_bytes, sizeof(VS_bytes), nullptr, vs.ReleaseAndGetAddressOf()));
    wrl::ComPtr<ID3D11PixelShader> ps_flat{};
    CheckHR(d3d_dev->CreatePixelShader(PSFlat_bytes, sizeof(PSFlat_bytes), nullptr, ps_flat.ReleaseAndGetAddressOf()));

    // input layout
    wrl::ComPtr<ID3D11InputLayout> input_layout{};
    {
        D3D11_INPUT_ELEMENT_DESC desc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        CheckHR(d3d_dev->CreateInputLayout(desc, std::size(desc), VS_bytes, sizeof(VS_bytes), input_layout.ReleaseAndGetAddressOf()));
    }

    // rasterizer states
    wrl::ComPtr<ID3D11RasterizerState> rs_default{};
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_BACK;
        desc.FrontCounterClockwise = true;
        desc.DepthBias = 0;
        desc.DepthBiasClamp = 0.0f;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthClipEnable = true;
        desc.ScissorEnable = false;
        desc.MultisampleEnable = false;
        desc.AntialiasedLineEnable = false;
        CheckHR(d3d_dev->CreateRasterizerState(&desc, rs_default.ReleaseAndGetAddressOf()));
    }

    // scene constant buffer
    wrl::ComPtr<ID3D11Buffer> cb_scene{};
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(SceneConstants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        CheckHR(d3d_dev->CreateBuffer(&desc, nullptr, cb_scene.ReleaseAndGetAddressOf()));
    }

    // object constant buffer
    wrl::ComPtr<ID3D11Buffer> cb_object{};
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(ObjectConstants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        CheckHR(d3d_dev->CreateBuffer(&desc, nullptr, cb_object.ReleaseAndGetAddressOf()));
    }

    // meshes
    Mesh quad{ Mesh::Quad(d3d_dev.Get()) };
    Mesh cube{ Mesh::Cube(d3d_dev.Get()) };

    // ImGui handle
    ImGuiHandle imgui_handle{ window, d3d_dev.Get(), d3d_ctx.Get() };

    // camera
    Camera camera{};
    camera.eye = { 0.0f, 2.0f, 10.0f };
    camera.yaw_deg = CAMERA_START_YAW_DEG;
    camera.pitch_deg = CAMERA_START_PITCH_DEG;
    camera.fov_deg = CAMERA_FOV_DEG;
    camera.near_plane = CAMERA_NEAR_PLANE;
    camera.far_plane = CAMERA_FAR_PLANE;
    camera.target = {};

    // TODO: to be removed
    Vector3 quad_position{};
    Vector3 quad_rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 quad_scaling{ 1.0f, 1.0f, 1.0f };
    Vector3 quad_albedo{ 1.0f, 0.0f, 0.0f };

    // time data
    const LARGE_INTEGER performance_counter_frequency{ GetWin32PerformanceFrequency() };
    LARGE_INTEGER frame_timestamp{ GetWin32PerformanceCounter() };
    float frame_t_sec{};
    float frame_dt_sec{};

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
                // update input state
                {
                    // compute mouse delta
                    s_mouse.dx = s_mouse.current.x - s_mouse.previous.x;
                    s_mouse.dy = s_mouse.current.y - s_mouse.previous.y;

                    // update previous mouse position
                    s_mouse.previous.x = s_mouse.current.x;
                    s_mouse.previous.y = s_mouse.current.y;
                }

                // fetch current window width and height
                int window_w{}, window_h{};
                {
                    RECT rect{};
                    Check(GetClientRect(window, &rect));
                    window_w = std::max(WINDOW_MIN_H, static_cast<int>(rect.right));
                    window_h = std::max(WINDOW_MIN_W, static_cast<int>(rect.bottom));
                }

                // handle resize event
                if (s_did_resize)
                {
                    // clear state (some resources may be implicitly referenced by the context)
                    d3d_ctx->ClearState();

                    // destroy frame buffer
                    frame_buffer = {};

                    // resize swap chain
                    CheckHR(swap_chain->ResizeBuffers(0, window_w, window_h, DXGI_FORMAT_UNKNOWN, 0));

                    // create new frame buffer
                    frame_buffer = { d3d_dev.Get(), swap_chain.Get() };

                    // resize event has been handled
                    s_did_resize = false;
                }

                // update logic
                {
                    // update camera
                    {
                        // update camera yaw and pitch, only when the RIGHT MOUSE BUTTON is pressed
                        if (s_mouse.right)
                        {
                            camera.yaw_deg += s_mouse.dx * frame_dt_sec * MOUSE_SENSITIVITY;
                            camera.pitch_deg -= s_mouse.dy * frame_dt_sec * MOUSE_SENSITIVITY;
                            camera.pitch_deg = std::clamp(camera.pitch_deg, CAMERA_MIN_PITCH_DEG, CAMERA_MAX_PITCH_DEG);
                        }

                        // compute camera forward from yaw and pitch
                        Vector3 camera_forward{};
                        {
                            const float yaw_rad{ DirectX::XMConvertToRadians(camera.yaw_deg) };
                            const float pitch_rad{ DirectX::XMConvertToRadians(camera.pitch_deg) };
                            camera_forward.x = std::cos(yaw_rad) * std::cos(pitch_rad);
                            camera_forward.y = std::sin(pitch_rad);
                            camera_forward.z = std::sin(yaw_rad) * std::cos(pitch_rad);
                            camera_forward.Normalize();
                        }

                        Vector3 camera_right{ camera_forward.Cross({0.0f, 1.0f, 0.0f}) };
                        camera_right.Normalize();

                        // move camera based on WASD keys
                        {
                            Vector3 move{};
                            if (s_keyboard['W']) // forward
                            {
                                move += camera_forward;
                            }
                            if (s_keyboard['S']) // backwards
                            {
                                move -= camera_forward;
                            }
                            if (s_keyboard['A']) // left
                            {
                                move -= camera_right;
                            }
                            if (s_keyboard['D']) // right
                            {
                                move += camera_right;
                            }
                            move.Normalize();

                            float speed_multipler{ s_keyboard[VK_SHIFT] ? CAMERA_MOVE_SPEED_MULTIPLIER : 1.0f };
                            camera.eye += move * CAMERA_MOVE_SPEED * speed_multipler * frame_dt_sec;
                            camera.target = camera.eye + camera_forward;
                        }
                    }
                }

                // render scene
                {
                    ID3D11RenderTargetView* back_buffer_rtv{ frame_buffer.BackBufferRTV() };
                    ID3D11DepthStencilView* back_buffer_dsv{ frame_buffer.DepthBufferDSV() };

                    float clear_color[4]{ 0.2f, 0.3f, 0.3f, 1.0f };
                    d3d_ctx->ClearRenderTargetView(back_buffer_rtv, clear_color);
                    d3d_ctx->ClearDepthStencilView(back_buffer_dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

                    // set viewport dimension
                    viewport.Width = static_cast<float>(window_w);
                    viewport.Height = static_cast<float>(window_h);

                    // prepare pipeline for drawing
                    {
                        ID3D11Buffer* cbufs[]{ cb_scene.Get(), cb_object.Get() };

                        d3d_ctx->ClearState();
                        d3d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        d3d_ctx->IASetInputLayout(input_layout.Get());
                        d3d_ctx->VSSetShader(vs.Get(), nullptr, 0);
                        d3d_ctx->VSSetConstantBuffers(0, std::size(cbufs), cbufs);
                        d3d_ctx->PSSetShader(ps_flat.Get(), nullptr, 0);
                        d3d_ctx->PSSetConstantBuffers(0, std::size(cbufs), cbufs);
                        d3d_ctx->RSSetState(rs_default.Get());
                        d3d_ctx->RSSetViewports(1, &viewport);
                        d3d_ctx->OMSetRenderTargets(1, &back_buffer_rtv, back_buffer_dsv);
                        //d3d_ctx->IASetIndexBuffer(quad.Indices(), quad.IndexFormat(), 0);
                        //d3d_ctx->IASetVertexBuffers(0, 1, quad.Vertices(), quad.Stride(), quad.Offset());
                        d3d_ctx->IASetIndexBuffer(cube.Indices(), cube.IndexFormat(), 0);
                        d3d_ctx->IASetVertexBuffers(0, 1, cube.Vertices(), cube.Stride(), cube.Offset());
                    }

                    // upload scene constants
                    {
                        float aspect{ viewport.Width / viewport.Height };
                        float fov_rad{ DirectX::XMConvertToRadians(camera.fov_deg) };

                        SubresourceMap map{ d3d_ctx.Get(), cb_scene.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                        auto constants{ static_cast<SceneConstants*>(map.Data()) };
                        constants->view = Matrix::CreateLookAt(camera.eye, camera.target, { 0.0f, 1.0f, 0.0f });
                        constants->projection = Matrix::CreatePerspectiveFieldOfView(fov_rad, aspect, camera.near_plane, camera.far_plane);
                    }

                    // upload object constants
                    {
                        Vector3 rotation_rad{};
                        rotation_rad.x = DirectX::XMConvertToRadians(quad_rotation.x);
                        rotation_rad.y = DirectX::XMConvertToRadians(quad_rotation.y);
                        rotation_rad.z = DirectX::XMConvertToRadians(quad_rotation.z);

                        Matrix translate{ Matrix::CreateTranslation(quad_position) };
                        Matrix rotate{ Matrix::CreateFromYawPitchRoll(rotation_rad) };
                        Matrix scale{ Matrix::CreateScale(quad_scaling) };
                        Matrix model{ scale * rotate * translate };
                        Matrix normal{ scale * rotate };
                        normal.Invert();
                        normal.Transpose();

                        SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                        auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                        constants->model = model;
                        constants->normal = normal;
                        constants->albedo = quad_albedo;
                    }

                    // draw
                    //d3d_ctx->DrawIndexed(quad.IndexCount(), 0, 0);
                    d3d_ctx->DrawIndexed(cube.IndexCount(), 0, 0);
                }

                // render ui
                StartNewImGuiFrame();
                {
                    ImGui::Begin("VPL");
                    {
                        if (ImGui::CollapsingHeader("Frame Data", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Text("Time: %.1f sec", frame_t_sec);
                            ImGui::Text("Delta Time: %.3f sec", frame_dt_sec);
                            ImGui::Text("Delta Time: %.2f msec", frame_dt_sec * 1000.0f);
                        }
                        if (ImGui::CollapsingHeader("Quad", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            // position editor
                            {
                                float position[3]{ quad_position.x, quad_position.y, quad_position.z };
                                ImGui::DragFloat3("Position", position, 0.01f);
                                quad_position = { position[0], position[1], position[2] };
                            }
                            // rotation editor
                            {
                                float rotation[3]{ quad_rotation.x, quad_rotation.y, quad_rotation.z };
                                ImGui::DragFloat3("Rotation", rotation, 0.1f, 0.0f, 360.0f);
                                quad_rotation = { rotation[0], rotation[1], rotation[2] };
                            }
                            // scaling editor
                            {
                                float scaling[3]{ quad_scaling.x, quad_scaling.y, quad_scaling.z };
                                ImGui::DragFloat3("Scaling", scaling, 0.01f);
                                quad_scaling = { scaling[0], scaling[1], scaling[2] };
                            }
                            // albedo
                            {
                                float color[3]{ quad_albedo.x, quad_albedo.y, quad_albedo.z };
                                ImGui::ColorEdit3("Albedo", color);
                                quad_albedo = { color[0], color[1], color[2] };
                            }
                        }
                    }
                    ImGui::End();
                }
                RenderImGuiFrame(d3d_ctx.Get(), frame_buffer.BackBufferRTV());

                // present
                {
                    CheckHR(swap_chain->Present(1, 0)); // use vsync
                }

                // update frame time data
                {
                    LARGE_INTEGER timestamp{ GetWin32PerformanceCounter() };
                    frame_dt_sec = GetElapsedSec(frame_timestamp, timestamp, performance_counter_frequency);
                    frame_t_sec += frame_dt_sec;
                    frame_timestamp = timestamp;
                }
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

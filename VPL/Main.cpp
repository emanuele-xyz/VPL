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

// Math Library
#include "SimpleMath.h"
using Matrix = DirectX::SimpleMath::Matrix;
using Vector2 = DirectX::SimpleMath::Vector2;
using Vector3 = DirectX::SimpleMath::Vector3;
using Vector4 = DirectX::SimpleMath::Vector4;
using Quaternion = DirectX::SimpleMath::Quaternion;

// ImGui Library
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Shaders bytecode
#include "VS.h"
#include "PSFlat.h"
#include "PSLit.h"
#include "PSShadowed.h"
#include "PSPointLight.h"
#include "PSCubeShadowMap.h"
#include "PSSkybox.h"

// Constant buffers
#define float2 Vector2
#define float3 Vector3
#define float4 Vector4
#define matrix Matrix
#define int int32_t
#include "ConstantBuffers.hlsli"
#undef float2
#undef float3
#undef float4
#undef matrix
#undef int

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
constexpr float POINT_LIGHT_RADIUS{ 0.25f };
constexpr float POINT_LIGHT_MIN_INTENSITY{ 1.0f };
constexpr float POINT_LIGHT_MAX_INTENSITY{ 100.0f };
constexpr float POINT_LIGHT_START_INTENSITY{ 5.0f };
constexpr UINT LINE_VERTEX_COUNT{ 2 };
constexpr Vector3 LINE_OK_COLOR{ 0.0f, 1.0f, 0.0f };
constexpr Vector3 LINE_ERROR_COLOR{ 1.0f, 0.0f, 0.0f };
constexpr Vector3 LINE_NORMAL_COLOR{ 1.0f, 0.0f, 1.0f };
constexpr float LINE_NORMAL_T{ 0.5f };
constexpr float LINE_ERROR_T{ 10.0f };
constexpr int PARTICLES_COUNT_START{ 10 };
constexpr int PARTICLES_COUNT_MIN{ 1 };
constexpr int PARTICLES_COUNT_MAX{ 1000 };
constexpr float MEAN_REFLECTIVITY_START{ 0.5f };
constexpr float MEAN_REFLECTIVITY_MIN{ 0.1f };
constexpr float MEAN_REFLECTIVITY_MAX{ 0.9f };
constexpr int MIN_SELECTED_LIGHT_PATH_INDEX{ -1 };
constexpr int MIN_SELECTED_LIGHT_INDEX{ -1 };
constexpr int POINT_LIGHT_INDEX{};
constexpr int CUBE_MAP_FACES{ 6 };
constexpr int CUBE_SHADOW_MAP_SIZE{ 1024 };
constexpr float CUBE_SHADOW_MAP_NEAR{ 0.1f };
constexpr float CUBE_SHADOW_MAP_FAR{ 10.0f };
constexpr float CUBE_SHADOW_MAP_STATIC_BIAS_START{ 0.01f };
constexpr float CUBE_SHADOW_MAP_MAX_DYNAMIC_BIAS_START{ 0.1f };
constexpr float CUBE_SHADOW_MAP_BIAS_MIN{ 0.0f };
constexpr float CUBE_SHADOW_MAP_BIAS_MAX{ 1.0f };
constexpr int CUBE_SHADOW_MAP_PCF_SAMPLES_START{ 4 };
constexpr int CUBE_SHADOW_MAP_PCF_SAMPLES_MIN{ 1 };
constexpr int CUBE_SHADOW_MAP_PCF_SAMPLES_MAX{ PCF_MAX_SAMPLES };
constexpr float CUBE_SHADOW_MAP_PCF_OFFSET_SCALE_START{ 0.005f };
constexpr float CUBE_SHADOW_MAP_PCF_OFFSET_SCALE_MIN{ 0.0f };
constexpr float CUBE_SHADOW_MAP_PCF_OFFSET_SCALE_MAX{ 1.0f };

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
// Vertex Definition
// ----------------------------------------------------------------------------

struct Vertex
{
    Vector3 position;
    Vector3 normal;
};

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

class CubeShadowMap
{
public:
    CubeShadowMap(ID3D11Device* d3d_dev);
    ~CubeShadowMap() = default;
    CubeShadowMap(const CubeShadowMap&) = delete;
    CubeShadowMap(CubeShadowMap&&) noexcept = delete;
    CubeShadowMap& operator=(const CubeShadowMap&) = delete;
    CubeShadowMap& operator=(CubeShadowMap&&) noexcept = delete;
public:
    ID3D11DepthStencilView* DSVs(int face_idx) const noexcept { return m_dsvs[face_idx].Get(); }
    ID3D11ShaderResourceView* SRV() const noexcept { return m_srv.Get(); }
private:
    wrl::ComPtr<ID3D11Texture2D> m_cube_map;
    wrl::ComPtr<ID3D11DepthStencilView> m_dsvs[CUBE_MAP_FACES];
    wrl::ComPtr<ID3D11ShaderResourceView> m_srv;
};

CubeShadowMap::CubeShadowMap(ID3D11Device* d3d_dev)
    : m_cube_map{}
{
    // create cube map texture
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(CUBE_SHADOW_MAP_SIZE);
        desc.Height = static_cast<UINT>(CUBE_SHADOW_MAP_SIZE);
        desc.MipLevels = 1;
        desc.ArraySize = static_cast<UINT>(CUBE_MAP_FACES);
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.SampleDesc = { .Count = 1, .Quality = 0 };
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        CheckHR(d3d_dev->CreateTexture2D(&desc, nullptr, m_cube_map.ReleaseAndGetAddressOf()));
    }

    // create DSVs
    for (int face_idx{}; face_idx < CUBE_MAP_FACES; face_idx++)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray.MipSlice = 0;
        desc.Texture2DArray.FirstArraySlice = face_idx;
        desc.Texture2DArray.ArraySize = 1;
        CheckHR(d3d_dev->CreateDepthStencilView(m_cube_map.Get(), &desc, m_dsvs[face_idx].ReleaseAndGetAddressOf()));
    }

    // create SRV
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        desc.TextureCube.MostDetailedMip = 0;
        desc.TextureCube.MipLevels = 1;
        CheckHR(d3d_dev->CreateShaderResourceView(m_cube_map.Get(), &desc, m_srv.ReleaseAndGetAddressOf()));
    }
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

    std::println("adapter: {}", StrFromWStr(adapter_desc.Description));
    std::println("VRAM: {}", GetBytesStr(adapter_desc.DedicatedVideoMemory));
    std::println("dedicated RAM: {}", GetBytesStr(adapter_desc.DedicatedSystemMemory));
    std::println("shared RAM: {}", GetBytesStr(adapter_desc.SharedSystemMemory));

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
// Geometry
// ----------------------------------------------------------------------------

struct Ray
{
    Vector3 origin;
    Vector3 direction;
};

struct RayHit
{
    bool valid;
    Vector3 position;
    Vector3 normal;
};

using RayIntersectFn = RayHit(Ray ray, const Matrix& model, const Matrix& normal);

static RayHit RayQuadIntersect(Ray ray, const Matrix& model, const Matrix& normal)
{
    /*
        ray/quad intersection test in local space

        ray: p(t) = o + td
        plane p.n + s = 0

        quad in local space lies on z=0 (we know it because we know how the quad mesh has been defined)
        n: (0,0,1)
        s: 0
        plane: p.(0,0,1) = 0

        we plug the ray equation into the plane equation

        (o + td).(0,0,1) = 0
        o.(0,0,1) + td.(0,0,1) = 0
        o_z + t * d_z = 0
        t * d_z = - o_z
        t = - o_z / d_z

        if d_z != 0 then t exists
        since we have a ray t must also be >= 0

        if both conditions are met, there is an intersection at point p(t)
    */

    RayHit hit{};

    // inverse transform: world -> model
    Matrix inverse_model{ model.Invert() };

    // transform world space ray into model space ray
    {
        Vector4 origin{ ray.origin.x, ray.origin.y, ray.origin.z, 1.0f }; // influenced by translations
        Vector4 direction{ ray.direction.x, ray.direction.y, ray.direction.z, 0.0f }; // NOT influenced by translations

        origin = Vector4::Transform(origin, inverse_model); // local space ray origin
        direction = Vector4::Transform(direction, inverse_model); // local space ray direction

        ray.origin = { origin.x, origin.y, origin.z };
        ray.direction = { direction.x, direction.y, direction.z };
    }

    // ray/plane intersection in local space
    if (ray.direction.z != 0)
    {
        float t{ -(ray.origin.z) / (ray.direction.z) };
        if (t > 0) // we ignore hits at the ray's origin
        {
            // find hit local space position 
            Vector3 local_hit{ ray.origin + t * ray.direction };

            // check whether local_hit is within the quad's bounds or not
            if ((-0.5f <= local_hit.x && local_hit.x <= +0.5f) && (-0.5f <= local_hit.y && local_hit.y <= +0.5f))
            {
                hit.valid = true;

                // compute world hit
                Vector4 world_hit{ Vector4::Transform({local_hit.x, local_hit.y, local_hit.z, 1.0f}, model) };
                hit.position = { world_hit.x, world_hit.y, world_hit.z };

                // compute normal at hit point
                Vector4 local_normal{ 0.0f, 0.0f, 1.0f, 0.0f }; // NOT influenced by translations
                Vector4 world_normal{ Vector4::Transform(local_normal, normal) };
                world_normal.Normalize();
                hit.normal = { world_normal.x, world_normal.y, world_normal.z };
            }
        }
    }

    return hit;
}

static RayHit RayBoxIntersect(Ray ray, const Matrix& model, const Matrix& normal)
{
    /*
        ray/box intersection test in local space

        ray: p(t) = o + td

        in local space, the box is an AABB from (-0.5, -0.5, -0.5) to (+0.5, +0.5, +0.5)
        we can consider the box as being made up of three slabs
        - one slab with normal (0, 0, 1) and shifts -0.5, +0.5
        - one slab with normal (0, 1, 0) and shifts -0.5, +0.5
        - one slab with normal (1, 0, 0) and shifts -0.5, +0.5

        we use the slab method for finding the ray/box intersection
        - we intersect the ray with the first slab, finding an interval for t
        - we intersect the ray with the second slab, finding an interval for t
        - we intersect the ray with the third slab, finding an interval for t

        if the intersection of the found intervals is not empty, we have an intersection point at p(t)
    */

    RayHit hit{};

    // inverse transform: world -> model
    Matrix inverse_model{ model.Invert() };

    // transform world space ray into model space ray
    {
        Vector4 origin{ ray.origin.x, ray.origin.y, ray.origin.z, 1.0f }; // influenced by translations
        Vector4 direction{ ray.direction.x, ray.direction.y, ray.direction.z, 0.0f }; // NOT influenced by translations

        origin = Vector4::Transform(origin, inverse_model); // local space ray origin
        direction = Vector4::Transform(direction, inverse_model); // local space ray direction

        ray.origin = { origin.x, origin.y, origin.z };
        ray.direction = { direction.x, direction.y, direction.z };
    }

    /*
        ray/slab intersection (slab 1)

        slab:
        p.(0,0,1) - 0.5 = 0
        p.(0,0,1) + 0.5 = 0

        we plug the ray equation

        (o + t_a * d).(0, 0, 1) - 0.5 = 0
        o.(0,0,1) + (t_a * d).(0,0,1) - 0.5 = 0
        o_z + t_a * d_z - 0.5 = 0
        t_a * d_z = 0.5 - o_z
        t_a = (0.5 - o_z) / d_z

        analogously

        t_b = (-0.5 - o_z) / d_z

        it is easy to derive the formulas for the other slabs
    */

    float t_min{ 0.0f }; // intervals intersection lower bound
    float t_max{ std::numeric_limits<float>::infinity() }; // intervals intersection upper bound
    {
        float ray_origin[3]{ ray.origin.x, ray.origin.y, ray.origin.z };
        float ray_direction[3]{ ray.direction.x, ray.direction.y, ray.direction.z };

        for (int i{}; i < 3; i++) // the hardcoded 3 stands for the three slabs
        {
            if (ray_direction[i] != 0) // there is an intersection
            {
                // find interval
                float t_a{ (+0.5f - ray_origin[i]) / ray_direction[i] };
                float t_b{ (-0.5f - ray_origin[i]) / ray_direction[i] };

                // intersect found interval
                t_min = std::max(t_min, std::min(t_a, t_b));
                t_max = std::min(t_max, std::max(t_a, t_b));
            }
            else // there is no intersection
            {
                t_max = t_min; // collapse the intervals intersection into a single value
            }
        }
    }

    if (t_min < t_max) // the ray intersects the box (we ignore single value intervals)
    {
        if (t_min > 0) // we ignore hits at the ray's origin
        {
            hit.valid = true;

            // find hit local space position 
            Vector3 local_hit{ ray.origin + t_min * ray.direction };

            // compute world hit
            Vector4 world_hit{ Vector4::Transform({local_hit.x, local_hit.y, local_hit.z, 1.0f}, model) };
            hit.position = { world_hit.x, world_hit.y, world_hit.z };

            // find hit normal
            {
                float local_normal[3]{};
                float hit_position[3]{ local_hit.x, local_hit.y, local_hit.z };

                constexpr float EPSILON{ 0.0001 }; // TODO: hardcoded epsilon
                for (int i{}; i < 3; i++)
                {
                    if (std::abs(std::abs(hit_position[i]) - 0.5f) < EPSILON)
                    {
                        local_normal[i] = hit_position[i] > 0.0f ? +1.0f : -1.0f;
                        break;
                    }
                }

                Vector4 world_normal{ Vector4::Transform({local_normal[0], local_normal[1], local_normal[2], 0.0f}, normal) };
                world_normal.Normalize();
                hit.normal = { world_normal.x, world_normal.y, world_normal.z };
            }
        }
    }

    return hit;
}

// ----------------------------------------------------------------------------
// Scene
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

struct Object
{
    std::string name{ "unknown" };
    Vector3 position{};
    Vector3 rotation{};
    Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    Mesh* mesh{};
    Vector3 albedo{ 1.0f, 1.0f, 1.0f };
    RayIntersectFn* ray_intersect_fn{};
    Matrix model{ Matrix::Identity };
    Matrix normal{ Matrix::Identity };
};

struct PointLight
{
    Vector3 position;
    Vector3 color;
    float intenisty;
};

// ----------------------------------------------------------------------------
// VPL
// ----------------------------------------------------------------------------

struct LightPathNode
{
    Ray ray;
    RayHit hit;
    Vector3 ray_color;
    Vector3 hit_color;
};

/*
    Either a point light or a VPL
*/
struct VirtualLight
{
    Vector3 position;
    Vector3 normal;
    Vector3 color;
    int bounce;
};

static Vector3 CompensateVPLColor(int particles_count, float mean_reflectivity, int bounce, Vector3 color)
{
    // TODO: revise this
    /*
        Keller corrects each VPL color multiplying it by N / floor(w), where
        - N is the number of particles/rays we shot from the light source
        - w = mean_reflectivity^bounce * N
        here bounce is the number of bounces the ray had to do before hitting the point in which the VPL was spawned
        Keller spaws N VPLs on the surface of the light source and considers them to be at bounce 0.
        Then, mean_reflectivity * N rays are cast.
        Their hit points are at bounce 1, and so on ...
        In our implementation, we did not spawn N VPLs on the light source.
        Instead, we simply shot N rays from it.
        These N rays will hit something.
        These hits are considered to be at bounce zero.
        Thus, for our implementation to adhere to Keller's, our compensation needs to use as exponent bounce+1 instead of bounce.
        NOTE: is this right? I don't think so!
    */
    // NOTE: here should we use as exponent bounce? or bounce+1?
    // NOTE: if we use bounce+1 the denominator may become zero!
    float num{ static_cast<float> (particles_count) };
    float den{ static_cast<float>(std::floor(std::pow(mean_reflectivity, bounce) * particles_count)) };
    Vector3 compensated_color{};
    float compensation{ num / den };
    Check(den != 0.0f);
    //if (den != 0.0f)
    {
        compensated_color = compensation * color;
    }
    return compensated_color;
}

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

    // cube shadow map
    CubeShadowMap cube_shadow_map{ d3d_dev.Get() };

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
    wrl::ComPtr<ID3D11PixelShader> ps_lit{};
    CheckHR(d3d_dev->CreatePixelShader(PSLit_bytes, sizeof(PSLit_bytes), nullptr, ps_lit.ReleaseAndGetAddressOf()));
    wrl::ComPtr<ID3D11PixelShader> ps_shadowed{};
    CheckHR(d3d_dev->CreatePixelShader(PSShadowed_bytes, sizeof(PSShadowed_bytes), nullptr, ps_shadowed.ReleaseAndGetAddressOf()));
    wrl::ComPtr<ID3D11PixelShader> ps_point_light{};
    CheckHR(d3d_dev->CreatePixelShader(PSPointLight_bytes, sizeof(PSPointLight_bytes), nullptr, ps_point_light.ReleaseAndGetAddressOf()));
    wrl::ComPtr<ID3D11PixelShader> ps_cube_shadow_map{};
    CheckHR(d3d_dev->CreatePixelShader(PSCubeShadowMap_bytes, sizeof(PSCubeShadowMap_bytes), nullptr, ps_cube_shadow_map.ReleaseAndGetAddressOf()));
    wrl::ComPtr<ID3D11PixelShader> ps_skybox{};
    CheckHR(d3d_dev->CreatePixelShader(PSSkybox_bytes, sizeof(PSSkybox_bytes), nullptr, ps_skybox.ReleaseAndGetAddressOf()));

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

    // default rasterizer state
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

    // rasterizer state used for rendering the cube shadow map
    wrl::ComPtr<ID3D11RasterizerState> rs_cube_shadow_map{};
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.FrontCounterClockwise = true;
        desc.DepthBias = 0;
        desc.DepthBiasClamp = 0.0f;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthClipEnable = true;
        desc.ScissorEnable = false;
        desc.MultisampleEnable = false;
        desc.AntialiasedLineEnable = false;
        CheckHR(d3d_dev->CreateRasterizerState(&desc, rs_cube_shadow_map.ReleaseAndGetAddressOf()));
    }

    // rasterizer state with no culling enabled
    wrl::ComPtr<ID3D11RasterizerState> rs_no_cull{};
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.FrontCounterClockwise = true;
        desc.DepthBias = 0;
        desc.DepthBiasClamp = 0.0f;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthClipEnable = true;
        desc.ScissorEnable = false;
        desc.MultisampleEnable = false;
        desc.AntialiasedLineEnable = false;
        CheckHR(d3d_dev->CreateRasterizerState(&desc, rs_no_cull.ReleaseAndGetAddressOf()));
    }

    // cube shadow map sampler
    wrl::ComPtr<ID3D11SamplerState> ss_cube_shadow_map{};
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        //desc.ComparisonFunc = ;
        desc.BorderColor[0] = 1.0f;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        CheckHR(d3d_dev->CreateSamplerState(&desc, ss_cube_shadow_map.ReleaseAndGetAddressOf()));
    }

    // skybox sampler
    wrl::ComPtr<ID3D11SamplerState> ss_skybox{};
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.BorderColor[0] = 1.0f;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        CheckHR(d3d_dev->CreateSamplerState(&desc, ss_skybox.ReleaseAndGetAddressOf()));
    }

    // blend state for summing
    wrl::ComPtr<ID3D11BlendState> bs_sum{};
    {
        D3D11_BLEND_DESC desc{};
        desc.AlphaToCoverageEnable = false;
        desc.IndependentBlendEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        CheckHR(d3d_dev->CreateBlendState(&desc, bs_sum.ReleaseAndGetAddressOf()));
    }

    // depth stencil state for depth testing pass only with equal depth
    wrl::ComPtr<ID3D11DepthStencilState> ds_equal{};
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = true;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_EQUAL;
        desc.StencilEnable = false;
        //desc.StencilReadMask = ;
        //desc.StencilWriteMask = ;
        //desc.FrontFace = ;
        //desc.BackFace = ;
        CheckHR(d3d_dev->CreateDepthStencilState(&desc, ds_equal.ReleaseAndGetAddressOf()));
    }

    // depth stencil state for no depth testing
    wrl::ComPtr<ID3D11DepthStencilState> ds_no_depth{};
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = false;
        desc.StencilEnable = false;
        CheckHR(d3d_dev->CreateDepthStencilState(&desc, ds_no_depth.ReleaseAndGetAddressOf()));
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

    // light constant buffer
    wrl::ComPtr<ID3D11Buffer> cb_light{};
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(LightConstants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        CheckHR(d3d_dev->CreateBuffer(&desc, nullptr, cb_light.ReleaseAndGetAddressOf()));
    }

    // cube shadow map constant buffer
    wrl::ComPtr<ID3D11Buffer> cb_shadow{};
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(ShadowConstants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        CheckHR(d3d_dev->CreateBuffer(&desc, nullptr, cb_shadow.ReleaseAndGetAddressOf()));
    }

    // line vertex buffer
    wrl::ComPtr<ID3D11Buffer> vb_line{};
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(Vertex) * LINE_VERTEX_COUNT;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        CheckHR(d3d_dev->CreateBuffer(&desc, nullptr, vb_line.ReleaseAndGetAddressOf()));
    }

    // meshes
    Mesh quad_mesh{ Mesh::Quad(d3d_dev.Get()) };
    Mesh cube_mesh{ Mesh::Cube(d3d_dev.Get()) };

    // ImGui handle
    ImGuiHandle imgui_handle{ window, d3d_dev.Get(), d3d_ctx.Get() };

    // configuration variables
    int seed{};
    int particles_count{ PARTICLES_COUNT_START };
    float mean_reflectivity{ MEAN_REFLECTIVITY_START };
    bool draw_light_paths{ true };
    bool draw_lost_light_path_rays{};
    int selected_light_path_index{ MIN_SELECTED_LIGHT_PATH_INDEX };
    bool draw_vpls{ true };
    int selected_light_index{ MIN_SELECTED_LIGHT_INDEX };
    int selected_vpl_type{ LIGHT_TYPE_POINT };
    bool draw_cube_shadow_map{};
    float cube_shadow_map_static_bias{ CUBE_SHADOW_MAP_STATIC_BIAS_START };
    float cube_shadow_map_max_dynamic_bias{ CUBE_SHADOW_MAP_MAX_DYNAMIC_BIAS_START };
    int pcf_samples{ CUBE_SHADOW_MAP_PCF_SAMPLES_START };
    float pcf_offset_scale{ CUBE_SHADOW_MAP_PCF_OFFSET_SCALE_START };

    // controls conficuration variables
    bool invert_camera_mouse_x{};
    bool invert_camera_mouse_y{};

    // uniform distribution between [0, 1)
    std::uniform_real_distribution<float> dis{ 0.0f, 1.0f };

    // scene camera
    Camera camera{};
    camera.eye = { 0.0f, 2.0f, 10.0f };
    camera.yaw_deg = CAMERA_START_YAW_DEG;
    camera.pitch_deg = CAMERA_START_PITCH_DEG;
    camera.fov_deg = CAMERA_FOV_DEG;
    camera.near_plane = CAMERA_NEAR_PLANE;
    camera.far_plane = CAMERA_FAR_PLANE;
    camera.target = {};

    // scene point light
    PointLight point_light{};
    point_light.position = { 0.0f, 3.25f, 1.0f };
    point_light.color = { 1.0f, 1.0f, 1.0f };
    point_light.intenisty = POINT_LIGHT_START_INTENSITY;

    // scene objects
    std::vector<Object> objects{};
    {
        Object& obj{ objects.emplace_back() };
        obj.name = "Left Cube";
        obj.position = { -0.40f, 1.35f, -0.75f };
        obj.rotation = { 0.0f, 20.0f, 0.0f };
        obj.scaling = { 1.5f, 2.75f, 1.0f };
        obj.mesh = &cube_mesh;
        obj.albedo = { 1.0f, 1.0f, 1.0f };
        obj.ray_intersect_fn = RayBoxIntersect;
    }
    {
        Object& obj{ objects.emplace_back() };
        obj.name = "Right Cube";
        obj.position = { 1.0f, 0.61f, 1.15f };
        obj.rotation = { 0.0f, -15.0f, 0.0f };
        obj.scaling = { 1.25f, 1.25f, 1.25f };
        obj.mesh = &cube_mesh;
        obj.albedo = { 1.0f, 1.0f, 1.0f };
        obj.ray_intersect_fn = RayBoxIntersect;
    }
    {
        Object& obj{ objects.emplace_back() };
        obj.name = "Floor";
        obj.position = {};
        obj.rotation = { 270.0f, 0.0f, 0.0f };
        obj.scaling = { 4.0f, 4.0f, 1.0f };
        obj.mesh = &quad_mesh;
        obj.albedo = { 1.0f, 1.0f, 1.0f };
        obj.ray_intersect_fn = RayQuadIntersect;
    }
    {
        Object& obj{ objects.emplace_back() };
        obj.name = "Cieling";
        obj.position = { 0.0f, 4.0f, 0.0f };
        obj.rotation = { 90.0f, 0.0f, 0.0f };
        obj.scaling = { 4.0f, 4.0f, 1.0f };
        obj.mesh = &quad_mesh;
        obj.albedo = { 1.0f, 1.0f, 1.0f };
        obj.ray_intersect_fn = RayQuadIntersect;
    }
    {
        Object& obj{ objects.emplace_back() };
        obj.name = "Left Wall";
        obj.position = { -2.0f, 2.0f, 0.0f };
        obj.rotation = { 0.0f, 90.0f, 0.0f };
        obj.scaling = { 4.0f, 4.0f, 1.0f };
        obj.mesh = &quad_mesh;
        obj.albedo = { 1.0f, 0.0f, 0.0f };
        obj.ray_intersect_fn = RayQuadIntersect;
    }
    {
        Object& obj{ objects.emplace_back() };
        obj.name = "Right Wall";
        obj.position = { 2.0f, 2.0f, 0.0f };
        obj.rotation = { 0.0f, 270.0f, 0.0f };
        obj.scaling = { 4.0f, 4.0f, 1.0f };
        obj.mesh = &quad_mesh;
        obj.albedo = { 0.0f, 1.0f, 0.0f };
        obj.ray_intersect_fn = RayQuadIntersect;
    }
    {
        Object& obj{ objects.emplace_back() };
        obj.name = "Back Wall";
        obj.position = { 0.0f, 2.0f, -2.0f };
        obj.rotation = { 0.0f, 0.0f, 0.0f };
        obj.scaling = { 4.0f, 4.0f, 1.0f };
        obj.mesh = &quad_mesh;
        obj.albedo = { 1.0f, 1.0f, 1.0f };
        obj.ray_intersect_fn = RayQuadIntersect;
    }
    {
        Object& obj{ objects.emplace_back() };
        obj.name = "Front Wall";
        obj.position = { 0.0f, 2.0f, 2.0f };
        obj.rotation = { 0.0f, 180.0f, 0.0f };
        obj.scaling = { 4.0f, 4.0f, 1.0f };
        obj.mesh = &quad_mesh;
        obj.albedo = { 1.0f, 1.0f, 1.0f };
        obj.ray_intersect_fn = RayQuadIntersect;
    }

    // light paths
    std::vector<std::vector<LightPathNode>> light_paths{};

    // virtual lights (main point light + VPLs)
    std::vector<VirtualLight> virtual_lights{};

    // validate scene objects: no two objects can have the same name
    {
        std::unordered_set<std::string> object_names{};
        for (const Object& obj : objects)
        {
            if (object_names.contains(obj.name))
            {
                Crash(std::format("two or more scene objects have the same name '{}'", obj.name));
            }
            else
            {
                object_names.emplace(obj.name);
            }
        }
    }

    // validate scene objects: all objects must be able to intersect with a ray
    for (const Object& obj : objects)
    {
        if (!obj.ray_intersect_fn)
        {
            Crash(std::format("object '{}' doesn't support ray intersection", obj.name));
        }
    }

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
                            camera.yaw_deg += (invert_camera_mouse_x ? -1.0f : +1.0f) * s_mouse.dx * frame_dt_sec * MOUSE_SENSITIVITY;
                            camera.pitch_deg -= (invert_camera_mouse_y ? -1.0f : +1.0f) * s_mouse.dy * frame_dt_sec * MOUSE_SENSITIVITY;
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

                    // update object model and normal matrices (any change to the object's transform MUST happen BEFORE this)
                    for (Object& obj : objects)
                    {
                        Vector3 rotation_rad{};
                        rotation_rad.x = DirectX::XMConvertToRadians(obj.rotation.x);
                        rotation_rad.y = DirectX::XMConvertToRadians(obj.rotation.y);
                        rotation_rad.z = DirectX::XMConvertToRadians(obj.rotation.z);

                        Matrix translate{ Matrix::CreateTranslation(obj.position) };
                        Matrix rotate{ Matrix::CreateFromYawPitchRoll(rotation_rad) };
                        Matrix scale{ Matrix::CreateScale(obj.scaling) };
                        Matrix model{ scale * rotate * translate };
                        Matrix normal{ scale * rotate };
                        normal.Invert();
                        normal.Transpose();

                        obj.model = model;
                        obj.normal = normal;
                    }

                    // validate configuration variables
                    {
                        particles_count = std::clamp(particles_count, PARTICLES_COUNT_MIN, PARTICLES_COUNT_MAX);
                        mean_reflectivity = std::clamp(mean_reflectivity, MEAN_REFLECTIVITY_MIN, MEAN_REFLECTIVITY_MAX);
                        selected_light_path_index = std::clamp(selected_light_path_index, MIN_SELECTED_LIGHT_PATH_INDEX, static_cast<int>(light_paths.size()) - 1);
                        selected_light_index = std::clamp(selected_light_index, MIN_SELECTED_LIGHT_INDEX, static_cast<int>(virtual_lights.size()) - 1);
                        cube_shadow_map_static_bias = std::clamp(cube_shadow_map_static_bias, CUBE_SHADOW_MAP_BIAS_MIN, CUBE_SHADOW_MAP_BIAS_MAX) ;
                        cube_shadow_map_max_dynamic_bias = std::clamp(cube_shadow_map_max_dynamic_bias, CUBE_SHADOW_MAP_BIAS_MIN, CUBE_SHADOW_MAP_BIAS_MAX);
                        pcf_samples = std::clamp(pcf_samples, CUBE_SHADOW_MAP_PCF_SAMPLES_MIN, CUBE_SHADOW_MAP_PCF_SAMPLES_MAX);
                        pcf_offset_scale = std::clamp(pcf_offset_scale, CUBE_SHADOW_MAP_PCF_OFFSET_SCALE_MIN, CUBE_SHADOW_MAP_PCF_OFFSET_SCALE_MAX);
                    }

                    // start new light paths by shooting random rays from the point light
                    {
                        light_paths.clear(); // forget the previous frame's light paths

                        // generate random ray direction from random positions on a unit sphere
                        {
                            std::mt19937 generator{ static_cast<unsigned>(seed) };

                            for (int i{}; i < particles_count; i++)
                            {
                                float theta{ 2.0f * static_cast<float>(std::numbers::pi) * dis(generator) }; // azimuthal angle (0 to 2π)
                                float z{ 2.0f * dis(generator) - 1.0f }; // z-coordinate (-1 to 1)
                                float r{ sqrt(1.0f - z * z) }; // radius at that z

                                float x{ r * cos(theta) };
                                float y{ r * sin(theta) };

                                Ray ray{};
                                ray.origin = point_light.position;
                                ray.direction = { x, y, z };

                                // generate new light path with randomly generated starting ray
                                std::vector<LightPathNode>& light_path{ light_paths.emplace_back() };
                                LightPathNode start{};
                                start.ray = ray;
                                start.ray_color = point_light.color;
                                light_path.emplace_back(start);
                            }
                        }
                    }

                    // build light paths by intersecting rays with the scene geometry and eventually making them bounce
                    {
                        for (int i{}; i < static_cast<int>(light_paths.size()); i++)
                        {
                            std::vector<LightPathNode>& light_path{ light_paths[i] }; // light path we are building

                            int bounce{}; // counter for the number of ray bounces
                            bool last_ray_hit_something{ true };

                            /*
                                This while loop deals with ray bounce logic.
                                Keller tells us that:
                                - the first mean_reflectivity^1 * N rays bounce at least once.
                                - the first mean_reflectivity^2 * N rays bounce at least twice.
                                - the first mean_reflectivity^3 * N rays bounce at least trice.
                                - ...
                                - the first mean_reflectivity^j * N rays bounce at least j times.
                                - and so on ...
                            */
                            while (i < static_cast<int>(std::pow(mean_reflectivity, bounce) * particles_count) && last_ray_hit_something)
                            {
                                Ray ray{ light_path.back().ray }; // starting ray
                                RayHit closest{}; // closest ray hit
                                const Object* closest_obj{}; // closest object hit

                                for (const Object& obj : objects) // test each object for ray intersection
                                {
                                    RayHit hit{ obj.ray_intersect_fn(ray, obj.model, obj.normal) };
                                    if (hit.valid) // there is an intersection point
                                    {
                                        // check if the current hit is closer than the closest hit recorded up until now
                                        if (!closest.valid) // cloeset hit up until now is not valid
                                        {
                                            // current hit is closer
                                            closest = hit;
                                            closest_obj = &obj;
                                        }
                                        else // cloeset hit up until now is valid
                                        {
                                            // we need to compare the distance between both hits and the ray origin
                                            Vector3 o_closest{ closest.position - ray.origin }; // from ray origin to closest hit position
                                            Vector3 o_hit{ hit.position - ray.origin }; // from ray origin to current hit position
                                            float d_o_closest{ o_closest.Dot(o_closest) }; // squared adistance between ray origin and closest hit 
                                            float d_o_hit{ o_hit.Dot(o_hit) }; // squared adistance between ray origin and current hit 
                                            if (d_o_hit < d_o_closest)
                                            {
                                                // current hit is closer than closest hit
                                                closest = hit;
                                                closest_obj = &obj;
                                            }
                                        }
                                    }
                                }

                                if (closest.valid) // the ray hit something
                                {
                                    // compute ray reflection
                                    Vector3 reflection{ Vector3::Reflect(ray.direction, closest.normal) };
                                    Ray reflected_ray{ closest.position, reflection };

                                    // compute hit albedo attenuating the ray's color by the object's albedo divided by PI
                                    Vector3 hit_color{ light_path.back().ray_color * (closest_obj->albedo / std::numbers::pi_v<float>) };

                                    // record current ray hit into the light path
                                    light_path.back().hit = closest;
                                    light_path.back().hit_color = hit_color;

                                    // append the next light path node given by the reflected direction vector
                                    {
                                        LightPathNode next{};
                                        next.ray = reflected_ray;
                                        next.ray_color = hit_color;
                                        light_path.emplace_back(next);
                                    }
                                }

                                last_ray_hit_something = closest.valid;

                                bounce++; // go to the next bounce
                            }
                        }
                    }

                    // spawn VPLs
                    {
                        virtual_lights.clear(); // forget about previous frame virtual lights

                        // the main point light is treated as a virtual light (and must have index POINT_LIGHT_INDEX)
                        {
                            VirtualLight light{};
                            light.position = point_light.position;
                            light.color = point_light.color;
                            virtual_lights.emplace_back(light);
                        }

                        // spawn VPLs at light paths hits
                        for (const std::vector<LightPathNode>& light_path : light_paths)
                        {
                            for (int j{}; j < static_cast<int>(light_path.size()); j++)
                            {
                                const LightPathNode& node{ light_path[j] };

                                // if we hit something, we spawn a VPL
                                if (node.hit.valid)
                                {
                                    VirtualLight vpl{};
                                    vpl.position = node.hit.position;
                                    vpl.normal = node.hit.normal;
                                    vpl.color = CompensateVPLColor(particles_count, mean_reflectivity, j, node.hit_color);
                                    vpl.bounce = j;
                                    virtual_lights.emplace_back(vpl);
                                }
                            }
                        }
                    }
                }

                // prepare cube shaodw map render
                {
                    // set viewport dimension
                    viewport.Width = static_cast<float>(CUBE_SHADOW_MAP_SIZE);
                    viewport.Height = static_cast<float>(CUBE_SHADOW_MAP_SIZE);

                    // prepare pipeline for drawing
                    {
                        ID3D11Buffer* cbufs[]{ cb_scene.Get(), cb_object.Get(), cb_light.Get(), cb_shadow.Get() };

                        d3d_ctx->ClearState();
                        d3d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        d3d_ctx->IASetInputLayout(input_layout.Get());
                        d3d_ctx->VSSetShader(vs.Get(), nullptr, 0);
                        d3d_ctx->VSSetConstantBuffers(0, std::size(cbufs), cbufs);
                        d3d_ctx->PSSetShader(ps_cube_shadow_map.Get(), nullptr, 0);
                        d3d_ctx->PSSetConstantBuffers(0, std::size(cbufs), cbufs);
                        d3d_ctx->RSSetState(rs_cube_shadow_map.Get());
                        d3d_ctx->RSSetViewports(1, &viewport);
                    }
                }

                // disable no RTV bound warning
                #if defined(_DEBUG)
                {
                    wrl::ComPtr<ID3D11InfoQueue> queue{};
                    CheckHR(d3d_dev->QueryInterface(queue.ReleaseAndGetAddressOf()));

                    D3D11_MESSAGE_ID deny_msgs[]{
                        D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
                        D3D11_MESSAGE_ID_DEVICE_DRAW_SAMPLER_NOT_SET,
                        D3D11_MESSAGE_ID_DEVICE_DRAW_SHADERRESOURCEVIEW_NOT_SET,
                    };
                    D3D11_INFO_QUEUE_FILTER filter{};
                    filter.DenyList.NumIDs = std::size(deny_msgs);
                    filter.DenyList.pIDList = deny_msgs;

                    CheckHR(queue->PushStorageFilter(&filter));
                }
                #endif

                // render cube shadow map
                {
                    // directions to use for computing the view matrix of each side of the shadow cube map
                    Vector3 view_directions[6]
                    {
                        { +1.0f, +0.0f, +0.0f }, // +X
                        { -1.0f, +0.0f, +0.0f }, // -X
                        { +0.0f, +1.0f, +0.0f }, // +Y
                        { +0.0f, -1.0f, +0.0f }, // -Y
                        { +0.0f, +0.0f, +1.0f }, // +Z
                        { +0.0f, +0.0f, -1.0f }, // -Z
                    };

                    // up vectors to use for computing the view matrix of each side of the shadow cube map
                    Vector3 view_ups[6]
                    {
                        { +0.0f, +1.0f, +0.0f }, // +X
                        { +0.0f, +1.0f, +0.0f }, // -X
                        { +0.0f, +0.0f, -1.0f }, // +Y
                        { +0.0f, +0.0f, +1.0f }, // -Y
                        { +0.0f, +1.0f, +0.0f }, // +Z
                        { +0.0f, +1.0f, +0.0f }, // -Z
                    };

                    // upload light constants
                    {
                        SubresourceMap map{ d3d_ctx.Get(), cb_light.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                        auto constants{ static_cast<LightConstants*>(map.Data()) };
                        constants->world_position = point_light.position;
                    }

                    // for each side of the cube
                    for (int face_idx{}; face_idx < CUBE_MAP_FACES; face_idx++)
                    {
                        // render the scene
                        {
                            // clear color buffer and depth buffer
                            d3d_ctx->ClearDepthStencilView(cube_shadow_map.DSVs(face_idx), D3D11_CLEAR_DEPTH, 1.0f, 0);

                            // set cube shadow map side as depth buffer
                            d3d_ctx->OMSetRenderTargets(0, nullptr, cube_shadow_map.DSVs(face_idx));

                            // upload scene constants
                            {
                                float fov_rad{ static_cast<float>(std::numbers::pi) / 2.0f };
                                float aspect{ 1.0f };

                                SubresourceMap map{ d3d_ctx.Get(), cb_scene.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                auto constants{ static_cast<SceneConstants*>(map.Data()) };
                                constants->view = DirectX::XMMatrixLookAtLH(point_light.position, point_light.position + view_directions[face_idx], view_ups[face_idx]);
                                constants->projection = DirectX::XMMatrixPerspectiveFovLH(fov_rad, aspect, CUBE_SHADOW_MAP_NEAR, CUBE_SHADOW_MAP_FAR);
                            }

                            // upload shadow constants
                            {
                                SubresourceMap map{ d3d_ctx.Get(), cb_shadow.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                auto constants{ static_cast<ShadowConstants*>(map.Data()) };
                                constants->far_plane = CUBE_SHADOW_MAP_FAR;
                                constants->static_bias = cube_shadow_map_static_bias;
                                constants->max_dynamic_bias = cube_shadow_map_max_dynamic_bias;
                                constants->pcf_samples = pcf_samples;
                                constants->offset_scale = pcf_offset_scale;
                            }

                            // render each object
                            for (const Object& obj : objects)
                            {
                                // upload object constants
                                {
                                    SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                    auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                                    constants->model = obj.model;
                                    constants->normal = obj.normal;
                                    constants->albedo = obj.albedo;
                                }

                                // set pipeline state
                                d3d_ctx->IASetIndexBuffer(obj.mesh->Indices(), obj.mesh->IndexFormat(), 0);
                                d3d_ctx->IASetVertexBuffers(0, 1, obj.mesh->Vertices(), obj.mesh->Stride(), obj.mesh->Offset());

                                // draw
                                d3d_ctx->DrawIndexed(obj.mesh->IndexCount(), 0, 0);
                            }
                        }
                    }
                }

                // reset D3D11 info queue
                #if defined(_DEBUG)
                {
                    wrl::ComPtr<ID3D11InfoQueue> queue{};
                    CheckHR(d3d_dev->QueryInterface(queue.ReleaseAndGetAddressOf()));
                    queue->PopStorageFilter();
                }
                #endif

                // prepare final render
                {
                    // clear color buffer and depth buffer
                    {
                        float clear_color[4]{ 0.2f, 0.3f, 0.3f, 1.0f };
                        d3d_ctx->ClearRenderTargetView(frame_buffer.BackBufferRTV(), clear_color);
                        d3d_ctx->ClearDepthStencilView(frame_buffer.DepthBufferDSV(), D3D11_CLEAR_DEPTH, 1.0f, 0);
                    }

                    // set viewport dimension
                    viewport.Width = static_cast<float>(window_w);
                    viewport.Height = static_cast<float>(window_h);

                    // prepare pipeline for drawing
                    {
                        ID3D11RenderTargetView* rtv{ frame_buffer.BackBufferRTV() };
                        ID3D11Buffer* cbufs[]{ cb_scene.Get(), cb_object.Get(), cb_light.Get(), cb_shadow.Get() };
                        ID3D11ShaderResourceView* srvs[]{ cube_shadow_map.SRV() };
                        ID3D11SamplerState* sss[]{ ss_cube_shadow_map.Get(), ss_skybox.Get() };

                        d3d_ctx->ClearState();
                        d3d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        d3d_ctx->IASetInputLayout(input_layout.Get());
                        d3d_ctx->VSSetShader(vs.Get(), nullptr, 0);
                        d3d_ctx->VSSetConstantBuffers(0, std::size(cbufs), cbufs);
                        d3d_ctx->PSSetConstantBuffers(0, std::size(cbufs), cbufs);
                        d3d_ctx->PSSetShaderResources(0, std::size(srvs), srvs);
                        d3d_ctx->PSSetSamplers(0, std::size(sss), sss);
                        d3d_ctx->RSSetState(rs_default.Get());
                        d3d_ctx->RSSetViewports(1, &viewport);
                        d3d_ctx->OMSetRenderTargets(1, &rtv, frame_buffer.DepthBufferDSV());
                    }

                    // upload scene constants
                    {
                        float aspect{ viewport.Width / viewport.Height };
                        float fov_rad{ DirectX::XMConvertToRadians(camera.fov_deg) };

                        SubresourceMap map{ d3d_ctx.Get(), cb_scene.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                        auto constants{ static_cast<SceneConstants*>(map.Data()) };
                        constants->view = Matrix::CreateLookAt(camera.eye, camera.target, { 0.0f, 1.0f, 0.0f });
                        constants->projection = Matrix::CreatePerspectiveFieldOfView(fov_rad, aspect, camera.near_plane, camera.far_plane);
                        constants->world_eye = camera.eye;
                        constants->particles_count = selected_light_index > MIN_SELECTED_LIGHT_INDEX ? 1.0f : static_cast<float>(particles_count);
                    }
                }

                // render the scene for each virtual light, accumulating the result
                {
                    /*
                        A non negative selected light index means that the user wants to see the contribution of a single light source
                        A negative selected light index means that the user wants to see the final frame
                    */
                    for (int i{}; i < static_cast<int>(virtual_lights.size()); i++)
                    {
                        // skip non selected light (when one is actually selected)
                        if (selected_light_index > MIN_SELECTED_LIGHT_INDEX && i != selected_light_index) continue;

                        const VirtualLight& light{ virtual_lights[i] };

                        // upload light constants
                        {
                            SubresourceMap map{ d3d_ctx.Get(), cb_light.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                            auto constants{ static_cast<LightConstants*>(map.Data()) };
                            constants->world_position = light.position;
                            constants->color = light.color;
                            constants->normal = light.normal;
                            constants->intensity = point_light.intenisty;
                            constants->type = i == POINT_LIGHT_INDEX ? LIGHT_TYPE_POINT : selected_vpl_type;
                        }

                        // it is the first frame we render, or we only want to render the contribution of a single virtual light
                        if (i == 0 || selected_light_index > MIN_SELECTED_LIGHT_INDEX)
                        {
                            d3d_ctx->OMSetBlendState(nullptr, nullptr, 0XFFFFFFFF); // default blend state
                            d3d_ctx->OMSetDepthStencilState(nullptr, 0); // default blend state
                        }
                        else // we need to accumulate the currently rendered frame into the back buffer
                        {
                            // sum the color of the current fragment with the one in the depth buffer
                            d3d_ctx->OMSetBlendState(bs_sum.Get(), nullptr, 0XFFFFFFFF);
                            // depth test passes only for fragments that have the same depth of the one stored in the depth buffer
                            d3d_ctx->OMSetDepthStencilState(ds_equal.Get(), 0);
                        }

                        // render each object
                        for (const Object& obj : objects)
                        {
                            // upload object constants
                            {
                                SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                                constants->model = obj.model;
                                constants->normal = obj.normal;
                                constants->albedo = obj.albedo;
                            }

                            // set pipeline state
                            {
                                // set pipeline state
                                d3d_ctx->IASetIndexBuffer(obj.mesh->Indices(), obj.mesh->IndexFormat(), 0);
                                d3d_ctx->IASetVertexBuffers(0, 1, obj.mesh->Vertices(), obj.mesh->Stride(), obj.mesh->Offset());

                                // if we are lighting the scene using the main point light, then also render shadows
                                if (i == POINT_LIGHT_INDEX)
                                {
                                    d3d_ctx->PSSetShader(ps_shadowed.Get(), nullptr, 0);
                                }
                                else // otherwise, if lighting the scene using a VPL, don't render any shadows
                                {
                                    d3d_ctx->PSSetShader(ps_lit.Get(), nullptr, 0);
                                }
                            }

                            // draw
                            d3d_ctx->DrawIndexed(obj.mesh->IndexCount(), 0, 0);
                        }
                    }

                    // from this point onwards, we use the default blend state and depth stencil state
                    {
                        d3d_ctx->OMSetBlendState(nullptr, nullptr, 0XFFFFFFFF); // default blend state
                        d3d_ctx->OMSetDepthStencilState(nullptr, 0); // default blend state
                    }

                    // render main point light (only if we are rendering the final frame or we have selected the point light)
                    if (selected_light_index == MIN_SELECTED_LIGHT_INDEX || selected_light_index == POINT_LIGHT_INDEX)
                    {
                        // upload object constants (light impostor cube)
                        {
                            float point_ligt_diameter{ POINT_LIGHT_RADIUS * 2.0f };

                            Matrix translate{ Matrix::CreateTranslation(point_light.position) };
                            Matrix scale{ Matrix::CreateScale({point_ligt_diameter, point_ligt_diameter, point_ligt_diameter}) };
                            Matrix model{ scale * translate };

                            SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                            auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                            constants->model = model;
                        }

                        // upload light constants
                        {
                            SubresourceMap map{ d3d_ctx.Get(), cb_light.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                            auto constants{ static_cast<LightConstants*>(map.Data()) };
                            constants->world_position = point_light.position;
                            constants->radius = POINT_LIGHT_RADIUS;
                            constants->color = point_light.color;
                            constants->intensity = point_light.intenisty;
                            constants->type = LIGHT_TYPE_POINT;
                        }

                        // set pipeline state
                        d3d_ctx->IASetIndexBuffer(cube_mesh.Indices(), cube_mesh.IndexFormat(), 0); // use cube mesh as light impostor
                        d3d_ctx->IASetVertexBuffers(0, 1, cube_mesh.Vertices(), cube_mesh.Stride(), cube_mesh.Offset()); // use cube mesh as light impostor
                        d3d_ctx->PSSetShader(ps_point_light.Get(), nullptr, 0);

                        // draw
                        d3d_ctx->DrawIndexed(cube_mesh.IndexCount(), 0, 0);
                    }
                }

                // render visualizations
                {
                    // render VPLs
                    {
                        for (int i{ POINT_LIGHT_INDEX + 1 }; i < static_cast<int>(virtual_lights.size()) && draw_vpls; i++)
                        {
                            // skip non selected VPL (when one is actually selected)
                            if (selected_light_index > MIN_SELECTED_LIGHT_INDEX && i != selected_light_index) continue;

                            const VirtualLight& vpl{ virtual_lights[i] };

                            float radius{ POINT_LIGHT_RADIUS / 2.0f }; // TODO: hardcoded

                            // upload object constants
                            {
                                float diameter{ radius * 2.0f };

                                Matrix translate{ Matrix::CreateTranslation(vpl.position) };
                                Matrix scale{ Matrix::CreateScale({diameter, diameter, diameter}) };
                                Matrix model{ scale * translate };

                                SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                                constants->model = model;
                            }

                            // upload light constants
                            {
                                SubresourceMap map{ d3d_ctx.Get(), cb_light.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                auto constants{ static_cast<LightConstants*>(map.Data()) };
                                constants->world_position = vpl.position;
                                constants->radius = radius;
                                constants->color = vpl.color;
                                constants->intensity = point_light.intenisty;
                                constants->type = i == POINT_LIGHT_INDEX ? LIGHT_TYPE_POINT : selected_vpl_type;
                            }

                            // set pipeline state
                            d3d_ctx->IASetIndexBuffer(cube_mesh.Indices(), cube_mesh.IndexFormat(), 0); // use cube mesh as light impostor
                            d3d_ctx->IASetVertexBuffers(0, 1, cube_mesh.Vertices(), cube_mesh.Stride(), cube_mesh.Offset()); // use cube mesh as light impostor
                            d3d_ctx->PSSetShader(ps_point_light.Get(), nullptr, 0);

                            // draw
                            d3d_ctx->DrawIndexed(cube_mesh.IndexCount(), 0, 0);
                        }
                    }

                    // from here onwads, we only render lines
                    d3d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

                    // render light paths visualizations
                    for (int i{}; i < static_cast<int>(light_paths.size()) && draw_light_paths; i++)
                    {
                        // skip non selected light paths (when one is actually selected)
                        if (selected_light_path_index > MIN_SELECTED_LIGHT_PATH_INDEX && i != selected_light_path_index) continue;

                        const std::vector<LightPathNode>& light_path{ light_paths[i] };
                        for (int j{}; j < static_cast<int>(light_path.size()); j++)
                        {
                            LightPathNode node{ light_path[j] };

                            // if the current light path segment is a valid, render it
                            if (i < static_cast<int>(std::pow(mean_reflectivity, j) * particles_count))
                            {
                                bool is_lost{ !node.hit.valid }; // has the light path segment been lost?
                                // we should render a light path segment either if it is not lost or if we want to render lost rays
                                bool should_be_rendered{ !is_lost || draw_lost_light_path_rays };

                                if (should_be_rendered)
                                {
                                    // upload object constants (line)
                                    {
                                        SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                        auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                                        constants->model = Matrix::Identity; // we pass line vertices in world space
                                        constants->albedo = node.hit.valid ? LINE_OK_COLOR : LINE_ERROR_COLOR;
                                    }

                                    // upload line vertices
                                    {
                                        SubresourceMap map{ d3d_ctx.Get(), vb_line.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                        auto vertices{ static_cast<Vertex*>(map.Data()) };
                                        if (node.hit.valid)
                                        {
                                            vertices[0] = { .position = { node.ray.origin } };
                                            vertices[1] = { .position = { node.hit.position } };
                                        }
                                        else
                                        {
                                            vertices[0] = { .position = { node.ray.origin } };
                                            vertices[1] = { .position = { node.ray.origin + LINE_ERROR_T * node.ray.direction } };
                                        }
                                    }

                                    // set pipeline state
                                    {
                                        ID3D11Buffer* vbufs[]{ vb_line.Get() };
                                        UINT strides[]{ sizeof(Vertex) };
                                        UINT offsets[]{ 0 };

                                        d3d_ctx->IASetVertexBuffers(0, std::size(vbufs), vbufs, strides, offsets);
                                        d3d_ctx->PSSetShader(ps_flat.Get(), nullptr, 0);
                                    }

                                    // draw
                                    d3d_ctx->Draw(LINE_VERTEX_COUNT, 0);
                                }
                            }
                        }
                    }

                    // render VPLs normals
                    for (int i{ POINT_LIGHT_INDEX + 1 }; i < static_cast<int>(virtual_lights.size()) && draw_vpls; i++)
                    {
                        // skip non selected VPL (when one is actually selected)
                        if (selected_light_index > POINT_LIGHT_INDEX && i != selected_light_index) continue;

                        const VirtualLight& vpl{ virtual_lights[i] };

                        // upload object constants (line)
                        {
                            SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                            auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                            constants->model = Matrix::Identity; // we pass line vertices in world space
                            constants->albedo = LINE_NORMAL_COLOR; // obnoxious pink 
                        }

                        // upload line vertices
                        {
                            SubresourceMap map{ d3d_ctx.Get(), vb_line.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                            auto vertices{ static_cast<Vertex*>(map.Data()) };
                            vertices[0] = { .position = { vpl.position} };
                            vertices[1] = { .position = { vpl.position + LINE_NORMAL_T * vpl.normal} };
                        }

                        // set pipeline state
                        {
                            ID3D11Buffer* vbufs[]{ vb_line.Get() };
                            UINT strides[]{ sizeof(Vertex) };
                            UINT offsets[]{ 0 };

                            d3d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
                            d3d_ctx->IASetVertexBuffers(0, std::size(vbufs), vbufs, strides, offsets);
                            d3d_ctx->PSSetShader(ps_flat.Get(), nullptr, 0);
                        }

                        // draw
                        d3d_ctx->Draw(LINE_VERTEX_COUNT, 0);
                    }
                }

                // render cube shadow map as a skybox
                if (draw_cube_shadow_map)
                {
                    // prepare pipeline for drawing
                    {
                        ID3D11RenderTargetView* rtv{ frame_buffer.BackBufferRTV() };

                        d3d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        d3d_ctx->IASetIndexBuffer(cube_mesh.Indices(), cube_mesh.IndexFormat(), 0);
                        d3d_ctx->IASetVertexBuffers(0, 1, cube_mesh.Vertices(), cube_mesh.Stride(), cube_mesh.Offset());
                        d3d_ctx->RSSetState(rs_no_cull.Get());
                        d3d_ctx->PSSetShader(ps_skybox.Get(), nullptr, 0);
                        d3d_ctx->OMSetDepthStencilState(ds_no_depth.Get(), 0);
                        d3d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
                    }

                    // upload scene constants
                    {
                        float aspect{ viewport.Width / viewport.Height };
                        float fov_rad{ DirectX::XMConvertToRadians(camera.fov_deg) };

                        SubresourceMap map{ d3d_ctx.Get(), cb_scene.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                        auto constants{ static_cast<SceneConstants*>(map.Data()) };
                        constants->view = Matrix::CreateLookAt({ 0.0f, 0.0f, 0.0f }, camera.target - camera.eye, { 0.0f, 1.0f, 0.0f }); // we don't want translations
                        constants->projection = Matrix::CreatePerspectiveFieldOfView(fov_rad, aspect, camera.near_plane, camera.far_plane);
                    }

                    // upload object constants
                    {
                        SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                        auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                        constants->model = Matrix::Identity;
                    }

                    // draw
                    d3d_ctx->DrawIndexed(cube_mesh.IndexCount(), 0, 0);
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
                        if (ImGui::CollapsingHeader("Configuration", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::DragInt("Seed", &seed, 1.0f);
                            ImGui::DragInt("Particles", &particles_count, 1.0f, PARTICLES_COUNT_MIN, PARTICLES_COUNT_MAX);
                            ImGui::DragFloat("Mean Reflectivity", &mean_reflectivity, 0.001f, MEAN_REFLECTIVITY_MIN, MEAN_REFLECTIVITY_MAX);
                            ImGui::Checkbox("Draw Light Paths", &draw_light_paths);
                            ImGui::Checkbox("Draw Lost Light Path Rays", &draw_lost_light_path_rays);
                            ImGui::DragInt("Light Path Index", &selected_light_path_index, 0.1f, MIN_SELECTED_LIGHT_PATH_INDEX, static_cast<int>(light_paths.size()) - 1);
                            ImGui::Checkbox("Draw VPLs", &draw_vpls);
                            ImGui::DragInt("Light Index", &selected_light_index, 0.1f, MIN_SELECTED_LIGHT_INDEX, static_cast<int>(virtual_lights.size()) - 1);
                            // VPL type editor
                            {
                                const char* vpl_type_descs[]{ "Point", "Sign Cosine Weighted", "Cosine Weighted" };
                                ImGui::Combo("VPL Type", &selected_vpl_type, vpl_type_descs, std::size(vpl_type_descs));
                            }
                        }
                        if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Checkbox("Draw Shadow Map", &draw_cube_shadow_map);
                            ImGui::DragFloat("Static Bias", &cube_shadow_map_static_bias, 0.001f, CUBE_SHADOW_MAP_BIAS_MIN, CUBE_SHADOW_MAP_BIAS_MAX);
                            ImGui::DragFloat("Max Dynamic Bias", &cube_shadow_map_max_dynamic_bias, 0.001f, CUBE_SHADOW_MAP_BIAS_MIN, CUBE_SHADOW_MAP_BIAS_MAX);
                            ImGui::DragInt("PCF Samples", &pcf_samples, 1.0f, CUBE_SHADOW_MAP_PCF_SAMPLES_MIN, CUBE_SHADOW_MAP_PCF_SAMPLES_MAX);
                            ImGui::DragFloat("PCF Offset Scale", &pcf_offset_scale, 0.001f, CUBE_SHADOW_MAP_PCF_OFFSET_SCALE_MIN, CUBE_SHADOW_MAP_PCF_OFFSET_SCALE_MAX);
                        }
                        if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Checkbox("Invert Camera Mouse X", &invert_camera_mouse_x);
                            ImGui::Checkbox("Invert Camera Mouse Y", &invert_camera_mouse_y);
                        }
                        if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            // position editor
                            {
                                float position[3]{ point_light.position.x, point_light.position.y, point_light.position.z };
                                ImGui::DragFloat3("Position", position, 0.01f);
                                point_light.position = { position[0], position[1], position[2] };
                            }
                            // intensity editor
                            ImGui::DragFloat("Intensity", &point_light.intenisty, 0.1f, POINT_LIGHT_MIN_INTENSITY, POINT_LIGHT_MAX_INTENSITY);
                            // color editor
                            {
                                float color[3]{ point_light.color.x, point_light.color.y, point_light.color.z };
                                ImGui::ColorEdit3("Color", color);
                                point_light.color = { color[0], color[1], color[2] };
                            }
                        }
                        if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            for (std::size_t i{}; i < objects.size(); i++)
                            {
                                if (ImGui::TreeNode(objects[i].name.c_str()))
                                {
                                    // position editor
                                    {
                                        float position[3]{ objects[i].position.x, objects[i].position.y, objects[i].position.z };
                                        ImGui::DragFloat3("Position", position, 0.01f);
                                        objects[i].position = { position[0], position[1], position[2] };
                                    }
                                    // rotation editor
                                    {
                                        float rotation[3]{ objects[i].rotation.x, objects[i].rotation.y, objects[i].rotation.z };
                                        ImGui::DragFloat3("Rotation", rotation, 0.1f, 0.0f, 360.0f);
                                        objects[i].rotation = { rotation[0], rotation[1], rotation[2] };
                                    }
                                    // scaling editor
                                    {
                                        float scaling[3]{ objects[i].scaling.x, objects[i].scaling.y, objects[i].scaling.z };
                                        ImGui::DragFloat3("Scaling", scaling, 0.01f);
                                        objects[i].scaling = { scaling[0], scaling[1], scaling[2] };
                                    }
                                    // albedo editor
                                    {
                                        float color[3]{ objects[i].albedo.x, objects[i].albedo.y, objects[i].albedo.z };
                                        ImGui::ColorEdit3("Albedo", color);
                                        objects[i].albedo = { color[0], color[1], color[2] };
                                    }

                                    ImGui::TreePop();
                                }
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
    // TODO: this should be removed
    {
        int renders{};

        constexpr int N{ 10 };
        constexpr float rho{ 0.25 }; // mean reflectivity
        constexpr int M{ static_cast<int>((1.0 / (1 - rho)) * N) };

        double w{}, start{};
        int end{}, reflections{};

        start = end = N;

        while (end > 0)
        {
            start *= rho;

            std::println("start: {} - end: {}", static_cast<int>(start), end);
            for (int i{ static_cast<int>(start) }; i < end; i++)
            {
                w = N;

                for (int j{}; j <= reflections; j++)
                {
                    std::println("render scene - particle={}, j={}, L={}/{}", i, j, N, std::floor(w));
                    w *= rho;

                    renders++;
                }
            }

            reflections++;
            end = static_cast<int>(start);
        }
        std::println("total renders: {} - M: {}", renders, M);
    }

    try
    {
        Entry();
    }
    catch (const Error& e)
    {
        std::println("{}", e.what());
    }

    return 0;
}

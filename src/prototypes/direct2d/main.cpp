
#include <windows.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <iostream>
#include <wincodec.h>
#include <wrl/client.h>



#pragma comment(lib, "d2d1.lib")

ID2D1Factory* factory = nullptr;
ID2D1HwndRenderTarget* renderTarget = nullptr;
ID2D1Bitmap* catBitmap = nullptr;
Microsoft::WRL::ComPtr<ID2D1Bitmap> compositionCatBitmap;

bool CreateRenderTarget(HWND window)
{
    RECT bounds;
    GetClientRect(window, &bounds);

    HRESULT result = factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(
            window,
            D2D1::SizeU(
                bounds.right - bounds.left,
                bounds.bottom - bounds.top
            )
        ),
        &renderTarget
    );

    return SUCCEEDED(result);
}

Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory;
// Direct3D
Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;

// DXGI
Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2dTargetBitmap;

// Direct2D 1.1
Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext;

// DirectComposition
Microsoft::WRL::ComPtr<IDCompositionDevice> compositionDevice;
Microsoft::WRL::ComPtr<IDCompositionTarget> compositionTarget;
Microsoft::WRL::ComPtr<IDCompositionVisual> compositionVisual;

bool RefreshBackBufferTarget()
{
    // Detach the old target before releasing its resources
    d2dContext->SetTarget(nullptr);

    d2dTargetBitmap.Reset();
    backBuffer.Reset();

    // Get the current swap-chain back buffer
    HRESULT result = swapChain->GetBuffer(
        0,
        IID_PPV_ARGS(&backBuffer)
    );

    if (FAILED(result))
    {
        std::cerr << "Failed to retrieve the current back buffer.\n";
        return false;
    }

    // Describe the new Direct2D target
    D2D1_BITMAP_PROPERTIES1 bitmapProperties =
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED
            ),
            96.0f,
            96.0f
        );

    // Create a new Direct2D bitmap from the back buffer
    result = d2dContext->CreateBitmapFromDxgiSurface(
        backBuffer.Get(),
        &bitmapProperties,
        &d2dTargetBitmap
    );

    if (FAILED(result))
    {
        std::cerr << "Failed to create the new Direct2D target.\n";
        std::cerr << "HRESULT: 0x"
                  << std::hex
                  << static_cast<unsigned long>(result)
                  << std::dec << "\n";

        return false;
    }

    // Attach the new target
    d2dContext->SetTarget(d2dTargetBitmap.Get());

    return true;
}

ID2D1Bitmap* LoadBitmapFromFile(
    ID2D1RenderTarget* target,
    const wchar_t* filename
)
{
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;

    HRESULT result = wicFactory->CreateDecoderFromFilename(
        filename,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );

    if (FAILED(result))
        return nullptr;

    result = decoder->GetFrame(0, &frame);

    if (FAILED(result))
        return nullptr;

    result = wicFactory->CreateFormatConverter(&converter);

    if (FAILED(result))
        return nullptr;

    result = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );

    if (FAILED(result))
        return nullptr;

    ID2D1Bitmap* bitmap = nullptr;

    result = target->CreateBitmapFromWicBitmap(
        converter.Get(),
        nullptr,
        &bitmap
    );

    if (FAILED(result))
        return nullptr;

    return bitmap;
}

LRESULT CALLBACK WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message)
    {
    case WM_SIZE:
        if (renderTarget)
        {
            HRESULT result = renderTarget->Resize(
                D2D1::SizeU(
                    LOWORD(lParam),
                    HIWORD(lParam)
                )
            );

            if (result == D2DERR_RECREATE_TARGET)
            {
                renderTarget->Release();
                renderTarget = nullptr;
            }
        }
        InvalidateRect(window, nullptr, FALSE);
        return 0;

case WM_PAINT:
{
    PAINTSTRUCT paint;
    BeginPaint(window, &paint);

    if (d2dContext && d2dTargetBitmap && compositionCatBitmap)
    {
        d2dContext->BeginDraw();

        // Clear the surface to fully transparent
        d2dContext->Clear(
            D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)
        );

        D2D1_SIZE_F imageSize = compositionCatBitmap->GetSize();
        D2D1_SIZE_F targetSize = d2dContext->GetSize();

        // Calculate the scale while preserving proportions
        float scaleX = targetSize.width / imageSize.width;
        float scaleY = targetSize.height / imageSize.height;
        float scale = (scaleX < scaleY) ? scaleX : scaleY;

        // Calculate the image dimensions
        float width = imageSize.width * scale;
        float height = imageSize.height * scale;

        // Center the image
        D2D1_RECT_F destination = D2D1::RectF(
            (targetSize.width - width) / 2.0f,
            (targetSize.height - height) / 2.0f,
            (targetSize.width + width) / 2.0f,
            (targetSize.height + height) / 2.0f
        );

        // Draw the cat using the new renderer
        d2dContext->DrawBitmap(
            compositionCatBitmap.Get(),
            &destination,
            1.0f,
            D2D1_INTERPOLATION_MODE_LINEAR
        );

        // Finish the Direct2D drawing operation
        HRESULT drawResult = d2dContext->EndDraw();

        if (FAILED(drawResult))
        {
            std::cerr << "Direct2D drawing failed. HRESULT: 0x"
                      << std::hex
                      << static_cast<unsigned long>(drawResult)
                      << std::dec << "\n";
        }
        else
        {
            HRESULT presentResult = swapChain->Present(1, 0);
        
            if (FAILED(presentResult))
            {
                std::cerr << "Swap chain presentation failed. HRESULT: 0x"
                          << std::hex
                          << static_cast<unsigned long>(presentResult)
                          << std::dec << "\n";
            }
        }
    }

    EndPaint(window, &paint);
    return 0;
}

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(window, message, wParam, lParam);
}


int main()
{
    // ==========================================
    // 1. Initialize COM
    // ==========================================

    HRESULT result = CoInitializeEx(
        nullptr,
        COINIT_APARTMENTTHREADED
    );

    if (FAILED(result))
    {
        std::cerr << "COM initialization failed.\n";
        return 1;
    }

    // ==========================================
    // 2. Create Direct3D device
    // ==========================================

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3D_FEATURE_LEVEL featureLevel{};

    HRESULT d3dResult = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &d3dDevice,
        &featureLevel,
        &d3dContext
    );

    if (FAILED(d3dResult))
    {
        std::cerr << "Direct3D device creation failed.\n";
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 3. Retrieve DXGI device and factory
    // ==========================================

    d3dResult = d3dDevice.As(&dxgiDevice);

    if (FAILED(d3dResult))
    {
        std::cerr << "Failed to get DXGI device.\n";
        CoUninitialize();
        return 1;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;

    d3dResult = dxgiDevice->GetAdapter(&dxgiAdapter);

    if (FAILED(d3dResult))
    {
        std::cerr << "Failed to get DXGI adapter.\n";
        CoUninitialize();
        return 1;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;

    d3dResult = dxgiAdapter->GetParent(
        IID_PPV_ARGS(&dxgiFactory)
    );

    if (FAILED(d3dResult))
    {
        std::cerr << "Failed to get DXGI factory.\n";
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 4. Create Direct2D 1.1 factory and device
    // ==========================================

    D2D1_FACTORY_OPTIONS factoryOptions{};

    HRESULT factoryResult = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &factoryOptions,
        reinterpret_cast<void**>(d2dFactory.GetAddressOf())
    );

    if (FAILED(factoryResult))
    {
        std::cerr << "Direct2D 1.1 factory creation failed.\n";
        CoUninitialize();
        return 1;
    }

    d3dResult = d2dFactory->CreateDevice(
        dxgiDevice.Get(),
        &d2dDevice
    );

    if (FAILED(d3dResult))
    {
        std::cerr << "Direct2D device creation failed.\n";
        CoUninitialize();
        return 1;
    }

    // Create the Direct2D device context
    d3dResult = d2dDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &d2dContext
    );

    if (FAILED(d3dResult))
    {
        std::cerr << "Direct2D device context creation failed.\n";
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 5. Create DirectComposition device
    // ==========================================

    d3dResult = DCompositionCreateDevice(
        dxgiDevice.Get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(compositionDevice.GetAddressOf())
    );

    if (FAILED(d3dResult))
    {
        std::cerr << "DirectComposition device creation failed.\n";
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 6. Initialize WIC
    // ==========================================

    result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wicFactory)
    );

    if (FAILED(result))
    {
        std::cerr << "WIC factory creation failed.\n";
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 7. Initialize the original Direct2D renderer
    // ==========================================

    result = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        &factory
    );

    if (FAILED(result))
    {
        std::cerr << "Original Direct2D factory creation failed.\n";
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 8. Register and create the window
    // ==========================================

    HINSTANCE instance = GetModuleHandle(nullptr);

    WNDCLASSW windowClass{};

    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"Direct2DPrototype";

    if (!RegisterClassW(&windowClass))
    {
        std::cerr << "Window class registration failed.\n";
        factory->Release();
        CoUninitialize();
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"Desktop Companion - Direct2D Prototype",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!window)
    {
        std::cerr << "Window creation failed.\n";
        factory->Release();
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 9. Create composition swap chain
    // ==========================================

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

    RECT clientRect{};
    GetClientRect(window, &clientRect);

    swapChainDesc.Width = clientRect.right - clientRect.left;
    swapChainDesc.Height = clientRect.bottom - clientRect.top;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    swapChainDesc.Flags = 0;

    d3dResult = dxgiFactory->CreateSwapChainForComposition(
        d3dDevice.Get(),
        &swapChainDesc,
        nullptr,
        &swapChain
    );

    if (FAILED(d3dResult))
    {
        std::cerr << "Composition swap chain creation failed.\n";
        DestroyWindow(window);
        factory->Release();
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 9.1. Get the swap chain's back buffer
    // ==========================================

    d3dResult = swapChain->GetBuffer(
        0,
        IID_PPV_ARGS(&backBuffer)
    );

    if (FAILED(d3dResult))
    {
        std::cerr << "Failed to get swap chain back buffer.\n";
        DestroyWindow(window);
        factory->Release();
        CoUninitialize();
        return 1;
    }

    // ==========================================
    // 9.2. Create Direct2D target bitmap
    // ==========================================

    D2D1_BITMAP_PROPERTIES1 bitmapProperties =
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET |
            D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED
            ),
            96.0f,
            96.0f
        );

    d3dResult = d2dContext->CreateBitmapFromDxgiSurface(
        backBuffer.Get(),
        &bitmapProperties,
        &d2dTargetBitmap
    );

    if (FAILED(d3dResult))
    {
        std::cerr << "Failed to create Direct2D target bitmap.\n";
        std::cerr << "HRESULT: 0x"
                  << std::hex
                  << static_cast<unsigned long>(d3dResult)
                  << std::dec << "\n";

        DestroyWindow(window);
        factory->Release();
        CoUninitialize();
        return 1;
    }

    d2dContext->SetTarget(d2dTargetBitmap.Get());

    // Create a DirectComposition target for our window
    HRESULT dcompResult = compositionDevice->CreateTargetForHwnd(
        window,
        TRUE,
        &compositionTarget
    );

    if (FAILED(dcompResult))
    {
        std::cout << "Failed to create composition target\n";
        return -1;
    }

    // Create a visual
    dcompResult = compositionDevice->CreateVisual(&compositionVisual);

    if (FAILED(dcompResult))
    {
        std::cout << "Failed to create composition visual\n";
        return -1;
    }

    // Attach the swap chain to the visual
    dcompResult = compositionVisual->SetContent(swapChain.Get());

    if (FAILED(dcompResult))
    {
        std::cout << "Failed to attach swap chain to visual\n";
        return -1;
    }

    // Set the visual as the root of our window's composition tree
    dcompResult = compositionTarget->SetRoot(compositionVisual.Get());

    if (FAILED(dcompResult))
    {
        std::cout << "Failed to set composition root\n";
        return -1;
    }

    // Apply the composition changes
    dcompResult = compositionDevice->Commit();

    if (FAILED(dcompResult))
    {
        std::cout << "Failed to commit composition\n";
        return -1;
    }

    // ==========================================
    // 10. Keep the original renderer working
    // ==========================================

    if (!CreateRenderTarget(window))
    {
        std::cerr << "Original render target creation failed.\n";
        DestroyWindow(window);
        factory->Release();
        CoUninitialize();
        return 1;
    }

    catBitmap = LoadBitmapFromFile(
        renderTarget,
        L"assets/test.png"
    );

    if (!catBitmap)
    {
        std::cerr << "Failed to load cat image.\n";

        renderTarget->Release();
        renderTarget = nullptr;

        DestroyWindow(window);
        factory->Release();
        CoUninitialize();

        return 1;
    }

    compositionCatBitmap = LoadBitmapFromFile(
        d2dContext.Get(),
        L"assets/test.png"
    );

    if (!compositionCatBitmap)
    {
        std::cerr << "Failed to load composition cat bitmap.\n";
        return 1;
    }
    // ==========================================
    // 11. Display the original prototype
    // ==========================================

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};

    while (GetMessage(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    // ==========================================
    // 12. Cleanup
    // ==========================================

    if (catBitmap)
    {
        catBitmap->Release();
        catBitmap = nullptr;
    }

    if (renderTarget)
    {
        renderTarget->Release();
        renderTarget = nullptr;
    }

    factory->Release();

    CoUninitialize();

    return 0;
}

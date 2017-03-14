// CGS HW Project A "Line Land".
// Author: L.Norri CD CGS, FullSail University

// Introduction:
// Welcome to the hardware project of the Computer Graphics Systems class.
// This assignment is fully guided but still requires significant effort on your part. 
// Future classes will demand the habits & foundation you develop right now!  
// It is CRITICAL that you follow each and every step. ESPECIALLY THE READING!!!

// TO BEGIN: Open the word document that acompanies this project and start from the top.

//************************************************************
//************ INCLUDES & DEFINES ****************************
//************************************************************

#include <iostream>
#include <ctime>
#include "XTime.h"

using namespace std;

#include <d3d11.h>
#pragma comment(lib,"d3d11.lib")

#include <DirectXMath.h>
using namespace DirectX;
#include "Trivial_PS.csh"
#include "Trivial_VS.csh"

#define BACKBUFFER_WIDTH	500
#define BACKBUFFER_HEIGHT	500


#include "../FBX/FBX.h"

//************************************************************
//************ SIMPLE WINDOWS APP CLASS **********************
//************************************************************

class DEMO_APP
{	
public:
	struct SIMPLE_VERTEX { XMFLOAT4 xyzw;/* XMFLOAT4 color; */};
	struct VRAM { XMFLOAT4X4 camView; XMFLOAT4X4 camProj; XMFLOAT4X4 modelPos; };
		XTime Time;

	DEMO_APP(HINSTANCE hinst, WNDPROC proc);
	bool Run();
	bool ShutDown();
private:
	HINSTANCE						application;
	WNDPROC							appWndProc;
	HWND							window;

	ID3D11Device *device = NULL;
	IDXGISwapChain *swapchain = NULL;
	ID3D11DeviceContext * context = NULL;
	ID3D11RenderTargetView * rtv = NULL;
	D3D11_VIEWPORT viewport;
	ID3D11Texture2D * depthStencil = nullptr;
	ID3D11DepthStencilView * depthStencleView = nullptr;

	int vertSize = 0;
	ID3D11Buffer * vertbuffer = NULL;
	ID3D11Buffer * indexbuffer = NULL;
	unsigned int circlevertcount = 0;
	ID3D11InputLayout * layout = NULL;
		struct SEND_TO_VRAM { XMFLOAT4 color; XMFLOAT2 offset; XMFLOAT2 padding; };

	
	ID3D11VertexShader*     vertexshader = NULL;
	ID3D11PixelShader*      pixelshader = NULL;


	ID3D11Buffer * constBuffer = NULL;

	VRAM send_to_ram;
};

//************************************************************
//************ CREATION OF OBJECTS & RESOURCES ***************
//************************************************************

DEMO_APP::DEMO_APP(HINSTANCE hinst, WNDPROC proc)
{
	// ****************** BEGIN WARNING ***********************// 
	// WINDOWS CODE, I DON'T TEACH THIS YOU MUST KNOW IT ALREADY! 
	application = hinst; 
	appWndProc = proc; 

	WNDCLASSEX  wndClass;
    ZeroMemory( &wndClass, sizeof( wndClass ) );
    wndClass.cbSize         = sizeof( WNDCLASSEX );             
    wndClass.lpfnWndProc    = appWndProc;						
    wndClass.lpszClassName  = L"DirectXApplication";            
	wndClass.hInstance      = application;		               
    wndClass.hCursor        = LoadCursor( NULL, IDC_ARROW );    
    wndClass.hbrBackground  = ( HBRUSH )( COLOR_WINDOWFRAME ); 
	//wndClass.hIcon			= LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_FSICON));
    RegisterClassEx( &wndClass );

	RECT window_size = { 0, 0, BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT };
	AdjustWindowRect(&window_size, WS_OVERLAPPEDWINDOW, false);

	window = CreateWindow(	L"DirectXApplication", L"CGS Hardware Project",	WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME|WS_MAXIMIZEBOX), 
							CW_USEDEFAULT, CW_USEDEFAULT, window_size.right-window_size.left, window_size.bottom-window_size.top,					
							NULL, NULL,	application, this );												

    ShowWindow( window, SW_SHOW );
	//********************* END WARNING ************************//

	//camera data
	static const XMVECTORF32 eye = { 0.0f, 0.0f, -5.0f, 0.0f };
	static const XMVECTORF32 at = { 1.0f, 0.0f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMStoreFloat4x4(&send_to_ram.camView,XMMatrixLookAtLH(eye, at, up));
	//XMStoreFloat4x4(&send_to_ram.camView, XMMatrixInverse(NULL, XMMatrixLookAtLH(eye, at, up)));

	float aspectRatio = BACKBUFFER_WIDTH / BACKBUFFER_HEIGHT;
	float fovAngleY = 70.0f * XM_PI / 180.0f;
	if (aspectRatio < 1.0f)
		fovAngleY *= 2.0f;
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovLH(
		fovAngleY,
		aspectRatio,
		0.01f,
		1000000.0f
	);
	XMStoreFloat4x4(&send_to_ram.camProj, perspectiveMatrix);

	//model data
	XMStoreFloat4x4(&send_to_ram.modelPos, XMMatrixIdentity()); 



	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = window;
	scd.SampleDesc.Count = 1;
	scd.Windowed = TRUE;

	D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &scd, &swapchain, &device, 0, &context);
	ID3D11Texture2D * BackBuffer;
	ZeroMemory(&BackBuffer, sizeof(ID3D11Texture2D));
	swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&BackBuffer);
	device->CreateRenderTargetView(BackBuffer, NULL, &rtv);
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;
	viewport.Height = BACKBUFFER_HEIGHT;
	viewport.Width = BACKBUFFER_WIDTH;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;

	//depth stencl
	CD3D11_TEXTURE2D_DESC depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		lround(BACKBUFFER_WIDTH),
		lround(BACKBUFFER_HEIGHT),
		1,
		1,
		D3D11_BIND_DEPTH_STENCIL
	);
	device->CreateTexture2D(
		&depthStencilDesc,
		nullptr,
		&depthStencil
	);
	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	device->CreateDepthStencilView(
		depthStencil,
		&depthStencilViewDesc,
		&depthStencleView
	);
	/*SIMPLE_VERTEX circle[366]{};
	for (size_t i = 0; i < 366; i++)
	{
		XMFLOAT4 pos = XMFLOAT4((sin((i * (3.14f / 180)))), (cos((i * (3.14f / 180)))),0.0f,1.0f);
		circle[i].xyzw = pos;
	}*/

	SIMPLE_VERTEX circle[3]{};
	circle[0].xyzw = XMFLOAT4(0.0f, 0.0f, 0, 0);
	circle[1].xyzw = XMFLOAT4(0.0f, 0.2f, 0, 0);
	circle[2].xyzw = XMFLOAT4(0.2f, 0.0f, 0, 0);



	D3D11_BUFFER_DESC bufferdescription;
	ZeroMemory(&bufferdescription, sizeof(bufferdescription));
	bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	bufferdescription.ByteWidth = sizeof(SIMPLE_VERTEX) * 3;
	bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferdescription.CPUAccessFlags = NULL;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = circle;
	device->CreateBuffer(&bufferdescription, &InitData, &vertbuffer);



	device->CreateVertexShader(Trivial_VS, sizeof(Trivial_VS), NULL, &vertexshader);
	device->CreatePixelShader(Trivial_PS, sizeof(Trivial_PS), NULL, &pixelshader);
	D3D11_INPUT_ELEMENT_DESC vertlayout[] =
	{
		"POSITION", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,0 /*,{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }*/
	};
	UINT numElements = ARRAYSIZE(vertlayout);
	device->CreateInputLayout(vertlayout, numElements, Trivial_VS, sizeof(Trivial_VS), &layout);

	CD3D11_BUFFER_DESC constBufferDesc(sizeof(VRAM), D3D11_BIND_CONSTANT_BUFFER);
	device->CreateBuffer(&constBufferDesc, nullptr, &constBuffer);

}

//************************************************************
//************ EXECUTION *************************************
//************************************************************

bool DEMO_APP::Run()
{
	function();
	context->OMSetRenderTargets(1, &rtv, NULL);
	context->RSSetViewports(1, &viewport);
	float color[4]{ 0.0f, 0.0f, 1.0f, 0.0f };
	context->ClearRenderTargetView(rtv, color);
	//context->ClearDepthStencilView(, )
	context->UpdateSubresource(
		constBuffer,
		0,
		nullptr,
		&send_to_ram,
		0,
		0
	);
	UINT stride = sizeof(SIMPLE_VERTEX);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &vertbuffer, &stride, &offset);

	context->VSSetShader(vertexshader, NULL, NULL);
	context->PSSetShader(pixelshader, NULL, NULL);

	context->IASetInputLayout(layout);
	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetConstantBuffers(
		0,
		1,
		&constBuffer
	);
	context->Draw(3, 0);

	
	swapchain->Present(0, 0);
	return true; 
}

//************************************************************
//************ DESTRUCTION ***********************************
//************************************************************

bool DEMO_APP::ShutDown()
{
	device->Release();
	context->Release();
	rtv->Release();
	swapchain->Release();
	UnregisterClass( L"DirectXApplication", application ); 
	return true;
}

//************************************************************
//************ WINDOWS RELATED *******************************
//************************************************************

// ****************** BEGIN WARNING ***********************// 
// WINDOWS CODE, I DON'T TEACH THIS YOU MUST KNOW IT ALREADY!
	
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine,	int nCmdShow );						   
LRESULT CALLBACK WndProc(HWND hWnd,	UINT message, WPARAM wparam, LPARAM lparam );		
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE, LPTSTR, int )
{
	srand(unsigned int(time(0)));
	DEMO_APP myApp(hInstance,(WNDPROC)WndProc);	
    MSG msg; ZeroMemory( &msg, sizeof( msg ) );
    while ( msg.message != WM_QUIT && myApp.Run() )
    {	
	    if ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        { 
            TranslateMessage( &msg );
            DispatchMessage( &msg ); 
        }
    }
	myApp.ShutDown(); 
	return 0; 
}
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    if(GetAsyncKeyState(VK_ESCAPE))
		message = WM_DESTROY;
    switch ( message )
    {
        case ( WM_DESTROY ): { PostQuitMessage( 0 ); }
        break;
    }
    return DefWindowProc( hWnd, message, wParam, lParam );
}
//********************* END WARNING ************************//
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
#include <windowsx.h>
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

#pragma region mouse and keyboard imput class
class Imput
{
public:
	Imput();
	~Imput();
	char buttons[256]{};
	float x = 0.0f;
	float y = 0.0f;
	float prevX = 0.0f;
	float prevY = 0.0f;
	float diffx = 0.0f;
	float diffy = 0.0f;
	bool mouse_move = false;
	bool left_click = false;
	bool has_left_window = false;
};

Imput::Imput()
{
}

Imput::~Imput()
{
}

Imput imput;
#pragma endregion

//************************************************************
//************ SIMPLE WINDOWS APP CLASS **********************
//************************************************************

class DEMO_APP
{
public:
	struct SIMPLE_VERTEX { XMFLOAT4 xyzw; XMFLOAT4 color;  };
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
	ID3D11DepthStencilView * depthStencilView = nullptr;
	ID3D11RasterizerState * wireFrameRasterizerState = nullptr;
	ID3D11RasterizerState * SolidRasterizerState = nullptr;

	//model 
	ID3D11Buffer * modelvertbuffer = NULL;
	ID3D11Buffer * modelindexbuffer = NULL;
	unsigned int modelindexCount = 0;

	//ground plane
	ID3D11Buffer * groundvertbuffer = NULL;
	ID3D11Buffer * groundindexbuffer = NULL;
	unsigned int groundindexCount = 0;

	ID3D11InputLayout * layout = NULL;
	struct SEND_TO_VRAM { XMFLOAT4 color; XMFLOAT2 offset; XMFLOAT2 padding; };


	ID3D11VertexShader*     vertexshader = NULL;
	ID3D11PixelShader*      pixelshader = NULL;


	ID3D11Buffer * constBuffer = NULL;
	VRAM send_to_ram;

	XMFLOAT4X4 camera;


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
	ZeroMemory(&wndClass, sizeof(wndClass));
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.lpfnWndProc = appWndProc;
	wndClass.lpszClassName = L"DirectXApplication";
	wndClass.hInstance = application;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOWFRAME);
	//wndClass.hIcon			= LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_FSICON));
	RegisterClassEx(&wndClass);

	RECT window_size = { 0, 0, BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT };
	AdjustWindowRect(&window_size, WS_OVERLAPPEDWINDOW, false);

	window = CreateWindow(L"DirectXApplication", L"Antonio Arbona", WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
		CW_USEDEFAULT, CW_USEDEFAULT, window_size.right - window_size.left, window_size.bottom - window_size.top,
		NULL, NULL, application, this);

	ShowWindow(window, SW_SHOW);
	//********************* END WARNING ************************//

	//XTime
	Time.Restart();

	//camera data
	static const XMVECTORF32 eye = { 0.0f, 350.0f, -300.0f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(XMMatrixLookAtLH(eye, at, up)));
	XMStoreFloat4x4(&camera, XMMatrixLookAtLH(eye, at, up));

	float aspectRatio = BACKBUFFER_WIDTH / BACKBUFFER_HEIGHT;
	float fovAngleY = 70.0f * XM_PI / 180.0f;
	if (aspectRatio < 1.0f)
		fovAngleY *= 2.0f;
	fovAngleY = XMConvertToDegrees(fovAngleY);
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovLH(
		fovAngleY,
		aspectRatio,
		0.01f,
		100000.0f
	);
	XMStoreFloat4x4(&send_to_ram.camProj, XMMatrixTranspose(perspectiveMatrix));
	//model data
	XMStoreFloat4x4(&send_to_ram.modelPos, XMMatrixTranspose(XMMatrixIdentity()));



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

#pragma region	depth stencl
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
		&depthStencilView
	);
#pragma endregion

#pragma region raster state wireframe & culling
	D3D11_RASTERIZER_DESC wireFrameDesc;
	ZeroMemory(&wireFrameDesc, sizeof(D3D11_RASTERIZER_DESC));
	wireFrameDesc.FillMode = D3D11_FILL_WIREFRAME;
	wireFrameDesc.CullMode = D3D11_CULL_NONE;
	wireFrameDesc.DepthClipEnable = true;
	device->CreateRasterizerState(&wireFrameDesc, &wireFrameRasterizerState);
	ZeroMemory(&wireFrameDesc, sizeof(D3D11_RASTERIZER_DESC));
	wireFrameDesc.FillMode = D3D11_FILL_SOLID;
	wireFrameDesc.CullMode = D3D11_CULL_BACK;
	wireFrameDesc.DepthClipEnable = true;
	device->CreateRasterizerState(&wireFrameDesc, &SolidRasterizerState);
#pragma endregion

#pragma region fbx loading
	char file[]{ "Teddy_Idle.fbx" };
	char mesh[]{ "mesh.bin" };
	char bone[]{ "bone.bin" };
	char animation[]{ "animation.bin" };

	function(file, mesh, bone, animation);

	unsigned int triCount = 0;
	vector<unsigned int> triIndices;
	vector<BlendingVertex> verts;
	Skeleton * mSkeleton = new Skeleton();
	vector<Bone> bind_pose;
	functionality(mesh, bone, animation, triCount, triIndices, verts, mSkeleton, bind_pose);
#pragma endregion

#pragma region model buffers

	//teddy//
	SIMPLE_VERTEX * model;
	model = new SIMPLE_VERTEX[verts.size()];
	float modelColor[4]{ 0.0f, 0.0f, 1.0f, 0.0f };
	for (size_t i = 0; i < verts.size(); i++)
	{
		model[i].xyzw.x = verts[i].mPosition.x;
		model[i].xyzw.y = verts[i].mPosition.y;
		model[i].xyzw.z = verts[i].mPosition.z;
		model[i].xyzw.w = 1.0f;
		model[i].color.x = modelColor[0];
		model[i].color.y = modelColor[1];
		model[i].color.z = modelColor[2];
		model[i].color.w = modelColor[3];
	}

	D3D11_BUFFER_DESC vertbufferdescription;
	ZeroMemory(&vertbufferdescription, sizeof(D3D11_BUFFER_DESC));
	vertbufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	vertbufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * verts.size());
	vertbufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertbufferdescription.CPUAccessFlags = NULL;
	vertbufferdescription.MiscFlags = NULL;
	vertbufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	D3D11_SUBRESOURCE_DATA vertInitData;
	ZeroMemory(&vertInitData, sizeof(vertInitData));
	vertInitData.pSysMem = model;
	device->CreateBuffer(&vertbufferdescription, &vertInitData, &modelvertbuffer);
	
	modelindexCount = (unsigned int)triIndices.size();
	unsigned int * modelIndex;
	modelIndex = new unsigned int[modelindexCount];
	for (size_t i = 0; i < modelindexCount; i++)
		modelIndex[i] = triIndices[i];

	D3D11_BUFFER_DESC indexbufferdescription;
	ZeroMemory(&indexbufferdescription, sizeof(D3D11_BUFFER_DESC));
	indexbufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	indexbufferdescription.ByteWidth = (UINT)(sizeof(unsigned int) * triIndices.size());
	indexbufferdescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexbufferdescription.CPUAccessFlags = NULL;
	indexbufferdescription.MiscFlags = NULL;
	indexbufferdescription.StructureByteStride = sizeof(unsigned int);
	D3D11_SUBRESOURCE_DATA indexInitData;
	ZeroMemory(&indexInitData, sizeof(indexInitData));
	indexInitData.pSysMem = modelIndex;
	device->CreateBuffer(&indexbufferdescription, &indexInitData, &modelindexbuffer);
	//end teddy//

	//ground plane//
	groundindexCount = 6;
	SIMPLE_VERTEX groundPlane[4]{};
	groundPlane[0].xyzw = XMFLOAT4(100.0f, 0.0f, 100.0f, 0);
	groundPlane[1].xyzw = XMFLOAT4(100.0f, 0.0f, -100.0f, 0);
	groundPlane[2].xyzw = XMFLOAT4(-100.0f, 0.0f, -100.0f, 0);
	groundPlane[3].xyzw = XMFLOAT4(-100.0f, 0.0f, 100.0f, 0);
	float groundColor[4]{ 1.0f, 1.0f, 0.0f, 0.0f };
	for (size_t i = 0; i < 4; i++)
	{
		groundPlane[i].color.x = groundColor[0];
		groundPlane[i].color.y = groundColor[1];
		groundPlane[i].color.z = groundColor[2];
		groundPlane[i].color.w = groundColor[3];
	}
	ZeroMemory(&vertbufferdescription, sizeof(D3D11_BUFFER_DESC));
	vertbufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	vertbufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * groundindexCount);
	vertbufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertbufferdescription.CPUAccessFlags = NULL;
	vertbufferdescription.MiscFlags = NULL;
	vertbufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	ZeroMemory(&vertInitData, sizeof(vertInitData));
	vertInitData.pSysMem = groundPlane;
	device->CreateBuffer(&vertbufferdescription, &vertInitData, &groundvertbuffer);

	unsigned int groundPlaneindex[6]{ 0,1,3,1,2,3 };
	
	ZeroMemory(&indexbufferdescription, sizeof(D3D11_BUFFER_DESC));
	indexbufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	indexbufferdescription.ByteWidth = (UINT)(sizeof(unsigned int) * groundindexCount);
	indexbufferdescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexbufferdescription.CPUAccessFlags = NULL;
	indexbufferdescription.MiscFlags = NULL;
	indexbufferdescription.StructureByteStride = sizeof(unsigned int);
	ZeroMemory(&indexInitData, sizeof(indexInitData));
	indexInitData.pSysMem = groundPlaneindex;
	device->CreateBuffer(&indexbufferdescription, &indexInitData, &groundindexbuffer);
	//end ground plane//
#pragma endregion

	device->CreateVertexShader(Trivial_VS, sizeof(Trivial_VS), NULL, &vertexshader);
	device->CreatePixelShader(Trivial_PS, sizeof(Trivial_PS), NULL, &pixelshader);

	D3D11_INPUT_ELEMENT_DESC vertlayout[] =
	{
		"POSITION", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,0 ,{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
	Time.Signal();
	if (imput.mouse_move)
		if (imput.left_click)
		{
			XMMATRIX newcamera = XMLoadFloat4x4(&camera);
			XMVECTOR pos = newcamera.r[3];
			newcamera.r[3] = XMLoadFloat4(&XMFLOAT4(0, 0, 0, 1));
			newcamera = XMMatrixRotationX(imput.diffy * (float)Time.Delta()) * newcamera * XMMatrixRotationY(imput.diffx * (float)Time.Delta());
			newcamera.r[3] = pos;
			XMStoreFloat4x4(&camera, newcamera);
			XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
		}
	imput.mouse_move = false;


	if (imput.buttons['W'])
	{
		XMMATRIX newcamera = XMLoadFloat4x4(&camera);
		XMVECTOR forward{ 0.0f,0.0f,5.0f,0.0f };
		newcamera.r[3] -= forward * (float)Time.Delta();
		XMStoreFloat4x4(&camera, newcamera);
		XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
	}
	if (imput.buttons['S'])
	{
		XMMATRIX newcamera = XMLoadFloat4x4(&camera);
		XMVECTOR forward{ 0.0f,0.0f,5.0f,0.0f };
		newcamera.r[3] += forward * (float)Time.Delta();
		XMStoreFloat4x4(&camera, newcamera);
		XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
	}
	if (imput.buttons['A'])
	{
		XMMATRIX newcamera = XMLoadFloat4x4(&camera);
		XMVECTOR Right{ 5.0f,0.0f,0.0f,0.0f };
		newcamera.r[3] += Right * (float)Time.Delta();
		XMStoreFloat4x4(&camera, newcamera);
		XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
	}
	if (imput.buttons['D'])
	{
		XMMATRIX newcamera = XMLoadFloat4x4(&camera);
		XMVECTOR Right{ 5.0f,0.0f,0.0f,0.0f };
		newcamera.r[3] -= Right *(float) Time.Delta();
		XMStoreFloat4x4(&camera, newcamera);
		XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
	}

	float color[4]{ 0.0f, 1.0f, 0.0f, 0.0f };
	context->OMSetRenderTargets(1, &rtv, depthStencilView);
	context->ClearRenderTargetView(rtv, color);
	context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->RSSetViewports(1, &viewport);
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
	context->IASetVertexBuffers(0, 1, &modelvertbuffer, &stride, &offset);
	context->IASetIndexBuffer(modelindexbuffer, DXGI_FORMAT_R32_UINT, offset);
	context->VSSetShader(vertexshader, NULL, NULL);
	context->PSSetShader(pixelshader, NULL, NULL);


	context->IASetInputLayout(layout);
	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetConstantBuffers(
		0,
		1,
		&constBuffer
	);
	context->RSSetState(wireFrameRasterizerState);
	context->DrawIndexed(modelindexCount, 0, 0);


	//draw ground ground 
	context->IASetVertexBuffers(0, 1, &groundvertbuffer, &stride, &offset);
	context->IASetIndexBuffer(groundindexbuffer, DXGI_FORMAT_R32_UINT, offset);
	context->RSSetState(SolidRasterizerState);
	context->DrawIndexed(groundindexCount, 0, 0);
	//end ground draw



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
	depthStencil->Release();
	depthStencilView->Release();
	layout->Release();
	vertexshader->Release();
	pixelshader->Release();
	constBuffer->Release();
	UnregisterClass(L"DirectXApplication", application);
	return true;
}

//************************************************************
//************ WINDOWS RELATED *******************************
//************************************************************

// ****************** BEGIN WARNING ***********************// 
// WINDOWS CODE, I DON'T TEACH THIS YOU MUST KNOW IT ALREADY!

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
	srand(unsigned int(time(0)));
	DEMO_APP myApp(hInstance, (WNDPROC)WndProc);
	MSG msg; ZeroMemory(&msg, sizeof(msg));

	tagTRACKMOUSEEVENT mouse_tracker;
	TrackMouseEvent(&mouse_tracker);

	while (msg.message != WM_QUIT && myApp.Run())
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg); //this calls the WndProc function
		}
	}
	myApp.ShutDown();
	return 0;
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (GetAsyncKeyState(VK_ESCAPE))
		message = WM_DESTROY;
	switch (message)
	{
	case (WM_DESTROY):
	{
		PostQuitMessage(0);
		break;
	}
	case (WM_KEYDOWN):
	{
		if (wParam)
		{
			imput.buttons[wParam] = true;
		}
		break;
	};
	case (WM_KEYUP):
	{
		if (wParam)
		{
			imput.buttons[wParam] = false;
		}
		break;
	};
	case (WM_LBUTTONDOWN):
	{
		imput.diffx = 0.0f;
		imput.diffy = 0.0f;
		imput.left_click = true;
		imput.mouse_move = true;
		break;
	};
	case (WM_LBUTTONUP):
	{
		imput.diffx = 0.0f;
		imput.diffy = 0.0f;
		imput.left_click = false;
		imput.mouse_move = false;
		break;
	};
	case (WM_MOUSEMOVE):
	{
		imput.mouse_move = true;
		imput.x = (float)GET_X_LPARAM(lParam);
		imput.y = (float)GET_Y_LPARAM(lParam);
		imput.diffx = imput.x - imput.prevX;
		imput.diffy = imput.y - imput.prevY;
		imput.prevX = imput.x;
		imput.prevY = imput.y;
		break;
	};
	case (WM_MOUSELEAVE):
	{
		imput.diffx = 0.0f;
		imput.diffy = 0.0f;
		imput.left_click = false;
		imput.mouse_move = false;
		imput.has_left_window = true;
		break;
	};
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}
//********************* END WARNING ************************//
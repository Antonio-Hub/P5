// CGS HW Project A "Line Land".
// Author: L.Norri CD CGS, FullSail University

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

#define BACKBUFFER_WIDTH	1000
#define BACKBUFFER_HEIGHT	1000

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
	bool bLeft = false;
	bool bRight = false;
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
	struct SIMPLE_VERTEX { XMFLOAT4 xyzw; XMFLOAT4 color;  XMFLOAT4 index; XMFLOAT4 weights; };
	struct VRAM { XMFLOAT4X4 camView; XMFLOAT4X4 camProj; XMFLOAT4X4 modelPos; };
	struct ANIMATION_VRAM { XMFLOAT4X4 InverseBindPose[128]; XMFLOAT4X4 RealTimePose[128]; };
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

	//model mesh
	vector<vert_pos_skinned> FileMesh;
	ID3D11Buffer * modelvertbuffer = NULL;
	ID3D11Buffer * modelindexbuffer = NULL;
	unsigned int modelindexCount = 0;
	unsigned int modelVertCount = 0;

	//model animation data
	anim_clip * IdleAnimationData = nullptr;
	unsigned int triCount = 0;
	vector<unsigned int> triIndices;
	vector<BlendingVertex> verts;
	vector<joint> bind_pose;
	SIMPLE_VERTEX * realTimeModel = nullptr;
	unsigned int keyFrameCount = 0;
	unsigned int boneCount = 0;
	double animLoopTime = 0.0f;
	double currAnimTime = 0.0f;
	int keyframeAnimIndex = 0;
	double twoKeyFrameTimes[2]{};

	//ground plane
	ID3D11Buffer * groundvertbuffer = NULL;
	ID3D11Buffer * groundindexbuffer = NULL;
	unsigned int groundindexCount = 0;

	//debug buffer
	ID3D11Buffer * pDebugPointBuffer = nullptr;
	unsigned int DebugPointCount = 128;
	SIMPLE_VERTEX DebugPointData[128]{};
	ID3D11Buffer * pDebugLineBuffer = nullptr;
	unsigned int DebugLineCount = 128;
	SIMPLE_VERTEX DebugLineData[128]{};

	ID3D11InputLayout * layout = NULL;

	ID3D11VertexShader * vertexshader = NULL;
	ID3D11PixelShader * pixelshader = NULL;

	ID3D11Buffer * constBuffer = NULL;
	ID3D11Buffer * modelAnimationConstBuffer = nullptr;

	VRAM send_to_ram{};
	ANIMATION_VRAM send_to_ram2{};

	XMFLOAT4X4 camera;

	UINT stride = sizeof(SIMPLE_VERTEX);
	UINT offset = 0;
	D3D11_MAPPED_SUBRESOURCE mapResource;
};

DEMO_APP::DEMO_APP(HINSTANCE hinst, WNDPROC proc)
{
	Time.Restart();
#pragma region wind
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
#pragma endregion

#pragma region swap chain device context
	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = window;
	scd.SampleDesc.Count = 1;
	scd.Windowed = TRUE;
	D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &scd, &swapchain, &device, 0, &context);
#pragma endregion

#pragma region render target view
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
#pragma endregion

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
	IdleAnimationData = new anim_clip();
	//matrix comes in from fbx like this
	// 1.0 0.0 0.0 0.0
	// 0.0 1.0 0.0 0.0
	// 0.0 0.0 1.0 0.0
	// x   y   z   w
	function(file, mesh, bone, animation, IdleAnimationData, FileMesh);
	functionality(mesh, bone, animation, triCount, triIndices, verts, bind_pose);

#pragma endregion

#pragma region model buffers
	D3D11_BUFFER_DESC bufferdescription;
	D3D11_SUBRESOURCE_DATA InitData;
	//teddy
	modelVertCount = (unsigned int)verts.size();
	realTimeModel = new SIMPLE_VERTEX[modelVertCount];
	float modelColor[4]{ 1.0f, 1.0f, 1.0f, 0.0f };
	for (size_t i = 0; i < modelVertCount; i++)
	{
		realTimeModel[i].xyzw.x = verts[i].mPosition.x;
		realTimeModel[i].xyzw.y = verts[i].mPosition.y;
		realTimeModel[i].xyzw.z = verts[i].mPosition.z;
		realTimeModel[i].xyzw.w = 1.0f;
		realTimeModel[i].color.x = modelColor[0];
		realTimeModel[i].color.y = modelColor[1];
		realTimeModel[i].color.z = modelColor[2];
		realTimeModel[i].color.w = modelColor[3];
		realTimeModel[i].index.x = (float)verts[i].mVertexBlendingInfos[0].mBlendingIndex;
		realTimeModel[i].index.y = (float)verts[i].mVertexBlendingInfos[1].mBlendingIndex;
		realTimeModel[i].index.z = (float)verts[i].mVertexBlendingInfos[2].mBlendingIndex;
		realTimeModel[i].index.w = (float)verts[i].mVertexBlendingInfos[3].mBlendingIndex;
		realTimeModel[i].weights.x = (float)verts[i].mVertexBlendingInfos[0].mBlendWeight;
		realTimeModel[i].weights.y = (float)verts[i].mVertexBlendingInfos[1].mBlendWeight;
		realTimeModel[i].weights.z = (float)verts[i].mVertexBlendingInfos[2].mBlendWeight;
		realTimeModel[i].weights.w = (float)verts[i].mVertexBlendingInfos[3].mBlendWeight;
	}



	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * modelVertCount);
	bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferdescription.CPUAccessFlags = NULL;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = realTimeModel;
	device->CreateBuffer(&bufferdescription, &InitData, &modelvertbuffer);


	modelindexCount = (unsigned int)triIndices.size();
	unsigned int * modelIndex;
	modelIndex = new unsigned int[modelindexCount];
	for (size_t i = 0; i < modelindexCount; i++)
		modelIndex[i] = triIndices[i];

	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	bufferdescription.ByteWidth = (UINT)(sizeof(unsigned int) * triIndices.size());
	bufferdescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferdescription.CPUAccessFlags = NULL;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(unsigned int);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = modelIndex;
	device->CreateBuffer(&bufferdescription, &InitData, &modelindexbuffer);

	//ground plane
	groundindexCount = 6;
	SIMPLE_VERTEX groundPlane[4]{};
	groundPlane[0].xyzw = XMFLOAT4(10.0f, 0.0f, 10.0f, 0);
	groundPlane[1].xyzw = XMFLOAT4(10.0f, 0.0f, -10.0f, 0);
	groundPlane[2].xyzw = XMFLOAT4(-10.0f, 0.0f, -10.0f, 0);
	groundPlane[3].xyzw = XMFLOAT4(-10.0f, 0.0f, 10.0f, 0);
	float groundColor[4]{ 0.0f, 1.0f, 0.0f, 0.0f };
	for (size_t i = 0; i < 4; i++)
	{
		groundPlane[i].color.x = groundColor[0];
		groundPlane[i].color.y = groundColor[1];
		groundPlane[i].color.z = groundColor[2];
		groundPlane[i].color.w = groundColor[3];
	}
	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * groundindexCount);
	bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferdescription.CPUAccessFlags = NULL;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = groundPlane;
	device->CreateBuffer(&bufferdescription, &InitData, &groundvertbuffer);

	unsigned int groundPlaneindex[6]{ 0,1,3,1,2,3 };

	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	bufferdescription.ByteWidth = (UINT)(sizeof(unsigned int) * groundindexCount);
	bufferdescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferdescription.CPUAccessFlags = NULL;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(unsigned int);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = groundPlaneindex;
	device->CreateBuffer(&bufferdescription, &InitData, &groundindexbuffer);
#pragma endregion

#pragma region debug render buffers
	//debug buffer
	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_DYNAMIC;
	bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * DebugPointCount);
	bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferdescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = DebugPointData;
	device->CreateBuffer(&bufferdescription, &InitData, &pDebugPointBuffer);

	int index = 0;
	for (size_t i = 0; i < bind_pose.size(); i++)
	{
		if (bind_pose[i].Parent_Index != -1)
		{
			DebugLineData[index].xyzw.x = bind_pose[bind_pose[i].Parent_Index].global_xform._41;
			DebugLineData[index].xyzw.y = bind_pose[bind_pose[i].Parent_Index].global_xform._42;
			DebugLineData[index].xyzw.z = bind_pose[bind_pose[i].Parent_Index].global_xform._43;
			DebugLineData[index].xyzw.w = bind_pose[bind_pose[i].Parent_Index].global_xform._44;
			index++;
			DebugLineData[index].xyzw.x = bind_pose[i].global_xform._41;
			DebugLineData[index].xyzw.y = bind_pose[i].global_xform._42;
			DebugLineData[index].xyzw.z = bind_pose[i].global_xform._43;
			DebugLineData[index].xyzw.w = bind_pose[i].global_xform._44;
			index++;

		}

	}
	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_DYNAMIC;
	bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * DebugLineCount);
	bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferdescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = DebugLineData;
	device->CreateBuffer(&bufferdescription, &InitData, &pDebugLineBuffer);

#pragma endregion

#pragma region const buffer data init
	//const buff
	//camera data
	//static const XMVECTORF32 eye = { 0.0f, 0.0f, 5.5f, 0.0f };
	//static const XMVECTORF32 eye = { 0.0f, 35.0f, 30.0f, 0.0f };
	//static const XMVECTORF32 eye = { 0.0f, 350.0f, 300.0f, 0.0f };
	static const XMVECTORF32 eye = { 0.0f, 350.0f, 300.0f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMStoreFloat4x4(&camera, XMMatrixInverse(NULL, XMMatrixLookAtLH(eye, at, up)));
	XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(XMMatrixLookAtLH(eye, at, up)));

	float aspectRatio = BACKBUFFER_WIDTH / BACKBUFFER_HEIGHT;
	float fovAngleY = 70.0f * XM_PI / 180.0f;
	if (aspectRatio < 1.0f)
		fovAngleY *= 2.0f;
	fovAngleY = XMConvertToDegrees(fovAngleY);
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, 0.01f, 100000.0f);
	XMStoreFloat4x4(&send_to_ram.camProj, XMMatrixTranspose(perspectiveMatrix));
	//model data
	XMStoreFloat4x4(&send_to_ram.modelPos, XMMatrixTranspose(XMMatrixIdentity()));
	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_DYNAMIC;
	bufferdescription.ByteWidth = sizeof(VRAM);
	bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferdescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(VRAM);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = &send_to_ram;
	device->CreateBuffer(&bufferdescription, &InitData, &constBuffer);


	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_DYNAMIC;
	bufferdescription.ByteWidth = sizeof(ANIMATION_VRAM);
	bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferdescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(ANIMATION_VRAM);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = &send_to_ram2;
	device->CreateBuffer(&bufferdescription, &InitData, &modelAnimationConstBuffer);
#pragma endregion

#pragma region shaders and imput layout
	device->CreateVertexShader(Trivial_VS, sizeof(Trivial_VS), NULL, &vertexshader);
	device->CreatePixelShader(Trivial_PS, sizeof(Trivial_PS), NULL, &pixelshader);

	D3D11_INPUT_ELEMENT_DESC vertlayout[] =
	{
		"POSITION", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0,
		"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0,
		"INDEX", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0,
		"WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0,
	};
	UINT numElements = ARRAYSIZE(vertlayout);
	device->CreateInputLayout(vertlayout, numElements, Trivial_VS, sizeof(Trivial_VS), &layout);
#pragma endregion

}

bool DEMO_APP::Run()
{
#pragma region time
	Time.Signal();
	currAnimTime += Time.SmoothDelta();
	if (currAnimTime > animLoopTime)
		currAnimTime = 0.0;
#pragma endregion

#pragma region update
	XMMATRIX newcamera = XMLoadFloat4x4(&camera);
	if (imput.buttons['W'])
		newcamera.r[3] = newcamera.r[3] + newcamera.r[2] * +(float)Time.Delta() * 100.0f;
	if (imput.buttons['S'])
		newcamera.r[3] = newcamera.r[3] + newcamera.r[2] * -(float)Time.Delta() * 100.0f;
	if (imput.buttons['A'])
		newcamera.r[3] = newcamera.r[3] + newcamera.r[0] * -(float)Time.Delta() * 100.0f;
	if (imput.buttons['D'])
		newcamera.r[3] = newcamera.r[3] + newcamera.r[0] * +(float)Time.Delta() * 100.0f;

	if (imput.mouse_move)
		if (imput.left_click)
		{
			XMVECTOR pos = newcamera.r[3];
			newcamera.r[3] = XMLoadFloat4(&XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
			newcamera =
				XMMatrixRotationX(imput.diffy * (float)Time.Delta() * 100.0f)
				* newcamera
				* XMMatrixRotationY(imput.diffx * (float)Time.Delta() * 100.0f);
			newcamera.r[3] = pos;
		}
	imput.mouse_move = false;
	XMStoreFloat4x4(&camera, newcamera);
	XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(XMMatrixInverse(0, newcamera)));

	if (imput.bLeft == false && imput.buttons[VK_LEFT])
	{
		currAnimTime -= 100.0 * Time.Delta();
		if (currAnimTime < 0)
			currAnimTime = IdleAnimationData->Duration;
		imput.bLeft = true;
	}
	if (!imput.buttons[VK_LEFT])
		imput.bLeft = false;

	if (imput.bRight == false && imput.buttons[VK_RIGHT])
	{
		currAnimTime += 100.0 * Time.Delta();
		if (currAnimTime > IdleAnimationData->Duration)
			currAnimTime = 0.0;
		imput.bRight = true;
	}
	if (!imput.buttons[VK_RIGHT])
		imput.bRight = false;

#pragma endregion

#pragma region settings for all draw calls
	float color[4]{ 0.87f, 0.87f, 1.0f, 0.0f };

	context->OMSetRenderTargets(1, &rtv, depthStencilView);
	context->ClearRenderTargetView(rtv, color);
	context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->RSSetViewports(1, &viewport);
	context->VSSetShader(vertexshader, NULL, NULL);
	context->VSSetConstantBuffers(0, 1, &constBuffer);
	context->VSSetConstantBuffers(1, 1, &modelAnimationConstBuffer);
	context->PSSetShader(pixelshader, NULL, NULL);
	context->IASetInputLayout(layout);

	ZeroMemory(&mapResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	context->Map(constBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
	memcpy(mapResource.pData, &send_to_ram, sizeof(VRAM));
	context->Unmap(constBuffer, 0);

#pragma endregion

#pragma region draw model

	keyFrameCount = (int)IdleAnimationData->Frames.size();
	boneCount = (int)IdleAnimationData->Frames[0].Joints.size();
	animLoopTime = IdleAnimationData->Duration;

	////////////////////find current times animation index///////////////////
	for (size_t i = 0; i < keyFrameCount; i++)
		if (currAnimTime > IdleAnimationData->Frames[i].Time)
			keyframeAnimIndex = (unsigned int)i;
	///////////////////

	////store the time stamp for the keyframe infront and behind current time////
	ZeroMemory(twoKeyFrameTimes, sizeof(double) * 2);
	twoKeyFrameTimes[0] = IdleAnimationData->Frames[keyframeAnimIndex].Time;
	if ((unsigned)keyframeAnimIndex + 1 < keyFrameCount)
		twoKeyFrameTimes[1] = IdleAnimationData->Frames[keyframeAnimIndex + 1].Time;
	else
		twoKeyFrameTimes[1] = IdleAnimationData->Frames[0].Time;
	////

	///////////////////calculate ratio between keyframes//////////////////////////
	double ratio = (currAnimTime - twoKeyFrameTimes[0]) / (twoKeyFrameTimes[1] - twoKeyFrameTimes[0]);
	///////////////////

	/////////////////slerp between two key frames using ratio/////////////////////
	XMMATRIX pos = XMMatrixIdentity();
	XMFLOAT4 t{};
	for (size_t i = 0; i < boneCount; i++)
	{
		XMVECTOR from = XMVectorSet(
			IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]._41,
			IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]._42,
			IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]._43,
			IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]._44);
		XMVECTOR to;
		if ((unsigned)keyframeAnimIndex + 1 < keyFrameCount)
			to = XMVectorSet(
				IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]._41,
				IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]._42,
				IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]._43,
				IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]._44);
		else
			to = XMVectorSet(
				IdleAnimationData->Frames[0].Joints[i]._41,
				IdleAnimationData->Frames[0].Joints[i]._42,
				IdleAnimationData->Frames[0].Joints[i]._43,
				IdleAnimationData->Frames[0].Joints[i]._44);


		XMVECTOR eye = XMQuaternionSlerp(from, to, (float)ratio);
		XMStoreFloat4(&t, eye);
		DebugPointData[i].xyzw = t;
		send_to_ram2.RealTimePose[i]._11 = 1.0f; send_to_ram2.RealTimePose[i]._12 = 0.0f; send_to_ram2.RealTimePose[i]._13 = 0.0f; send_to_ram2.RealTimePose[i]._14 = 0.0f;
		send_to_ram2.RealTimePose[i]._21 = 0.0f; send_to_ram2.RealTimePose[i]._22 = 1.0f; send_to_ram2.RealTimePose[i]._23 = 0.0f; send_to_ram2.RealTimePose[i]._24 = 0.0f;
		send_to_ram2.RealTimePose[i]._31 = 0.0f; send_to_ram2.RealTimePose[i]._32 = 0.0f; send_to_ram2.RealTimePose[i]._33 = 1.0f; send_to_ram2.RealTimePose[i]._34 = 0.0f;
		send_to_ram2.RealTimePose[i]._41 = t.x;	 send_to_ram2.RealTimePose[i]._42 = t.y;  send_to_ram2.RealTimePose[i]._43 = t.z;  send_to_ram2.RealTimePose[i]._44 = t.w;

		//XMStoreFloat4x4(&send_to_ram2.RealTimePose[i], XMMatrixTranspose(XMMatrixLookAtLH(eye, at, up)));

	}
	/////////////////
	ZeroMemory(&mapResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	context->Map(modelAnimationConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
	memcpy(mapResource.pData, &send_to_ram2, sizeof(ANIMATION_VRAM));
	context->Unmap(modelAnimationConstBuffer, 0);

	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetVertexBuffers(0, 1, &modelvertbuffer, &stride, &offset);
	context->IASetIndexBuffer(modelindexbuffer, DXGI_FORMAT_R32_UINT, offset);
	context->RSSetState(wireFrameRasterizerState);
	context->DrawIndexed(modelindexCount, 0, 0);

#pragma endregion

#pragma region debug
	ZeroMemory(&mapResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	context->Map(pDebugPointBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
	memcpy(mapResource.pData, &DebugPointData, sizeof(SIMPLE_VERTEX) * DebugPointCount);
	context->Unmap(pDebugPointBuffer, 0);

	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	context->IASetVertexBuffers(0, 1, &pDebugPointBuffer, &stride, &offset);
	context->RSSetState(wireFrameRasterizerState);
	context->Draw(DebugPointCount, 0);

	int index = 0;
	for (size_t i = 0; i < bind_pose.size(); i++)
	{
		if (bind_pose[i].Parent_Index != -1)
		{
			DebugLineData[index].xyzw.x = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._41;
			DebugLineData[index].xyzw.y = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._42;
			DebugLineData[index].xyzw.z = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._43;
			DebugLineData[index].xyzw.w = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._44;
			index++;
			DebugLineData[index].xyzw.x = send_to_ram2.RealTimePose[i]._41;
			DebugLineData[index].xyzw.y = send_to_ram2.RealTimePose[i]._42;
			DebugLineData[index].xyzw.z = send_to_ram2.RealTimePose[i]._43;
			DebugLineData[index].xyzw.w = send_to_ram2.RealTimePose[i]._44;
			index++;

		}

	}
	ZeroMemory(&mapResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	context->Map(pDebugLineBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
	memcpy(mapResource.pData, &DebugLineData, sizeof(SIMPLE_VERTEX) * DebugLineCount);
	context->Unmap(pDebugLineBuffer, 0);

	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	context->IASetVertexBuffers(0, 1, &pDebugLineBuffer, &stride, &offset);
	context->RSSetState(wireFrameRasterizerState);
	//context->Draw(DebugLineCount, 0);
#pragma endregion

#pragma region draw ground
	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetVertexBuffers(0, 1, &groundvertbuffer, &stride, &offset);
	context->IASetIndexBuffer(groundindexbuffer, DXGI_FORMAT_R32_UINT, offset);
	context->RSSetState(SolidRasterizerState);
	context->DrawIndexed(groundindexCount, 0, 0);
#pragma endregion

	swapchain->Present(0, 0);
	return true;
}

bool DEMO_APP::ShutDown()
{

	device->Release();
	swapchain->Release();
	context->Release();
	rtv->Release();

	depthStencil->Release();
	depthStencilView->Release();
	wireFrameRasterizerState->Release();
	SolidRasterizerState->Release();

	modelvertbuffer->Release();
	modelindexbuffer->Release();

	delete realTimeModel;

	groundvertbuffer->Release();
	groundindexbuffer->Release();

	if (pDebugPointBuffer)
		pDebugPointBuffer->Release();
	if (pDebugLineBuffer)
		pDebugLineBuffer->Release();


	layout->Release();

	vertexshader->Release();
	pixelshader->Release();

	constBuffer->Release();
	modelAnimationConstBuffer->Release();
	UnregisterClass(L"DirectXApplication", application);
	return true;
}

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
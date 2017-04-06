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
	struct SIMPLE_VERTEX { XMFLOAT4 xyzw; XMFLOAT4 color; };
	struct VRAM { XMFLOAT4X4 camView; XMFLOAT4X4 camProj; XMFLOAT4X4 modelPos; };
	XTime Time;

	DEMO_APP(HINSTANCE hinst, WNDPROC proc);
	bool Run();
	bool ShutDown();
	void DrawPoints(SIMPLE_VERTEX & ThePoints, int PointCount);
	void DrawLines(SIMPLE_VERTEX & TheLines, int LineCount);
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
	ID3D11Buffer * modelvertbuffer = NULL;
	ID3D11Buffer * modelindexbuffer = NULL;
	unsigned int modelindexCount = 0;

	//model animation data
	Skeleton * mSkeleton = nullptr;
	unsigned int triCount = 0;
	vector<unsigned int> triIndices;
	vector<BlendingVertex> verts;
	vector<Bone> bind_pose;
	SIMPLE_VERTEX * keyFrames = nullptr;
	unsigned int keyFrameCount = 0;
	unsigned int boneCount = 0;
	unsigned int currKeyFrame = 0;
	double animLoopTime = 0.0f;
	double currAnimTime = 0.0f;
	int keyframeAnimIndex = 0;
	double twoKeyFrameTimes[2]{};

	//ground plane
	ID3D11Buffer * groundvertbuffer = NULL;
	ID3D11Buffer * groundindexbuffer = NULL;
	unsigned int groundindexCount = 0;

	//debug buffer
	ID3D11Buffer * debugPointBuffer = nullptr;
	ID3D11Buffer * debugLineBuffer = nullptr;
	bool debugPointInit = false;
	bool debugLineInit = false;

	ID3D11InputLayout * layout = NULL;

	ID3D11VertexShader*     vertexshader = NULL;
	ID3D11PixelShader*      pixelshader = NULL;

	ID3D11Buffer * constBuffer = NULL;
	VRAM send_to_ram;

	XMFLOAT4X4 camera;

	UINT stride = sizeof(SIMPLE_VERTEX);
	UINT offset = 0;

};

//************************************************************
//************ CREATION OF OBJECTS & RESOURCES ***************
//************************************************************

DEMO_APP::DEMO_APP(HINSTANCE hinst, WNDPROC proc)
{

#pragma region wind
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
#pragma endregion

	//XTime
	Time.Restart();
	currAnimTime = 0.01f;


#pragma region camera model
	//camera data
	//static const XMVECTORF32 eye = { 0.0f, 1.0f, -1.0f, 0.0f };
	//static const XMVECTORF32 eye = { 0.0f, 35.0f, -30.0f, 0.0f };
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

	//function(file, mesh, bone, animation);

	mSkeleton = new Skeleton();
	functionality(mesh, bone, animation, triCount, triIndices, verts, mSkeleton, bind_pose);
#pragma endregion

#pragma region model buffers
	D3D11_BUFFER_DESC bufferdescription;
	D3D11_SUBRESOURCE_DATA InitData;

	//teddy//
	SIMPLE_VERTEX * model;
	model = new SIMPLE_VERTEX[verts.size()];
	float modelColor[4]{ 1.0f, 1.0f, 1.0f, 0.0f };
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

	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * verts.size());
	bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferdescription.CPUAccessFlags = NULL;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = model;
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
	//end teddy//

	//ground plane//
	groundindexCount = 6;
	SIMPLE_VERTEX groundPlane[4]{};
	groundPlane[0].xyzw = XMFLOAT4(1.0f, 0.0f, 1.0f, 0);
	groundPlane[1].xyzw = XMFLOAT4(1.0f, 0.0f, -1.0f, 0);
	groundPlane[2].xyzw = XMFLOAT4(-1.0f, 0.0f, -1.0f, 0);
	groundPlane[3].xyzw = XMFLOAT4(-1.0f, 0.0f, 1.0f, 0);
	float groundColor[4]{ 1.0f, 1.0f, 0.0f, 0.0f };
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
	//end ground plane//

#pragma endregion

#pragma region create shaders
	device->CreateVertexShader(Trivial_VS, sizeof(Trivial_VS), NULL, &vertexshader);
	device->CreatePixelShader(Trivial_PS, sizeof(Trivial_PS), NULL, &pixelshader);
#pragma endregion

#pragma region imput layout
	D3D11_INPUT_ELEMENT_DESC vertlayout[] =
	{
		"POSITION", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,0 ,{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(vertlayout);
	device->CreateInputLayout(vertlayout, numElements, Trivial_VS, sizeof(Trivial_VS), &layout);
#pragma endregion

#pragma region const buff
	CD3D11_BUFFER_DESC constBufferDesc(sizeof(VRAM), D3D11_BIND_CONSTANT_BUFFER);
	device->CreateBuffer(&constBufferDesc, nullptr, &constBuffer);
#pragma endregion

}

//************************************************************
//************ EXECUTION *************************************
//************************************************************

bool DEMO_APP::Run()
{
#pragma region time
	Time.Signal();
	currAnimTime += Time.SmoothDelta();
	if (currAnimTime > animLoopTime)
		currAnimTime = 0.01f;
#pragma endregion

#pragma region mouse update
	if (imput.mouse_move)
		if (imput.left_click)
		{
			XMMATRIX newcamera = XMLoadFloat4x4(&camera);
			XMVECTOR pos = newcamera.r[3];
			newcamera.r[3] = XMLoadFloat4(&XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
			newcamera = XMMatrixRotationX(imput.diffy * (float)Time.Delta()) * newcamera * XMMatrixRotationY(imput.diffx * (float)Time.Delta());
			newcamera.r[3] = pos;
			XMStoreFloat4x4(&camera, newcamera);
			XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
		}
	imput.mouse_move = false;
#pragma endregion

#pragma region keyboard update
	if (imput.buttons['W'])
	{
		XMMATRIX newcamera = XMLoadFloat4x4(&camera);
		XMVECTOR forward{ 0.0f,0.0f,50.0f,0.0f };
		newcamera.r[3] -= forward * (float)Time.Delta();
		XMStoreFloat4x4(&camera, newcamera);
		XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
	}
	if (imput.buttons['S'])
	{
		XMMATRIX newcamera = XMLoadFloat4x4(&camera);
		XMVECTOR forward{ 0.0f,0.0f,50.0f,0.0f };
		newcamera.r[3] += forward * (float)Time.Delta();
		XMStoreFloat4x4(&camera, newcamera);
		XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
	}
	if (imput.buttons['A'])
	{
		XMMATRIX newcamera = XMLoadFloat4x4(&camera);
		XMVECTOR Right{ 50.0f,0.0f,0.0f,0.0f };
		newcamera.r[3] += Right * (float)Time.Delta();
		XMStoreFloat4x4(&camera, newcamera);
		XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
	}
	if (imput.buttons['D'])
	{
		XMMATRIX newcamera = XMLoadFloat4x4(&camera);
		XMVECTOR Right{ 50.0f,0.0f,0.0f,0.0f };
		newcamera.r[3] -= Right *(float)Time.Delta();
		XMStoreFloat4x4(&camera, newcamera);
		XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(newcamera));
	}
	if (imput.bLeft == false && imput.buttons[VK_LEFT])
	{
		if (currKeyFrame <= 0)
			currKeyFrame = 0;
		else
			currKeyFrame--;
		imput.bLeft = true;
	}
	if (!imput.buttons[VK_LEFT])
		imput.bLeft = false;
	
	if (imput.bRight == false && imput.buttons[VK_RIGHT])
	{
		if (currKeyFrame >= keyFrameCount)
			currKeyFrame = keyFrameCount;
		else
			currKeyFrame++;
		imput.bRight = true;
	}
	if (!imput.buttons[VK_RIGHT])
		imput.bRight = false;
#pragma endregion

#pragma region settings for all draw calls
	float color[4]{ 0.0f, 1.0f, 0.0f, 0.0f };
	context->OMSetRenderTargets(1, &rtv, depthStencilView);
	context->ClearRenderTargetView(rtv, color);
	context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->RSSetViewports(1, &viewport);

	context->UpdateSubresource(constBuffer, 0, nullptr, &send_to_ram, 0, 0);

	context->VSSetShader(vertexshader, NULL, NULL);
	context->PSSetShader(pixelshader, NULL, NULL);
	context->IASetInputLayout(layout);
	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetConstantBuffers(0, 1, &constBuffer);
#pragma endregion

#pragma region draw model
	context->IASetVertexBuffers(0, 1, &modelvertbuffer, &stride, &offset);
	context->IASetIndexBuffer(modelindexbuffer, DXGI_FORMAT_R32_UINT, offset);
	context->RSSetState(wireFrameRasterizerState);
	//context->DrawIndexed(modelindexCount, 0, 0);
#pragma endregion

#pragma region draw ground
	context->IASetVertexBuffers(0, 1, &groundvertbuffer, &stride, &offset);
	context->IASetIndexBuffer(groundindexbuffer, DXGI_FORMAT_R32_UINT, offset);
	context->RSSetState(SolidRasterizerState);
	context->DrawIndexed(groundindexCount, 0, 0);
#pragma endregion

#pragma region debug joints
	/*static SIMPLE_VERTEX * jointBindPose = new SIMPLE_VERTEX[bind_pose.size()];
	for (size_t i = 0; i < bind_pose.size(); i++)
	{
		jointBindPose[i].xyzw = XMFLOAT4(-bind_pose[i].matrix._41, -bind_pose[i].matrix._42, -bind_pose[i].matrix._43, -bind_pose[i].matrix._44);
		jointBindPose[i].color = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}*/
	//DrawPoints(jointBindPose[0], (int)bind_pose.size());

	//keyframes 
	if (debugPointInit == false)
	{
		keyFrameCount = (int)mSkeleton->joints.size();
		boneCount = (int)mSkeleton->joints[0]->bones.size();
		keyFrames = new SIMPLE_VERTEX[keyFrameCount * boneCount];
		animLoopTime = mSkeleton->joints[keyFrameCount-1]->Time;
		for (size_t i = 0; i < (keyFrameCount * boneCount); )
		{
			ZeroMemory(keyFrames, sizeof(SIMPLE_VERTEX) * (keyFrameCount * boneCount));
			for (size_t j = 0; j < keyFrameCount; j++)
				for (size_t k = 0; k < boneCount; k++)
				{
					keyFrames[i].xyzw = XMFLOAT4(mSkeleton->joints[j]->bones[k].matrix._41, mSkeleton->joints[j]->bones[k].matrix._42, mSkeleton->joints[j]->bones[k].matrix._43, mSkeleton->joints[j]->bones[k].matrix._44);
					i++;
				}
		}

		//find the two key frames you use
		for (size_t i = 0; i < keyFrameCount; i++)
			if (currAnimTime > mSkeleton->joints[i]->Time)
				keyframeAnimIndex = (int)i;
		//end find the two key frames you use

		//get the time of the two key frames
		ZeroMemory(twoKeyFrameTimes, sizeof(double) * 2);
		twoKeyFrameTimes[0] = mSkeleton->joints[keyframeAnimIndex]->Time;
		twoKeyFrameTimes[1] = mSkeleton->joints[keyframeAnimIndex + 1]->Time;
		//end get the time of the two key frames

		//get ratio representing the real time between the two key frames
		double ratio = (currAnimTime - twoKeyFrameTimes[0]) / (twoKeyFrameTimes[1] - twoKeyFrameTimes[0]);
		//end get ratio representing the real time between the two key frames

		vector<SIMPLE_VERTEX> realTimeJoints;
		
		//calculation for slerp to generate real time vector 
		/*
		for (size_t i = 0; i < boneCount; i++)
		{
			XMVECTOR from = XMVectorSet(mSkeleton->joints[keyframeAnimIndex]->bones[i].matrix._41, mSkeleton->joints[keyframeAnimIndex]->bones[i].matrix._42, mSkeleton->joints[keyframeAnimIndex]->bones[i].matrix._43, mSkeleton->joints[keyframeAnimIndex]->bones[i].matrix._44);
			XMVECTOR to = XMVectorSet(mSkeleton->joints[keyframeAnimIndex + 1]->bones[i].matrix._41, mSkeleton->joints[keyframeAnimIndex + 1]->bones[i].matrix._42, mSkeleton->joints[keyframeAnimIndex + 1]->bones[i].matrix._43, mSkeleton->joints[keyframeAnimIndex + 1]->bones[i].matrix._44);
			XMVECTOR realTimePoint = XMQuaternionSlerp(from, to, )
		}
		XMFLOAT4 from(0.0f,0.0f,0.0f,0.0f);
		XMFLOAT4 to(1.0f, 1.0f, 1.0f, 1.0f);

		XMVECTOR realTimePoint = XMQuaternionSlerp(XMLoadFloat4(&from), XMLoadFloat4(&to), 0.5f);
		XMFLOAT4 point;
		XMStoreFloat4(&point, realTimePoint);
		*/
		//end calculation for slerp to generate real time vector 

		DrawPoints(keyFrames[0], boneCount);
	}
	else
	{
		//find the two key frames you use
		for (size_t i = 0; i < keyFrameCount; i++)
			if (currAnimTime > mSkeleton->joints[i]->Time)
				keyframeAnimIndex = (int)i;
		//end find the two key frames you use

		//get the time of the two key frames
		ZeroMemory(twoKeyFrameTimes, sizeof(double) * 2);
		twoKeyFrameTimes[0] = mSkeleton->joints[keyframeAnimIndex]->Time;
		twoKeyFrameTimes[1] = mSkeleton->joints[keyframeAnimIndex + 1]->Time;
		//end get the time of the two key frames

		//get ratio representing the real time between the two key frames
		double ratio = (currAnimTime - twoKeyFrameTimes[0]) / (twoKeyFrameTimes[1] - twoKeyFrameTimes[0]);
		//end get ratio representing the real time between the two key frames

		DrawPoints(keyFrames[0], boneCount);
	}
	//end keyframes 
#pragma endregion

#pragma region debug bones

	static SIMPLE_VERTEX * boneBindPose = new SIMPLE_VERTEX[256];
	//one
	boneBindPose[0].xyzw = XMFLOAT4(-bind_pose[1].matrix._41, -bind_pose[1].matrix._42, -bind_pose[1].matrix._43, -bind_pose[1].matrix._44);
	boneBindPose[1].xyzw = XMFLOAT4(-bind_pose[2].matrix._41, -bind_pose[2].matrix._42, -bind_pose[2].matrix._43, -bind_pose[2].matrix._44);
	//two
	boneBindPose[2].xyzw = XMFLOAT4(-bind_pose[2].matrix._41, -bind_pose[2].matrix._42, -bind_pose[2].matrix._43, -bind_pose[2].matrix._44);
	boneBindPose[3].xyzw = XMFLOAT4(-bind_pose[3].matrix._41, -bind_pose[3].matrix._42, -bind_pose[3].matrix._43, -bind_pose[3].matrix._44);
	//three
	boneBindPose[4].xyzw = XMFLOAT4(-bind_pose[3].matrix._41, -bind_pose[3].matrix._42, -bind_pose[3].matrix._43, -bind_pose[3].matrix._44);
	boneBindPose[5].xyzw = XMFLOAT4(-bind_pose[4].matrix._41, -bind_pose[4].matrix._42, -bind_pose[4].matrix._43, -bind_pose[4].matrix._44);
	//four
	boneBindPose[6].xyzw = XMFLOAT4(-bind_pose[4].matrix._41, -bind_pose[4].matrix._42, -bind_pose[4].matrix._43, -bind_pose[4].matrix._44);
	boneBindPose[7].xyzw = XMFLOAT4(-bind_pose[5].matrix._41, -bind_pose[5].matrix._42, -bind_pose[5].matrix._43, -bind_pose[5].matrix._44);
	//five
	boneBindPose[8].xyzw = XMFLOAT4(-bind_pose[3].matrix._41, -bind_pose[3].matrix._42, -bind_pose[3].matrix._43, -bind_pose[3].matrix._44);
	boneBindPose[9].xyzw = XMFLOAT4(-bind_pose[6].matrix._41, -bind_pose[6].matrix._42, -bind_pose[6].matrix._43, -bind_pose[6].matrix._44);
	//six
	boneBindPose[10].xyzw = XMFLOAT4(-bind_pose[6].matrix._41, -bind_pose[6].matrix._42, -bind_pose[6].matrix._43, -bind_pose[6].matrix._44);
	boneBindPose[11].xyzw = XMFLOAT4(-bind_pose[7].matrix._41, -bind_pose[7].matrix._42, -bind_pose[7].matrix._43, -bind_pose[7].matrix._44);
	//sevin
	boneBindPose[12].xyzw = XMFLOAT4(-bind_pose[7].matrix._41, -bind_pose[7].matrix._42, -bind_pose[7].matrix._43, -bind_pose[7].matrix._44);
	boneBindPose[13].xyzw = XMFLOAT4(-bind_pose[8].matrix._41, -bind_pose[8].matrix._42, -bind_pose[8].matrix._43, -bind_pose[8].matrix._44);
	//eight
	boneBindPose[14].xyzw = XMFLOAT4(-bind_pose[8].matrix._41, -bind_pose[8].matrix._42, -bind_pose[8].matrix._43, -bind_pose[8].matrix._44);
	boneBindPose[15].xyzw = XMFLOAT4(-bind_pose[9].matrix._41, -bind_pose[9].matrix._42, -bind_pose[9].matrix._43, -bind_pose[9].matrix._44);
	//nine
	boneBindPose[16].xyzw = XMFLOAT4(-bind_pose[9].matrix._41, -bind_pose[9].matrix._42, -bind_pose[9].matrix._43, -bind_pose[9].matrix._44);
	boneBindPose[17].xyzw = XMFLOAT4(-bind_pose[10].matrix._41, -bind_pose[10].matrix._42, -bind_pose[10].matrix._43, -bind_pose[10].matrix._44);
	//ten
	boneBindPose[18].xyzw = XMFLOAT4(-bind_pose[10].matrix._41, -bind_pose[10].matrix._42, -bind_pose[10].matrix._43, -bind_pose[10].matrix._44);
	boneBindPose[19].xyzw = XMFLOAT4(-bind_pose[11].matrix._41, -bind_pose[11].matrix._42, -bind_pose[11].matrix._43, -bind_pose[11].matrix._44);
	//eleven
	boneBindPose[20].xyzw = XMFLOAT4(-bind_pose[10].matrix._41, -bind_pose[10].matrix._42, -bind_pose[10].matrix._43, -bind_pose[10].matrix._44);
	boneBindPose[21].xyzw = XMFLOAT4(-bind_pose[12].matrix._41, -bind_pose[12].matrix._42, -bind_pose[12].matrix._43, -bind_pose[12].matrix._44);
	//twelve
	boneBindPose[22].xyzw = XMFLOAT4(-bind_pose[3].matrix._41, -bind_pose[3].matrix._42, -bind_pose[3].matrix._43, -bind_pose[3].matrix._44);
	boneBindPose[23].xyzw = XMFLOAT4(-bind_pose[13].matrix._41, -bind_pose[13].matrix._42, -bind_pose[13].matrix._43, -bind_pose[13].matrix._44);
	//thirteen
	boneBindPose[24].xyzw = XMFLOAT4(-bind_pose[13].matrix._41, -bind_pose[13].matrix._42, -bind_pose[13].matrix._43, -bind_pose[13].matrix._44);
	boneBindPose[25].xyzw = XMFLOAT4(-bind_pose[14].matrix._41, -bind_pose[14].matrix._42, -bind_pose[14].matrix._43, -bind_pose[14].matrix._44);
	//fourteen
	boneBindPose[26].xyzw = XMFLOAT4(-bind_pose[14].matrix._41, -bind_pose[14].matrix._42, -bind_pose[14].matrix._43, -bind_pose[14].matrix._44);
	boneBindPose[27].xyzw = XMFLOAT4(-bind_pose[15].matrix._41, -bind_pose[15].matrix._42, -bind_pose[15].matrix._43, -bind_pose[15].matrix._44);
	//fifteen
	boneBindPose[28].xyzw = XMFLOAT4(-bind_pose[15].matrix._41, -bind_pose[15].matrix._42, -bind_pose[15].matrix._43, -bind_pose[15].matrix._44);
	boneBindPose[29].xyzw = XMFLOAT4(-bind_pose[16].matrix._41, -bind_pose[16].matrix._42, -bind_pose[16].matrix._43, -bind_pose[16].matrix._44);
	//sixteen
	boneBindPose[30].xyzw = XMFLOAT4(-bind_pose[16].matrix._41, -bind_pose[16].matrix._42, -bind_pose[16].matrix._43, -bind_pose[16].matrix._44);
	boneBindPose[31].xyzw = XMFLOAT4(-bind_pose[17].matrix._41, -bind_pose[17].matrix._42, -bind_pose[17].matrix._43, -bind_pose[17].matrix._44);
	//sevinteen
	boneBindPose[32].xyzw = XMFLOAT4(-bind_pose[17].matrix._41, -bind_pose[17].matrix._42, -bind_pose[17].matrix._43, -bind_pose[17].matrix._44);
	boneBindPose[33].xyzw = XMFLOAT4(-bind_pose[18].matrix._41, -bind_pose[18].matrix._42, -bind_pose[18].matrix._43, -bind_pose[18].matrix._44);
	//eighteen
	boneBindPose[34].xyzw = XMFLOAT4(-bind_pose[17].matrix._41, -bind_pose[17].matrix._42, -bind_pose[17].matrix._43, -bind_pose[17].matrix._44);
	boneBindPose[35].xyzw = XMFLOAT4(-bind_pose[19].matrix._41, -bind_pose[19].matrix._42, -bind_pose[19].matrix._43, -bind_pose[19].matrix._44);
	//nighnteen
	boneBindPose[36].xyzw = XMFLOAT4(-bind_pose[4].matrix._41, -bind_pose[4].matrix._42, -bind_pose[4].matrix._43, -bind_pose[4].matrix._44);
	boneBindPose[37].xyzw = XMFLOAT4(-bind_pose[20].matrix._41, -bind_pose[20].matrix._42, -bind_pose[20].matrix._43, -bind_pose[20].matrix._44);
	//twenty
	boneBindPose[38].xyzw = XMFLOAT4(-bind_pose[20].matrix._41, -bind_pose[20].matrix._42, -bind_pose[20].matrix._43, -bind_pose[20].matrix._44);
	boneBindPose[39].xyzw = XMFLOAT4(-bind_pose[21].matrix._41, -bind_pose[21].matrix._42, -bind_pose[21].matrix._43, -bind_pose[21].matrix._44);
	//twenty one
	boneBindPose[40].xyzw = XMFLOAT4(-bind_pose[21].matrix._41, -bind_pose[21].matrix._42, -bind_pose[21].matrix._43, -bind_pose[21].matrix._44);
	boneBindPose[41].xyzw = XMFLOAT4(-bind_pose[22].matrix._41, -bind_pose[22].matrix._42, -bind_pose[22].matrix._43, -bind_pose[22].matrix._44);
	//twenty two
	boneBindPose[42].xyzw = XMFLOAT4(-bind_pose[22].matrix._41, -bind_pose[22].matrix._42, -bind_pose[22].matrix._43, -bind_pose[22].matrix._44);
	boneBindPose[43].xyzw = XMFLOAT4(-bind_pose[23].matrix._41, -bind_pose[23].matrix._42, -bind_pose[23].matrix._43, -bind_pose[23].matrix._44);
	//twenty three
	boneBindPose[44].xyzw = XMFLOAT4(-bind_pose[23].matrix._41, -bind_pose[23].matrix._42, -bind_pose[23].matrix._43, -bind_pose[23].matrix._44);
	boneBindPose[45].xyzw = XMFLOAT4(-bind_pose[24].matrix._41, -bind_pose[24].matrix._42, -bind_pose[24].matrix._43, -bind_pose[24].matrix._44);
	//twenty four
	boneBindPose[46].xyzw = XMFLOAT4(-bind_pose[0].matrix._41, -bind_pose[0].matrix._42, -bind_pose[0].matrix._43, -bind_pose[0].matrix._44);
	boneBindPose[47].xyzw = XMFLOAT4(-bind_pose[25].matrix._41, -bind_pose[25].matrix._42, -bind_pose[25].matrix._43, -bind_pose[25].matrix._44);
	//twenty five
	boneBindPose[48].xyzw = XMFLOAT4(-bind_pose[25].matrix._41, -bind_pose[25].matrix._42, -bind_pose[25].matrix._43, -bind_pose[25].matrix._44);
	boneBindPose[49].xyzw = XMFLOAT4(-bind_pose[26].matrix._41, -bind_pose[26].matrix._42, -bind_pose[26].matrix._43, -bind_pose[26].matrix._44);
	//twenty six
	boneBindPose[50].xyzw = XMFLOAT4(-bind_pose[26].matrix._41, -bind_pose[26].matrix._42, -bind_pose[26].matrix._43, -bind_pose[26].matrix._44);
	boneBindPose[51].xyzw = XMFLOAT4(-bind_pose[27].matrix._41, -bind_pose[27].matrix._42, -bind_pose[27].matrix._43, -bind_pose[27].matrix._44);
	//twenty sevin
	boneBindPose[52].xyzw = XMFLOAT4(-bind_pose[27].matrix._41, -bind_pose[27].matrix._42, -bind_pose[27].matrix._43, -bind_pose[27].matrix._44);
	boneBindPose[53].xyzw = XMFLOAT4(-bind_pose[28].matrix._41, -bind_pose[28].matrix._42, -bind_pose[28].matrix._43, -bind_pose[28].matrix._44);
	//twenty eight
	boneBindPose[54].xyzw = XMFLOAT4(-bind_pose[0].matrix._41, -bind_pose[0].matrix._42, -bind_pose[0].matrix._43, -bind_pose[0].matrix._44);
	boneBindPose[55].xyzw = XMFLOAT4(-bind_pose[29].matrix._41, -bind_pose[29].matrix._42, -bind_pose[29].matrix._43, -bind_pose[29].matrix._44);
	//twenty nine
	boneBindPose[56].xyzw = XMFLOAT4(-bind_pose[29].matrix._41, -bind_pose[29].matrix._42, -bind_pose[29].matrix._43, -bind_pose[29].matrix._44);
	boneBindPose[57].xyzw = XMFLOAT4(-bind_pose[30].matrix._41, -bind_pose[30].matrix._42, -bind_pose[30].matrix._43, -bind_pose[30].matrix._44);
	//thirty
	boneBindPose[58].xyzw = XMFLOAT4(-bind_pose[30].matrix._41, -bind_pose[30].matrix._42, -bind_pose[30].matrix._43, -bind_pose[30].matrix._44);
	boneBindPose[59].xyzw = XMFLOAT4(-bind_pose[31].matrix._41, -bind_pose[31].matrix._42, -bind_pose[31].matrix._43, -bind_pose[31].matrix._44);
	//thirty one
	boneBindPose[60].xyzw = XMFLOAT4(-bind_pose[31].matrix._41, -bind_pose[31].matrix._42, -bind_pose[31].matrix._43, -bind_pose[31].matrix._44);
	boneBindPose[61].xyzw = XMFLOAT4(-bind_pose[32].matrix._41, -bind_pose[32].matrix._42, -bind_pose[32].matrix._43, -bind_pose[32].matrix._44);

	//DrawLines(boneBindPose[0], (int)62);

#pragma endregion

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

#pragma region debug functions

void DEMO_APP::DrawPoints(SIMPLE_VERTEX & ThePoints, int PointCount)
{
	if (debugPointInit == false)
	{
		D3D11_BUFFER_DESC bufferdescription;
		D3D11_SUBRESOURCE_DATA InitData;
		UINT VertexCount = PointCount;
		ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
		bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
		bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * VertexCount * keyFrameCount);
		bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferdescription.CPUAccessFlags = NULL;
		bufferdescription.MiscFlags = NULL;
		bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
		ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
		InitData.pSysMem = &ThePoints;
		device->CreateBuffer(&bufferdescription, &InitData, &debugPointBuffer);
		debugPointInit = true;
	}
	if (debugPointInit == true)
	{
		UINT VertexCount = PointCount;
		context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		context->IASetVertexBuffers(0, 1, &debugPointBuffer, &stride, &offset);
		context->RSSetState(SolidRasterizerState);
		UINT indexoffset = currKeyFrame * boneCount;
		context->Draw(VertexCount, indexoffset);
	}
}

void DEMO_APP::DrawLines(SIMPLE_VERTEX & TheLines, int LineCount)
{
	if (debugLineInit == false)
	{
		D3D11_BUFFER_DESC bufferdescription;
		D3D11_SUBRESOURCE_DATA InitData;
		UINT VertexCount = LineCount;
		ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
		bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
		bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * VertexCount);
		bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferdescription.CPUAccessFlags = NULL;
		bufferdescription.MiscFlags = NULL;
		bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
		ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
		InitData.pSysMem = &TheLines;
		device->CreateBuffer(&bufferdescription, &InitData, &debugLineBuffer);
		debugLineInit = true;
	}
	if (debugLineInit == true)
	{
		UINT VertexCount = 1048;
		context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		context->IASetVertexBuffers(0, 1, &debugLineBuffer, &stride, &offset);
		context->RSSetState(SolidRasterizerState);
		context->Draw(VertexCount, 0);
	}
}

#pragma endregion

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
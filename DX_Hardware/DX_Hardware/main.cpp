// CGS HW Project A "Line Land".
// Author: L.Norri CD CGS, FullSail University


#include <iostream>
#include <ctime>
#include "DDSTextureLoader.h"
#include "XTime.h"
#include <windowsx.h>
using namespace std;

#include <d3d11.h>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"User32.lib")

#include <DirectXMath.h>
using namespace DirectX;
#include "Trivial_PS.csh"
#include "Trivial_VS.csh"
#include "BasicShader.csh"
#include "BasicPixelShader.csh"


#define BACKBUFFER_WIDTH	1000
#define BACKBUFFER_HEIGHT	1000

#include "../FBX/FBX.h"

#pragma region mouse&keyboard imput class
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
	bool bSpace = false;
	bool bHome = false;
	bool bTab = false;
};
Imput::Imput()
{
}
Imput::~Imput()
{
}
Imput imput;
#pragma endregion

#pragma region cam pos save
class Save
{
public:
	Save();
	void LoadFromFile(XMFLOAT4X4 &data);
	void SaveToFile(XMFLOAT4X4 data);
	~Save();

private:
	char file[9]{ "save.bin" };
	FILE * f = nullptr;
	int error = 0;
};
Save::Save()
{
}
void Save::LoadFromFile(XMFLOAT4X4 &data)
{
	f = nullptr;
	error = fopen_s(&f, file, "rb");
	if (error)
		return;
	fread(&data, sizeof(XMFLOAT4X4), 1, f);
	fclose(f);
	f = nullptr;
}
void Save::SaveToFile(XMFLOAT4X4 data)
{
	error = fopen_s(&f, file, "wb");
	if (f)
		fwrite(&data, sizeof(XMFLOAT4X4), 1, f);
	fclose(f);
	f = nullptr;
}
Save::~Save()
{
}
Save save;
#pragma endregion

class DEMO_APP
{
public:
	struct SIMPLE_VERTEX { XMFLOAT4 xyzw; XMFLOAT4 normal; XMFLOAT4 color; XMFLOAT2 uv; XMFLOAT4 index; XMFLOAT4 weights; };
	struct VRAM { XMFLOAT4X4 camView; XMFLOAT4X4 camProj; XMFLOAT4X4 modelPos; XMFLOAT4 spot_light_pos; XMFLOAT4 spot_light_dir; };
	struct ANIMATION_VRAM { XMFLOAT4X4 InverseBindPose[128]; XMFLOAT4X4 RealTimePose[128]; };
	XTime Time;

	DEMO_APP(HINSTANCE hinst, WNDPROC proc);
	bool Run();
	bool ShutDown();
private:
	HINSTANCE						application;
	WNDPROC							appWndProc;
	HWND							window;
	HRESULT							HR;

	ID3D11Device *device = NULL;
	IDXGISwapChain *swapchain = NULL;
	ID3D11DeviceContext * context = NULL;
	ID3D11RenderTargetView * rtv = NULL;
	D3D11_VIEWPORT viewport;

	ID3D11Texture2D * depthStencil = nullptr;
	ID3D11DepthStencilView * depthStencilView = nullptr;
	ID3D11RasterizerState * wireFrameRasterizerState = nullptr;
	ID3D11RasterizerState * SolidRasterizerState = nullptr;
	bool RenderWireFrame = false;

	//model mesh
	ID3D11ShaderResourceView * pModelTexture = nullptr;
	vert_pos_skinned * pTheVerts = nullptr;
	unsigned int * pVertIndices = nullptr;
	ID3D11Buffer * modelvertbuffer = NULL;
	ID3D11Buffer * modelindexbuffer = NULL;
	unsigned int modelindexCount = 0;
	unsigned int modelVertCount = 0;

	//model animation data
	anim_clip * IdleAnimationData = nullptr;
	vector<joint> bind_pose;
	SIMPLE_VERTEX * realTimeModel = nullptr;
	unsigned int keyFrameCount = 0;
	unsigned int boneCount = 0;
	double animLoopTime = 0.0f;
	double currAnimTime = 0.0f;
	bool animationPaused = false;
	int keyframeAnimIndex = 0;
	double twoKeyFrameTimes[2]{};

	//ground plane
	ID3D11Buffer * groundvertbuffer = NULL;
	ID3D11Buffer * groundindexbuffer = NULL;
	unsigned int groundindexCount = 0;

	//debug buffer
	/*ID3D11Buffer * pDebugPointBuffer = nullptr;
	unsigned int DebugPointCount = 128;
	SIMPLE_VERTEX DebugPointData[128]{};
	ID3D11Buffer * pDebugLineBuffer = nullptr;
	unsigned int DebugLineCount = 128;
	SIMPLE_VERTEX DebugLineData[128]{};*/

	ID3D11InputLayout * layout = NULL;

	ID3D11VertexShader * vertexshader = NULL;
	ID3D11PixelShader * pixelshader = NULL;

	ID3D11VertexShader * Basicvertexshader = NULL;
	ID3D11PixelShader * Basicpixelshader = NULL;

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
	//char file[]{ "Box_Idle.fbx" };
	char mesh[]{ "mesh.bin" };
	char bone[]{ "bone.bin" };
	char animation[]{ "animation.bin" };
	IdleAnimationData = new anim_clip();
	//matrix comes in from fbx like this
	// 1.0 0.0 0.0 0.0
	// 0.0 1.0 0.0 0.0
	// 0.0 0.0 1.0 0.0
	// x   y   z   w
	function(file, mesh, bone, animation, IdleAnimationData, pTheVerts, modelVertCount, pVertIndices, modelindexCount);
	functionality(mesh, bone, animation, bind_pose);

	keyFrameCount = (int)IdleAnimationData->Frames.size();
	boneCount = (int)IdleAnimationData->Frames[0].Joints.size();
	animLoopTime = IdleAnimationData->Duration;
#pragma endregion

#pragma region models

	D3D11_BUFFER_DESC bufferdescription;
	D3D11_SUBRESOURCE_DATA InitData;
	//teddy
	realTimeModel = new SIMPLE_VERTEX[modelVertCount];
	XMFLOAT4 VertColor{ 0.0f, 1.0f, 0.0f, 0.0f };
	for (size_t i = 0; i < modelVertCount; i++)
	{
		realTimeModel[i].xyzw = pTheVerts[i].pos;
		realTimeModel[i].normal = pTheVerts[i].norm;
		realTimeModel[i].uv = pTheVerts[i].uv;
		realTimeModel[i].color = VertColor;
		for (int j = 0; j < pTheVerts[i].joints.size(); j++)
		{
			if (j == 0)
				realTimeModel[i].index.x = (float)pTheVerts[i].joints[0];
			else if (j == 1)
				realTimeModel[i].index.y = (float)pTheVerts[i].joints[1];
			else if (j == 2)
				realTimeModel[i].index.z = (float)pTheVerts[i].joints[2];
			else if (j == 3)
				realTimeModel[i].index.w = (float)pTheVerts[i].joints[3];
		}
		for (int j = 0; j < pTheVerts[i].weights.size(); j++)
		{
			if (j == 0)
				realTimeModel[i].weights.x = pTheVerts[i].weights[0];
			else if (j == 1)
				realTimeModel[i].weights.y = pTheVerts[i].weights[1];
			else if (j == 2)
				realTimeModel[i].weights.z = pTheVerts[i].weights[2];
			else if (j == 3)
				realTimeModel[i].weights.w = pTheVerts[i].weights[3];
		}
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

	ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	bufferdescription.Usage = D3D11_USAGE_IMMUTABLE;
	bufferdescription.ByteWidth = (UINT)(sizeof(unsigned int) * modelindexCount);
	bufferdescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferdescription.CPUAccessFlags = NULL;
	bufferdescription.MiscFlags = NULL;
	bufferdescription.StructureByteStride = sizeof(unsigned int);
	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = pVertIndices;
	device->CreateBuffer(&bufferdescription, &InitData, &modelindexbuffer);

	//ground plane
	groundindexCount = 6;
	SIMPLE_VERTEX groundPlane[4]{};
	groundPlane[0].xyzw = XMFLOAT4(10.0f, 0.0f, 10.0f, 0.0f);
	groundPlane[1].xyzw = XMFLOAT4(10.0f, 0.0f, -10.0f, 0.0f);
	groundPlane[2].xyzw = XMFLOAT4(-10.0f, 0.0f, -10.0f, 0.0f);
	groundPlane[3].xyzw = XMFLOAT4(-10.0f, 0.0f, 10.0f, 0.0f);

	XMFLOAT4 groundColor{ 0.0f, 1.0f, 0.0f, 0.0f };
	groundPlane[0].color = groundColor;
	groundPlane[1].color = groundColor;
	groundPlane[2].color = groundColor;
	groundPlane[3].color = groundColor;

	groundPlane[0].normal = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	groundPlane[1].normal = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	groundPlane[2].normal = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	groundPlane[3].normal = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

	groundPlane[0].uv = XMFLOAT2(0.0f, 1.0f);
	groundPlane[1].uv = XMFLOAT2(1.0f, 1.0f);
	groundPlane[2].uv = XMFLOAT2(1.0f, 0.0f);
	groundPlane[3].uv = XMFLOAT2(0.0f, 0.0f);
	
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

#pragma region textures
	//HR = CreateDDSTextureFromFile(device, L"Teddy_D.dds", nullptr, &pModelTexture);
	HR = CreateDDSTextureFromFile(device, L"TestCube.dds", nullptr, &pModelTexture);
#pragma endregion

#pragma region debug render buffers
	//debug buffer
	//ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	//bufferdescription.Usage = D3D11_USAGE_DYNAMIC;
	//bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * DebugPointCount);
	//bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	//bufferdescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	//bufferdescription.MiscFlags = NULL;
	//bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	//ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	//InitData.pSysMem = DebugPointData;
	//device->CreateBuffer(&bufferdescription, &InitData, &pDebugPointBuffer);

	//int index = 0;
	//for (size_t i = 0; i < bind_pose.size(); i++)
	//{
	//	if (bind_pose[i].Parent_Index != -1)
	//	{
	//		DebugLineData[index].xyzw.x = bind_pose[bind_pose[i].Parent_Index].global_xform._41;
	//		DebugLineData[index].xyzw.y = bind_pose[bind_pose[i].Parent_Index].global_xform._42;
	//		DebugLineData[index].xyzw.z = bind_pose[bind_pose[i].Parent_Index].global_xform._43;
	//		DebugLineData[index].xyzw.w = bind_pose[bind_pose[i].Parent_Index].global_xform._44;
	//		index++;
	//		DebugLineData[index].xyzw.x = bind_pose[i].global_xform._41;
	//		DebugLineData[index].xyzw.y = bind_pose[i].global_xform._42;
	//		DebugLineData[index].xyzw.z = bind_pose[i].global_xform._43;
	//		DebugLineData[index].xyzw.w = bind_pose[i].global_xform._44;
	//		index++;

	//	}

	//}
	//ZeroMemory(&bufferdescription, sizeof(D3D11_BUFFER_DESC));
	//bufferdescription.Usage = D3D11_USAGE_DYNAMIC;
	//bufferdescription.ByteWidth = (UINT)(sizeof(SIMPLE_VERTEX) * DebugLineCount);
	//bufferdescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	//bufferdescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	//bufferdescription.MiscFlags = NULL;
	//bufferdescription.StructureByteStride = sizeof(SIMPLE_VERTEX);
	//ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	//InitData.pSysMem = DebugLineData;
	//device->CreateBuffer(&bufferdescription, &InitData, &pDebugLineBuffer);

#pragma endregion

#pragma region const buffer data init
	//const buff
	//camera data
	//static const XMVECTORF32 eye = { 0.0f, 0.0f, 5.5f, 0.0f };
	static const XMVECTORF32 eye = { 0.0f, 35.0f, 30.0f, 0.0f };
	//static const XMVECTORF32 eye = { 0.0f, 350.0f, 300.0f, 0.0f };
	//static const XMVECTORF32 eye = { 0.0f, 350.0f, 300.0f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMMATRIX m = XMMatrixLookAtRH(eye, at, up);
	XMStoreFloat4x4(&camera, XMMatrixInverse(NULL, m));
	save.LoadFromFile(camera);
	XMStoreFloat4x4(&send_to_ram.camView, XMMatrixTranspose(m));
	//use camera matrix in rather than this every frame
	//XMStoreFloat4(&send_to_ram.spot_light_pos, XMLoadFloat4(&XMFLOAT4(send_to_ram.camView.m[3])));
	//XMStoreFloat4(&send_to_ram.spot_light_dir, XMLoadFloat4(&XMFLOAT4(send_to_ram.camView.m[2])));
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

	m = XMMatrixIdentity();
	for (size_t i = 0; i < bind_pose.size(); i++)
	{
		m = XMLoadFloat4x4(&bind_pose[i].global_xform);
		m = XMMatrixInverse(nullptr, m);
		XMStoreFloat4x4(&send_to_ram2.InverseBindPose[i], m);
	}
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
	device->CreateVertexShader(BasicShader, sizeof(BasicShader), NULL, &Basicvertexshader);
	device->CreatePixelShader(BasicPixelShader, sizeof(BasicPixelShader), NULL, &Basicpixelshader);

	D3D11_INPUT_ELEMENT_DESC vertlayout[] =
	{
		"POSITION", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0,
		"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0,
		"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0,
		"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0,
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
	if (!animationPaused)
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

	send_to_ram.spot_light_pos.x = 0.0f;
	send_to_ram.spot_light_pos.y = 1.0f;
	send_to_ram.spot_light_pos.z = 0.0f;

	send_to_ram.spot_light_dir.x = 0.0f;
	send_to_ram.spot_light_dir.y = -1.0f;
	send_to_ram.spot_light_dir.z = 0.0f;

	if (!imput.buttons[VK_LEFT])
		imput.bLeft = false;
	if (imput.bLeft == false && imput.buttons[VK_LEFT])
	{
		if (animationPaused)
			if (--keyframeAnimIndex < 0)
				keyframeAnimIndex = keyFrameCount - 1;
		imput.bLeft = true;
	}

	if (!imput.buttons[VK_RIGHT])
		imput.bRight = false;
	if (imput.bRight == false && imput.buttons[VK_RIGHT])
	{
		if (animationPaused)
			if (++keyframeAnimIndex > (int)keyFrameCount - 1)
				keyframeAnimIndex = 0;
		imput.bRight = true;
	}

	if (!imput.buttons[VK_SPACE])
		imput.bSpace = false;
	if (imput.bSpace == false && imput.buttons[VK_SPACE])
	{
		animationPaused = !animationPaused;
		imput.bSpace = true;
	}
	if (!imput.buttons[VK_HOME])
		imput.bHome = false;
	if (imput.bHome == false && imput.buttons[VK_HOME])
	{
		save.SaveToFile(camera);
		imput.bHome = true;
	}
	if (!imput.buttons[VK_TAB])
		imput.bTab = false;
	if (imput.bTab == false && imput.buttons[VK_TAB])
	{
		RenderWireFrame = !RenderWireFrame;
		imput.bTab = true;
	}

#pragma endregion

#pragma region settings for all draw calls
	//float color[4]{ 0.87f, 0.87f, 1.0f, 0.0f };
	float color[4]{ 0.0f, 0.0f, 0.0f, 0.0f };

	context->OMSetRenderTargets(1, &rtv, depthStencilView);
	context->ClearRenderTargetView(rtv, color);
	context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->RSSetViewports(1, &viewport);
	//context->VSSetShader(vertexshader, NULL, NULL);
	context->VSSetConstantBuffers(0, 1, &constBuffer);
	context->VSSetConstantBuffers(1, 1, &modelAnimationConstBuffer);
	//context->PSSetShader(pixelshader, NULL, NULL);
	context->IASetInputLayout(layout);

	//context->PSSetShaderResources(0, 1, &pModelTexture);
	ZeroMemory(&mapResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	context->Map(constBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
	memcpy(mapResource.pData, &send_to_ram, sizeof(VRAM));
	context->Unmap(constBuffer, 0);

#pragma endregion

#pragma region draw model

	if (!animationPaused)
		for (size_t i = 0; i < keyFrameCount; i++)
			if (currAnimTime > IdleAnimationData->Frames[i].Time)
				keyframeAnimIndex = (unsigned int)i;

	ZeroMemory(twoKeyFrameTimes, sizeof(double) * 2);
	twoKeyFrameTimes[0] = IdleAnimationData->Frames[keyframeAnimIndex].Time;
	if ((unsigned)keyframeAnimIndex + 1 < keyFrameCount)
		twoKeyFrameTimes[1] = IdleAnimationData->Frames[keyframeAnimIndex + 1].Time;
	else
		twoKeyFrameTimes[1] = IdleAnimationData->Frames[0].Time;

	double ratio = (currAnimTime - twoKeyFrameTimes[0]) / (twoKeyFrameTimes[1] - twoKeyFrameTimes[0]);
	XMMATRIX m1 = XMMatrixIdentity(), m2 = XMMatrixIdentity(), m3 = XMMatrixIdentity();
	if (!animationPaused)
	{
		XMFLOAT4 new_pos{};
		XMFLOAT4X4 new_rot{};
		XMVECTOR from_pos{}, to_pos{}, from_rotation{}, to_rotation{}, real_time_rotation{};
		int index = 0;

		for (size_t i = 0; i < boneCount; i++)
		{
			from_rotation = XMQuaternionRotationMatrix(XMLoadFloat4x4(&IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]));
			from_pos = XMVectorSet(
				IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]._41,
				IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]._42,
				IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]._43,
				IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]._44);

			if ((unsigned)keyframeAnimIndex + 1 < keyFrameCount)
			{
				to_rotation = XMQuaternionRotationMatrix(XMLoadFloat4x4(&IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]));
				to_pos = XMVectorSet(
					IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]._41,
					IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]._42,
					IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]._43,
					IdleAnimationData->Frames[keyframeAnimIndex + 1].Joints[i]._44);
			}
			else
			{
				to_rotation = XMQuaternionRotationMatrix(XMLoadFloat4x4(&IdleAnimationData->Frames[0].Joints[i]));
				to_pos = XMVectorSet(
					IdleAnimationData->Frames[0].Joints[i]._41,
					IdleAnimationData->Frames[0].Joints[i]._42,
					IdleAnimationData->Frames[0].Joints[i]._43,
					IdleAnimationData->Frames[0].Joints[i]._44);
			}
			XMStoreFloat4x4(&new_rot, XMMatrixRotationQuaternion(XMQuaternionSlerp(from_rotation, to_rotation, (float)ratio)));
			XMStoreFloat4(&new_pos, XMVectorLerp(from_pos, to_pos, (float)ratio));

			send_to_ram2.RealTimePose[i]._11 = new_rot._11; send_to_ram2.RealTimePose[i]._12 = new_rot._12; send_to_ram2.RealTimePose[i]._13 = new_rot._13; send_to_ram2.RealTimePose[i]._14 = new_rot._14;
			send_to_ram2.RealTimePose[i]._21 = new_rot._21; send_to_ram2.RealTimePose[i]._22 = new_rot._22; send_to_ram2.RealTimePose[i]._23 = new_rot._23; send_to_ram2.RealTimePose[i]._24 = new_rot._24;
			send_to_ram2.RealTimePose[i]._31 = new_rot._31; send_to_ram2.RealTimePose[i]._32 = new_rot._32; send_to_ram2.RealTimePose[i]._33 = new_rot._33; send_to_ram2.RealTimePose[i]._34 = new_rot._34;
			send_to_ram2.RealTimePose[i]._41 = new_pos.x;	 send_to_ram2.RealTimePose[i]._42 = new_pos.y;  send_to_ram2.RealTimePose[i]._43 = new_pos.z;  send_to_ram2.RealTimePose[i]._44 = new_pos.w;
			
//#pragma region debug skeleton 
//
//			DebugPointData[i].xyzw = new_pos;
//			if (bind_pose[i].Parent_Index != -1)
//			{
//				DebugLineData[index].xyzw.x = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._41;
//				DebugLineData[index].xyzw.y = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._42;
//				DebugLineData[index].xyzw.z = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._43;
//				DebugLineData[index].xyzw.w = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._44;
//				DebugLineData[index].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
//				index++;
//				DebugLineData[index].xyzw.x = send_to_ram2.RealTimePose[i]._41;
//				DebugLineData[index].xyzw.y = send_to_ram2.RealTimePose[i]._42;
//				DebugLineData[index].xyzw.z = send_to_ram2.RealTimePose[i]._43;
//				DebugLineData[index].xyzw.w = send_to_ram2.RealTimePose[i]._44;
//				DebugLineData[index].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
//				index++;
//			}
//			else //Render line from origin to root node.
//			{
//				DebugLineData[index].xyzw.x = 0.0f;
//				DebugLineData[index].xyzw.y = 0.0f;
//				DebugLineData[index].xyzw.z = 0.0f;
//				DebugLineData[index].xyzw.w = 0.0f;
//				DebugLineData[index].color = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
//				index++;
//				DebugLineData[index].xyzw.x = send_to_ram2.RealTimePose[i]._41;
//				DebugLineData[index].xyzw.y = send_to_ram2.RealTimePose[i]._42;
//				DebugLineData[index].xyzw.z = send_to_ram2.RealTimePose[i]._43;
//				DebugLineData[index].xyzw.w = send_to_ram2.RealTimePose[i]._44;
//				DebugLineData[index].color = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
//				index++;
//			}
//
//#pragma endregion

			m1 = XMLoadFloat4x4(&send_to_ram2.InverseBindPose[i]);
			m2 = XMLoadFloat4x4(&send_to_ram2.RealTimePose[i]);
			m3 = m1 * m2;
			XMStoreFloat4x4(&send_to_ram2.RealTimePose[i], XMMatrixTranspose(m3));
		}
	}
	else
	{
		for (size_t i = 0; i < boneCount; i++)
		{
			m1 = XMLoadFloat4x4(&send_to_ram2.InverseBindPose[i]);
			m2 = XMLoadFloat4x4(&IdleAnimationData->Frames[keyframeAnimIndex].Joints[i]);
			m3 = m1 * m2;
			XMStoreFloat4x4(&send_to_ram2.RealTimePose[i], XMMatrixTranspose(m3));
			/*DebugPointData[i].xyzw.x = send_to_ram2.RealTimePose[i]._41;
			DebugPointData[i].xyzw.y = send_to_ram2.RealTimePose[i]._42;
			DebugPointData[i].xyzw.z = send_to_ram2.RealTimePose[i]._43;
			DebugPointData[i].xyzw.w = send_to_ram2.RealTimePose[i]._44;*/
		}
	}

	ZeroMemory(&mapResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	context->Map(modelAnimationConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
	memcpy(mapResource.pData, &send_to_ram2, sizeof(ANIMATION_VRAM));
	context->Unmap(modelAnimationConstBuffer, 0);

	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetVertexBuffers(0, 1, &modelvertbuffer, &stride, &offset);
	context->IASetIndexBuffer(modelindexbuffer, DXGI_FORMAT_R32_UINT, offset);
	context->VSSetShader(vertexshader, NULL, NULL);
	context->PSSetShader(pixelshader, NULL, NULL);
	context->PSSetShaderResources(0, 1, &pModelTexture);
	if (!RenderWireFrame)
		context->RSSetState(wireFrameRasterizerState);
	else
		context->RSSetState(SolidRasterizerState);
	context->DrawIndexed(modelindexCount, 0, 0);

#pragma endregion

#pragma region debug

	/*ZeroMemory(&mapResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	context->Map(pDebugPointBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
	memcpy(mapResource.pData, &DebugPointData, sizeof(SIMPLE_VERTEX) * DebugPointCount);
	context->Unmap(pDebugPointBuffer, 0);

	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	context->IASetVertexBuffers(0, 1, &pDebugPointBuffer, &stride, &offset);
	context->RSSetState(wireFrameRasterizerState);
	context->VSSetShader(Basicvertexshader, NULL, NULL);
	context->PSSetShader(Basicpixelshader, NULL, NULL);
	context->Draw(DebugPointCount, 0);
*/
	//int index = 0;
	//for (size_t i = 0; i < bind_pose.size(); i++)
	//{
	//	if (bind_pose[i].Parent_Index != -1)
	//	{
	//		DebugLineData[index].xyzw.x = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._41;
	//		DebugLineData[index].xyzw.y = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._42;
	//		DebugLineData[index].xyzw.z = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._43;
	//		DebugLineData[index].xyzw.w = send_to_ram2.RealTimePose[bind_pose[i].Parent_Index]._44;
	//		DebugLineData[index].color = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
	//		index++;
	//		DebugLineData[index].xyzw.x = send_to_ram2.RealTimePose[i]._41;
	//		DebugLineData[index].xyzw.y = send_to_ram2.RealTimePose[i]._42;
	//		DebugLineData[index].xyzw.z = send_to_ram2.RealTimePose[i]._43;
	//		DebugLineData[index].xyzw.w = send_to_ram2.RealTimePose[i]._44;
	//		DebugLineData[index].color = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	//		index++;
	//	}
	//	else //Render line from origin to root node.
	//	{
	//		DebugLineData[index].xyzw.x = 0.0f;
	//		DebugLineData[index].xyzw.y = 0.0f;
	//		DebugLineData[index].xyzw.z = 0.0f;
	//		DebugLineData[index].xyzw.w = 0.0f;
	//		DebugLineData[index].color = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	//		index++;
	//		DebugLineData[index].xyzw.x = send_to_ram2.RealTimePose[i]._41;
	//		DebugLineData[index].xyzw.y = send_to_ram2.RealTimePose[i]._42;
	//		DebugLineData[index].xyzw.z = send_to_ram2.RealTimePose[i]._43;
	//		DebugLineData[index].xyzw.w = send_to_ram2.RealTimePose[i]._44;
	//		DebugLineData[index].color = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	//		index++;
	//	}
	//}

	//ZeroMemory(&mapResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	//context->Map(pDebugLineBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
	//memcpy(mapResource.pData, &DebugLineData, sizeof(SIMPLE_VERTEX) * DebugLineCount);
	//context->Unmap(pDebugLineBuffer, 0);

	//context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	//context->IASetVertexBuffers(0, 1, &pDebugLineBuffer, &stride, &offset);
	//context->RSSetState(wireFrameRasterizerState);
	//context->Draw(DebugLineCount, 0);

#pragma endregion

#pragma region draw ground
	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetVertexBuffers(0, 1, &groundvertbuffer, &stride, &offset);
	context->IASetIndexBuffer(groundindexbuffer, DXGI_FORMAT_R32_UINT, offset);
	context->RSSetState(SolidRasterizerState);
	context->VSSetShader(Basicvertexshader, NULL, NULL);
	context->PSSetShader(Basicpixelshader, NULL, NULL);
	//context->PSSetShaderResources(0, 1, &pModelTexture);
	context->DrawIndexed(groundindexCount, 0, 0);
#pragma endregion

	swapchain->Present(0, 0);
	return true;
}

bool DEMO_APP::ShutDown()
{
	save.SaveToFile(camera);
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

	//if (pDebugPointBuffer)
	//	pDebugPointBuffer->Release();
	//if (pDebugLineBuffer)
	//	pDebugLineBuffer->Release();


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

	tagTRACKMOUSEEVENT mouse_tracker; ZeroMemory(&mouse_tracker, sizeof(mouse_tracker));
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
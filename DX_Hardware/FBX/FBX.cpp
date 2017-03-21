// FBX.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "fbxsdk.h"
void InitializeSdkObjects(FbxManager * & pManager, FbxScene * &pScene)
{
	pManager = FbxManager::Create();
	FbxIOSettings * ios = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(ios);
	FbxString lPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(lPath.Buffer());
	pScene = FbxScene::Create(pManager, "MyScene");

}
__declspec(dllexport) void function(int &number)
{
	number = 100;
	FbxManager * mManager = nullptr;
	FbxScene * mScene = nullptr;
	bool bReturn = false;
	bool mHasAnimation = true;
	InitializeSdkObjects(mManager, mScene);
	if (mManager)
	{
		//bReturn = LoadScene(mManager, mScene, )
	}
}


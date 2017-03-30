// FBX.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "fbxsdk.h"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>
using namespace DirectX;
using namespace std;

#include "Containers.h"
#include "FBX.h"

void InitializeSdkObjects(FbxManager * & pManager, FbxScene * &pScene)
{
	pManager = FbxManager::Create();
	FbxIOSettings * ios = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(ios);
	FbxString lPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(lPath.Buffer());
	pScene = FbxScene::Create(pManager, "MyScene");

}
bool LoadScene(FbxManager * & pManager, FbxScene * & pScene, const char * pFileName)
{
	bool loaded = false;
	FbxImporter * lImporter = FbxImporter::Create(pManager, "");
	lImporter->Initialize(pFileName, -1, pManager->GetIOSettings());
	loaded = lImporter->Import(pScene);
	lImporter->Destroy();
	return loaded;
}
void GetControlPoints(FbxNode* node, unordered_map<unsigned int, ControlPoint*>&pControlPoints)
{
	FbxMesh* currMesh = node->GetMesh();
	unsigned int ctrlPointCount = currMesh->GetControlPointsCount();
	for (unsigned int i = 0; i < ctrlPointCount; i++)
	{
		ControlPoint* currCtrlPoint = new ControlPoint();
		XMFLOAT3 currPosition;
		pControlPoints[i] = currCtrlPoint;
	}
}
XMFLOAT4X4 FBXMatrixtoXMMatrix(FbxMatrix m)
{
	XMFLOAT4X4 rm;
	for (unsigned int i = 0; i < 4; i++)
		for (unsigned int j = 0; j < 4; j++)
		{
			rm.m[i][j] = (float)m[i][j];
			if (j == 2)
				rm.m[i][j] *= -1;
			if (i == 2)
				rm.m[i][j] *= -1;
		}
	return rm;
}
bool LoadBonesandAnimation(FbxNode * node, FbxScene* scene, Skeleton* & bones, vector<Bone> & bind_pose, unordered_map<unsigned int, ControlPoint*> & mControlPoints)
{
	FbxMesh * mesh = node->GetMesh();
	unsigned int numDeformers = mesh->GetDeformerCount();
	for (unsigned int deformerIndex = 0; deformerIndex < numDeformers; deformerIndex++)
	{
		FbxSkin* skin = reinterpret_cast<FbxSkin*>(mesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
		if (!skin)
			continue;
		FbxAnimStack* currAnimStack = scene->GetSrcObject<FbxAnimStack>(0);
		FbxString animStackName = currAnimStack->GetName();
		string animationName = animStackName.Buffer();
		FbxTakeInfo* takeInfo = scene->GetTakeInfo(animStackName);
		FbxTime start = takeInfo->mLocalTimeSpan.GetStart();
		FbxTime end = takeInfo->mLocalTimeSpan.GetStop();
		FbxLongLong mAnimationLength = end.GetFrameCount(FbxTime::eFrames24) - start.GetFrameCount(FbxTime::eFrames24) + 1;
		for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames24); i <= end.GetFrameCount(FbxTime::eFrames24); i++)
		{
			FbxTime currTime;
			currTime.SetFrame(i, FbxTime::eFrames24);
			KeyFrame* currAnim = new KeyFrame();
			currAnim->Time = (float)currTime.GetSecondDouble();
			FbxMatrix currentTransformOffset = node->EvaluateGlobalTransform(currTime);
			unsigned int numClusters = skin->GetClusterCount();
			for (unsigned int clusterIndex = 0; clusterIndex < numClusters; clusterIndex++)
			{
				FbxCluster* cluster = skin->GetCluster(clusterIndex);
				if (!cluster)
					continue;
				FbxAMatrix tranMat;
				FbxAMatrix tranMatLink;
				cluster->GetTransformMatrix(tranMat);
				cluster->GetTransformLinkMatrix(tranMatLink);
				FbxAMatrix global_bone = tranMatLink.Inverse() * tranMat;
				if (start.GetFrameCount(FbxTime::eFrames24) == i)
				{
					Bone newBone;
					newBone.matrix = FBXMatrixtoXMMatrix(global_bone);
					bind_pose.push_back(newBone);
					unsigned int numIndices = cluster->GetControlPointIndicesCount();
					for (unsigned int j = 0; j < numIndices; j++)
					{
						int ctrlPointIndex = cluster->GetControlPointIndices()[j];
						BlendingIndividualWeightPair currBlend;
						currBlend.mBlendingIndex = clusterIndex;
						currBlend.mBlendWeight = cluster->GetControlPointWeights()[j];
						mControlPoints[ctrlPointIndex]->mBlendingInfo.push_back(currBlend);
					}
				}
				FbxMatrix bone_mat = node->EvaluateGlobalTransform(currTime);
				XMFLOAT4X4 bone;
				bone = FBXMatrixtoXMMatrix(bone_mat.Inverse()*cluster->GetLink()->EvaluateGlobalTransform(currTime));
				Bone b;
				b.matrix = bone;
				currAnim->bones.push_back(b);
			}
			bones->joints.push_back(currAnim);
		}
	}
	BlendingIndividualWeightPair currBlendingIndexWeightPair;
	currBlendingIndexWeightPair.mBlendingIndex = 0;
	currBlendingIndexWeightPair.mBlendWeight = 0;
	for (auto itr = mControlPoints.begin(); itr != mControlPoints.end(); itr++)
	{
		for (unsigned int i = (unsigned int)itr->second->mBlendingInfo.size(); i <= 4; i++)
		{
			itr->second->mBlendingInfo.push_back(currBlendingIndexWeightPair);
		}
	}
	return true;
}
void GetNormals(FbxMesh* inMesh, int inCtrlPointIndex, int inVertexCounter, XMFLOAT3& outNormal)
{
	if (inMesh->GetElementNormalCount() < 1)
		throw exception("incalid normal number");
	FbxGeometryElementNormal* vertexNormal = inMesh->GetElementNormal(0);
	switch (vertexNormal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{

		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[2]);
			break;
		}
		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(inCtrlPointIndex);
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
			break;
		}
		default:
		{
			throw exception("Invalid Reference");
			break;
		}
		}
		break;
	}
	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[2]);
			break;
		}
		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(inVertexCounter);
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
			break;
		}
		default:
		{
			throw exception("Invalid Reference");
			break;
		}
		}
		break;
	}

	default:
	{
		throw exception("Invalid Reference");
		break;
	}
	}
}
void GetUVs(FbxMesh* inMesh, int inCtrlPointIndex, int inVertexCounter, XMFLOAT2 & outUV)
{
	if (inMesh->GetElementUVCount() < 1)
		throw exception("Inalid UV Number");
	FbxGeometryElementUV* vertexUV = inMesh->GetElementUV(0);
	switch (vertexUV->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{

		switch (vertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outUV.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(inCtrlPointIndex).mData[0]);
			outUV.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(inCtrlPointIndex).mData[1]);
			break;
		}
		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexUV->GetIndexArray().GetAt(inCtrlPointIndex);
			outUV.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[0]);
			outUV.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[1]);
			break;
		}
		default:
		{

			throw exception("invalid Reference");
			break;
		}
		}
		break;
	}
	case FbxGeometryElement::eByPolygonVertex:
	{

		switch (vertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outUV.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(inVertexCounter).mData[0]);
			outUV.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(inVertexCounter).mData[1]);
			break;
		}
		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexUV->GetIndexArray().GetAt(inVertexCounter);
			outUV.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[0]);
			outUV.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[1]);
			break;
		}
		default:
		{
			throw exception("invalid Reference");
			break;
		}
		}
		break;
	}
	default:
	{
		throw exception("invalid Reference");
		break;
	}
	}
}
void GetTangent(FbxMesh* inMesh, int inCtrlPointIndex, int inVertexCounter, XMFLOAT3& outTangent)
{
	if (inMesh->GetElementTangentCount() < 1)
	{
		outTangent = { 0,0,0 };
		return;
	}
	FbxGeometryElementTangent* vertexTangent = inMesh->GetElementTangent(0);
	switch (vertexTangent->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{

		switch (vertexTangent->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outTangent.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(inCtrlPointIndex).mData[0]);
			outTangent.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(inCtrlPointIndex).mData[1]);
			outTangent.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(inCtrlPointIndex).mData[2]);
			break;
		}
		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexTangent->GetIndexArray().GetAt(inCtrlPointIndex);
			outTangent.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[0]);
			outTangent.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[1]);
			outTangent.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[2]);
			break;
		}
		default:
		{
			throw exception("invalid reference");
			break;
		}
		}
		break;
	}
	case FbxGeometryElement::eByPolygonVertex:
	{

		switch (vertexTangent->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outTangent.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(inVertexCounter).mData[0]);
			outTangent.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(inVertexCounter).mData[1]);
			outTangent.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(inVertexCounter).mData[2]);
			break;
		}
		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexTangent->GetIndexArray().GetAt(inVertexCounter);
			outTangent.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[0]);
			outTangent.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[1]);
			outTangent.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[2]);
			break;
		}
		default:
		{
			throw exception("invalid reference");
			break;
		}
		}
		break;
	}
	default:
	{
		throw exception("invalid reference");
		break;
	}
	}
}
bool LoadMesh(FbxNode* node, unordered_map<unsigned int, ControlPoint*> mControlPoints, unsigned int& mTriangleCount, vector<Triangle>&mTriangles, vector<BlendingVertex>&mVertices, unsigned int& vertCount)
{
	FbxMesh* currMesh = node->GetMesh();
	FbxVector4* see = currMesh->GetControlPoints();
	mTriangleCount = currMesh->GetPolygonCount();
	int vertexCounter = 0;
	mTriangles.reserve(mTriangleCount);
	for (unsigned int i = 0; i < mTriangleCount; i++)
	{
		XMFLOAT3 normal[3];
		XMFLOAT2 UV[3];
		XMFLOAT3 tangent[3];
		Triangle currTriangle;
		mTriangles.push_back(currTriangle);
		for (unsigned int j = 0; j < 3; j++)
		{
			int ctrlPointIndex = currMesh->GetPolygonVertex(i, j);
			ControlPoint* currCtrlPoint = mControlPoints[ctrlPointIndex];
			GetNormals(currMesh, ctrlPointIndex, vertexCounter, normal[j]);
			int k = 0;
			GetUVs(currMesh, ctrlPointIndex, vertexCounter, UV[j]);
			GetTangent(currMesh, ctrlPointIndex, vertexCounter, tangent[j]);
			BlendingVertex temp;
			temp.mPosition = currCtrlPoint->mPosition;
			temp.mPosition.z *= -1;
			temp.mNormal = normal[j];
			temp.mNormal.z *= -1;
			temp.mUV = UV[j];
			temp.mUV.y = 1.0f - temp.mUV.y;
			temp.mTangent = tangent[j];
			temp.mTangent.z *= -1;
			for (unsigned int i = 0; i < currCtrlPoint->mBlendingInfo.size(); i++)
			{
				VertexBlending currBlendingInfo;
				currBlendingInfo.mBlendingIndex = currCtrlPoint->mBlendingInfo[i].mBlendingIndex;
				currBlendingInfo.mBlendWeight = currCtrlPoint->mBlendingInfo[i].mBlendWeight;
				temp.mVertexBlendingInfos.push_back(currBlendingInfo);
			}
			temp.SortBlendingInfoByWeight();
			mVertices.push_back(temp);
			mTriangles.back().indicies.push_back(vertexCounter);
			++vertexCounter;
		}
		vertCount = vertexCounter;
	}
	for (auto itr = mControlPoints.begin(); itr != mControlPoints.end(); itr++)
		delete itr->second;
	mControlPoints.clear();
	return true;
}
void ProcessGeometry(unordered_map<unsigned int, ControlPoint*>mControlPoints, Skeleton* mSkeleton, FbxNode* inNode, unsigned int &triCount, vector<Triangle> &triIndices, vector<BlendingVertex> &verts, bool hasAnimation, FbxScene* & mScene, vector<Bone> & bind_pose, unsigned int & vertCount)
{
	if (inNode->GetNodeAttribute())
	{
		switch (inNode->GetNodeAttribute()->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:
			GetControlPoints(inNode, mControlPoints);
			if (hasAnimation)
			{
				LoadBonesandAnimation(inNode, mScene, mSkeleton, bind_pose, mControlPoints);
			}
			LoadMesh(inNode, mControlPoints, triCount, triIndices, verts, vertCount);
			break;
		}
	}
	for (int i = 0; i < inNode->GetChildCount(); i++)
	{
		ProcessGeometry(mControlPoints, mSkeleton, inNode->GetChild(i), triCount, triIndices, verts, hasAnimation, mScene, bind_pose, vertCount);
	}
}
__declspec(dllexport) void function(char * fileName, char * outFileNameMesh, char * outFileNameBone, char * outFileNameAnimations)
{

	FbxManager * mManager = nullptr;
	FbxScene * mScene = nullptr;
	bool bReturn = false;
	bool mHasAnimation = true;
	InitializeSdkObjects(mManager, mScene);
	if (mManager)
	{
		bReturn = LoadScene(mManager, mScene, fileName);
		FbxGeometryConverter * converter = new FbxGeometryConverter(mManager);
		converter->Triangulate(mScene, true);
		Skeleton * mSkeleton = new Skeleton();
		vector<Bone> bind_pose;
		unsigned int vertCount = 0;
		//
		//
		//
		unordered_map<unsigned int, ControlPoint *> mControlPoints;
		unsigned int TriCount = 0;
		vector<Triangle> tris;
		vector<BlendingVertex> verts;
		ProcessGeometry(mControlPoints, mSkeleton, mScene->GetRootNode(), TriCount, tris, verts, mHasAnimation, mScene, bind_pose, vertCount);
		unsigned int KeyFrameCount = (unsigned int)mSkeleton->joints.size();
		unsigned int bindposeCount = (unsigned int)bind_pose.size();
		FILE* file = nullptr;
		fopen_s(&file, outFileNameMesh, "wb");
		if (file)
		{
			fwrite(&TriCount, sizeof(unsigned int), 1, file);
			fwrite(&vertCount, sizeof(unsigned int), 1, file);
			for (unsigned int i = 0; i < TriCount; i++)
			{
				fwrite(&tris[i].indicies[0], sizeof(unsigned int), 1, file);
				fwrite(&tris[i].indicies[1], sizeof(unsigned int), 1, file);
				fwrite(&tris[i].indicies[2], sizeof(unsigned int), 1, file);
			}
			for (unsigned int i = 0; i < vertCount; i++)
			{
				unsigned int blendInfoSize = (unsigned int)verts[i].mVertexBlendingInfos.size();
				fwrite(&verts[i].mPosition, sizeof(float), 3, file);
				fwrite(&verts[i].mNormal, sizeof(float), 3, file);
				fwrite(&verts[i].mUV, sizeof(float), 2, file);
				fwrite(&verts[i].mTangent, sizeof(float), 3, file);
				fwrite(&blendInfoSize, sizeof(unsigned int), 1, file);
				for (unsigned int j = 0; j < blendInfoSize; j++)
				{
					fwrite(&verts[i].mVertexBlendingInfos[j].mBlendingIndex, sizeof(unsigned int), 1, file);
					fwrite(&verts[i].mVertexBlendingInfos[j].mBlendWeight, sizeof(double), 1, file);
				}
			}
			fclose(file);
		}
		file = nullptr;
		fopen_s(&file, outFileNameAnimations, "wb");
		if (file)
		{
			fwrite(&KeyFrameCount, sizeof(unsigned int), 1, file);
			for (int i = 0; i < mSkeleton->joints.size(); i++)
			{
				unsigned int bone_size = (unsigned int)mSkeleton->joints[i]->bones.size();
				fwrite(&bone_size, sizeof(unsigned int), 1, file);
				fwrite(&mSkeleton->joints[i]->bones[0], sizeof(Bone), mSkeleton->joints[i]->bones.size(), file);
				fwrite(&mSkeleton->joints[i]->Time, sizeof(float), 1, file);
			}
			fclose(file);
		}
		file = nullptr;
		fopen_s(&file, outFileNameBone, "wb");
		if (file)
		{
			fwrite(&bindposeCount, sizeof(unsigned int), 1, file);
			if (bindposeCount > 0)
			{
				fwrite(&bind_pose[0], sizeof(Bone), bindposeCount, file);
			}
			fclose(file);
		}
		delete converter;
		mScene->Destroy();
		mManager->Destroy();
		tris.clear();
		verts.clear();
		for (int i = 0; i < mSkeleton->joints.size(); i++)
		{
			mSkeleton->joints[i]->bones.clear();
		}
		mSkeleton->joints.clear();
	}
}

__declspec(dllexport) bool functionality(char * inFileNameMesh, char * inFileNameBone, char * inFileNameAnimations, unsigned int & triCount, vector<unsigned int>& triIndices, vector<BlendingVertex>& verts, Skeleton* & mSkeleton, vector<Bone>& bind_pose)
{
	FILE * f;
	bool bReturn = false;
	fopen_s(&f, inFileNameMesh, "rb");
	if (f)
	{
		bReturn = true;
		triCount = 0;
		int vertCount = 0;
		fread(&triCount, sizeof(unsigned int), 1, f);
		fread(&vertCount, sizeof(unsigned int), 1, f);
		triIndices.resize(triCount * 3);
		fread(&triIndices[0], sizeof(unsigned int), triCount * 3, f);
		verts.resize(vertCount);
		for (unsigned int i = 0; i < (unsigned)vertCount; i++)
		{
			unsigned int blendInfoSize = 0;
			BlendingVertex t;
			fread(&t.mPosition, sizeof(float), 3, f);
			fread(&t.mNormal, sizeof(float), 3, f);
			fread(&t.mUV, sizeof(float), 2, f);
			fread(&t.mTangent, sizeof(float), 3, f);
			fread(&blendInfoSize, sizeof(unsigned int), 1, f);
			for (unsigned int j = 0; j < blendInfoSize; j++)
			{
				VertexBlending tb;
				fread(&tb.mBlendingIndex, sizeof(unsigned int), 1, f);
				fread(&tb.mBlendWeight, sizeof(double), 1, f);
				t.mVertexBlendingInfos.push_back(tb);
			}
			verts.push_back(t);
		}
		fclose(f);
	}
	f = nullptr;
	fopen_s(&f, inFileNameBone, "rb");
	if (f)
	{
		unsigned int bindCount = 0;
		fread(&bindCount, sizeof(unsigned int), 1, f);
		bind_pose.resize(bindCount);
		fread(&bind_pose[0], sizeof(Bone), bindCount, f);
		fclose(f);
	}
	f = nullptr;
	fopen_s(&f, inFileNameAnimations, "rb");
	if (f)
	{
		unsigned int frameCount = 0;
		fread(&frameCount, sizeof(unsigned int), 1, f);
		mSkeleton->joints.resize(frameCount);
		for (unsigned int i = 0; i < frameCount; i++)
		{
			KeyFrame * t = new KeyFrame();
			unsigned int bone_size = 0;
			fread(&bone_size, sizeof(unsigned int), 1, f);
			t->bones.resize(bone_size);
			fread(&t->bones[0], sizeof(Bone), bone_size, f);
			fread(&t->Time, sizeof(float), 1, f);
			mSkeleton->joints[i] = t;
		}
		fclose(f);
	}
	return bReturn;
}


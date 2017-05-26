// FBX.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "fbxsdk.h"
#include <vector>
#include <DirectXMath.h>
using namespace DirectX;
using namespace std;
using namespace fbxsdk;
#include "Containers.h"
#include "FBX.h"


struct my_fbx_joint { char Name[32]; int Parent_Index; FbxNode * pNode; };
void DepthFirstSearch(FbxNode * pNode, vector<my_fbx_joint> & Container)
{
	my_fbx_joint * Joint = new my_fbx_joint;
	Joint->pNode = pNode;
	for (size_t i = 0; i < 32; i++)
		if (pNode->GetName()[i] != NULL)
			Joint->Name[i] = pNode->GetName()[i];
		else
		{
			Joint->Name[i] = NULL;
			break;
		}
	Container.push_back(*Joint);
	int childCount = pNode->GetChildCount();
	for (int i = 0; i < childCount; i++)
		DepthFirstSearch(pNode->GetChild((int)i), Container);
}
__declspec(dllexport) void function(char * fileName, char * outFileNameMesh, char * outFileNameBone, char * outFileNameAnimations, anim_clip* & animation, vert_pos_skinned* & FileMesh, unsigned int & VertCount, unsigned int* & VertIndices, unsigned int & IndicesCount)
{
	FbxManager * pManager = FbxManager::Create();
	FbxIOSettings * pIOSettings = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(pIOSettings);
	FbxString LPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(LPath.Buffer());
	FbxScene * pScene = FbxScene::Create(pManager, "MyScene");
	FbxImporter * lImporter = FbxImporter::Create(pManager, "");
	lImporter->Initialize(fileName, -1, pManager->GetIOSettings());
	lImporter->Import(pScene);
	lImporter->Destroy();
	unsigned int PoseCount = pScene->GetPoseCount();
	FbxPose * pBindPose = pScene->GetPose(0);
	unsigned int JointCount = pBindPose->GetCount();
	FbxSkeleton * pSkeleton = nullptr;
	int count = 0;
	do
	{
		FbxNode * pNode = pBindPose->GetNode(count++);
		pSkeleton = pNode->GetSkeleton();
	} while (!pSkeleton);
	FbxNode * pNode = pSkeleton->GetNode(0);
	//The inner loop can be reversed to redude the amount of nodes to resrch through//////
	vector<my_fbx_joint> arrBindPose;
	DepthFirstSearch(pNode, arrBindPose);
	arrBindPose[0].Parent_Index = -1;
	for (size_t i = 1; i < arrBindPose.size(); i++)
		for (size_t j = 0; j < arrBindPose.size(); j++)
			if (arrBindPose[i].pNode->GetParent() == arrBindPose[j].pNode)
				arrBindPose[i].Parent_Index = (int)j;
	vector<joint> arrTransforms;
	for (size_t i = 0; i < arrBindPose.size(); i++)
	{
		joint * pJoint = new joint;
		pJoint->Parent_Index = arrBindPose[i].Parent_Index;
		//x-rot
		pJoint->global_xform._11 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[0][0];
		pJoint->global_xform._12 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[0][1];
		pJoint->global_xform._13 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[0][2];

		//y-rot
		pJoint->global_xform._21 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[1][0];
		pJoint->global_xform._22 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[1][1];
		pJoint->global_xform._23 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[1][2];

		//z-rot
		pJoint->global_xform._31 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[2][0];
		pJoint->global_xform._32 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[2][1];
		pJoint->global_xform._33 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[2][2];

		//scale
		pJoint->global_xform._14 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[0][3];
		pJoint->global_xform._24 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[1][3];
		pJoint->global_xform._34 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[2][3];

		//pos
		pJoint->global_xform._41 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[3][0];
		pJoint->global_xform._42 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[3][1];
		pJoint->global_xform._43 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[3][2];
		pJoint->global_xform._44 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[3][3];

		arrTransforms.push_back(*pJoint);
	}
	FbxAnimStack * pAnimStack = pScene->GetCurrentAnimationStack();
	FbxTimeSpan TimeSpan = pAnimStack->GetLocalTimeSpan();
	FbxTime Time = TimeSpan.GetDuration();
	FbxLongLong FrameCount = Time.GetFrameCount(FbxTime::EMode::eFrames24);
	animation->Duration = Time.GetSecondDouble();
	for (size_t FrameIndex = 1; FrameIndex < (size_t)FrameCount; FrameIndex++)
	{
		keyframe TheKeyFrame;
		Time.SetFrame(FrameIndex, FbxTime::EMode::eFrames24);
		TheKeyFrame.Time = Time.GetSecondDouble();
		for (size_t JointIndex = 0; JointIndex < arrBindPose.size(); JointIndex++)
		{
			XMFLOAT4X4 * pJoint = new XMFLOAT4X4;
			//x-rot
			pJoint->_11 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[0][0];
			pJoint->_12 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[0][1];
			pJoint->_13 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[0][2];

			//y-rot
			pJoint->_21 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[1][0];
			pJoint->_22 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[1][1];
			pJoint->_23 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[1][2];

			//z-rot
			pJoint->_31 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[2][0];
			pJoint->_32 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[2][1];
			pJoint->_33 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[2][2];

			//scale
			pJoint->_14 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[0][3];
			pJoint->_24 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[1][3];
			pJoint->_34 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[2][3];

			//pos
			pJoint->_41 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[3][0];
			pJoint->_42 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[3][1];
			pJoint->_43 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[3][2];
			pJoint->_44 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[3][3];
			TheKeyFrame.Joints.push_back(*pJoint);
		}
		animation->Frames.push_back(TheKeyFrame);
	}
	FbxMesh * pMesh = nullptr;
	count = 0;
	do
	{
		FbxNode * pNode = pBindPose->GetNode(count++);
		pMesh = pNode->GetMesh();
	} while (!pMesh);
	vert_pos_skinned * pTheMeshVerts = new vert_pos_skinned[pMesh->GetControlPointsCount()]{};
	IndicesCount = (unsigned int)pMesh->GetPolygonVertexCount();
	VertIndices = (unsigned int *)pMesh->GetPolygonVertices();
	VertCount = (unsigned int)pMesh->GetControlPointsCount();

	FbxGeometryElementNormal * pNormalElement = pMesh->GetElementNormal(0);
	FbxGeometryElementUV * pUVElement = pMesh->GetElementUV(0);
	
	FbxVector4 CurrentNormal;
	FbxVector2 CurrentUV;

	for (int i = 0; i < pMesh->GetControlPointsCount(); i++)
	{
		FbxVector4 v = pMesh->GetControlPointAt(i);
		pTheMeshVerts[i].pos.x = (float)v.mData[0];
		pTheMeshVerts[i].pos.y = (float)v.mData[1];
		pTheMeshVerts[i].pos.z = (float)v.mData[2];
		pTheMeshVerts[i].pos.w = (float)v.mData[3];

		int NormalIndex = i;
		if (pNormalElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
			NormalIndex = pNormalElement->GetIndexArray().GetAt(i);

		CurrentNormal = pNormalElement->GetDirectArray().GetAt(NormalIndex);
		pTheMeshVerts[i].norm.x = (float)CurrentNormal[0];
		pTheMeshVerts[i].norm.y = (float)CurrentNormal[1];
		pTheMeshVerts[i].norm.z = (float)CurrentNormal[2];

		int UVIndex = i;
		if (pUVElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
			UVIndex = pUVElement->GetIndexArray().GetAt(i);
		
		CurrentUV = pUVElement->GetDirectArray().GetAt(UVIndex);
		pTheMeshVerts[i].uv.x = (float)CurrentUV[0];
		pTheMeshVerts[i].uv.y = (float)CurrentUV[1];

	}


	FbxSkin * pSkin = (FbxSkin*)pMesh->GetDeformer(0);
	int clusterCount = pSkin->GetClusterCount();
	for (int ClusterIndex = 0; ClusterIndex < clusterCount; ClusterIndex++)
	{
		FbxCluster *pCluster = pSkin->GetCluster(ClusterIndex);
		FbxNode * pLinkedNode = pCluster->GetLink();
		FbxNode * pIndexNode = nullptr;
		count = 0;
		do
		{
			pIndexNode = arrBindPose[count++].pNode;
		} while (pLinkedNode != pIndexNode);
		count--;
		for (int j = 0; j < arrBindPose.size(); j++)
		{
			pLinkedNode = pCluster->GetLink();
			pIndexNode = arrBindPose[j].pNode;
			if (pLinkedNode == pIndexNode)
			{
				count = j;
				break;
			}
		}
		double * pWeights = pCluster->GetControlPointWeights();
		int * pControllPointIndices = pCluster->GetControlPointIndices();
		size_t ControlPointIndicesCount = pCluster->GetControlPointIndicesCount();
		for (size_t affectedVertIndex = 0; affectedVertIndex < ControlPointIndicesCount; affectedVertIndex++)
		{
			pTheMeshVerts[pControllPointIndices[affectedVertIndex]].joints.push_back(count);
			pTheMeshVerts[pControllPointIndices[affectedVertIndex]].weights.push_back((float)pWeights[affectedVertIndex]);
		}
	}
	FileMesh = pTheMeshVerts;
	FILE* file = nullptr;
	file = nullptr;
	fopen_s(&file, outFileNameBone, "wb");
	if (file)
	{
		unsigned int count = (unsigned int)arrTransforms.size();
		fwrite(&count, sizeof(unsigned int), 1, file);
		if (count > 0)
			fwrite(&arrTransforms[0], sizeof(joint), count, file);
		fclose(file);
	}

}
__declspec(dllexport) void functionality(char * inFileNameMesh, char * inFileNameBone, char * inFileNameAnimations, vector<joint>& bind_pose)
{
	FILE * f = nullptr;
	f = nullptr;
	fopen_s(&f, inFileNameBone, "rb");
	if (f)
	{
		unsigned int bindCount = 0;
		fread(&bindCount, sizeof(unsigned int), 1, f);
		bind_pose.resize(bindCount);
		fread(&bind_pose[0], sizeof(joint), bindCount, f);
		fclose(f);
	}
}


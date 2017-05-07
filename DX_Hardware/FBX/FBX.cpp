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
		currPosition.x = (float)currMesh->GetControlPointAt(i).mData[0];
		currPosition.y = (float)currMesh->GetControlPointAt(i).mData[1];
		currPosition.z = (float)currMesh->GetControlPointAt(i).mData[2];
		currCtrlPoint->mPosition = currPosition;
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
			//temp.mPosition.z *= -1;
			temp.mNormal = normal[j];
			//temp.mNormal.z *= -1;
			temp.mUV = UV[j];
			//temp.mUV.y = 1.0f - temp.mUV.y;
			temp.mTangent = tangent[j];
			//temp.mTangent.z *= -1;
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
struct my_fbx_joint { FbxNode * pNode; int Parent_Index; };
void DepthFirstSearch(FbxNode * pNode, vector<my_fbx_joint> & Container, int Parent_Index)
{
	my_fbx_joint * Joint = new my_fbx_joint;
	Joint->pNode = pNode;
	Joint->Parent_Index = Parent_Index++;
	Container.push_back(*Joint);
	for (size_t i = 0; i < (size_t)pNode->GetChildCount(); i++)
		DepthFirstSearch(pNode->GetChild((int)i), Container, Parent_Index);
			
}
__declspec(dllexport) void function(char * fileName, char * outFileNameMesh, char * outFileNameBone, char * outFileNameAnimations, anim_clip* & animation, vector<vert_pos_skinned> & FileMesh)
{

	FbxManager * pManager = FbxManager::Create();
	FbxIOSettings * pIOSettings = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(pIOSettings);
	FbxString LPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(LPath.Buffer());
	FbxScene * pScene = FbxScene::Create(pManager, "MyScene");
	LoadScene(pManager, pScene, fileName);



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
	int ParentIndex = -1;
	vector<my_fbx_joint> arrBindPose;
	DepthFirstSearch(pNode, arrBindPose, ParentIndex);
	vector<joint> arrTransforms;
	for (size_t i = 0; i < arrBindPose.size(); i++)
	{
		joint * pJoint = new joint;
		pJoint->Parent_Index = arrBindPose[i].Parent_Index;
		pJoint->global_xform._11 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[0][0];
		pJoint->global_xform._12 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[0][1];
		pJoint->global_xform._13 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[0][2];
		pJoint->global_xform._14 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[0][3];
		pJoint->global_xform._21 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[1][0];
		pJoint->global_xform._22 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[1][1];
		pJoint->global_xform._23 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[1][2];
		pJoint->global_xform._24 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[1][3];
		pJoint->global_xform._31 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[2][0];
		pJoint->global_xform._32 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[2][1];
		pJoint->global_xform._33 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[2][2];
		pJoint->global_xform._34 = (float)arrBindPose[i].pNode->EvaluateGlobalTransform().mData[2][3];
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
			pJoint->_11 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[0][0];
			pJoint->_12 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[0][1];
			pJoint->_13 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[0][2];
			pJoint->_14 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[0][3];
			pJoint->_21 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[1][0];
			pJoint->_22 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[1][1];
			pJoint->_23 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[1][2];
			pJoint->_24 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[1][3];
			pJoint->_31 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[2][0];
			pJoint->_32 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[2][1];
			pJoint->_33 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[2][2];
			pJoint->_34 = (float)arrBindPose[JointIndex].pNode->EvaluateGlobalTransform(Time).mData[2][3];
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

	unsigned int ctrlPointCount = pMesh->GetControlPointsCount();
	vector<vert_pos_skinned> MeshVerts;

	for (unsigned int i = 0; i < ctrlPointCount; i++)
	{
		vert_pos_skinned* currCtrlPoint = new vert_pos_skinned();
		currCtrlPoint->pos.w = 1.0f;
		currCtrlPoint->pos.x = (float)pMesh->GetControlPointAt(i).mData[0];
		currCtrlPoint->pos.y = (float)pMesh->GetControlPointAt(i).mData[1];
		currCtrlPoint->pos.z = (float)pMesh->GetControlPointAt(i).mData[2];
		currCtrlPoint->joints[0] = 0;
		currCtrlPoint->joints[1] = 0;
		currCtrlPoint->joints[2] = 0;
		currCtrlPoint->joints[3] = 0;

		MeshVerts.push_back(*currCtrlPoint);
	}


	int DeformerCount = pMesh->GetDeformerCount();
	//FbxDeformer * Deformer = pMesh->GetDeformer(0);
	//FbxDeformer::EDeformerType DeformerType = Deformer->GetDeformerType();
	FbxSkin * pSkin = (FbxSkin*)pMesh->GetDeformer(0);
	int ClusterCount = pSkin->GetClusterCount();
	FbxNode * pIndexNode = nullptr;
	for (size_t ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
	{
		FbxCluster *pCluster = pSkin->GetCluster((int)ClusterIndex);
		FbxNode * pNode = pCluster->GetLink();
		count = 0;
		pIndexNode = nullptr;
		do
		{
			pIndexNode = arrBindPose[count++].pNode;
			//pIndexNode = pBindPose->GetNode(count++);
		} while (pNode != pIndexNode);
		count--;

		int ControlPointIndicesCount = pCluster->GetControlPointIndicesCount();

		double * pWeights = pCluster->GetControlPointWeights();
		int * pControllPointIndices = pCluster->GetControlPointIndices();
		for (size_t ControlPointIndex = 0; ControlPointIndex < ControlPointIndicesCount; ControlPointIndex++)
		{
	//		pWeights[pControllPointIndices[ControlPointIndex]];
//			MeshVerts[pControllPointIndices[ControlPointIndex]].joints;
		}
		
	}





	/////////////////////////////////////
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
		//unsigned int KeyFrameCount = (unsigned int)mSkeleton->joints.size();
		unsigned int KeyFrameCount = (unsigned int)animation->Frames.size();
		//unsigned int bindposeCount = (unsigned int)bind_pose.size();
		unsigned int bindposeCount = (unsigned int)arrTransforms.size();
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
			for (size_t i = 0; i < KeyFrameCount; i++)
			{
				//unsigned int bone_size = (unsigned int)mSkeleton->joints[i]->bones.size();
				unsigned int bone_size = (unsigned int)animation->Frames[i].Joints.size();
				fwrite(&bone_size, sizeof(unsigned int), 1, file);
				//fwrite(&mSkeleton->joints[i]->bones[0], sizeof(Bone), mSkeleton->joints[i]->bones.size(), file);
				fwrite(&animation->Frames[i].Joints[0], sizeof(joint), animation->Frames[i].Joints.size(), file);
				//fwrite(&mSkeleton->joints[i]->Time, sizeof(float), 1, file);
				fwrite(&animation->Frames[i].Time, sizeof(double), 1, file);
			}
			fclose(file);
		}
		file = nullptr;
		fopen_s(&file, outFileNameBone, "wb");
		if (file)
		{
			fwrite(&bindposeCount, sizeof(unsigned int), 1, file);
			if (bindposeCount > 0)
				fwrite(&arrTransforms[0], sizeof(joint), bindposeCount, file);
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
__declspec(dllexport) bool functionality(char * inFileNameMesh, char * inFileNameBone, char * inFileNameAnimations, unsigned int & triCount, vector<unsigned int>& triIndices, vector<BlendingVertex>& verts, vector<joint>& bind_pose)
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
		fread(&bind_pose[0], sizeof(joint), bindCount, f);
		fclose(f);
	}
	f = nullptr;
	fopen_s(&f, inFileNameAnimations, "rb");
	if (f)
	{
		unsigned int frameCount = 0;
		//fread(&frameCount, sizeof(unsigned int), 1, f);
		//animation->Frames.resize(frameCount);
		for (unsigned int i = 0; i < frameCount; i++)
		{
			//keyframe * t = new keyframe();
			//unsigned int bone_size = 0;
			//fread(&bone_size, sizeof(unsigned int), 1, f);
			//t->Joints.resize(bone_size);
			//fread(&t->Joints[0], sizeof(joint), bone_size, f);
			//fread(&t->Time, sizeof(double), 1, f);
			//animation->Frames[i] = *t;
		}
		fclose(f);
	}
	return bReturn;
}


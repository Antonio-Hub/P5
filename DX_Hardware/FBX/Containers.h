#pragma once
#include <algorithm>
struct joint { XMFLOAT4X4 global_xform; int Parent_Index; };
struct keyframe { double Time; vector<XMFLOAT4X4> Joints; };
struct anim_clip { double Duration; vector<keyframe> Frames; };
struct vert_pos_skinned { XMFLOAT4 pos; vector<int> joints; vector<float> weights; };

struct Bone
{
	XMFLOAT4X4 matrix;
};
struct KeyFrame
{
	float Time;
	vector<Bone> bones;
};
struct Skeleton
{
	vector<KeyFrame*> joints;
};
struct BlendingIndividualWeightPair
{
	unsigned int mBlendingIndex;
	double mBlendWeight;
	BlendingIndividualWeightPair() :mBlendingIndex(0), mBlendWeight(0) {}
};
struct ControlPoint
{
	XMFLOAT3 mPosition;
	vector<BlendingIndividualWeightPair> mBlendingInfo;
	ControlPoint() { mBlendingInfo.reserve(4); }
};
struct Triangle
{
	vector<unsigned int> indicies;
};
struct VertexBlending
{
	unsigned int mBlendingIndex;
	double mBlendWeight;
	VertexBlending() :mBlendingIndex(0), mBlendWeight(0.0f) {}
	bool operator < (const VertexBlending& rhs) { return (mBlendWeight > rhs.mBlendWeight); }
};
const XMFLOAT2 vector2Epsilon = XMFLOAT2(0.00001f, 0.00001f);
const XMFLOAT3 vector3Epsilon = XMFLOAT3(0.00001f, 0.00001f, 0.00001f);

struct BlendingVertex
{
	XMFLOAT3 mPosition;
	XMFLOAT3 mNormal;
	XMFLOAT2 mUV;
	XMFLOAT3 mTangent;
	vector<VertexBlending> mVertexBlendingInfos;
	void SortBlendingInfoByWeight()	{ sort(mVertexBlendingInfos.begin(), mVertexBlendingInfos.end()); }
	bool operator == (const BlendingVertex& rhs) const
	{
		bool sameBlendingInfo = true;
		if (!(mVertexBlendingInfos.empty() && rhs.mVertexBlendingInfos.empty()))
		{
			for (unsigned int i = 0; i < 4; i++)
			{
				if (mVertexBlendingInfos[i].mBlendingIndex != rhs.mVertexBlendingInfos[i].mBlendingIndex ||
					abs(mVertexBlendingInfos[i].mBlendWeight - rhs.mVertexBlendingInfos[i].mBlendWeight) > 0.001)
				{
					sameBlendingInfo = false;
					break;
				}
			}
		}
		bool firstResult = XMVector3NearEqual(XMLoadFloat3(&mPosition), XMLoadFloat3(&rhs.mPosition), XMLoadFloat3(&vector3Epsilon));
		bool secondResult = XMVector3NearEqual(XMLoadFloat3(&mNormal), XMLoadFloat3(&rhs.mNormal), XMLoadFloat3(&vector3Epsilon));
		bool thirdResult = XMVector3NearEqual(XMLoadFloat2(&mUV), XMLoadFloat2(&rhs.mUV), XMLoadFloat2(&vector2Epsilon));
		return firstResult && secondResult && thirdResult &&sameBlendingInfo;
	}
};
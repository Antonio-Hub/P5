#pragma once
#include <vector>
#include "Containers.h"

__declspec(dllexport) void function(char * fileName, char * outFileNameMesh, char * outFileNameBone, char * outFileNameAnimations, vector<joint> & Bind_Pose, anim_clip & Animation, vector<vert_pos_skinned> & FileMesh);
__declspec(dllexport) bool functionality(char * inFileNameMesh, char * inFileNameBone, char * inFileNameAnimations, unsigned int &triCount, vector<unsigned int> & triIndices, vector<BlendingVertex> & verts, Skeleton* & mSkeleton, vector<Bone> & bind_pose);


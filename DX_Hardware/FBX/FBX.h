#pragma once
#include <vector>
#include "Containers.h"

__declspec(dllexport) void function(char * fileName, char * outFileNameMesh, char * outFileNameBone, char * outFileNameAnimations, anim_clip* & animation, vector<vert_pos_skinned> & FileMesh);
__declspec(dllexport) bool functionality(char * inFileNameMesh, char * inFileNameBone, char * inFileNameAnimations, unsigned int &triCount, vector<unsigned int> & triIndices, vector<BlendingVertex> & verts, vector<joint> & bind_pose);


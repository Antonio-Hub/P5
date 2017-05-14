#pragma once
#include <vector>
#include "Containers.h"

__declspec(dllexport) void function(char * fileName, char * outFileNameMesh, char * outFileNameBone, char * outFileNameAnimations, anim_clip* & animation, vert_pos_skinned* & FileMesh, unsigned int & VertCount, unsigned int * & VertIndices,unsigned int & IndiceCount);
__declspec(dllexport) void functionality(char * inFileNameMesh, char * inFileNameBone, char * inFileNameAnimations, vector<joint> & bind_pose);


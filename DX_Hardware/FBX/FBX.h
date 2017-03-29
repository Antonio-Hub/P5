#pragma once
#include <vector>
#include "Containers.h"

__declspec(dllexport) void function(char * fileName, char * outFileNameMesh, char * outFileNameBine, char * outFileNameAnimations);
__declspec(dllexport) bool functionality(char * fileName, char * filenameBone, char * fileNameAnimation, unsigned int &triCount, vector<unsigned int> & triIndices, vector<BlendingVertex> & verts, Skeleton* & mSkeleton, vector<Bone> & bind_pose);


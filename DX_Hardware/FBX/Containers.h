#pragma once
struct joint { XMFLOAT4X4 global_xform; int Parent_Index; };
struct keyframe { double Time; vector<XMFLOAT4X4> Joints; };
struct anim_clip { double Duration; vector<keyframe> Frames; };
struct vert_pos_skinned { XMFLOAT4 pos; XMFLOAT4 norm; vector<int> joints; vector<float> weights; XMFLOAT2 uv; };

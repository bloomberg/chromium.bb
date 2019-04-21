#include <_mingw_unicode.h>
#undef INTERFACE
/*
 * Copyright 2011 Dylan Smith
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_D3DX9ANIM_H
#define __WINE_D3DX9ANIM_H

DEFINE_GUID(IID_ID3DXAnimationSet,           0x698cfb3f, 0x9289, 0x4d95, 0x9a, 0x57, 0x33, 0xa9, 0x4b, 0x5a, 0x65, 0xf9);
DEFINE_GUID(IID_ID3DXKeyframedAnimationSet,  0xfa4e8e3a, 0x9786, 0x407d, 0x8b, 0x4c, 0x59, 0x95, 0x89, 0x37, 0x64, 0xaf);
DEFINE_GUID(IID_ID3DXCompressedAnimationSet, 0x6cc2480d, 0x3808, 0x4739, 0x9f, 0x88, 0xde, 0x49, 0xfa, 0xcd, 0x8d, 0x4c);
DEFINE_GUID(IID_ID3DXAnimationController,    0xac8948ec, 0xf86d, 0x43e2, 0x96, 0xde, 0x31, 0xfc, 0x35, 0xf9, 0x6d, 0x9e);

typedef enum _D3DXMESHDATATYPE
{
    D3DXMESHTYPE_MESH        = 1,
    D3DXMESHTYPE_PMESH       = 2,
    D3DXMESHTYPE_PATCHMESH   = 3,
    D3DXMESHTYPE_FORCE_DWORD = 0x7fffffff,
} D3DXMESHDATATYPE;

typedef enum _D3DXCALLBACK_SEARCH_FLAGS
{
    D3DXCALLBACK_SEARCH_EXCLUDING_INITIAL_POSITION = 0x00000001,
    D3DXCALLBACK_SEARCH_BEHIND_INITIAL_POSITION    = 0x00000002,
    D3DXCALLBACK_SEARCH_FORCE_DWORD                = 0x7fffffff,
} D3DXCALLBACK_SEARCH_FLAGS;

typedef enum _D3DXPLAYBACK_TYPE
{
    D3DXPLAY_LOOP        = 0,
    D3DXPLAY_ONCE        = 1,
    D3DXPLAY_PINGPONG    = 2,
    D3DXPLAY_FORCE_DWORD = 0x7fffffff,
} D3DXPLAYBACK_TYPE;

typedef enum _D3DXCOMPRESSION_FLAGS
{
    D3DXCOMPRESSION_DEFAULT     = 0x00000000,
    D3DXCOMPRESSION_FORCE_DWORD = 0x7fffffff,
} D3DXCOMPRESSION_FLAGS;

typedef enum _D3DXPRIORITY_TYPE
{
    D3DXPRIORITY_LOW         = 0,
    D3DXPRIORITY_HIGH        = 1,
    D3DXPRIORITY_FORCE_DWORD = 0x7fffffff,
} D3DXPRIORITY_TYPE;

typedef enum _D3DXEVENT_TYPE
{
    D3DXEVENT_TRACKSPEED    = 0,
    D3DXEVENT_TRACKWEIGHT   = 1,
    D3DXEVENT_TRACKPOSITION = 2,
    D3DXEVENT_TRACKENABLE   = 3,
    D3DXEVENT_PRIORITYBLEND = 4,
    D3DXEVENT_FORCE_DWORD   = 0x7fffffff,
} D3DXEVENT_TYPE;

typedef enum _D3DXTRANSITION_TYPE
{
    D3DXTRANSITION_LINEAR        = 0,
    D3DXTRANSITION_EASEINEASEOUT = 1,
    D3DXTRANSITION_FORCE_DWORD   = 0x7fffffff,
} D3DXTRANSITION_TYPE;


typedef struct _D3DXMESHDATA
{
    D3DXMESHDATATYPE Type;

    union
    {
        LPD3DXMESH pMesh;
        LPD3DXPMESH pPMesh;
        LPD3DXPATCHMESH pPatchMesh;
    } DUMMYUNIONNAME;
} D3DXMESHDATA, *LPD3DXMESHDATA;

typedef struct _D3DXMESHCONTAINER
{
    LPSTR Name;
    D3DXMESHDATA MeshData;
    LPD3DXMATERIAL pMaterials;
    LPD3DXEFFECTINSTANCE pEffects;
    DWORD NumMaterials;
    DWORD *pAdjacency;
    LPD3DXSKININFO pSkinInfo;
    struct _D3DXMESHCONTAINER *pNextMeshContainer;
} D3DXMESHCONTAINER, *LPD3DXMESHCONTAINER;

typedef struct _D3DXFRAME
{
    LPSTR Name;
    D3DXMATRIX TransformationMatrix;
    LPD3DXMESHCONTAINER pMeshContainer;
    struct _D3DXFRAME *pFrameSibling;
    struct _D3DXFRAME *pFrameFirstChild;
} D3DXFRAME, *LPD3DXFRAME;

typedef struct _D3DXKEY_VECTOR3
{
    FLOAT Time;
    D3DXVECTOR3 Value;
} D3DXKEY_VECTOR3, *LPD3DXKEY_VECTOR3;

typedef struct _D3DXKEY_QUATERNION
{
    FLOAT Time;
    D3DXQUATERNION Value;
} D3DXKEY_QUATERNION, *LPD3DXKEY_QUATERNION;

typedef struct _D3DXKEY_CALLBACK
{
    FLOAT Time;
    LPVOID pCallbackData;
} D3DXKEY_CALLBACK, *LPD3DXKEY_CALLBACK;

typedef struct _D3DXTRACK_DESC
{
    D3DXPRIORITY_TYPE Priority;
    FLOAT Weight;
    FLOAT Speed;
    DOUBLE Position;
    WINBOOL Enable;
} D3DXTRACK_DESC, *LPD3DXTRACK_DESC;

typedef struct _D3DXEVENT_DESC
{
    D3DXEVENT_TYPE Type;
    UINT Track;
    DOUBLE StartTime;
    DOUBLE Duration;
    D3DXTRANSITION_TYPE Transition;
    union
    {
        FLOAT Weight;
        FLOAT Speed;
        DOUBLE Position;
        WINBOOL Enable;
    } DUMMYUNIONNAME;
} D3DXEVENT_DESC, *LPD3DXEVENT_DESC;

typedef DWORD D3DXEVENTHANDLE, *LPD3DXEVENTHANDLE;

typedef interface ID3DXAllocateHierarchy *LPD3DXALLOCATEHIERARCHY;
typedef interface ID3DXLoadUserData *LPD3DXLOADUSERDATA;
typedef interface ID3DXSaveUserData *LPD3DXSAVEUSERDATA;
typedef interface ID3DXAnimationSet *LPD3DXANIMATIONSET;
typedef interface ID3DXKeyframedAnimationSet *LPD3DXKEYFRAMEDANIMATIONSET;
typedef interface ID3DXCompressedAnimationSet *LPD3DXCOMPRESSEDANIMATIONSET;
typedef interface ID3DXAnimationCallbackHandler *LPD3DXANIMATIONCALLBACKHANDLER;
typedef interface ID3DXAnimationController *LPD3DXANIMATIONCONTROLLER;

#undef INTERFACE

#define INTERFACE ID3DXAllocateHierarchy
DECLARE_INTERFACE(ID3DXAllocateHierarchy)
{
    STDMETHOD(CreateFrame)(THIS_ LPCSTR Name, LPD3DXFRAME *new_frame) PURE;
    STDMETHOD(CreateMeshContainer)(THIS_ LPCSTR Name, CONST D3DXMESHDATA *mesh_data,
            CONST D3DXMATERIAL *materials, CONST D3DXEFFECTINSTANCE *effect_instances,
            DWORD num_materials, CONST DWORD *adjacency, LPD3DXSKININFO skin_info,
            LPD3DXMESHCONTAINER *new_mesh_container) PURE;
    STDMETHOD(DestroyFrame)(THIS_ LPD3DXFRAME frame) PURE;
    STDMETHOD(DestroyMeshContainer)(THIS_ LPD3DXMESHCONTAINER mesh_container) PURE;
};
#undef INTERFACE

#define INTERFACE ID3DXLoadUserData
DECLARE_INTERFACE(ID3DXLoadUserData)
{
    STDMETHOD(LoadTopLevelData)(LPD3DXFILEDATA child_data) PURE;
    STDMETHOD(LoadFrameChildData)(LPD3DXFRAME frame, LPD3DXFILEDATA child_data) PURE;
    STDMETHOD(LoadMeshChildData)(LPD3DXMESHCONTAINER mesh_container, LPD3DXFILEDATA child_data) PURE;
};
#undef INTERFACE

#define INTERFACE ID3DXSaveUserData
DECLARE_INTERFACE(ID3DXSaveUserData)
{
    STDMETHOD(AddFrameChildData)(CONST LPD3DXFRAME *frame, LPD3DXFILESAVEOBJECT save_obj,
            LPD3DXFILESAVEDATA frame_data) PURE;
    STDMETHOD(AddMeshChildData)(CONST LPD3DXMESHCONTAINER *mesh_container,
            LPD3DXFILESAVEOBJECT save_obj, LPD3DXFILESAVEDATA mesh_data) PURE;
    STDMETHOD(AddTopLevelDataObjectsPre)(LPD3DXFILESAVEOBJECT save_obj) PURE;
    STDMETHOD(AddTopLevelDataObjectsPost)(LPD3DXFILESAVEOBJECT save_obj) PURE;
    STDMETHOD(RegisterTemplates)(LPD3DXFILE xfile) PURE;
    STDMETHOD(SaveTemplates)(LPD3DXFILESAVEOBJECT save_obj) PURE;
};
#undef INTERFACE

#define INTERFACE ID3DXAnimationSet
DECLARE_INTERFACE_(ID3DXAnimationSet, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID* object) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /*** ID3DXAnimationSet methods ***/
    STDMETHOD_(LPCSTR, GetName)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetPeriod)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetPeriodicPosition)(THIS_ DOUBLE position) PURE;
    STDMETHOD_(UINT, GetNumAnimations)(THIS) PURE;
    STDMETHOD(GetAnimationNameByIndex)(THIS_ UINT index, LPCSTR *name) PURE;
    STDMETHOD(GetAnimationIndexByName)(THIS_ LPCSTR name, UINT *index) PURE;
    STDMETHOD(GetSRT)(THIS_ DOUBLE periodic_position, UINT animation, D3DXVECTOR3 *scale,
            D3DXQUATERNION *rotation, D3DXVECTOR3 *translation) PURE;
    STDMETHOD(GetCallback)(THIS_ DOUBLE position, DWORD flags, DOUBLE *callback_position,
            LPVOID *callback_data) PURE;
};
#undef INTERFACE

#define INTERFACE ID3DXKeyframedAnimationSet
DECLARE_INTERFACE_(ID3DXKeyframedAnimationSet, ID3DXAnimationSet)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID* object) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /*** ID3DXAnimationSet methods ***/
    STDMETHOD_(LPCSTR, GetName)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetPeriod)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetPeriodicPosition)(THIS_ DOUBLE position) PURE;
    STDMETHOD_(UINT, GetNumAnimations)(THIS) PURE;
    STDMETHOD(GetAnimationNameByIndex)(THIS_ UINT index, LPCSTR *name) PURE;
    STDMETHOD(GetAnimationIndexByName)(THIS_ LPCSTR name, UINT *index) PURE;
    STDMETHOD(GetSRT)(THIS_ DOUBLE periodic_position, UINT animation, D3DXVECTOR3 *scale,
            D3DXQUATERNION *rotation, D3DXVECTOR3 *translation) PURE;
    STDMETHOD(GetCallback)(THIS_ DOUBLE position, DWORD flags, DOUBLE *callback_position,
            LPVOID *callback_data) PURE;
    /*** ID3DXKeyframedAnimationSet methods ***/
    STDMETHOD_(D3DXPLAYBACK_TYPE, GetPlaybackType)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetSourceTicksPerSecond)(THIS) PURE;
    STDMETHOD_(UINT, GetNumScaleKeys)(THIS_ UINT animation) PURE;
    STDMETHOD(GetScaleKeys)(THIS_ UINT animation, LPD3DXKEY_VECTOR3 scale_keys) PURE;
    STDMETHOD(GetScaleKey)(THIS_ UINT animation, UINT key, LPD3DXKEY_VECTOR3 scale_key) PURE;
    STDMETHOD(SetScaleKey)(THIS_ UINT animation, UINT key, LPD3DXKEY_VECTOR3 scale_key) PURE;
    STDMETHOD_(UINT, GetNumRotationKeys)(THIS_ UINT animation) PURE;
    STDMETHOD(GetRotationKeys)(THIS_ UINT animation, LPD3DXKEY_QUATERNION rotation_keys) PURE;
    STDMETHOD(GetRotationKey)(THIS_ UINT animation, UINT key, LPD3DXKEY_QUATERNION rotation_key) PURE;
    STDMETHOD(SetRotationKey)(THIS_ UINT animation, UINT key, LPD3DXKEY_QUATERNION rotation_key) PURE;
    STDMETHOD_(UINT, GetNumTranslationKeys)(THIS_ UINT animation) PURE;
    STDMETHOD(GetTranslationKeys)(THIS_ UINT animation, LPD3DXKEY_VECTOR3 translation_keys) PURE;
    STDMETHOD(GetTranslationKey)(THIS_ UINT animation, UINT key, LPD3DXKEY_VECTOR3 translation_key) PURE;
    STDMETHOD(SetTranslationKey)(THIS_ UINT animation, UINT key, LPD3DXKEY_VECTOR3 translation_key) PURE;
    STDMETHOD_(UINT, GetNumCallbackKeys)(THIS) PURE;
    STDMETHOD(GetCallbackKeys)(THIS_ LPD3DXKEY_CALLBACK callback_keys) PURE;
    STDMETHOD(GetCallbackKey)(THIS_ UINT key, LPD3DXKEY_CALLBACK callback_key) PURE;
    STDMETHOD(SetCallbackKey)(THIS_ UINT key, LPD3DXKEY_CALLBACK callback_key) PURE;
    STDMETHOD(UnregisterScaleKey)(THIS_ UINT animation, UINT key) PURE;
    STDMETHOD(UnregisterRotationKey)(THIS_ UINT animation, UINT key) PURE;
    STDMETHOD(UnregisterTranslationKey)(THIS_ UINT animation, UINT key) PURE;
    STDMETHOD(RegisterAnimationSRTKeys)(THIS_ LPCSTR name, UINT num_scale_keys,
            UINT num_rotation_keys, UINT num_translation_keys, CONST D3DXKEY_VECTOR3 *scale_keys,
            CONST D3DXKEY_QUATERNION *rotation_keys, CONST D3DXKEY_VECTOR3 *translation_keys,
            DWORD *animation_index) PURE;
    STDMETHOD(Compress)(THIS_ DWORD flags, FLOAT lossiness, LPD3DXFRAME hierarchy,
            LPD3DXBUFFER *compressed_data) PURE;
    STDMETHOD(UnregisterAnimation)(THIS_ UINT index) PURE;
};
#undef INTERFACE

#define INTERFACE ID3DXCompressedAnimationSet
DECLARE_INTERFACE_(ID3DXCompressedAnimationSet, ID3DXAnimationSet)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID* object) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /*** ID3DXAnimationSet methods ***/
    STDMETHOD_(LPCSTR, GetName)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetPeriod)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetPeriodicPosition)(THIS_ DOUBLE position) PURE;
    STDMETHOD_(UINT, GetNumAnimations)(THIS) PURE;
    STDMETHOD(GetAnimationNameByIndex)(THIS_ UINT index, LPCSTR *name) PURE;
    STDMETHOD(GetAnimationIndexByName)(THIS_ LPCSTR name, UINT *index) PURE;
    STDMETHOD(GetSRT)(THIS_ DOUBLE periodic_position, UINT animation, D3DXVECTOR3 *scale,
            D3DXQUATERNION *rotation, D3DXVECTOR3 *translation) PURE;
    STDMETHOD(GetCallback)(THIS_ DOUBLE position, DWORD flags, DOUBLE *callback_position,
            LPVOID *callback_data) PURE;
    /*** ID3DXCompressedAnimationSet methods ***/
    STDMETHOD_(D3DXPLAYBACK_TYPE, GetPlaybackType)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetSourceTicksPerSecond)(THIS) PURE;
    STDMETHOD(GetCompressedData)(THIS_ LPD3DXBUFFER *compressed_data) PURE;
    STDMETHOD_(UINT, GetNumCallbackKeys)(THIS) PURE;
    STDMETHOD(GetCallbackKeys)(THIS_ LPD3DXKEY_CALLBACK callback_keys) PURE;
};
#undef INTERFACE

#define INTERFACE ID3DXAnimationCallbackHandler
DECLARE_INTERFACE(ID3DXAnimationCallbackHandler)
{
    STDMETHOD(HandleCallback)(THIS_ UINT track, LPVOID callback_data) PURE;
};
#undef INTERFACE

#define INTERFACE ID3DXAnimationController
DECLARE_INTERFACE_(ID3DXAnimationController, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID* object) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    /*** ID3DXAnimationController methods ***/
    STDMETHOD_(UINT, GetMaxNumAnimationOutputs)(THIS) PURE;
    STDMETHOD_(UINT, GetMaxNumAnimationSets)(THIS) PURE;
    STDMETHOD_(UINT, GetMaxNumTracks)(THIS) PURE;
    STDMETHOD_(UINT, GetMaxNumEvents)(THIS) PURE;
    STDMETHOD(RegisterAnimationOutput)(THIS_ LPCSTR name, D3DXMATRIX *matrix,
            D3DXVECTOR3 *scale, D3DXQUATERNION *rotation, D3DXVECTOR3 *translation) PURE;
    STDMETHOD(RegisterAnimationSet)(THIS_ LPD3DXANIMATIONSET anim_set) PURE;
    STDMETHOD(UnregisterAnimationSet)(THIS_ LPD3DXANIMATIONSET anim_set) PURE;
    STDMETHOD_(UINT, GetNumAnimationSets)(THIS) PURE;
    STDMETHOD(GetAnimationSet)(THIS_ UINT index, LPD3DXANIMATIONSET *anim_set) PURE;
    STDMETHOD(GetAnimationSetByName)(THIS_ LPCSTR name, LPD3DXANIMATIONSET *anim_set) PURE;
    STDMETHOD(AdvanceTime)(THIS_ DOUBLE time_delta, LPD3DXANIMATIONCALLBACKHANDLER *callback_handler) PURE;
    STDMETHOD(ResetTime)(THIS) PURE;
    STDMETHOD_(DOUBLE, GetTime)(THIS) PURE;
    STDMETHOD(SetTrackAnimationSet)(THIS_ UINT track, LPD3DXANIMATIONSET anim_set) PURE;
    STDMETHOD(GetTrackAnimationSet)(THIS_ UINT track, LPD3DXANIMATIONSET *anim_set) PURE;
    STDMETHOD(GetTrackPriority)(THIS_ UINT track, D3DXPRIORITY_TYPE *priority) PURE;
    STDMETHOD(SetTrackSpeed)(THIS_ UINT track, FLOAT speed) PURE;
    STDMETHOD(SetTrackWeight)(THIS_ UINT track, FLOAT weight) PURE;
    STDMETHOD(SetTrackPosition)(THIS_ UINT track, DOUBLE position) PURE;
    STDMETHOD(SetTrackEnable)(THIS_ UINT track, WINBOOL enable) PURE;
    STDMETHOD(SetTrackDesc)(THIS_ UINT track, LPD3DXTRACK_DESC desc) PURE;
    STDMETHOD(GetTrackDesc)(THIS_ UINT track, LPD3DXTRACK_DESC desc) PURE;
    STDMETHOD(SetPriorityBlend)(THIS_ FLOAT blend_weight) PURE;
    STDMETHOD_(FLOAT, GetPriorityBlend)(THIS) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, KeyTrackSpeed)(THIS_ UINT track, FLOAT new_speed,
            DOUBLE start_time, DOUBLE duration, D3DXTRANSITION_TYPE transition) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, KeyTrackWeight)(THIS_ UINT track, FLOAT new_weight,
            DOUBLE start_time, DOUBLE duration, D3DXTRANSITION_TYPE transition) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, KeyTrackPosition)(THIS_ UINT track, DOUBLE new_position, DOUBLE start_time) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, KeyTrackEnable)(THIS_ UINT track, WINBOOL new_enable, DOUBLE start_time) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, KeyPriorityBlend)(THIS_ FLOAT new_blend_weight,
            DOUBLE start_time, DOUBLE duration, D3DXTRANSITION_TYPE transition) PURE;
    STDMETHOD(UnkeyEvent)(THIS_ D3DXEVENTHANDLE event) PURE;
    STDMETHOD(UnkeyAllTrackEvents)(THIS_ UINT track) PURE;
    STDMETHOD(UnkeyAllPriorityBlends)(THIS) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, GetCurrentTrackEvent)(THIS_ UINT track, D3DXEVENT_TYPE event_type) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, GetCurrentPriorityBlend)(THIS) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, GetUpcomingTrackEvent)(THIS_ UINT track, D3DXEVENTHANDLE event) PURE;
    STDMETHOD_(D3DXEVENTHANDLE, GetUpcomingPriorityBlend)(THIS_ D3DXEVENTHANDLE handle) PURE;
    STDMETHOD(ValidateEvent)(THIS_ D3DXEVENTHANDLE event) PURE;
    STDMETHOD(GetEventDesc)(THIS_ D3DXEVENTHANDLE event, LPD3DXEVENT_DESC desc) PURE;
    STDMETHOD(CloneAnimationController)(THIS_ UINT max_num_anim_outputs, UINT max_num_anim_sets,
            UINT max_num_tracks, UINT max_num_events, LPD3DXANIMATIONCONTROLLER *anim_controller) PURE;
};
#undef INTERFACE

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DXLoadMeshHierarchyFromXA(LPCSTR, DWORD, LPDIRECT3DDEVICE9, LPD3DXALLOCATEHIERARCHY, LPD3DXLOADUSERDATA, LPD3DXFRAME*, LPD3DXANIMATIONCONTROLLER*);
HRESULT WINAPI D3DXLoadMeshHierarchyFromXW(LPCWSTR, DWORD, LPDIRECT3DDEVICE9, LPD3DXALLOCATEHIERARCHY, LPD3DXLOADUSERDATA, LPD3DXFRAME*, LPD3DXANIMATIONCONTROLLER*);
#define D3DXLoadMeshHierarchyFromX __MINGW_NAME_AW(D3DXLoadMeshHierarchyFromX)
HRESULT WINAPI D3DXLoadMeshHierarchyFromXInMemory(LPCVOID, DWORD, DWORD, LPDIRECT3DDEVICE9, LPD3DXALLOCATEHIERARCHY, LPD3DXLOADUSERDATA, LPD3DXFRAME*, LPD3DXANIMATIONCONTROLLER*);
HRESULT WINAPI D3DXSaveMeshHierarchyToFileA(LPCSTR, DWORD, CONST D3DXFRAME*, LPD3DXANIMATIONCONTROLLER, LPD3DXSAVEUSERDATA);
HRESULT WINAPI D3DXSaveMeshHierarchyToFileW(LPCWSTR, DWORD, CONST D3DXFRAME*, LPD3DXANIMATIONCONTROLLER, LPD3DXSAVEUSERDATA);
#define D3DXSaveMeshHierarchyToFile __MINGW_NAME_AW(D3DXSaveMeshHierarchyToFile)
HRESULT WINAPI D3DXFrameDestroy(LPD3DXFRAME, LPD3DXALLOCATEHIERARCHY);
HRESULT WINAPI D3DXFrameAppendChild(LPD3DXFRAME, CONST D3DXFRAME*);
LPD3DXFRAME WINAPI D3DXFrameFind(CONST D3DXFRAME*, LPCSTR);
HRESULT WINAPI D3DXFrameRegisterNamedMatrices(LPD3DXFRAME, LPD3DXANIMATIONCONTROLLER);
UINT WINAPI D3DXFrameNumNamedMatrices(CONST D3DXFRAME *frame_root);
HRESULT WINAPI D3DXFrameCalculateBoundingSphere(CONST D3DXFRAME*, LPD3DXVECTOR3, FLOAT*);
HRESULT WINAPI D3DXCreateKeyframedAnimationSet(LPCSTR, DOUBLE, D3DXPLAYBACK_TYPE, UINT, UINT, CONST D3DXKEY_CALLBACK*, LPD3DXKEYFRAMEDANIMATIONSET*);
HRESULT WINAPI D3DXCreateCompressedAnimationSet(LPCSTR, DOUBLE, D3DXPLAYBACK_TYPE, LPD3DXBUFFER, UINT, CONST D3DXKEY_CALLBACK*, LPD3DXCOMPRESSEDANIMATIONSET*);
HRESULT WINAPI D3DXCreateAnimationController(UINT, UINT, UINT, UINT, LPD3DXANIMATIONCONTROLLER*);

#ifdef __cplusplus
}
#endif

#endif /* __WINE_D3DX9ANIM_H */

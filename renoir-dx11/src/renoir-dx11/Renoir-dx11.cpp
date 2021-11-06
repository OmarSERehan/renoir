#include "renoir-dx11/Exports.h"

#include <renoir/Renoir.h>

#include <mn/Thread.h>
#include <mn/Pool.h>
#include <mn/Buf.h>
#include <mn/Defer.h>
#include <mn/Log.h>
#include <mn/Map.h>
#include <mn/Str.h>
#include <mn/Str_Intern.h>
#include <mn/Map.h>
#include <mn/Debug.h>
#include <mn/Assert.h>

#include <atomic>
#include <math.h>
#include <stdio.h>

#include <d3d11.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
#include <dxgi.h>

const char* DEPTH_RESOLVE_COMPUTE = R"""(
Texture2DMS<float> input: register(t0);
RWTexture2D<float> output: register(u0);

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
	uint2 dim;
	uint sampleCount;
	input.GetDimensions(dim.x, dim.y, sampleCount);

	float result = 1;
	for (uint i = 0; i < sampleCount; ++i)
		result = min(result, input.Load(dispatchThreadId.xy, i).r);
	output[dispatchThreadId.xy] = result;
}
)""";

inline static int
_renoir_buffer_type_to_dx(RENOIR_BUFFER type)
{
	switch(type)
	{
	case RENOIR_BUFFER_VERTEX: return D3D11_BIND_VERTEX_BUFFER;
	case RENOIR_BUFFER_UNIFORM: return D3D11_BIND_CONSTANT_BUFFER;
	case RENOIR_BUFFER_INDEX: return D3D11_BIND_INDEX_BUFFER;
	case RENOIR_BUFFER_COMPUTE: return D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	default: mn_unreachable(); return 0;
	}
}

inline static D3D11_USAGE
_renoir_usage_to_dx(RENOIR_USAGE usage)
{
	switch(usage)
	{
	case RENOIR_USAGE_STATIC: return D3D11_USAGE_IMMUTABLE;
	case RENOIR_USAGE_DYNAMIC: return D3D11_USAGE_DYNAMIC;
	default: mn_unreachable(); return D3D11_USAGE_DEFAULT;
	}
}

inline static int
_renoir_access_to_dx(RENOIR_ACCESS access)
{
	switch(access)
	{
	case RENOIR_ACCESS_NONE: return 0;
	case RENOIR_ACCESS_READ: return D3D11_CPU_ACCESS_READ;
	case RENOIR_ACCESS_WRITE: return D3D11_CPU_ACCESS_WRITE;
	case RENOIR_ACCESS_READ_WRITE: return D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
	default: mn_unreachable(); return 0;
	}
}

inline static DXGI_FORMAT
_renoir_pixelformat_to_dx(RENOIR_PIXELFORMAT format)
{
	switch(format)
	{
	case RENOIR_PIXELFORMAT_RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RENOIR_PIXELFORMAT_R16I: return DXGI_FORMAT_R16_SINT;
	case RENOIR_PIXELFORMAT_R16UI: return DXGI_FORMAT_R16_UINT;
	case RENOIR_PIXELFORMAT_R16F: return DXGI_FORMAT_R16_FLOAT;
	case RENOIR_PIXELFORMAT_R32F: return DXGI_FORMAT_R32_FLOAT;
	case RENOIR_PIXELFORMAT_R32G32F: return DXGI_FORMAT_R32G32_FLOAT;
	case RENOIR_PIXELFORMAT_R32G32B32A32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RENOIR_PIXELFORMAT_R16G16B16A16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case RENOIR_PIXELFORMAT_D24S8: return DXGI_FORMAT_R24G8_TYPELESS;
	case RENOIR_PIXELFORMAT_D32: return DXGI_FORMAT_R32_TYPELESS;
	case RENOIR_PIXELFORMAT_R8: return DXGI_FORMAT_R8_UNORM;
	default: mn_unreachable(); return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
}

inline static int
_renoir_pixelformat_to_size(RENOIR_PIXELFORMAT format)
{
	switch(format)
	{
	case RENOIR_PIXELFORMAT_RGBA8:
	case RENOIR_PIXELFORMAT_D32:
	case RENOIR_PIXELFORMAT_R32F:
	case RENOIR_PIXELFORMAT_D24S8:
		return 4;
	case RENOIR_PIXELFORMAT_R16I:
	case RENOIR_PIXELFORMAT_R16UI:
	case RENOIR_PIXELFORMAT_R16F:
		return 2;
	case RENOIR_PIXELFORMAT_R32G32F:
	case RENOIR_PIXELFORMAT_R16G16B16A16F:
		return 8;
	case RENOIR_PIXELFORMAT_R32G32B32A32F: return 16;
	case RENOIR_PIXELFORMAT_R8: return 1;
	default: mn_unreachable(); return 0;
	}
}

inline static bool
_renoir_pixelformat_is_depth(RENOIR_PIXELFORMAT format)
{
	return (format == RENOIR_PIXELFORMAT_D32 ||
			format == RENOIR_PIXELFORMAT_D24S8);
}

inline static DXGI_FORMAT
_renoir_pixelformat_depth_to_dx_srv(RENOIR_PIXELFORMAT format)
{
	switch(format)
	{
	case RENOIR_PIXELFORMAT_D24S8: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case RENOIR_PIXELFORMAT_D32: return DXGI_FORMAT_R32_FLOAT;
	default: mn_unreachable(); return (DXGI_FORMAT)0;
	}
}

inline static DXGI_FORMAT
_renoir_pixelformat_depth_to_dx_depth_view(RENOIR_PIXELFORMAT format)
{
	switch(format)
	{
	case RENOIR_PIXELFORMAT_D24S8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case RENOIR_PIXELFORMAT_D32: return DXGI_FORMAT_D32_FLOAT;
	default: mn_unreachable(); return (DXGI_FORMAT)0;
	}
}

inline static DXGI_FORMAT
_renoir_pixelformat_depth_to_dx_uav(RENOIR_PIXELFORMAT format)
{
	switch(format)
	{
	case RENOIR_PIXELFORMAT_D24S8: return DXGI_FORMAT_R32_FLOAT;
	case RENOIR_PIXELFORMAT_D32: return DXGI_FORMAT_R32_FLOAT;
	default: mn_unreachable(); return (DXGI_FORMAT)0;
	}
}

inline static D3D11_BLEND
_renoir_blend_to_dx(RENOIR_BLEND blend)
{
	switch(blend)
	{
	case RENOIR_BLEND_ZERO: return D3D11_BLEND_ZERO;
	case RENOIR_BLEND_ONE: return D3D11_BLEND_ONE;
	case RENOIR_BLEND_SRC_COLOR: return D3D11_BLEND_SRC_COLOR;
	case RENOIR_BLEND_ONE_MINUS_SRC_COLOR: return D3D11_BLEND_INV_SRC_COLOR;
	case RENOIR_BLEND_DST_COLOR: return D3D11_BLEND_DEST_COLOR;
	case RENOIR_BLEND_ONE_MINUS_DST_COLOR: return D3D11_BLEND_INV_DEST_COLOR;
	case RENOIR_BLEND_SRC_ALPHA: return D3D11_BLEND_SRC_ALPHA;
	case RENOIR_BLEND_ONE_MINUS_SRC_ALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
	default: mn_unreachable(); return D3D11_BLEND_ZERO;
	}
}

inline static D3D11_BLEND_OP
_renoir_blend_eq_to_dx(RENOIR_BLEND_EQ eq)
{
	switch(eq)
	{
	case RENOIR_BLEND_EQ_ADD: return D3D11_BLEND_OP_ADD;
	case RENOIR_BLEND_EQ_SUBTRACT: return D3D11_BLEND_OP_SUBTRACT;
	case RENOIR_BLEND_EQ_MIN: return D3D11_BLEND_OP_MIN;
	case RENOIR_BLEND_EQ_MAX: return D3D11_BLEND_OP_MAX;
	default: mn_unreachable(); return D3D11_BLEND_OP_ADD;
	}
}

inline static D3D11_FILTER
_renoir_filter_to_dx(RENOIR_FILTER filter)
{
	switch(filter)
	{
	case RENOIR_FILTER_LINEAR: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	case RENOIR_FILTER_POINT: return D3D11_FILTER_MIN_MAG_MIP_POINT;
	default: mn_unreachable(); return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}
}

inline static D3D11_TEXTURE_ADDRESS_MODE
_renoir_texmode_to_dx(RENOIR_TEXMODE m)
{
	switch(m)
	{
	case RENOIR_TEXMODE_WRAP: return D3D11_TEXTURE_ADDRESS_WRAP;
	case RENOIR_TEXMODE_CLAMP: return D3D11_TEXTURE_ADDRESS_CLAMP;
	case RENOIR_TEXMODE_BORDER: return D3D11_TEXTURE_ADDRESS_BORDER;
	case RENOIR_TEXMODE_MIRROR: return D3D11_TEXTURE_ADDRESS_MIRROR;
	default: mn_unreachable(); return D3D11_TEXTURE_ADDRESS_WRAP;
	}
}

inline static D3D11_COMPARISON_FUNC
_renoir_compare_to_dx(RENOIR_COMPARE c)
{
	switch(c)
	{
	case RENOIR_COMPARE_LESS: return D3D11_COMPARISON_LESS;
	case RENOIR_COMPARE_EQUAL: return D3D11_COMPARISON_EQUAL;
	case RENOIR_COMPARE_LESS_EQUAL: return D3D11_COMPARISON_LESS_EQUAL;
	case RENOIR_COMPARE_GREATER: return D3D11_COMPARISON_GREATER;
	case RENOIR_COMPARE_NOT_EQUAL: return D3D11_COMPARISON_NOT_EQUAL;
	case RENOIR_COMPARE_GREATER_EQUAL: return D3D11_COMPARISON_GREATER_EQUAL;
	case RENOIR_COMPARE_NEVER: return D3D11_COMPARISON_NEVER;
	case RENOIR_COMPARE_ALWAYS: return D3D11_COMPARISON_ALWAYS;
	default: mn_unreachable(); return D3D11_COMPARISON_LESS;
	}
}

inline static DXGI_FORMAT
_renoir_type_to_dx(RENOIR_TYPE type)
{
	switch(type)
	{
	case RENOIR_TYPE_UINT8: return DXGI_FORMAT_R8_UINT;
	case RENOIR_TYPE_UINT8_4: return DXGI_FORMAT_R8G8B8A8_UINT;
	case RENOIR_TYPE_UINT8_4N: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RENOIR_TYPE_UINT16: return DXGI_FORMAT_R16_UINT;
	case RENOIR_TYPE_UINT32: return DXGI_FORMAT_R32_UINT;
	case RENOIR_TYPE_INT16: return DXGI_FORMAT_R16_SINT;
	case RENOIR_TYPE_INT32: return DXGI_FORMAT_R32_SINT;
	case RENOIR_TYPE_FLOAT: return DXGI_FORMAT_R32_FLOAT;
	case RENOIR_TYPE_FLOAT_2: return DXGI_FORMAT_R32G32_FLOAT;
	case RENOIR_TYPE_FLOAT_3: return DXGI_FORMAT_R32G32B32_FLOAT;
	case RENOIR_TYPE_FLOAT_4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	default: mn_unreachable(); return (DXGI_FORMAT)0;
	}
}

inline static size_t
_renoir_type_to_size(RENOIR_TYPE type)
{
	size_t res = 0;
	switch (type)
	{
	case RENOIR_TYPE_UINT8:
		res = 1;
		break;
	case RENOIR_TYPE_UINT8_4:
	case RENOIR_TYPE_UINT8_4N:
	case RENOIR_TYPE_UINT32:
	case RENOIR_TYPE_INT32:
	case RENOIR_TYPE_FLOAT:
		res = 4;
		break;
	case RENOIR_TYPE_INT16:
	case RENOIR_TYPE_UINT16:
		res = 2;
		break;
	case RENOIR_TYPE_FLOAT_2:
		res = 8;
		break;
	case RENOIR_TYPE_FLOAT_3:
		res = 12;
		break;
	case RENOIR_TYPE_FLOAT_4:
		res = 16;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static int
_renoir_msaa_to_dx(RENOIR_MSAA_MODE msaa)
{
	switch(msaa)
	{
	case RENOIR_MSAA_MODE_NONE: return 1;
	case RENOIR_MSAA_MODE_2: return 2;
	case RENOIR_MSAA_MODE_4: return 4;
	case RENOIR_MSAA_MODE_8: return 8;
	default: mn_unreachable(); return 0;
	}
}

inline static UINT8
_renoir_color_mask_to_dx(int color_mask)
{
	UINT8 res{};
	if (color_mask == RENOIR_COLOR_MASK_NONE)
		return res;

	if (color_mask & RENOIR_COLOR_MASK_RED) res |= D3D11_COLOR_WRITE_ENABLE_RED;
	if (color_mask & RENOIR_COLOR_MASK_GREEN) res |= D3D11_COLOR_WRITE_ENABLE_GREEN;
	if (color_mask & RENOIR_COLOR_MASK_BLUE) res |= D3D11_COLOR_WRITE_ENABLE_BLUE;
	if (color_mask & RENOIR_COLOR_MASK_ALPHA) res |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
	return res;
}


struct Renoir_Handle;

struct Renoir_Compute_Write_Slot
{
	Renoir_Handle* resource;
	int slot;
};

struct Renoir_Command;

enum RENOIR_TIMER_STATE
{
	// timer has not added begin
	RENOIR_TIMER_STATE_NONE,
	// timer has added a begin but not an end yet
	RENOIR_TIMER_STATE_BEGIN,
	// timer has added an end without being ready or even elapsed polled
	RENOIR_TIMER_STATE_END,
	// timer read has been scheduled but the actual GPU polling hasn't executed yet
	RENOIR_TIMER_STATE_READ_SCHEDULED,
	// timer is ready but not elapsed polled yet
	RENOIR_TIMER_STATE_READY,
};

enum RENOIR_HANDLE_KIND
{
	RENOIR_HANDLE_KIND_NONE,
	RENOIR_HANDLE_KIND_SWAPCHAIN,
	RENOIR_HANDLE_KIND_RASTER_PASS,
	RENOIR_HANDLE_KIND_COMPUTE_PASS,
	RENOIR_HANDLE_KIND_BUFFER,
	RENOIR_HANDLE_KIND_TEXTURE,
	RENOIR_HANDLE_KIND_SAMPLER,
	RENOIR_HANDLE_KIND_PROGRAM,
	RENOIR_HANDLE_KIND_COMPUTE,
	RENOIR_HANDLE_KIND_PIPELINE,
	RENOIR_HANDLE_KIND_TIMER,
};

struct Renoir_Handle
{
	RENOIR_HANDLE_KIND kind;
	std::atomic<int> rc;
	union
	{
		struct
		{
			int width;
			int height;
			void* window;
			IDXGISwapChain* swapchain;
			ID3D11RenderTargetView* render_target_view;
			ID3D11DepthStencilView *depth_stencil_view;
			ID3D11Texture2D* depth_buffer;
		} swapchain;

		struct
		{
			Renoir_Command *command_list_head;
			Renoir_Command *command_list_tail;
			// used when rendering is done on screen/window
			Renoir_Handle* swapchain;
			// used when rendering is done off screen
			ID3D11RenderTargetView* render_target_view[RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE];
			ID3D11DepthStencilView* depth_stencil_view;
			int width, height;
			Renoir_Pass_Offscreen_Desc offscreen;
		} raster_pass;

		struct
		{
			Renoir_Command *command_list_head;
			Renoir_Command *command_list_tail;
			// can be buffers or textures
			mn::Buf<Renoir_Compute_Write_Slot> write_resources;
		} compute_pass;

		struct
		{
			ID3D11Buffer* buffer;
			RENOIR_BUFFER type;
			RENOIR_USAGE usage;
			RENOIR_ACCESS access;
			size_t size;
			ID3D11Buffer* buffer_staging;
			ID3D11ShaderResourceView* srv;
			ID3D11UnorderedAccessView* uav;
		} buffer;

		struct
		{
			// normal texture part
			ID3D11Texture1D* texture1d;
			ID3D11Texture2D* texture2d;
			ID3D11Texture3D* texture3d;
			ID3D11ShaderResourceView* shader_view;
			mn::Buf<ID3D11UnorderedAccessView*> uavs;
			// staging part (for fast CPU writes)
			ID3D11Texture1D* texture1d_staging;
			ID3D11Texture2D* texture2d_staging;
			ID3D11Texture3D* texture3d_staging;
			// render target part
			ID3D11Texture2D* render_color_buffer;
			// shader resource view used in case msaa is enabled for this render target and it's
			// a depth target so that we can resolve it using compute shader
			ID3D11ShaderResourceView* render_depth_buffer_srv;
			Renoir_Texture_Desc desc;
		} texture;

		struct
		{
			ID3D11SamplerState* sampler;
			Renoir_Sampler_Desc desc;
		} sampler;

		struct
		{
			ID3D11InputLayout* input_layout;
			ID3D11VertexShader* vertex_shader;
			ID3D10Blob* vertex_shader_blob;
			ID3D11PixelShader* pixel_shader;
			ID3D11GeometryShader* geometry_shader;
		} program;

		struct
		{
			ID3D11ComputeShader* compute_shader;
		} compute;

		struct
		{
			Renoir_Pipeline_Desc desc;
			ID3D11DepthStencilState* depth_state;
			ID3D11RasterizerState* raster_state;
			ID3D11BlendState* blend_state;
		} pipeline;

		struct
		{
			ID3D11Query* start;
			ID3D11Query* end;
			ID3D11Query* frequency;
			uint64_t elapsed_time_in_nanos;
			RENOIR_TIMER_STATE state;
		} timer;
	};
};

inline static const char*
_renoir_handle_kind_name(RENOIR_HANDLE_KIND kind)
{
	switch(kind)
	{
	case RENOIR_HANDLE_KIND_NONE: return "none";
	case RENOIR_HANDLE_KIND_SWAPCHAIN: return "swapchain";
	case RENOIR_HANDLE_KIND_RASTER_PASS: return "raster_pass";
	case RENOIR_HANDLE_KIND_COMPUTE_PASS: return "compute_pass";
	case RENOIR_HANDLE_KIND_BUFFER: return "buffer";
	case RENOIR_HANDLE_KIND_TEXTURE: return "texture";
	case RENOIR_HANDLE_KIND_SAMPLER: return "sampler";
	case RENOIR_HANDLE_KIND_PROGRAM: return "program";
	case RENOIR_HANDLE_KIND_COMPUTE: return "compute";
	case RENOIR_HANDLE_KIND_PIPELINE: return "pipeline";
	default: mn_unreachable_msg("invalid handle kind"); return "<INVALID>";
	}
}

inline static bool
_renoir_handle_kind_should_track(RENOIR_HANDLE_KIND kind)
{
	return (
		kind == RENOIR_HANDLE_KIND_NONE ||
		kind == RENOIR_HANDLE_KIND_SWAPCHAIN ||
		kind == RENOIR_HANDLE_KIND_RASTER_PASS ||
		kind == RENOIR_HANDLE_KIND_COMPUTE_PASS ||
		kind == RENOIR_HANDLE_KIND_BUFFER ||
		kind == RENOIR_HANDLE_KIND_TEXTURE ||
		// we ignore the samplers because they are cached not user created
		// kind == RENOIR_HANDLE_KIND_SAMPLER ||
		kind == RENOIR_HANDLE_KIND_PROGRAM ||
		kind == RENOIR_HANDLE_KIND_COMPUTE
		// we ignore the pipeline because they are cached not user created
		// kind == RENOIR_HANDLE_KIND_PIPELINE
	);
}

inline static void
_renoir_dx11_pipeline_desc_defaults(Renoir_Pipeline_Desc* desc)
{
	if (desc->rasterizer.cull == RENOIR_SWITCH_DEFAULT)
		desc->rasterizer.cull = RENOIR_SWITCH_ENABLE;
	if (desc->rasterizer.cull_face == RENOIR_FACE_NONE)
		desc->rasterizer.cull_face = RENOIR_FACE_BACK;
	if (desc->rasterizer.cull_front == RENOIR_ORIENTATION_NONE)
		desc->rasterizer.cull_front = RENOIR_ORIENTATION_CCW;
	if (desc->rasterizer.scissor == RENOIR_SWITCH_DEFAULT)
		desc->rasterizer.scissor = RENOIR_SWITCH_DISABLE;

	if (desc->depth_stencil.depth == RENOIR_SWITCH_DEFAULT)
		desc->depth_stencil.depth = RENOIR_SWITCH_ENABLE;
	if (desc->depth_stencil.depth_write_mask == RENOIR_SWITCH_DEFAULT)
		desc->depth_stencil.depth_write_mask = RENOIR_SWITCH_ENABLE;

	if (desc->independent_blend == RENOIR_SWITCH_DEFAULT)
		desc->independent_blend = RENOIR_SWITCH_DISABLE;

	for (int i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
	{
		if (desc->blend[i].enabled == RENOIR_SWITCH_DEFAULT)
			desc->blend[i].enabled = RENOIR_SWITCH_ENABLE;
		if (desc->blend[i].src_rgb == RENOIR_BLEND_NONE)
			desc->blend[i].src_rgb = RENOIR_BLEND_SRC_ALPHA;
		if (desc->blend[i].dst_rgb == RENOIR_BLEND_NONE)
			desc->blend[i].dst_rgb = RENOIR_BLEND_ONE_MINUS_SRC_ALPHA;
		if (desc->blend[i].src_alpha == RENOIR_BLEND_NONE)
			desc->blend[i].src_alpha = RENOIR_BLEND_ONE;
		if (desc->blend[i].dst_alpha == RENOIR_BLEND_NONE)
			desc->blend[i].dst_alpha = RENOIR_BLEND_ONE_MINUS_SRC_ALPHA;
		if (desc->blend[i].eq_rgb == RENOIR_BLEND_EQ_NONE)
			desc->blend[i].eq_rgb = RENOIR_BLEND_EQ_ADD;
		if (desc->blend[i].eq_alpha == RENOIR_BLEND_EQ_NONE)
			desc->blend[i].eq_alpha = RENOIR_BLEND_EQ_ADD;

		if (desc->blend[i].color_mask == RENOIR_COLOR_MASK_DEFAULT)
			desc->blend[i].color_mask = RENOIR_COLOR_MASK_ALL;

		if (desc->independent_blend == RENOIR_SWITCH_DISABLE)
			break;
	}
}

enum RENOIR_COMMAND_KIND
{
	RENOIR_COMMAND_KIND_NONE,
	RENOIR_COMMAND_KIND_INIT,
	RENOIR_COMMAND_KIND_SWAPCHAIN_NEW,
	RENOIR_COMMAND_KIND_SWAPCHAIN_FREE,
	RENOIR_COMMAND_KIND_SWAPCHAIN_RESIZE,
	RENOIR_COMMAND_KIND_PASS_SWAPCHAIN_NEW,
	RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW,
	RENOIR_COMMAND_KIND_PASS_COMPUTE_NEW,
	RENOIR_COMMAND_KIND_PASS_FREE,
	RENOIR_COMMAND_KIND_BUFFER_NEW,
	RENOIR_COMMAND_KIND_BUFFER_FREE,
	RENOIR_COMMAND_KIND_TEXTURE_NEW,
	RENOIR_COMMAND_KIND_TEXTURE_FREE,
	RENOIR_COMMAND_KIND_SAMPLER_NEW,
	RENOIR_COMMAND_KIND_SAMPLER_FREE,
	RENOIR_COMMAND_KIND_PROGRAM_NEW,
	RENOIR_COMMAND_KIND_PROGRAM_FREE,
	RENOIR_COMMAND_KIND_COMPUTE_NEW,
	RENOIR_COMMAND_KIND_COMPUTE_FREE,
	RENOIR_COMMAND_KIND_PIPELINE_NEW,
	RENOIR_COMMAND_KIND_PIPELINE_FREE,
	RENOIR_COMMAND_KIND_TIMER_NEW,
	RENOIR_COMMAND_KIND_TIMER_FREE,
	RENOIR_COMMAND_KIND_TIMER_ELAPSED,
	RENOIR_COMMAND_KIND_PASS_BEGIN,
	RENOIR_COMMAND_KIND_PASS_END,
	RENOIR_COMMAND_KIND_PASS_CLEAR,
	RENOIR_COMMAND_KIND_USE_PIPELINE,
	RENOIR_COMMAND_KIND_USE_PROGRAM,
	RENOIR_COMMAND_KIND_USE_COMPUTE,
	RENOIR_COMMAND_KIND_SCISSOR,
	RENOIR_COMMAND_KIND_BUFFER_CLEAR,
	RENOIR_COMMAND_KIND_BUFFER_WRITE,
	RENOIR_COMMAND_KIND_TEXTURE_WRITE,
	RENOIR_COMMAND_KIND_BUFFER_READ,
	RENOIR_COMMAND_KIND_TEXTURE_READ,
	RENOIR_COMMAND_KIND_BUFFER_BIND,
	RENOIR_COMMAND_KIND_BUFFER_STORAGE_BIND,
	RENOIR_COMMAND_KIND_TEXTURE_BIND,
	RENOIR_COMMAND_KIND_DRAW,
	RENOIR_COMMAND_KIND_DISPATCH,
	RENOIR_COMMAND_KIND_TIMER_BEGIN,
	RENOIR_COMMAND_KIND_TIMER_END,
};

struct Renoir_Command
{
	Renoir_Command *prev, *next;
	RENOIR_COMMAND_KIND kind;
	union
	{
		struct
		{
		} init;

		struct
		{
			Renoir_Handle* handle;
		} swapchain_new;

		struct
		{
			Renoir_Handle* handle;
		} swapchain_free;

		struct
		{
			Renoir_Handle* handle;
			int width, height;
		} swapchain_resize;

		struct
		{
			Renoir_Handle* handle;
		} pass_swapchain_new;

		struct
		{
			Renoir_Handle* handle;
			Renoir_Pass_Offscreen_Desc desc;
		} pass_offscreen_new;

		struct
		{
			Renoir_Handle* handle;
		} pass_compute_new;

		struct
		{
			Renoir_Handle* handle;
		} pass_free;

		struct
		{
			Renoir_Handle* handle;
			Renoir_Buffer_Desc desc;
			bool owns_data;
		} buffer_new;

		struct
		{
			Renoir_Handle* handle;
		} buffer_free;

		struct
		{
			Renoir_Handle* handle;
			Renoir_Texture_Desc desc;
			bool owns_data;
		} texture_new;

		struct
		{
			Renoir_Handle* handle;
		} texture_free;

		struct
		{
			Renoir_Handle* handle;
			Renoir_Sampler_Desc desc;
		} sampler_new;

		struct
		{
			Renoir_Handle* handle;
		} sampler_free;

		struct
		{
			Renoir_Handle* handle;
			Renoir_Program_Desc desc;
			bool owns_data;
		} program_new;

		struct
		{
			Renoir_Handle* handle;
		} program_free;

		struct
		{
			Renoir_Handle* handle;
			Renoir_Compute_Desc desc;
			bool owns_data;
		} compute_new;

		struct
		{
			Renoir_Handle* handle;
		} compute_free;

		struct
		{
			Renoir_Handle* handle;
		} pipeline_new;

		struct
		{
			Renoir_Handle* handle;
		} pipeline_free;

		struct
		{
			Renoir_Handle* handle;
		} timer_new;

		struct
		{
			Renoir_Handle* handle;
		} timer_free;

		struct
		{
			Renoir_Handle* handle;
		} timer_elapsed;

		struct
		{
			Renoir_Handle* handle;
		} pass_begin;

		struct
		{
			Renoir_Handle* handle;
		} pass_end;

		struct
		{
			Renoir_Clear_Desc desc;
		} pass_clear;

		struct
		{
			Renoir_Handle* pipeline;
		} use_pipeline;

		struct
		{
			Renoir_Handle* program;
		} use_program;

		struct
		{
			Renoir_Handle* compute;
		} use_compute;

		struct
		{
			int x, y, w, h;
		} scissor;

		struct
		{
			Renoir_Handle* handle;
		} buffer_clear;

		struct
		{
			Renoir_Handle* handle;
			size_t offset;
			void* bytes;
			size_t bytes_size;
		} buffer_write;

		struct
		{
			Renoir_Handle* handle;
			Renoir_Texture_Edit_Desc desc;
		} texture_write;

		struct
		{
			Renoir_Handle* handle;
			size_t offset;
			void* bytes;
			size_t bytes_size;
		} buffer_read;

		struct
		{
			Renoir_Handle* handle;
			Renoir_Texture_Edit_Desc desc;
		} texture_read;

		struct
		{
			Renoir_Handle* handle;
			RENOIR_SHADER shader;
			int slot;
			RENOIR_ACCESS gpu_access;
		} buffer_bind;

		struct
		{
			Renoir_Handle* handle[RENOIR_CONSTANT_BUFFER_STORAGE_SIZE];
			int start_slot;
		} buffer_storage_bind;

		struct
		{
			Renoir_Handle* handle;
			RENOIR_SHADER shader;
			int slot;
			int mip_level;
			Renoir_Handle* sampler;
			RENOIR_ACCESS gpu_access;
		} texture_bind;

		struct
		{
			Renoir_Draw_Desc desc;
		} draw;

		struct
		{
			int x, y, z;
		} dispatch;

		struct
		{
			Renoir_Handle* handle;
		} timer_begin;

		struct
		{
			Renoir_Handle* handle;
		} timer_end;
	};
};

struct Renoir_Leak_Info
{
	void* callstack[20];
	size_t callstack_size;
};

struct IRenoir
{
	mn::Mutex mtx;
	IDXGIFactory* factory;
	IDXGIAdapter* adapter;
	ID3D11Device* device;
	ID3D11DeviceContext* context;
	mn::Pool handle_pool;
	mn::Pool command_pool;
	Renoir_Settings settings;

	mn::Str info_description;
	size_t gpu_memory_in_bytes;

	// handle to the default pipeline, because we don't require the user to set a pipeline
	// on pass clear, which is a major over sight, we'll solve it soon
	Renoir_Handle* default_pipeline;
	// MSAA depth buffers can't be resolved using ResolveSubresource so we have this compute
	// shader to do the resolve ourselves
	ID3D11ComputeShader* depth_resolve_compute;

	// global command list
	Renoir_Command *command_list_head;
	Renoir_Command *command_list_tail;

	// command execution context
	Renoir_Handle* current_pipeline;
	Renoir_Handle* current_program;
	Renoir_Handle* current_compute;
	Renoir_Handle* current_pass;

	// caches
	mn::Buf<Renoir_Handle*> sampler_cache;
	mn::Buf<Renoir_Handle*> pipeline_cache;

	// leak detection
	mn::Map<Renoir_Handle*, Renoir_Leak_Info> alive_handles;
};

static void
_renoir_dx11_command_execute(IRenoir* self, Renoir_Command* command);

static Renoir_Handle*
_renoir_dx11_handle_new(IRenoir* self, RENOIR_HANDLE_KIND kind)
{
	auto handle = (Renoir_Handle*)mn::pool_get(self->handle_pool);
	memset(handle, 0, sizeof(*handle));
	handle->kind = kind;
	handle->rc = 1;

	#ifdef DEBUG
	if (_renoir_handle_kind_should_track(kind))
	{
		mn_assert_msg(mn::map_lookup(self->alive_handles, handle) == nullptr, "reuse of already alive renoir handle");
		Renoir_Leak_Info info{};
		#if RENOIR_LEAK
			info.callstack_size = mn::callstack_capture(info.callstack, 20);
		#endif
		mn::map_insert(self->alive_handles, handle, info);
	}
	#endif

	return handle;
}

static void
_renoir_dx11_handle_free(IRenoir* self, Renoir_Handle* h)
{
	#ifdef DEBUG
	if (_renoir_handle_kind_should_track(h->kind))
	{
		auto removed = mn::map_remove(self->alive_handles, h);
		mn_assert_msg(removed, "free was called with an invalid renoir handle");
	}
	#endif
	mn::pool_put(self->handle_pool, h);
}

static Renoir_Handle*
_renoir_dx11_handle_ref(Renoir_Handle* h)
{
	h->rc.fetch_add(1);
	return h;
}

static bool
_renoir_dx11_handle_unref(Renoir_Handle* h)
{
	return h->rc.fetch_sub(1) == 1;
}

template<typename T>
static Renoir_Command*
_renoir_dx11_command_new(T* self, RENOIR_COMMAND_KIND kind)
{
	auto command = (Renoir_Command*)mn::pool_get(self->command_pool);
	memset(command, 0, sizeof(*command));
	command->kind = kind;
	return command;
}

template<typename T>
static void
_renoir_dx11_command_free(T* self, Renoir_Command* command)
{
	switch(command->kind)
	{
	case RENOIR_COMMAND_KIND_BUFFER_NEW:
	{
		if(command->buffer_new.owns_data)
			mn::free(mn::Block{(void*)command->buffer_new.desc.data, command->buffer_new.desc.data_size});
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_NEW:
	{
		if(command->texture_new.owns_data)
		{
			for (int i = 0; i < 6; ++i)
			{
				if (command->texture_new.desc.data[i] == nullptr)
					continue;

				mn::free(mn::Block{(void*)command->texture_new.desc.data[i], command->texture_new.desc.data_size});
			}
		}
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_NEW:
	{
		if(command->program_new.owns_data)
		{
			mn::free(mn::Block{(void*)command->program_new.desc.vertex.bytes, command->program_new.desc.vertex.size});
			mn::free(mn::Block{(void*)command->program_new.desc.pixel.bytes, command->program_new.desc.pixel.size});
			if (command->program_new.desc.geometry.bytes != nullptr)
				mn::free(mn::Block{(void*)command->program_new.desc.geometry.bytes, command->program_new.desc.geometry.size});
		}
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_NEW:
	{
		if(command->compute_new.owns_data)
		{
			mn::free(mn::Block{(void*)command->compute_new.desc.compute.bytes, command->compute_new.desc.compute.size});
		}
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_WRITE:
	{
		mn::free(mn::Block{(void*)command->buffer_write.bytes, command->buffer_write.bytes_size});
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_WRITE:
	{
		mn::free(mn::Block{(void*)command->texture_write.desc.bytes, command->texture_write.desc.bytes_size});
		break;
	}
	case RENOIR_COMMAND_KIND_NONE:
	case RENOIR_COMMAND_KIND_INIT:
	case RENOIR_COMMAND_KIND_SWAPCHAIN_NEW:
	case RENOIR_COMMAND_KIND_SWAPCHAIN_FREE:
	case RENOIR_COMMAND_KIND_SWAPCHAIN_RESIZE:
	case RENOIR_COMMAND_KIND_PASS_SWAPCHAIN_NEW:
	case RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW:
	case RENOIR_COMMAND_KIND_PASS_COMPUTE_NEW:
	case RENOIR_COMMAND_KIND_PASS_FREE:
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	case RENOIR_COMMAND_KIND_SAMPLER_NEW:
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
	case RENOIR_COMMAND_KIND_PIPELINE_NEW:
	case RENOIR_COMMAND_KIND_PIPELINE_FREE:
	case RENOIR_COMMAND_KIND_TIMER_NEW:
	case RENOIR_COMMAND_KIND_TIMER_FREE:
	case RENOIR_COMMAND_KIND_TIMER_ELAPSED:
	case RENOIR_COMMAND_KIND_PASS_BEGIN:
	case RENOIR_COMMAND_KIND_PASS_END:
	case RENOIR_COMMAND_KIND_PASS_CLEAR:
	case RENOIR_COMMAND_KIND_USE_PIPELINE:
	case RENOIR_COMMAND_KIND_USE_PROGRAM:
	case RENOIR_COMMAND_KIND_USE_COMPUTE:
	case RENOIR_COMMAND_KIND_SCISSOR:
	case RENOIR_COMMAND_KIND_BUFFER_READ:
	case RENOIR_COMMAND_KIND_TEXTURE_READ:
	case RENOIR_COMMAND_KIND_BUFFER_CLEAR:
	case RENOIR_COMMAND_KIND_BUFFER_BIND:
	case RENOIR_COMMAND_KIND_BUFFER_STORAGE_BIND:
	case RENOIR_COMMAND_KIND_TEXTURE_BIND:
	case RENOIR_COMMAND_KIND_DRAW:
	case RENOIR_COMMAND_KIND_DISPATCH:
	case RENOIR_COMMAND_KIND_TIMER_BEGIN:
	case RENOIR_COMMAND_KIND_TIMER_END:
	default:
		// do nothing
		break;
	}
	mn::pool_put(self->command_pool, command);
}

template<typename T>
static void
_renoir_dx11_command_push_back(T* self, Renoir_Command* command)
{
	if(self->command_list_tail == nullptr)
	{
		self->command_list_tail = command;
		self->command_list_head = command;
		return;
	}

	self->command_list_tail->next = command;
	command->prev = self->command_list_tail;
	self->command_list_tail = command;
}

template<typename T>
static void
_renoir_dx11_command_push_front(T* self, Renoir_Command* command)
{
	if(self->command_list_head == nullptr)
	{
		self->command_list_head = command;
		self->command_list_tail = command;
		return;
	}

	self->command_list_head->prev = command;
	command->next = self->command_list_head;
	self->command_list_head = command;
}

static void
_renoir_dx11_command_process(IRenoir* self, Renoir_Command* command)
{
	if (self->settings.defer_api_calls)
	{
		_renoir_dx11_command_push_back(self, command);
	}
	else
	{
		_renoir_dx11_command_execute(self, command);
		_renoir_dx11_command_free(self, command);
	}
}

inline static void
_internal_renoir_dx11_swapchain_new(IRenoir* self, Renoir_Handle* h)
{
	auto dx_msaa = _renoir_msaa_to_dx(self->settings.msaa);

	// create swapchain itself
	DXGI_SWAP_CHAIN_DESC swapchain_desc{};
	swapchain_desc.BufferCount = 1;
	swapchain_desc.BufferDesc.Width = h->swapchain.width;
	swapchain_desc.BufferDesc.Height = h->swapchain.height;
	swapchain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	if (self->settings.vsync == RENOIR_VSYNC_MODE_OFF)
	{
		swapchain_desc.BufferDesc.RefreshRate.Numerator = 0;
		swapchain_desc.BufferDesc.RefreshRate.Denominator = 1;
	}
	else
	{
		// find the numerator/denominator for correct vsync
		IDXGIOutput* output = nullptr;
		auto res = self->adapter->EnumOutputs(0, &output);
		mn_assert(SUCCEEDED(res));
		mn_defer(output->Release());

		UINT modes_count = 0;
		res = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &modes_count, NULL);
		mn_assert(SUCCEEDED(res));

		auto modes = mn::buf_with_count<DXGI_MODE_DESC>(modes_count);
		mn_defer(mn::buf_free(modes));

		res = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &modes_count, modes.ptr);
		mn_assert(SUCCEEDED(res));

		for (const auto& mode: modes)
		{
			if (mode.Width == h->swapchain.width && mode.Height == h->swapchain.height)
			{
				swapchain_desc.BufferDesc.RefreshRate.Numerator = mode.RefreshRate.Numerator;
				swapchain_desc.BufferDesc.RefreshRate.Denominator = mode.RefreshRate.Denominator;
			}
		}
	}
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.OutputWindow = (HWND)h->swapchain.window;
	swapchain_desc.SampleDesc.Count = dx_msaa;
	swapchain_desc.Windowed = true;
	swapchain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapchain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	auto res = self->factory->CreateSwapChain(self->device, &swapchain_desc, &h->swapchain.swapchain);
	mn_assert(SUCCEEDED(res));

	// create render target view
	ID3D11Texture2D* color_buffer = nullptr;
	res = h->swapchain.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&color_buffer);
	mn_assert(SUCCEEDED(res));

	res = self->device->CreateRenderTargetView(color_buffer, nullptr, &h->swapchain.render_target_view);
	mn_assert(SUCCEEDED(res));

	color_buffer->Release(); color_buffer = nullptr;

	// create depth buffer
	D3D11_TEXTURE2D_DESC depth_desc{};
	depth_desc.Width = h->swapchain.width;
	depth_desc.Height = h->swapchain.height;
	depth_desc.MipLevels = 1;
	depth_desc.ArraySize = 1;
	depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_desc.SampleDesc.Count = dx_msaa;
	depth_desc.Usage = D3D11_USAGE_DEFAULT;
	depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	res = self->device->CreateTexture2D(&depth_desc, nullptr, &h->swapchain.depth_buffer);
	mn_assert(SUCCEEDED(res));

	// create depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view{};
	depth_stencil_view.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	if (self->settings.msaa == RENOIR_MSAA_MODE_NONE)
		depth_stencil_view.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	else
		depth_stencil_view.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	res = self->device->CreateDepthStencilView(h->swapchain.depth_buffer, &depth_stencil_view, &h->swapchain.depth_stencil_view);
	mn_assert(SUCCEEDED(res));
}

inline static void
_renoir_dx11_input_layout_create(IRenoir* self, Renoir_Handle* h, const Renoir_Draw_Desc& draw)
{
	mn_defer({
		h->program.vertex_shader_blob->Release();
		h->program.vertex_shader_blob = nullptr;
	});
	ID3D11ShaderReflection* reflection = nullptr;
	auto res = D3DReflect(
		h->program.vertex_shader_blob->GetBufferPointer(),
		h->program.vertex_shader_blob->GetBufferSize(),
		__uuidof(ID3D11ShaderReflection),
		(void**)&reflection
	);
	mn_assert(SUCCEEDED(res));

	D3D11_SHADER_DESC shader_desc{};
	res = reflection->GetDesc(&shader_desc);
	mn_assert(SUCCEEDED(res));

	mn_assert(shader_desc.InputParameters < RENOIR_CONSTANT_DRAW_VERTEX_BUFFER_SIZE);
	D3D11_SIGNATURE_PARAMETER_DESC input_desc[RENOIR_CONSTANT_DRAW_VERTEX_BUFFER_SIZE];
	::memset(input_desc, 0, sizeof(input_desc));

	for (UINT i = 0; i < shader_desc.InputParameters; ++i)
	{
		res = reflection->GetInputParameterDesc(i, &input_desc[i]);
		mn_assert(SUCCEEDED(res));
	}

	D3D11_INPUT_ELEMENT_DESC input_layout_desc[RENOIR_CONSTANT_DRAW_VERTEX_BUFFER_SIZE];
	int count = 0;
	::memset(input_layout_desc, 0, sizeof(input_layout_desc));
	for (int i = 0; i < RENOIR_CONSTANT_DRAW_VERTEX_BUFFER_SIZE; ++i)
	{
		if (draw.vertex_buffers[i].buffer.handle == nullptr)
			continue;

		auto dx_type = _renoir_type_to_dx(draw.vertex_buffers[i].type);

		auto& desc = input_layout_desc[count++];
		desc.SemanticName = input_desc[i].SemanticName;
		desc.SemanticIndex = input_desc[i].SemanticIndex;
		desc.Format = dx_type;
		desc.InputSlot = i;
		desc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		desc.InstanceDataStepRate = 0;
	}

	res = self->device->CreateInputLayout(
		input_layout_desc,
		count,
		h->program.vertex_shader_blob->GetBufferPointer(),
		h->program.vertex_shader_blob->GetBufferSize(),
		&h->program.input_layout
	);
	mn_assert(SUCCEEDED(res));
}

inline static Renoir_Handle*
_renoir_dx11_sampler_new(IRenoir* self, Renoir_Sampler_Desc desc)
{
	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_SAMPLER);
	h->sampler.desc = desc;

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_SAMPLER_NEW);
	command->sampler_new.handle = h;
	command->sampler_new.desc = desc;
	_renoir_dx11_command_process(self, command);
	return h;
}

inline static void
_renoir_dx11_sampler_free(IRenoir* self, Renoir_Handle* h)
{
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_SAMPLER_FREE);
	command->sampler_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

inline static bool
operator==(const Renoir_Sampler_Desc& a, const Renoir_Sampler_Desc& b)
{
	return (
		a.filter == b.filter &&
		a.u == b.u &&
		a.v == b.v &&
		a.w == b.w &&
		a.compare == b.compare &&
		a.border.r == b.border.r &&
		a.border.g == b.border.g &&
		a.border.b == b.border.b &&
		a.border.a == b.border.a
	);
}

inline static Renoir_Handle*
_renoir_dx11_pipeline_new(IRenoir* self, Renoir_Pipeline_Desc desc)
{
	_renoir_dx11_pipeline_desc_defaults(&desc);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_PIPELINE);
	h->pipeline.desc = desc;

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PIPELINE_NEW);
	command->pipeline_new.handle = h;
	_renoir_dx11_command_process(self, command);
	return h;
}

inline static void
_renoir_dx11_pipeline_free(IRenoir* self, Renoir_Handle* pipeline)
{
	mn_assert(pipeline != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PIPELINE_FREE);
	command->pipeline_free.handle = pipeline;
	_renoir_dx11_command_process(self, command);
}

inline static void
_renoir_dx11_pipeline_use(IRenoir* self, Renoir_Handle* h)
{
	self->context->OMSetBlendState(h->pipeline.blend_state, nullptr, 0xFFFFFFFF);
	self->context->OMSetDepthStencilState(h->pipeline.depth_state, 1);
	self->context->RSSetState(h->pipeline.raster_state);
}

inline static bool
operator==(const Renoir_Rasterizer_Desc& a, const Renoir_Rasterizer_Desc& b)
{
	if (a.cull != b.cull)
		return false;
	if (a.cull == RENOIR_SWITCH_ENABLE)
	{
		if (a.cull_face != b.cull_face)
			return false;
		if (a.cull_front != b.cull_front)
			return false;
	}
	if (a.scissor != b.scissor)
		return false;
	return true;
}

inline static bool
operator!=(const Renoir_Rasterizer_Desc& a, const Renoir_Rasterizer_Desc& b)
{
	return !(a == b);
}

inline static bool
operator==(const Renoir_Depth_Desc& a, const Renoir_Depth_Desc& b)
{
	if (a.depth != b.depth)
		return false;
	if (a.depth == RENOIR_SWITCH_ENABLE)
	{
		if (a.depth_write_mask != b.depth_write_mask)
			return false;
	}
	return true;
}

inline static bool
operator!=(const Renoir_Depth_Desc& a, const Renoir_Depth_Desc& b)
{
	return !(a == b);
}

inline static bool
operator==(const Renoir_Blend_Desc& a, const Renoir_Blend_Desc& b)
{
	if (a.enabled != b.enabled)
		return false;
	if (a.enabled == RENOIR_SWITCH_ENABLE)
	{
		if (a.src_rgb != b.src_rgb)
			return false;
		if (a.dst_rgb != b.dst_rgb)
			return false;
		if (a.src_alpha != b.src_alpha)
			return false;
		if (a.dst_alpha != b.dst_alpha)
			return false;
		if (a.eq_rgb != b.eq_rgb)
			return false;
		if (a.eq_alpha != b.eq_alpha)
			return false;
	}
	if (a.color_mask != b.color_mask)
		return false;
	return true;
}

inline static bool
operator!=(const Renoir_Blend_Desc& a, const Renoir_Blend_Desc& b)
{
	return !(a == b);
}

inline static bool
operator==(const Renoir_Pipeline_Desc& a, const Renoir_Pipeline_Desc& b)
{
	if (a.rasterizer != b.rasterizer)
		return false;
	if (a.depth_stencil != b.depth_stencil)
		return false;
	if (a.independent_blend != b.independent_blend)
		return false;
	if (a.independent_blend == RENOIR_SWITCH_ENABLE)
	{
		for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
			if (a.blend[i] != b.blend[i])
				return false;
	}
	else
	{
		if (a.blend[0] != b.blend[0])
			return false;
	}
	return true;
}

inline static Renoir_Handle*
_renoir_dx11_pipeline_get(IRenoir* self, Renoir_Pipeline_Desc desc)
{
	size_t best_ix = self->pipeline_cache.count;
	size_t first_empty_ix = self->pipeline_cache.count;
	for (size_t i = 0; i < self->pipeline_cache.count; ++i)
	{
		auto hpipeline = self->pipeline_cache[i];
		if (hpipeline == nullptr)
		{
			if (first_empty_ix == self->pipeline_cache.count)
				first_empty_ix = i;
			continue;
		}

		if (desc == hpipeline->pipeline.desc)
		{
			best_ix = i;
			break;
		}
	}

	// we found what we were looking for
	if (best_ix < self->pipeline_cache.count)
	{
		auto res = self->pipeline_cache[best_ix];
		// reorder the cache
		for (size_t i = 0; i < best_ix; ++i)
		{
			auto index = best_ix - i - 1;
			self->pipeline_cache[index + 1] = self->pipeline_cache[index];
		}
		self->pipeline_cache[0] = res;
		return res;
	}

	// we didn't find a matching pipeline, so create new one
	size_t pipeline_ix = first_empty_ix;

	// we didn't find an empty slot for the new pipeline so we'll have to make one for it
	if (pipeline_ix == self->pipeline_cache.count)
	{
		auto to_be_evicted = mn::buf_top(self->pipeline_cache);
		for (size_t i = 0; i + 1 < self->pipeline_cache.count; ++i)
		{
			auto index = self->pipeline_cache.count - i - 1;
			self->pipeline_cache[index] = self->pipeline_cache[index - 1];
		}
		_renoir_dx11_pipeline_free(self, to_be_evicted);
		mn::log_warning("dx11: pipeline evicted");
		pipeline_ix = 0;
	}

	// create the new pipeline and put it at the head of the cache
	auto pipeline = _renoir_dx11_pipeline_new(self, desc);
	self->pipeline_cache[pipeline_ix] = pipeline;
	return pipeline;
}

inline static Renoir_Handle*
_renoir_dx11_sampler_get(IRenoir* self, Renoir_Sampler_Desc desc)
{
	size_t best_ix = self->sampler_cache.count;
	size_t first_empty_ix = self->sampler_cache.count;
	for (size_t i = 0; i < self->sampler_cache.count; ++i)
	{
		auto hsampler = self->sampler_cache[i];
		if (hsampler == nullptr)
		{
			if (first_empty_ix == self->sampler_cache.count)
				first_empty_ix = i;
			continue;
		}

		if (desc == hsampler->sampler.desc)
		{
			best_ix = i;
			break;
		}
	}

	// we found what we were looking for
	if (best_ix < self->sampler_cache.count)
	{
		auto res = self->sampler_cache[best_ix];
		// reorder the cache
		for (size_t i = 0; i < best_ix; ++i)
		{
			auto index = best_ix - i - 1;
			self->sampler_cache[index + 1] = self->sampler_cache[index];
		}
		self->sampler_cache[0] = res;
		return res;
	}

	// we didn't find a matching sampler, so create new one
	size_t sampler_ix = first_empty_ix;

	// we didn't find an empty slot for the new sampler so we'll have to make one for it
	if (sampler_ix == self->sampler_cache.count)
	{
		auto to_be_evicted = mn::buf_top(self->sampler_cache);
		for (size_t i = 0; i + 1 < self->sampler_cache.count; ++i)
		{
			auto index = self->sampler_cache.count - i - 1;
			self->sampler_cache[index] = self->sampler_cache[index - 1];
		}
		_renoir_dx11_sampler_free(self, to_be_evicted);
		mn::log_warning("dx11: sampler evicted");
		sampler_ix = 0;
	}

	// create the new sampler and put it at the head of the cache
	auto sampler = _renoir_dx11_sampler_new(self, desc);
	self->sampler_cache[sampler_ix] = sampler;
	return sampler;
}


static void
_renoir_dx11_command_execute(IRenoir* self, Renoir_Command* command)
{
	switch(command->kind)
	{
	case RENOIR_COMMAND_KIND_INIT:
	{
		if (self->settings.external_context == false)
		{
			DXGI_ADAPTER_DESC dxgi_adapter_desc{};
			auto res = self->adapter->GetDesc(&dxgi_adapter_desc);
			mn_assert(SUCCEEDED(res));

			self->info_description = mn::from_os_encoding(mn::block_from(dxgi_adapter_desc.Description), mn::allocator_top());
			self->gpu_memory_in_bytes = dxgi_adapter_desc.DedicatedVideoMemory;
			mn::log_info("D3D11 Renderer: {}", self->info_description);
			mn::log_info("D3D11 Video Memory: {}Mb", dxgi_adapter_desc.DedicatedVideoMemory / 1024 / 1024);
		}
		else
		{
			IDXGIDevice* dxgi_device = nullptr;
			auto res = self->device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);
			mn_assert(SUCCEEDED(res));
			mn_defer(dxgi_device->Release());

			IDXGIAdapter* dxgi_adapter = nullptr;
			res = dxgi_device->GetAdapter(&dxgi_adapter);
			mn_assert(SUCCEEDED(res));
			mn_defer(dxgi_adapter->Release());

			DXGI_ADAPTER_DESC dxgi_adapter_desc{};
			res = dxgi_adapter->GetDesc(&dxgi_adapter_desc);
			mn_assert(SUCCEEDED(res));

			self->info_description = mn::from_os_encoding(mn::block_from(dxgi_adapter_desc.Description), mn::allocator_top());
			self->gpu_memory_in_bytes = dxgi_adapter_desc.DedicatedVideoMemory;
			mn::log_info("D3D11 Renderer: {}", self->info_description);
			mn::log_info("D3D11 Video Memory: {}Mb", dxgi_adapter_desc.DedicatedVideoMemory / 1024 / 1024);
		}

		self->default_pipeline = _renoir_dx11_pipeline_new(self, Renoir_Pipeline_Desc{});
		// init depth resolve compute shader
		{
			ID3D10Blob* error = nullptr;
			ID3D10Blob* compute_shader_blob = nullptr;
			auto res = D3DCompile(
				DEPTH_RESOLVE_COMPUTE,
				::strlen(DEPTH_RESOLVE_COMPUTE),
				NULL,
				NULL,
				NULL,
				"main",
				"cs_5_0",
				0,
				0,
				&compute_shader_blob,
				&error
			);
			if (FAILED(res))
			{
				mn::log_error("compute shader compile error\n{}", (char *)error->GetBufferPointer());
				break;
			}
			res = self->device->CreateComputeShader(
				compute_shader_blob->GetBufferPointer(),
				compute_shader_blob->GetBufferSize(),
				NULL,
				&self->depth_resolve_compute
			);
			mn_assert(SUCCEEDED(res));
			compute_shader_blob->Release();
		}
		break;
	}
	case RENOIR_COMMAND_KIND_SWAPCHAIN_NEW:
	{
		auto h = command->swapchain_new.handle;
		_internal_renoir_dx11_swapchain_new(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_SWAPCHAIN_FREE:
	{
		auto h = command->swapchain_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		h->swapchain.swapchain->Release();
		h->swapchain.render_target_view->Release();
		h->swapchain.depth_stencil_view->Release();
		h->swapchain.depth_buffer->Release();
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_SWAPCHAIN_RESIZE:
	{
		auto h = command->swapchain_resize.handle;
		h->swapchain.width = command->swapchain_resize.width;
		h->swapchain.height = command->swapchain_resize.height;

		// free the current swapchain
		h->swapchain.swapchain->Release();
		h->swapchain.render_target_view->Release();
		h->swapchain.depth_stencil_view->Release();
		h->swapchain.depth_buffer->Release();

		// reinitialize the swapchain
		_internal_renoir_dx11_swapchain_new(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_SWAPCHAIN_NEW:
	{
		// do nothing
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW:
	{
		auto h = command->pass_offscreen_new.handle;
		auto &desc = command->pass_offscreen_new.desc;
		h->raster_pass.offscreen = desc;

		int msaa = -1;

		for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
		{
			auto color = (Renoir_Handle*)desc.color[i].texture.handle;
			if (color == nullptr)
				continue;
			mn_assert(color->texture.desc.render_target);

			_renoir_dx11_handle_ref(color);

			auto dx_format = _renoir_pixelformat_to_dx(color->texture.desc.pixel_format);

			if (color->texture.desc.cube_map == false)
			{
				if (color->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert_msg(desc.color[i].level == 0, "multisampled textures does not support mipmaps");
					auto res = self->device->CreateRenderTargetView(color->texture.render_color_buffer, nullptr, &h->raster_pass.render_target_view[i]);
					mn_assert(SUCCEEDED(res));
				}
				else
				{
					mn_assert_msg(desc.color[i].level < color->texture.desc.mipmaps, "out of range mip level");
					D3D11_RENDER_TARGET_VIEW_DESC render_target_desc{};
					render_target_desc.Format = dx_format;
					render_target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					render_target_desc.Texture2D.MipSlice = desc.color[i].level;
					auto res = self->device->CreateRenderTargetView(color->texture.texture2d, &render_target_desc, &h->raster_pass.render_target_view[i]);
					mn_assert(SUCCEEDED(res));
				}
			}
			else
			{
				if (color->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert_msg(desc.color[i].level == 0, "multisampled textures does not support mipmaps");
					D3D11_RENDER_TARGET_VIEW_DESC render_target_desc{};
					render_target_desc.Format = dx_format;
					render_target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
					render_target_desc.Texture2DMSArray.FirstArraySlice = desc.color[i].subresource;
					render_target_desc.Texture2DMSArray.ArraySize = 1;
					auto res = self->device->CreateRenderTargetView(color->texture.render_color_buffer, &render_target_desc, &h->raster_pass.render_target_view[i]);
					mn_assert(SUCCEEDED(res));
				}
				else
				{
					mn_assert_msg(desc.color[i].level < color->texture.desc.mipmaps, "out of range mip level");
					D3D11_RENDER_TARGET_VIEW_DESC render_target_desc{};
					render_target_desc.Format = dx_format;
					render_target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
					render_target_desc.Texture2DArray.FirstArraySlice = desc.color[i].subresource;
					render_target_desc.Texture2DArray.ArraySize = 1;
					render_target_desc.Texture2DArray.MipSlice = desc.color[i].level;
					auto res = self->device->CreateRenderTargetView(color->texture.texture2d, &render_target_desc, &h->raster_pass.render_target_view[i]);
					mn_assert(SUCCEEDED(res));
				}
			}

			// check that all of them has the same msaa
			if (msaa == -1)
			{
				msaa = color->texture.desc.msaa;
			}
			else
			{
				mn_assert(msaa == color->texture.desc.msaa);
			}
		}

		auto depth = (Renoir_Handle*)desc.depth_stencil.texture.handle;
		if (depth)
		{
			mn_assert(depth->texture.desc.render_target);
			_renoir_dx11_handle_ref(depth);
			if (depth->texture.desc.cube_map == false)
			{
				if (depth->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert_msg(desc.depth_stencil.level == 0, "multisampled textures does not support mipmaps");
					auto dx_format = _renoir_pixelformat_depth_to_dx_depth_view(depth->texture.desc.pixel_format);
					D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc{};
					depth_view_desc.Format = dx_format;
					depth_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
					auto res = self->device->CreateDepthStencilView(depth->texture.render_color_buffer, &depth_view_desc, &h->raster_pass.depth_stencil_view);
					mn_assert(SUCCEEDED(res));
				}
				else
				{
					mn_assert_msg(desc.depth_stencil.level < depth->texture.desc.mipmaps, "out of range mip level");
					auto dx_format = _renoir_pixelformat_depth_to_dx_depth_view(depth->texture.desc.pixel_format);
					D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc{};
					depth_view_desc.Format = dx_format;
					depth_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
					depth_view_desc.Texture2D.MipSlice = desc.depth_stencil.level;
					auto res = self->device->CreateDepthStencilView(depth->texture.texture2d, &depth_view_desc, &h->raster_pass.depth_stencil_view);
					mn_assert(SUCCEEDED(res));
				}
			}
			else
			{
				if (depth->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert_msg(desc.depth_stencil.level == 0, "multisampled textures does not support mipmaps");
					auto dx_format = _renoir_pixelformat_depth_to_dx_depth_view(depth->texture.desc.pixel_format);
					D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc{};
					depth_view_desc.Format = dx_format;
					depth_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
					depth_view_desc.Texture2DMSArray.FirstArraySlice = desc.depth_stencil.subresource;
					depth_view_desc.Texture2DMSArray.ArraySize = 1;
					auto res = self->device->CreateDepthStencilView(depth->texture.render_color_buffer, &depth_view_desc, &h->raster_pass.depth_stencil_view);
					mn_assert(SUCCEEDED(res));
				}
				else
				{
					mn_assert_msg(desc.depth_stencil.level < depth->texture.desc.mipmaps, "out of range mip level");
					auto dx_format = _renoir_pixelformat_depth_to_dx_depth_view(depth->texture.desc.pixel_format);
					D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc{};
					depth_view_desc.Format = dx_format;
					depth_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
					depth_view_desc.Texture2DArray.FirstArraySlice = desc.depth_stencil.subresource;
					depth_view_desc.Texture2DArray.ArraySize = 1;
					depth_view_desc.Texture2DArray.MipSlice = desc.depth_stencil.level;
					auto res = self->device->CreateDepthStencilView(depth->texture.texture2d, &depth_view_desc, &h->raster_pass.depth_stencil_view);
					mn_assert(SUCCEEDED(res));
				}
			}

			// check that all of them has the same msaa
			if (msaa == -1)
			{
				msaa = depth->texture.desc.msaa;
			}
			else
			{
				mn_assert(msaa == depth->texture.desc.msaa);
			}
		}
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_COMPUTE_NEW:
	{
		// do nothing
		auto h = command->pass_compute_new.handle;
		h->compute_pass.write_resources = mn::buf_new<Renoir_Compute_Write_Slot>();
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_FREE:
	{
		auto h = command->pass_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			for(auto it = h->raster_pass.command_list_head; it != NULL; it = it->next)
				_renoir_dx11_command_free(self, command);

			// free all the bound textures if it's a framebuffer pass
			if (h->raster_pass.swapchain == nullptr)
			{
				for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
				{
					auto color = (Renoir_Handle*)h->raster_pass.offscreen.color[i].texture.handle;
					if (color == nullptr)
						continue;

					h->raster_pass.render_target_view[i]->Release();
					// issue command to free the color texture
					auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = color;
					_renoir_dx11_command_execute(self, command);
					_renoir_dx11_command_free(self, command);
				}

				auto depth = (Renoir_Handle*)h->raster_pass.offscreen.depth_stencil.texture.handle;
				if (depth)
				{
					h->raster_pass.depth_stencil_view->Release();
					// issue command to free the depth texture
					auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = depth;
					_renoir_dx11_command_execute(self, command);
					_renoir_dx11_command_free(self, command);
				}
			}
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			for(auto it = h->compute_pass.command_list_head; it != NULL; it = it->next)
				_renoir_dx11_command_free(self, command);

			mn::buf_free(h->compute_pass.write_resources);
		}

		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_NEW:
	{
		auto h = command->buffer_new.handle;
		auto& desc = command->buffer_new.desc;

		auto dx_buffer_type = _renoir_buffer_type_to_dx(desc.type);

		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = desc.data_size;
		buffer_desc.BindFlags = dx_buffer_type;
		if (desc.usage == RENOIR_USAGE_STATIC)
		{
			buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
		}
		else
		{
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		}

		if (desc.type == RENOIR_BUFFER_COMPUTE)
		{
			mn_assert_msg(
				desc.compute_buffer_stride > 0 && desc.compute_buffer_stride % 4 == 0,
				"compute buffer stride should be greater than 0, no greater than 2048, and a multiple of 4"
			);

			buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			buffer_desc.StructureByteStride = desc.compute_buffer_stride;
		}

		if (desc.data)
		{
			D3D11_SUBRESOURCE_DATA data_desc{};
			data_desc.pSysMem = desc.data;
			auto res = self->device->CreateBuffer(&buffer_desc, &data_desc, &h->buffer.buffer);
			mn_assert(SUCCEEDED(res));
		}
		else
		{
			auto res = self->device->CreateBuffer(&buffer_desc, nullptr, &h->buffer.buffer);
			mn_assert(SUCCEEDED(res));
		}

		if (desc.type == RENOIR_BUFFER_COMPUTE)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.BufferEx.NumElements = buffer_desc.ByteWidth / buffer_desc.StructureByteStride;
			auto res = self->device->CreateShaderResourceView(h->buffer.buffer, &srv_desc, &h->buffer.srv);
			mn_assert(SUCCEEDED(res));

			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uav_desc.Format = DXGI_FORMAT_UNKNOWN;
			uav_desc.Buffer.NumElements = buffer_desc.ByteWidth / buffer_desc.StructureByteStride;
			res = self->device->CreateUnorderedAccessView(h->buffer.buffer, &uav_desc, &h->buffer.uav);
			mn_assert(SUCCEEDED(res));
		}

		if (desc.access != RENOIR_ACCESS_NONE)
		{
			auto buffer_staging_desc = buffer_desc;
			buffer_staging_desc.BindFlags = 0;
			buffer_staging_desc.Usage = D3D11_USAGE_STAGING;
			buffer_staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
			auto res = self->device->CreateBuffer(&buffer_staging_desc, nullptr, &h->buffer.buffer_staging);
			mn_assert(SUCCEEDED(res));
		}
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	{
		auto h = command->buffer_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		h->buffer.buffer->Release();
		if (h->buffer.buffer_staging) h->buffer.buffer_staging->Release();
		if (h->buffer.srv) h->buffer.srv->Release();
		if (h->buffer.uav) h->buffer.uav->Release();
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_NEW:
	{
		auto h = command->texture_new.handle;
		auto& desc = command->texture_new.desc;

		auto dx_access = _renoir_access_to_dx(desc.access);
		auto dx_usage = _renoir_usage_to_dx(desc.usage);
		auto dx_pixelformat = _renoir_pixelformat_to_dx(desc.pixel_format);
		auto dx_pixelformat_size = _renoir_pixelformat_to_size(desc.pixel_format);

		if (desc.size.height == 0 && desc.size.depth == 0)
		{
			D3D11_TEXTURE1D_DESC texture_desc{};
			texture_desc.ArraySize = 1;
			texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			texture_desc.MipLevels = desc.mipmaps;
			texture_desc.Width = desc.size.width;
			texture_desc.Usage = D3D11_USAGE_DEFAULT;
			texture_desc.Format = dx_pixelformat;
			if (desc.mipmaps > 1)
				texture_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

			if (desc.data[0])
			{
				D3D11_SUBRESOURCE_DATA data_desc{};
				data_desc.pSysMem = desc.data[0];
				data_desc.SysMemPitch = desc.data_size;
				auto res = self->device->CreateTexture1D(&texture_desc, &data_desc, &h->texture.texture1d);
				mn_assert(SUCCEEDED(res));
			}
			else
			{
				auto res = self->device->CreateTexture1D(&texture_desc, nullptr, &h->texture.texture1d);
				mn_assert(SUCCEEDED(res));
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
			view_desc.Format = dx_pixelformat;
			view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
			view_desc.Texture1D.MipLevels = texture_desc.MipLevels;
			auto res = self->device->CreateShaderResourceView(h->texture.texture1d, &view_desc, &h->texture.shader_view);
			mn_assert(SUCCEEDED(res));

			if (desc.access != RENOIR_ACCESS_NONE)
			{
				auto texture_staging_desc = texture_desc;
				texture_staging_desc.BindFlags = 0;
				texture_staging_desc.Usage = D3D11_USAGE_STAGING;
				texture_staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
				res = self->device->CreateTexture1D(&texture_staging_desc, nullptr, &h->texture.texture1d_staging);
				mn_assert(SUCCEEDED(res));
			}

			mn::buf_resize(h->texture.uavs, desc.mipmaps);
			for (int i = 0; i < h->texture.uavs.count; ++i)
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
				uav_desc.Format = dx_pixelformat;
				uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
				uav_desc.Texture1D.MipSlice = i;
				res = self->device->CreateUnorderedAccessView(h->texture.texture1d, &uav_desc, &h->texture.uavs[i]);
				mn_assert(SUCCEEDED(res));
			}

			if (desc.mipmaps > 1)
				self->context->GenerateMips(h->texture.shader_view);
		}
		else if (desc.size.height > 0 && desc.size.depth == 0)
		{
			DXGI_FORMAT texture_format = dx_pixelformat;

			D3D11_TEXTURE2D_DESC texture_desc{};
			texture_desc.ArraySize = desc.cube_map ? 6 : 1;
			if (desc.render_target && desc.msaa != RENOIR_MSAA_MODE_NONE)
			{
				texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				// in case of depth texture that supports msaa we'll have to adjust format to be
				// UAV friendly
				if (_renoir_pixelformat_is_depth(desc.pixel_format))
				{
					texture_format = _renoir_pixelformat_depth_to_dx_uav(desc.pixel_format);
				}
			}
			else
			{
				if (_renoir_pixelformat_is_depth(desc.pixel_format) == false)
					texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				else
					texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
			}
			texture_desc.MipLevels = desc.mipmaps;
			texture_desc.Width = desc.size.width;
			texture_desc.Height = desc.size.height;
			texture_desc.Usage = D3D11_USAGE_DEFAULT;
			texture_desc.Format = texture_format;
			texture_desc.SampleDesc.Count = 1;
			if (desc.mipmaps > 1)
				texture_desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
			if (desc.cube_map)
				texture_desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

			bool no_data = true;
			D3D11_SUBRESOURCE_DATA data_desc[6];
			::memset(data_desc, 0, sizeof(data_desc));
			for (int i = 0; i < 6; ++i)
			{
				if (desc.data[i] == nullptr)
					continue;

				no_data = false;
				data_desc[i].pSysMem = desc.data[i];
				data_desc[i].SysMemPitch = desc.size.width * dx_pixelformat_size;
				data_desc[i].SysMemSlicePitch = desc.data_size;
			}

			if (no_data)
			{
				auto res = self->device->CreateTexture2D(&texture_desc, nullptr, &h->texture.texture2d);
				mn_assert(SUCCEEDED(res));
			}
			else
			{
				auto res = self->device->CreateTexture2D(&texture_desc, data_desc, &h->texture.texture2d);
				mn_assert(SUCCEEDED(res));
			}

			// create SRV
			if ((texture_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0)
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
				if (_renoir_pixelformat_is_depth(desc.pixel_format) == false)
				{
					view_desc.Format = texture_format;
				}
				else
				{
					if (desc.render_target && desc.msaa != RENOIR_MSAA_MODE_NONE)
						view_desc.Format = texture_format;
					else
						view_desc.Format = _renoir_pixelformat_depth_to_dx_srv(desc.pixel_format);
				}
				if (desc.cube_map == false)
				{
					view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					view_desc.Texture2D.MipLevels = texture_desc.MipLevels;
				}
				else
				{
					view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
					view_desc.TextureCube.MipLevels = texture_desc.MipLevels;
				}
				auto res = self->device->CreateShaderResourceView(h->texture.texture2d, &view_desc, &h->texture.shader_view);
				mn_assert(SUCCEEDED(res));
			}

			// create UAV
			if ((texture_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0)
			{
				mn::buf_resize(h->texture.uavs, desc.mipmaps);
				for (int i = 0; i < h->texture.uavs.count; ++i)
				{
					D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
					uav_desc.Format = texture_format;
					if (desc.cube_map == false)
					{
						uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
						uav_desc.Texture2D.MipSlice = i;
					}
					else
					{
						uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
						uav_desc.Texture2DArray.ArraySize = 6;
						uav_desc.Texture2DArray.MipSlice = i;
					}
					auto res = self->device->CreateUnorderedAccessView(h->texture.texture2d, &uav_desc, &h->texture.uavs[i]);
					mn_assert(SUCCEEDED(res));
				}
			}

			// create render buffer that supports MSAA
			if (desc.render_target && desc.msaa != RENOIR_MSAA_MODE_NONE)
			{
				auto dx_msaa = _renoir_msaa_to_dx(desc.msaa);

				// create the msaa rendertarget
				D3D11_TEXTURE2D_DESC texture_desc{};
				texture_desc.ArraySize = desc.cube_map ? 6 : 1;
				if (_renoir_pixelformat_is_depth(desc.pixel_format) == false)
					texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
				else
					texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
				texture_desc.MipLevels = 1;
				texture_desc.Width = desc.size.width;
				texture_desc.Height = desc.size.height;
				texture_desc.Usage = D3D11_USAGE_DEFAULT;
				texture_desc.Format = dx_pixelformat;
				texture_desc.SampleDesc.Count = dx_msaa;
				auto res = self->device->CreateTexture2D(&texture_desc, nullptr, &h->texture.render_color_buffer);
				mn_assert(SUCCEEDED(res));

				// in case of depth we have to create the srv so that we can resolve it later
				// using a compute shader
				if (_renoir_pixelformat_is_depth(desc.pixel_format))
				{
					auto dx_depth_srv_pixelformat = _renoir_pixelformat_depth_to_dx_srv(desc.pixel_format);
					D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
					srv_desc.Format = dx_depth_srv_pixelformat;
					srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
					auto res = self->device->CreateShaderResourceView(h->texture.render_color_buffer, &srv_desc, &h->texture.render_depth_buffer_srv);
					mn_assert(SUCCEEDED(res));
				}
			}

			// create staging texture
			if (desc.access != RENOIR_ACCESS_NONE)
			{
				auto texture_staging_desc = texture_desc;
				texture_staging_desc.BindFlags = 0;
				texture_staging_desc.Usage = D3D11_USAGE_STAGING;
				texture_staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
				auto res = self->device->CreateTexture2D(&texture_staging_desc, nullptr, &h->texture.texture2d_staging);
				mn_assert(SUCCEEDED(res));
			}

			if (desc.mipmaps > 1)
				self->context->GenerateMips(h->texture.shader_view);
		}
		else if (desc.size.height > 0 && desc.size.depth > 0)
		{
			D3D11_TEXTURE3D_DESC texture_desc{};
			texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			texture_desc.MipLevels = desc.mipmaps;
			texture_desc.Width = desc.size.width;
			texture_desc.Height = desc.size.height;
			texture_desc.Depth = desc.size.depth;
			texture_desc.Usage = D3D11_USAGE_DEFAULT;
			texture_desc.Format = dx_pixelformat;
			if (desc.mipmaps > 1)
				texture_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

			if (desc.data[0])
			{
				D3D11_SUBRESOURCE_DATA data_desc{};
				data_desc.pSysMem = desc.data[0];
				data_desc.SysMemPitch = desc.size.width * dx_pixelformat_size;
				data_desc.SysMemSlicePitch = desc.size.height * data_desc.SysMemPitch;
				auto res = self->device->CreateTexture3D(&texture_desc, &data_desc, &h->texture.texture3d);
				mn_assert(SUCCEEDED(res));
			}
			else
			{
				auto res = self->device->CreateTexture3D(&texture_desc, nullptr, &h->texture.texture3d);
				mn_assert(SUCCEEDED(res));
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
			view_desc.Format = dx_pixelformat;
			view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
			view_desc.Texture3D.MipLevels = texture_desc.MipLevels;
			auto res = self->device->CreateShaderResourceView(h->texture.texture3d, &view_desc, &h->texture.shader_view);
			mn_assert(SUCCEEDED(res));

			mn::buf_resize(h->texture.uavs, desc.mipmaps);
			for (int i = 0; i < h->texture.uavs.count; ++i)
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
				uav_desc.Format = dx_pixelformat;
				uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
				uav_desc.Texture3D.WSize = desc.size.depth;
				uav_desc.Texture3D.MipSlice = i;
				res = self->device->CreateUnorderedAccessView(h->texture.texture3d, &uav_desc, &h->texture.uavs[i]);
				mn_assert(SUCCEEDED(res));
			}

			if (desc.access != RENOIR_ACCESS_NONE)
			{
				auto texture_staging_desc = texture_desc;
				texture_staging_desc.BindFlags = 0;
				texture_staging_desc.Usage = D3D11_USAGE_STAGING;
				texture_staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
				res = self->device->CreateTexture3D(&texture_staging_desc, nullptr, &h->texture.texture3d_staging);
				mn_assert(SUCCEEDED(res));
			}

			if (desc.mipmaps > 1)
				self->context->GenerateMips(h->texture.shader_view);
		}
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	{
		auto h = command->texture_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		if (h->texture.texture1d) h->texture.texture1d->Release();
		if (h->texture.texture2d) h->texture.texture2d->Release();
		if (h->texture.texture3d) h->texture.texture3d->Release();
		if (h->texture.shader_view) h->texture.shader_view->Release();
		for (auto uav: h->texture.uavs) uav->Release();
		mn::buf_free(h->texture.uavs);
		if (h->texture.texture1d_staging) h->texture.texture1d_staging->Release();
		if (h->texture.texture2d_staging) h->texture.texture2d_staging->Release();
		if (h->texture.texture3d_staging) h->texture.texture3d_staging->Release();
		if (h->texture.render_color_buffer) h->texture.render_color_buffer->Release();
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_SAMPLER_NEW:
	{
		auto h = command->sampler_new.handle;
		auto& desc = command->sampler_new.desc;

		auto dx_filter = _renoir_filter_to_dx(desc.filter);
		auto dx_u = _renoir_texmode_to_dx(desc.u);
		auto dx_v = _renoir_texmode_to_dx(desc.v);
		auto dx_w = _renoir_texmode_to_dx(desc.w);
		auto dx_compare = _renoir_compare_to_dx(desc.compare);

		D3D11_SAMPLER_DESC sampler_desc{};
		sampler_desc.Filter = dx_filter;
		sampler_desc.AddressU = dx_u;
		sampler_desc.AddressV = dx_v;
		sampler_desc.AddressW = dx_w;
		sampler_desc.MipLODBias = 0;
		sampler_desc.MaxAnisotropy = 1;
		sampler_desc.ComparisonFunc = dx_compare;
		sampler_desc.BorderColor[0] = desc.border.r;
		sampler_desc.BorderColor[1] = desc.border.g;
		sampler_desc.BorderColor[2] = desc.border.b;
		sampler_desc.BorderColor[3] = desc.border.a;
		sampler_desc.MinLOD = 0;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
		auto res = self->device->CreateSamplerState(&sampler_desc, &h->sampler.sampler);
		mn_assert(SUCCEEDED(res));
		break;
	}
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	{
		auto h = command->sampler_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		h->buffer.buffer->Release();
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_NEW:
	{
		auto h = command->program_new.handle;
		auto& desc = command->program_new.desc;

		ID3D10Blob* error = nullptr;

		auto res = D3DCompile(
			desc.vertex.bytes,
			desc.vertex.size,
			NULL,
			NULL,
			NULL,
			"main",
			"vs_5_0",
			0,
			0,
			&h->program.vertex_shader_blob,
			&error
		);
		if (FAILED(res))
		{
			mn::log_error("vertex shader compile error\n{}", (char *)error->GetBufferPointer());
			break;
		}
		res = self->device->CreateVertexShader(
			h->program.vertex_shader_blob->GetBufferPointer(),
			h->program.vertex_shader_blob->GetBufferSize(),
			NULL,
			&h->program.vertex_shader
		);
		mn_assert(SUCCEEDED(res));

		ID3D10Blob* pixel_shader_blob = nullptr;
		res = D3DCompile(
			desc.pixel.bytes,
			desc.pixel.size,
			NULL,
			NULL,
			NULL,
			"main",
			"ps_5_0",
			0,
			0,
			&pixel_shader_blob,
			&error
		);
		if (FAILED(res))
		{
			mn::log_error("pixel shader compile error\n{}", (char *)error->GetBufferPointer());
			break;
		}
		res = self->device->CreatePixelShader(
			pixel_shader_blob->GetBufferPointer(),
			pixel_shader_blob->GetBufferSize(),
			NULL,
			&h->program.pixel_shader
		);
		mn_assert(SUCCEEDED(res));
		pixel_shader_blob->Release();

		if (desc.geometry.bytes)
		{
			ID3D10Blob* geometry_shader_blob = nullptr;
			auto res = D3DCompile(
				desc.geometry.bytes,
				desc.geometry.size,
				NULL,
				NULL,
				NULL,
				"main",
				"gs_5_0",
				0,
				0,
				&geometry_shader_blob,
				&error
			);
			if (FAILED(res))
			{
				mn::log_error("geometry shader compile error\n{}", (char *)error->GetBufferPointer());
				break;
			}
			res = self->device->CreateGeometryShader(
				geometry_shader_blob->GetBufferPointer(),
				geometry_shader_blob->GetBufferSize(),
				NULL,
				&h->program.geometry_shader
			);
			mn_assert(SUCCEEDED(res));
			geometry_shader_blob->Release();
		}
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	{
		auto h = command->program_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		if (h->program.vertex_shader) h->program.vertex_shader->Release();
		if (h->program.vertex_shader_blob) h->program.vertex_shader_blob->Release();
		if (h->program.pixel_shader) h->program.pixel_shader->Release();
		if (h->program.geometry_shader) h->program.geometry_shader->Release();
		if (h->program.input_layout) h->program.input_layout->Release();
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_NEW:
	{
		auto h = command->compute_new.handle;
		auto& desc = command->compute_new.desc;

		ID3D10Blob* error = nullptr;

		ID3D10Blob* compute_shader_blob = nullptr;
		auto res = D3DCompile(
			desc.compute.bytes,
			desc.compute.size,
			NULL,
			NULL,
			NULL,
			"main",
			"cs_5_0",
			0,
			0,
			&compute_shader_blob,
			&error
		);
		if (FAILED(res))
		{
			mn::log_error("compute shader compile error\n{}", (char *)error->GetBufferPointer());
			break;
		}
		res = self->device->CreateComputeShader(
			compute_shader_blob->GetBufferPointer(),
			compute_shader_blob->GetBufferSize(),
			NULL,
			&h->compute.compute_shader
		);
		mn_assert(SUCCEEDED(res));
		compute_shader_blob->Release();
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
	{
		auto h = command->compute_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		if (h->compute.compute_shader) h->compute.compute_shader->Release();
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PIPELINE_NEW:
	{
		auto h = command->pipeline_new.handle;
		auto& desc = h->pipeline.desc;

		D3D11_DEPTH_STENCIL_DESC depth_desc{};

		depth_desc.DepthEnable = desc.depth_stencil.depth == RENOIR_SWITCH_ENABLE;
		if (desc.depth_stencil.depth_write_mask == RENOIR_SWITCH_ENABLE)
			depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		else
			depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_desc.DepthFunc = D3D11_COMPARISON_LESS;

		depth_desc.StencilEnable = false;
		depth_desc.StencilReadMask = 0xFF;
		depth_desc.StencilWriteMask = 0xFF;
		depth_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depth_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depth_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depth_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		depth_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depth_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depth_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depth_desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		auto res = self->device->CreateDepthStencilState(&depth_desc, &h->pipeline.depth_state);
		mn_assert(SUCCEEDED(res));

		D3D11_RASTERIZER_DESC raster_desc{};
		raster_desc.AntialiasedLineEnable = true;
		if (desc.rasterizer.cull == RENOIR_SWITCH_ENABLE)
		{
			switch(desc.rasterizer.cull_face)
			{
			case RENOIR_FACE_BACK:
				raster_desc.CullMode = D3D11_CULL_BACK;
				break;
			case RENOIR_FACE_FRONT:
				raster_desc.CullMode = D3D11_CULL_FRONT;
				break;
			case RENOIR_FACE_FRONT_BACK:
				break;
			default:
				mn_unreachable();
				break;
			}
		}
		else
		{
			raster_desc.CullMode = D3D11_CULL_NONE;
		}
		raster_desc.DepthBias = 0;
		raster_desc.DepthBiasClamp = 0.0f;
		raster_desc.DepthClipEnable = true;
		raster_desc.FillMode = D3D11_FILL_SOLID;
		raster_desc.FrontCounterClockwise = desc.rasterizer.cull_front == RENOIR_ORIENTATION_CCW;
		raster_desc.MultisampleEnable = true;
		raster_desc.ScissorEnable = desc.rasterizer.scissor == RENOIR_SWITCH_ENABLE;
		raster_desc.SlopeScaledDepthBias = 0.0f;
		res = self->device->CreateRasterizerState(&raster_desc, &h->pipeline.raster_state);
		mn_assert(SUCCEEDED(res));

		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = false;
		blend_desc.IndependentBlendEnable = desc.independent_blend == RENOIR_SWITCH_ENABLE;
		for (int i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
		{
			blend_desc.RenderTarget[i].BlendEnable = desc.blend[i].enabled == RENOIR_SWITCH_ENABLE;
			blend_desc.RenderTarget[i].SrcBlend = _renoir_blend_to_dx(desc.blend[i].src_rgb);
			blend_desc.RenderTarget[i].DestBlend = _renoir_blend_to_dx(desc.blend[i].dst_rgb);
			blend_desc.RenderTarget[i].BlendOp = _renoir_blend_eq_to_dx(desc.blend[i].eq_rgb);
			blend_desc.RenderTarget[i].SrcBlendAlpha = _renoir_blend_to_dx(desc.blend[i].src_alpha);
			blend_desc.RenderTarget[i].DestBlendAlpha = _renoir_blend_to_dx(desc.blend[i].dst_alpha);
			blend_desc.RenderTarget[i].BlendOpAlpha = _renoir_blend_eq_to_dx(desc.blend[i].eq_alpha);
			blend_desc.RenderTarget[i].RenderTargetWriteMask = _renoir_color_mask_to_dx(desc.blend[i].color_mask);
			if (desc.independent_blend == RENOIR_SWITCH_DISABLE)
				break;
		}
		res = self->device->CreateBlendState(&blend_desc, &h->pipeline.blend_state);
		mn_assert(SUCCEEDED(res));
		break;
	}
	case RENOIR_COMMAND_KIND_PIPELINE_FREE:
	{
		auto h = command->pipeline_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		if (h->pipeline.depth_state) h->pipeline.depth_state->Release();
		if (h->pipeline.raster_state) h->pipeline.raster_state->Release();
		if (h->pipeline.blend_state) h->pipeline.blend_state->Release();
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_NEW:
	{
		auto h = command->timer_new.handle;

		D3D11_QUERY_DESC desc{};
		desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		auto res = self->device->CreateQuery(&desc, &h->timer.frequency);
		mn_assert(SUCCEEDED(res));

		desc.Query = D3D11_QUERY_TIMESTAMP;
		res = self->device->CreateQuery(&desc, &h->timer.start);
		mn_assert(SUCCEEDED(res));

		res = self->device->CreateQuery(&desc, &h->timer.end);
		mn_assert(SUCCEEDED(res));
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_FREE:
	{
		auto h = command->timer_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;

		h->timer.frequency->Release();
		h->timer.start->Release();
		h->timer.end->Release();
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_ELAPSED:
	{
		auto h = command->timer_elapsed.handle;
		mn_assert(h->timer.state == RENOIR_TIMER_STATE_READ_SCHEDULED);
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT frequency{};
		auto res = self->context->GetData(h->timer.frequency, &frequency, sizeof(frequency), 0);
		if (SUCCEEDED(res))
		{
			uint64_t start = 0, end = 0;
			res = self->context->GetData(h->timer.start, &start, sizeof(start), 0);
			mn_assert(SUCCEEDED(res));
			res = self->context->GetData(h->timer.end, &end, sizeof(end), 0);
			mn_assert(SUCCEEDED(res));

			auto delta = end - start;
			double freq = (double)frequency.Frequency;
			auto t = (double)delta / freq;
			t *= 1000000000;
			h->timer.elapsed_time_in_nanos = (uint64_t)t;
			h->timer.state = RENOIR_TIMER_STATE_READY;

			if (frequency.Disjoint)
			{
				mn::log_warning("unreliable GPU timer reporting {}nanos, this could be due to unplugging the AC cord on a laptop, overheating, etc...", h->timer.elapsed_time_in_nanos);
			}
		}
		else
		{
			h->timer.state = RENOIR_TIMER_STATE_END;
		}
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_BEGIN:
	{
		auto h = command->pass_begin.handle;

		self->current_pass = h;

		if (self->current_pass->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			// if this is an on screen/window
			if (auto swapchain = h->raster_pass.swapchain)
			{
				self->context->OMSetRenderTargets(1, &swapchain->swapchain.render_target_view, swapchain->swapchain.depth_stencil_view);
				D3D11_VIEWPORT viewport{};
				viewport.Width = swapchain->swapchain.width;
				viewport.Height = swapchain->swapchain.height;
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;
				viewport.TopLeftX = 0.0f;
				viewport.TopLeftY = 0.0f;
				self->context->RSSetViewports(1, &viewport);
				D3D11_RECT scissor{};
				scissor.left = 0;
				scissor.right = viewport.Width;
				scissor.top = 0;
				scissor.bottom = viewport.Height;
				self->context->RSSetScissorRects(1, &scissor);
			}
			// this is an off screen
			else
			{
				self->context->OMSetRenderTargets(RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE, h->raster_pass.render_target_view, h->raster_pass.depth_stencil_view);
				D3D11_VIEWPORT viewport{};
				viewport.Width = h->raster_pass.width;
				viewport.Height = h->raster_pass.height;
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;
				viewport.TopLeftX = 0.0f;
				viewport.TopLeftY = 0.0f;
				self->context->RSSetViewports(1, &viewport);
				D3D11_RECT scissor{};
				scissor.left = 0;
				scissor.right = viewport.Width;
				scissor.top = 0;
				scissor.bottom = viewport.Height;
				self->context->RSSetScissorRects(1, &scissor);
			}
		}
		else if (self->current_pass->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			mn::buf_clear(self->current_pass->compute_pass.write_resources);
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_END:
	{
		auto h = command->pass_end.handle;
		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			// unbind render targets and uavs
			self->context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0, nullptr, nullptr);
			self->current_pipeline = nullptr;
			self->current_program = nullptr;
			self->current_pass = nullptr;

			// if this is an off screen view with msaa we'll need to issue a read command to move the data
			// from renderbuffer to the texture
			for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
			{
				auto color = (Renoir_Handle*)h->raster_pass.offscreen.color[i].texture.handle;
				if (color == nullptr)
					continue;

				// only resolve msaa textures
				if (color->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					auto dx_pixel_format = _renoir_pixelformat_to_dx(color->texture.desc.pixel_format);
					self->context->ResolveSubresource(
						color->texture.texture2d,
						h->raster_pass.offscreen.color[i].subresource,
						color->texture.render_color_buffer,
						h->raster_pass.offscreen.color[i].subresource,
						dx_pixel_format
					);
				}

				// schedule copy on staging cpu read access
				if (color->texture.desc.access == RENOIR_ACCESS_READ || color->texture.desc.access == RENOIR_ACCESS_READ_WRITE)
				{
					if (color->texture.texture2d)
					{
						auto subresource = D3D11CalcSubresource(
							h->raster_pass.offscreen.color[i].level,
							h->raster_pass.offscreen.color[i].subresource,
							color->texture.desc.mipmaps
						);
						D3D11_BOX src_box{};
						src_box.left = 0;
						src_box.right = color->texture.desc.size.width;
						src_box.top = 0;
						src_box.bottom = color->texture.desc.size.height;
						src_box.back = 1;
						self->context->CopySubresourceRegion(
							color->texture.texture2d_staging,
							subresource,
							0,
							0,
							0,
							color->texture.texture2d,
							subresource,
							&src_box
						);
					}
					else
					{
						mn_unreachable_msg("we only support 2d render targets");
					}
				}
			}

			// resolve depth textures as well
			auto depth = (Renoir_Handle*)h->raster_pass.offscreen.depth_stencil.texture.handle;
			if (depth)
			{
				if (depth->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert(depth->texture.uavs.count > 0);
					mn_assert(depth->texture.render_depth_buffer_srv != nullptr);

					ID3D11ShaderResourceView* srv[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {nullptr};
					::memset(srv, 0, sizeof(srv));

					self->context->VSSetShaderResources(0, sizeof(srv) / sizeof(*srv), srv);
					self->context->PSSetShaderResources(0, sizeof(srv) / sizeof(*srv), srv);
					self->context->GSSetShaderResources(0, sizeof(srv) / sizeof(*srv), srv);
					self->context->CSSetShaderResources(0, sizeof(srv) / sizeof(*srv), srv);

					self->context->VSSetShader(nullptr, nullptr, 0);
					self->context->PSSetShader(nullptr, nullptr, 0);
					self->context->GSSetShader(nullptr, nullptr, 0);
					self->context->CSSetShader(self->depth_resolve_compute, nullptr, 0);

					self->context->CSSetShaderResources(0, 1, &depth->texture.render_depth_buffer_srv);
					self->context->CSSetUnorderedAccessViews(0, 1, &depth->texture.uavs[0], nullptr);
					self->context->Dispatch(
						(UINT)::ceilf(depth->texture.desc.size.width / 16.0f),
						(UINT)::ceilf(depth->texture.desc.size.height / 16.0f),
						1
					);
					// clear compute shader uav
					ID3D11UnorderedAccessView* uav[1] = { nullptr };
					self->context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

					// clear compute shader srv
					self->context->CSSetShaderResources(0, sizeof(srv) / sizeof(*srv), srv);
				}
				// schedule copy on staging cpu read access
				if (depth->texture.desc.access == RENOIR_ACCESS_READ || depth->texture.desc.access == RENOIR_ACCESS_READ_WRITE)
				{
					if (depth->texture.texture2d)
					{
						auto subresource = D3D11CalcSubresource(
							h->raster_pass.offscreen.depth_stencil.level,
							h->raster_pass.offscreen.depth_stencil.subresource,
							depth->texture.desc.mipmaps
						);
						D3D11_BOX src_box{};
						src_box.left = 0;
						src_box.right = depth->texture.desc.size.width;
						src_box.top = 0;
						src_box.bottom = depth->texture.desc.size.height;
						src_box.back = 1;
						self->context->CopySubresourceRegion(
							depth->texture.texture2d_staging,
							subresource,
							0,
							0,
							0,
							depth->texture.texture2d,
							subresource,
							&src_box
						);
					}
					else
					{
						mn_unreachable_msg("we only support 2d render targets");
					}
				}
			}

		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			// schedule buffer copy here
			for (auto [hres, slot]: h->compute_pass.write_resources)
			{
				if (hres->kind == RENOIR_HANDLE_KIND_BUFFER)
				{
					if (hres->buffer.access == RENOIR_ACCESS_READ || hres->buffer.access == RENOIR_ACCESS_READ_WRITE)
					{
						self->context->CopyResource(hres->buffer.buffer_staging, hres->buffer.buffer);
					}
				}
				else if (hres->kind == RENOIR_HANDLE_KIND_TEXTURE)
				{
					if (hres->texture.desc.access == RENOIR_ACCESS_READ || hres->texture.desc.access == RENOIR_ACCESS_READ_WRITE)
					{
						if (hres->texture.texture1d)
						{
							self->context->CopyResource(hres->texture.texture1d_staging, hres->texture.texture1d);
						}
						else if (hres->texture.texture2d)
						{
							self->context->CopyResource(hres->texture.texture2d_staging, hres->texture.texture2d);
						}
						else if (hres->texture.texture3d)
						{
							self->context->CopyResource(hres->texture.texture3d_staging, hres->texture.texture3d);
						}
					}
				}
				else
				{
					mn_unreachable_msg("invalid resource");
				}
				// clear compute shader write slot
				ID3D11UnorderedAccessView* uav[1] = { nullptr };
				self->context->CSSetUnorderedAccessViews(slot, 1, uav, nullptr);
			}
			self->current_program = nullptr;
			self->current_pass = nullptr;
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_CLEAR:
	{
		auto& desc = command->pass_clear.desc;

		mn_assert(self->current_pass->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

		// TODO(Moustapha): pipeline should be set when we PASS_BEGIN but for now if you don't set
		// current pipeline, you get the default one :shrug:
		if (self->current_pipeline == nullptr)
		{
			_renoir_dx11_pipeline_use(self, self->default_pipeline);
		}

		if (desc.flags & RENOIR_CLEAR_COLOR)
		{
			if (auto swapchain = self->current_pass->raster_pass.swapchain)
			{
				self->context->ClearRenderTargetView(swapchain->swapchain.render_target_view, &desc.color[0].r);
			}
			else
			{
				for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
				{
					auto render_target = self->current_pass->raster_pass.render_target_view[i];
					if (render_target == nullptr)
						continue;
					float* color = &desc.color[0].r;
					if (desc.independent_clear_color == RENOIR_SWITCH_ENABLE)
						color = &desc.color[i].r;
					self->context->ClearRenderTargetView(render_target, color);
				}
			}
		}

		if (desc.flags & RENOIR_CLEAR_DEPTH)
		{
			if (auto swapchain = self->current_pass->raster_pass.swapchain)
			{
				self->context->ClearDepthStencilView(swapchain->swapchain.depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, desc.depth, desc.stencil);
			}
			else
			{
				auto depth_stencil = self->current_pass->raster_pass.depth_stencil_view;
				self->context->ClearDepthStencilView(depth_stencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, desc.depth, desc.stencil);
			}
		}
		break;
	}
	case RENOIR_COMMAND_KIND_USE_PIPELINE:
	{
		self->current_pipeline = command->use_pipeline.pipeline;
		_renoir_dx11_pipeline_use(self, self->current_pipeline);
		break;
	}
	case RENOIR_COMMAND_KIND_USE_PROGRAM:
	{
		auto h = command->use_program.program;
		self->current_program = h;
		self->current_compute = nullptr;
		self->context->VSSetShader(h->program.vertex_shader, NULL, 0);
		self->context->PSSetShader(h->program.pixel_shader, NULL, 0);
		if (h->program.geometry_shader)
			self->context->GSSetShader(h->program.geometry_shader, NULL, 0);
		else
			self->context->GSSetShader(NULL, NULL, 0);
		if (h->program.input_layout)
			self->context->IASetInputLayout(h->program.input_layout);
		break;
	}
	case RENOIR_COMMAND_KIND_USE_COMPUTE:
	{
		auto h = command->use_compute.compute;
		self->current_compute = h;
		self->current_program = nullptr;
		self->context->CSSetShader(h->compute.compute_shader, NULL, 0);
		break;
	}
	case RENOIR_COMMAND_KIND_SCISSOR:
	{
		D3D11_RECT scissor{};
		scissor.left = command->scissor.x;
		scissor.right = command->scissor.x + command->scissor.w;
		scissor.top = command->scissor.y;
		scissor.bottom = command->scissor.y + command->scissor.h;
		self->context->RSSetScissorRects(1, &scissor);
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_CLEAR:
	{
		auto h = command->buffer_clear.handle;
		UINT value[4] = {};
		self->context->ClearUnorderedAccessViewUint(h->buffer.uav, value);
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_WRITE:
	{
		auto h = command->buffer_write.handle;

		mn_assert(h->buffer.access == RENOIR_ACCESS_WRITE || h->buffer.access == RENOIR_ACCESS_READ_WRITE);

		D3D11_MAPPED_SUBRESOURCE mapped_resource{};
		auto res = self->context->Map(h->buffer.buffer_staging, 0, D3D11_MAP_WRITE, 0, &mapped_resource);
		mn_assert(SUCCEEDED(res));
		::memcpy(
			(char*)mapped_resource.pData + command->buffer_write.offset,
			command->buffer_write.bytes,
			command->buffer_write.bytes_size
		);
		self->context->Unmap(h->buffer.buffer_staging, 0);

		D3D11_BOX src_box{};
		src_box.left = command->buffer_write.offset;
		src_box.right = command->buffer_write.offset + command->buffer_write.bytes_size;
		src_box.bottom = 1;
		src_box.back = 1;
		self->context->CopySubresourceRegion(
			h->buffer.buffer,
			0,
			command->buffer_write.offset,
			0,
			0,
			h->buffer.buffer_staging,
			0,
			&src_box
		);
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_WRITE:
	{
		auto h = command->texture_write.handle;
		auto& desc = command->texture_write.desc;

		auto dx_pixel_size = _renoir_pixelformat_to_size(h->texture.desc.pixel_format);

		if (h->texture.texture1d)
		{
			D3D11_MAPPED_SUBRESOURCE mapped_resource{};
			auto subresource = D3D11CalcSubresource(0, 0, h->texture.desc.mipmaps);
			auto res = self->context->Map(h->texture.texture1d_staging, subresource, D3D11_MAP_WRITE, 0, &mapped_resource);
			mn_assert(SUCCEEDED(res));
			::memcpy(
				(char*)mapped_resource.pData + desc.x * dx_pixel_size,
				desc.bytes,
				desc.bytes_size
			);
			self->context->Unmap(h->texture.texture1d_staging, subresource);

			D3D11_BOX src_box{};
			src_box.left = desc.x;
			src_box.right = desc.x + desc.width;
			src_box.bottom = 1;
			src_box.back = 1;
			self->context->CopySubresourceRegion(
				h->texture.texture1d,
				subresource,
				desc.x,
				0,
				0,
				h->texture.texture1d_staging,
				subresource,
				&src_box
			);
			if (h->texture.desc.mipmaps > 1)
				self->context->GenerateMips(h->texture.shader_view);
		}
		else if (h->texture.texture2d)
		{
			if (h->texture.desc.cube_map == false)
				desc.z = 0;

			D3D11_MAPPED_SUBRESOURCE mapped_resource{};
			auto subresource = D3D11CalcSubresource(0, desc.z, h->texture.desc.mipmaps);
			auto res = self->context->Map(h->texture.texture2d_staging, subresource, D3D11_MAP_WRITE, 0, &mapped_resource);
			mn_assert(SUCCEEDED(res));

			char* write_ptr = (char*)mapped_resource.pData;
			write_ptr += mapped_resource.RowPitch * desc.y;
			char* read_ptr = (char*)desc.bytes;
			for (size_t i = 0; i < desc.height; ++i)
			{
				::memcpy(
					write_ptr + desc.x * dx_pixel_size,
					read_ptr,
					desc.width * dx_pixel_size
				);
				write_ptr += mapped_resource.RowPitch;
				read_ptr += desc.width * dx_pixel_size;
			}
			self->context->Unmap(h->texture.texture2d_staging, subresource);

			D3D11_BOX src_box{};
			src_box.left = desc.x;
			src_box.right = desc.x + desc.width;
			src_box.top = desc.y;
			src_box.bottom = desc.y + desc.height;
			src_box.back = 1;
			self->context->CopySubresourceRegion(
				h->texture.texture2d,
				subresource,
				desc.x,
				desc.y,
				0,
				h->texture.texture2d_staging,
				subresource,
				&src_box
			);
			if (h->texture.desc.mipmaps > 1)
				self->context->GenerateMips(h->texture.shader_view);
		}
		else if (h->texture.texture3d)
		{
			D3D11_MAPPED_SUBRESOURCE mapped_resource{};
			auto subresource = D3D11CalcSubresource(0, 0, h->texture.desc.mipmaps);
			auto res = self->context->Map(h->texture.texture3d_staging, subresource, D3D11_MAP_WRITE, 0, &mapped_resource);
			mn_assert(SUCCEEDED(res));

			char* write_ptr = (char*)mapped_resource.pData;
			write_ptr += mapped_resource.DepthPitch * desc.z + mapped_resource.RowPitch * desc.y;
			char* read_ptr = (char*)desc.bytes;
			for (size_t i = 0; i < desc.depth; ++i)
			{
				auto write_2d_ptr = write_ptr;
				for (size_t j = 0; j < desc.height; ++j)
				{
					::memcpy(
						write_2d_ptr + desc.x * dx_pixel_size,
						read_ptr,
						desc.width * dx_pixel_size
					);
					write_2d_ptr += mapped_resource.RowPitch;
					read_ptr += desc.width * dx_pixel_size;
				}
				write_ptr += mapped_resource.DepthPitch;
			}
			self->context->Unmap(h->texture.texture3d_staging, subresource);

			D3D11_BOX src_box{};
			src_box.left = desc.x;
			src_box.right = desc.x + desc.width;
			src_box.top = desc.y;
			src_box.bottom = desc.y + desc.height;
			src_box.front = desc.z;
			src_box.back = desc.z + desc.depth;
			self->context->CopySubresourceRegion(
				h->texture.texture3d,
				subresource,
				desc.x,
				desc.y,
				desc.z,
				h->texture.texture3d_staging,
				subresource,
				&src_box
			);
			if (h->texture.desc.mipmaps > 1)
				self->context->GenerateMips(h->texture.shader_view);
		}
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_READ:
	{
		auto h = command->buffer_read.handle;

		D3D11_MAPPED_SUBRESOURCE mapped_resource{};
		self->context->Map(h->buffer.buffer_staging, 0, D3D11_MAP_READ, 0, &mapped_resource);
		::memcpy(
			command->buffer_read.bytes,
			(char*)mapped_resource.pData + command->buffer_read.offset,
			command->buffer_read.bytes_size
		);
		self->context->Unmap(h->buffer.buffer_staging, 0);
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_READ:
	{
		auto h = command->texture_read.handle;
		auto& desc = command->texture_read.desc;

		auto dx_pixel_size = _renoir_pixelformat_to_size(h->texture.desc.pixel_format);

		if (h->texture.texture1d)
		{
			D3D11_MAPPED_SUBRESOURCE mapped_resource{};
			auto subresource = D3D11CalcSubresource(0, 0, h->texture.desc.mipmaps);
			self->context->Map(h->texture.texture1d_staging, subresource, D3D11_MAP_READ, 0, &mapped_resource);
			::memcpy(
				desc.bytes,
				(char*)mapped_resource.pData + desc.x * dx_pixel_size,
				desc.bytes_size
			);
			self->context->Unmap(h->texture.texture1d_staging, subresource);
		}
		else if (h->texture.texture2d)
		{
			if (h->texture.desc.cube_map == false)
				desc.z = 0;

			D3D11_MAPPED_SUBRESOURCE mapped_resource{};
			auto subresource = D3D11CalcSubresource(0, desc.z, h->texture.desc.mipmaps);
			self->context->Map(h->texture.texture2d_staging, subresource, D3D11_MAP_READ, 0, &mapped_resource);

			char* read_ptr = (char*)mapped_resource.pData;
			read_ptr += mapped_resource.RowPitch * desc.y;
			char* write_ptr = (char*)desc.bytes;
			for(size_t i = 0; i < desc.height; ++i)
			{
				::memcpy(
					write_ptr,
					read_ptr + desc.x * dx_pixel_size,
					desc.width * dx_pixel_size
				);
				read_ptr += mapped_resource.RowPitch;
				write_ptr += desc.width * dx_pixel_size;
			}
			self->context->Unmap(h->texture.texture2d_staging, subresource);
		}
		else if (h->texture.texture3d)
		{
			D3D11_MAPPED_SUBRESOURCE mapped_resource{};
			auto subresource = D3D11CalcSubresource(0, 0, h->texture.desc.mipmaps);
			self->context->Map(h->texture.texture3d_staging, subresource, D3D11_MAP_READ, 0, &mapped_resource);

			char* read_ptr = (char*)mapped_resource.pData;
			read_ptr += mapped_resource.DepthPitch * desc.z + mapped_resource.RowPitch * desc.y;
			char* write_ptr = (char*)desc.bytes;
			for(size_t i = 0; i < desc.depth; ++i)
			{
				auto read_2d_ptr = read_ptr;
				for(size_t j = 0; j < desc.height; ++j)
				{
					::memcpy(
						write_ptr,
						read_2d_ptr + desc.x * dx_pixel_size,
						desc.width * dx_pixel_size
					);
					read_2d_ptr += mapped_resource.RowPitch;
					write_ptr += desc.width * dx_pixel_size;
				}
				read_ptr += mapped_resource.DepthPitch;
			}
			self->context->Unmap(h->texture.texture3d_staging, subresource);
		}
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_BIND:
	{
		auto h = command->buffer_bind.handle;

		if (h->buffer.type == RENOIR_BUFFER_UNIFORM)
		{
			switch(command->buffer_bind.shader)
			{
			case RENOIR_SHADER_VERTEX:
				self->context->VSSetConstantBuffers(command->buffer_bind.slot, 1, &h->buffer.buffer);
				break;
			case RENOIR_SHADER_PIXEL:
				self->context->PSSetConstantBuffers(command->buffer_bind.slot, 1, &h->buffer.buffer);
				break;
			case RENOIR_SHADER_GEOMETRY:
				self->context->GSSetConstantBuffers(command->buffer_bind.slot, 1, &h->buffer.buffer);
				break;
			case RENOIR_SHADER_COMPUTE:
				self->context->CSSetConstantBuffers(command->buffer_bind.slot, 1, &h->buffer.buffer);
				break;
			default:
				mn_unreachable();
				break;
			}
		}
		else if (h->buffer.type == RENOIR_BUFFER_COMPUTE)
		{
			if (command->buffer_bind.gpu_access == RENOIR_ACCESS_READ)
			{
				self->context->CSSetShaderResources(command->buffer_bind.slot, 1, &h->buffer.srv);
			}
			else if (command->buffer_bind.gpu_access == RENOIR_ACCESS_WRITE ||
					 command->buffer_bind.gpu_access == RENOIR_ACCESS_READ_WRITE)
			{
				self->context->CSSetUnorderedAccessViews(command->buffer_bind.slot, 1, &h->buffer.uav, nullptr);
				Renoir_Compute_Write_Slot write_slot{};
				write_slot.resource = h;
				write_slot.slot = command->buffer_bind.slot;
				mn::buf_push(self->current_pass->compute_pass.write_resources, write_slot);
			}
		}
		else
		{
			mn_unreachable_msg("invalid buffer");
		}
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_STORAGE_BIND:
	{
		ID3D11UnorderedAccessView *uavs[RENOIR_CONSTANT_BUFFER_STORAGE_SIZE] = {};
		for (int i = 0; i < RENOIR_CONSTANT_BUFFER_STORAGE_SIZE; ++i)
		{
			auto h = command->buffer_storage_bind.handle[i];
			if (h == nullptr)
				continue;

			uavs[i] = h->buffer.uav;
		}

		self->context->OMSetRenderTargetsAndUnorderedAccessViews(
			D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL,
			nullptr,
			nullptr,
			command->buffer_storage_bind.start_slot,
			RENOIR_CONSTANT_BUFFER_STORAGE_SIZE,
			uavs,
			0
		);

		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_BIND:
	{
		auto h = command->texture_bind.handle;
		switch(command->texture_bind.shader)
		{
		case RENOIR_SHADER_VERTEX:
			self->context->VSSetShaderResources(command->texture_bind.slot, 1, &h->texture.shader_view);
			self->context->VSSetSamplers(command->texture_bind.slot, 1, &command->texture_bind.sampler->sampler.sampler);
			break;
		case RENOIR_SHADER_PIXEL:
			self->context->PSSetShaderResources(command->texture_bind.slot, 1, &h->texture.shader_view);
			self->context->PSSetSamplers(command->texture_bind.slot, 1, &command->texture_bind.sampler->sampler.sampler);
			break;
		case RENOIR_SHADER_GEOMETRY:
			self->context->GSSetShaderResources(command->texture_bind.slot, 1, &h->texture.shader_view);
			self->context->GSSetSamplers(command->texture_bind.slot, 1, &command->texture_bind.sampler->sampler.sampler);
			break;
		case RENOIR_SHADER_COMPUTE:
			if (command->texture_bind.sampler == nullptr)
			{
				mn_assert_msg(_renoir_pixelformat_is_depth(h->texture.desc.pixel_format) == false, "you can't write to depth buffer from compute shader");
				if (command->texture_bind.gpu_access == RENOIR_ACCESS_READ)
				{
					self->context->CSSetShaderResources(command->texture_bind.slot, 1, &h->texture.shader_view);
				}
				else if (command->texture_bind.gpu_access == RENOIR_ACCESS_WRITE ||
						command->texture_bind.gpu_access == RENOIR_ACCESS_READ_WRITE)
				{
					self->context->CSSetUnorderedAccessViews(command->texture_bind.slot, 1, &h->texture.uavs[command->texture_bind.mip_level], nullptr);
					Renoir_Compute_Write_Slot write_slot{};
					write_slot.resource = h;
					write_slot.slot = command->texture_bind.slot;
					mn::buf_push(self->current_pass->compute_pass.write_resources, write_slot);
				}
			}
			else
			{
				self->context->CSSetShaderResources(command->texture_bind.slot, 1, &h->texture.shader_view);
				self->context->CSSetSamplers(command->texture_bind.slot, 1, &command->texture_bind.sampler->sampler.sampler);
			}
			break;
		default:
			mn_unreachable();
			break;
		}
		break;
	}
	case RENOIR_COMMAND_KIND_DRAW:
	{
		mn_assert_msg(self->current_pipeline && self->current_program, "you should use a program and a pipeline before drawing");

		auto& desc = command->draw.desc;
		auto hprogram = self->current_program;
		if (hprogram->program.input_layout == nullptr)
			_renoir_dx11_input_layout_create(self, hprogram, desc);

		self->context->IASetInputLayout(hprogram->program.input_layout);
		switch(desc.primitive)
		{
		case RENOIR_PRIMITIVE_POINTS:
			self->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			break;
		case RENOIR_PRIMITIVE_LINES:
			self->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
			break;
		case RENOIR_PRIMITIVE_TRIANGLES:
			self->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			break;
		default:
			mn_unreachable();
			break;
		}

		for (size_t i = 0; i < RENOIR_CONSTANT_DRAW_VERTEX_BUFFER_SIZE; ++i)
		{
			auto& vertex_buffer = desc.vertex_buffers[i];
			if (vertex_buffer.buffer.handle == nullptr)
				continue;

			// calculate the default stride for the vertex buffer
			if (vertex_buffer.stride == 0)
				vertex_buffer.stride = _renoir_type_to_size(vertex_buffer.type);

			auto hbuffer = (Renoir_Handle*)vertex_buffer.buffer.handle;
			UINT offset = vertex_buffer.offset;
			UINT stride = vertex_buffer.stride;
			self->context->IASetVertexBuffers(i, 1, &hbuffer->buffer.buffer, &stride, &offset);
		}

		if (desc.index_buffer.handle != nullptr)
		{
			if (desc.index_type == RENOIR_TYPE_NONE)
				desc.index_type = RENOIR_TYPE_UINT16;

			auto dx_type = _renoir_type_to_dx(desc.index_type);
			auto dx_type_size = _renoir_type_to_size(desc.index_type);
			auto hbuffer = (Renoir_Handle*)desc.index_buffer.handle;
			self->context->IASetIndexBuffer(hbuffer->buffer.buffer, dx_type, desc.base_element * dx_type_size);

			if (desc.instances_count > 1)
			{
				self->context->DrawIndexedInstanced(
					desc.elements_count,
					desc.instances_count,
					0,
					0,
					0
				);
			}
			else
			{
				self->context->DrawIndexed(
					desc.elements_count,
					0,
					0
				);
			}
		}
		else
		{
			if (desc.instances_count > 1)
			{
				self->context->DrawInstanced(
					desc.elements_count,
					desc.instances_count,
					desc.base_element,
					0
				);
			}
			else
			{
				self->context->Draw(desc.elements_count, desc.base_element);
			}
		}
		break;
	}
	case RENOIR_COMMAND_KIND_DISPATCH:
	{
		mn_assert_msg(self->current_compute, "you should use a compute before dispatching it");
		self->context->Dispatch(command->dispatch.x, command->dispatch.y, command->dispatch.z);
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_BEGIN:
	{
		auto h = command->timer_begin.handle;
		self->context->Begin(h->timer.frequency);
		self->context->End(h->timer.start);
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_END:
	{
		auto h = command->timer_begin.handle;
		self->context->End(h->timer.end);
		self->context->End(h->timer.frequency);
		break;
	}
	default:
		mn_unreachable();
		break;
	}
}

inline static void
_renoir_dx11_handle_leak_free(IRenoir* self, Renoir_Command* command)
{
	switch(command->kind)
	{
	case RENOIR_COMMAND_KIND_SWAPCHAIN_FREE:
	{
		auto h = command->swapchain_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_FREE:
	{
		auto h = command->pass_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			// free all the bound textures if it's a framebuffer pass
			if (h->raster_pass.swapchain == nullptr)
			{
				for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
				{
					auto color = (Renoir_Handle*)h->raster_pass.offscreen.color[i].texture.handle;
					if (color == nullptr)
						continue;

					// issue command to free the color texture
					auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = color;
					_renoir_dx11_handle_leak_free(self, command);
				}

				auto depth = (Renoir_Handle*)h->raster_pass.offscreen.depth_stencil.texture.handle;
				if (depth)
				{
					// issue command to free the depth texture
					auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = depth;
					_renoir_dx11_handle_leak_free(self, command);
				}
			}
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			mn::buf_free(h->compute_pass.write_resources);
		}
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	{
		auto h = command->buffer_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	{
		auto h = command->texture_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		mn::buf_free(h->texture.uavs);
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	{
		auto h = command->sampler_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	{
		auto h = command->program_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
	{
		auto h = command->compute_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PIPELINE_FREE:
	{
		auto h = command->pipeline_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		_renoir_dx11_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_FREE:
	{
		auto h = command->timer_free.handle;
		if (_renoir_dx11_handle_unref(h) == false)
			break;
		_renoir_dx11_handle_free(self, h);
		break;
	}
	}
}

// API
static bool
_renoir_dx11_init(Renoir* api, Renoir_Settings settings, void*)
{
	static_assert(RENOIR_CONSTANT_DEFAULT_SAMPLER_CACHE_SIZE > 0, "sampler cache size should be > 0");
	static_assert(RENOIR_CONSTANT_DEFAULT_PIPELINE_CACHE_SIZE > 0, "pipeline cache size should be > 0");

	if (settings.sampler_cache_size <= 0)
		settings.sampler_cache_size = RENOIR_CONSTANT_DEFAULT_SAMPLER_CACHE_SIZE;
	if (settings.pipeline_cache_size <= 0)
		settings.pipeline_cache_size = RENOIR_CONSTANT_DEFAULT_PIPELINE_CACHE_SIZE;

	IDXGIFactory* factory = nullptr;
	IDXGIAdapter* adapter = nullptr;
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	mn_defer({
		if (factory) factory->Release();
		if (adapter) adapter->Release();
		if (device) device->Release();
		if (context) context->Release();
	});
	if (settings.external_context == false)
	{
		// create dx11 factory
		auto res = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
		if (FAILED(res))
			return false;

		// choose adapter/GPU
		res = factory->EnumAdapters(0, &adapter);
		if (FAILED(res))
			return false;

		const D3D_FEATURE_LEVEL feature_levels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};

		UINT creation_flags = 0;
		#if RENOIR_DEBUG_LAYER
			creation_flags = D3D11_CREATE_DEVICE_DEBUG;
		#endif

		// create device and device context
		res = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			creation_flags,
			feature_levels,
			2,
			D3D11_SDK_VERSION,
			&device,
			nullptr,
			&context
		);
		if (FAILED(res))
			return false;
	}

	auto self = mn::alloc_zerod<IRenoir>();
	self->mtx = mn_mutex_new_with_srcloc("renoir dx11");
	self->factory = factory; factory = nullptr;
	self->adapter = adapter; adapter = nullptr;
	self->device = device; device = nullptr;
	self->context = context; context = nullptr;
	self->handle_pool = mn::pool_new(sizeof(Renoir_Handle), 128);
	self->command_pool = mn::pool_new(sizeof(Renoir_Command), 128);
	self->settings = settings;
	self->info_description = mn::str_new();
	self->sampler_cache = mn::buf_new<Renoir_Handle*>();
	self->pipeline_cache = mn::buf_new<Renoir_Handle*>();
	self->alive_handles = mn::map_new<Renoir_Handle*, Renoir_Leak_Info>();
	mn::buf_resize_fill(self->sampler_cache, self->settings.sampler_cache_size, nullptr);
	mn::buf_resize_fill(self->pipeline_cache, self->settings.pipeline_cache_size, nullptr);

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_INIT);
	_renoir_dx11_command_process(self, command);

	api->ctx = self;
	return true;
}

static void
_renoir_dx11_dispose(Renoir* api)
{
	auto self = api->ctx;

	// free the default pipeline
	_renoir_dx11_pipeline_free(self, self->default_pipeline);
	// free the depth resolve compute shader
	if (self->depth_resolve_compute) self->depth_resolve_compute->Release();
	self->depth_resolve_compute = nullptr;

	// process these commands for frees to give correct leak report
	for (auto it = self->command_list_head; it != nullptr; it = it->next)
		_renoir_dx11_handle_leak_free(self, it);
	#if RENOIR_LEAK
		for(auto[handle, info]: self->alive_handles)
		{
			::fprintf(stderr, "renoir handle to '%s' leaked, callstack:\n", _renoir_handle_kind_name(handle->kind));
			mn::callstack_print_to(info.callstack, info.callstack_size, mn::file_stderr());
			::fprintf(stderr, "\n\n");
		}
		if (self->alive_handles.count > 0)
			::fprintf(stderr, "renoir leak count: %zu\n", self->alive_handles.count);
	#else
		if (self->alive_handles.count > 0)
			::fprintf(stderr, "renoir leak count: %zu, for callstack turn on 'RENOIR_LEAK' flag\n", self->alive_handles.count);
	#endif
	mn::mutex_free(self->mtx);
	if (self->settings.external_context == false)
	{
		self->factory->Release();
		self->adapter->Release();
		self->device->Release();
		self->context->Release();
	}
	mn::pool_free(self->handle_pool);
	mn::pool_free(self->command_pool);
	mn::str_free(self->info_description);
	mn::buf_free(self->sampler_cache);
	mn::buf_free(self->pipeline_cache);
	mn::map_free(self->alive_handles);
	mn::free(self);
}

static const char*
_renoir_dx11_name()
{
	return "dx11";
}

static RENOIR_TEXTURE_ORIGIN
_renoir_dx11_texture_origin()
{
	return RENOIR_TEXTURE_ORIGIN_TOP_LEFT;
}

static Renoir_Info
_renoir_dx11_info(Renoir* api)
{
	auto self = api->ctx;

	Renoir_Info res{};
	res.description = self->info_description.ptr;
	res.gpu_memory_in_bytes = self->gpu_memory_in_bytes;
	return res;
}

static void
_renoir_dx11_handle_ref(Renoir* api, void* handle)
{
	auto h = (Renoir_Handle*)handle;
	h->rc.fetch_add(1);
}

static void
_renoir_dx11_flush(Renoir* api, void* device, void* context)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	self->device = (ID3D11Device*)device;
	self->context = (ID3D11DeviceContext*)context;

	// process commands
	for(auto it = self->command_list_head; it != nullptr; it = it->next)
	{
		_renoir_dx11_command_execute(self, it);
		_renoir_dx11_command_free(self, it);
	}

	self->command_list_head = nullptr;
	self->command_list_tail = nullptr;
}

static Renoir_Swapchain
_renoir_dx11_swapchain_new(Renoir* api, int width, int height, void* window, void*)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_SWAPCHAIN);
	h->swapchain.width = width;
	h->swapchain.height = height;
	h->swapchain.window = window;

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_SWAPCHAIN_NEW);
	command->swapchain_new.handle = h;
	_renoir_dx11_command_process(self, command);
	return Renoir_Swapchain{h};
}

static void
_renoir_dx11_swapchain_free(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_SWAPCHAIN_FREE);
	command->swapchain_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

static void
_renoir_dx11_swapchain_resize(Renoir* api, Renoir_Swapchain swapchain, int width, int height)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_SWAPCHAIN_RESIZE);
	command->swapchain_resize.handle = h;
	command->swapchain_resize.width = width;
	command->swapchain_resize.height = height;
	_renoir_dx11_command_process(self, command);
}

static void
_renoir_dx11_swapchain_present(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	// process commands
	for(auto it = self->command_list_head; it != nullptr; it = it->next)
	{
		_renoir_dx11_command_execute(self, it);
		_renoir_dx11_command_free(self, it);
	}

	self->command_list_head = nullptr;
	self->command_list_tail = nullptr;

	if (self->settings.vsync == RENOIR_VSYNC_MODE_ON)
		h->swapchain.swapchain->Present(1, 0);
	else
		h->swapchain.swapchain->Present(0, 0);
}

static Renoir_Buffer
_renoir_dx11_buffer_new(Renoir* api, Renoir_Buffer_Desc desc)
{
	if (desc.usage == RENOIR_USAGE_NONE)
		desc.usage = RENOIR_USAGE_STATIC;

	if (desc.usage == RENOIR_USAGE_DYNAMIC && desc.access == RENOIR_ACCESS_NONE)
	{
		mn_unreachable_msg("a dynamic buffer with cpu access set to none is a static buffer");
	}

	if (desc.type == RENOIR_BUFFER_UNIFORM && desc.data_size % 16 != 0)
	{
		mn_unreachable_msg("uniform buffers should be aligned to 16 bytes");
	}

	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_BUFFER);
	h->buffer.type = desc.type;
	h->buffer.usage = desc.usage;
	h->buffer.access = desc.access;
	h->buffer.size = desc.data_size;

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_NEW);
	command->buffer_new.handle = h;
	command->buffer_new.desc = desc;
	if (self->settings.defer_api_calls)
	{
		if (desc.data)
		{
			command->buffer_new.desc.data = mn::alloc(desc.data_size, alignof(char)).ptr;
			::memcpy(command->buffer_new.desc.data, desc.data, desc.data_size);
			command->buffer_new.owns_data = true;
		}
	}
	_renoir_dx11_command_process(self, command);
	return Renoir_Buffer{h};
}

static void
_renoir_dx11_buffer_free(Renoir* api, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_FREE);
	command->buffer_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

static size_t
_renoir_dx11_buffer_size(Renoir* api, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr && h->kind == RENOIR_HANDLE_KIND_BUFFER);

	return h->buffer.size;
}

static Renoir_Texture
_renoir_dx11_texture_new(Renoir* api, Renoir_Texture_Desc desc)
{
	mn_assert_msg(desc.size.width > 0, "a texture must have at least width");

	if (desc.usage == RENOIR_USAGE_NONE)
		desc.usage = RENOIR_USAGE_STATIC;

	if (desc.mipmaps == 0)
		desc.mipmaps = 1;

	if (desc.usage == RENOIR_USAGE_DYNAMIC && desc.access == RENOIR_ACCESS_NONE)
	{
		mn_unreachable_msg("a dynamic texture with cpu access set to none is a static texture");
	}

	if (desc.usage == RENOIR_USAGE_STATIC && (desc.access == RENOIR_ACCESS_WRITE || desc.access == RENOIR_ACCESS_READ_WRITE))
	{
		mn_unreachable_msg("a static texture cannot have write access");
	}

	if (desc.cube_map)
	{
		mn_assert_msg(desc.size.width == desc.size.height, "width should equal height in cube map texture");
	}

	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_TEXTURE);
	h->texture.desc = desc;
	::memset(h->texture.desc.data, 0, sizeof(h->texture.desc.data));
	h->texture.desc.data_size = 0;

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_NEW);
	command->texture_new.handle = h;
	command->texture_new.desc = desc;
	if (self->settings.defer_api_calls)
	{
		for (int i = 0; i < 6; ++i)
		{
			if (desc.data[i] == nullptr)
				continue;

			command->texture_new.desc.data[i] = mn::alloc(desc.data_size, alignof(char)).ptr;
			::memcpy(command->texture_new.desc.data[i], desc.data[i], desc.data_size);
			command->texture_new.owns_data = true;
		}
	}
	_renoir_dx11_command_process(self, command);
	return Renoir_Texture{h};
}

static void
_renoir_dx11_texture_free(Renoir* api, Renoir_Texture texture)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
	command->texture_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

static void*
_renoir_dx11_texture_native_handle(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);
	if (h->texture.texture1d)
		return h->texture.texture1d;
	else if (h->texture.texture2d)
		return h->texture.texture2d;
	else if (h->texture.texture3d)
		return h->texture.texture3d;
	return nullptr;
}

static Renoir_Size
_renoir_dx11_texture_size(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr && h->kind == RENOIR_HANDLE_KIND_TEXTURE);
	return h->texture.desc.size;
}

static Renoir_Texture_Desc
_renoir_dx11_texture_desc(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_TEXTURE);
	return h->texture.desc;
}

static Renoir_Program
_renoir_dx11_program_new(Renoir* api, Renoir_Program_Desc desc)
{
	mn_assert(desc.vertex.bytes != nullptr && desc.pixel.bytes != nullptr);
	if (desc.vertex.size == 0)
		desc.vertex.size = ::strlen(desc.vertex.bytes);
	if (desc.pixel.size == 0)
		desc.pixel.size = ::strlen(desc.pixel.bytes);
	if (desc.geometry.bytes != nullptr && desc.geometry.size == 0)
		desc.geometry.size = ::strlen(desc.geometry.bytes);

	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_PROGRAM);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PROGRAM_NEW);
	command->program_new.handle = h;
	command->program_new.desc = desc;
	if (self->settings.defer_api_calls)
	{
		command->program_new.desc.vertex.bytes = (char*)mn::alloc(command->program_new.desc.vertex.size, alignof(char)).ptr;
		::memcpy((char*)command->program_new.desc.vertex.bytes, desc.vertex.bytes, desc.vertex.size);

		command->program_new.desc.pixel.bytes = (char*)mn::alloc(command->program_new.desc.pixel.size, alignof(char)).ptr;
		::memcpy((char*)command->program_new.desc.pixel.bytes, desc.pixel.bytes, desc.pixel.size);

		if (command->program_new.desc.geometry.bytes != nullptr)
		{
			command->program_new.desc.geometry.bytes = (char*)mn::alloc(command->program_new.desc.geometry.size, alignof(char)).ptr;
			::memcpy((char*)command->program_new.desc.geometry.bytes, desc.geometry.bytes, desc.geometry.size);
		}

		command->program_new.owns_data = true;
	}
	_renoir_dx11_command_process(self, command);
	return Renoir_Program{h};
}

static void
_renoir_dx11_program_free(Renoir* api, Renoir_Program program)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)program.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PROGRAM_FREE);
	command->program_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

static Renoir_Compute
_renoir_dx11_compute_new(Renoir* api, Renoir_Compute_Desc desc)
{
	mn_assert(desc.compute.bytes != nullptr);
	if (desc.compute.size == 0)
		desc.compute.size = ::strlen(desc.compute.bytes);

	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_COMPUTE);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_COMPUTE_NEW);
	command->compute_new.handle = h;
	command->compute_new.desc = desc;
	if (self->settings.defer_api_calls)
	{
		command->compute_new.desc.compute.bytes = (char*)mn::alloc(command->compute_new.desc.compute.size, alignof(char)).ptr;
		::memcpy((char*)command->compute_new.desc.compute.bytes, desc.compute.bytes, desc.compute.size);

		command->compute_new.owns_data = true;
	}
	_renoir_dx11_command_process(self, command);
	return Renoir_Compute{h};
}

static void
_renoir_dx11_compute_free(Renoir* api, Renoir_Compute compute)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)compute.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_COMPUTE_FREE);
	command->compute_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

static Renoir_Pipeline
_renoir_dx11_pipeline_new(Renoir* api, Renoir_Pipeline_Desc desc)
{
	auto self = api->ctx;

	_renoir_dx11_pipeline_desc_defaults(&desc);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_PIPELINE);
	h->pipeline.desc = desc;

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PIPELINE_NEW);
	command->pipeline_new.handle = h;
	_renoir_dx11_command_process(self, command);

	return Renoir_Pipeline{h};
}

static void
_renoir_dx11_pipeline_free(Renoir* api, Renoir_Pipeline pipeline)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pipeline.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_PIPELINE);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PIPELINE_FREE);
	command->pipeline_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

static Renoir_Pass
_renoir_dx11_pass_swapchain_new(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_RASTER_PASS);
	h->raster_pass.swapchain = (Renoir_Handle*)swapchain.handle;

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_SWAPCHAIN_NEW);
	command->pass_swapchain_new.handle = h;
	_renoir_dx11_command_process(self, command);
	return Renoir_Pass{h};
}

static Renoir_Pass
_renoir_dx11_pass_offscreen_new(Renoir* api, Renoir_Pass_Offscreen_Desc desc)
{
	auto self = api->ctx;

	// check that all sizes match
	int width = -1, height = -1;
	for (int i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
	{
		auto color = (Renoir_Handle*)desc.color[i].texture.handle;
		if (color == nullptr)
			continue;

		// first time getting the width/height
		if (width == -1 && height == -1)
		{
			width = color->texture.desc.size.width * ::powf(0.5f, desc.color[i].level);
			height = color->texture.desc.size.height * ::powf(0.5f, desc.color[i].level);
		}
		else
		{
			mn_assert(color->texture.desc.size.width * ::powf(0.5f, desc.color[i].level) == width);
			mn_assert(color->texture.desc.size.height * ::powf(0.5f, desc.color[i].level) == height);
		}
	}

	auto depth = (Renoir_Handle*)desc.depth_stencil.texture.handle;
	if (depth)
	{
		// first time getting the width/height
		if (width == -1 && height == -1)
		{
			width = depth->texture.desc.size.width * ::powf(0.5f, desc.depth_stencil.level);
			height = depth->texture.desc.size.height * ::powf(0.5f, desc.depth_stencil.level);
		}
		else
		{
			mn_assert(depth->texture.desc.size.width * ::powf(0.5f, desc.depth_stencil.level) == width);
			mn_assert(depth->texture.desc.size.height * ::powf(0.5f, desc.depth_stencil.level) == height);
		}
	}

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_RASTER_PASS);
	h->raster_pass.offscreen = desc;
	h->raster_pass.width = width;
	h->raster_pass.height = height;

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW);
	command->pass_offscreen_new.handle = h;
	command->pass_offscreen_new.desc = desc;
	_renoir_dx11_command_process(self, command);
	return Renoir_Pass{h};
}

static Renoir_Pass
_renoir_dx11_pass_compute_new(Renoir* api)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_COMPUTE_PASS);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_COMPUTE_NEW);
	command->pass_compute_new.handle = h;
	_renoir_dx11_command_process(self, command);
	return Renoir_Pass{h};
}

static void
_renoir_dx11_pass_free(Renoir* api, Renoir_Pass pass)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_FREE);
	command->pass_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

static Renoir_Size
_renoir_dx11_pass_size(Renoir* api, Renoir_Pass pass)
{
	Renoir_Size res{};
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	// if this is an on screen/window
	if (auto swapchain = h->raster_pass.swapchain)
	{
		res.width = swapchain->swapchain.width;
		res.height = swapchain->swapchain.height;
	}
	// this must be an offscreen pass then
	else
	{
		res.width = h->raster_pass.width;
		res.height = h->raster_pass.height;
	}
	return res;
}

static Renoir_Pass_Offscreen_Desc
_renoir_dx11_pass_offscreen_desc(Renoir* api, Renoir_Pass pass)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
	return h->raster_pass.offscreen;
}

static Renoir_Timer
_renoir_dx11_timer_new(Renoir* api)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_dx11_handle_new(self, RENOIR_HANDLE_KIND_TIMER);

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TIMER_NEW);
	command->timer_new.handle = h;
	_renoir_dx11_command_process(self, command);
	return Renoir_Timer{h};
}

static void
_renoir_dx11_timer_free(struct Renoir* api, Renoir_Timer timer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)timer.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TIMER_FREE);
	command->timer_free.handle = h;
	_renoir_dx11_command_process(self, command);
}

static bool
_renoir_dx11_timer_elapsed(struct Renoir* api, Renoir_Timer timer, uint64_t* elapsed_time_in_nanos)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)timer.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_TIMER);

	if (h->timer.state == RENOIR_TIMER_STATE_READY)
	{
		if (elapsed_time_in_nanos) *elapsed_time_in_nanos = h->timer.elapsed_time_in_nanos;
		h->timer.state = RENOIR_TIMER_STATE_NONE;
		return true;
	}
	else if (h->timer.state == RENOIR_TIMER_STATE_END)
	{
		mn::mutex_lock(self->mtx);
		auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TIMER_ELAPSED);
		h->timer.state = RENOIR_TIMER_STATE_READ_SCHEDULED;
		mn::mutex_unlock(self->mtx);

		command->timer_elapsed.handle = h;
		_renoir_dx11_command_process(self, command);

		return false;
	}

	return false;
}

// Graphics Commands
static void
_renoir_dx11_pass_submit(Renoir* api, Renoir_Pass pass)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
	{
		if (h->raster_pass.command_list_head != nullptr)
		{
			mn::mutex_lock(self->mtx);

			// push the pass begin command
			{
				auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_BEGIN);
				command->pass_begin.handle = h;
				_renoir_dx11_command_push_front(&h->raster_pass, command);
			}

			// push the pass end command
			{
				auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_END);
				command->pass_end.handle = h;
				_renoir_dx11_command_push_back(&h->raster_pass, command);
			}

			// push the commands to the end of command list, if the user requested to defer api calls
			if (self->settings.defer_api_calls)
			{
				if (self->command_list_tail == nullptr)
				{
					self->command_list_head = h->raster_pass.command_list_head;
					self->command_list_tail = h->raster_pass.command_list_tail;
				}
				else
				{
					self->command_list_tail->next = h->raster_pass.command_list_head;
					self->command_list_tail = h->raster_pass.command_list_tail;
				}
			}
			// other than this just process the command
			else
			{
				for(auto it = h->raster_pass.command_list_head; it != nullptr; it = it->next)
				{
					_renoir_dx11_command_execute(self, it);
					_renoir_dx11_command_free(self, it);
				}
			}

			// clear
			h->raster_pass.command_list_head = nullptr;
			h->raster_pass.command_list_tail = nullptr;

			mn::mutex_unlock(self->mtx);
		}
		h->raster_pass.command_list_head = nullptr;
		h->raster_pass.command_list_tail = nullptr;
	}
	else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
	{
		if (h->compute_pass.command_list_head != nullptr)
		{
			mn::mutex_lock(self->mtx);

			// push the pass begin command
			{
				auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_BEGIN);
				command->pass_begin.handle = h;
				_renoir_dx11_command_push_front(&h->compute_pass, command);
			}

			// push the pass end command
			{
				auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_END);
				command->pass_end.handle = h;
				_renoir_dx11_command_push_back(&h->compute_pass, command);
			}

			// push the commands to the end of command list, if the user requested to defer api calls
			if (self->settings.defer_api_calls)
			{
				if (self->command_list_tail == nullptr)
				{
					self->command_list_head = h->compute_pass.command_list_head;
					self->command_list_tail = h->compute_pass.command_list_tail;
				}
				else
				{
					self->command_list_tail->next = h->compute_pass.command_list_head;
					self->command_list_tail = h->compute_pass.command_list_tail;
				}
			}
			// other than this just process the command
			else
			{
				for(auto it = h->compute_pass.command_list_head; it != nullptr; it = it->next)
				{
					_renoir_dx11_command_execute(self, it);
					_renoir_dx11_command_free(self, it);
				}
			}

			mn::mutex_unlock(self->mtx);
		}
		h->compute_pass.command_list_head = nullptr;
		h->compute_pass.command_list_tail = nullptr;
	}
	else
	{
		mn_unreachable_msg("invalid pass");
	}
}

static void
_renoir_dx11_clear(Renoir* api, Renoir_Pass pass, Renoir_Clear_Desc desc)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	if (desc.independent_clear_color == RENOIR_SWITCH_DEFAULT)
		desc.independent_clear_color = RENOIR_SWITCH_DISABLE;

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_PASS_CLEAR);
	mn::mutex_unlock(self->mtx);

	command->pass_clear.desc = desc;
	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_use_pipeline(Renoir* api, Renoir_Pass pass, Renoir_Pipeline pipeline)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	auto h_pipeline = (Renoir_Handle*)pipeline.handle;
	mn_assert(h_pipeline->kind == RENOIR_HANDLE_KIND_PIPELINE);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_USE_PIPELINE);
	mn::mutex_unlock(self->mtx);

	command->use_pipeline.pipeline = h_pipeline;
	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_use_program(Renoir* api, Renoir_Pass pass, Renoir_Program program)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_USE_PROGRAM);
	mn::mutex_unlock(self->mtx);

	command->use_program.program = (Renoir_Handle*)program.handle;
	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_use_compute(Renoir* api, Renoir_Pass pass, Renoir_Compute compute)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_USE_COMPUTE);
	mn::mutex_unlock(self->mtx);

	command->use_compute.compute = (Renoir_Handle*)compute.handle;
	_renoir_dx11_command_push_back(&h->compute_pass, command);
}

static void
_renoir_dx11_scissor(Renoir* api, Renoir_Pass pass, int x, int y, int width, int height)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_SCISSOR);
	mn::mutex_unlock(self->mtx);

	command->scissor.x = x;
	command->scissor.y = y;
	command->scissor.w = width;
	command->scissor.h = height;
	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_buffer_zero_global(Renoir* api, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto hbuffer = (Renoir_Handle*)buffer.handle;
	mn_assert(hbuffer != nullptr);

	mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_CLEAR);
	command->buffer_clear.handle = hbuffer;
	_renoir_dx11_command_process(self, command);
}

static void
_renoir_dx11_buffer_zero(Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	if (h == nullptr)
	{
		_renoir_dx11_buffer_zero_global(api, buffer);
	}
	else
	{
		auto hbuffer = (Renoir_Handle*)buffer.handle;
		mn_assert(hbuffer != nullptr);

		mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);
		mn_assert(hbuffer->buffer.uav);

		mn::mutex_lock(self->mtx);
		auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_CLEAR);
		mn::mutex_unlock(self->mtx);

		command->buffer_clear.handle = hbuffer;

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			_renoir_dx11_command_push_back(&h->raster_pass, command);
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			_renoir_dx11_command_push_back(&h->compute_pass, command);
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
	}
}

static void
_renoir_dx11_buffer_write_global(Renoir* api, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size)
{
	auto self = api->ctx;
	auto hbuffer = (Renoir_Handle*)buffer.handle;
	mn_assert(hbuffer != nullptr);

	mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_WRITE);
	command->buffer_write.handle = hbuffer;
	command->buffer_write.offset = offset;
	command->buffer_write.bytes = mn::alloc(bytes_size, alignof(char)).ptr;
	command->buffer_write.bytes_size = bytes_size;
	::memcpy(command->buffer_write.bytes, bytes, bytes_size);
	_renoir_dx11_command_process(self, command);
}

static void
_renoir_dx11_buffer_write(Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size)
{
	// this means he's trying to write nothing so no-op
	if (bytes_size == 0)
		return;

	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	if (h == nullptr)
	{
		_renoir_dx11_buffer_write_global(api, buffer, offset, bytes, bytes_size);
	}
	else
	{
		auto hbuffer = (Renoir_Handle*)buffer.handle;
		mn_assert(hbuffer != nullptr);

		mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);

		mn::mutex_lock(self->mtx);
		auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_WRITE);
		mn::mutex_unlock(self->mtx);

		command->buffer_write.handle = hbuffer;
		command->buffer_write.offset = offset;
		command->buffer_write.bytes = mn::alloc(bytes_size, alignof(char)).ptr;
		command->buffer_write.bytes_size = bytes_size;
		::memcpy(command->buffer_write.bytes, bytes, bytes_size);

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			_renoir_dx11_command_push_back(&h->raster_pass, command);
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			_renoir_dx11_command_push_back(&h->compute_pass, command);
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
	}
}

static void
_renoir_dx11_texture_write_global(Renoir* api, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc)
{
	auto self = api->ctx;

	auto htexture = (Renoir_Handle*)texture.handle;
	mn_assert(htexture->texture.desc.usage != RENOIR_USAGE_STATIC);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_WRITE);
	command->texture_write.handle = htexture;
	command->texture_write.desc = desc;
	command->texture_write.desc.bytes = mn::alloc(desc.bytes_size, alignof(char)).ptr;
	::memcpy(command->texture_write.desc.bytes, desc.bytes, desc.bytes_size);
	_renoir_dx11_command_process(self, command);
}

static void
_renoir_dx11_texture_write(Renoir* api, Renoir_Pass pass, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc)
{
	// this means he's trying to write nothing so no-op
	if (desc.bytes_size == 0)
		return;

	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	if (h == nullptr)
	{
		_renoir_dx11_texture_write_global(api, texture, desc);
	}
	else
	{
		auto htexture = (Renoir_Handle*)texture.handle;
		mn_assert(htexture->texture.desc.usage != RENOIR_USAGE_STATIC);

		mn::mutex_lock(self->mtx);
		auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_WRITE);
		mn::mutex_unlock(self->mtx);

		command->texture_write.handle = htexture;
		command->texture_write.desc = desc;
		command->texture_write.desc.bytes = mn::alloc(desc.bytes_size, alignof(char)).ptr;
		::memcpy(command->texture_write.desc.bytes, desc.bytes, desc.bytes_size);

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			_renoir_dx11_command_push_back(&h->raster_pass, command);
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			_renoir_dx11_command_push_back(&h->compute_pass, command);
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
	}
}

static void
_renoir_dx11_buffer_read(Renoir* api, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size)
{
	// this means he's trying to read nothing so no-op
	if (bytes_size == 0)
		return;

	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr);
	// this means that buffer creation didn't execute yet
	if (h->buffer.buffer == nullptr)
	{
		::memset(bytes, 0, bytes_size);
		return;
	}

	auto self = api->ctx;

	Renoir_Command command{};
	command.kind = RENOIR_COMMAND_KIND_BUFFER_READ;
	command.buffer_read.handle = h;
	command.buffer_read.offset = offset;
	command.buffer_read.bytes = bytes;
	command.buffer_read.bytes_size = bytes_size;

	mn::mutex_lock(self->mtx);
	_renoir_dx11_command_execute(self, &command);
	mn::mutex_unlock(self->mtx);
}

static void
_renoir_dx11_texture_read(Renoir* api, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc)
{
	// this means he's trying to read nothing so no-op
	if (desc.bytes_size == 0)
		return;

	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);
	// this means that texture creation didn't execute yet
	if (h->texture.texture1d == nullptr && h->texture.texture2d == nullptr && h->texture.texture3d == nullptr)
	{
		::memset(desc.bytes, 0, desc.bytes_size);
		return;
	}

	auto self = api->ctx;

	Renoir_Command command{};
	command.kind = RENOIR_COMMAND_KIND_TEXTURE_READ;
	command.texture_read.handle = h;
	command.texture_read.desc = desc;

	mn::mutex_lock(self->mtx);
	_renoir_dx11_command_execute(self, &command);
	mn::mutex_unlock(self->mtx);
}

static void
_renoir_dx11_buffer_bind(Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, RENOIR_SHADER shader, int slot)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_BIND);
	mn::mutex_unlock(self->mtx);

	command->buffer_bind.handle = (Renoir_Handle*)buffer.handle;
	command->buffer_bind.shader = shader;
	command->buffer_bind.slot = slot;
	command->buffer_bind.gpu_access = RENOIR_ACCESS_NONE;

	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_buffer_storage_bind(Renoir* api, Renoir_Pass pass, Renoir_Buffer_Storage_Bind_Desc desc)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_STORAGE_BIND);
	mn::mutex_unlock(self->mtx);

	size_t render_target_count = 0;
	if (h->raster_pass.swapchain)
	{
		render_target_count = 1;
	}
	else
	{
		for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
		{
			auto texture = h->raster_pass.offscreen.color[i].texture;
			if (texture.handle == nullptr)
				continue;
			++render_target_count;
		}
	}
	mn_assert(desc.start_slot + render_target_count <= 8);
	mn_assert(desc.start_slot >= render_target_count);

	for (int i = 0; i < RENOIR_CONSTANT_BUFFER_STORAGE_SIZE; ++i)
	{
		auto h = (Renoir_Handle*)desc.buffers[i].handle;
		if (desc.buffers[i].handle)
		{
			mn_assert(h->buffer.uav);
			command->buffer_storage_bind.handle[i] = h;
		}
	}
	command->buffer_storage_bind.start_slot = desc.start_slot;

	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_texture_bind(Renoir* api, Renoir_Pass pass, Renoir_Texture texture, RENOIR_SHADER shader, int slot)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htex = (Renoir_Handle*)texture.handle;
	mn_assert(htex != nullptr);

	mn::mutex_lock(self->mtx);
	auto hsampler = _renoir_dx11_sampler_get(self, htex->texture.desc.sampler);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_BIND);
	mn::mutex_unlock(self->mtx);

	command->texture_bind.handle = htex;
	command->texture_bind.shader = shader;
	command->texture_bind.slot = slot;
	command->texture_bind.sampler = hsampler;

	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_texture_sampler_bind(Renoir* api, Renoir_Pass pass, Renoir_Texture texture, RENOIR_SHADER shader, int slot, Renoir_Sampler_Desc sampler)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htex = (Renoir_Handle*)texture.handle;
	mn_assert(htex != nullptr);

	mn::mutex_lock(self->mtx);
	auto hsampler = _renoir_dx11_sampler_get(self, sampler);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_BIND);
	mn::mutex_unlock(self->mtx);

	command->texture_bind.handle = htex;
	command->texture_bind.shader = shader;
	command->texture_bind.slot = slot;
	command->texture_bind.sampler = hsampler;

	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_buffer_compute_bind(Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, int slot, RENOIR_ACCESS gpu_access)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
	mn_assert_msg(
		gpu_access != RENOIR_ACCESS_NONE,
		"gpu should read, write, or both, it has no meaning to bind a buffer that the GPU cannot read or write from"
	);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_BUFFER_BIND);
	mn::mutex_unlock(self->mtx);

	command->buffer_bind.handle = (Renoir_Handle*)buffer.handle;
	command->buffer_bind.shader = RENOIR_SHADER_COMPUTE;
	command->buffer_bind.slot = slot;
	command->buffer_bind.gpu_access = gpu_access;

	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_texture_compute_bind(Renoir* api, Renoir_Pass pass, Renoir_Texture texture, int slot, int mip_level, RENOIR_ACCESS gpu_access)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
	mn_assert_msg(
		gpu_access != RENOIR_ACCESS_NONE,
		"gpu should read, write, or both, it has no meaning to bind a texture that the GPU cannot read or write from"
	);

	if (gpu_access == RENOIR_ACCESS_READ)
	{
		mn_assert_msg(mip_level == 0, "read only textures are bound as samplers, so you can't change mip level");
	}

	auto htex = (Renoir_Handle*)texture.handle;

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_BIND);
	mn::mutex_unlock(self->mtx);

	command->texture_bind.handle = htex;
	command->texture_bind.shader = RENOIR_SHADER_COMPUTE;
	command->texture_bind.slot = slot;
	command->texture_bind.mip_level = mip_level;
	command->texture_bind.gpu_access = gpu_access;

	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_draw(Renoir* api, Renoir_Pass pass, Renoir_Draw_Desc desc)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_DRAW);
	mn::mutex_unlock(self->mtx);

	command->draw.desc = desc;

	_renoir_dx11_command_push_back(&h->raster_pass, command);
}

static void
_renoir_dx11_dispatch(Renoir* api, Renoir_Pass pass, int x, int y, int z)
{
	mn_assert(x >= 0 && y >= 0 && z >= 0);

	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_DISPATCH);
	mn::mutex_unlock(self->mtx);

	command->dispatch.x = x;
	command->dispatch.y = y;
	command->dispatch.z = z;

	_renoir_dx11_command_push_back(&h->compute_pass, command);
}

static void
_renoir_dx11_timer_begin(struct Renoir* api, Renoir_Pass pass, Renoir_Timer timer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htimer = (Renoir_Handle*)timer.handle;
	mn_assert(htimer != nullptr && htimer->kind == RENOIR_HANDLE_KIND_TIMER);

	if(htimer->timer.state != RENOIR_TIMER_STATE_NONE)
		return;

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TIMER_BEGIN);
	mn::mutex_unlock(self->mtx);

	command->timer_begin.handle = htimer;
	htimer->timer.state = RENOIR_TIMER_STATE_BEGIN;

	if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
	{
		_renoir_dx11_command_push_back(&h->raster_pass, command);
	}
	else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
	{
		_renoir_dx11_command_push_back(&h->compute_pass, command);
	}
	else
	{
		mn_unreachable();
	}
}

static void
_renoir_dx11_timer_end(struct Renoir* api, Renoir_Pass pass, Renoir_Timer timer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htimer = (Renoir_Handle*)timer.handle;
	mn_assert(htimer != nullptr && htimer->kind == RENOIR_HANDLE_KIND_TIMER);
	if (htimer->timer.state != RENOIR_TIMER_STATE_BEGIN)
		return;

	mn::mutex_lock(self->mtx);
	auto command = _renoir_dx11_command_new(self, RENOIR_COMMAND_KIND_TIMER_END);
	mn::mutex_unlock(self->mtx);

	command->timer_end.handle = htimer;
	htimer->timer.state = RENOIR_TIMER_STATE_END;

	if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
	{
		_renoir_dx11_command_push_back(&h->raster_pass, command);
	}
	else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
	{
		_renoir_dx11_command_push_back(&h->compute_pass, command);
	}
	else
	{
		mn_unreachable();
	}
}


inline static void
_renoir_load_api(Renoir* api)
{
	api->global_pass = Renoir_Pass{};

	api->init = _renoir_dx11_init;
	api->dispose = _renoir_dx11_dispose;

	api->name = _renoir_dx11_name;
	api->texture_origin = _renoir_dx11_texture_origin;
	api->info = _renoir_dx11_info;

	api->handle_ref = _renoir_dx11_handle_ref;
	api->flush = _renoir_dx11_flush;

	api->swapchain_new = _renoir_dx11_swapchain_new;
	api->swapchain_free = _renoir_dx11_swapchain_free;
	api->swapchain_resize = _renoir_dx11_swapchain_resize;
	api->swapchain_present = _renoir_dx11_swapchain_present;

	api->buffer_new = _renoir_dx11_buffer_new;
	api->buffer_free = _renoir_dx11_buffer_free;
	api->buffer_size = _renoir_dx11_buffer_size;

	api->texture_new = _renoir_dx11_texture_new;
	api->texture_free = _renoir_dx11_texture_free;
	api->texture_native_handle = _renoir_dx11_texture_native_handle;
	api->texture_size = _renoir_dx11_texture_size;
	api->texture_desc = _renoir_dx11_texture_desc;

	api->program_new = _renoir_dx11_program_new;
	api->program_free = _renoir_dx11_program_free;

	api->compute_new = _renoir_dx11_compute_new;
	api->compute_free = _renoir_dx11_compute_free;

	api->pipeline_new = _renoir_dx11_pipeline_new;
	api->pipeline_free = _renoir_dx11_pipeline_free;

	api->pass_swapchain_new = _renoir_dx11_pass_swapchain_new;
	api->pass_offscreen_new = _renoir_dx11_pass_offscreen_new;
	api->pass_compute_new = _renoir_dx11_pass_compute_new;
	api->pass_free = _renoir_dx11_pass_free;
	api->pass_size = _renoir_dx11_pass_size;
	api->pass_offscreen_desc = _renoir_dx11_pass_offscreen_desc;

	api->timer_new = _renoir_dx11_timer_new;
	api->timer_free = _renoir_dx11_timer_free;
	api->timer_elapsed = _renoir_dx11_timer_elapsed;

	api->pass_submit = _renoir_dx11_pass_submit;
	api->clear = _renoir_dx11_clear;
	api->use_pipeline = _renoir_dx11_use_pipeline;
	api->use_program = _renoir_dx11_use_program;
	api->use_compute = _renoir_dx11_use_compute;
	api->scissor = _renoir_dx11_scissor;
	api->buffer_zero = _renoir_dx11_buffer_zero;
	api->buffer_write = _renoir_dx11_buffer_write;
	api->texture_write = _renoir_dx11_texture_write;
	api->buffer_read = _renoir_dx11_buffer_read;
	api->texture_read = _renoir_dx11_texture_read;
	api->buffer_bind = _renoir_dx11_buffer_bind;
	api->buffer_storage_bind = _renoir_dx11_buffer_storage_bind;
	api->texture_bind = _renoir_dx11_texture_bind;
	api->texture_sampler_bind = _renoir_dx11_texture_sampler_bind;
	api->texture_compute_bind = _renoir_dx11_texture_compute_bind;
	api->buffer_compute_bind = _renoir_dx11_buffer_compute_bind;
	api->draw = _renoir_dx11_draw;
	api->dispatch = _renoir_dx11_dispatch;
	api->timer_begin = _renoir_dx11_timer_begin;
	api->timer_end = _renoir_dx11_timer_end;
}

extern "C" Renoir*
renoir_dx11_api()
{
	static Renoir _api;
	_renoir_load_api(&_api);
	return &_api;
}

extern "C" RENOIR_DX11_EXPORT void*
rad_api(void* api, bool reload)
{
	if (api == nullptr)
	{
		auto self = mn::alloc_zerod<Renoir>();
		_renoir_load_api(self);
		return self;
	}
	else if (api != nullptr && reload)
	{
		auto self = (Renoir*)api;
		_renoir_load_api(self);
		return api;
	}
	else if (api != nullptr && reload == false)
	{
		mn::free((Renoir*)api);
		return nullptr;
	}
	return nullptr;
}

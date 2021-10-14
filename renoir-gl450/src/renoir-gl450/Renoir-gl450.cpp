#include "renoir-gl450/Renoir-gl450.h"
#include "renoir-gl450/Context.h"
#include "renoir-gl450/Handle.h"

#include <mn/Memory.h>
#include <mn/Thread.h>
#include <mn/Pool.h>
#include <mn/Defer.h>
#include <mn/IO.h>
#include <mn/OS.h>
#include <mn/Log.h>
#include <mn/Map.h>
#include <mn/Debug.h>
#include <mn/Assert.h>

#include <GL/glew.h>

#include <math.h>
#include <stdio.h>

inline static bool
_renoir_gl450_check()
{
	GLenum err = glGetError();
	switch (err)
	{
	case GL_INVALID_ENUM:
		mn_assert_msg(false, "invalid enum value was passed");
		return false;

	case GL_INVALID_VALUE:
		mn_assert_msg(false, "invalid value was passed");
		return false;

	case GL_INVALID_OPERATION:
		mn_assert_msg(false, "invalid operation at the current state of opengl");
		return false;

	case GL_INVALID_FRAMEBUFFER_OPERATION:
		mn_assert_msg(false, "invalid framebuffer operation");
		return false;

	case GL_OUT_OF_MEMORY:
		mn_assert_msg(false, "out of memory");
		return false;

	case GL_STACK_UNDERFLOW:
		mn_assert_msg(false, "stack underflow");
		return false;

	case GL_STACK_OVERFLOW:
		mn_assert_msg(false, "stack overflow");
		return false;

	case GL_NO_ERROR:
	default:
		return true;
	}
}

inline static const char*
_renoir_gl450_error_source_string(GLenum v)
{
	switch(v)
	{
	case GL_DEBUG_SOURCE_API: return "api";
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "window system";
	case GL_DEBUG_SOURCE_SHADER_COMPILER: return "shader compiler";
	case GL_DEBUG_SOURCE_THIRD_PARTY: return "third party";
	case GL_DEBUG_SOURCE_APPLICATION: return "application";
	case GL_DEBUG_SOURCE_OTHER: return "other";
	default: return "<unknown>";
	}
}

inline static const char*
_renoir_gl450_error_type_string(GLenum v)
{
	switch(v)
	{
	case GL_DEBUG_TYPE_ERROR: return "error";
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "deprecated behavior";
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "undefined behavior";
	case GL_DEBUG_TYPE_PORTABILITY: return "not portable";
	case GL_DEBUG_TYPE_PERFORMANCE: return "other";
	case GL_DEBUG_TYPE_MARKER: return "marker";
	case GL_DEBUG_TYPE_PUSH_GROUP: return "push group";
	case GL_DEBUG_TYPE_POP_GROUP: return "pop group";
	case GL_DEBUG_TYPE_OTHER: return "other";
	default: return "<unknown>";
	}
}

inline static const char*
_renoir_gl450_error_severtiy_string(GLenum v)
{
	switch(v)
	{
	case GL_DEBUG_SEVERITY_HIGH: return "high";
	case GL_DEBUG_SEVERITY_MEDIUM: return "medium";
	case GL_DEBUG_SEVERITY_LOW: return "low";
	case GL_DEBUG_SEVERITY_NOTIFICATION: return "notification";
	default: return "<unknown>";
	}
}

void GLAPIENTRY
_renoir_gl450_error_log(GLenum source,
				GLenum type,
				GLuint id,
				GLenum severity,
				GLsizei length,
				const GLchar* message,
				const void* userParam)
{
	mn::log_debug(
		"OpenGL450: source = '{}', type = '{}', severity = '{}', message = '{}'",
		_renoir_gl450_error_source_string(source),
		_renoir_gl450_error_type_string(type),
		_renoir_gl450_error_severtiy_string(severity),
		message
	);
}

inline static GLenum
_renoir_face_to_gl(RENOIR_FACE f)
{
	GLenum res = 0;
	switch (f)
	{
	case RENOIR_FACE_BACK:
		res = GL_BACK;
		break;
	case RENOIR_FACE_FRONT:
		res = GL_FRONT;
		break;
	case RENOIR_FACE_FRONT_BACK:
		res = GL_FRONT_AND_BACK;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_orientation_to_gl(RENOIR_ORIENTATION ori)
{
	GLenum res = 0;
	switch (ori)
	{
	case RENOIR_ORIENTATION_CCW:
		res = GL_CCW;
		break;
	case RENOIR_ORIENTATION_CW:
		res = GL_CW;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_blend_to_gl(RENOIR_BLEND p)
{
	GLenum res = 0;
	switch (p)
	{
	case RENOIR_BLEND_ZERO:
		res = GL_ZERO;
		break;
	case RENOIR_BLEND_ONE:
		res = GL_ONE;
		break;
	case RENOIR_BLEND_SRC_COLOR:
		res = GL_SRC_COLOR;
		break;
	case RENOIR_BLEND_ONE_MINUS_SRC_COLOR:
		res = GL_ONE_MINUS_SRC_COLOR;
		break;
	case RENOIR_BLEND_DST_COLOR:
		res = GL_DST_COLOR;
		break;
	case RENOIR_BLEND_ONE_MINUS_DST_COLOR:
		res = GL_ONE_MINUS_DST_COLOR;
		break;
	case RENOIR_BLEND_SRC_ALPHA:
		res = GL_SRC_ALPHA;
		break;
	case RENOIR_BLEND_ONE_MINUS_SRC_ALPHA:
		res = GL_ONE_MINUS_SRC_ALPHA;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_blend_eq_to_gl(RENOIR_BLEND_EQ eq)
{
	GLenum res = 0;
	switch (eq)
	{
	case RENOIR_BLEND_EQ_ADD:
		res = GL_FUNC_ADD;
		break;
	case RENOIR_BLEND_EQ_SUBTRACT:
		res = GL_FUNC_SUBTRACT;
		break;
	case RENOIR_BLEND_EQ_MIN:
		res = GL_MIN;
		break;
	case RENOIR_BLEND_EQ_MAX:
		res = GL_MAX;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_buffer_type_to_gl(RENOIR_BUFFER type)
{
	GLenum res = 0;
	switch (type)
	{
	case RENOIR_BUFFER_VERTEX:
		res = GL_ARRAY_BUFFER;
		break;
	case RENOIR_BUFFER_INDEX:
		res = GL_ELEMENT_ARRAY_BUFFER;
		break;
	case RENOIR_BUFFER_UNIFORM:
		res = GL_UNIFORM_BUFFER;
		break;
	case RENOIR_BUFFER_COMPUTE:
		res = GL_SHADER_STORAGE_BUFFER;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_usage_to_gl(RENOIR_USAGE usage)
{
	GLenum res = 0;
	switch (usage)
	{
	case RENOIR_USAGE_STATIC:
		res = GL_STATIC_DRAW;
		break;
	case RENOIR_USAGE_DYNAMIC:
		res = GL_DYNAMIC_DRAW;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_pixelformat_to_internal_gl(RENOIR_PIXELFORMAT format)
{
	GLenum res = 0;
	switch (format)
	{
	case RENOIR_PIXELFORMAT_RGBA8:
		res = GL_RGBA8;
		break;
	case RENOIR_PIXELFORMAT_R16I:
		res = GL_R16I;
		break;
	case RENOIR_PIXELFORMAT_R16UI:
		res = GL_R16UI;
		break;
	case RENOIR_PIXELFORMAT_R16F:
		res = GL_R16F;
		break;
	case RENOIR_PIXELFORMAT_R32F:
		res = GL_R32F;
		break;
	case RENOIR_PIXELFORMAT_R16G16B16A16F:
		res = GL_RGBA16F;
		break;
	case RENOIR_PIXELFORMAT_R32G32F:
		res = GL_RG32F;
		break;
	case RENOIR_PIXELFORMAT_R32G32B32A32F:
		res = GL_RGBA32F;
		break;
	case RENOIR_PIXELFORMAT_D24S8:
		res = GL_DEPTH24_STENCIL8;
		break;
	case RENOIR_PIXELFORMAT_D32:
		res = GL_DEPTH_COMPONENT32F;
		break;
	case RENOIR_PIXELFORMAT_R8:
		res = GL_R8;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLint
_renoir_pixelformat_to_gl(RENOIR_PIXELFORMAT format)
{
	GLint res = 0;
	switch (format)
	{
	case RENOIR_PIXELFORMAT_RGBA8:
	case RENOIR_PIXELFORMAT_R16G16B16A16F:
		res = GL_RGBA;
		break;
	case RENOIR_PIXELFORMAT_R16I:
	case RENOIR_PIXELFORMAT_R16UI:
		res = GL_RED_INTEGER;
		break;
	case RENOIR_PIXELFORMAT_R16F:
	case RENOIR_PIXELFORMAT_R32F:
	case RENOIR_PIXELFORMAT_R8:
		res = GL_RED;
		break;
	case RENOIR_PIXELFORMAT_R32G32F:
		res = GL_RG;
		break;
	case RENOIR_PIXELFORMAT_R32G32B32A32F:
		res = GL_RGBA;
		break;
	case RENOIR_PIXELFORMAT_D32:
		res = GL_DEPTH_COMPONENT;
		break;
	case RENOIR_PIXELFORMAT_D24S8:
		res = GL_DEPTH_STENCIL;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_pixelformat_to_type_gl(RENOIR_PIXELFORMAT format)
{
	GLenum res = 0;
	switch (format)
	{
	case RENOIR_PIXELFORMAT_R8:
	case RENOIR_PIXELFORMAT_RGBA8:
		res = GL_UNSIGNED_BYTE;
		break;
	case RENOIR_PIXELFORMAT_R16I:
		res = GL_SHORT;
		break;
	case RENOIR_PIXELFORMAT_R16UI:
		res = GL_UNSIGNED_SHORT;
		break;
	case RENOIR_PIXELFORMAT_R32F:
	case RENOIR_PIXELFORMAT_R32G32F:
	case RENOIR_PIXELFORMAT_R32G32B32A32F:
	case RENOIR_PIXELFORMAT_R16G16B16A16F:
	case RENOIR_PIXELFORMAT_R16F:
		res = GL_FLOAT;
		break;
	case RENOIR_PIXELFORMAT_D32:
		res = GL_FLOAT;
		break;
	case RENOIR_PIXELFORMAT_D24S8:
		res = GL_UNSIGNED_INT_24_8;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_pixelformat_to_gl_compute(RENOIR_PIXELFORMAT format)
{
	GLenum res = 0;
	switch(format)
	{
	case RENOIR_PIXELFORMAT_RGBA8:
		res = GL_RGBA8;
		break;
	case RENOIR_PIXELFORMAT_R16I:
		res = GL_R16I;
		break;
	case RENOIR_PIXELFORMAT_R16UI:
		res = GL_R16UI;
		break;
	case RENOIR_PIXELFORMAT_R16F:
		res = GL_R16F;
		break;
	case RENOIR_PIXELFORMAT_R32F:
		res = GL_R32F;
		break;
	case RENOIR_PIXELFORMAT_R16G16B16A16F:
		res = GL_RGBA16F;
		break;
	case RENOIR_PIXELFORMAT_R32G32F:
		res = GL_RG32F;
		break;
	case RENOIR_PIXELFORMAT_R32G32B32A32F:
		res = GL_RGBA32F;
		break;
	case RENOIR_PIXELFORMAT_D24S8:
	case RENOIR_PIXELFORMAT_D32:
		res = GL_R32F;
		break;
	case RENOIR_PIXELFORMAT_R8:
		res = GL_R8;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_pixelformat_to_depth_attachment(RENOIR_PIXELFORMAT format)
{
	GLenum res = 0;
	switch(format)
	{
	case RENOIR_PIXELFORMAT_D32:
		res = GL_DEPTH_ATTACHMENT;
		break;
	case RENOIR_PIXELFORMAT_D24S8:
		res = GL_DEPTH_STENCIL_ATTACHMENT;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_type_to_gl(RENOIR_TYPE type)
{
	GLenum res = 0;
	switch (type)
	{
	case RENOIR_TYPE_UINT8:
	case RENOIR_TYPE_UINT8_4:
	case RENOIR_TYPE_UINT8_4N:
		res = GL_UNSIGNED_BYTE;
		break;
	case RENOIR_TYPE_UINT16:
		res = GL_UNSIGNED_SHORT;
		break;
	case RENOIR_TYPE_UINT32:
		res = GL_UNSIGNED_INT;
		break;
	case RENOIR_TYPE_INT16:
		res = GL_SHORT;
		break;
	case RENOIR_TYPE_INT32:
		res = GL_INT;
		break;
	case RENOIR_TYPE_FLOAT:
	case RENOIR_TYPE_FLOAT_2:
	case RENOIR_TYPE_FLOAT_3:
	case RENOIR_TYPE_FLOAT_4:
		res = GL_FLOAT;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
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


inline static GLint
_renoir_type_to_gl_element_count(RENOIR_TYPE type)
{
	GLint res = 0;
	switch (type)
	{
	case RENOIR_TYPE_UINT8:
	case RENOIR_TYPE_UINT16:
	case RENOIR_TYPE_UINT32:
	case RENOIR_TYPE_INT16:
	case RENOIR_TYPE_INT32:
	case RENOIR_TYPE_FLOAT:
		res = 1;
		break;
	case RENOIR_TYPE_FLOAT_2:
		res = 2;
		break;
	case RENOIR_TYPE_FLOAT_3:
		res = 3;
		break;
	case RENOIR_TYPE_FLOAT_4:
	case RENOIR_TYPE_UINT8_4:
	case RENOIR_TYPE_UINT8_4N:
		res = 4;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static bool
_renoir_type_normalized(RENOIR_TYPE type)
{
	switch (type)
	{
	case RENOIR_TYPE_UINT8_4N: return true;
	default: return false;
	}
}

inline static bool
_renoir_type_is_int(RENOIR_TYPE type)
{
	switch(type)
	{
	case RENOIR_TYPE_UINT8:
	case RENOIR_TYPE_UINT8_4:
	case RENOIR_TYPE_UINT16:
	case RENOIR_TYPE_UINT32:
	case RENOIR_TYPE_INT16:
	case RENOIR_TYPE_INT32:
		return true;
	default: return false;
	}
}

inline static GLenum
_renoir_min_filter_to_gl(RENOIR_FILTER filter)
{
	GLenum res = 0;
	switch (filter)
	{
	case RENOIR_FILTER_POINT:
		res = GL_NEAREST_MIPMAP_NEAREST;
		break;

	case RENOIR_FILTER_LINEAR:
		res = GL_LINEAR_MIPMAP_LINEAR;
		break;

	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_mag_filter_to_gl(RENOIR_FILTER filter)
{
	GLenum res = 0;
	switch (filter)
	{
	case RENOIR_FILTER_POINT:
		res = GL_NEAREST;
		break;

	case RENOIR_FILTER_LINEAR:
		res = GL_LINEAR;
		break;

	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_compare_to_gl(RENOIR_COMPARE compare)
{
	GLenum res = 0;
	switch (compare)
	{
	case RENOIR_COMPARE_NEVER:
		res = GL_NEVER;
		break;

	case RENOIR_COMPARE_LESS:
		res = GL_LESS;
		break;

	case RENOIR_COMPARE_EQUAL:
		res = GL_EQUAL;
		break;

	case RENOIR_COMPARE_LESS_EQUAL:
		res = GL_LEQUAL;
		break;

	case RENOIR_COMPARE_GREATER:
		res = GL_GREATER;
		break;

	case RENOIR_COMPARE_NOT_EQUAL:
		res = GL_NOTEQUAL;
		break;

	case RENOIR_COMPARE_GREATER_EQUAL:
		res = GL_GEQUAL;
		break;

	case RENOIR_COMPARE_ALWAYS:
		res = GL_ALWAYS;
		break;

	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_texmode_to_gl(RENOIR_TEXMODE mode)
{
	GLenum res = 0;
	switch (mode)
	{
	case RENOIR_TEXMODE_CLAMP:
		res = GL_CLAMP_TO_EDGE;
		break;

	case RENOIR_TEXMODE_WRAP:
		res = GL_REPEAT;
		break;

	case RENOIR_TEXMODE_BORDER:
		res = GL_CLAMP_TO_BORDER;
		break;

	case RENOIR_TEXMODE_MIRROR:
		res = GL_MIRRORED_REPEAT;
		break;

	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_primitive_to_gl(RENOIR_PRIMITIVE p)
{
	GLenum res = 0;
	switch (p)
	{
	case RENOIR_PRIMITIVE_POINTS:
		res = GL_POINTS;
		break;
	case RENOIR_PRIMITIVE_LINES:
		res = GL_LINES;
		break;
	case RENOIR_PRIMITIVE_TRIANGLES:
		res = GL_TRIANGLES;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

inline static GLenum
_renoir_access_to_gl(RENOIR_ACCESS a)
{
	GLenum res = 0;
	switch(a)
	{
	case RENOIR_ACCESS_READ:
		res = GL_READ_ONLY;
		break;
	case RENOIR_ACCESS_WRITE:
		res = GL_WRITE_ONLY;
		break;
	case RENOIR_ACCESS_READ_WRITE:
		res = GL_READ_WRITE;
		break;
	default:
		mn_unreachable();
		break;
	}
	return res;
}

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
_renoir_gl450_pipeline_desc_defaults(Renoir_Pipeline_Desc* desc)
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
			Renoir_Pipeline_Desc pipeline_desc;
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
			int level;
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

struct Renoir_GL450_State
{
	// this is a copy from imgui
	GLint last_viewport[4];
	GLint last_scissor_box[4];
	GLenum last_blend_src_rgb;
	GLenum last_blend_dst_rgb;
	GLenum last_blend_src_alpha;
	GLenum last_blend_dst_alpha;
	GLenum last_blend_equation_rgb;
	GLenum last_blend_equation_alpha;
	GLboolean last_enable_blend;
	GLboolean last_enable_cull_face;
	GLboolean last_enable_depth_test;
	GLboolean last_enable_depth_write_mask;
	GLboolean last_enable_scissor_test;
	GLint last_program;
	GLint last_texture;
	mn::Buf<GLint> last_samplers;
	GLenum last_active_texture;
	GLint last_polygon_mode[2];
	GLint last_array_buffer;
};

inline static Renoir_GL450_State
_renoir_gl450_state_new()
{
	return Renoir_GL450_State{};
}

inline static void
_renoir_gl450_state_free(Renoir_GL450_State& self)
{
	mn::buf_free(self.last_samplers);
}

inline static void
_renoir_gl450_state_capture(Renoir_GL450_State& state)
{
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.last_texture);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.last_array_buffer);
	glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *)&state.last_active_texture);
	for (size_t i = 0; i < state.last_samplers.count; ++i)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glGetIntegerv(GL_SAMPLER_BINDING, &state.last_samplers[i]);
	}
	glGetIntegerv(GL_CURRENT_PROGRAM, &state.last_program);
	glGetIntegerv(GL_VIEWPORT, state.last_viewport);
	glGetIntegerv(GL_SCISSOR_BOX, state.last_scissor_box);
	glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *)&state.last_blend_src_rgb);
	glGetIntegerv(GL_BLEND_DST_RGB, (GLint *)&state.last_blend_dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *)&state.last_blend_src_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *)&state.last_blend_dst_alpha);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *)&state.last_blend_equation_rgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *)&state.last_blend_equation_alpha);
	glGetIntegerv(GL_POLYGON_MODE, state.last_polygon_mode);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &state.last_enable_depth_write_mask);
	state.last_enable_blend		= glIsEnabled(GL_BLEND);
	state.last_enable_cull_face	= glIsEnabled(GL_CULL_FACE);
	state.last_enable_depth_test	= glIsEnabled(GL_DEPTH_TEST);
	state.last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
}

inline static void
_renoir_gl450_state_reset(Renoir_GL450_State& state)
{
	glUseProgram(state.last_program);
	glBindTexture(GL_TEXTURE_2D, state.last_texture);
	glActiveTexture(state.last_active_texture);
	glBindBuffer(GL_ARRAY_BUFFER, state.last_array_buffer);
	glBlendEquationSeparate(state.last_blend_equation_rgb, state.last_blend_equation_alpha);
	for (size_t i = 0; i < state.last_samplers.count; ++i)
		glBindSampler(i, state.last_samplers[i]);
	glPolygonMode(GL_FRONT_AND_BACK, (GLenum)state.last_polygon_mode[0]);
	glBlendFuncSeparate(
		state.last_blend_src_rgb,
		state.last_blend_dst_rgb,
		state.last_blend_src_alpha,
		state.last_blend_dst_alpha);
	if (state.last_enable_blend)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
	if (state.last_enable_cull_face)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
	if (state.last_enable_depth_test)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
	if (state.last_enable_depth_write_mask)
		glDepthMask(GL_TRUE);
	else
		glDepthMask(GL_FALSE);
	if (state.last_enable_scissor_test)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
	glViewport(
		state.last_viewport[0],
		state.last_viewport[1],
		(GLsizei)state.last_viewport[2],
		(GLsizei)state.last_viewport[3]);
	glScissor(
		state.last_scissor_box[0],
		state.last_scissor_box[1],
		(GLsizei)state.last_scissor_box[2],
		(GLsizei)state.last_scissor_box[3]);
}

struct Renoir_Leak_Info
{
	void* callstack[20];
	size_t callstack_size;
};

struct IRenoir
{
	mn::Mutex mtx;
	Renoir_GL450_Context* ctx;
	mn::Pool handle_pool;
	mn::Pool command_pool;
	Renoir_Settings settings;

	mn::Str info_description;

	// global command list
	Renoir_Command *command_list_head;
	Renoir_Command *command_list_tail;

	// command execution context
	Renoir_Handle* current_pipeline;
	Renoir_Handle* current_program;
	Renoir_Handle* current_compute;
	Renoir_Handle* current_pass;

	// caches
	GLuint vao;
	GLuint msaa_resolve_fb;
	mn::Buf<Renoir_Handle*> sampler_cache;

	// opengl state used to prevent state leaks in case of external opengl context
	bool glewInited;
	Renoir_GL450_State state;

	// leak detection
	mn::Map<Renoir_Handle*, Renoir_Leak_Info> alive_handles;
};

static void
_renoir_gl450_command_execute(IRenoir* self, Renoir_Command* command);

static Renoir_Handle*
_renoir_gl450_handle_new(IRenoir* self, RENOIR_HANDLE_KIND kind)
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
_renoir_gl450_handle_free(IRenoir* self, Renoir_Handle* h)
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
_renoir_gl450_handle_ref(Renoir_Handle* h)
{
	h->rc.fetch_add(1);
	return h;
}

static bool
_renoir_gl450_handle_unref(Renoir_Handle* h)
{
	return h->rc.fetch_sub(1) == 1;
}

template<typename T>
static Renoir_Command*
_renoir_gl450_command_new(T* self, RENOIR_COMMAND_KIND kind)
{
	auto command = (Renoir_Command*)mn::pool_get(self->command_pool);
	memset(command, 0, sizeof(*command));
	command->kind = kind;
	return command;
}

template<typename T>
static void
_renoir_gl450_command_free(T* self, Renoir_Command* command)
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
	case RENOIR_COMMAND_KIND_PASS_SWAPCHAIN_NEW:
	case RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW:
	case RENOIR_COMMAND_KIND_PASS_FREE:
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	case RENOIR_COMMAND_KIND_SAMPLER_NEW:
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
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
_renoir_gl450_command_push_back(T* self, Renoir_Command* command)
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
_renoir_gl450_command_push_front(T* self, Renoir_Command* command)
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
_renoir_gl450_command_process(IRenoir* self, Renoir_Command* command)
{
	if (self->settings.defer_api_calls)
	{
		_renoir_gl450_command_push_back(self, command);
	}
	else
	{
		_renoir_gl450_command_execute(self, command);
		_renoir_gl450_command_free(self, command);
	}
}

static void
_renoir_gl450_command_execute(IRenoir* self, Renoir_Command* command)
{
	switch(command->kind)
	{
	case RENOIR_COMMAND_KIND_INIT:
	{
		renoir_gl450_context_bind(self->ctx);
		auto renderer = glGetString(GL_RENDERER);
		auto version = glGetString(GL_VERSION);
		auto shader = glGetString(GL_SHADING_LANGUAGE_VERSION);
		if (self->settings.external_context)
		{
			// we just need to init glew in case of external context
			GLenum glew_result = glewInit();
			mn_assert_msg(glew_result == GLEW_OK, "glewInit failed");

			mn::log_info("OpenGL Renderer: {}", renderer);
			mn::log_info("OpenGL Version: {}", version);
			mn::log_info("GLSL Version: {}", shader);

			GLint major = 0, minor = 0;
			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);

			mn::log_ensure(major >= 4 && minor >= 5, "incompatibile OpenGL Context");
			_renoir_gl450_state_capture(self->state);
		}
		self->glewInited = true;
		// During init, enable debug output
		#if RENOIR_DEBUG_LAYER
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(_renoir_gl450_error_log, nullptr);
		#endif

		glCreateVertexArrays(1, &self->vao);
		glCreateFramebuffers(1, &self->msaa_resolve_fb);
		GLint max_samplers = 0;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_samplers);
		mn::buf_resize(self->state.last_samplers, max_samplers);
		mn_assert(_renoir_gl450_check());

		self->info_description = mn::strf("Renderer: {}, Version: {}, GLSL Version: {}", renderer, version, shader);
		break;
	}
	case RENOIR_COMMAND_KIND_SWAPCHAIN_NEW:
	{
		renoir_gl450_context_window_init(self->ctx, command->swapchain_new.handle, &self->settings);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_SWAPCHAIN_FREE:
	{
		auto h = command->swapchain_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		renoir_gl450_context_window_free(self->ctx, h);
		mn_assert(_renoir_gl450_check());
		_renoir_gl450_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_SWAPCHAIN_NEW:
	{
		// do nothing
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW:
	{
		auto h = command->pass_offscreen_new.handle;
		auto& desc = command->pass_offscreen_new.desc;

		int msaa = -1;

		glCreateFramebuffers(1, &h->raster_pass.fb);
		// we want to initialize this to GL_NONE which is 0 so this initialization is safe
		GLenum attachments[RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE] = {};
		for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
		{
			auto color = (Renoir_Handle*)desc.color[i].texture.handle;
			if (color == nullptr)
				continue;
			mn_assert(color->texture.desc.render_target);
			attachments[i] = GL_COLOR_ATTACHMENT0 + i;

			_renoir_gl450_handle_ref(color);
			if (color->texture.desc.cube_map == false)
			{
				if (color->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert_msg(desc.color[i].level == 0, "multisampled textures does not support mipmaps");
					glNamedFramebufferRenderbuffer(h->raster_pass.fb, GL_COLOR_ATTACHMENT0+i,  GL_RENDERBUFFER, color->texture.render_buffer[0]);
				}
				else
				{
					mn_assert_msg(desc.color[i].level < color->texture.desc.mipmaps, "out of range mip level");
					glNamedFramebufferTexture(h->raster_pass.fb, GL_COLOR_ATTACHMENT0+i, color->texture.id, desc.color[i].level);
				}
			}
			else
			{
				if (color->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert_msg(desc.color[i].level == 0, "multisampled textures does not support mipmaps");
					glNamedFramebufferRenderbuffer(h->raster_pass.fb, GL_COLOR_ATTACHMENT0+i,  GL_RENDERBUFFER, color->texture.render_buffer[desc.color[i].subresource]);
				}
				else
				{
					mn_assert_msg(desc.color[i].level < color->texture.desc.mipmaps, "out of range mip level");
					mn_assert(_renoir_gl450_check());
					glBindFramebuffer(GL_FRAMEBUFFER, h->raster_pass.fb);
					glFramebufferTexture2D(
						GL_FRAMEBUFFER,
						GL_COLOR_ATTACHMENT0 + i,
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc.color[i].subresource,
						color->texture.id,
						desc.color[i].level
					);
					mn_assert(_renoir_gl450_check());
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
		glNamedFramebufferDrawBuffers(h->raster_pass.fb, RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE, attachments);
		mn_assert(_renoir_gl450_check());

		auto depth = (Renoir_Handle*)desc.depth_stencil.texture.handle;
		if (depth)
		{
			mn_assert(depth->texture.desc.render_target);
			_renoir_gl450_handle_ref(depth);

			auto attachment = _renoir_pixelformat_to_depth_attachment(depth->texture.desc.pixel_format);

			if (depth->texture.desc.cube_map == false)
			{
				if (depth->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert_msg(desc.depth_stencil.level == 0, "multisampled textures does not support mipmaps");
					glNamedFramebufferRenderbuffer(h->raster_pass.fb, attachment,  GL_RENDERBUFFER, depth->texture.render_buffer[0]);
				}
				else
				{
					mn_assert_msg(desc.depth_stencil.level < depth->texture.desc.mipmaps, "out of range mip level");
					glNamedFramebufferTexture(h->raster_pass.fb, attachment, depth->texture.id, desc.depth_stencil.level);
				}
			}
			else
			{
				if (depth->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					mn_assert_msg(desc.depth_stencil.level == 0, "multisampled textures does not support mipmaps");
					glNamedFramebufferRenderbuffer(h->raster_pass.fb, attachment,  GL_RENDERBUFFER, depth->texture.render_buffer[desc.depth_stencil.subresource]);
				}
				else
				{
					mn_assert_msg(desc.depth_stencil.level < depth->texture.desc.mipmaps, "out of range mip level");
					glBindFramebuffer(GL_FRAMEBUFFER, h->raster_pass.fb);
					glFramebufferTexture2D(
						GL_FRAMEBUFFER,
						attachment,
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc.depth_stencil.subresource,
						depth->texture.id,
						desc.depth_stencil.level
					);
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
		mn_assert(_renoir_gl450_check());
		mn_assert(glCheckNamedFramebufferStatus(h->raster_pass.fb, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_COMPUTE_NEW:
	{
		// do nothing
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_FREE:
	{
		auto h = command->pass_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			for(auto it = h->raster_pass.command_list_head; it != NULL; it = it->next)
				_renoir_gl450_command_free(self, command);

			// free all the bound textures if it's a framebuffer pass
			if (h->raster_pass.fb != 0)
			{
				for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
				{
					auto color = (Renoir_Handle*)h->raster_pass.offscreen.color[i].texture.handle;
					if (color == nullptr)
						continue;

					// issue command to free the color texture
					auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = color;
					_renoir_gl450_command_process(self, command);
				}

				auto depth = (Renoir_Handle*)h->raster_pass.offscreen.depth_stencil.texture.handle;
				if (depth)
				{
					// issue command to free the depth texture
					auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = depth;
					_renoir_gl450_command_process(self, command);
				}

				glDeleteFramebuffers(1, &h->raster_pass.fb);
			}
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			for(auto it = h->compute_pass.command_list_head; it != NULL; it = it->next)
				_renoir_gl450_command_free(self, command);
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}

		_renoir_gl450_handle_free(self, h);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_NEW:
	{
		auto h = command->buffer_new.handle;
		auto& desc = command->buffer_new.desc;

		if (desc.type == RENOIR_BUFFER_COMPUTE)
		{
			mn_assert_msg(
				desc.compute_buffer_stride > 0 && desc.compute_buffer_stride % 4 == 0,
				"compute buffer stride should be greater than 0, no greater than 2048, and a multiple of 4"
			);
		}

		auto gl_usage = _renoir_usage_to_gl(desc.usage);

		renoir_gl450_context_bind(self->ctx);
		glCreateBuffers(1, &h->buffer.id);
		glNamedBufferData(h->buffer.id, desc.data_size, desc.data, gl_usage);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	{
		auto h = command->buffer_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		glDeleteBuffers(1, &h->buffer.id);
		_renoir_gl450_handle_free(self, h);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_NEW:
	{
		auto h = command->texture_new.handle;
		auto& desc = command->texture_new.desc;

		auto gl_internal_format = _renoir_pixelformat_to_internal_gl(desc.pixel_format);
		auto gl_format = _renoir_pixelformat_to_gl(desc.pixel_format);
		auto gl_type = _renoir_pixelformat_to_type_gl(desc.pixel_format);

		// change alignment to match pixel data
		GLint original_pack_alignment = 0;
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &original_pack_alignment);
		if (desc.pixel_format == RENOIR_PIXELFORMAT_R8)
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		mn_defer({
			if (desc.pixel_format == RENOIR_PIXELFORMAT_R8)
				glPixelStorei(GL_UNPACK_ALIGNMENT, original_pack_alignment);
		});

		if (desc.size.height == 0 && desc.size.depth == 0)
		{
			glCreateTextures(GL_TEXTURE_1D, 1, &h->texture.id);
			// 1D texture
			glTextureStorage1D(h->texture.id, h->texture.desc.mipmaps, gl_internal_format, desc.size.width);
			if (desc.data[0] != nullptr)
			{
				glTextureSubImage1D(
					h->texture.id,
					0,
					0,
					desc.size.width,
					gl_format,
					gl_type,
					desc.data[0]
				);
				if (h->texture.desc.mipmaps > 1)
					glGenerateTextureMipmap(h->texture.id);
			}
		}
		else if (desc.size.height > 0 && desc.size.depth == 0)
		{
			if (desc.cube_map == false)
			{
				glCreateTextures(GL_TEXTURE_2D, 1, &h->texture.id);
				// 2D texture
				glTextureStorage2D(h->texture.id, h->texture.desc.mipmaps, gl_internal_format, desc.size.width, desc.size.height);
				if (desc.data[0] != nullptr)
				{
					glTextureSubImage2D(
						h->texture.id,
						0,
						0,
						0,
						desc.size.width,
						desc.size.height,
						gl_format,
						gl_type,
						desc.data[0]
					);
					if (h->texture.desc.mipmaps > 1)
						glGenerateTextureMipmap(h->texture.id);
				}

				// create renderbuffer to handle msaa
				if (desc.render_target && desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					glCreateRenderbuffers(1, &h->texture.render_buffer[0]);
					glNamedRenderbufferStorageMultisample(
						h->texture.render_buffer[0],
						(GLsizei)desc.msaa,
						gl_internal_format,
						desc.size.width,
						desc.size.height
					);
				}
			}
			else
			{
				glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &h->texture.id);
				glTextureStorage2D(h->texture.id, h->texture.desc.mipmaps, gl_internal_format, desc.size.width, desc.size.height);
				for (size_t i = 0; i < 6; ++i)
				{
					if (desc.data[i] == nullptr)
						continue;
					glTextureSubImage3D(
						h->texture.id,
						0,
						0,
						0,
						i,
						desc.size.width,
						desc.size.height,
						1,
						gl_format,
						gl_type,
						desc.data[i]
					);
				}

				// create renderbuffer to handle msaa
				if (desc.render_target && desc.msaa != RENOIR_MSAA_MODE_NONE)
				{
					for (int i = 0; i < 6; ++i)
					{
						glCreateRenderbuffers(1, &h->texture.render_buffer[i]);
						glNamedRenderbufferStorageMultisample(
							h->texture.render_buffer[i],
							(GLsizei)desc.msaa,
							gl_internal_format,
							desc.size.width,
							desc.size.height
						);
					}
				}

				if (h->texture.desc.mipmaps > 1)
					glGenerateTextureMipmap(h->texture.id);
			}
		}
		else if (desc.size.height > 0 && desc.size.depth > 0)
		{
			glCreateTextures(GL_TEXTURE_3D, 1, &h->texture.id);
			// 3D texture
			glTextureStorage3D(h->texture.id, h->texture.desc.mipmaps, gl_internal_format, desc.size.width, desc.size.height, desc.size.depth);
			if (desc.data[0] != nullptr)
			{
				glTextureSubImage3D(
					h->texture.id,
					0,
					0,
					0,
					0,
					desc.size.width,
					desc.size.height,
					desc.size.depth,
					gl_format,
					gl_type,
					desc.data[0]
				);
				if (h->texture.desc.mipmaps > 1)
					glGenerateTextureMipmap(h->texture.id);
			}
		}
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	{
		auto h = command->texture_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		glDeleteTextures(1, &h->texture.id);
		for (int i = 0; i < 6; ++i)
		{
			if (h->texture.render_buffer[i] == 0)
				continue;

			glDeleteRenderbuffers(1, &h->texture.render_buffer[i]);
		}
		_renoir_gl450_handle_free(self, h);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_SAMPLER_NEW:
	{
		auto h = command->sampler_new.handle;
		auto& desc = command->sampler_new.desc;

		auto gl_min_filter = _renoir_min_filter_to_gl(desc.filter);
		auto gl_mag_filter = _renoir_mag_filter_to_gl(desc.filter);
		auto gl_u_texmode = _renoir_texmode_to_gl(desc.u);
		auto gl_v_texmode = _renoir_texmode_to_gl(desc.v);
		auto gl_w_texmode = _renoir_texmode_to_gl(desc.w);
		auto gl_compare = _renoir_compare_to_gl(desc.compare);

		glGenSamplers(1, &h->sampler.id);
		glSamplerParameteri(h->sampler.id, GL_TEXTURE_MIN_FILTER, gl_min_filter);
		glSamplerParameteri(h->sampler.id, GL_TEXTURE_MAG_FILTER, gl_mag_filter);

		glSamplerParameteri(h->sampler.id, GL_TEXTURE_WRAP_S, gl_u_texmode);
		glSamplerParameteri(h->sampler.id, GL_TEXTURE_WRAP_T, gl_v_texmode);
		glSamplerParameteri(h->sampler.id, GL_TEXTURE_WRAP_R, gl_w_texmode);

		if (desc.compare == RENOIR_COMPARE_NEVER)
			glSamplerParameteri(h->sampler.id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		else
			glSamplerParameteri(h->sampler.id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

		glSamplerParameteri(h->sampler.id, GL_TEXTURE_COMPARE_FUNC, gl_compare);
		glSamplerParameterfv(h->sampler.id, GL_TEXTURE_BORDER_COLOR, &desc.border.r);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	{
		auto h = command->sampler_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		glDeleteSamplers(1, &h->sampler.id);
		_renoir_gl450_handle_free(self, h);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_NEW:
	{
		auto& desc = command->program_new.desc;
		auto h = command->program_new.handle;
		constexpr size_t error_length = 1024;
		char error[error_length];
		GLint size = 0;
		GLint success = 0;

		auto vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		size = desc.vertex.size;
		glShaderSource(vertex_shader, 1, &desc.vertex.bytes, &size);
		glCompileShader(vertex_shader);
		glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
		if (success == GL_FALSE)
		{
			::memset(error, 0, sizeof(error));
			glGetShaderInfoLog(vertex_shader, error_length, &size, error);
			mn::log_error("vertex shader compile error\n{}", error);
			break;
		}

		auto pixel_shader = glCreateShader(GL_FRAGMENT_SHADER);
		size = desc.pixel.size;
		glShaderSource(pixel_shader, 1, &desc.pixel.bytes, &size);
		glCompileShader(pixel_shader);
		glGetShaderiv(pixel_shader, GL_COMPILE_STATUS, &success);
		if (success == GL_FALSE)
		{
			::memset(error, 0, sizeof(error));
			glGetShaderInfoLog(pixel_shader, error_length, &size, error);
			mn::log_error("pixel shader compile error\n{}", error);
			break;
		}

		GLuint geometry_shader = 0;
		if (desc.geometry.bytes != nullptr)
		{
			geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
			size = desc.geometry.size;
			glShaderSource(geometry_shader, 1, &desc.geometry.bytes, &size);
			glCompileShader(geometry_shader);
			glGetShaderiv(geometry_shader, GL_COMPILE_STATUS, &success);
			if (success == GL_FALSE)
			{
				::memset(error, 0, sizeof(error));
				glGetShaderInfoLog(geometry_shader, error_length, &size, error);
				mn::log_error("pixel shader compile error\n{}", error);
				break;
			}
		}

		h->program.id = glCreateProgram();
		glAttachShader(h->program.id, vertex_shader);
		glAttachShader(h->program.id, pixel_shader);
		if(desc.geometry.bytes != nullptr)
			glAttachShader(h->program.id, geometry_shader);

		glLinkProgram(h->program.id);
		glGetProgramiv(h->program.id, GL_LINK_STATUS, &success);
		if (success == GL_FALSE)
		{
			glGetProgramiv(h->program.id, GL_INFO_LOG_LENGTH, &size);
			size = size > error_length ? error_length : size;
			glGetProgramInfoLog(h->program.id, size, &size, error);
			mn::panic("program linking error\n{}", error);
		}

		glDetachShader(h->program.id, vertex_shader);
		glDeleteShader(vertex_shader);
		glDetachShader(h->program.id, pixel_shader);
		glDeleteShader(pixel_shader);
		if (desc.geometry.bytes != nullptr)
		{
			glDetachShader(h->program.id, geometry_shader);
			glDeleteShader(geometry_shader);
		}

		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	{
		auto h = command->program_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		glDeleteProgram(h->program.id);
		_renoir_gl450_handle_free(self, h);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_NEW:
	{
		auto& desc = command->compute_new.desc;
		auto h = command->compute_new.handle;
		constexpr size_t error_length = 1024;
		char error[error_length];
		GLint size = 0;
		GLint success = 0;

		auto compute_shader = glCreateShader(GL_COMPUTE_SHADER);
		size = desc.compute.size;
		glShaderSource(compute_shader, 1, &desc.compute.bytes, &size);
		glCompileShader(compute_shader);
		glGetShaderiv(compute_shader, GL_COMPILE_STATUS, &success);
		if (success == GL_FALSE)
		{
			glGetShaderiv(compute_shader, GL_INFO_LOG_LENGTH, &size);
			size = size > error_length ? error_length : size;
			glGetShaderInfoLog(compute_shader, size, &size, error);
			mn::panic("compute shader compile error\n{}", error);
		}

		h->compute.id = glCreateProgram();
		glAttachShader(h->compute.id, compute_shader);

		glLinkProgram(h->compute.id);
		glGetProgramiv(h->compute.id, GL_LINK_STATUS, &success);
		if (success == GL_FALSE)
		{
			glGetProgramiv(h->compute.id, GL_INFO_LOG_LENGTH, &size);
			size = size > error_length ? error_length : size;
			glGetProgramInfoLog(h->compute.id, size, &size, error);
			mn::panic("compute program linking error\n{}", error);
		}

		glDetachShader(h->compute.id, compute_shader);
		glDeleteShader(compute_shader);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
	{
		auto h = command->compute_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		glDeleteProgram(h->compute.id);
		_renoir_gl450_handle_free(self, h);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_NEW:
	{
		auto h = command->timer_new.handle;
		glGenQueries(2, h->timer.timepoints);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_FREE:
	{
		auto h = command->timer_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		glDeleteQueries(2, h->timer.timepoints);
		_renoir_gl450_handle_free(self, h);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_ELAPSED:
	{
		auto h = command->timer_elapsed.handle;
		mn_assert(h->timer.state == RENOIR_TIMER_STATE_READ_SCHEDULED);
		GLint result_available = 0;
		glGetQueryObjectiv(h->timer.timepoints[1], GL_QUERY_RESULT_AVAILABLE, &result_available);
		if (result_available)
		{
			GLuint64 timepoint[2];
			glGetQueryObjectui64v(h->timer.timepoints[0], GL_QUERY_RESULT, &timepoint[0]);
			glGetQueryObjectui64v(h->timer.timepoints[1], GL_QUERY_RESULT, &timepoint[1]);
			h->timer.elapsed_time_in_nanos = timepoint[1] - timepoint[0];
			h->timer.state = RENOIR_TIMER_STATE_READY;
		}
		else
		{
			h->timer.state = RENOIR_TIMER_STATE_END;
		}
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_BEGIN:
	{
		auto h = command->pass_begin.handle;
		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			// if this is an on screen/window
			if (auto swapchain = h->raster_pass.swapchain)
			{
				renoir_gl450_context_window_bind(self->ctx, swapchain);
				glBindFramebuffer(GL_FRAMEBUFFER, NULL);
				glViewport(0, 0, swapchain->swapchain.width, swapchain->swapchain.height);
				glDisable(GL_SCISSOR_TEST);
				self->current_pass = h;
			}
			// this is an offscreen
			else if (h->raster_pass.fb != 0)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, h->raster_pass.fb);
				glViewport(0, 0, h->raster_pass.width, h->raster_pass.height);
				glDisable(GL_SCISSOR_TEST);
				self->current_pass = h;
			}
			else
			{
				mn_unreachable();
			}
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			// do nothing
			self->current_pass = h;
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_END:
	{
		auto h = command->pass_end.handle;

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			// Note(Moustapha): this is because of opengl weird specs, scissor box will affect the blit
			auto scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
			glDisable(GL_SCISSOR_TEST);

			// if this is an offscreen view with msaa we'll need to issue a read command to move the data
			// from renderbuffer to the texture
			for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
			{
				auto color = (Renoir_Handle*)h->raster_pass.offscreen.color[i].texture.handle;
				if (color == nullptr)
					continue;

				// only resolve msaa textures
				if (color->texture.desc.msaa == RENOIR_MSAA_MODE_NONE)
					continue;

				if (color->texture.desc.cube_map == false)
				{
					glNamedFramebufferTexture(self->msaa_resolve_fb, GL_COLOR_ATTACHMENT0, color->texture.id, 0);
					glNamedFramebufferDrawBuffer(self->msaa_resolve_fb, GL_COLOR_ATTACHMENT0);
				}
				else
				{
					glBindFramebuffer(GL_FRAMEBUFFER, self->msaa_resolve_fb);
					glFramebufferTexture2D(
						GL_FRAMEBUFFER,
						GL_COLOR_ATTACHMENT0,
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + h->raster_pass.offscreen.color[i].subresource,
						color->texture.id,
						0
					);
					glNamedFramebufferDrawBuffer(self->msaa_resolve_fb, GL_COLOR_ATTACHMENT0);
				}

				glNamedFramebufferReadBuffer(h->raster_pass.fb, GL_COLOR_ATTACHMENT0 + i);
				glBlitNamedFramebuffer(
					h->raster_pass.fb,
					self->msaa_resolve_fb,
					0, 0, h->raster_pass.width, h->raster_pass.height,
					0, 0, h->raster_pass.width, h->raster_pass.height,
					GL_COLOR_BUFFER_BIT,
					GL_LINEAR
				);
				// clear color attachment
				glNamedFramebufferTexture(self->msaa_resolve_fb, GL_COLOR_ATTACHMENT0 + i, 0, 0);
			}
			mn_assert(_renoir_gl450_check());

			// resolve depth textures as well
			auto depth = (Renoir_Handle*)h->raster_pass.offscreen.depth_stencil.texture.handle;
			if (depth && depth->texture.desc.msaa != RENOIR_MSAA_MODE_NONE)
			{
				auto attachment = _renoir_pixelformat_to_depth_attachment(depth->texture.desc.pixel_format);

				if (depth->texture.desc.cube_map == false)
				{
					glNamedFramebufferTexture(self->msaa_resolve_fb, attachment, depth->texture.id, 0);
				}
				else
				{
					glBindFramebuffer(GL_FRAMEBUFFER, self->msaa_resolve_fb);
					glFramebufferTexture2D(
						GL_FRAMEBUFFER,
						attachment,
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + h->raster_pass.offscreen.depth_stencil.subresource,
						depth->texture.id,
						0
					);
				}
				glBlitNamedFramebuffer(
					h->raster_pass.fb,
					self->msaa_resolve_fb,
					0, 0, h->raster_pass.width, h->raster_pass.height,
					0, 0, h->raster_pass.width, h->raster_pass.height,
					GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
					GL_NEAREST
				);
				// clear depth attachment
				glNamedFramebufferTexture(self->msaa_resolve_fb, attachment, 0, 0);
			}

			if (scissor_enabled)
				glEnable(GL_SCISSOR_TEST);
			else
				glDisable(GL_SCISSOR_TEST);
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			// do nothing
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
		self->current_pass = nullptr;
		self->current_pipeline->pipeline.desc = Renoir_Pipeline_Desc{};
		_renoir_gl450_pipeline_desc_defaults(&self->current_pipeline->pipeline.desc);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_CLEAR:
	{
		auto& desc = command->pass_clear.desc;

		GLbitfield clear_bits = 0;
		if (desc.flags & RENOIR_CLEAR_COLOR)
		{
			if (desc.independent_clear_color == RENOIR_SWITCH_DISABLE)
			{
				glClearColor(
					desc.color[0].r,
					desc.color[0].g,
					desc.color[0].b,
					desc.color[0].a
				);
				clear_bits |= GL_COLOR_BUFFER_BIT;
			}
			else
			{
				auto pass = self->current_pass;
				for (int i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
				{
					// swapchain passes can only have one color target
					if (pass->raster_pass.swapchain)
					{
						if (i > 0)
							break;
					}
					// offscreen passes can have multiple color targets, but we have to make sure that this target exists
					else
					{
						if (pass->raster_pass.offscreen.color[i].texture.handle == nullptr)
							continue;
					}

					glClearBufferfv(GL_COLOR, i, &desc.color[i].r);
				}
			}
		}

		if (command->pass_clear.desc.flags & RENOIR_CLEAR_DEPTH)
		{
			glDepthMask(GL_TRUE);
			glClearDepth(command->pass_clear.desc.depth);
			glClearStencil(command->pass_clear.desc.stencil);
			clear_bits |= GL_DEPTH_BUFFER_BIT;
			clear_bits |= GL_STENCIL_BUFFER_BIT;
		}
		glClear(clear_bits);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_USE_PIPELINE:
	{
		self->current_pipeline->pipeline.desc = command->use_pipeline.pipeline_desc;
		auto h = self->current_pipeline;

		if (h->pipeline.desc.rasterizer.cull == RENOIR_SWITCH_ENABLE)
		{
			auto gl_face = _renoir_face_to_gl(h->pipeline.desc.rasterizer.cull_face);
			auto gl_orientation = _renoir_orientation_to_gl(h->pipeline.desc.rasterizer.cull_front);
			glEnable(GL_CULL_FACE);
			glCullFace(gl_face);
			glFrontFace(gl_orientation);
		}
		else
		{
			glDisable(GL_CULL_FACE);
		}

		switch (h->pipeline.desc.rasterizer.scissor)
		{
		case RENOIR_SWITCH_ENABLE:
			glEnable(GL_SCISSOR_TEST);
			break;
		case RENOIR_SWITCH_DISABLE:
			glDisable(GL_SCISSOR_TEST);
			break;
		default:
			mn_unreachable();
			break;
		}

		if (h->pipeline.desc.depth_stencil.depth == RENOIR_SWITCH_ENABLE)
		{
			glEnable(GL_DEPTH_TEST);
			glDepthRange(0.0, 1.0);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		if (h->pipeline.desc.depth_stencil.depth_write_mask == RENOIR_SWITCH_ENABLE)
		{
			glDepthMask(GL_TRUE);
		}
		else
		{
			glDepthMask(GL_FALSE);
		}

		for (int i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
		{
			if (h->pipeline.desc.blend[i].enabled == RENOIR_SWITCH_ENABLE)
			{
				auto gl_src_rgb = _renoir_blend_to_gl(h->pipeline.desc.blend[i].src_rgb);
				auto gl_dst_rgb = _renoir_blend_to_gl(h->pipeline.desc.blend[i].dst_rgb);
				auto gl_src_alpha = _renoir_blend_to_gl(h->pipeline.desc.blend[i].src_alpha);
				auto gl_dst_alpha = _renoir_blend_to_gl(h->pipeline.desc.blend[i].dst_alpha);
				auto gl_blend_eq_rgb = _renoir_blend_eq_to_gl(h->pipeline.desc.blend[i].eq_rgb);
				auto gl_blend_eq_alpha = _renoir_blend_eq_to_gl(h->pipeline.desc.blend[i].eq_alpha);
				if (h->pipeline.desc.independent_blend == RENOIR_SWITCH_ENABLE)
				{
					glEnablei(GL_BLEND, i);
					glBlendFuncSeparatei(i, gl_src_rgb, gl_dst_rgb, gl_src_alpha, gl_dst_alpha);
					glBlendEquationSeparatei(i, gl_blend_eq_rgb, gl_blend_eq_alpha);
				}
				else
				{
					glEnable(GL_BLEND);
					glBlendFuncSeparate(gl_src_rgb, gl_dst_rgb, gl_src_alpha, gl_dst_alpha);
					glBlendEquationSeparate(gl_blend_eq_rgb, gl_blend_eq_alpha);
				}
			}
			else
			{
				if (h->pipeline.desc.independent_blend == RENOIR_SWITCH_ENABLE)
				{
					glDisablei(GL_BLEND, i);
				}
				else
				{
					glDisable(GL_BLEND);
				}
			}

			if (h->pipeline.desc.independent_blend == RENOIR_SWITCH_ENABLE)
			{
				if (h->pipeline.desc.blend[i].color_mask == RENOIR_COLOR_MASK_NONE)
				{
					glColorMaski(i, false, false, false, false);
				}
				else
				{
					glColorMaski(
						i,
						(h->pipeline.desc.blend[i].color_mask & RENOIR_COLOR_MASK_RED) != 0,
						(h->pipeline.desc.blend[i].color_mask & RENOIR_COLOR_MASK_GREEN) != 0,
						(h->pipeline.desc.blend[i].color_mask & RENOIR_COLOR_MASK_BLUE) != 0,
						(h->pipeline.desc.blend[i].color_mask & RENOIR_COLOR_MASK_ALPHA) != 0
					);
				}
			}
			else
			{
				if (h->pipeline.desc.blend[i].color_mask == RENOIR_COLOR_MASK_NONE)
				{
					glColorMask(false, false, false, false);
				}
				else
				{
					glColorMask(
						(h->pipeline.desc.blend[i].color_mask& RENOIR_COLOR_MASK_RED) != 0,
						(h->pipeline.desc.blend[i].color_mask& RENOIR_COLOR_MASK_GREEN) != 0,
						(h->pipeline.desc.blend[i].color_mask& RENOIR_COLOR_MASK_BLUE) != 0,
						(h->pipeline.desc.blend[i].color_mask& RENOIR_COLOR_MASK_ALPHA) != 0
					);
				}
			}

			if (h->pipeline.desc.independent_blend == RENOIR_SWITCH_DISABLE)
				break;
		}

		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_USE_PROGRAM:
	{
		auto h = command->use_program.program;
		self->current_program = h;
		self->current_compute = nullptr;
		glUseProgram(self->current_program->program.id);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_USE_COMPUTE:
	{
		auto h = command->use_compute.compute;
		self->current_compute = h;
		self->current_program = nullptr;
		glUseProgram(self->current_compute->compute.id);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_SCISSOR:
	{
		glScissor(command->scissor.x, command->scissor.y, command->scissor.w, command->scissor.h);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_CLEAR:
	{
		auto h = command->buffer_clear.handle;
		uint8_t value = 0;
		glClearNamedBufferData(h->buffer.id, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &value);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_WRITE:
	{
		auto h = command->buffer_write.handle;
		glNamedBufferSubData(
			h->buffer.id,
			command->buffer_write.offset,
			command->buffer_write.bytes_size,
			command->buffer_write.bytes
		);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_WRITE:
	{
		auto h = command->texture_write.handle;
		auto gl_format = _renoir_pixelformat_to_gl(h->texture.desc.pixel_format);
		auto gl_type = _renoir_pixelformat_to_type_gl(h->texture.desc.pixel_format);

		// change alignment to match pixel data
		GLint original_pack_alignment = 0;
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &original_pack_alignment);
		if (h->texture.desc.pixel_format == RENOIR_PIXELFORMAT_R8)
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		mn_defer({
			if (h->texture.desc.pixel_format == RENOIR_PIXELFORMAT_R8)
				glPixelStorei(GL_UNPACK_ALIGNMENT, original_pack_alignment);
		});

		if (h->texture.desc.size.height == 0 && h->texture.desc.size.depth == 0)
		{
			// 1D texture
			glTextureSubImage1D(
				h->texture.id,
				0,
				command->texture_write.desc.x,
				command->texture_write.desc.width,
				gl_format,
				gl_type,
				command->texture_write.desc.bytes
			);
			if (h->texture.desc.mipmaps > 1)
				glGenerateTextureMipmap(h->texture.id);
		}
		else if (h->texture.desc.size.height > 0 && h->texture.desc.size.depth == 0)
		{
			if (h->texture.desc.cube_map == false)
			{
				// 2D texture
				glTextureSubImage2D(
					h->texture.id,
					0,
					command->texture_write.desc.x,
					command->texture_write.desc.y,
					command->texture_write.desc.width,
					command->texture_write.desc.height,
					gl_format,
					gl_type,
					command->texture_write.desc.bytes
				);
				if (h->texture.desc.mipmaps > 1)
					glGenerateTextureMipmap(h->texture.id);
			}
			else
			{
				// Cube Map texture
				glTextureSubImage3D(
					h->texture.id,
					0,
					command->texture_write.desc.x,
					command->texture_write.desc.y,
					command->texture_write.desc.z,
					command->texture_write.desc.width,
					command->texture_write.desc.height,
					1,
					gl_format,
					gl_type,
					command->texture_write.desc.bytes
				);
				if (h->texture.desc.mipmaps > 1)
					glGenerateTextureMipmap(h->texture.id);
			}
		}
		else if (h->texture.desc.size.height > 0 && h->texture.desc.size.depth > 0)
		{
			// 3D texture
			glTextureSubImage3D(
				h->texture.id,
				0,
				command->texture_write.desc.x,
				command->texture_write.desc.y,
				command->texture_write.desc.z,
				command->texture_write.desc.width,
				command->texture_write.desc.height,
				command->texture_write.desc.depth,
				gl_format,
				gl_type,
				command->texture_write.desc.bytes
			);
			if (h->texture.desc.mipmaps > 1)
				glGenerateTextureMipmap(h->texture.id);
		}
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_READ:
	{
		auto h = command->buffer_read.handle;
		void* ptr = glMapNamedBufferRange(
			h->buffer.id,
			command->buffer_read.offset,
			command->buffer_read.bytes_size,
			GL_MAP_READ_BIT
		);
		::memcpy(command->buffer_read.bytes, ptr, command->buffer_read.bytes_size);
		glUnmapNamedBuffer(h->buffer.id);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_READ:
	{
		auto h = command->texture_read.handle;
		auto gl_format = _renoir_pixelformat_to_gl(h->texture.desc.pixel_format);
		auto gl_type = _renoir_pixelformat_to_type_gl(h->texture.desc.pixel_format);

		// change alignment to match pixel data
		GLint original_pack_alignment = 0;
		glGetIntegerv(GL_PACK_ALIGNMENT, &original_pack_alignment);
		if (h->texture.desc.pixel_format == RENOIR_PIXELFORMAT_R8)
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
		mn_defer({
			if (h->texture.desc.pixel_format == RENOIR_PIXELFORMAT_R8)
				glPixelStorei(GL_PACK_ALIGNMENT, original_pack_alignment);
		});

		if (h->texture.desc.size.height == 0 && h->texture.desc.size.depth == 0)
		{
			// 1D texture
			glGetTextureSubImage(
				h->texture.id,
				0,
				command->texture_read.desc.x,
				0,
				0,
				command->texture_read.desc.width,
				1,
				0,
				gl_format,
				gl_type,
				command->texture_read.desc.bytes_size,
				command->texture_read.desc.bytes
			);
		}
		else if (h->texture.desc.size.height > 0 && h->texture.desc.size.depth == 0)
		{
			if (h->texture.desc.cube_map == false)
			{
				// 2D texture
				glGetTextureSubImage(
					h->texture.id,
					0,
					command->texture_read.desc.x,
					command->texture_read.desc.y,
					0,
					command->texture_read.desc.width,
					command->texture_read.desc.height,
					1,
					gl_format,
					gl_type,
					command->texture_read.desc.bytes_size,
					command->texture_read.desc.bytes
				);
			}
			else
			{
				// 2D texture
				glGetTextureSubImage(
					h->texture.id,
					0,
					command->texture_read.desc.x,
					command->texture_read.desc.y,
					command->texture_read.desc.z,
					command->texture_read.desc.width,
					command->texture_read.desc.height,
					1,
					gl_format,
					gl_type,
					command->texture_read.desc.bytes_size,
					command->texture_read.desc.bytes
				);
			}
		}
		else if (h->texture.desc.size.height > 0 && h->texture.desc.size.depth > 0)
		{
			// 3D texture
			glGetTextureSubImage(
				h->texture.id,
				0,
				command->texture_read.desc.x,
				command->texture_read.desc.y,
				command->texture_read.desc.z,
				command->texture_read.desc.width,
				command->texture_read.desc.height,
				command->texture_read.desc.depth,
				gl_format,
				gl_type,
				command->texture_read.desc.bytes_size,
				command->texture_read.desc.bytes
			);
		}
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_BIND:
	{
		auto h = command->buffer_bind.handle;
		mn_assert(h->buffer.type == RENOIR_BUFFER_UNIFORM || h->buffer.type == RENOIR_BUFFER_COMPUTE);
		auto gl_type = _renoir_buffer_type_to_gl(h->buffer.type);
		glBindBufferBase(gl_type, command->buffer_bind.slot, h->buffer.id);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_STORAGE_BIND:
	{
		for (int i = 0; i < RENOIR_CONSTANT_BUFFER_STORAGE_SIZE; ++i)
		{
			auto h = command->buffer_storage_bind.handle[i];
			if (h == nullptr)
				continue;

			auto gl_type = _renoir_buffer_type_to_gl(h->buffer.type);
			glBindBufferBase(gl_type, command->buffer_storage_bind.start_slot + i, h->buffer.id);
		}
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_BIND:
	{
		auto h = command->texture_bind.handle;
		glActiveTexture(GL_TEXTURE0 + command->texture_bind.slot);
		if (command->texture_bind.sampler == nullptr)
		{
			auto gl_format = _renoir_pixelformat_to_gl_compute(h->texture.desc.pixel_format);
			auto gl_gpu_access = _renoir_access_to_gl(command->texture_bind.gpu_access);
			auto layered = GL_FALSE;
			if (h->texture.desc.size.depth > 0 ||
				h->texture.desc.cube_map)
			{
				layered = GL_TRUE;
			}

			glBindImageTexture(
				command->texture_bind.slot,
				h->texture.id,
				command->texture_bind.level,
				layered,
				0,
				gl_gpu_access,
				gl_format
			);
		}
		else
		{
			if (h->texture.desc.size.height == 0 && h->texture.desc.size.depth == 0)
			{
				// 1D texture
				glBindTexture(GL_TEXTURE_1D, h->texture.id);
			}
			else if (h->texture.desc.size.height > 0 && h->texture.desc.size.depth == 0)
			{
				if (h->texture.desc.cube_map == false)
				{
					// 2D texture
					glBindTexture(GL_TEXTURE_2D, h->texture.id);
				}
				else
				{
					glBindTexture(GL_TEXTURE_CUBE_MAP, h->texture.id);
				}
			}
			else if (h->texture.desc.size.height > 0 && h->texture.desc.size.depth > 0)
			{
				// 3D texture
				glBindTexture(GL_TEXTURE_3D, h->texture.id);
			}
			// bind the used sampler
			glBindSampler(command->texture_bind.slot, command->texture_bind.sampler->sampler.id);
		}
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_DRAW:
	{
		mn_assert_msg(self->current_pipeline && self->current_program, "you should use a program and a pipeline before drawing");

		auto& desc = command->draw.desc;
		glBindVertexArray(self->vao);

		for (size_t i = 0; i < RENOIR_CONSTANT_DRAW_VERTEX_BUFFER_SIZE; ++i)
		{
			auto& vertex = desc.vertex_buffers[i];
			if (vertex.buffer.handle == nullptr)
				continue;

			// calculate the default stride for the vertex buffer
			if (vertex.stride == 0)
				vertex.stride = _renoir_type_to_size(vertex.type);

			auto h = (Renoir_Handle*)vertex.buffer.handle;

			glBindBuffer(GL_ARRAY_BUFFER, h->buffer.id);

			GLint gl_size = _renoir_type_to_gl_element_count(vertex.type);
			GLenum gl_type = _renoir_type_to_gl(vertex.type);

			if (_renoir_type_is_int(vertex.type))
			{
				glVertexAttribIPointer(
					GLuint(i),
					gl_size,
					gl_type,
					vertex.stride,
					(void*)vertex.offset
				);
			}
			else
			{
				bool gl_normalized = _renoir_type_normalized(vertex.type);
				glVertexAttribPointer(
					GLuint(i),
					gl_size,
					gl_type,
					gl_normalized,
					vertex.stride,
					(void*)vertex.offset
				);
			}
			glEnableVertexAttribArray(i);
		}

		auto gl_primitive = _renoir_primitive_to_gl(desc.primitive);
		if (desc.index_buffer.handle != nullptr)
		{
			if (desc.index_type == RENOIR_TYPE_NONE)
				desc.index_type = RENOIR_TYPE_UINT16;

			auto gl_index_type = _renoir_type_to_gl(desc.index_type);
			auto gl_index_type_size = _renoir_type_to_size(desc.index_type);

			auto h = (Renoir_Handle*)desc.index_buffer.handle;
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, h->buffer.id);

			if (desc.instances_count > 1)
			{
				glDrawElementsInstanced(
					gl_primitive,
					desc.elements_count,
					gl_index_type,
					(void*)(desc.base_element * gl_index_type_size),
					desc.instances_count
				);
			}
			else
			{
				glDrawElements(
					gl_primitive,
					desc.elements_count,
					gl_index_type,
					(void*)(desc.base_element * gl_index_type_size)
				);
			}
		}
		else
		{
			if (desc.instances_count > 1)
				glDrawArraysInstanced(gl_primitive, desc.base_element, desc.elements_count, desc.instances_count);
			else
				glDrawArrays(gl_primitive, desc.base_element, desc.elements_count);
		}
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_DISPATCH:
	{
		mn_assert_msg(self->current_compute, "you should use a compute before dispatching it");
		glDispatchCompute(command->dispatch.x, command->dispatch.y, command->dispatch.z);
		// Note(Moustapha): not sure about this barrier
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_BEGIN:
	{
		auto h = command->timer_begin.handle;
		glQueryCounter(h->timer.timepoints[0], GL_TIMESTAMP);
		mn_assert(_renoir_gl450_check());
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_END:
	{
		auto h = command->timer_begin.handle;
		glQueryCounter(h->timer.timepoints[1], GL_TIMESTAMP);
		mn_assert(_renoir_gl450_check());
		break;
	}
	default:
		mn_unreachable();
		break;
	}
}

inline static Renoir_Handle*
_renoir_gl450_sampler_new(IRenoir* self, Renoir_Sampler_Desc desc)
{
	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_SAMPLER);
	h->sampler.desc = desc;

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_SAMPLER_NEW);
	command->sampler_new.handle = h;
	command->sampler_new.desc = desc;
	_renoir_gl450_command_process(self, command);
	return h;
}

inline static void
_renoir_gl450_sampler_free(IRenoir* self, Renoir_Handle* h)
{
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_SAMPLER_FREE);
	command->sampler_free.handle = h;
	_renoir_gl450_command_process(self, command);
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
_renoir_gl450_sampler_get(IRenoir* self, Renoir_Sampler_Desc desc)
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

	// we didn't find an empty slot for the new sampler, so we'll have to make one for it
	if (sampler_ix == self->sampler_cache.count)
	{
		auto to_be_evicted = mn::buf_top(self->sampler_cache);
		for (size_t i = 0; i + 1 < self->sampler_cache.count; ++i)
		{
			auto index = self->sampler_cache.count - i - 1;
			self->sampler_cache[index] = self->sampler_cache[index - 1];
		}
		_renoir_gl450_sampler_free(self, to_be_evicted);
		mn::log_warning("gl450: sampler evicted");
		sampler_ix = 0;
	}

	// create the new sampler and put it at the head of the cache
	auto sampler = _renoir_gl450_sampler_new(self, desc);
	self->sampler_cache[sampler_ix] = sampler;
	return sampler;
}

inline static void
_renoir_gl450_handle_leak_free(IRenoir* self, Renoir_Command* command)
{
	switch(command->kind)
	{
	case RENOIR_COMMAND_KIND_SWAPCHAIN_FREE:
	{
		auto h = command->swapchain_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		_renoir_gl450_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_FREE:
	{
		auto h = command->pass_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
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
					auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = color;
					_renoir_gl450_handle_leak_free(self, command);
				}

				auto depth = (Renoir_Handle*)h->raster_pass.offscreen.depth_stencil.texture.handle;
				if (depth)
				{
					// issue command to free the depth texture
					auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = depth;
					_renoir_gl450_handle_leak_free(self, command);
				}
			}
		}
		_renoir_gl450_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	{
		auto h = command->buffer_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		_renoir_gl450_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	{
		auto h = command->texture_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		_renoir_gl450_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	{
		auto h = command->sampler_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		_renoir_gl450_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	{
		auto h = command->program_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		_renoir_gl450_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
	{
		auto h = command->compute_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		_renoir_gl450_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_FREE:
	{
		auto h = command->timer_free.handle;
		if (_renoir_gl450_handle_unref(h) == false)
			break;
		_renoir_gl450_handle_free(self, h);
		break;
	}
	}
}

// API
static bool
_renoir_gl450_init(Renoir* api, Renoir_Settings settings, void* display)
{
	static_assert(RENOIR_CONSTANT_DEFAULT_SAMPLER_CACHE_SIZE > 0, "sampler cache size should be > 0");
	static_assert(RENOIR_CONSTANT_DEFAULT_PIPELINE_CACHE_SIZE > 0, "pipeline cache size should be > 0");

	if (settings.sampler_cache_size <= 0)
		settings.sampler_cache_size = RENOIR_CONSTANT_DEFAULT_SAMPLER_CACHE_SIZE;
	if (settings.pipeline_cache_size <= 0)
		settings.pipeline_cache_size = RENOIR_CONSTANT_DEFAULT_PIPELINE_CACHE_SIZE;

	auto ctx = renoir_gl450_context_new(&settings, display);
	if (ctx == nullptr && settings.external_context == false)
		return false;

	auto self = mn::alloc_zerod<IRenoir>();
	self->mtx = mn_mutex_new_with_srcloc("renoir gl450");
	self->handle_pool = mn::pool_new(sizeof(Renoir_Handle), 128);
	self->command_pool = mn::pool_new(sizeof(Renoir_Command), 128);
	self->settings = settings;
	self->info_description = mn::str_new();
	self->ctx = ctx;
	self->sampler_cache = mn::buf_new<Renoir_Handle*>();
	self->state = _renoir_gl450_state_new();
	self->alive_handles = mn::map_new<Renoir_Handle*, Renoir_Leak_Info>();
	mn::buf_resize_fill(self->sampler_cache, self->settings.sampler_cache_size, nullptr);

	self->current_pipeline = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_PIPELINE);
	self->current_pipeline->pipeline.desc = Renoir_Pipeline_Desc{};
	_renoir_gl450_pipeline_desc_defaults(&self->current_pipeline->pipeline.desc);

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_INIT);
	_renoir_gl450_command_process(self, command);

	api->ctx = self;

	return true;
}

static void
_renoir_gl450_dispose(Renoir* api)
{
	auto self = api->ctx;
	// process these commands for frees to give correct leak report
	for (auto it = self->command_list_head; it != nullptr; it = it->next)
		_renoir_gl450_handle_leak_free(self, it);
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
	renoir_gl450_context_free(self->ctx);
	mn::pool_free(self->handle_pool);
	mn::pool_free(self->command_pool);
	mn::str_free(self->info_description);
	mn::buf_free(self->sampler_cache);
	_renoir_gl450_state_free(self->state);
	mn::map_free(self->alive_handles);
	mn::free(self);
}

static const char*
_renoir_gl450_name()
{
	return "gl450";
}

static RENOIR_TEXTURE_ORIGIN
_renoir_gl450_texture_origin()
{
	return RENOIR_TEXTURE_ORIGIN_BOTTOM_LEFT;
}

static Renoir_Info
_renoir_gl450_info(Renoir* api)
{
	auto self = api->ctx;

	Renoir_Info res{};
	res.description = self->info_description.ptr;
	return res;
}

static void
_renoir_gl450_handle_ref(Renoir* api, void* handle)
{
	auto h = (Renoir_Handle*)handle;
	h->rc.fetch_add(1);
}

static void
_renoir_gl450_flush(Renoir* api, void*, void*)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	if (auto error = glGetError(); error != GL_NO_ERROR)
	{
		mn::log_error("external opengl context has error {:#x}", error);
	}

	if (self->glewInited)
		_renoir_gl450_state_capture(self->state);

	// process commands
	for(auto it = self->command_list_head; it != nullptr; it = it->next)
	{
		_renoir_gl450_command_execute(self, it);
		_renoir_gl450_command_free(self, it);
	}

	mn_assert(_renoir_gl450_check());

	_renoir_gl450_state_reset(self->state);

	self->command_list_head = nullptr;
	self->command_list_tail = nullptr;
}

static Renoir_Swapchain
_renoir_gl450_swapchain_new(Renoir* api, int width, int height, void* window, void* display)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_SWAPCHAIN);
	h->swapchain.width = width;
	h->swapchain.height = height;
	h->swapchain.handle = window;
	h->swapchain.display = display;

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_SWAPCHAIN_NEW);
	command->swapchain_new.handle = h;
	_renoir_gl450_command_process(self, command);
	return Renoir_Swapchain{h};
}

static void
_renoir_gl450_swapchain_free(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_SWAPCHAIN_FREE);
	command->swapchain_free.handle = h;
	_renoir_gl450_command_process(self, command);
}

static void
_renoir_gl450_swapchain_resize(Renoir*, Renoir_Swapchain swapchain, int width, int height)
{
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	h->swapchain.width = width;
	h->swapchain.height = height;
}

static void
_renoir_gl450_swapchain_present(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	// process commands
	for(auto it = self->command_list_head; it != nullptr; it = it->next)
	{
		_renoir_gl450_command_execute(self, it);
		_renoir_gl450_command_free(self, it);
	}

	self->command_list_head = nullptr;
	self->command_list_tail = nullptr;

	renoir_gl450_context_window_present(self->ctx, h);
}

static Renoir_Buffer
_renoir_gl450_buffer_new(Renoir* api, Renoir_Buffer_Desc desc)
{
	if (desc.usage == RENOIR_USAGE_NONE)
		desc.usage = RENOIR_USAGE_STATIC;

	if (desc.usage == RENOIR_USAGE_DYNAMIC && desc.access == RENOIR_ACCESS_NONE)
	{
		mn_unreachable_msg("a dynamic buffer with cpu access set to none is a static buffer");
	}

	if (desc.usage == RENOIR_USAGE_STATIC && desc.data == nullptr)
	{
		mn_unreachable_msg("a static buffer should have data to initialize it");
	}

	if (desc.type == RENOIR_BUFFER_UNIFORM && desc.data_size % 16 != 0)
	{
		mn_unreachable_msg("uniform buffers should be aligned to 16 bytes");
	}

	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_BUFFER);
	h->buffer.access = desc.access;
	h->buffer.type = desc.type;
	h->buffer.usage = desc.usage;
	h->buffer.size = desc.data_size;

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_NEW);
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
	_renoir_gl450_command_process(self, command);
	return Renoir_Buffer{h};
}

static void
_renoir_gl450_buffer_free(Renoir* api, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_FREE);
	command->buffer_free.handle = h;
	_renoir_gl450_command_process(self, command);
}

static size_t
_renoir_gl450_buffer_size(Renoir* api, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr && h->kind == RENOIR_HANDLE_KIND_BUFFER);

	return h->buffer.size;
}

static Renoir_Texture
_renoir_gl450_texture_new(Renoir* api, Renoir_Texture_Desc desc)
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

	bool no_data = true;
	for (size_t i = 0; i < sizeof(desc.data) / sizeof(*desc.data); ++i)
	{
		if (desc.data[i] != nullptr)
		{
			no_data = false;
			break;
		}
	}

	if (desc.render_target == false && desc.usage == RENOIR_USAGE_STATIC && no_data)
	{
		mn_unreachable_msg("a static texture should have data to initialize it");
	}

	if (desc.cube_map)
	{
		mn_assert_msg(desc.size.width == desc.size.height, "width should equal height in cube map texture");
	}

	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_TEXTURE);
	h->texture.desc = desc;
	::memset(h->texture.desc.data, 0, sizeof(h->texture.desc.data));
	h->texture.desc.data_size = 0;

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_NEW);
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
	_renoir_gl450_command_process(self, command);
	return Renoir_Texture{h};
}

static void
_renoir_gl450_texture_free(Renoir* api, Renoir_Texture texture)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
	command->texture_free.handle = h;
	_renoir_gl450_command_process(self, command);
}

static void*
_renoir_gl450_texture_native_handle(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);
	return (void*)h->texture.id;
}

static Renoir_Size
_renoir_gl450_texture_size(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr && h->kind == RENOIR_HANDLE_KIND_TEXTURE);
	return h->texture.desc.size;
}

static Renoir_Texture_Desc
_renoir_gl450_texture_desc(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_TEXTURE);
	return h->texture.desc;
}

static Renoir_Program
_renoir_gl450_program_new(Renoir* api, Renoir_Program_Desc desc)
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

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_PROGRAM);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PROGRAM_NEW);
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
	_renoir_gl450_command_process(self, command);
	return Renoir_Program{h};
}

static void
_renoir_gl450_program_free(Renoir* api, Renoir_Program program)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)program.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PROGRAM_FREE);
	command->program_free.handle = h;
	_renoir_gl450_command_process(self, command);
}

static Renoir_Compute
_renoir_gl450_compute_new(Renoir* api, Renoir_Compute_Desc desc)
{
	mn_assert(desc.compute.bytes != nullptr);
	if (desc.compute.size == 0)
		desc.compute.size = ::strlen(desc.compute.bytes);

	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_COMPUTE);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_COMPUTE_NEW);
	command->compute_new.handle = h;
	command->compute_new.desc = desc;
	if (self->settings.defer_api_calls)
	{
		command->compute_new.desc.compute.bytes = (char*)mn::alloc(command->compute_new.desc.compute.size, alignof(char)).ptr;
		::memcpy((char*)command->compute_new.desc.compute.bytes, desc.compute.bytes, desc.compute.size);

		command->compute_new.owns_data = true;
	}
	_renoir_gl450_command_process(self, command);
	return Renoir_Compute{h};
}

static void
_renoir_gl450_compute_free(Renoir* api, Renoir_Compute compute)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)compute.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_COMPUTE_FREE);
	command->compute_free.handle = h;
	_renoir_gl450_command_process(self, command);
}

static Renoir_Pass
_renoir_gl450_pass_swapchain_new(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_RASTER_PASS);
	h->raster_pass.swapchain = (Renoir_Handle*)swapchain.handle;

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_SWAPCHAIN_NEW);
	command->pass_swapchain_new.handle = h;
	_renoir_gl450_command_process(self, command);
	return Renoir_Pass{h};
}

static Renoir_Pass
_renoir_gl450_pass_offscreen_new(Renoir* api, Renoir_Pass_Offscreen_Desc desc)
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

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_RASTER_PASS);
	h->raster_pass.offscreen = desc;
	h->raster_pass.width = width;
	h->raster_pass.height = height;

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW);
	command->pass_offscreen_new.handle = h;
	command->pass_offscreen_new.desc = desc;
	_renoir_gl450_command_process(self, command);
	return Renoir_Pass{h};
}

static Renoir_Pass
_renoir_gl450_pass_compute_new(Renoir* api)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_COMPUTE_PASS);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_COMPUTE_NEW);
	command->pass_compute_new.handle = h;
	_renoir_gl450_command_process(self, command);
	return Renoir_Pass{h};
}

static void
_renoir_gl450_pass_free(Renoir* api, Renoir_Pass pass)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_FREE);
	command->pass_free.handle = h;
	_renoir_gl450_command_process(self, command);
}

static Renoir_Size
_renoir_gl450_pass_size(Renoir* api, Renoir_Pass pass)
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
_renoir_gl450_pass_offscreen_desc(Renoir* api, Renoir_Pass pass)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
	return h->raster_pass.offscreen;
}

static Renoir_Timer
_renoir_gl450_timer_new(Renoir* api)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto h = _renoir_gl450_handle_new(self, RENOIR_HANDLE_KIND_TIMER);

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TIMER_NEW);
	command->timer_new.handle = h;
	_renoir_gl450_command_process(self, command);
	return Renoir_Timer{h};
}

static void
_renoir_gl450_timer_free(struct Renoir* api, Renoir_Timer timer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)timer.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TIMER_FREE);
	command->timer_free.handle = h;
	_renoir_gl450_command_process(self, command);
}

static bool
_renoir_gl450_timer_elapsed(struct Renoir* api, Renoir_Timer timer, uint64_t* elapsed_time_in_nanos)
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
		auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TIMER_ELAPSED);
		h->timer.state = RENOIR_TIMER_STATE_READ_SCHEDULED;
		mn::mutex_unlock(self->mtx);

		command->timer_elapsed.handle = h;
		_renoir_gl450_command_process(self, command);

		return false;
	}

	return false;
}

// Graphics Commands
static void
_renoir_gl450_pass_submit(Renoir* api, Renoir_Pass pass)
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
				auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_BEGIN);
				command->pass_begin.handle = h;
				_renoir_gl450_command_push_front(&h->raster_pass, command);
			}

			// push the pass end command
			{
				auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_END);
				command->pass_end.handle = h;
				_renoir_gl450_command_push_back(&h->raster_pass, command);
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
					_renoir_gl450_command_execute(self, it);
					_renoir_gl450_command_free(self, it);
				}
			}

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
				auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_BEGIN);
				command->pass_begin.handle = h;
				_renoir_gl450_command_push_front(&h->compute_pass, command);
			}

			// push the pass end command
			{
				auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_END);
				command->pass_end.handle = h;
				_renoir_gl450_command_push_back(&h->compute_pass, command);
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
					_renoir_gl450_command_execute(self, it);
					_renoir_gl450_command_free(self, it);
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
_renoir_gl450_clear(Renoir* api, Renoir_Pass pass, Renoir_Clear_Desc desc)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	if (desc.independent_clear_color == RENOIR_SWITCH_DEFAULT)
		desc.independent_clear_color = RENOIR_SWITCH_DISABLE;

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_PASS_CLEAR);
	mn::mutex_unlock(self->mtx);

	command->pass_clear.desc = desc;
	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_use_pipeline(Renoir* api, Renoir_Pass pass, Renoir_Pipeline_Desc pipeline_desc)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
	_renoir_gl450_pipeline_desc_defaults(&pipeline_desc);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_USE_PIPELINE);
	mn::mutex_unlock(self->mtx);

	command->use_pipeline.pipeline_desc = pipeline_desc;
	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_use_program(Renoir* api, Renoir_Pass pass, Renoir_Program program)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_USE_PROGRAM);
	mn::mutex_unlock(self->mtx);

	command->use_program.program = (Renoir_Handle*)program.handle;
	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_use_compute(Renoir* api, Renoir_Pass pass, Renoir_Compute compute)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_USE_COMPUTE);
	mn::mutex_unlock(self->mtx);

	command->use_compute.compute = (Renoir_Handle*)compute.handle;
	_renoir_gl450_command_push_back(&h->compute_pass, command);
}

static void
_renoir_gl450_scissor(Renoir* api, Renoir_Pass pass, int x, int y, int width, int height)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_SCISSOR);
	mn::mutex_unlock(self->mtx);

	command->scissor.x = x;
	command->scissor.y = y;
	command->scissor.w = width;
	command->scissor.h = height;
	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_buffer_zero_global(Renoir* api, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto hbuffer = (Renoir_Handle*)buffer.handle;
	mn_assert(hbuffer != nullptr);

	mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_CLEAR);
	command->buffer_clear.handle = hbuffer;
	_renoir_gl450_command_process(self, command);
}

static void
_renoir_gl450_buffer_zero(Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	if (h == nullptr)
	{
		_renoir_gl450_buffer_zero_global(api, buffer);
	}
	else
	{
		auto hbuffer = (Renoir_Handle*)buffer.handle;
		mn_assert(hbuffer != nullptr);

		mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);

		mn::mutex_lock(self->mtx);
		auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_CLEAR);
		mn::mutex_unlock(self->mtx);

		command->buffer_clear.handle = hbuffer;

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			_renoir_gl450_command_push_back(&h->raster_pass, command);
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			_renoir_gl450_command_push_back(&h->compute_pass, command);
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
	}
}

static void
_renoir_gl450_buffer_write_global(Renoir* api, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size)
{
	auto self = api->ctx;
	auto hbuffer = (Renoir_Handle*)buffer.handle;
	mn_assert(hbuffer != nullptr);

	mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_WRITE);
	command->buffer_write.handle = hbuffer;
	command->buffer_write.offset = offset;
	command->buffer_write.bytes = mn::alloc(bytes_size, alignof(char)).ptr;
	command->buffer_write.bytes_size = bytes_size;
	::memcpy(command->buffer_write.bytes, bytes, bytes_size);
	_renoir_gl450_command_process(self, command);
}

static void
_renoir_gl450_buffer_write(Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size)
{
	// this means he's trying to write nothing so no-op
	if (bytes_size == 0)
		return;

	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	if (h == nullptr)
	{
		_renoir_gl450_buffer_write_global(api, buffer, offset, bytes, bytes_size);
	}
	else
	{
		auto hbuffer = (Renoir_Handle*)buffer.handle;
		mn_assert(hbuffer != nullptr);

		mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);

		mn::mutex_lock(self->mtx);
		auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_WRITE);
		mn::mutex_unlock(self->mtx);

		command->buffer_write.handle = hbuffer;
		command->buffer_write.offset = offset;
		command->buffer_write.bytes = mn::alloc(bytes_size, alignof(char)).ptr;
		command->buffer_write.bytes_size = bytes_size;
		::memcpy(command->buffer_write.bytes, bytes, bytes_size);

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			_renoir_gl450_command_push_back(&h->raster_pass, command);
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			_renoir_gl450_command_push_back(&h->compute_pass, command);
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
	}
}

static void
_renoir_gl450_texture_write_global(Renoir* api, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc)
{
	auto self = api->ctx;

	auto htexture = (Renoir_Handle*)texture.handle;
	mn_assert(htexture->texture.desc.usage != RENOIR_USAGE_STATIC);

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_WRITE);
	command->texture_write.handle = htexture;
	command->texture_write.desc = desc;
	command->texture_write.desc.bytes = mn::alloc(desc.bytes_size, alignof(char)).ptr;
	::memcpy(command->texture_write.desc.bytes, desc.bytes, desc.bytes_size);
	_renoir_gl450_command_process(self, command);
}

static void
_renoir_gl450_texture_write(Renoir* api, Renoir_Pass pass, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc)
{
	// this means he's trying to write nothing so no-op
	if (desc.bytes_size == 0)
		return;

	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	if (h == nullptr)
	{
		_renoir_gl450_texture_write_global(api, texture, desc);
	}
	else
	{
		auto htexture = (Renoir_Handle*)texture.handle;
		mn_assert(htexture->texture.desc.usage != RENOIR_USAGE_STATIC);

		mn::mutex_lock(self->mtx);
		auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_WRITE);
		mn::mutex_unlock(self->mtx);

		command->texture_write.handle = htexture;
		command->texture_write.desc = desc;
		command->texture_write.desc.bytes = mn::alloc(desc.bytes_size, alignof(char)).ptr;
		::memcpy(command->texture_write.desc.bytes, desc.bytes, desc.bytes_size);

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			_renoir_gl450_command_push_back(&h->raster_pass, command);
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			_renoir_gl450_command_push_back(&h->compute_pass, command);
		}
		else
		{
			mn_unreachable_msg("invalid pass");
		}
	}
}

static void
_renoir_gl450_buffer_read(Renoir* api, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size)
{
	// this means he's trying to read nothing so no-op
	if (bytes_size == 0)
		return;

	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr);
	// this means that buffer creation didn't execute yet
	if (h->buffer.id == 0)
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
	_renoir_gl450_command_execute(self, &command);
	mn::mutex_unlock(self->mtx);
}

static void
_renoir_gl450_texture_read(Renoir* api, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc)
{
	// this means he's trying to read nothing so no-op
	if (desc.bytes_size == 0)
		return;

	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);
	// this means that texture creation didn't execute yet
	if (h->texture.id == 0)
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
	_renoir_gl450_command_execute(self, &command);
	mn::mutex_unlock(self->mtx);
}

static void
_renoir_gl450_buffer_bind(Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, RENOIR_SHADER shader, int slot)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_BIND);
	mn::mutex_unlock(self->mtx);

	command->buffer_bind.handle = (Renoir_Handle*)buffer.handle;
	command->buffer_bind.shader = shader;
	command->buffer_bind.slot = slot;

	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_buffer_storage_bind(Renoir* api, Renoir_Pass pass, Renoir_Buffer_Storage_Bind_Desc desc)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_STORAGE_BIND);
	mn::mutex_unlock(self->mtx);

	size_t render_target_count = 0;
	if (h->raster_pass.swapchain)
	{
		render_target_count = 1;
	}
	else if (h->raster_pass.fb)
	{
		for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
		{
			auto texture = h->raster_pass.offscreen.color[i].texture;
			if (texture.handle == nullptr)
				continue;
			++render_target_count;
		}
	}
	else
	{
		mn_unreachable();
	}
	mn_assert(desc.start_slot + render_target_count <= 8);
	mn_assert(desc.start_slot >= render_target_count);

	for (int i = 0; i < RENOIR_CONSTANT_BUFFER_STORAGE_SIZE; ++i)
	{
		auto h = (Renoir_Handle*)desc.buffers[i].handle;
		if (h)
			command->buffer_storage_bind.handle[i] = h;
	}
	command->buffer_storage_bind.start_slot = desc.start_slot;

	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_texture_bind(Renoir* api, Renoir_Pass pass, Renoir_Texture texture, RENOIR_SHADER shader, int slot)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htex = (Renoir_Handle*)texture.handle;
	mn_assert(htex != nullptr);

	mn::mutex_lock(self->mtx);
	auto sampler = _renoir_gl450_sampler_get(self, htex->texture.desc.sampler);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_BIND);
	mn::mutex_unlock(self->mtx);

	command->texture_bind.handle = htex;
	command->texture_bind.shader = shader;
	command->texture_bind.slot = slot;
	command->texture_bind.sampler = sampler;

	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_texture_sampler_bind(Renoir* api, Renoir_Pass pass, Renoir_Texture texture, RENOIR_SHADER shader, int slot, Renoir_Sampler_Desc sampler)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htex = (Renoir_Handle*)texture.handle;
	mn_assert(htex != nullptr);

	mn::mutex_lock(self->mtx);
	auto hsampler = _renoir_gl450_sampler_get(self, sampler);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_BIND);
	mn::mutex_unlock(self->mtx);

	command->texture_bind.handle = htex;
	command->texture_bind.shader = shader;
	command->texture_bind.slot = slot;
	command->texture_bind.sampler = hsampler;

	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_buffer_compute_bind(Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, int slot, RENOIR_ACCESS gpu_access)
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
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_BUFFER_BIND);
	mn::mutex_unlock(self->mtx);

	command->buffer_bind.handle = (Renoir_Handle*)buffer.handle;
	command->buffer_bind.shader = RENOIR_SHADER_COMPUTE;
	command->buffer_bind.slot = slot;
	command->buffer_bind.gpu_access = gpu_access;

	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_texture_compute_bind(Renoir* api, Renoir_Pass pass, Renoir_Texture texture, int slot, int mip_level, RENOIR_ACCESS gpu_access)
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
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_BIND);
	mn::mutex_unlock(self->mtx);

	command->texture_bind.handle = htex;
	command->texture_bind.shader = RENOIR_SHADER_COMPUTE;
	command->texture_bind.slot = slot;
	command->texture_bind.level = mip_level;
	command->texture_bind.gpu_access = gpu_access;

	_renoir_gl450_command_push_back(&h->compute_pass, command);
}

static void
_renoir_gl450_draw(Renoir* api, Renoir_Pass pass, Renoir_Draw_Desc desc)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_DRAW);
	mn::mutex_unlock(self->mtx);

	command->draw.desc = desc;

	_renoir_gl450_command_push_back(&h->raster_pass, command);
}

static void
_renoir_gl450_dispatch(Renoir* api, Renoir_Pass pass, int x, int y, int z)
{
	mn_assert(x >= 0 && y >= 0 && z >= 0);

	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_DISPATCH);
	mn::mutex_unlock(self->mtx);

	command->dispatch.x = x;
	command->dispatch.y = y;
	command->dispatch.z = z;

	_renoir_gl450_command_push_back(&h->compute_pass, command);
}

static void
_renoir_gl450_timer_begin(struct Renoir* api, Renoir_Pass pass, Renoir_Timer timer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htimer = (Renoir_Handle*)timer.handle;
	mn_assert(htimer != nullptr && htimer->kind == RENOIR_HANDLE_KIND_TIMER);

	if(htimer->timer.state != RENOIR_TIMER_STATE_NONE)
		return;

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TIMER_BEGIN);
	mn::mutex_unlock(self->mtx);

	command->timer_begin.handle = htimer;
	htimer->timer.state = RENOIR_TIMER_STATE_BEGIN;

	if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
	{
		_renoir_gl450_command_push_back(&h->raster_pass, command);
	}
	else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
	{
		_renoir_gl450_command_push_back(&h->compute_pass, command);
	}
	else
	{
		mn_unreachable();
	}
}

static void
_renoir_gl450_timer_end(struct Renoir* api, Renoir_Pass pass, Renoir_Timer timer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htimer = (Renoir_Handle*)timer.handle;
	mn_assert(htimer != nullptr && htimer->kind == RENOIR_HANDLE_KIND_TIMER);
	if (htimer->timer.state != RENOIR_TIMER_STATE_BEGIN)
		return;

	mn::mutex_lock(self->mtx);
	auto command = _renoir_gl450_command_new(self, RENOIR_COMMAND_KIND_TIMER_END);
	mn::mutex_unlock(self->mtx);

	command->timer_end.handle = htimer;
	htimer->timer.state = RENOIR_TIMER_STATE_END;

	if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
	{
		_renoir_gl450_command_push_back(&h->raster_pass, command);
	}
	else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
	{
		_renoir_gl450_command_push_back(&h->compute_pass, command);
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

	api->init = _renoir_gl450_init;
	api->dispose = _renoir_gl450_dispose;

	api->name = _renoir_gl450_name;
	api->texture_origin = _renoir_gl450_texture_origin;
	api->info = _renoir_gl450_info;

	api->handle_ref = _renoir_gl450_handle_ref;
	api->flush = _renoir_gl450_flush;

	api->swapchain_new = _renoir_gl450_swapchain_new;
	api->swapchain_free = _renoir_gl450_swapchain_free;
	api->swapchain_resize = _renoir_gl450_swapchain_resize;
	api->swapchain_present = _renoir_gl450_swapchain_present;

	api->buffer_new = _renoir_gl450_buffer_new;
	api->buffer_free = _renoir_gl450_buffer_free;
	api->buffer_size = _renoir_gl450_buffer_size;

	api->texture_new = _renoir_gl450_texture_new;
	api->texture_free = _renoir_gl450_texture_free;
	api->texture_native_handle = _renoir_gl450_texture_native_handle;
	api->texture_size = _renoir_gl450_texture_size;
	api->texture_desc = _renoir_gl450_texture_desc;

	api->program_new = _renoir_gl450_program_new;
	api->program_free = _renoir_gl450_program_free;

	api->compute_new = _renoir_gl450_compute_new;
	api->compute_free = _renoir_gl450_compute_free;

	api->pass_swapchain_new = _renoir_gl450_pass_swapchain_new;
	api->pass_offscreen_new = _renoir_gl450_pass_offscreen_new;
	api->pass_compute_new = _renoir_gl450_pass_compute_new;
	api->pass_free = _renoir_gl450_pass_free;
	api->pass_size = _renoir_gl450_pass_size;
	api->pass_offscreen_desc = _renoir_gl450_pass_offscreen_desc;

	api->timer_new = _renoir_gl450_timer_new;
	api->timer_free = _renoir_gl450_timer_free;
	api->timer_elapsed = _renoir_gl450_timer_elapsed;

	api->pass_submit = _renoir_gl450_pass_submit;
	api->clear = _renoir_gl450_clear;
	api->use_pipeline = _renoir_gl450_use_pipeline;
	api->use_program = _renoir_gl450_use_program;
	api->use_compute = _renoir_gl450_use_compute;
	api->scissor = _renoir_gl450_scissor;
	api->buffer_zero = _renoir_gl450_buffer_zero;
	api->buffer_write = _renoir_gl450_buffer_write;
	api->texture_write = _renoir_gl450_texture_write;
	api->buffer_read = _renoir_gl450_buffer_read;
	api->texture_read = _renoir_gl450_texture_read;
	api->buffer_bind = _renoir_gl450_buffer_bind;
	api->buffer_storage_bind = _renoir_gl450_buffer_storage_bind;
	api->texture_bind = _renoir_gl450_texture_bind;
	api->texture_sampler_bind = _renoir_gl450_texture_sampler_bind;
	api->buffer_compute_bind = _renoir_gl450_buffer_compute_bind;
	api->texture_compute_bind = _renoir_gl450_texture_compute_bind;
	api->draw = _renoir_gl450_draw;
	api->dispatch = _renoir_gl450_dispatch;
	api->timer_begin = _renoir_gl450_timer_begin;
	api->timer_end = _renoir_gl450_timer_end;
}

Renoir*
renoir_api()
{
	static Renoir _api;
	_renoir_load_api(&_api);
	return &_api;
}

extern "C" RENOIR_GL450_EXPORT void*
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
		renoir_gl450_context_reload(self->ctx->ctx);
		return api;
	}
	else if (api != nullptr && reload == false)
	{
		mn::free((Renoir*)api);
		return nullptr;
	}
	return nullptr;
}

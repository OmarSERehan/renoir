#pragma once

#include <stddef.h>
#include <stdint.h>

#if __cplusplus
extern "C" {
#endif

typedef enum RENOIR_CONSTANT {
	RENOIR_CONSTANT_DEFAULT_SAMPLER_CACHE_SIZE = 32,
	RENOIR_CONSTANT_DRAW_VERTEX_BUFFER_SIZE = 10,
	RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE = 4,
	RENOIR_CONSTANT_BUFFER_STORAGE_SIZE = 8,
	RENOIR_CONSTANT_DEFAULT_PIPELINE_CACHE_SIZE = 64
} RENOIR_CONSTANT;

// Enums
typedef enum RENOIR_CLEAR {
	RENOIR_CLEAR_COLOR = 1 << 0,
	RENOIR_CLEAR_DEPTH = 1 << 1
} RENOIR_CLEAR;

typedef enum RENOIR_SHADER {
	RENOIR_SHADER_NONE,
	RENOIR_SHADER_VERTEX,
	RENOIR_SHADER_PIXEL,
	RENOIR_SHADER_GEOMETRY,
	RENOIR_SHADER_COMPUTE
} RENOIR_SHADER;

typedef enum RENOIR_BUFFER {
	RENOIR_BUFFER_NONE,
	RENOIR_BUFFER_VERTEX,
	RENOIR_BUFFER_INDEX,
	RENOIR_BUFFER_UNIFORM,
	// TODO(Moustapha): rename this to storage buffer
	RENOIR_BUFFER_COMPUTE
} RENOIR_BUFFER;

typedef enum RENOIR_USAGE {
	RENOIR_USAGE_NONE,
	RENOIR_USAGE_STATIC,
	RENOIR_USAGE_DYNAMIC
} RENOIR_USAGE;

typedef enum RENOIR_ACCESS {
	RENOIR_ACCESS_NONE,
	RENOIR_ACCESS_READ,
	RENOIR_ACCESS_WRITE,
	RENOIR_ACCESS_READ_WRITE
} RENOIR_ACCESS;

typedef enum RENOIR_PIXELFORMAT {
	RENOIR_PIXELFORMAT_NONE,
	RENOIR_PIXELFORMAT_RGBA8,
	RENOIR_PIXELFORMAT_R16I,
	RENOIR_PIXELFORMAT_R16UI,
	RENOIR_PIXELFORMAT_R16F,
	RENOIR_PIXELFORMAT_R32F,
	RENOIR_PIXELFORMAT_R16G16B16A16F,
	RENOIR_PIXELFORMAT_R32G32F,
	RENOIR_PIXELFORMAT_R32G32B32A32F,
	RENOIR_PIXELFORMAT_D24S8,
	RENOIR_PIXELFORMAT_D32,
	RENOIR_PIXELFORMAT_R8
} RENOIR_PIXELFORMAT;

typedef enum RENOIR_TYPE {
	RENOIR_TYPE_NONE,
	RENOIR_TYPE_UINT8,
	RENOIR_TYPE_UINT8_4,
	RENOIR_TYPE_UINT8_4N,
	RENOIR_TYPE_UINT16,
	RENOIR_TYPE_UINT32,
	RENOIR_TYPE_INT16,
	RENOIR_TYPE_INT32,
	RENOIR_TYPE_FLOAT,
	RENOIR_TYPE_FLOAT_2,
	RENOIR_TYPE_FLOAT_3,
	RENOIR_TYPE_FLOAT_4
} RENOIR_TYPE;

typedef enum RENOIR_FACE {
	RENOIR_FACE_NONE,
	RENOIR_FACE_BACK,
	RENOIR_FACE_FRONT,
	RENOIR_FACE_FRONT_BACK
} RENOIR_FACE;

typedef enum RENOIR_BLEND {
	RENOIR_BLEND_NONE,
	RENOIR_BLEND_ZERO,
	RENOIR_BLEND_ONE,
	RENOIR_BLEND_SRC_COLOR,
	RENOIR_BLEND_ONE_MINUS_SRC_COLOR,
	RENOIR_BLEND_DST_COLOR,
	RENOIR_BLEND_ONE_MINUS_DST_COLOR,
	RENOIR_BLEND_SRC_ALPHA,
	RENOIR_BLEND_ONE_MINUS_SRC_ALPHA
} RENOIR_BLEND;

typedef enum RENOIR_BLEND_EQ {
	RENOIR_BLEND_EQ_NONE,
	RENOIR_BLEND_EQ_ADD,
	RENOIR_BLEND_EQ_SUBTRACT,
	RENOIR_BLEND_EQ_MIN,
	RENOIR_BLEND_EQ_MAX
} RENOIR_BLEND_EQ;

typedef enum RENOIR_ORIENTATION {
	RENOIR_ORIENTATION_NONE,
	RENOIR_ORIENTATION_CCW,
	RENOIR_ORIENTATION_CW
} RENOIR_ORIENTATION;

typedef enum RENOIR_SEMANTIC {
	RENOIR_SEMANTIC_NONE,
	RENOIR_SEMANTIC_POSITION,
	RENOIR_SEMANTIC_POSITION0 = RENOIR_SEMANTIC_POSITION,
	RENOIR_SEMANTIC_POSITION1,
	RENOIR_SEMANTIC_COLOR,
	RENOIR_SEMANTIC_TEXCOORD,
	RENOIR_SEMANTIC_NORMAL,
	RENOIR_SEMANTIC_PSIZE
} RENOIR_SEMANTIC;

typedef enum RENOIR_COMPARE {
	RENOIR_COMPARE_NEVER,
	RENOIR_COMPARE_LESS,
	RENOIR_COMPARE_EQUAL,
	RENOIR_COMPARE_LESS_EQUAL,
	RENOIR_COMPARE_GREATER,
	RENOIR_COMPARE_NOT_EQUAL,
	RENOIR_COMPARE_GREATER_EQUAL,
	RENOIR_COMPARE_ALWAYS
} RENOIR_COMPARE;

typedef enum RENOIR_TEXMODE {
	RENOIR_TEXMODE_WRAP,
	RENOIR_TEXMODE_CLAMP,
	RENOIR_TEXMODE_BORDER,
	RENOIR_TEXMODE_MIRROR
} RENOIR_TEXMODE;

typedef enum RENOIR_FILTER {
	RENOIR_FILTER_LINEAR,
	RENOIR_FILTER_POINT
} RENOIR_FILTER;

typedef enum RENOIR_PRIMITIVE {
	RENOIR_PRIMITIVE_TRIANGLES,
	RENOIR_PRIMITIVE_POINTS,
	RENOIR_PRIMITIVE_LINES
} RENOIR_PRIMITIVE;

typedef enum RENOIR_SWITCH {
	RENOIR_SWITCH_DEFAULT,
	RENOIR_SWITCH_ENABLE,
	RENOIR_SWITCH_DISABLE
} RENOIR_ACTIVE;

typedef enum RENOIR_MSAA_MODE {
	RENOIR_MSAA_MODE_NONE = 0,
	RENOIR_MSAA_MODE_2 = 2,
	RENOIR_MSAA_MODE_4 = 4,
	RENOIR_MSAA_MODE_8 = 8
} RENOIR_MSAA_MODE;

typedef enum RENOIR_VSYNC_MODE {
	RENOIR_VSYNC_MODE_ON,
	RENOIR_VSYNC_MODE_OFF
} RENOIR_VSYNC_MODE;

typedef enum RENOIR_TEXTURE_ORIGIN {
	RENOIR_TEXTURE_ORIGIN_TOP_LEFT,
	RENOIR_TEXTURE_ORIGIN_BOTTOM_LEFT
} RENOIR_TEXTURE_ORIGIN;

typedef enum RENOIR_CUBE_FACE
{
	RENOIR_CUBE_FACE_POS_X,
	RENOIR_CUBE_FACE_NEG_X,
	RENOIR_CUBE_FACE_POS_Y,
	RENOIR_CUBE_FACE_NEG_Y,
	RENOIR_CUBE_FACE_POS_Z,
	RENOIR_CUBE_FACE_NEG_Z
} RENOIR_CUBE_FACE;

typedef enum RENOIR_COLOR_MASK
{
	RENOIR_COLOR_MASK_NONE = -1,
	RENOIR_COLOR_MASK_DEFAULT = 0,
	RENOIR_COLOR_MASK_RED = 1 << 0,
	RENOIR_COLOR_MASK_GREEN = 1 << 1,
	RENOIR_COLOR_MASK_BLUE = 1 << 2,
	RENOIR_COLOR_MASK_ALPHA = 1 << 3,
	RENOIR_COLOR_MASK_ALL = RENOIR_COLOR_MASK_RED |
							RENOIR_COLOR_MASK_GREEN |
							RENOIR_COLOR_MASK_BLUE |
							RENOIR_COLOR_MASK_ALPHA,
} RENOIR_COLOR_MASK;

// Handles
typedef struct Renoir_Buffer { void* handle; } Renoir_Buffer;
typedef struct Renoir_Texture { void* handle; } Renoir_Texture;
typedef struct Renoir_Program { void* handle; } Renoir_Program;
typedef struct Renoir_Compute { void* handle; } Renoir_Compute;
typedef struct Renoir_Pass { void* handle; } Renoir_Pass;
typedef struct Renoir_Swapchain { void* handle; } Renoir_Swapchain;
typedef struct Renoir_Timer { void* handle; } Renoir_Timer;


// Descriptons
// it's recommended to memset all the description structs before using them
// Desc desc;
// memset(&desc, 0, sizeof(desc));
// desc.field_of_interest = value;
typedef struct Renoir_Size {
	int width, height, depth;
} Renoir_Size;

typedef struct Renoir_Color {
	float r, g, b, a;
} Renoir_Color;

typedef struct Renoir_Settings {
	bool defer_api_calls; // default: false
	bool external_context; // default: false
	RENOIR_MSAA_MODE msaa; // default: RENOIR_MSAA_MODE_NONE
	RENOIR_VSYNC_MODE vsync; // default: RENOIR_VSYNC_MODE_ON
	int sampler_cache_size; // default: RENOIR_CONSTANT_DEFAULT_SAMPLER_CACHE_SIZE
	int pipeline_cache_size; // default: RENOIR_CONSTANT_DEFAULT_PIPELINE_CACHE_SIZE
} Renoir_Settings;

typedef struct Renoir_Depth_Desc {
	RENOIR_SWITCH depth; // default: RENOIR_SWITCH_ENABLE
	RENOIR_SWITCH depth_write_mask; // default: RENOIR_SWITCH_ENABLE
} Renoir_Depth_Desc;

typedef struct Renoir_Rasterizer_Desc {
	RENOIR_SWITCH cull; // default: RENOIR_SWITCH_ENABLE
	RENOIR_FACE cull_face; // default: RENOIR_FACE_BACK
	RENOIR_ORIENTATION cull_front; // default: RENOIR_ORIENTATION_CCW
	RENOIR_SWITCH scissor; // default: RENOIR_SWITCH_DISABLE
} Renoir_Rasterizer_Desc;

typedef struct Renoir_Blend_Desc {
	RENOIR_SWITCH enabled; // default: RENOIR_SWITCH_ENABLE
	RENOIR_BLEND src_rgb; // default: RENOIR_BLEND_SRC_ALPHA
	RENOIR_BLEND dst_rgb; // default: RENOIR_BLEND_ONE_MINUS_SRC_ALPHA
	RENOIR_BLEND src_alpha; // default: RENOIR_BLEND_ONE
	RENOIR_BLEND dst_alpha; // default: RENOIR_BLEND_ONE_MINUS_SRC_ALPHA
	RENOIR_BLEND_EQ eq_rgb; // default: RENOIR_BLEND_EQ_ADD
	RENOIR_BLEND_EQ eq_alpha; // default: RENOIR_BLEND_EQ_ADD
	int color_mask; // default: RENOIR_COLOR_MASK_ALL
} Renoir_Blend_Desc;

typedef struct Renoir_Pipeline_Desc {
	Renoir_Rasterizer_Desc rasterizer;
	Renoir_Depth_Desc depth_stencil;
	RENOIR_SWITCH independent_blend; // default: RENOIR_SWITCH_DISABLE
	Renoir_Blend_Desc blend[RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE];
} Pipeline_Desc;

typedef struct Renoir_Buffer_Desc {
	RENOIR_BUFFER type;
	RENOIR_USAGE usage; // default: RENOIR_USAGE_STATIC
	RENOIR_ACCESS access; // default: RENOIR_ACCESS_NONE
	void* data; // you can pass null here to only allocate buffer without initializing it
	size_t data_size;
	size_t compute_buffer_stride;
} Renoir_Buffer_Desc;

typedef struct Renoir_Sampler_Desc {
	RENOIR_FILTER filter; // default: RENOIR_FILTER_LINEAR
	RENOIR_TEXMODE u; // default: RENOIR_TEXMODE_WRAP
	RENOIR_TEXMODE v; // default: RENOIR_TEXMODE_WRAP
	RENOIR_TEXMODE w; // default: RENOIR_TEXMODE_WRAP
	RENOIR_COMPARE compare; // default: RENOIR_COMPARE_NEVER
	Renoir_Color border; // default: black
} Renoir_Sampler_Desc;

typedef struct Renoir_Texture_Desc {
	Renoir_Size size; // you can only fill what you need (1D = width, 2D = width, and height, 3D = width, height, and depth)
	RENOIR_USAGE usage; // default: RENOIR_USAGE_STATIC
	RENOIR_ACCESS access; // default: RENOIR_ACCESS_NONE
	RENOIR_PIXELFORMAT pixel_format;
	int mipmaps; // default: 0, if > 0 will generate this number of mipmaps level for the texture
	// by default use data[0], in case of cube map index the array with RENOIR_CUBE_FACE and set data pointers accordingly
	void* data[6]; // you can pass null here to only allocate texture without initializing it
	size_t data_size;
	// render target
	bool render_target; // default: false
	RENOIR_MSAA_MODE msaa; // default: RENOIR_MSAA_MODE_NONE
	// cube map
	bool cube_map; // default: false, should be true in case of a cube map texture
	Renoir_Sampler_Desc sampler; // default: see sampler default
} Renoir_Texture_Desc;

typedef struct Renoir_Shader_Blob {
	const char* bytes;
	size_t size; // you can set size = 0 it will assume it's a null terminating string and will calc its strlen
} Renoir_Shader_Blob;

typedef struct Renoir_Program_Desc {
	Renoir_Shader_Blob vertex;
	Renoir_Shader_Blob pixel;
	Renoir_Shader_Blob geometry;
} Renoir_Program_Desc;

typedef struct Renoir_Compute_Desc {
	Renoir_Shader_Blob compute;
} Renoir_Compute_Desc;

typedef struct Renoir_Clear_Desc {
	RENOIR_CLEAR flags; // default: RENOIR_CLEAR_COLOR
	RENOIR_SWITCH independent_clear_color; // default: RENOIR_SWITCH_DISABLE
	Renoir_Color color[RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE];
	float depth;
	uint8_t stencil;
} Renoir_Clear_Desc;

typedef struct Renoir_Vertex_Desc {
	Renoir_Buffer buffer;
	RENOIR_TYPE type;
	size_t stride;
	size_t offset;
} Renoir_Vertex_Desc;

typedef struct Renoir_Draw_Desc {
	RENOIR_PRIMITIVE primitive; // default: RENOIR_PRIMITIVE_TRIANGLES
	int base_element;
	int elements_count;
	int instances_count;
	Renoir_Vertex_Desc vertex_buffers[RENOIR_CONSTANT_DRAW_VERTEX_BUFFER_SIZE];
	Renoir_Buffer index_buffer;
	RENOIR_TYPE index_type; // default: RENOIR_TYPE_UINT16
} Renoir_Draw_Desc;

typedef struct Renoir_Texture_Edit_Desc {
	int x, y, z;
	int width, height, depth;
	void* bytes;
	size_t bytes_size;
} Renoir_Texture_Read_Desc;

typedef struct Renoir_Pass_Attachment {
	Renoir_Texture texture;
	// this is used for cube maps and it should hold face index (RENOIR_CUBE_FACE), otherwise it should be 0
	int subresource;
	// this is used to choose which mip map level you want to be attached to the pass
	int level;
} Renoir_Pass_Attachment;

typedef struct Renoir_Pass_Offscreen_Desc {
	Renoir_Pass_Attachment color[RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE];
	Renoir_Pass_Attachment depth_stencil;
} Renoir_Pass_Offscreen_Desc;

typedef struct Renoir_Buffer_Storage_Bind_Desc {
	Renoir_Buffer buffers[RENOIR_CONSTANT_BUFFER_STORAGE_SIZE];
	int start_slot;
} Renoir_Buffer_Storage_Bind_Desc;

typedef struct Renoir_Info {
	const char* description; // description of the gpu renoir is currently using
	size_t gpu_memory_in_bytes; // gpu memory size, in case we can't get memory size we set it to 0
} Renoir_Info;

struct IRenoir;

typedef struct Renoir
{
	struct IRenoir* ctx;

	bool (*init)(struct Renoir* self, Renoir_Settings settings, void* display);
	void (*dispose)(struct Renoir* self);

	const char* (*name)();
	RENOIR_TEXTURE_ORIGIN (*texture_origin)();
	Renoir_Info (*info)(struct Renoir* api);

	void (*handle_ref)(struct Renoir* api, void* handle);
	void (*flush)(struct Renoir* api, void* device, void* context);

	Renoir_Swapchain (*swapchain_new)(struct Renoir* api, int width, int height, void* window, void* display);
	void (*swapchain_free)(struct Renoir* api, Renoir_Swapchain view);
	void (*swapchain_resize)(struct Renoir* api, Renoir_Swapchain view, int width, int height);
	void (*swapchain_present)(struct Renoir* api, Renoir_Swapchain view);

	Renoir_Buffer (*buffer_new)(struct Renoir* api, Renoir_Buffer_Desc desc);
	void (*buffer_free)(struct Renoir* api, Renoir_Buffer buffer);
	size_t (*buffer_size)(struct Renoir* api, Renoir_Buffer buffer);

	Renoir_Texture (*texture_new)(struct Renoir* api, Renoir_Texture_Desc desc);
	void (*texture_free)(struct Renoir* api, Renoir_Texture texture);
	void* (*texture_native_handle)(struct Renoir* api, Renoir_Texture texture);
	Renoir_Size (*texture_size)(struct Renoir* api, Renoir_Texture texture);
	Renoir_Texture_Desc (*texture_desc)(struct Renoir* api, Renoir_Texture texture);

	Renoir_Program (*program_new)(struct Renoir* api, Renoir_Program_Desc desc);
	void (*program_free)(struct Renoir* api, Renoir_Program program);

	Renoir_Compute (*compute_new)(struct Renoir* api, Renoir_Compute_Desc desc);
	void (*compute_free)(struct Renoir* api, Renoir_Compute compute);

	Renoir_Pass (*pass_swapchain_new)(struct Renoir* api, Renoir_Swapchain view);
	Renoir_Pass (*pass_offscreen_new)(struct Renoir* api, Renoir_Pass_Offscreen_Desc desc);
	Renoir_Pass (*pass_compute_new)(struct Renoir* api);
	void (*pass_free)(struct Renoir* api, Renoir_Pass pass);
	Renoir_Size (*pass_size)(struct Renoir* api, Renoir_Pass pass);
	Renoir_Pass_Offscreen_Desc (*pass_offscreen_desc)(struct Renoir* api, Renoir_Pass pass);

	Renoir_Timer (*timer_new)(struct Renoir* api);
	void (*timer_free)(struct Renoir* api, Renoir_Timer timer);
	bool (*timer_elapsed)(struct Renoir* api, Renoir_Timer timer, uint64_t* elapsed_time_in_nanos);

	// Graphics Commands
	void (*pass_begin)(struct Renoir* api, Renoir_Pass pass);
	void (*pass_end)(struct Renoir* api, Renoir_Pass pass);
	void (*clear)(struct Renoir* api, Renoir_Pass pass, Renoir_Clear_Desc desc);
	void (*use_pipeline)(struct Renoir* api, Renoir_Pass pass, Renoir_Pipeline_Desc pipeline);
	void (*use_program)(struct Renoir* api, Renoir_Pass pass, Renoir_Program program);
	void (*use_compute)(struct Renoir* api, Renoir_Pass pass, Renoir_Compute compute);
	void (*scissor)(struct Renoir* api, Renoir_Pass pass, int x, int y, int width, int height);
	// Write Functions
	// TODO(Moustapha): consider allowing the user to provide clear value other than zero
	void (*buffer_zero)(struct Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer);
	void (*buffer_write)(struct Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size);
	void (*texture_write)(struct Renoir* api, Renoir_Pass pass, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc);
	// queues a buffer zero command in the global command list (without a pass)
	void (*buffer_zero_global)(struct Renoir* api, Renoir_Buffer buffer);
	// queues a buffer write command in the global command list (without a pass)
	void (*buffer_write_global)(struct Renoir* api, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size);
	// queues a texture write command in the global command list (without a pass)
	void (*texture_write_global)(struct Renoir* api, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc);
	// Read Functions
	void (*buffer_read)(struct Renoir* api, Renoir_Buffer buffer, size_t offset, void* bytes, size_t bytes_size);
	void (*texture_read)(struct Renoir* api, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc);
	// Bind Functions
	void (*buffer_bind)(struct Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, RENOIR_SHADER shader, int slot);
	// TODO(Moustapha): consider making buffer_bind work like buffer_storage_bind, which means providing all the bindings
	// at once, if you do that then there will be no need for separate buffer_storage_bind function
	void (*buffer_storage_bind)(struct Renoir* api, Renoir_Pass pass, Renoir_Buffer_Storage_Bind_Desc desc);
	void (*texture_bind)(struct Renoir* api, Renoir_Pass pass, Renoir_Texture texture, RENOIR_SHADER shader, int slot);
	void (*texture_sampler_bind)(struct Renoir* api, Renoir_Pass pass, Renoir_Texture texture, RENOIR_SHADER shader, int slot, Renoir_Sampler_Desc sampler);
	// Compute Bind Functions
	void (*buffer_compute_bind)(struct Renoir* api, Renoir_Pass pass, Renoir_Buffer buffer, int slot, RENOIR_ACCESS gpu_access);
	void (*texture_compute_bind)(struct Renoir* api, Renoir_Pass pass, Renoir_Texture texture, int slot, int mip_level, RENOIR_ACCESS gpu_access);
	// Draw
	void (*draw)(struct Renoir* api, Renoir_Pass pass, Renoir_Draw_Desc desc);
	// Dispatch
	void (*dispatch)(struct Renoir* api, Renoir_Pass pass, int x, int y, int z);
	// Timer
	void (*timer_begin)(struct Renoir* api, Renoir_Pass pass, Renoir_Timer timer);
	void (*timer_end)(struct Renoir* api, Renoir_Pass pass, Renoir_Timer timer);
} Renoir;

#define RENOIR_API "renoir"

#if __cplusplus
}
#endif

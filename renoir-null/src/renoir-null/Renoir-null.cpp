#include "renoir-null/Exports.h"

#include <renoir/Renoir.h>

#include <mn/Thread.h>
#include <mn/Pool.h>
#include <mn/Buf.h>
#include <mn/Defer.h>
#include <mn/Log.h>
#include <mn/Map.h>
#include <mn/Str.h>
#include <mn/Debug.h>
#include <mn/Assert.h>

#include <atomic>
#include <math.h>
#include <stdio.h>

struct Renoir_Handle;

struct Renoir_Command;

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
		} swapchain;

		struct
		{
			Renoir_Command *command_list_head;
			Renoir_Command *command_list_tail;
			// used when rendering is done on screen/window
			Renoir_Handle* swapchain;
			// used when rendering is done off screen
			int width, height;
			Renoir_Pass_Offscreen_Desc offscreen;
		} raster_pass;

		struct
		{
			Renoir_Command *command_list_head;
			Renoir_Command *command_list_tail;
		} compute_pass;

		struct
		{
			RENOIR_BUFFER type;
			RENOIR_USAGE usage;
			RENOIR_ACCESS access;
			size_t size;
		} buffer;

		struct
		{
			Renoir_Texture_Desc desc;
		} texture;

		struct
		{
			Renoir_Sampler_Desc desc;
		} sampler;

		struct
		{
		} program;

		struct
		{
		} compute;

		struct
		{
			Renoir_Pipeline_Desc desc;
			Renoir_Handle* program;
		} pipeline;

		struct
		{
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
_renoir_null_pipeline_desc_defaults(Renoir_Pipeline_Desc* desc)
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
	RENOIR_COMMAND_KIND_SWAPCHAIN_FREE,
	RENOIR_COMMAND_KIND_SWAPCHAIN_RESIZE,
	RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW,
	RENOIR_COMMAND_KIND_PASS_COMPUTE_NEW,
	RENOIR_COMMAND_KIND_PIPELINE_NEW,
	RENOIR_COMMAND_KIND_PIPELINE_FREE,
	RENOIR_COMMAND_KIND_PASS_FREE,
	RENOIR_COMMAND_KIND_BUFFER_FREE,
	RENOIR_COMMAND_KIND_TEXTURE_FREE,
	RENOIR_COMMAND_KIND_SAMPLER_FREE,
	RENOIR_COMMAND_KIND_PROGRAM_FREE,
	RENOIR_COMMAND_KIND_COMPUTE_FREE,
	RENOIR_COMMAND_KIND_TIMER_FREE,
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
	mn::Pool handle_pool;
	mn::Pool command_pool;
	Renoir_Settings settings;

	mn::Str info_description;
	size_t gpu_memory_in_bytes;

	// global command list
	Renoir_Command *command_list_head;
	Renoir_Command *command_list_tail;

	// caches
	mn::Buf<Renoir_Handle*> sampler_cache;

	// leak detection
	mn::Map<Renoir_Handle*, Renoir_Leak_Info> alive_handles;
};

static void
_renoir_null_command_execute(IRenoir* self, Renoir_Command* command);

static Renoir_Handle*
_renoir_null_handle_new(IRenoir* self, RENOIR_HANDLE_KIND kind)
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
_renoir_null_handle_free(IRenoir* self, Renoir_Handle* h)
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
_renoir_null_handle_ref(Renoir_Handle* h)
{
	h->rc.fetch_add(1);
	return h;
}

static bool
_renoir_null_handle_unref(Renoir_Handle* h)
{
	return h->rc.fetch_sub(1) == 1;
}

template<typename T>
static Renoir_Command*
_renoir_null_command_new(T* self, RENOIR_COMMAND_KIND kind)
{
	auto command = (Renoir_Command*)mn::pool_get(self->command_pool);
	memset(command, 0, sizeof(*command));
	command->kind = kind;
	return command;
}

template<typename T>
static void
_renoir_null_command_free(T* self, Renoir_Command* command)
{
	switch(command->kind)
	{
	case RENOIR_COMMAND_KIND_NONE:
	case RENOIR_COMMAND_KIND_SWAPCHAIN_FREE:
	case RENOIR_COMMAND_KIND_SWAPCHAIN_RESIZE:
	case RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW:
	case RENOIR_COMMAND_KIND_PASS_COMPUTE_NEW:
	case RENOIR_COMMAND_KIND_PASS_FREE:
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
	case RENOIR_COMMAND_KIND_PIPELINE_FREE:
	case RENOIR_COMMAND_KIND_TIMER_FREE:
	default:
		// do nothing
		break;
	}
	mn::pool_put(self->command_pool, command);
}

template<typename T>
static void
_renoir_null_command_push_back(T* self, Renoir_Command* command)
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
_renoir_null_command_push_front(T* self, Renoir_Command* command)
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
_renoir_null_command_process(IRenoir* self, Renoir_Command* command)
{
	if (self->settings.defer_api_calls)
	{
		_renoir_null_command_push_back(self, command);
	}
	else
	{
		_renoir_null_command_execute(self, command);
		_renoir_null_command_free(self, command);
	}
}

static void
_renoir_null_command_execute(IRenoir* self, Renoir_Command* command)
{
	switch(command->kind)
	{
	case RENOIR_COMMAND_KIND_SWAPCHAIN_FREE:
	{
		auto h = command->swapchain_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_SWAPCHAIN_RESIZE:
	{
		auto h = command->swapchain_resize.handle;
		h->swapchain.width = command->swapchain_resize.width;
		h->swapchain.height = command->swapchain_resize.height;
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW:
	{
		auto h = command->pass_offscreen_new.handle;
		auto &desc = command->pass_offscreen_new.desc;
		h->raster_pass.offscreen = desc;

		for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
		{
			auto color = (Renoir_Handle*)desc.color[i].texture.handle;
			if (color == nullptr)
				continue;
			mn_assert(color->texture.desc.render_target);

			_renoir_null_handle_ref(color);
		}

		auto depth = (Renoir_Handle*)desc.depth_stencil.texture.handle;
		if (depth)
		{
			mn_assert(depth->texture.desc.render_target);
			_renoir_null_handle_ref(depth);
		}
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_FREE:
	{
		auto h = command->pass_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;

		if (h->kind == RENOIR_HANDLE_KIND_RASTER_PASS)
		{
			for(auto it = h->raster_pass.command_list_head; it != NULL; it = it->next)
				_renoir_null_command_free(self, command);

			// free all the bound textures if it's a framebuffer pass
			if (h->raster_pass.swapchain == nullptr)
			{
				for (size_t i = 0; i < RENOIR_CONSTANT_COLOR_ATTACHMENT_SIZE; ++i)
				{
					auto color = (Renoir_Handle*)h->raster_pass.offscreen.color[i].texture.handle;
					if (color == nullptr)
						continue;

					// issue command to free the color texture
					auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = color;
					_renoir_null_command_execute(self, command);
					_renoir_null_command_free(self, command);
				}

				auto depth = (Renoir_Handle*)h->raster_pass.offscreen.depth_stencil.texture.handle;
				if (depth)
				{
					// issue command to free the depth texture
					auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = depth;
					_renoir_null_command_execute(self, command);
					_renoir_null_command_free(self, command);
				}
			}
		}
		else if (h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS)
		{
			for(auto it = h->compute_pass.command_list_head; it != NULL; it = it->next)
				_renoir_null_command_free(self, command);
		}

		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	{
		auto h = command->buffer_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	{
		auto h = command->texture_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	{
		auto h = command->sampler_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	{
		auto h = command->program_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
	{
		auto h = command->compute_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PIPELINE_NEW:
	{
		auto h = command->pipeline_new.handle;
		_renoir_null_handle_ref(h->pipeline.program);
		break;
	}
	case RENOIR_COMMAND_KIND_PIPELINE_FREE:
	{
		auto h = command->pipeline_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;

		// issue command to free the program
		auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_PROGRAM_FREE);
		command->program_free.handle = h->pipeline.program;
		_renoir_null_command_process(self, command);

		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_FREE:
	{
		auto h = command->timer_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	default:
		mn_unreachable();
		break;
	}
}

inline static void
_renoir_null_handle_leak_free(IRenoir* self, Renoir_Command* command)
{
	switch(command->kind)
	{
	case RENOIR_COMMAND_KIND_SWAPCHAIN_FREE:
	{
		auto h = command->swapchain_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PASS_FREE:
	{
		auto h = command->pass_free.handle;
		if (_renoir_null_handle_unref(h) == false)
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
					auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = color;
					_renoir_null_handle_leak_free(self, command);
				}

				auto depth = (Renoir_Handle*)h->raster_pass.offscreen.depth_stencil.texture.handle;
				if (depth)
				{
					// issue command to free the depth texture
					auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
					command->texture_free.handle = depth;
					_renoir_null_handle_leak_free(self, command);
				}
			}
		}
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_BUFFER_FREE:
	{
		auto h = command->buffer_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TEXTURE_FREE:
	{
		auto h = command->texture_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_SAMPLER_FREE:
	{
		auto h = command->sampler_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PROGRAM_FREE:
	{
		auto h = command->program_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_COMPUTE_FREE:
	{
		auto h = command->compute_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_PIPELINE_FREE:
	{
		auto h = command->pipeline_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;

		// issue command to free the program
		auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_PROGRAM_FREE);
		command->program_free.handle = h->pipeline.program;
		_renoir_null_handle_leak_free(self, command);

		_renoir_null_handle_free(self, h);
		break;
	}
	case RENOIR_COMMAND_KIND_TIMER_FREE:
	{
		auto h = command->timer_free.handle;
		if (_renoir_null_handle_unref(h) == false)
			break;
		_renoir_null_handle_free(self, h);
		break;
	}
	}
}

// API
static bool
_renoir_null_init(Renoir* api, Renoir_Settings settings, void*)
{
	static_assert(RENOIR_CONSTANT_DEFAULT_SAMPLER_CACHE_SIZE > 0, "sampler cache size should be > 0");

	if (settings.sampler_cache_size <= 0)
		settings.sampler_cache_size = RENOIR_CONSTANT_DEFAULT_SAMPLER_CACHE_SIZE;

	auto self = mn::alloc_zerod<IRenoir>();
	self->mtx = mn_mutex_new_with_srcloc("renoir null");
	self->handle_pool = mn::pool_new(sizeof(Renoir_Handle), 128);
	self->command_pool = mn::pool_new(sizeof(Renoir_Command), 128);
	self->settings = settings;
	self->info_description = mn::str_new();
	self->sampler_cache = mn::buf_new<Renoir_Handle*>();
	self->alive_handles = mn::map_new<Renoir_Handle*, Renoir_Leak_Info>();
	mn::buf_resize_fill(self->sampler_cache, self->settings.sampler_cache_size, nullptr);

	api->ctx = self;
	return true;
}

static void
_renoir_null_dispose(Renoir* api)
{
	auto self = api->ctx;

	// process these commands for frees to give correct leak report
	for (auto it = self->command_list_head; it != nullptr; it = it->next)
		_renoir_null_handle_leak_free(self, it);
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
	mn::pool_free(self->handle_pool);
	mn::pool_free(self->command_pool);
	mn::str_free(self->info_description);
	mn::buf_free(self->sampler_cache);
	mn::map_free(self->alive_handles);
	mn::free(self);
}

static const char*
_renoir_null_name()
{
	return "null";
}

static RENOIR_TEXTURE_ORIGIN
_renoir_null_texture_origin()
{
	return RENOIR_TEXTURE_ORIGIN_TOP_LEFT;
}

static Renoir_Info
_renoir_null_info(Renoir* api)
{
	auto self = api->ctx;

	Renoir_Info res{};
	res.description = self->info_description.ptr;
	res.gpu_memory_in_bytes = self->gpu_memory_in_bytes;
	return res;
}

static void
_renoir_null_handle_ref(Renoir* api, void* handle)
{
	auto h = (Renoir_Handle*)handle;
	h->rc.fetch_add(1);
}

static void
_renoir_null_flush(Renoir* api, void* device, void* context)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	// process commands
	for(auto it = self->command_list_head; it != nullptr; it = it->next)
	{
		_renoir_null_command_execute(self, it);
		_renoir_null_command_free(self, it);
	}

	self->command_list_head = nullptr;
	self->command_list_tail = nullptr;
}

static Renoir_Swapchain
_renoir_null_swapchain_new(Renoir* api, int width, int height, void* window, void*)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_SWAPCHAIN);
	h->swapchain.width = width;
	h->swapchain.height = height;
	h->swapchain.window = window;

	return Renoir_Swapchain{h};
}

static void
_renoir_null_swapchain_free(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_SWAPCHAIN_FREE);
	command->swapchain_free.handle = h;
	_renoir_null_command_process(self, command);
}

static void
_renoir_null_swapchain_resize(Renoir* api, Renoir_Swapchain swapchain, int width, int height)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_SWAPCHAIN_RESIZE);
	command->swapchain_resize.handle = h;
	command->swapchain_resize.width = width;
	command->swapchain_resize.height = height;
	_renoir_null_command_process(self, command);
}

static void
_renoir_null_swapchain_present(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)swapchain.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	// process commands
	for(auto it = self->command_list_head; it != nullptr; it = it->next)
	{
		_renoir_null_command_execute(self, it);
		_renoir_null_command_free(self, it);
	}

	self->command_list_head = nullptr;
	self->command_list_tail = nullptr;
}

static Renoir_Buffer
_renoir_null_buffer_new(Renoir* api, Renoir_Buffer_Desc desc)
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
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_BUFFER);
	h->buffer.type = desc.type;
	h->buffer.usage = desc.usage;
	h->buffer.access = desc.access;
	h->buffer.size = desc.data_size;

	return Renoir_Buffer{h};
}

static void
_renoir_null_buffer_free(Renoir* api, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};
	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_BUFFER_FREE);
	command->buffer_free.handle = h;
	_renoir_null_command_process(self, command);
}

static size_t
_renoir_null_buffer_size(Renoir* api, Renoir_Buffer buffer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr && h->kind == RENOIR_HANDLE_KIND_BUFFER);

	return h->buffer.size;
}

static Renoir_Texture
_renoir_null_texture_new(Renoir* api, Renoir_Texture_Desc desc)
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
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_TEXTURE);
	h->texture.desc = desc;
	::memset(h->texture.desc.data, 0, sizeof(h->texture.desc.data));
	h->texture.desc.data_size = 0;
	return Renoir_Texture{h};
}

static void
_renoir_null_texture_free(Renoir* api, Renoir_Texture texture)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};
	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_TEXTURE_FREE);
	command->texture_free.handle = h;
	_renoir_null_command_process(self, command);
}

static void*
_renoir_null_texture_native_handle(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);
	return nullptr;
}

static Renoir_Size
_renoir_null_texture_size(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr && h->kind == RENOIR_HANDLE_KIND_TEXTURE);
	return h->texture.desc.size;
}

static Renoir_Texture_Desc
_renoir_null_texture_desc(Renoir* api, Renoir_Texture texture)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_TEXTURE);
	return h->texture.desc;
}

static Renoir_Program
_renoir_null_program_new(Renoir* api, Renoir_Program_Desc desc)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_PROGRAM);
	return Renoir_Program{h};
}

static void
_renoir_null_program_free(Renoir* api, Renoir_Program program)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)program.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_PROGRAM_FREE);
	command->program_free.handle = h;
	_renoir_null_command_process(self, command);
}

static Renoir_Compute
_renoir_null_compute_new(Renoir* api, Renoir_Compute_Desc desc)
{
	mn_assert(desc.compute.bytes != nullptr);
	if (desc.compute.size == 0)
		desc.compute.size = ::strlen(desc.compute.bytes);

	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_COMPUTE);
	return Renoir_Compute{h};
}

static void
_renoir_null_compute_free(Renoir* api, Renoir_Compute compute)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)compute.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_COMPUTE_FREE);
	command->compute_free.handle = h;
	_renoir_null_command_process(self, command);
}

static Renoir_Pipeline
_renoir_null_pipeline_new(Renoir* api, Renoir_Pipeline_Desc desc)
{
	auto self = api->ctx;

	_renoir_null_pipeline_desc_defaults(&desc);
	auto h_program = (Renoir_Handle*)desc.program.handle;
	mn_assert(h_program);
	mn_assert(h_program->kind == RENOIR_HANDLE_KIND_PROGRAM);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_PIPELINE);
	h->pipeline.desc = desc;
	h->pipeline.program = h_program;

	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_PIPELINE_NEW);
	command->pipeline_new.handle = h;
	_renoir_null_command_process(self, command);

	return Renoir_Pipeline{h};
}

static void
_renoir_null_pipeline_free(Renoir* api, Renoir_Pipeline pipeline)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)pipeline.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_PIPELINE);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_PIPELINE_FREE);
	command->pipeline_free.handle = h;
	_renoir_null_command_process(self, command);
}

static Renoir_Pass
_renoir_null_pass_swapchain_new(Renoir* api, Renoir_Swapchain swapchain)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_RASTER_PASS);
	h->raster_pass.swapchain = (Renoir_Handle*)swapchain.handle;
	return Renoir_Pass{h};
}

static Renoir_Pass
_renoir_null_pass_offscreen_new(Renoir* api, Renoir_Pass_Offscreen_Desc desc)
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
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_RASTER_PASS);
	h->raster_pass.offscreen = desc;
	h->raster_pass.width = width;
	h->raster_pass.height = height;

	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_PASS_OFFSCREEN_NEW);
	command->pass_offscreen_new.handle = h;
	command->pass_offscreen_new.desc = desc;
	_renoir_null_command_process(self, command);
	return Renoir_Pass{h};
}

static Renoir_Pass
_renoir_null_pass_compute_new(Renoir* api)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_COMPUTE_PASS);
	return Renoir_Pass{h};
}

static void
_renoir_null_pass_free(Renoir* api, Renoir_Pass pass)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_PASS_FREE);
	command->pass_free.handle = h;
	_renoir_null_command_process(self, command);
}

static Renoir_Size
_renoir_null_pass_size(Renoir* api, Renoir_Pass pass)
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
_renoir_null_pass_offscreen_desc(Renoir* api, Renoir_Pass pass)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
	return h->raster_pass.offscreen;
}

static Renoir_Timer
_renoir_null_timer_new(Renoir* api)
{
	auto self = api->ctx;

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto h = _renoir_null_handle_new(self, RENOIR_HANDLE_KIND_TIMER);
	return Renoir_Timer{h};
}

static void
_renoir_null_timer_free(struct Renoir* api, Renoir_Timer timer)
{
	auto self = api->ctx;
	auto h = (Renoir_Handle*)timer.handle;
	mn_assert(h != nullptr);

	mn::mutex_lock(self->mtx);
	mn_defer{mn::mutex_unlock(self->mtx);};

	auto command = _renoir_null_command_new(self, RENOIR_COMMAND_KIND_TIMER_FREE);
	command->timer_free.handle = h;
	_renoir_null_command_process(self, command);
}

static bool
_renoir_null_timer_elapsed(Renoir*, Renoir_Timer timer, uint64_t*)
{
	auto h = (Renoir_Handle*)timer.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_TIMER);
	return false;
}

// Graphics Commands
static void
_renoir_null_pass_submit(Renoir*, Renoir_Pass pass)
{
	auto h = (Renoir_Handle*)pass.handle;
	if (h != nullptr)
	{
		mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS ||
			   h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
	}
}

static void
_renoir_null_clear(Renoir*, Renoir_Pass pass, Renoir_Clear_Desc)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
}

static void
_renoir_null_use_pipeline(Renoir*, Renoir_Pass pass, Renoir_Pipeline pipeline)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);

	auto h_pipeline = (Renoir_Handle*)pipeline.handle;
	mn_assert(h_pipeline->kind == RENOIR_HANDLE_KIND_PIPELINE);
}

static void
_renoir_null_use_compute(Renoir*, Renoir_Pass pass, Renoir_Compute)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
}

static void
_renoir_null_scissor(Renoir*, Renoir_Pass pass, int, int, int, int)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
}

static void
_renoir_null_buffer_zero(Renoir*, Renoir_Pass pass, Renoir_Buffer buffer)
{
	auto h = (Renoir_Handle*)pass.handle;
	if (h != nullptr)
	{
		mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS ||
			   h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
	}

	auto hbuffer = (Renoir_Handle*)buffer.handle;
	mn_assert(hbuffer != nullptr);

	mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);
}

static void
_renoir_null_buffer_write(Renoir*, Renoir_Pass pass, Renoir_Buffer buffer, size_t, void*, size_t)
{
	auto h = (Renoir_Handle*)pass.handle;
	if (h != nullptr)
	{
		mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS ||
			   h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
	}

	auto hbuffer = (Renoir_Handle*)buffer.handle;
	mn_assert(hbuffer != nullptr);

	mn_assert(hbuffer->buffer.usage != RENOIR_USAGE_STATIC);
}

static void
_renoir_null_texture_write(Renoir*, Renoir_Pass pass, Renoir_Texture texture, Renoir_Texture_Edit_Desc)
{
	auto h = (Renoir_Handle*)pass.handle;
	if (h != nullptr)
	{
		mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS ||
			   h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
	}

	auto htexture = (Renoir_Handle*)texture.handle;
	mn_assert(htexture->texture.desc.usage != RENOIR_USAGE_STATIC);
}

static void
_renoir_null_buffer_read(Renoir*, Renoir_Buffer buffer, size_t, void* bytes, size_t bytes_size)
{
	auto h = (Renoir_Handle*)buffer.handle;
	mn_assert(h != nullptr);
	::memset(bytes, 0, bytes_size);
}

static void
_renoir_null_texture_read(Renoir*, Renoir_Texture texture, Renoir_Texture_Edit_Desc desc)
{
	auto h = (Renoir_Handle*)texture.handle;
	mn_assert(h != nullptr);

	::memset(desc.bytes, 0, desc.bytes_size);
}

static void
_renoir_null_buffer_bind(Renoir*, Renoir_Pass pass, Renoir_Buffer, RENOIR_SHADER, int)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
}

static void
_renoir_null_buffer_storage_bind(Renoir*, Renoir_Pass pass, Renoir_Buffer_Storage_Bind_Desc)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
}

static void
_renoir_null_texture_bind(Renoir*, Renoir_Pass pass, Renoir_Texture texture, RENOIR_SHADER, int)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htex = (Renoir_Handle*)texture.handle;
	mn_assert(htex != nullptr);
}

static void
_renoir_null_texture_sampler_bind(Renoir*, Renoir_Pass pass, Renoir_Texture texture, RENOIR_SHADER, int, Renoir_Sampler_Desc )
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	auto htex = (Renoir_Handle*)texture.handle;
	mn_assert(htex != nullptr);
}

static void
_renoir_null_buffer_compute_bind(Renoir*, Renoir_Pass pass, Renoir_Buffer, int, RENOIR_ACCESS gpu_access)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
	mn_assert_msg(
		gpu_access != RENOIR_ACCESS_NONE,
		"gpu should read, write, or both, it has no meaning to bind a buffer that the GPU cannot read or write from"
	);
}

static void
_renoir_null_texture_compute_bind(Renoir*, Renoir_Pass pass, Renoir_Texture, int, int mip_level, RENOIR_ACCESS gpu_access)
{
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

}

static void
_renoir_null_draw(Renoir*, Renoir_Pass pass, Renoir_Draw_Desc)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS);
}

static void
_renoir_null_dispatch(Renoir*, Renoir_Pass pass, int x, int y, int z)
{
	mn_assert(x >= 0 && y >= 0 && z >= 0);

	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);

	mn_assert(h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);
}

static void
_renoir_null_timer_begin(Renoir*, Renoir_Pass pass, Renoir_Timer timer)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS ||
		   h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);

	auto htimer = (Renoir_Handle*)timer.handle;
	mn_assert(htimer != nullptr && htimer->kind == RENOIR_HANDLE_KIND_TIMER);
}

static void
_renoir_null_timer_end(Renoir*, Renoir_Pass pass, Renoir_Timer timer)
{
	auto h = (Renoir_Handle*)pass.handle;
	mn_assert(h != nullptr);
	mn_assert(h->kind == RENOIR_HANDLE_KIND_RASTER_PASS ||
		   h->kind == RENOIR_HANDLE_KIND_COMPUTE_PASS);

	auto htimer = (Renoir_Handle*)timer.handle;
	mn_assert(htimer != nullptr && htimer->kind == RENOIR_HANDLE_KIND_TIMER);
}


inline static void
_renoir_load_api(Renoir* api)
{
	api->global_pass = Renoir_Pass{};

	api->init = _renoir_null_init;
	api->dispose = _renoir_null_dispose;

	api->name = _renoir_null_name;
	api->texture_origin = _renoir_null_texture_origin;
	api->info = _renoir_null_info;

	api->handle_ref = _renoir_null_handle_ref;
	api->flush = _renoir_null_flush;

	api->swapchain_new = _renoir_null_swapchain_new;
	api->swapchain_free = _renoir_null_swapchain_free;
	api->swapchain_resize = _renoir_null_swapchain_resize;
	api->swapchain_present = _renoir_null_swapchain_present;

	api->buffer_new = _renoir_null_buffer_new;
	api->buffer_free = _renoir_null_buffer_free;
	api->buffer_size = _renoir_null_buffer_size;

	api->texture_new = _renoir_null_texture_new;
	api->texture_free = _renoir_null_texture_free;
	api->texture_native_handle = _renoir_null_texture_native_handle;
	api->texture_size = _renoir_null_texture_size;
	api->texture_desc = _renoir_null_texture_desc;

	api->program_new = _renoir_null_program_new;
	api->program_free = _renoir_null_program_free;

	api->compute_new = _renoir_null_compute_new;
	api->compute_free = _renoir_null_compute_free;

	api->pipeline_new = _renoir_null_pipeline_new;
	api->pipeline_free = _renoir_null_pipeline_free;

	api->pass_swapchain_new = _renoir_null_pass_swapchain_new;
	api->pass_offscreen_new = _renoir_null_pass_offscreen_new;
	api->pass_compute_new = _renoir_null_pass_compute_new;
	api->pass_free = _renoir_null_pass_free;
	api->pass_size = _renoir_null_pass_size;
	api->pass_offscreen_desc = _renoir_null_pass_offscreen_desc;

	api->timer_new = _renoir_null_timer_new;
	api->timer_free = _renoir_null_timer_free;
	api->timer_elapsed = _renoir_null_timer_elapsed;

	api->pass_submit = _renoir_null_pass_submit;
	api->clear = _renoir_null_clear;
	api->use_pipeline = _renoir_null_use_pipeline;
	api->use_compute = _renoir_null_use_compute;
	api->scissor = _renoir_null_scissor;
	api->buffer_zero = _renoir_null_buffer_zero;
	api->buffer_write = _renoir_null_buffer_write;
	api->texture_write = _renoir_null_texture_write;
	api->buffer_read = _renoir_null_buffer_read;
	api->texture_read = _renoir_null_texture_read;
	api->buffer_bind = _renoir_null_buffer_bind;
	api->buffer_storage_bind = _renoir_null_buffer_storage_bind;
	api->texture_bind = _renoir_null_texture_bind;
	api->texture_sampler_bind = _renoir_null_texture_sampler_bind;
	api->texture_compute_bind = _renoir_null_texture_compute_bind;
	api->buffer_compute_bind = _renoir_null_buffer_compute_bind;
	api->draw = _renoir_null_draw;
	api->dispatch = _renoir_null_dispatch;
	api->timer_begin = _renoir_null_timer_begin;
	api->timer_end = _renoir_null_timer_end;
}

extern "C" Renoir*
renoir_null_api()
{
	static Renoir _api;
	_renoir_load_api(&_api);
	return &_api;
}

extern "C" RENOIR_NULL_EXPORT void*
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

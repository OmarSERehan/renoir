#pragma once

#include "renoir/Renoir.h"

#include <atomic>

#include <GL/glew.h>

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
			void* handle;
			void* display;
			void* hdc;
		} swapchain;

		struct
		{
			Renoir_Command *command_list_head;
			Renoir_Command *command_list_tail;
			// used when rendering is done on screen/window
			Renoir_Handle* swapchain;
			// used when rendering is done off screen
			GLuint fb;
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
			GLuint id;
			RENOIR_BUFFER type;
			RENOIR_USAGE usage;
			RENOIR_ACCESS access;
			size_t size;
		} buffer;

		struct
		{
			GLuint id;
			GLuint render_buffer[6];
			Renoir_Texture_Desc desc;
		} texture;

		struct
		{
			GLuint id;
			Renoir_Sampler_Desc desc;
		} sampler;

		struct
		{
			GLuint id;
		} program;

		struct
		{
			GLuint id;
		} compute;

		struct
		{
			Renoir_Pipeline_Desc desc;
			Renoir_Handle* program;
		} pipeline;

		struct
		{
			GLuint timepoints[2];
			uint64_t elapsed_time_in_nanos;
			RENOIR_TIMER_STATE state;
		} timer;
	};
};

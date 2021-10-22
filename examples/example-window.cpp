#include <stdio.h>
#include <renoir-window/Window.h>

#if RENOIR_EXAMPLES_USES_NULL
#include <renoir-null/Renoir-null.h>
#elif RENOIR_EXAMPLES_USES_GL450
#include <renoir-gl450/Renoir-gl450.h>
#elif RENOIR_EXAMPLES_USES_MTL
#include <renoir-mtl/Renoir-mtl.h>
#endif

#include <assert.h>

const char *glsl_vertex_shader = R"""(
#version 450 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec3 color;

out vec3 v_color;

void main()
{
	gl_Position = vec4(pos, 0.0, 1.0);
	v_color = color;
}
)""";

const char *glsl_pixel_shader = R"""(
#version 450 core

in vec3 v_color;

out vec4 out_color;

void main()
{
	out_color = vec4(v_color, 1.0);
}
)""";

const char *mtl_vertex_shader = R"""(
#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct VS_Input
{
	float3 position [[attribute(0)]];
	float3 color [[attribute(1)]];
};
struct PS_Input
{
	float4 position [[position]];
	float3 color [[user(locn0)]];
};
vertex PS_Input vertex_main(VS_Input input [[stage_in]])
{
	PS_Input output = {};
	output.position = float4(input.position, 1.0f);
	output.color = input.color;
	return output;
}
)""";

const char *mtl_pixel_shader = R"""(
#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct PS_Input
{
	float3 color [[user(locn0)]];
};
struct PS_Output
{
	float4 color [[color(0)]];
};
fragment PS_Output fragment_main(PS_Input input [[stage_in]])
{
	PS_Output output = {};
	output.color = float4(input.color, 1.0f);
	return output;
}
)""";

#if RENOIR_EXAMPLES_USES_NULL
const char* vertex_shader = "";
const char* pixel_shader = "";
#elif RENOIR_EXAMPLES_USES_GL450
const char* vertex_shader = glsl_vertex_shader;
const char* pixel_shader = glsl_pixel_shader;
#elif RENOIR_EXAMPLES_USES_MTL
const char* vertex_shader = mtl_vertex_shader;
const char* pixel_shader = mtl_pixel_shader;
#endif

int main()
{
	auto gfx = renoir_api();

	Renoir_Settings settings{};
	settings.defer_api_calls = false;

	Renoir_Window* window = renoir_window_new(800, 600, "Mostafa", (RENOIR_WINDOW_MSAA_MODE)settings.msaa);

	void *handle, *display;
	renoir_window_native_handles(window, &handle, &display);

	bool ok = gfx->init(gfx, settings, display);
	assert(ok && "gfx init failed");

	auto swapchain = gfx->swapchain_new(gfx, 800, 600, handle, display);

	Renoir_Program_Desc program_desc{};
	program_desc.vertex.bytes = vertex_shader;
	program_desc.pixel.bytes = pixel_shader;
	Renoir_Program program = gfx->program_new(gfx, program_desc);

	Renoir_Pipeline_Desc pipeline{};

	float triangle_data[] = {
		 -1, -1,
		  1,  0,  0,

		  1, -1,
		  0,  1,  0,

		  0,  1,
		  0,  0,  1,
	};
	Renoir_Buffer_Desc vertices_desc{};
	vertices_desc.type = RENOIR_BUFFER_VERTEX;
	vertices_desc.data = triangle_data;
	vertices_desc.data_size = sizeof(triangle_data);
	Renoir_Buffer vertices = gfx->buffer_new(gfx, vertices_desc);

	uint16_t triangle_indices[] = {
		0, 1, 2
	};
	Renoir_Buffer_Desc indices_desc{};
	indices_desc.type = RENOIR_BUFFER_INDEX;
	indices_desc.data =  triangle_indices;
	indices_desc.data_size = sizeof(triangle_indices);
	Renoir_Buffer indices = gfx->buffer_new(gfx, indices_desc);

	Renoir_Pass pass = gfx->pass_swapchain_new(gfx, swapchain);

	while(true)
	{
		Renoir_Event event = renoir_window_poll(window);
		if(event.kind == RENOIR_EVENT_KIND_WINDOW_CLOSE)
			break;

		if (event.kind == RENOIR_EVENT_KIND_MOUSE_MOVE)
		{
			printf("position: %d, %d\n", event.mouse_move.x, event.mouse_move.y);
		}
		else if(event.kind == RENOIR_EVENT_KIND_MOUSE_WHEEL)
		{
			printf("wheel: %f\n", event.wheel);
		}
		else if(event.kind == RENOIR_EVENT_KIND_WINDOW_RESIZE)
		{
			printf("resize: %d %d\n", event.resize.width, event.resize.height);
			gfx->swapchain_resize(gfx, swapchain, event.resize.width, event.resize.height);
		}

		Renoir_Clear_Desc clear{};
		clear.flags = RENOIR_CLEAR(RENOIR_CLEAR_COLOR|RENOIR_CLEAR_DEPTH);
		clear.color[0] = {0.0f, 0.0f, 0.0f, 1.0f};
		clear.depth = 1.0f;
		clear.stencil = 0;
		gfx->clear(gfx, pass, clear);

		gfx->use_pipeline(gfx, pass, pipeline);
		gfx->use_program(gfx, pass, program);

		Renoir_Draw_Desc draw{};
		draw.primitive = RENOIR_PRIMITIVE_TRIANGLES;
		draw.elements_count = 3;
		// position
		draw.vertex_buffers[0].buffer = vertices;
		draw.vertex_buffers[0].type = RENOIR_TYPE_FLOAT_2;
		draw.vertex_buffers[0].stride = 5 * sizeof(float);

		draw.vertex_buffers[1].buffer = vertices;
		draw.vertex_buffers[1].type = RENOIR_TYPE_FLOAT_3;
		draw.vertex_buffers[1].stride = 5 * sizeof(float);
		draw.vertex_buffers[1].offset = 8;

		draw.index_buffer = indices;
		draw.index_type = RENOIR_TYPE_UINT16;
		gfx->draw(gfx, pass, draw);

		gfx->pass_submit(gfx, pass);
		gfx->swapchain_present(gfx, swapchain);
	}

	gfx->program_free(gfx, program);
	gfx->buffer_free(gfx, vertices);
	gfx->buffer_free(gfx, indices);
	gfx->swapchain_free(gfx, swapchain);
	gfx->pass_free(gfx, pass);
	gfx->dispose(gfx);

	renoir_window_free(window);
	return 0;
}

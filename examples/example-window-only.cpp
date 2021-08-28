#include <mn/IO.h>

#include <renoir-window/Window.h>

int
main()
{
	auto window = renoir_window_new(800, 600, "Mostafa", RENOIR_WINDOW_MSAA_MODE_NONE);

	while (true)
	{
		auto e = renoir_window_poll(window);
		if (e.kind == RENOIR_EVENT_KIND_WINDOW_CLOSE)
			break;

		switch (e.kind)
		{
		case RENOIR_EVENT_KIND_KEYBOARD_KEY:
			mn::print("key: {}, state: {}\n", e.keyboard.key, e.keyboard.state);
			break;
		case RENOIR_EVENT_KIND_MOUSE_MOVE:
			mn::print("x: {}, y: {}\n", e.mouse_move.x, e.mouse_move.y);
			break;
		case RENOIR_EVENT_KIND_MOUSE_BUTTON:
			mn::print("button: {}, state: {}\n", e.mouse.button, e.mouse.state);
			break;
		case RENOIR_EVENT_KIND_WINDOW_RESIZE:
			mn::print("resize, width: {}, height: {}\n", e.resize.width, e.resize.height);
			break;
		case RENOIR_EVENT_KIND_MOUSE_WHEEL:
			mn::print("wheel: {}\n", e.wheel);
			break;
		case RENOIR_EVENT_KIND_RUNE:
		{
			char buffer[5] = {0};
			mn::rune_encode(e.rune, {buffer, sizeof(buffer)});
			mn::print("rune: {}, {}\n", e.rune, buffer);
			break;
		}
		default:
			break;
		}
	}
	renoir_window_free(window);
	return 0;
}
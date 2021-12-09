#include "renoir-window/Window.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>

#include <mn/Memory.h>
#include <mn/Defer.h>

#include <string.h>
#include <assert.h>

struct Renoir_Window_Linux
{
	Renoir_Window window;
	::Window handle;
	::Display* display;
	Renoir_Event event;
	bool running;
	// old mouse position
	int old_x, old_y;
};

inline static int
_renoir_window_msaa_to_int(RENOIR_WINDOW_MSAA_MODE mode)
{
	int res = 0;
	switch(mode)
	{
	case RENOIR_WINDOW_MSAA_MODE_NONE:
		res = 0;
		break;
	case RENOIR_WINDOW_MSAA_MODE_2:
		res = 2;
		break;
	case RENOIR_WINDOW_MSAA_MODE_4:
		res = 4;
		break;
	case RENOIR_WINDOW_MSAA_MODE_8:
		res = 8;
		break;
	default:
		assert(false && "unreachable");
		break;
	}
	return res;
}

Atom*
_wm_delete_window(Display* display)
{
	static Atom atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
	return &atom;
}

inline static RENOIR_KEY
_renoir_map_keyboard_key(KeySym key)
{
	switch (key)
	{
	// letters
	case 'A': return RENOIR_KEY_A;
	case 'B': return RENOIR_KEY_B;
	case 'C': return RENOIR_KEY_C;
	case 'D': return RENOIR_KEY_D;
	case 'E': return RENOIR_KEY_E;
	case 'F': return RENOIR_KEY_F;
	case 'G': return RENOIR_KEY_G;
	case 'H': return RENOIR_KEY_H;
	case 'I': return RENOIR_KEY_I;
	case 'J': return RENOIR_KEY_J;
	case 'K': return RENOIR_KEY_K;
	case 'L': return RENOIR_KEY_L;
	case 'M': return RENOIR_KEY_M;
	case 'N': return RENOIR_KEY_N;
	case 'O': return RENOIR_KEY_O;
	case 'P': return RENOIR_KEY_P;
	case 'Q': return RENOIR_KEY_Q;
	case 'R': return RENOIR_KEY_R;
	case 'S': return RENOIR_KEY_S;
	case 'T': return RENOIR_KEY_T;
	case 'U': return RENOIR_KEY_U;
	case 'V': return RENOIR_KEY_V;
	case 'W': return RENOIR_KEY_W;
	case 'X': return RENOIR_KEY_X;
	case 'Y': return RENOIR_KEY_Y;
	case 'Z': return RENOIR_KEY_Z;

	// numbers
	case '0': return RENOIR_KEY_NUM_0;
	case '1': return RENOIR_KEY_NUM_1;
	case '2': return RENOIR_KEY_NUM_2;
	case '3': return RENOIR_KEY_NUM_3;
	case '4': return RENOIR_KEY_NUM_4;
	case '5': return RENOIR_KEY_NUM_5;
	case '6': return RENOIR_KEY_NUM_6;
	case '7': return RENOIR_KEY_NUM_7;
	case '8': return RENOIR_KEY_NUM_8;
	case '9': return RENOIR_KEY_NUM_9;

	// numbad
	case XK_KP_0: return RENOIR_KEY_NUMPAD_0;
	case XK_KP_1: return RENOIR_KEY_NUMPAD_1;
	case XK_KP_2: return RENOIR_KEY_NUMPAD_2;
	case XK_KP_3: return RENOIR_KEY_NUMPAD_3;
	case XK_KP_4: return RENOIR_KEY_NUMPAD_4;
	case XK_KP_5: return RENOIR_KEY_NUMPAD_5;
	case XK_KP_6: return RENOIR_KEY_NUMPAD_6;
	case XK_KP_7: return RENOIR_KEY_NUMPAD_7;
	case XK_KP_8: return RENOIR_KEY_NUMPAD_8;
	case XK_KP_9: return RENOIR_KEY_NUMPAD_9;

	// symbols
	case XK_slash: return RENOIR_KEY_FORWARD_SLASH;
	case XK_backslash: return RENOIR_KEY_BACKSLASH;
	case XK_bracketleft: return RENOIR_KEY_LEFT_BRACKET;
	case XK_bracketright: return RENOIR_KEY_RIGHT_BRACKET;
	case XK_grave: return RENOIR_KEY_BACKQUOTE;
	case XK_period: return RENOIR_KEY_PERIOD;
	case XK_minus: return RENOIR_KEY_MINUS;
	// case XK_plus: return RENOIR_KEY_PLUS;
	case XK_equal: return RENOIR_KEY_EQUAL;
	case XK_comma: return RENOIR_KEY_COMMA;
	case XK_semicolon: return RENOIR_KEY_SEMICOLON;
	// case XK_colon: return RENOIR_KEY_COLON;
	case XK_space: return RENOIR_KEY_SPACE;

	// special keys
	case XK_BackSpace: return RENOIR_KEY_BACKSPACE;
	case XK_Tab: return RENOIR_KEY_TAB;
	case XK_Return: return RENOIR_KEY_ENTER;
	case XK_Escape: return RENOIR_KEY_ESC;
	case XK_Delete: return RENOIR_KEY_DELETE;
	case XK_Up: return RENOIR_KEY_UP;
	case XK_Down: return RENOIR_KEY_DOWN;
	case XK_Right: return RENOIR_KEY_RIGHT;
	case XK_Left: return RENOIR_KEY_LEFT;
	case XK_Insert: return RENOIR_KEY_INSERT;
	case XK_Home: return RENOIR_KEY_HOME;
	case XK_End: return RENOIR_KEY_END;
	case XK_Page_Up: return RENOIR_KEY_PAGEUP;
	case XK_Page_Down: return RENOIR_KEY_PAGEDOWN;

	// modifiers
	case XK_Shift_L: return RENOIR_KEY_LEFT_SHIFT;
	case XK_Shift_R: return RENOIR_KEY_RIGHT_SHIFT;
	case XK_Control_L: return RENOIR_KEY_LEFT_CTRL;
	case XK_Control_R: return RENOIR_KEY_RIGHT_CTRL;
	case XK_Alt_L: return RENOIR_KEY_LEFT_ALT;
	case XK_Alt_R: return RENOIR_KEY_RIGHT_ALT;

	// function keys
	case XK_F1: return RENOIR_KEY_F1;
	case XK_F2: return RENOIR_KEY_F2;
	case XK_F3: return RENOIR_KEY_F3;
	case XK_F4: return RENOIR_KEY_F4;
	case XK_F5: return RENOIR_KEY_F5;
	case XK_F6: return RENOIR_KEY_F6;
	case XK_F7: return RENOIR_KEY_F7;
	case XK_F8: return RENOIR_KEY_F8;
	case XK_F9: return RENOIR_KEY_F9;
	case XK_F10: return RENOIR_KEY_F10;
	case XK_F11: return RENOIR_KEY_F11;
	case XK_F12: return RENOIR_KEY_F12;

	default: return RENOIR_KEY_COUNT;
	}
}

inline static RENOIR_MOUSE
_renoir_map_mouse_button(int button)
{
	switch(button)
	{
	case Button1: return RENOIR_MOUSE_LEFT;
	case Button2: return RENOIR_MOUSE_MIDDLE;
	case Button3: return RENOIR_MOUSE_RIGHT;
	default: return RENOIR_MOUSE_COUNT;
	}
}

// API
Renoir_Window*
renoir_window_new(int width, int height, const char* title, RENOIR_WINDOW_MSAA_MODE msaa)
{
	assert(width > 0 && height > 0);

	auto display = XOpenDisplay(nullptr);
	if (display == nullptr)
		return nullptr;
	mn_defer(if (display) XCloseDisplay(display));

	const int visual_attribs[] = {
		GLX_X_RENDERABLE        , True,
		GLX_DRAWABLE_TYPE       , GLX_WINDOW_BIT,
		GLX_DOUBLEBUFFER        , True,
		GLX_RENDER_TYPE         , GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE       , GLX_TRUE_COLOR,
		GLX_RED_SIZE            , 8,
		GLX_GREEN_SIZE          , 8,
		GLX_BLUE_SIZE           , 8,
		GLX_ALPHA_SIZE          , 8,
		GLX_DEPTH_SIZE          , 24,
		GLX_STENCIL_SIZE        , 8,
		GLX_SAMPLE_BUFFERS_ARB  , (msaa != RENOIR_WINDOW_MSAA_MODE_NONE) ? True : False,
		GLX_SAMPLES_ARB         , _renoir_window_msaa_to_int(msaa),
		None
	};

	int fbcount = 0;
	auto fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
	if (fbc == nullptr)
		return nullptr;
	mn_defer(XFree(fbc));

	int best_fbc = -1;
	int worst_fbc = -1;
	int best_num_samp = -1;
	int worst_num_samp = 999;
	for(int i = 0; i < fbcount; ++i)
	{
		XVisualInfo* vi = glXGetVisualFromFBConfig(display, fbc[i]);
		mn_defer(XFree(vi));
		if (vi)
		{
			int samp_buf = 0, samples = 0;
			glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
			glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLES, &samples);

			if (best_fbc < 0 || (samp_buf && samples > best_num_samp))
			{
				best_fbc = i;
				best_num_samp = samples;
			}
			if (worst_fbc < 0 || !samp_buf || samples < worst_num_samp)
			{
				worst_fbc = i;
				worst_num_samp = samples;
			}
		}
	}
	auto bestFbc = fbc[best_fbc];

	auto vi = glXGetVisualFromFBConfig(display, bestFbc);
	if (vi == nullptr)
		return nullptr;
	mn_defer(XFree(vi));

	auto color_map = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);

	XSetWindowAttributes swa{};
	swa.colormap = color_map;
	swa.background_pixmap = None;
	swa.border_pixel = BlackPixel(display, vi->screen);
	swa.background_pixel = WhitePixel(display, vi->screen);
	swa.event_mask = StructureNotifyMask | ExposureMask;
	auto handle = XCreateWindow(
		display,
		RootWindow(display, vi->screen),
		0,
		0,
		width,
		height,
		0,
		vi->depth,
		InputOutput,
		vi->visual,
		CWBorderPixel | CWColormap | CWEventMask,
		&swa
	);
	if (handle == None)
		return nullptr;
	mn_defer(if(handle) XDestroyWindow(display, handle));

	XSetWMProtocols(display, handle, _wm_delete_window(display), 1);

	XSelectInput(
		display,
		handle,
		StructureNotifyMask|
		KeyPressMask|
		KeyReleaseMask|
		ButtonPressMask|
		ButtonReleaseMask|
		ExposureMask
	);

	XStoreName(display, handle, title);
	XMapWindow(display, handle);

	auto self = mn::alloc_zerod<Renoir_Window_Linux>();

	self->window.width = width;
	self->window.height = height;
	self->window.title = title;
	self->running = true;
	self->display = display;
	self->handle = handle;

	display = nullptr;
	handle = None;

	return &self->window;
}

void
renoir_window_free(Renoir_Window* window)
{
	Renoir_Window_Linux* self = (Renoir_Window_Linux*)window;

	if (self->handle)
		XDestroyWindow(self->display, self->handle);
	if (self->display)
		XCloseDisplay(self->display);
	mn::free(self);
}

Renoir_Event
renoir_window_poll(Renoir_Window* window)
{
	Renoir_Window_Linux* self = (Renoir_Window_Linux*)window;

	XEvent event{};
	self->event = Renoir_Event{};

	if(XPending(self->display))
	{
		XNextEvent(self->display, &event);
		if(XFilterEvent(&event, None))
			return self->event;

		switch(event.type)
		{
			case ClientMessage:
			{
				if ((Atom)event.xclient.data.l[0] == *_wm_delete_window(self->display))
				{
					self->running = false;
					memset(&self->event, 0, sizeof(self->event));
					self->event.kind = RENOIR_EVENT_KIND_WINDOW_CLOSE;
				}
			}
			break;

			case KeyPress:
			{
				memset(&self->event, 0, sizeof(self->event));
				self->event.kind = RENOIR_EVENT_KIND_KEYBOARD_KEY;
				self->event.keyboard.key = _renoir_map_keyboard_key(XLookupKeysym(&event.xkey, 0));
				self->event.keyboard.state = RENOIR_KEY_STATE_DOWN;
			}
			break;

			case KeyRelease:
			{
				memset(&self->event, 0, sizeof(self->event));
				self->event.kind = RENOIR_EVENT_KIND_KEYBOARD_KEY;
				self->event.keyboard.key = _renoir_map_keyboard_key(XLookupKeysym(&event.xkey, 0));
				self->event.keyboard.state = RENOIR_KEY_STATE_UP;
			}
			break;

			case ButtonPress:
			{
				// Button4 = scroll up
				// Button5 = scroll down
				if (event.xbutton.button == Button4 || event.xbutton.button == Button5)
				{
					memset(&self->event, 0, sizeof(self->event));
					self->event.kind = RENOIR_EVENT_KIND_MOUSE_WHEEL;
					self->event.wheel = event.xbutton.button == Button4 ? -120.0f : 120.0f;
				}
				else
				{
					memset(&self->event, 0, sizeof(self->event));
					self->event.kind = RENOIR_EVENT_KIND_MOUSE_BUTTON;
					self->event.mouse.button = _renoir_map_mouse_button(event.xbutton.button);
					self->event.mouse.state = RENOIR_KEY_STATE_DOWN;
				}
			}
			break;

			case ButtonRelease:
			{
				memset(&self->event, 0, sizeof(self->event));
				self->event.kind = RENOIR_EVENT_KIND_MOUSE_BUTTON;
				self->event.mouse.button = _renoir_map_mouse_button(event.xbutton.button);
				self->event.mouse.state = RENOIR_KEY_STATE_UP;
			}
			break;

			case MotionNotify:
			{

			}
			break;

			case ConfigureNotify:
			{
				if (self->window.width != event.xconfigure.width ||
					self->window.height != event.xconfigure.height)
				{
					memset(&self->event, 0, sizeof(self->event));
					self->event.kind = RENOIR_EVENT_KIND_WINDOW_RESIZE;
					self->event.resize.width = event.xconfigure.width;
					self->event.resize.height = event.xconfigure.height;

					self->window.width = event.xconfigure.width;
					self->window.height = event.xconfigure.height;
				}
			}
			break;

			default:
				break;
		}
	}
	else
	{
		Window root_return, child_return;
		int root_x, root_y, win_x, win_y;
		unsigned int mask_return;
		XQueryPointer(
			self->display,
			self->handle,
			&root_return,
			&child_return,
			&root_x,
			&root_y,
			&win_x,
			&win_y,
			&mask_return
		);

		if (win_x >= 0 && win_x < self->window.width && win_y >= 0 && win_y < self->window.height)
		{
			if (win_x != self->old_x || win_y != self->old_y)
			{
				memset(&self->event, 0, sizeof(self->event));
				self->event.kind = RENOIR_EVENT_KIND_MOUSE_MOVE;
				self->event.mouse_move.x = win_x;
				self->event.mouse_move.y = win_y;
				self->old_x = win_x;
				self->old_y = win_y;
			}
		}
	}


	return self->event;
}

void
renoir_window_native_handles(Renoir_Window* window, void** handle, void** display)
{
	Renoir_Window_Linux* self = (Renoir_Window_Linux*)window;
	if (handle)
		*handle = (void*)self->handle;
	if (display)
		*display = self->display;
}

void
renoir_window_title_set(Renoir_Window* window, const char* title)
{
	Renoir_Window_Linux* self = (Renoir_Window_Linux*)window;
	if (title)
	{
		self->window.title = title;
		XStoreName(self->display, self->handle, self->window.title);
	}
}
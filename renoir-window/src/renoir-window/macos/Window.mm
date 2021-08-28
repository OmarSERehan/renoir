#include "renoir-window/Window.h"

#include <mn/Memory.h>
#include <mn/Rune.h>
#include <mn/Log.h>

#import <Cocoa/Cocoa.h>

@interface CocoaWindow: NSWindow
{
}
@end

@implementation CocoaWindow
@end

@interface CocoaWindowView: NSView
- (BOOL) acceptsFirstResponder;
- (BOOL) isOpaque;
@end

@implementation CocoaWindowView

- (void) _updateContentScale
{
	NSApplication* app = [NSApplication sharedApplication];
	NSWindow* main_window = [NSApp mainWindow];
	NSWindow* layer_window = [self window];
	if (main_window || layer_window)
	{
		CGFloat scale = [(layer_window != nil) ? layer_window : main_window backingScaleFactor];
		CALayer *layer = self.layer;
		if ([layer respondsToSelector: @selector(contentsScale)]) {
			[self.layer setContentsScale: scale];
		}
	}
}

- (void) scaleDidChange:(NSNotification*)n
{
	[self _updateContentScale];
}

- (void) viewDidMoveToWindow
{
	// Retina Display support
	[
		[NSNotificationCenter defaultCenter]
		addObserver: self
		selector: @selector(scaleDidChange:)
		name: @"NSWindowDidChangeBackingPropertiesNotification"
		object: [self window]
	];

	// immediately update scale after the view has been added to a window
	[self _updateContentScale];
}

- (void) removeFromSuperview
{
	[super removeFromSuperview];
	[[NSNotificationCenter defaultCenter] removeObserver: self name: @"NSWindowDidChangeBackingPropertiesNotification" object: [self window]];
}

- (BOOL) acceptsFirstResponder
{
	return YES;
}

- (BOOL) isOpaque
{
	return YES;
}

@end

struct Renoir_Window_Macos
{
	Renoir_Window window;
	CocoaWindow* cocoa_window;
	CocoaWindowView* cocoa_view;
	bool closed;
	bool resized;
	bool has_rune;
	int rune;
};

@interface CocoaWindowDelegate: NSObject<NSWindowDelegate>
- (instancetype)initWithWindow:(Renoir_Window_Macos*)window;
- (void)windowWillClose:(NSNotification *)notification;
@end

@implementation CocoaWindowDelegate

{
	Renoir_Window_Macos* mWindow;
}

- (instancetype)initWithWindow:(Renoir_Window_Macos *)window
{
	self = [super init];
	if (self)
	{
		mWindow = window;
	}
	return self;
}

- (void)windowWillClose:(NSNotification *)notification
{
	mWindow->closed = true;
}

- (void)windowDidResize:(NSNotification *)notification
{
	mWindow->resized = true;
}

@end

// get codes from /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.1.sdk/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/Headers/Events.h
inline static RENOIR_KEY
_macos_translate_key(UInt16 keyCode)
{
	RENOIR_KEY key = RENOIR_KEY_COUNT;
	switch(keyCode)
	{
	case 0x33: key = RENOIR_KEY_BACKSPACE; break;
	case 0x30: key = RENOIR_KEY_TAB; break;
	case 0x24: key = RENOIR_KEY_ENTER; break;
	case 0x35: key = RENOIR_KEY_ESC; break;
	case 0x31: key = RENOIR_KEY_SPACE; break;
	case 0x1B: key = RENOIR_KEY_MINUS; break;
	case 0x2F: key = RENOIR_KEY_PERIOD; break;
	case 0x2B: key = RENOIR_KEY_COMMA; break;
	case 0x1D: key = RENOIR_KEY_NUM_0; break;
	case 0x12: key = RENOIR_KEY_NUM_1; break;
	case 0x13: key = RENOIR_KEY_NUM_2; break;
	case 0x14: key = RENOIR_KEY_NUM_3; break;
	case 0x15: key = RENOIR_KEY_NUM_4; break;
	case 0x17: key = RENOIR_KEY_NUM_5; break;
	case 0x16: key = RENOIR_KEY_NUM_6; break;
	case 0x1A: key = RENOIR_KEY_NUM_7; break;
	case 0x1C: key = RENOIR_KEY_NUM_8; break;
	case 0x19: key = RENOIR_KEY_NUM_9; break;
	case 0x29: key = RENOIR_KEY_SEMICOLON; break;
	case 0x18: key = RENOIR_KEY_EQUAL; break;
	case 0x2C: key = RENOIR_KEY_FORWARD_SLASH; break;
	case 0x21: key = RENOIR_KEY_LEFT_BRACKET; break;
	case 0x1E: key = RENOIR_KEY_RIGHT_BRACKET; break;
	case 0x2A: key = RENOIR_KEY_BACKSLASH; break;
	case 0x27: key = RENOIR_KEY_QUOTE; break;
	case 0x32: key = RENOIR_KEY_BACKQUOTE; break;
	case 0x00: key = RENOIR_KEY_A; break;
	case 0x0B: key = RENOIR_KEY_B; break;
	case 0x08: key = RENOIR_KEY_C; break;
	case 0x02: key = RENOIR_KEY_D; break;
	case 0x0E: key = RENOIR_KEY_E; break;
	case 0x03: key = RENOIR_KEY_F; break;
	case 0x05: key = RENOIR_KEY_G; break;
	case 0x04: key = RENOIR_KEY_H; break;
	case 0x22: key = RENOIR_KEY_I; break;
	case 0x26: key = RENOIR_KEY_J; break;
	case 0x28: key = RENOIR_KEY_K; break;
	case 0x25: key = RENOIR_KEY_L; break;
	case 0x2E: key = RENOIR_KEY_M; break;
	case 0x2D: key = RENOIR_KEY_N; break;
	case 0x1F: key = RENOIR_KEY_O; break;
	case 0x23: key = RENOIR_KEY_P; break;
	case 0x0C: key = RENOIR_KEY_Q; break;
	case 0x0F: key = RENOIR_KEY_R; break;
	case 0x01: key = RENOIR_KEY_S; break;
	case 0x11: key = RENOIR_KEY_T; break;
	case 0x20: key = RENOIR_KEY_U; break;
	case 0x09: key = RENOIR_KEY_V; break;
	case 0x0D: key = RENOIR_KEY_W; break;
	case 0x07: key = RENOIR_KEY_X; break;
	case 0x10: key = RENOIR_KEY_Y; break;
	case 0x06: key = RENOIR_KEY_Z; break;
	case 0x75: key = RENOIR_KEY_DELETE; break;
	case 0x52: key = RENOIR_KEY_NUMPAD_0; break;
	case 0x53: key = RENOIR_KEY_NUMPAD_1; break;
	case 0x54: key = RENOIR_KEY_NUMPAD_2; break;
	case 0x55: key = RENOIR_KEY_NUMPAD_3; break;
	case 0x56: key = RENOIR_KEY_NUMPAD_4; break;
	case 0x57: key = RENOIR_KEY_NUMPAD_5; break;
	case 0x58: key = RENOIR_KEY_NUMPAD_6; break;
	case 0x59: key = RENOIR_KEY_NUMPAD_7; break;
	case 0x5B: key = RENOIR_KEY_NUMPAD_8; break;
	case 0x5C: key = RENOIR_KEY_NUMPAD_9; break;
	case 0x41: key = RENOIR_KEY_NUMPAD_PERIOD; break;
	case 0x4B: key = RENOIR_KEY_NUMPAD_DIVIDE; break;
	case 0x45: key = RENOIR_KEY_NUMPAD_PLUS; break;
	case 0x4E: key = RENOIR_KEY_NUMPAD_MINUS; break;
	case 0x4C: key = RENOIR_KEY_NUMPAD_ENTER; break;
	case 0x51: key = RENOIR_KEY_NUMPAD_EQUALS; break;
	case 0x43: key = RENOIR_KEY_NUMPAD_MULTIPLY; break;
	case 0x7E: key = RENOIR_KEY_UP; break;
	case 0x7D: key = RENOIR_KEY_DOWN; break;
	case 0x7C: key = RENOIR_KEY_RIGHT; break;
	case 0x7B: key = RENOIR_KEY_LEFT; break;
	case 0x72: key = RENOIR_KEY_INSERT; break;
	case 0x73: key = RENOIR_KEY_HOME; break;
	case 0x77: key = RENOIR_KEY_END; break;
	case 0x74: key = RENOIR_KEY_PAGEUP; break;
	case 0x79: key = RENOIR_KEY_PAGEDOWN; break;
	case 0x7A: key = RENOIR_KEY_F1; break;
	case 0x78: key = RENOIR_KEY_F2; break;
	case 0x63: key = RENOIR_KEY_F3; break;
	case 0x76: key = RENOIR_KEY_F4; break;
	case 0x60: key = RENOIR_KEY_F5; break;
	case 0x61: key = RENOIR_KEY_F6; break;
	case 0x62: key = RENOIR_KEY_F7; break;
	case 0x64: key = RENOIR_KEY_F8; break;
	case 0x65: key = RENOIR_KEY_F9; break;
	case 0x6D: key = RENOIR_KEY_F10; break;
	case 0x67: key = RENOIR_KEY_F11; break;
	case 0x6F: key = RENOIR_KEY_F12; break;
	case 0x47: key = RENOIR_KEY_NUM_LOCK; break;
	case 0x39: key = RENOIR_KEY_CAPS_LOCK; break;
	case 0x69: key = RENOIR_KEY_SCROLL_LOCK; break;
	case 0x3C: key = RENOIR_KEY_RIGHT_SHIFT; break;
	case 0x38: key = RENOIR_KEY_LEFT_SHIFT; break;
	case 0x3E: key = RENOIR_KEY_RIGHT_CTRL; break;
	case 0x3B: key = RENOIR_KEY_LEFT_CTRL; break;
	case 0x3A: key = RENOIR_KEY_LEFT_ALT; break;
	case 0x3D: key = RENOIR_KEY_RIGHT_ALT; break;
	case 0x37: key = RENOIR_KEY_LEFT_META; break;
	case 0x36: key = RENOIR_KEY_RIGHT_META; break;
	default: assert(false && "unreachable"); break;
	}
	return key;
}

Renoir_Window*
renoir_window_new(int width, int height, const char* title, RENOIR_WINDOW_MSAA_MODE msaa)
{
	auto app = [NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp activateIgnoringOtherApps:YES];

	auto window_rect = NSMakeRect(100, 100, width, height);
	auto style_mask = NSWindowStyleMaskTitled |
					  NSWindowStyleMaskClosable |
					  NSWindowStyleMaskResizable |
					  NSWindowStyleMaskMiniaturizable;
	CocoaWindow* win = [
		[CocoaWindow alloc]
		initWithContentRect: window_rect
		styleMask: style_mask
		backing: NSBackingStoreBuffered
		defer: NO
	];

	[win setTitle: [NSString stringWithUTF8String: title]];

	window_rect = [win backingAlignedRect: window_rect options: NSAlignAllEdgesOutward];
	CocoaWindowView* view = [[CocoaWindowView alloc] initWithFrame: window_rect];
	[view setHidden:NO];
	[view setNeedsDisplay:YES];
	[view setWantsLayer:YES];

	auto res = mn::alloc_zerod<Renoir_Window_Macos>();
	res->window.width = width;
	res->window.height = height;
	res->window.title = title;
	res->cocoa_window = win;
	res->cocoa_view = view;

	[win setContentView: view];
	[win setDelegate:[[CocoaWindowDelegate alloc] initWithWindow: res]];
	[win makeKeyAndOrderFront: app];

	return &res->window;
}

void
renoir_window_free(Renoir_Window* window)
{
	auto self = (Renoir_Window_Macos*)window;
	if (self)
	{
		[self->cocoa_window release];
		[self->cocoa_view release];
		mn::free(self);
	}
}

Renoir_Event
renoir_window_poll(Renoir_Window* window)
{
	auto self = (Renoir_Window_Macos*)window;

	Renoir_Event res{};
	if (self->closed)
	{
		res.kind = RENOIR_EVENT_KIND_WINDOW_CLOSE;
		return res;
	}
	else if (self->resized)
	{
		self->resized = false;

		res.kind = RENOIR_EVENT_KIND_WINDOW_RESIZE;
		NSSize size = [[self->cocoa_window contentView] frame].size;
		res.resize.width = size.width;
		res.resize.height = size.height;
		return res;
	}
	else if (self->has_rune)
	{
		self->has_rune = false;

		res.kind = RENOIR_EVENT_KIND_RUNE;
		res.rune = self->rune;
		return res;
	}

	auto app = [NSApplication sharedApplication];
	@autoreleasepool
	{
		NSEvent* e = [app nextEventMatchingMask: NSEventMaskAny untilDate: nil inMode: NSDefaultRunLoopMode dequeue: YES];
		switch (e.type)
		{
		case NSEventTypeKeyDown:
		{
			auto key = _macos_translate_key([e keyCode]);
			if (key != RENOIR_KEY_COUNT)
			{
				res.kind = RENOIR_EVENT_KIND_KEYBOARD_KEY;
				res.keyboard.key = key;
				res.keyboard.state = RENOIR_KEY_STATE_DOWN;
			}

			NSString* str = [e characters];
			if ([str length] > 0)
			{
				mn::log_debug("str len: {}, '{}'", [str length], [str UTF8String]);
				self->has_rune = true;
				// TODO(Moustapha): fix lam alef situation later
				self->rune = mn::rune_read([str UTF8String]);
			}
			break;
		}
		case NSEventTypeKeyUp:
		{
			auto key = _macos_translate_key([e keyCode]);
			if (key != RENOIR_KEY_COUNT)
			{
				res.kind = RENOIR_EVENT_KIND_KEYBOARD_KEY;
				res.keyboard.key = key;
				res.keyboard.state = RENOIR_KEY_STATE_UP;
			}
			break;
		}
		case NSEventTypeLeftMouseUp:
			res.kind = RENOIR_EVENT_KIND_MOUSE_BUTTON;
			res.mouse.button = RENOIR_MOUSE_LEFT;
			res.mouse.state = RENOIR_KEY_STATE_UP;
			break;
		case NSEventTypeLeftMouseDown:
			res.kind = RENOIR_EVENT_KIND_MOUSE_BUTTON;
			res.mouse.button = RENOIR_MOUSE_LEFT;
			res.mouse.state = RENOIR_KEY_STATE_DOWN;
			break;
		case NSEventTypeRightMouseUp:
			res.kind = RENOIR_EVENT_KIND_MOUSE_BUTTON;
			res.mouse.button = RENOIR_MOUSE_RIGHT;
			res.mouse.state = RENOIR_KEY_STATE_UP;
			break;
		case NSEventTypeRightMouseDown:
			res.kind = RENOIR_EVENT_KIND_MOUSE_BUTTON;
			res.mouse.button = RENOIR_MOUSE_RIGHT;
			res.mouse.state = RENOIR_KEY_STATE_DOWN;
			break;
		case NSEventTypeOtherMouseUp:
			res.kind = RENOIR_EVENT_KIND_MOUSE_BUTTON;
			res.mouse.button = RENOIR_MOUSE_MIDDLE;
			res.mouse.state = RENOIR_KEY_STATE_UP;
			break;
		case NSEventTypeOtherMouseDown:
			res.kind = RENOIR_EVENT_KIND_MOUSE_BUTTON;
			res.mouse.button = RENOIR_MOUSE_MIDDLE;
			res.mouse.state = RENOIR_KEY_STATE_DOWN;
			break;
		case NSEventTypeMouseMoved:
		{
			// NSPoint point = [self->cocoa_view convertPoint: [e locationInWindow] fromView: nil];
			NSPoint point = [self->cocoa_window mouseLocationOutsideOfEventStream];
			res.kind = RENOIR_EVENT_KIND_MOUSE_MOVE;
			res.mouse_move.x = point.x;
			res.mouse_move.y = self->window.height - point.y;

			if (res.mouse_move.x < 0 ||
				res.mouse_move.y < 0)
			{
				res.kind = RENOIR_EVENT_KIND_NONE;
			}
			break;
		}
		case NSEventTypeScrollWheel:
			res.kind = RENOIR_EVENT_KIND_MOUSE_WHEEL;
			res.wheel = [e deltaY];
			break;
		default:
			break;
		}

		[NSApp sendEvent: e];
		[NSApp updateWindows];
	}
	return res;
}

void
renoir_window_native_handles(Renoir_Window* window, void** handle, void** display)
{
	auto self = (Renoir_Window_Macos*)window;
	if (handle)
		*handle = self->cocoa_window;
	if (display)
		*display = nullptr;
}
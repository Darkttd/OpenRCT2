/*****************************************************************************
* Copyright (c) 2014 Ted John
* OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
*
* This file is part of OpenRCT2.
*
* OpenRCT2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "../addresses.h"
#include "../config.h"
#include "../game.h"
#include "../editor.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../localisation/localisation.h"
#include "../platform/platform.h"
#include "../scenario.h"
#include "../title.h"
#include "../windows/error.h"

#pragma region Widgets

#define WW 340
#define WH 400

enum {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE,
	WIDX_SCROLL
};

// 0x9DE48C
static rct_widget window_loadsave_widgets[] = {
	{ WWT_FRAME,			0,	0,		WW - 1,	0,		WH - 1,		STR_NONE,		STR_NONE },
	{ WWT_CAPTION,			0,	1,		WW - 2,	1,		14,			STR_NONE,		STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	WW-13,	WW - 3,	2,		13,			STR_CLOSE_X,	STR_CLOSE_WINDOW_TIP },
	{ WWT_SCROLL,			0,	4,		WW - 5,	18,		WH - 18,	2,				STR_NONE },
	{ WIDGETS_END }
};

#pragma endregion

#pragma region Events

void window_loadsave_emptysub() { }
static void window_loadsave_close();
static void window_loadsave_mouseup();
static void window_loadsave_update(rct_window *w);
static void window_loadsave_scrollgetsize();
static void window_loadsave_scrollmousedown();
static void window_loadsave_scrollmouseover();
static void window_loadsave_textinput();
static void window_loadsave_tooltip();
static void window_loadsave_paint();
static void window_loadsave_scrollpaint();

static void* window_loadsave_events[] = {
	window_loadsave_close,
	window_loadsave_mouseup,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_update,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_scrollgetsize,
	window_loadsave_scrollmousedown,
	window_loadsave_emptysub,
	window_loadsave_scrollmouseover,
	window_loadsave_textinput,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_tooltip,
	window_loadsave_emptysub,
	window_loadsave_emptysub,
	window_loadsave_emptysub, 
	window_loadsave_paint,
	window_loadsave_scrollpaint
};

#pragma endregion

typedef struct {
	char name[256];
	char path[MAX_PATH];
} loadsave_list_item;

int _listItemsCount = 0;
loadsave_list_item *_listItems = NULL;
char _directory[MAX_PATH];
char _extension[32];
int _loadsaveType;

static void window_loadsave_populate_list(int includeNewItem, const char *directory, const char *extension);
static void window_loadsave_select(rct_window *w, const char *path);

static rct_window *window_overwrite_prompt_open(const char *name, const char *path);

rct_window *window_loadsave_open(int type)
{
	char path[MAX_PATH], *ch;
	int includeNewItem;
	rct_window* w;

	w = window_bring_to_front_by_class(WC_LOADSAVE);
	if (w == NULL) {
		w = window_create_centred(WW, WH, (uint32*)window_loadsave_events, WC_LOADSAVE, WF_STICK_TO_FRONT);
		w->widgets = window_loadsave_widgets;
		w->enabled_widgets = (1 << WIDX_CLOSE);
		window_init_scroll_widgets(w);
		w->colours[0] = 7;
		w->colours[1] = 7;
		w->colours[2] = 7;
	}

	w->no_list_items = 0;
	w->selected_list_item = -1;

	_loadsaveType = type;
	switch (type) {
	case (LOADSAVETYPE_LOAD | LOADSAVETYPE_GAME):
		w->widgets[WIDX_TITLE].image = STR_LOAD_GAME;
		break;
	case (LOADSAVETYPE_SAVE | LOADSAVETYPE_GAME) :
		w->widgets[WIDX_TITLE].image = STR_SAVE_GAME;
		break;
	case (LOADSAVETYPE_LOAD | LOADSAVETYPE_LANDSCAPE) :
		w->widgets[WIDX_TITLE].image = STR_LOAD_LANDSCAPE;
		break;
	case (LOADSAVETYPE_SAVE | LOADSAVETYPE_LANDSCAPE) :
		w->widgets[WIDX_TITLE].image = STR_SAVE_LANDSCAPE;
		break;
	case (LOADSAVETYPE_SAVE | LOADSAVETYPE_SCENARIO) :
		w->widgets[WIDX_TITLE].image = STR_SAVE_SCENARIO;
		break;
	}

	includeNewItem = (type & 1) == LOADSAVETYPE_SAVE;
	switch (type & 6) {
	case LOADSAVETYPE_GAME:
		platform_get_user_directory(path, "save");
		if (!platform_ensure_directory_exists(path)) {
			fprintf(stderr, "Unable to create save directory.");
			window_close(w);
			return NULL;
		}

		window_loadsave_populate_list(includeNewItem, path, ".sv6");
		break;
	case LOADSAVETYPE_LANDSCAPE:
		platform_get_user_directory(path, "landscape");
		if (!platform_ensure_directory_exists(path)) {
			fprintf(stderr, "Unable to create landscapes directory.");
			window_close(w);
			return NULL;
		}

		window_loadsave_populate_list(includeNewItem, path, ".sc6");
		break;
	case LOADSAVETYPE_SCENARIO:
		/*
		Uncomment when user scenarios are separated

		platform_get_user_directory(path, "scenario");
		if (!platform_ensure_directory_exists(path)) {
			fprintf(stderr, "Unable to create scenarios directory.");
			window_close(w);
			return NULL;
		}
		*/

		strcpy(path, RCT2_ADDRESS(RCT2_ADDRESS_SCENARIOS_PATH, char));
		ch = strchr(path, '*');
		if (ch != NULL)
			*ch = 0;

		window_loadsave_populate_list(includeNewItem, path, ".sc6");
		break;
	}
	w->no_list_items = _listItemsCount;
	return w;
}

static void window_loadsave_close()
{
	if (_listItems != NULL) {
		free(_listItems);
		_listItems = NULL;
	}

	window_close_by_class(WC_LOADSAVE_OVERWRITE_PROMPT);
}

static void window_loadsave_mouseup()
{
	short widgetIndex;
	rct_window *w;

	window_widget_get_registers(w, widgetIndex);

	switch (widgetIndex){
	case WIDX_CLOSE:
		window_close(w);
		break;
	}
}

static void window_loadsave_update(rct_window *w)
{

}

static void window_loadsave_scrollgetsize()
{
	rct_window *w;
	int width, height;

	window_get_register(w);

	width = 0;
	height = w->no_list_items * 10;

	window_scrollsize_set_registers(width, height);
}

static void window_loadsave_scrollmousedown()
{
	int selectedItem;
	short x, y, scrollIndex;
	rct_window *w;

	window_scrollmouse_get_registers(w, scrollIndex, x, y);

	selectedItem = y / 10;
	if (selectedItem >= w->no_list_items)
		return;

	// Load or overwrite
	if (_listItems[selectedItem].path[0] == 0) {
		rct_string_id templateStringId = 3165;
		char *templateString;

		templateString = (char*)language_get_string(templateStringId);
		strcpy(templateString, (char*)RCT2_ADDRESS_SCENARIO_NAME);
		window_text_input_open(w, WIDX_SCROLL, STR_NONE, 2710, templateStringId, 0, 64);
	} else {
		if ((_loadsaveType & 1) == LOADSAVETYPE_SAVE)
			window_overwrite_prompt_open(_listItems[selectedItem].name, _listItems[selectedItem].path);
		else
			window_loadsave_select(w, _listItems[selectedItem].path);
	}
}

static void window_loadsave_scrollmouseover()
{
	int selectedItem;
	short x, y, scrollIndex;
	rct_window *w;

	window_scrollmouse_get_registers(w, scrollIndex, x, y);

	selectedItem = y / 10;
	if (selectedItem >= w->no_list_items)
		return;
	
	w->selected_list_item = selectedItem;

	window_invalidate(w);
}

static void window_loadsave_textinput()
{
	rct_window *w;
	short widgetIndex;
	uint8 result;
	char *text, path[MAX_PATH];
	int i, overwrite;

	window_textinput_get_registers(w, widgetIndex, result, text);

	if (!result || text[0] == 0)
		return;

	strncpy(path, _directory, sizeof(path));
	strncat(path, text, sizeof(path));
	strncat(path, _extension, sizeof(path));

	overwrite = 0;
	for (i = 0; i < _listItemsCount; i++) {
		if (_stricmp(_listItems[i].path, path) == 0) {
			overwrite = 1;
			break;
		}
	}

	if (overwrite)
		window_overwrite_prompt_open(text, path);
	else
		window_loadsave_select(w, path);
}

static void window_loadsave_tooltip()
{
	RCT2_GLOBAL(0x013CE952, uint16) = STR_LIST;
}

static void window_loadsave_paint()
{
	rct_window *w;
	rct_drawpixelinfo *dpi;

	window_paint_get_registers(w, dpi);

	window_draw_widgets(w, dpi);
}

static void window_loadsave_scrollpaint()
{
	int i, y;
	rct_window *w;
	rct_drawpixelinfo *dpi;
	rct_string_id stringId, templateStringId = 3165;
	char *templateString;

	window_paint_get_registers(w, dpi);

	gfx_fill_rect(dpi, dpi->x, dpi->y, dpi->x + dpi->width - 1, dpi->y + dpi->height - 1, RCT2_ADDRESS(0x0141FC48,uint8)[w->colours[1] * 8]);
	
	templateString = (char*)language_get_string(templateStringId);
	for (i = 0; i < w->no_list_items; i++) {
		y = i * 10;
		if (y > dpi->y + dpi->height)
			break;
		
		if (y + 10 < dpi->y)
			continue;

		stringId = STR_BLACK_STRING;
		if (i == w->selected_list_item) {
			stringId = STR_WINDOW_COLOUR_2_STRING;
			gfx_fill_rect(dpi, 0, y, 800, y + 9, 0x2000031);
		}

		strcpy(templateString, _listItems[i].name);
		gfx_draw_string_left(dpi, stringId, &templateStringId, 0, 0, y - 1);
	}
}

static void window_loadsave_populate_list(int includeNewItem, const char *directory, const char *extension)
{
	int i, listItemCapacity, fileEnumHandle;
	file_info fileInfo;
	loadsave_list_item *listItem;
	const char *src;
	char *dst, filter[MAX_PATH];

	strncpy(_directory, directory, sizeof(_directory));
	strncpy(_extension, extension, sizeof(_extension));

	strncpy(filter, directory, sizeof(filter));
	strncat(filter, "*", sizeof(filter));
	strncat(filter, extension, sizeof(filter));

	if (_listItems != NULL)
		free(_listItems);

	listItemCapacity = 8;
	_listItems = (loadsave_list_item*)malloc(listItemCapacity * sizeof(loadsave_list_item));
	_listItemsCount = 0;

	if (includeNewItem) {
		listItem = &_listItems[_listItemsCount];
		strncpy(listItem->name, "(new file)", sizeof(listItem->name));
		listItem->path[0] = 0;
		_listItemsCount++;
	}

	fileEnumHandle = platform_enumerate_files_begin(filter);
	while (platform_enumerate_files_next(fileEnumHandle, &fileInfo)) {
		if (listItemCapacity <= _listItemsCount) {
			listItemCapacity *= 2;
			_listItems = realloc(_listItems, listItemCapacity * sizeof(loadsave_list_item));
		}

		listItem = &_listItems[_listItemsCount];
		strncpy(listItem->path, directory, sizeof(listItem->path));
		strncat(listItem->path, fileInfo.path, sizeof(listItem->path));
		
		src = fileInfo.path;
		dst = listItem->name;
		i = 0;
		while (*src != 0 && *src != '.' && i < sizeof(listItem->name) - 1) {
			*dst++ = *src++;
			i++;
		}
		*dst = 0;

		_listItemsCount++;
	}
	platform_enumerate_files_end(fileEnumHandle);
}

static void window_loadsave_select(rct_window *w, const char *path)
{
	switch (_loadsaveType) {
	case (LOADSAVETYPE_LOAD | LOADSAVETYPE_GAME):
		if (game_load_save(path)) {
			window_close(w);
			gfx_invalidate_screen();
			rct2_endupdate();
		} else {
			// 1050, not the best message...
			window_error_open(STR_LOAD_GAME, 1050);
		}
		break;
	case (LOADSAVETYPE_SAVE | LOADSAVETYPE_GAME):
		if (scenario_save((char*)path, gGeneral_config.save_plugin_data ? 1 : 0)) {
			window_close(w);

			game_do_command(0, 1047, 0, -1, GAME_COMMAND_0, 0, 0);
			gfx_invalidate_screen();
		} else {
			window_error_open(STR_SAVE_GAME, 1047);
		}
		break;
	case (LOADSAVETYPE_LOAD | LOADSAVETYPE_LANDSCAPE) :
		editor_load_landscape(path);
		if (1) {
			gfx_invalidate_screen();
			rct2_endupdate();
		} else {
			// 1050, not the best message...
			window_error_open(STR_LOAD_LANDSCAPE, 1050);
		}
		break;
	case (LOADSAVETYPE_SAVE | LOADSAVETYPE_LANDSCAPE):
		if (scenario_save((char*)path, gGeneral_config.save_plugin_data ? 3 : 2)) {
			window_close(w);
			gfx_invalidate_screen();
		} else {
			window_error_open(STR_SAVE_LANDSCAPE, 1049);
		}
		break;
	case (LOADSAVETYPE_SAVE | LOADSAVETYPE_SCENARIO):
		{
			rct_s6_info *s6Info = (rct_s6_info*)0x0141F570;
			int parkFlagsBackup = RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32);
			RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) &= ~PARK_FLAGS_18;
			s6Info->var_000 = 255;
			int success = scenario_save((char*)path, gGeneral_config.save_plugin_data ? 3 : 2);
			RCT2_GLOBAL(RCT2_ADDRESS_PARK_FLAGS, uint32) = parkFlagsBackup;

			if (success) {
				window_close(w);
				title_load();
			} else {
				window_error_open(STR_SAVE_SCENARIO, STR_SCENARIO_SAVE_FAILED);
				s6Info->var_000 = 4;
			}
		}
		break;
	}
}

#pragma region Overwrite prompt

#define OVERWRITE_WW 200
#define OVERWRITE_WH 100

enum {
	WIDX_OVERWRITE_BACKGROUND,
	WIDX_OVERWRITE_TITLE,
	WIDX_OVERWRITE_CLOSE,
	WIDX_OVERWRITE_OVERWRITE,
	WIDX_OVERWRITE_CANCEL
};

static rct_widget window_overwrite_prompt_widgets[] = {
	{ WWT_FRAME,			0, 0,					OVERWRITE_WW - 1,	0,					OVERWRITE_WH - 1,	STR_NONE,				STR_NONE },
	{ WWT_CAPTION,			0, 1,					OVERWRITE_WW - 2,	1,					14,					2709,					STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0, OVERWRITE_WW - 13,	OVERWRITE_WW - 3,	2,					13,					STR_CLOSE_X,			STR_CLOSE_WINDOW_TIP },
	{ WWT_DROPDOWN_BUTTON,	0, 10,					94,					OVERWRITE_WH - 20,	OVERWRITE_WH - 9,	2709,					STR_NONE },
	{ WWT_DROPDOWN_BUTTON,	0, OVERWRITE_WW - 95,	OVERWRITE_WW - 11,	OVERWRITE_WH - 20,	OVERWRITE_WH - 9,	STR_SAVE_PROMPT_CANCEL, STR_NONE },
	{ WIDGETS_END }
};

static void window_overwrite_prompt_emptysub(){}
static void window_overwrite_prompt_mouseup();
static void window_overwrite_prompt_paint();

static void* window_overwrite_prompt_events[] = {
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_mouseup,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_emptysub,
	window_overwrite_prompt_paint,
	window_overwrite_prompt_emptysub
};

static char _window_overwrite_prompt_name[256];
static char _window_overwrite_prompt_path[MAX_PATH];

static rct_window *window_overwrite_prompt_open(const char *name, const char *path)
{
	rct_window *w;

	window_close_by_class(WC_LOADSAVE_OVERWRITE_PROMPT);

	w = window_create_centred(OVERWRITE_WW, OVERWRITE_WH, (uint32*)window_overwrite_prompt_events, WC_LOADSAVE_OVERWRITE_PROMPT, WF_STICK_TO_FRONT);
	w->widgets = window_overwrite_prompt_widgets;
	w->enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_OVERWRITE_CANCEL) | (1 << WIDX_OVERWRITE_OVERWRITE);
	window_init_scroll_widgets(w);
	w->flags |= WF_TRANSPARENT;
	w->colours[0] = 154;

	strncpy(_window_overwrite_prompt_name, name, sizeof(_window_overwrite_prompt_name));
	strncpy(_window_overwrite_prompt_path, path, sizeof(_window_overwrite_prompt_path));

	return w;
}

static void window_overwrite_prompt_mouseup()
{
	short widgetIndex;
	rct_window *w, *loadsaveWindow;

	window_widget_get_registers(w, widgetIndex);

	switch (widgetIndex){
	case WIDX_OVERWRITE_OVERWRITE:
		loadsaveWindow = window_find_by_class(WC_LOADSAVE);
		if (loadsaveWindow != NULL)
			window_loadsave_select(loadsaveWindow, _window_overwrite_prompt_path);
		window_close(w);
		break;
	case WIDX_OVERWRITE_CANCEL:
	case WIDX_OVERWRITE_CLOSE:
		window_close(w);
	}
}

static void window_overwrite_prompt_paint()
{
	rct_window *w;
	rct_drawpixelinfo *dpi;

	window_paint_get_registers(w, dpi);

	window_draw_widgets(w, dpi);

	rct_string_id templateStringId = 3165;
	char *templateString;

	templateString = (char*)language_get_string(templateStringId);
	strcpy(templateString, _window_overwrite_prompt_name);

	int x = w->x + w->width / 2;
	int y = w->y + (w->height / 2) - 3;
	gfx_draw_string_centred_wrapped(dpi, &templateStringId, x, y, w->width - 4, 2708, 0);
}


#pragma endregion
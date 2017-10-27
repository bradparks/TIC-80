﻿// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "machine.h"
#include "tools.h"

#include "wren.h"

static const char TicMachine[] = "_TIC80";

static WrenHandle* game_class;
static WrenHandle* new_handle;
static WrenHandle* update_handle;
static WrenHandle* scanline_handle;
static WrenHandle* call3_handle;

static bool file_loaded = false;

static char const* tic_wren_api = 
"import \"random\" for Random\n"
"class Tic {\n"
"foreign static btn(id)\n"
"foreign static btnp(id, hold, period)\n"
"foreign static mouse()\n"
"foreign static print(text)\n"
"foreign static print(text, x, y)\n"
"foreign static print(text, x, y, color)\n"
"foreign static print(text, x, y, color, fixed)\n"
"foreign static print(text, x, y, color, fixed, scale)\n"
"foreign static font(text)\n"
"foreign static font(text, x, y)\n"
"foreign static font(text, x, y, alpha_color)\n"
"foreign static font(text, x, y, alpha_color, w, h)\n"
"foreign static font(text, x, y, alpha_color, w, h, fixed)\n"
"foreign static font(text, x, y, alpha_color, w, h, fixed, scale)\n"
"static trace(v) { Tic.trace__(v.toString, 15) }\n"
"static trace(v,color) { Tic.trace__(v.toString, color) }\n"
"foreign static trace__(msg, color)\n"
"foreign static spr(id)\n"
"foreign static spr(id, x, y)\n"
"foreign static spr(id, x, y, alpha_color)\n"
"foreign static spr(id, x, y, alpha_color, scale)\n"
"foreign static spr(id, x, y, alpha_color, scale, flip)\n"
"foreign static spr(id, x, y, alpha_color, scale, flip, rotate)\n"
"foreign static spr(id, x, y, alpha_color, scale, flip, rotate, cell_width, cell_height)\n"
"foreign static map(cell_x, cell_y)\n"
"foreign static map(cell_x, cell_y, cell_w, cell_h)\n"
"foreign static map(cell_x, cell_y, cell_w, cell_h, x, y)\n"
"foreign static map(cell_x, cell_y, cell_w, cell_h, x, y, alpha_color)\n"
"foreign static map(cell_x, cell_y, cell_w, cell_h, x, y, alpha_color, scale)\n"
"foreign static map(cell_x, cell_y, cell_w, cell_h, x, y, alpha_color, scale, remap)\n"
"foreign static mset(cell_x, cell_y)\n"
"foreign static mset(cell_x, cell_y, index)\n"
"foreign static mget(cell_x, cell_y)\n"
"foreign static textri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3)\n"
"foreign static textri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, use_map)\n"
"foreign static textri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, use_map, alpha_color)\n"
"foreign static pix(x, y)\n"
"foreign static pix(x, y, color)\n"
"foreign static line(x0, y0, x1, y1, color)\n"
"foreign static circ(x, y, radius, color)\n"
"foreign static circb(x, y, radius, color)\n"
"foreign static rect(x, y, w, h, color)\n"
"foreign static rectb(x, y, w, h, color)\n"
"foreign static tri(x1, y1, x2, y2, x3, y3, color)\n"
"foreign static cls()\n"
"foreign static cls(color)\n"
"foreign static clip(x, y, w, h)\n"
"foreign static peek(addr)\n"
"foreign static poke(addr, val)\n"
"foreign static peek4(addr)\n"
"foreign static poke4(addr, val)\n"
"foreign static memcpy(dst, src, size)\n"
"foreign static memset(dst, src, size)\n"
"foreign static pmem(index, val)\n"
"foreign static sfx(id, note, duration, channel, volume, speed)\n"
"foreign static music(track, frame, loop)\n"
"foreign static time()\n"
"foreign static sync()\n"
"foreign static exit()\n"
"}\n"
"class Engine {\n"
"update(){}\n"
"scanline(row){}\n"
"}\n"
;

static inline s32 getWrenNumber(WrenVM* vm, s32 index)
{
	return (s32)wrenGetSlotDouble(vm, index);
}

static inline bool isNull(WrenVM* vm, s32 index)
{
	return wrenGetSlotType(vm, index) == WREN_TYPE_NULL;
}

static inline bool isBool(WrenVM* vm, s32 index)
{
	return wrenGetSlotType(vm, index) == WREN_TYPE_BOOL;
}

static inline bool isNumber(WrenVM* vm, s32 index)
{
	return wrenGetSlotType(vm, index) == WREN_TYPE_NUM;
}

static inline bool isString(WrenVM* vm, s32 index)
{
	return wrenGetSlotType(vm, index) == WREN_TYPE_STRING;
}

static inline bool isList(WrenVM* vm, s32 index)
{
	return wrenGetSlotType(vm, index) == WREN_TYPE_LIST;
}

void closeWren(tic_machine* machine)
{
	if(machine->wren)
	{	
		// release handles
		if (file_loaded)
		{
	  		wrenReleaseHandle(machine->wren, game_class);
	  		wrenReleaseHandle(machine->wren, new_handle);
	  		wrenReleaseHandle(machine->wren, update_handle);
	  		wrenReleaseHandle(machine->wren, scanline_handle);
	  		wrenReleaseHandle(machine->wren, call3_handle);
		}

		wrenFreeVM(machine->wren);
		machine->wren = NULL;

	}
	file_loaded = false;
}

static tic_machine* getWrenMachine(WrenVM* vm)
{
	tic_machine* machine = wrenGetUserData(vm);

	return machine;
}

static s32 wren_btn(WrenVM* vm)
{
	tic_machine* machine = getWrenMachine(vm);

	if(machine->memory.input == tic_gamepad_input)
	{
		s32 top = wrenGetSlotCount(vm);

		if (top == 1)
		{
			wrenSetSlotBool(vm, 0, machine->memory.ram.vram.input.gamepad.data);
		}
		else if (top == 2)
		{
			s32 index = getWrenNumber(vm, 1) & 0xf;
			wrenSetSlotBool(vm, 0, machine->memory.ram.vram.input.gamepad.data & (1 << index));
		}

		return 1;		
	}
	return 0;
}

static s32 wren_btnp(WrenVM* vm)
{	
	tic_machine* machine = getWrenMachine(vm);
	tic_mem* memory = (tic_mem*)machine;

	if(machine->memory.input == tic_gamepad_input)
	{
		s32 top = wrenGetSlotCount(vm);

		if (top == 1)
		{
			wrenSetSlotBool(vm, 0, memory->api.btnp(memory, -1, -1, -1));
		}
		else if(top == 2)
		{
			s32 index = getWrenNumber(vm, 1) & 0xf;

			wrenSetSlotBool(vm, 0, memory->api.btnp(memory, index, -1, -1));
		}
		else if (top == 4)
		{
			s32 index = getWrenNumber(vm, 1) & 0xf;
			u32 hold = getWrenNumber(vm, 2);
			u32 period = getWrenNumber(vm, 3);

			wrenSetSlotBool(vm, 0, memory->api.btnp(memory, index, hold, period));
		}

		return 1;
	}
	return 0;
}

static s32 wren_mouse(WrenVM* vm)
{
	tic_machine* machine = getWrenMachine(vm);

	if(machine->memory.input == tic_mouse_input)
	{
		u16 data = machine->memory.ram.vram.input.gamepad.data;
		// todo
		// wrenSetSlotDouble(vm, 0, (data & 0x7fff) % TIC80_WIDTH);
		// wrenSetSlotDouble(vm, 1, (data & 0x7fff) / TIC80_WIDTH);
		// wrenSetSlotBool(vm, 2, data >> 15);

		return 3;		
	} else {
		machine->data->error(machine->data->data, "mouse input not declared in metadata\n");
	}

	return 0;
}

static s32 wren_print(WrenVM* vm)
{
	int top = wrenGetSlotCount(vm);

	if(top > 1) 
	{
		tic_mem* memory = (tic_mem*)getWrenMachine(vm);

		s32 x = 0;
		s32 y = 0;
		s32 color = TIC_PALETTE_SIZE-1;
		bool fixed = false;
		s32 scale = 1;

		const char* text = NULL;
		if (isString(vm, 1))
		{
			text = wrenGetSlotString(vm, 1);
		}

		if(top > 3)
		{
			x = getWrenNumber(vm, 2);
			y = getWrenNumber(vm, 3);

			if(top > 4)
			{
				color = getWrenNumber(vm, 4) % TIC_PALETTE_SIZE;

				if(top > 5)
				{
					fixed = wrenGetSlotBool(vm, 5);

					if(top > 6)
					{
						scale = getWrenNumber(vm, 6);
					}
				}
			}
		}

		if(scale == 0)
		{
			wrenSetSlotDouble(vm, 0, 0);
			return 1;
		}

		s32 size = memory->api.text_ex(memory, text ? text : "null", x, y, color, fixed, scale);

		wrenSetSlotDouble(vm, 0, size);

		return 1;
	}

	return 0;
}

static s32 wren_font(WrenVM* vm)
{

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);
	s32 top = wrenGetSlotCount(vm);

	if(top > 1)
	{

		const char* text = NULL;
		if (isString(vm, 1))
		{
			text = wrenGetSlotString(vm, 1);
		}

		s32 x = 0;
		s32 y = 0;
		s32 width = TIC_SPRITESIZE;
		s32 height = TIC_SPRITESIZE;
		u8 chromakey = 0;
		bool fixed = false;
		s32 scale = 1;

		if(top > 3)
		{
			x = getWrenNumber(vm, 2);
			y = getWrenNumber(vm, 3);

			if(top > 4)
			{
				chromakey = getWrenNumber(vm, 4);

				if(top > 6)
				{
					width = getWrenNumber(vm, 5);
					height = getWrenNumber(vm, 6);

					if(top > 7)
					{
						fixed = wrenGetSlotBool(vm, 7);

						if(top > 8)
						{
							scale = getWrenNumber(vm, 8);
						}
					}
				}
			}
		}

		if(scale == 0)
		{
			wrenSetSlotDouble(vm, 0, 0);
			return 1;
		}

		s32 size = drawText(memory, text, x, y, width, height, chromakey, scale, fixed ? drawSpriteFont : drawFixedSpriteFont);

		wrenSetSlotDouble(vm, 0, size);


		return 1;
	}

	return 0;
}

static s32 wren_trace(WrenVM* vm)
{
	s32 top = wrenGetSlotCount(vm);
	tic_machine* machine = getWrenMachine(vm);

	const char* text = wrenGetSlotString(vm, 1);
	u8 color = (u8)wrenGetSlotDouble(vm, 2);

	machine->data->trace(machine->data->data, text ? text : "null", color);

	return 0;
}

static s32 wren_spr(WrenVM* vm)
{	
	s32 top = wrenGetSlotCount(vm);

	s32 index = 0;
	s32 x = 0;
	s32 y = 0;
	s32 w = 1;
	s32 h = 1;
	s32 scale = 1;
	tic_flip flip = tic_no_flip;
	tic_rotate rotate = tic_no_rotate;
	static u8 colors[TIC_PALETTE_SIZE];
	s32 count = 0;

	if(top > 1) 
	{
		index = getWrenNumber(vm, 1);

		if(top > 3)
		{
			x = getWrenNumber(vm, 2);
			y = getWrenNumber(vm, 3);

			if(top > 4)
			{
				if(isList(vm, 4))
				{
					for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
					{
						wrenGetListElement(vm, 4, i, 0); // todo, can be bug cause in slot 0 is object?
						if(isNumber(vm, 0))
						{
							colors[i] = getWrenNumber(vm, 0);
							count++;
						}
						else
						{
							break;
						}
					}
				}
				else 
				{
					colors[0] = getWrenNumber(vm, 4);
					count = 1;
				}

				if(top > 5)
				{
					scale = getWrenNumber(vm, 5);

					if(top > 6)
					{
						flip = getWrenNumber(vm, 6);

						if(top > 7)
						{
							rotate = getWrenNumber(vm, 7);

							if(top > 9)
							{
								w = getWrenNumber(vm, 8);
								h = getWrenNumber(vm, 9);
							}
						}
					}
				}
			}
		}
	}

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.sprite_ex(memory, &memory->ram.gfx, index, x, y, w, h, colors, count, scale, flip, rotate);

	return 0;
}

typedef struct
{
	WrenVM* vm;
	WrenHandle* handle;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
	RemapData* remap = (RemapData*)data;
	WrenVM* vm = remap->vm;

	wrenEnsureSlots(vm, 4);
	wrenSetSlotHandle(vm, 0, remap->handle);
	wrenSetSlotDouble(vm, 1, result->index);
	wrenSetSlotDouble(vm, 2, x);
	wrenSetSlotDouble(vm, 3, y);
	wrenCall(vm, call3_handle);

	if (isList(vm, 0))
	{
		// todo
		// wrenGetListElement(vm, 0, 0, 1);
		// if(isNumber(vm, 1))
		// {
		// 	result->index = getWrenNumber(vm, 1);
		// }
		// wrenGetListElement(vm, 0, 1, 1);
		// if(isNumber(vm, 1))
		// {
		// 	result->flip = getWrenNumber(vm, 1);
		// }
		// wrenGetListElement(vm, 0, 2, 1);
		// if(isNumber(vm, 1))
		// {
		// 	result->rotate = getWrenNumber(vm, 1);
		// }
	} else if(isNumber(vm, 0)){
		result->index = getWrenNumber(vm, 0);
	}
}

static s32 wren_map(WrenVM* vm)
{
	s32 x = 0;
	s32 y = 0;
	s32 w = TIC_MAP_SCREEN_WIDTH;
	s32 h = TIC_MAP_SCREEN_HEIGHT;
	s32 sx = 0;
	s32 sy = 0;
	u8 chromakey = -1;
	s32 scale = 1;

	s32 top = wrenGetSlotCount(vm);

	if(top > 2) 
	{
		x = getWrenNumber(vm, 1);
		y = getWrenNumber(vm, 2);

		if(top > 4)
		{
			w = getWrenNumber(vm, 3);
			h = getWrenNumber(vm, 4);

			if(top > 6)
			{
				sx = getWrenNumber(vm, 5);
				sy = getWrenNumber(vm, 6);

				if(top > 7)
				{
					chromakey = getWrenNumber(vm, 7);

					if(top > 8)
					{
						scale = getWrenNumber(vm, 8);

						if(top > 9)
						{
							WrenHandle* handle = wrenGetSlotHandle(vm, 9);

							RemapData data = {vm, handle};

							tic_mem* memory = (tic_mem*)getWrenMachine(vm);

							memory->api.remap(memory, &memory->ram.gfx, x, y, w, h, sx, sy, chromakey, scale, remapCallback, &data);

	  						wrenReleaseHandle(vm, data.handle);

							return 0;				
						}
					}
				}
			}
		}
	}

	tic_mem* memory = (tic_mem*)getWrenMachine(vm); // todo why we call it so many times?

	memory->api.map((tic_mem*)getWrenMachine(vm), &memory->ram.gfx, x, y, w, h, sx, sy, chromakey, scale);
	return 0;
}

static s32 wren_mset(WrenVM* vm)
{

	s32 x = getWrenNumber(vm, 1);
	s32 y = getWrenNumber(vm, 2);
	u8 val = getWrenNumber(vm, 3);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.map_set(memory, &memory->ram.gfx, x, y, val);

	return 0;
}

static s32 wren_mget(WrenVM* vm)
{		
	s32 x = getWrenNumber(vm, 1);
	s32 y = getWrenNumber(vm, 2);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	u8 value = memory->api.map_get(memory, &memory->ram.gfx, x, y);
	wrenSetSlotDouble(vm, 0, value);

	return 1;
}

static s32 wren_textri(WrenVM* vm)
{
	int top = wrenGetSlotCount(vm);

	s32 pt[12];

	for (s32 i = 0; i < COUNT_OF(pt); i++)
		pt[i] = getWrenNumber(vm, i + 1);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);
	u8 chroma = 0xff;
	bool use_map = false;

	//	check for use map 
	if (top > 13)
		use_map = wrenGetSlotBool(vm, 13);
	//	check for chroma 
	if (top > 14)
		chroma = (u8)getWrenNumber(vm, 14);

	memory->api.textri(memory, pt[0], pt[1],	//	xy 1
								pt[2], pt[3],	//	xy 2
								pt[4], pt[5],	//  xy 3
								pt[6], pt[7],	//	uv 1
								pt[8], pt[9],	//	uv 2
								pt[10], pt[11], //  uv 3
								use_map,		// use map
								chroma);		// chroma

	return 0;
}

static s32 wren_pix(WrenVM* vm)
{
	int top = wrenGetSlotCount(vm);

	s32 x = getWrenNumber(vm, 1);
	s32 y = getWrenNumber(vm, 2);
	
	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	if(top > 3)
	{
		s32 color = getWrenNumber(vm, 3);
		memory->api.pixel(memory, x, y, color);
	}
	else
	{
		wrenSetSlotDouble(vm, 0, memory->api.get_pixel(memory, x, y));
		return 1;
	}

	return 0;
}

static s32 wren_line(WrenVM* vm)
{
	s32 x0 = getWrenNumber(vm, 1);
	s32 y0 = getWrenNumber(vm, 2);
	s32 x1 = getWrenNumber(vm, 3);
	s32 y1 = getWrenNumber(vm, 4);
	s32 color = getWrenNumber(vm, 5);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.line(memory, x0, y0, x1, y1, color);

	return 0;
}

static s32 wren_circ(WrenVM* vm)
{
	s32 radius = getWrenNumber(vm, 3);
	if(radius < 0) return 0;
	
	s32 x = getWrenNumber(vm, 1);
	s32 y = getWrenNumber(vm, 2);
	s32 color = getWrenNumber(vm, 4);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.circle(memory, x, y, radius, color);

	return 0;
}

static s32 wren_circb(WrenVM* vm)
{
	s32 radius = getWrenNumber(vm, 3);
	if(radius < 0) return 0;
	
	s32 x = getWrenNumber(vm, 1);
	s32 y = getWrenNumber(vm, 2);
	s32 color = getWrenNumber(vm, 4);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.circle_border(memory, x, y, radius, color);

	return 0;
}

static s32 wren_rect(WrenVM* vm)
{
	s32 x = getWrenNumber(vm, 1);
	s32 y = getWrenNumber(vm, 2);
	s32 w = getWrenNumber(vm, 3);
	s32 h = getWrenNumber(vm, 4);
	s32 color = getWrenNumber(vm, 5);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.rect(memory, x, y, w, h, color);

	return 0;
}

static s32 wren_rectb(WrenVM* vm)
{
	s32 x = getWrenNumber(vm, 1);
	s32 y = getWrenNumber(vm, 2);
	s32 w = getWrenNumber(vm, 3);
	s32 h = getWrenNumber(vm, 4);
	s32 color = getWrenNumber(vm, 5);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.rect_border(memory, x, y, w, h, color);

	return 0;
}

static s32 wren_tri(WrenVM* vm)
{		
	s32 pt[6];

	for(s32 i = 0; i < COUNT_OF(pt); i++)
		pt[i] = getWrenNumber(vm, i+1);
	
	s32 color = getWrenNumber(vm, 7);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.tri(memory, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);

	return 0;
}

static s32 wren_cls(WrenVM* vm)
{
	int top = wrenGetSlotCount(vm);

	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.clear(memory, top == 1 ? 0 : getWrenNumber(vm, 1));

	return 0;
}

static s32 wren_clip(WrenVM* vm)
{
	s32 top = wrenGetSlotCount(vm);

	if(top == 1)
	{
		tic_mem* memory = (tic_mem*)getWrenMachine(vm);

		memory->api.clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
	}
	else if(top == 4)
	{
		s32 x = getWrenNumber(vm, 1);
		s32 y = getWrenNumber(vm, 2);
		s32 w = getWrenNumber(vm, 3);
		s32 h = getWrenNumber(vm, 4);

		tic_mem* memory = (tic_mem*)getWrenMachine(vm); // why is 3 times we get machine?

		memory->api.clip((tic_mem*)getWrenMachine(vm), x, y, w, h);
	}

	return 0;
}

static s32 wren_peek(WrenVM* vm)
{
	tic_machine* machine = getWrenMachine(vm);

	// check number of args
	s32 address = getWrenNumber(vm, 1);

	if(address >=0 && address < sizeof(tic_ram))
	{
		wrenSetSlotDouble(vm, 0, *((u8*)&machine->memory.ram + address));
		return 1;
	}

	return 0;
}

static s32 wren_poke(WrenVM* vm)
{
	tic_machine* machine = getWrenMachine(vm);

	s32 address = getWrenNumber(vm, 1);
	u8 value = getWrenNumber(vm, 2) & 0xff;

	if(address >=0 && address < sizeof(tic_ram))
	{
		*((u8*)&machine->memory.ram + address) = value;
	}

	return 0;
}

static s32 wren_peek4(WrenVM* vm)
{
	s32 address = getWrenNumber(vm, 1);

	if(address >= 0 && address < sizeof(tic_ram)*2)
	{
		wrenSetSlotDouble(vm, 0, tic_tool_peek4((u8*)&getWrenMachine(vm)->memory.ram, address));
		return 1;
	}	

	return 0;
}

static s32 wren_poke4(WrenVM* vm)
{
	s32 address = getWrenNumber(vm, 1);
	u8 value = getWrenNumber(vm, 2);

	if(address >= 0 && address < sizeof(tic_ram)*2)
	{
		tic_tool_poke4((u8*)&getWrenMachine(vm)->memory.ram, address, value);
	}

	return 0;
}

static s32 wren_memcpy(WrenVM* vm)
{
	s32 dest = getWrenNumber(vm, 1);
	s32 src = getWrenNumber(vm, 2);
	s32 size = getWrenNumber(vm, 3);
    s32 bound = sizeof(tic_ram) - size;

	if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && src >= 0 && dest <= bound && src <= bound)
	{
		u8* base = (u8*)&getWrenMachine(vm)->memory;
		memcpy(base + dest, base + src, size);
	}

	return 0;
}

static s32 wren_memset(WrenVM* vm)
{
	s32 dest = getWrenNumber(vm, 1);
	u8 value = getWrenNumber(vm, 2);
	s32 size = getWrenNumber(vm, 3);
	s32 bound = sizeof(tic_ram) - size;

	if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && dest <= bound)
	{
		u8* base = (u8*)&getWrenMachine(vm)->memory;
		memset(base + dest, value, size);
	}

	return 0;
}

static s32 wren_pmem(WrenVM* vm)
{
	s32 top = wrenGetSlotCount(vm);
	tic_machine* machine = getWrenMachine(vm);
	tic_mem* memory = &machine->memory;

	u32 index = getWrenNumber(vm, 1);
	index %= TIC_PERSISTENT_SIZE;

	s32 val = memory->ram.persistent.data[index];

	if(top > 2)
	{
		memory->ram.persistent.data[index] = getWrenNumber(vm, 2);
	}

	wrenSetSlotDouble(vm, 0, val);

	return 1;
}

static s32 wren_sfx(WrenVM* vm)
{
	s32 top = wrenGetSlotCount(vm);

	tic_machine* machine = getWrenMachine(vm);

	if(top > 1)
	{
		tic_mem* memory = &machine->memory;

		s32 note = -1;
		s32 octave = -1;
		s32 duration = -1;
		s32 channel = 0;
		s32 volume = MAX_VOLUME;
		s32 speed = SFX_DEF_SPEED;

		s32 index = getWrenNumber(vm, 1);

		if(index < SFX_COUNT)
		{
			if (index >= 0)
			{
				tic_sound_effect* effect = memory->ram.sound.sfx.data + index;

				note = effect->note;
				octave = effect->octave;
				speed = effect->speed;
			}

			if(top > 2)
			{
				if(isNumber(vm, 2))
				{
					s32 id = getWrenNumber(vm, 2);
					note = id % NOTES;
					octave = id / NOTES;
				}
				else if(isString(vm, 2))
				{
					const char* noteStr = wrenGetSlotString(vm, 2);

					if(!tic_tool_parse_note(noteStr, &note, &octave))
					{
						machine->data->error(machine->data->data, "invalid note, should be like C#4\n");
						return 0;
					}
				}

				if(top > 3)
				{
					duration = getWrenNumber(vm, 3);

					if(top > 4)
					{
						channel = getWrenNumber(vm, 4);

						if(top > 5)
						{
							volume = getWrenNumber(vm, 5);

							if(top > 6)
							{
								speed = getWrenNumber(vm, 6);
							}
						}
					}					
				}
			}

			if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
			{
				memory->api.sfx_stop(memory, channel);
				memory->api.sfx_ex(memory, index, note, octave, duration, channel, volume & 0xf, speed);
			}		
			else machine->data->error(machine->data->data, "unknown channel\n");
		}
		else machine->data->error(machine->data->data, "unknown sfx index\n");
	}
	else machine->data->error(machine->data->data, "invalid sfx params\n");

	return 0;
}

static s32 wren_music(WrenVM* vm)
{
	s32 top = wrenGetSlotCount(vm);
	
	tic_mem* memory = (tic_mem*)getWrenMachine(vm);
	memory->api.music(memory, -1, 0, 0, false);

	s32 track = getWrenNumber(vm, 1);
	s32 frame = -1;
	s32 row = -1;
	bool loop = true;

	if(top > 2)
	{
		frame = getWrenNumber(vm, 2);

		if(top > 3)
		{
			row = getWrenNumber(vm, 3);

			if(top > 4)
			{
				loop = wrenGetSlotBool(vm, 4);
			}
		}
	}

	memory->api.music(memory, track, frame, row, loop);

	return 0;
}

static s32 wren_time(WrenVM* vm)
{
	tic_mem* memory = (tic_mem*)getWrenMachine(vm);
	
	wrenSetSlotDouble(vm, 0, memory->api.time(memory));

	return 1;
}

static s32 wren_sync(WrenVM* vm)
{
	tic_mem* memory = (tic_mem*)getWrenMachine(vm);

	memory->api.sync(memory, true);

	return 0;
}

static s32 wren_exit(WrenVM* vm)
{
	tic_machine* machine = getWrenMachine(vm);

	machine->data->exit(machine->data->data);

	return 0;
}

static const char* const ApiKeywords[] = API_KEYWORDS;

static const WrenForeignMethodFn ApiFunc[] = 
{
	NULL, NULL, wren_print, wren_cls, wren_pix, wren_line, wren_rect, 
	wren_rectb, wren_spr, wren_btn, wren_btnp, wren_sfx, wren_map, wren_mget, 
	wren_mset, wren_peek, wren_poke, wren_peek4, wren_poke4, wren_memcpy, 
	wren_memset, wren_trace, wren_pmem, wren_time, wren_exit, wren_font, wren_mouse, 
	wren_circ, wren_circb, wren_tri, wren_textri, wren_clip, wren_music, wren_sync
};

WrenForeignMethodFn foreignTicMethods(const char* signature){

	if (strcmp(signature, "static Tic.btn(_)"                	) == 0) return wren_btn;
	if (strcmp(signature, "static Tic.btnp(_,_,_)"              ) == 0) return wren_btnp;
	if (strcmp(signature, "static Tic.mouse()"                	) == 0) return wren_mouse;

	if (strcmp(signature, "static Tic.print(_)"                	) == 0) return wren_print;
	if (strcmp(signature, "static Tic.print(_,_,_)"            	) == 0) return wren_print;
	if (strcmp(signature, "static Tic.print(_,_,_,_)"          	) == 0) return wren_print;
	if (strcmp(signature, "static Tic.print(_,_,_,_,_)"        	) == 0) return wren_print;
	if (strcmp(signature, "static Tic.print(_,_,_,_,_,_)"      	) == 0) return wren_print;

	if (strcmp(signature, "static Tic.font(_)"	                ) == 0) return wren_font;
	if (strcmp(signature, "static Tic.font(_,_,_)"	            ) == 0) return wren_font;
	if (strcmp(signature, "static Tic.font(_,_,_,_)"	        ) == 0) return wren_font;
	if (strcmp(signature, "static Tic.font(_,_,_,_,_,_)"	    ) == 0) return wren_font;
	if (strcmp(signature, "static Tic.font(_,_,_,_,_,_,_)"	    ) == 0) return wren_font;
	if (strcmp(signature, "static Tic.font(_,_,_,_,_,_,_,_)"	) == 0) return wren_font;

	if (strcmp(signature, "static Tic.trace__(_,_)"             ) == 0) return wren_trace;

	if (strcmp(signature, "static Tic.spr(_)"	                ) == 0) return wren_spr;
	if (strcmp(signature, "static Tic.spr(_,_,_)"	            ) == 0) return wren_spr;
	if (strcmp(signature, "static Tic.spr(_,_,_,_)"	            ) == 0) return wren_spr;
	if (strcmp(signature, "static Tic.spr(_,_,_,_,_)"	        ) == 0) return wren_spr;
	if (strcmp(signature, "static Tic.spr(_,_,_,_,_,_)"	        ) == 0) return wren_spr;
	if (strcmp(signature, "static Tic.spr(_,_,_,_,_,_,_)"	    ) == 0) return wren_spr;
	if (strcmp(signature, "static Tic.spr(_,_,_,_,_,_,_,_,_)"	) == 0) return wren_spr;

	if (strcmp(signature, "static Tic.map(_,_)"	                ) == 0) return wren_map;
	if (strcmp(signature, "static Tic.map(_,_,_,_)"	            ) == 0) return wren_map;
	if (strcmp(signature, "static Tic.map(_,_,_,_,_,_)"	        ) == 0) return wren_map;
	if (strcmp(signature, "static Tic.map(_,_,_,_,_,_,_)"	    ) == 0) return wren_map;
	if (strcmp(signature, "static Tic.map(_,_,_,_,_,_,_,_)"	    ) == 0) return wren_map;
	if (strcmp(signature, "static Tic.map(_,_,_,_,_,_,_,_,_)"	) == 0) return wren_map;

	if (strcmp(signature, "static Tic.mset(_,_)"	            ) == 0) return wren_mset;
	if (strcmp(signature, "static Tic.mset(_,_,_)"	            ) == 0) return wren_mset;
	if (strcmp(signature, "static Tic.mget(_,_)"	            ) == 0) return wren_mget;

	if (strcmp(signature, "static Tic.textri(_,_,_,_,_,_,_,_,_,_,_,_)"	     ) == 0) return wren_textri;
	if (strcmp(signature, "static Tic.textri(_,_,_,_,_,_,_,_,_,_,_,_,_)"	 ) == 0) return wren_textri;
	if (strcmp(signature, "static Tic.textri(_,_,_,_,_,_,_,_,_,_,_,_,_,_)"	 ) == 0) return wren_textri;

	if (strcmp(signature, "static Tic.pix(_,_)"          		) == 0) return wren_pix;
	if (strcmp(signature, "static Tic.pix(_,_,_)"        		) == 0) return wren_pix;
	if (strcmp(signature, "static Tic.line(_,_,_,_,_)"   		) == 0) return wren_line;
	if (strcmp(signature, "static Tic.circ(_,_,_,_)"     		) == 0) return wren_circ;
	if (strcmp(signature, "static Tic.circb(_,_,_,_)"    		) == 0) return wren_circb;
	if (strcmp(signature, "static Tic.rect(_,_,_,_,_)"   		) == 0) return wren_rect;
	if (strcmp(signature, "static Tic.rectb(_,_,_,_,_)"  		) == 0) return wren_rectb;
	if (strcmp(signature, "static Tic.tri(_,_,_,_,_,_,_)"		) == 0) return wren_tri;

	if (strcmp(signature, "static Tic.cls()"                    ) == 0) return wren_cls;
	if (strcmp(signature, "static Tic.cls(_)"                   ) == 0) return wren_cls;
	if (strcmp(signature, "static Tic.clip(_,_,_,_)"            ) == 0) return wren_clip;

	if (strcmp(signature, "static Tic.peek(_)"      			) == 0) return wren_peek;
	if (strcmp(signature, "static Tic.poke(_,_)"    			) == 0) return wren_poke;
	if (strcmp(signature, "static Tic.peek4(_)"     			) == 0) return wren_peek4;
	if (strcmp(signature, "static Tic.poke4(_,_)"   			) == 0) return wren_poke4;
	if (strcmp(signature, "static Tic.memcpy(_,_,_)"			) == 0) return wren_memcpy;
	if (strcmp(signature, "static Tic.memset(_,_,_)"			) == 0) return wren_memset;
	if (strcmp(signature, "static Tic.pmem(_,_)"    			) == 0) return wren_pmem;

	if (strcmp(signature, "static Tic.sfx(_,_,_,_,_,_)"    		) == 0) return wren_sfx;
	if (strcmp(signature, "static Tic.music(_,_,_)"    			) == 0) return wren_music;

	if (strcmp(signature, "static Tic.time()"    			    ) == 0) return wren_time;
	if (strcmp(signature, "static Tic.sync()"    			    ) == 0) return wren_sync;
	if (strcmp(signature, "static Tic.exit()"    			    ) == 0) return wren_exit;

	return NULL;
}

static WrenForeignMethodFn bindForeignMethod(
	WrenVM* vm, const char* module, const char* className,
	bool isStatic, const char* signature)
{  
	
	if (strcmp(module, "main") != 0) return NULL;

	// For convenience, concatenate all of the method qualifiers into a single
	// signature string.
	char fullName[256];
	fullName[0] = '\0';
	if (isStatic) {
		strcat(fullName, "static ");
	}

	strcat(fullName, className);
	strcat(fullName, ".");
	strcat(fullName, signature);

	WrenForeignMethodFn method = NULL;
	
	method = foreignTicMethods(fullName);

	return method;
}

static void initAPI(tic_machine* machine)
{
	wrenSetUserData(machine->wren, machine);

	if (wrenInterpret(machine->wren, tic_wren_api) != WREN_RESULT_SUCCESS)
	{					
		machine->data->error(machine->data->data, "can't load Tic wren api");
	}
}

static void reportError(WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message)
{
	tic_machine* machine = getWrenMachine(vm);

    char buffer[1024];

    if (module)
    {
    	snprintf(buffer, sizeof buffer, "\"%s\", %d ,\"%s\"",module, line, message);
    } else {
    	snprintf(buffer, sizeof buffer, "%d, \"%s\"",line, message);
    }

	machine->data->error(machine->data->data, buffer);
}

void writeFn(WrenVM* vm, const char* text) 
{
	tic_machine* machine = getWrenMachine(vm);
	u8 color = tic_color_blue;
	machine->data->trace(machine->data->data, text ? text : "null", color);
}

bool initWren(tic_machine* machine, const char* code)
{
	closeWren(machine);

	WrenConfiguration config; 
	wrenInitConfiguration(&config);

	config.bindForeignMethodFn = bindForeignMethod;

	config.errorFn = reportError;
	config.writeFn = writeFn;

	WrenVM* vm = machine->wren = wrenNewVM(&config);

	initAPI(machine);
	
	if (wrenInterpret(machine->wren, code) != WREN_RESULT_SUCCESS){
		return false;
	}

	file_loaded = true;

	// make handles
	wrenEnsureSlots(vm, 1);
	wrenGetVariable(vm, "main", "Game", 0);
	game_class = wrenGetSlotHandle(vm, 0); // handle from game class 
	new_handle = wrenMakeCallHandle(vm, "new()");
	update_handle = wrenMakeCallHandle(vm, "update()");
	scanline_handle = wrenMakeCallHandle(vm, "scanline(_)");
	call3_handle = wrenMakeCallHandle(vm, "call(_,_,_)");

	// create game class
	if (game_class)
	{
		wrenEnsureSlots(vm, 1);
		wrenSetSlotHandle(vm, 0, game_class);
		wrenCall(vm, new_handle);
	  	wrenReleaseHandle(machine->wren, game_class); // release game class handle
		game_class = wrenGetSlotHandle(vm, 0); // handle from game object 
	} else {
		machine->data->error(machine->data->data, "'Game class' isn't found :(");	
	}

	return true;
}

void callWrenTick(tic_machine* machine)
{
 	WrenVM* vm = machine->wren;

 	if(vm && game_class)
 	{
	  	wrenEnsureSlots(vm, 1);
		wrenSetSlotHandle(vm, 0, game_class);
		wrenCall(vm, update_handle);
 	}
}

void callWrenScanline(tic_mem* memory, s32 row)
{
	tic_machine* machine = (tic_machine*)memory;
 	WrenVM* vm = machine->wren;

 	if(vm && game_class)
 	{
	  	wrenEnsureSlots(vm, 2);
		wrenSetSlotHandle(vm, 0, game_class);
		wrenSetSlotDouble(vm, 1, row);
		wrenCall(vm, scanline_handle);
 	}
}
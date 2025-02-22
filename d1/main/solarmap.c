#include <math.h>
#include "gr.h"
#include "newmenu.h"
#include "pcx.h"
#include "dxxerror.h"
#include "window.h"
#include "ogl_init.h"
#include "timer.h"
#include "gamefont.h"
#include "screens.h"
#include "key.h"
#include "config.h"
#include "gameseq.h"
#include "physfsx.h"
#include "game.h"
#include "args.h"
#include "solarmap.h"
#include "u_mem.h"

#define BMS_MAP 0
#define BMS_SHIP 3
#define BMS_SHIP_NUM 2
#define BMS_NUM 5

static const char *bms_files[] = { "map01.pcx", "map02.pcx", "map03.pcx", "ship01.pcx", "ship02.pcx" };

struct solarmap {
	struct window *wind;
	ubyte pals[5][768];
	grs_bitmap bms[5];
	fix64 start_time;
	float step;
	int ship_idx;
	int next_idx;
	int end_idx;
	int reversed;
	int ending;
	int done;
	int select;
	fix64 last_frame;
	fix frame_time;
	int *rval;
};

void solarmap_init(struct solarmap *sm, grs_canvas *src, int x, int y, int w, int h);
int solarmap_handler(struct window *wind, d_event *event, void *data);
void solarmap_move(struct solarmap *sm, int dir);

#define NUM_LVLINFO 34
struct {
	short level, map, x, y;
	const char *name;
} lvlinfo[NUM_LVLINFO] = {
	{ 1, 0, 286, 143, "Lunar Outpost" },
	{ 2, 0, 249, 136, "Lunar Scilab" },
	{ 3, 0, 208, 120, "Lunar Military Base" },
	{ 4, 0, 157, 110, "Venus Atmosphere Lab" },
	{ 5, 0, 130, 115, "Venus Nickel-Iron Mine" },
	{ 6, 0, 97, 140, "Mercury Solar Lab" },
	{ 7, 0, 82, 168, "Mercury Military Lab" },
	{ -100, 0, 70, 199, "None1" },
	{ -101, 1, 0, 125, "None2" },
	{ 8, 1, 26, 105, "Mars Processing Station" },
	{ 9, 1, 53, 72, "Mars Colony" },
	{ 10, 1, 83, 64, "Mars Orbital Station" },
	{ -1, 1, 94, 69, "Secret Level #1" },
	{ 11, 1, 106, 74, "Io Sulpher Mine" },
	{ 12, 1, 125, 100, "Callisto Tower Colony" },
	{ 13, 1, 145, 112, "Europa Mining Colony" },
	{ 14, 1, 177, 119, "Ganymede Military Base" },
	{ 15, 1, 205, 135, "Europa Diamond Mine" },
	{ 16, 1, 225, 159, "Hyperion Methane Mine" },
	{ 17, 1, 262, 182, "Tethys Military Base" },
	{ -102, 1, 319, 193, "None3" },
	{ -103, 2, 0, 27, "None4" },
	{ 18, 2, 15, 48, "Miranda Mine" },
	{ 19, 2, 19, 76, "Oberon Mine" },
	{ 20, 2, 25, 110, "Oberon Iron Mine" },
	{ 21, 2, 65, 131, "Oberon Colony" },
	{ -2, 2, 86, 125, "Secret Level #2" },
	{ 22, 2, 107, 120, "Neptune Storage Depot" },
	{ 23, 2, 144, 96, "Triton Storage Depot" },
	{ 24, 2, 188, 90, "Nereid Volatile Mine" },
	{ -3, 2, 211, 96, "Secret Level #3" },
	{ 25, 2, 235, 102, "Pluto Outpost" },
	{ 26, 2, 267, 108, "Pluto Military Base" },
	{ 27, 2, 296, 98, "Charon Volatile Mine" } };

static void show_bitmap_at(grs_bitmap *bmp, int x, int y, int w, int h)
{
	const int hiresmode = 0; //HIRESMODE;
	const float sw = (float)SWIDTH / (hiresmode ? 640 : 320);
	const float sh = (float)SHEIGHT / (hiresmode ? 480 : 240);
	const float scale = (sw < sh) ? sw : sh;

	ogl_ubitmapm_cs(x * scale, y * scale, (w ? w : bmp->bm_w)*scale, (h ? h : bmp->bm_h)*scale, bmp, -1, F1_0); //f2fl(scale));
}

static void draw_solarmap(struct solarmap *sm)
{
	const float w = (float)SWIDTH / 320;
	const float h = (float)SHEIGHT / 240;
	const float scale = (w < h) ? w : h;
	int cur_map = lvlinfo[sm->ship_idx].map;
	fix64 time = timer_query() - sm->start_time;
	bool hide_cur = !sm->select && !sm->ending && time < F2_0 && (time & (1 << 13));
	grs_canvas *old_canv, *sub_canv;

	gr_clear_canvas(BM_XRGB(0,0,0));

	memcpy(gr_palette, sm->pals[BMS_MAP + cur_map], sizeof(gr_palette));
	gr_palette_load(gr_palette);

	old_canv = grd_curcanv;
	sub_canv = gr_create_sub_canvas(old_canv, 0, 20*scale, 320*scale, 200*scale);
	grd_curcanv = sub_canv;
	show_fullscr(&sm->bms[BMS_MAP + cur_map]);

	gr_palette[1*3+2] = gr_palette[1*3+0] = 0;
	gr_palette[1*3+1] = 8;
	gr_palette[2*3+2] = gr_palette[2*3+0] = 0;
	gr_palette[2*3+1] = 14;
	gr_palette[3*3+2] = gr_palette[3*3+0] = 0;
	gr_palette[3*3+1] = 28;
	gr_palette_load(gr_palette);
	int c1 = 1; //BM_XRGB(0, 4, 0);
	int c2 = 2; //BM_XRGB(0, 7, 0);
	int c3 = 3; //BM_XRGB(0, 14, 0);
	for (int i = 0; i < NUM_LVLINFO - 1 && i < sm->end_idx; i++) {
		if (lvlinfo[i].map != cur_map || lvlinfo[i + 1].map != cur_map)
			continue;
		if (hide_cur && i == sm->ship_idx) {
			gr_setcolor(c3);
			gr_disk(fl2f(lvlinfo[i].x * scale), fl2f(lvlinfo[i].y * scale), fl2f(scale));
			continue;
		}
		int x1 = lvlinfo[i].x * scale, y1 = lvlinfo[i].y * scale, x2 = lvlinfo[i + 1].x * scale, y2 = lvlinfo[i + 1].y * scale;
		for (int j = scale * 2; j > 0; j--) {
			int c = j >= scale ? c1 : c2;
			gr_setcolor(c);
			gr_line(i2f(x1), i2f(y1 - j), i2f(x2), i2f(y2 - j));
			gr_line(i2f(x1 + j), i2f(y1), i2f(x2 + j), i2f(y2));
			gr_line(i2f(x1), i2f(y1 + j), i2f(x2), i2f(y2 + j));
			gr_line(i2f(x1 - j), i2f(y1), i2f(x2 - j), i2f(y2));
		}
		gr_setcolor(c2);
		gr_line(i2f(x1), i2f(y1), i2f(x2), i2f(y2));
		if (lvlinfo[i].level > -100) {
			gr_disk(i2f(x1), i2f(y1), fl2f(scale * 2));
			gr_setcolor(c3);
			gr_disk(i2f(x1), i2f(y1), fl2f(scale));
		}

		if (lvlinfo[i + 1].level > -100) {
			gr_setcolor(c2);
			gr_disk(i2f(x2), i2f(y2), fl2f(scale * 2));
		}
	}

	int next_idx = sm->next_idx;
	if (sm->select) {
		int idx = sm->ship_idx;
		if (lvlinfo[idx].level < 0) // secret or transition
			idx += sm->reversed ? -1 : 1;
		gr_set_curfont(MEDIUM3_FONT);
		gr_string(0x8000, 4 * scale, "Select starting level");
		gr_set_curfont(MEDIUM1_FONT);
		gr_string(0x8000, (4 + 2) * scale + FNTScaleY * grd_curcanv->cv_font->ft_h,
			lvlinfo[idx].name);
	} else {
		gr_set_curfont(MEDIUM1_FONT);
		gr_string(0x8000, 4 * scale, "Progressing to");
		gr_string(0x8000, (4 + 2) * scale + FNTScaleY * grd_curcanv->cv_font->ft_h,
			lvlinfo[next_idx].name);
	}

	int dir = sm->reversed ? -1 : 1;
	int x = lvlinfo[sm->ship_idx].x * (1 - sm->step) + lvlinfo[sm->ship_idx + dir].x * sm->step;
	int y = lvlinfo[sm->ship_idx].y * (1 - sm->step) + lvlinfo[sm->ship_idx + dir].y * sm->step;

	int ship_type = (lvlinfo[sm->ship_idx].map != 0) ^ sm->reversed;
	memcpy(gr_palette, sm->pals[BMS_SHIP + ship_type], sizeof(gr_palette));
	gr_palette_load(gr_palette);
	show_bitmap_at(&sm->bms[BMS_SHIP + ship_type], x - 24, y - 13, 48, 27);
	
	if (sm->ship_idx == sm->next_idx) {
		if (!sm->ending) {
			sm->start_time = timer_query();
			sm->ending = 1;
		} else if (time > F2_0 && !sm->select)
			sm->done = 1;
	} else if (time > F2_0 || sm->select) {
		if (sm->step < 1) {
			int dx = lvlinfo[sm->ship_idx + dir].x - lvlinfo[sm->ship_idx].x;
			int dy = lvlinfo[sm->ship_idx + dir].y - lvlinfo[sm->ship_idx].y;
			float dist = sqrtf(dx * dx + dy * dy);

			sm->step += 1.0f / dist * f2fl(sm->frame_time) * 100;
		}
		if (sm->step >= 1 && (sm->reversed ? sm->ship_idx > 0 : sm->ship_idx + 1 < NUM_LVLINFO)) {
			sm->step -= 1;
			sm->ship_idx += dir;
			if (lvlinfo[sm->ship_idx].level <= -100) // map transition
				sm->ship_idx += dir;
			if (sm->ship_idx == sm->next_idx) {
				int left = lvlinfo[sm->ship_idx].map ? -1 : 1;
				if (keyd_pressed[KEY_LEFT] || keyd_pressed[KEY_PAD4])
					solarmap_move(sm, left);
				else if (keyd_pressed[KEY_RIGHT] || keyd_pressed[KEY_PAD6])
					solarmap_move(sm, -left);
				sm->step = 0;
				sm->start_time = timer_query();
			}
		} else if (sm->step >= 1)
			sm->step = 1;
	}
	grd_curcanv = old_canv;
	gr_free_sub_canvas(sub_canv);
}

void solarmap_init(struct solarmap *sm, grs_canvas *src, int x, int y, int w, int h)
{
	sm->wind = window_create(src, x, y, w, h, solarmap_handler, sm);
	sm->end_idx = NUM_LVLINFO;
	sm->reversed = 0;
	sm->ending = 0;
	sm->done = 0;
	sm->select = 0;
	sm->last_frame = timer_query();
	sm->frame_time = FrameTime;
}

static void do_delay(fix64 *t1, fix *frame_time) {
	fix64 t2 = timer_query();
	const int vsync = GameCfg.VSync;
	const fix bound = F1_0 / (vsync ? MAXIMUM_FPS : GameArg.SysMaxFPS);
	const int may_sleep = GameArg.SysUseNiceFPS && !vsync;
	while (t2 - *t1 < bound) // ogl is fast enough that the automap can read the input too fast and you start to turn really slow.  So delay a bit (and free up some cpu :)
	{
		//if (Game_mode & GM_MULTI)
		//	multi_do_frame(); // during long wait, keep packets flowing
		if (may_sleep)
			timer_delay(F1_0>>8);
		timer_update();
		t2 = timer_query();
	}
	*frame_time = t2 - *t1;
	*t1 = t2;
}

void solarmap_move(struct solarmap *sm, int dir)
{
	if (dir && sm->select && sm->ship_idx == sm->next_idx) {
		int i = sm->ship_idx;
		for (;;) {
			i += dir;
			if (i < 0 || i >= NUM_LVLINFO)
				break;
			if (lvlinfo[i].level > 0) {
				sm->next_idx = i;
				sm->reversed = dir < 0;
				break;
			}
		}
	}
}

int solarmap_handler(struct window *wind, d_event *event, void *data)
{
	struct solarmap *sm = (struct solarmap *)data;
	switch (event->type)
	{
		case EVENT_WINDOW_DRAW:
			draw_solarmap(sm);
			if (sm->done) {
				if (sm->rval)
					*sm->rval = 0;
				window_close(sm->wind);
				return 1;
			}
			do_delay(&sm->last_frame, &sm->frame_time);
			break;
			
		case EVENT_KEY_COMMAND: {
			int key = event_key_get((d_event *)event);
			int left = lvlinfo[sm->ship_idx].map ? -1 : 1;
			if (key == KEY_ESC) {
				if (sm->rval)
					*sm->rval = -1;
				window_close(sm->wind);
				return 1;
			}
			if (sm->select) {
				if (key == KEY_ENTER || key == KEY_PADENTER) {
					if (sm->rval)
						*sm->rval = lvlinfo[sm->ship_idx].level;
					sm->done = 1;
					window_close(sm->wind);
					return 1;
				} else if (key == KEY_LEFT || key == KEY_PAD4) {
					solarmap_move(sm, left);
				} else if (key == KEY_RIGHT || key == KEY_PAD6) {
					solarmap_move(sm, -left);
				}
			}
			break;
		}

		case EVENT_WINDOW_CLOSE:
			for (int i = 0; i < BMS_NUM; i++)
				gr_free_bitmap_data(&sm->bms[i]);
			free(sm);
			break;
	}
	return 0;
}

int solarmap_show(int next_level)
{
	struct solarmap *sm = malloc(sizeof(struct solarmap));
	
	for (int i = 0; i < BMS_NUM; i++) {
		memset(&sm->bms[i], 0, sizeof(grs_bitmap));
		int pcx_error = pcx_read_bitmap((char *)bms_files[i], &sm->bms[i], BM_LINEAR, sm->pals[i]);
		if (pcx_error != PCX_ERROR_NONE) {
			if (!i) {
				free(sm);
				return -2; // no map available
			}
			con_printf(CON_URGENT, "solarmap: File %s - PCX error: %s", bms_files[i], pcx_errormsg(pcx_error));
			gr_init_bitmap_alloc(&sm->bms[i], BM_LINEAR, 0, 0, 2, 2, 2);
			memset(sm->bms[i].bm_data, 0, 2 * 2);
		}
		if (i >= BMS_SHIP && i < BMS_SHIP + BMS_SHIP_NUM) {
			memcpy(gr_palette, sm->pals[i], sizeof(gr_palette));
			gr_palette_load(gr_palette);
			gr_remap_bitmap_good(&sm->bms[i], sm->pals[i], 254, -1); // fix transparent color
		}
	}

	solarmap_init(sm, &grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT);
	sm->step = 0;

	if (next_level == -100) { // select
		sm->select = 1;
		sm->next_idx = sm->ship_idx = 0;
		sm->end_idx = NUM_LVLINFO - 1;
	} else { // progress
		for (int i = 0; i < NUM_LVLINFO; i++)
			if (lvlinfo[i].level == Current_level_num)
				sm->ship_idx = i;
		sm->end_idx = sm->next_idx = sm->ship_idx;
		for (int i = 0; i < NUM_LVLINFO; i++)
			if (lvlinfo[i].level == next_level)
				sm->end_idx = sm->next_idx = i;
	}

	sm->start_time = timer_query();

	int rval;
	sm->rval = &rval;
	struct window *wind = sm->wind;

	while (window_exists(wind))
		event_process();

	return rval;
}

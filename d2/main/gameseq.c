/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Routines for EndGame, EndLevel, etc.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_MSC_VER) && !defined(macintosh)
#include <unistd.h>
#endif
#include <time.h>
#include "inferno.h"
#include "game.h"
#include "player.h"
#include "key.h"
#include "object.h"
#include "physics.h"
#include "dxxerror.h"
#include "joy.h"
#include "iff.h"
#include "pcx.h"
#include "timer.h"
#include "render.h"
#include "laser.h"
#include "screens.h"
#include "textures.h"
#include "slew.h"
#include "gauges.h"
#include "texmap.h"
#include "3d.h"
#include "effects.h"
#include "menu.h"
#include "gameseg.h"
#include "wall.h"
#include "ai.h"
#include "fuelcen.h"
#include "switch.h"
#include "digi.h"
#include "gamesave.h"
#include "scores.h"
#include "u_mem.h"
#include "palette.h"
#include "morph.h"
#include "lighting.h"
#include "newdemo.h"
#include "titles.h"
#include "collide.h"
#include "weapon.h"
#include "sounds.h"
#include "args.h"
#include "gameseq.h"
#include "gamefont.h"
#include "newmenu.h"
#include "endlevel.h"
#ifdef NETWORK
#  include "multi.h"
#endif
#include "playsave.h"
#include "ctype.h"
#include "fireball.h"
#include "kconfig.h"
#include "config.h"
#include "robot.h"
#include "automap.h"
#include "cntrlcen.h"
#include "powerup.h"
#include "text.h"
#include "piggy.h"
#include "texmerge.h"
#include "paging.h"
#include "mission.h"
#include "state.h"
#include "songs.h"
#include "gamepal.h"
#include "movie.h"
#include "controls.h"
#include "credits.h"
#include "gamemine.h"
#ifdef EDITOR
#include "editor/editor.h"
#endif

#ifdef OGL
#include "ogl_init.h"
#endif

#include "strutil.h"
#include "rle.h"
#include "byteswap.h"
#include "segment.h"
#include "gameseg.h"
#include "multibot.h"
#include "math.h"
#include "object.h"


void StartNewLevelSecret(int level_num, int page_in_textures);
void InitPlayerPosition(int random_flag);
void DoEndGame(void);
void AdvanceLevel(int secret_flag);
void StartLevel(int random);
void filter_objects_from_level();





//Current_level_num starts at 1 for the first level
//-1,-2,-3 are secret levels
//0 means not a real level loaded
int	Current_level_num=0,Next_level_num;
char	Current_level_name[LEVEL_NAME_LEN];

// Global variables describing the player
int	N_players=1;	// Number of players ( >1 means a net game, eh?)
int 	Player_num=0;	// The player number who is on the console.
player	Players[MAX_PLAYERS+4];	// Misc player info
ranking Ranking; // Ranking system mod variables.
parTime ParTime; // Par time algorithm variables.
restartLevel RestartLevel;
obj_position	Player_init[MAX_PLAYERS];

// Global variables telling what sort of game we have
int NumNetPlayerPositions = -1;

extern fix ThisLevelTime;

// Extern from game.c to fix a bug in the cockpit!

extern int Last_level_path_created;

//	HUD_clear_messages external, declared in gauges.h
#ifndef _GAUGES_H
extern void HUD_clear_messages(); // From hud.c
#endif

//	Extra prototypes declared for the sake of LINT
void init_player_stats_new_ship(ubyte pnum);
void copy_defaults_to_robot_all(void);

int	Do_appearance_effect=0;

extern int Rear_view;

int	First_secret_visit = 1;

extern int descent_critical_error;

//--------------------------------------------------------------------
void verify_console_object()
{
	Assert( Player_num > -1 );
	Assert( Players[Player_num].objnum > -1 );
	ConsoleObject = &Objects[Players[Player_num].objnum];
	Assert( ConsoleObject->type==OBJ_PLAYER || (is_observer() && Player_num == OBSERVER_PLAYER_ID) );
	Assert( ConsoleObject->id==Player_num );
}

int count_number_of_robots()
{
	int robot_count;
	int i;

	robot_count = 0;
	for (i=0;i<=Highest_object_index;i++) {
		if (Objects[i].type == OBJ_ROBOT)
			robot_count++;
	}

	return robot_count;
}


int count_number_of_hostages()
{
	int count;
	int i;

	count = 0;
	for (i=0;i<=Highest_object_index;i++) {
		if (Objects[i].type == OBJ_HOSTAGE)
			count++;
	}

	return count;
}

//added 10/12/95: delete buddy bot if coop game.  Probably doesn't really belong here. -MT
void
gameseq_init_network_players()
{
	int i,k,j;

	// Initialize network player start locations and object numbers

	ConsoleObject = &Objects[0];
	k = 0;
	j = 0;
	for (i=0;i<=Highest_object_index;i++) {

		if (( Objects[i].type==OBJ_PLAYER )	|| (Objects[i].type == OBJ_GHOST) || (Objects[i].type == OBJ_COOP))
		{
			if ( (!(Game_mode & GM_MULTI_COOP) && ((Objects[i].type == OBJ_PLAYER)||(Objects[i].type==OBJ_GHOST))) ||
	           ((Game_mode & GM_MULTI_COOP) && ((j == 0) || ( Objects[i].type==OBJ_COOP ))) )
			{
				Objects[i].type=OBJ_PLAYER;
				Player_init[k].pos = Objects[i].pos;
				Player_init[k].orient = Objects[i].orient;
				Player_init[k].segnum = Objects[i].segnum;
				Players[k].objnum = i;
				Objects[i].id = k;
				k++;
			}
			else
				obj_delete(i);
			j++;
		}

		if ((Objects[i].type==OBJ_ROBOT) && (Robot_info[Objects[i].id].companion) && (Game_mode & GM_MULTI))
			obj_delete(i);		//kill the buddy in netgames

	}
	NumNetPlayerPositions = k;

	if (Game_mode & GM_MULTI) {
		// Ensure we have 8 starting locations, even if there aren't 8 in the file.  This makes observer mode work in all levels.
		for (; k < MAX_PLAYERS; k++) {
			i = obj_allocate();

			Objects[i].type = OBJ_PLAYER;

			Player_init[k].pos = Objects[k % NumNetPlayerPositions].pos;
			Player_init[k].orient = Objects[k % NumNetPlayerPositions].orient;
			Player_init[k].segnum = Objects[k % NumNetPlayerPositions].segnum;
			Players[k].objnum = i;
			Objects[i].id = k;

			Objects[i].attached_obj = -1;
		}

		NumNetPlayerPositions = MAX_PLAYERS;
	}
}

void gameseq_remove_unused_players()
{
	int i;

	// 'Remove' the unused players

#ifdef NETWORK
	if (Game_mode & GM_MULTI)
	{
		for (i=0; i < NumNetPlayerPositions; i++)
		{
			if ((i == 0 && Netgame.host_is_obs) || (!Players[i].connected) || (i >= N_players))
			{
				multi_make_player_ghost(i);
			}
		}
	}
	else
#endif
	{		// Note link to above if!!!
		for (i=1; i < NumNetPlayerPositions; i++)
		{
			obj_delete(Players[i].objnum);
		}
	}
}

fix StartingShields=INITIAL_SHIELDS;

// Setup player for new game
void init_player_stats_game(ubyte pnum)
{
	Players[pnum].score = 0;
	Players[pnum].last_score = 0;
	Players[pnum].lives = INITIAL_LIVES;
	Players[pnum].level = 1;
	Players[pnum].time_level = 0;
	Players[pnum].time_total = 0;
	Players[pnum].hours_level = 0;
	Players[pnum].hours_total = 0;
	Players[pnum].killer_objnum = -1;
	Players[pnum].net_killed_total = 0;
	Players[pnum].net_kills_total = 0;
	Players[pnum].num_kills_level = 0;
	Players[pnum].num_kills_total = 0;
	Players[pnum].num_robots_level = 0;
	Players[pnum].num_robots_total = 0;
	Players[pnum].KillGoalCount = 0;
	Players[pnum].hostages_rescued_total = 0;
	Players[pnum].hostages_level = 0;
	Players[pnum].hostages_total = 0;
	Players[pnum].laser_level = 0;
	Players[pnum].flags = 0;
	Players[pnum].shields_delta = 0;
	Players[pnum].shields_time = 0;

	init_player_stats_new_ship(pnum);

	if (pnum == Player_num)
		First_secret_visit = 1;

	if (pnum == Player_num && Game_mode & GM_MULTI)
		multi_send_ship_status();
}

void init_ammo_and_energy(void)
{
	if (Players[Player_num].energy < INITIAL_ENERGY)
		Players[Player_num].energy = INITIAL_ENERGY;
	if (Players[Player_num].shields < StartingShields)
		Players[Player_num].shields = StartingShields;

//	for (i=0; i<MAX_PRIMARY_WEAPONS; i++)
//		if (Players[Player_num].primary_ammo[i] < Default_primary_ammo_level[i])
//			Players[Player_num].primary_ammo[i] = Default_primary_ammo_level[i];

//	for (i=0; i<MAX_SECONDARY_WEAPONS; i++)
//		if (Players[Player_num].secondary_ammo[i] < Default_secondary_ammo_level[i])
//			Players[Player_num].secondary_ammo[i] = Default_secondary_ammo_level[i];
	if (Players[Player_num].secondary_ammo[0] < 2 + NDL - Difficulty_level)
		Players[Player_num].secondary_ammo[0] = 2 + NDL - Difficulty_level;
}

extern	ubyte	Last_afterburner_state;

// Setup player for new level (After completion of previous level)
void init_player_stats_level(int secret_flag)
{
	// int	i;

	Players[Player_num].last_score = Players[Player_num].score;

	Players[Player_num].level = Current_level_num;

#ifdef NETWORK
	if (!Network_rejoined) {
#endif
		Players[Player_num].time_level = 0;
		Players[Player_num].hours_level = 0;
#ifdef NETWORK
	}
#endif

	Players[Player_num].killer_objnum = -1;

	Players[Player_num].num_kills_level = 0;
	Players[Player_num].num_robots_level = count_number_of_robots();
	Players[Player_num].num_robots_total += Players[Player_num].num_robots_level;

	Players[Player_num].hostages_level = count_number_of_hostages();
	Players[Player_num].hostages_total += Players[Player_num].hostages_level;
	Players[Player_num].hostages_on_board = 0;

	if (!secret_flag) {
		init_ammo_and_energy();

		Players[Player_num].flags &= (~KEY_BLUE);
		Players[Player_num].flags &= (~KEY_RED);
		Players[Player_num].flags &= (~KEY_GOLD);

		Players[Player_num].flags &=   ~(PLAYER_FLAGS_INVULNERABLE |
													PLAYER_FLAGS_CLOAKED |
													PLAYER_FLAGS_MAP_ALL);

		Players[Player_num].cloak_time = 0;
		Players[Player_num].invulnerable_time = 0;

		if ((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP))
			Players[Player_num].flags |= (KEY_BLUE | KEY_RED | KEY_GOLD);
	}

	Player_is_dead = 0; // Added by RH
	Dead_player_camera = NULL;
	Players[Player_num].homing_object_dist = -F1_0; // Added by RH

	// properly init these cursed globals
	Next_flare_fire_time = Last_laser_fired_time = Next_laser_fire_time = Next_missile_fire_time = GameTime64;

	Controls.afterburner_state = 0;
	Last_afterburner_state = 0;

	digi_kill_sound_linked_to_object(Players[Player_num].objnum);

	init_gauges();

	Missile_viewer = NULL;

	if (Game_mode & GM_MULTI)
		multi_send_ship_status();
}

extern	void init_ai_for_ship(void);

// Setup player for a brand-new ship
void init_player_stats_new_ship(ubyte pnum)
{
	int i;

	if (pnum == Player_num)
	{
		if (Newdemo_state == ND_STATE_RECORDING)
		{
			newdemo_record_laser_level(Players[Player_num].laser_level, 0);
			newdemo_record_player_weapon(0, 0);
			newdemo_record_player_weapon(1, 0);
		}
		Players[Player_num].primary_weapon = 0;
		Players[Player_num].secondary_weapon = 0;
		for (i=0; i<MAX_PRIMARY_WEAPONS; i++)
			Primary_last_was_super[i] = 0;
		for (i=1; i<MAX_SECONDARY_WEAPONS; i++)
			Secondary_last_was_super[i] = 0;
		dead_player_end(); //player no longer dead
		Player_is_dead = 0;
		Player_exploded = 0;
		Player_eggs_dropped = 0;

		int delete_camera = 1; 
#ifdef NETWORK	
		if ((Game_mode & GM_MULTI) && (Netgame.SpawnStyle == SPAWN_STYLE_PREVIEW)) {	
			delete_camera = 0; 
		}	
#endif
		if(delete_camera)
			Dead_player_camera = 0;

		Global_laser_firing_count=0;
		Players[pnum].afterburner_charge = 0;
		Controls.afterburner_state = 0;
		Last_afterburner_state = 0;
		Missile_viewer=NULL; //reset missile camera if out there
		Missile_viewer_sig=-1;
		init_ai_for_ship();
	}

	Players[pnum].energy = INITIAL_ENERGY;
	Players[pnum].shields = StartingShields;
	Players[pnum].shields_certain = 1;
	Players[pnum].laser_level = 0;
	Players[pnum].killer_objnum = -1;
	Players[pnum].hostages_on_board = 0;
	for (i=0; i<MAX_PRIMARY_WEAPONS; i++)
		Players[pnum].primary_ammo[i] = 0;
	for (i=1; i<MAX_SECONDARY_WEAPONS; i++)
		Players[pnum].secondary_ammo[i] = 0;
	Players[pnum].secondary_ammo[0] = 2 + NDL - Difficulty_level;
	Players[pnum].primary_weapon_flags = HAS_LASER_FLAG;
	Players[pnum].secondary_weapon_flags = HAS_CONCUSSION_FLAG;
	Players[pnum].flags &= ~( PLAYER_FLAGS_QUAD_LASERS | PLAYER_FLAGS_AFTERBURNER | PLAYER_FLAGS_CLOAKED | PLAYER_FLAGS_INVULNERABLE | PLAYER_FLAGS_MAP_ALL | PLAYER_FLAGS_CONVERTER | PLAYER_FLAGS_AMMO_RACK | PLAYER_FLAGS_HEADLIGHT | PLAYER_FLAGS_HEADLIGHT_ON | PLAYER_FLAGS_FLAG);
	Players[pnum].cloak_time = 0;
	Players[pnum].invulnerable_time = 0;
	Players[pnum].homing_object_dist = -F1_0; // Added by RH
	RespawningConcussions[pnum] = 0; 	
	VulcanAmmoBoxesOnBoard[pnum] = 0; 		
	VulcanBoxAmmo[pnum] = 0;

#ifdef NETWORK
	if(Game_mode & GM_MULTI && Netgame.BornWithBurner && !is_observer()) {
		Players[pnum].flags |= PLAYER_FLAGS_AFTERBURNER;
		if (pnum == Player_num)
			Players[pnum].afterburner_charge = f1_0;
	}
#endif
	digi_kill_sound_linked_to_object(Players[pnum].objnum);

	if (pnum == Player_num && Game_mode & GM_MULTI)
		multi_send_ship_status();
}

extern void init_stuck_objects(void);

#ifdef EDITOR

extern int Slide_segs_computed;

extern int game_handler(window *wind, d_event *event, void *data);

//reset stuff so game is semi-normal when playing from editor
void editor_reset_stuff_on_level()
{
	gameseq_init_network_players();
	init_player_stats_level(0);
	Viewer = ConsoleObject;
	ConsoleObject = Viewer = &Objects[Players[Player_num].objnum];
	ConsoleObject->id=Player_num;
	ConsoleObject->control_type = CT_FLYING;
	ConsoleObject->movement_type = MT_PHYSICS;
	Game_suspended = 0;
	verify_console_object();
	Control_center_destroyed = 0;
	if (Newdemo_state != ND_STATE_PLAYBACK)
		gameseq_remove_unused_players();
	init_cockpit();
	init_robots_for_level();
	init_ai_objects();
	init_morphs();
	init_all_matcens();
	init_player_stats_new_ship(Player_num);
	init_controlcen_for_level();
	automap_clear_visited();
	init_stuck_objects();
	init_thief_for_level();

	Slide_segs_computed = 0;
	if (!Game_wind)
		Game_wind = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, game_handler, NULL);
}
#endif

//do whatever needs to be done when a player dies in multiplayer

void DoGameOver()
{
//	nm_messagebox( TXT_GAME_OVER, 1, TXT_OK, "" );

	if (PLAYING_BUILTIN_MISSION)
		scores_maybe_add_player(0);

	if (Game_wind)
		window_close(Game_wind);		// Exit out of game loop
}

//update various information about the player
void update_player_stats()
{
	Players[Player_num].time_level += FrameTime;	//the never-ending march of time...
	if (Current_level_num < 0 && !Ranking.freezeTimer)
		Ranking.secretlevel_time += FrameTime;
	if ( Players[Player_num].time_level > i2f(3600) )	{
		Players[Player_num].time_level -= i2f(3600);
		Players[Player_num].hours_level++;
	}

	Players[Player_num].time_total += FrameTime;	//the never-ending march of time...
	if ( Players[Player_num].time_total > i2f(3600) )	{
		Players[Player_num].time_total -= i2f(3600);
		Players[Player_num].hours_total++;
	}
}

//hack to not start object when loading level
extern int Dont_start_sound_objects;

//go through this level and start any eclip sounds
void set_sound_sources()
{
	int segnum,sidenum;
	segment *seg;

	digi_init_sounds();		//clear old sounds

	Dont_start_sound_objects = 1;

	for (seg=&Segments[0],segnum=0;segnum<=Highest_segment_index;seg++,segnum++)
		for (sidenum=0;sidenum<MAX_SIDES_PER_SEGMENT;sidenum++) {
			int tm,ec,sn;

			if (WALL_IS_DOORWAY(seg,sidenum) & WID_RENDER_FLAG)
				if ((((tm=seg->sides[sidenum].tmap_num2) != 0) && ((ec=TmapInfo[tm&0x3fff].eclip_num)!=-1)) || ((ec=TmapInfo[seg->sides[sidenum].tmap_num].eclip_num)!=-1))
					if ((sn=Effects[ec].sound_num)!=-1) {
						vms_vector pnt;
						int csegnum = seg->children[sidenum];

						//check for sound on other side of wall.  Don't add on
						//both walls if sound travels through wall.  If sound
						//does travel through wall, add sound for lower-numbered
						//segment.

						if (IS_CHILD(csegnum) && csegnum < segnum) {
							if (WALL_IS_DOORWAY(seg, sidenum) & (WID_FLY_FLAG+WID_RENDPAST_FLAG)) {
								segment *csegp;
								int csidenum;

								csegp = &Segments[seg->children[sidenum]];
								csidenum = find_connect_side(seg, csegp);

								if (csegp->sides[csidenum].tmap_num2 == seg->sides[sidenum].tmap_num2)
									continue;		//skip this one
							}
						}

						compute_center_point_on_side(&pnt,seg,sidenum);
						digi_link_sound_to_pos(sn,segnum,sidenum,&pnt,1, F1_0/2);
					}
		}

	Dont_start_sound_objects = 0;

}


//fix flash_dist=i2f(1);
fix flash_dist=fl2f(.9);

//create flash for player appearance
void create_player_appearance_effect(object *player_obj)
{
	vms_vector pos;
	object *effect_obj;

#ifndef NDEBUG
	{
		int objnum = player_obj-Objects;
		if ( (objnum < 0) || (objnum > Highest_object_index) )
			Int3(); // See Rob, trying to track down weird network bug
	}
#endif

	if (player_obj == Viewer)
		vm_vec_scale_add(&pos, &player_obj->pos, &player_obj->orient.fvec, fixmul(player_obj->size,flash_dist));
	else
		pos = player_obj->pos;

	effect_obj = object_create_explosion(player_obj->segnum, &pos, player_obj->size, VCLIP_PLAYER_APPEARANCE );

	if (effect_obj) {
		effect_obj->orient = player_obj->orient;

		if ( Vclip[VCLIP_PLAYER_APPEARANCE].sound_num > -1 )
			digi_link_sound_to_object( Vclip[VCLIP_PLAYER_APPEARANCE].sound_num, effect_obj-Objects, 0, F1_0);
	}
}

//
// New Game sequencing functions
//

// routine to calculate the checksum of the segments.
void do_checksum_calc(ubyte *b, int len, unsigned int *s1, unsigned int *s2)
{

	while(len--) {
		*s1 += *b++;
		if (*s1 >= 255) *s1 -= 255;
		*s2 += *s1;
	}
}

ushort netmisc_calc_checksum()
{
	int i, j, k;
	unsigned int sum1,sum2;
	short s;
	int t;

	sum1 = sum2 = 0;
	for (i = 0; i < Highest_segment_index + 1; i++) {
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			do_checksum_calc((unsigned char *)&(Segments[i].sides[j].type), 1, &sum1, &sum2);
			do_checksum_calc(&(Segments[i].sides[j].pad), 1, &sum1, &sum2);
			s = INTEL_SHORT(Segments[i].sides[j].wall_num);
			do_checksum_calc((ubyte *)&s, 2, &sum1, &sum2);
			s = INTEL_SHORT(Segments[i].sides[j].tmap_num);
			do_checksum_calc((ubyte *)&s, 2, &sum1, &sum2);
			s = INTEL_SHORT(Segments[i].sides[j].tmap_num2);
			do_checksum_calc((ubyte *)&s, 2, &sum1, &sum2);
			for (k = 0; k < 4; k++) {
				t = INTEL_INT(((int)Segments[i].sides[j].uvls[k].u));
				do_checksum_calc((ubyte *)&t, 4, &sum1, &sum2);
				t = INTEL_INT(((int)Segments[i].sides[j].uvls[k].v));
				do_checksum_calc((ubyte *)&t, 4, &sum1, &sum2);
				t = INTEL_INT(((int)Segments[i].sides[j].uvls[k].l));
				do_checksum_calc((ubyte *)&t, 4, &sum1, &sum2);
			}
			for (k = 0; k < 2; k++) {
				t = INTEL_INT(((int)Segments[i].sides[j].normals[k].x));
				do_checksum_calc((ubyte *)&t, 4, &sum1, &sum2);
				t = INTEL_INT(((int)Segments[i].sides[j].normals[k].y));
				do_checksum_calc((ubyte *)&t, 4, &sum1, &sum2);
				t = INTEL_INT(((int)Segments[i].sides[j].normals[k].z));
				do_checksum_calc((ubyte *)&t, 4, &sum1, &sum2);
			}
		}
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			s = INTEL_SHORT(Segments[i].children[j]);
			do_checksum_calc((ubyte *)&s, 2, &sum1, &sum2);
		}
		for (j = 0; j < MAX_VERTICES_PER_SEGMENT; j++) {
			s = INTEL_SHORT(Segments[i].verts[j]);
			do_checksum_calc((ubyte *)&s, 2, &sum1, &sum2);
		}
		t = INTEL_INT(Segments[i].objects);
		do_checksum_calc((ubyte *)&t, 4, &sum1, &sum2);
	}
	sum2 %= 255;
	return ((sum1<<8)+ sum2);
}

void free_polygon_models();
void load_robot_replacements(char *level_name);
int read_hamfile();
extern int Robot_replacements_loaded;

// load just the hxm file
void load_level_robots(int level_num)
{
	char *level_name;

	Assert(level_num <= Last_level  && level_num >= Last_secret_level  && level_num != 0);

	if (level_num<0)		//secret level
		level_name = Secret_level_names[-level_num-1];
	else					//normal level
		level_name = Level_names[level_num-1];

	if (Robot_replacements_loaded) {
		int load_mission_ham();
		free_polygon_models();
		load_mission_ham();
		Robot_replacements_loaded = 0;
	}
	load_robot_replacements(level_name);
	Robot_replacements_loaded |= multi_change_weapon_info();
}

//load a level off disk. level numbers start at 1.  Secret levels are -1,-2,-3
void LoadLevel(int level_num,int page_in_textures)
{
	char *level_name;
	player save_player;
	int load_ret;

	save_player = Players[Player_num];

	if (Newdemo_state == ND_STATE_PLAYBACK && level_num == 0) {
		// Workaround for Redux bug. We're hoping that the demo was recorded in a one-level
		// mission; otherwise we don't have a good way to guess which one to load.
		level_num = 1;
		nm_messagebox(NULL, 1, TXT_OK, "This demo was recorded in\nan old version of D2X-Redux.\n"
			"Some game information\nmay be missing.");
	}

	Assert(level_num <= Last_level  && level_num >= Last_secret_level  && level_num != 0);

	if (level_num<0)		//secret level
		level_name = Secret_level_names[-level_num-1];
	else					//normal level
		level_name = Level_names[level_num-1];

	gr_set_current_canvas(NULL);
	gr_clear_canvas(BM_XRGB(0, 0, 0));		//so palette switching is less obvious

	load_ret = load_level(level_name);		//actually load the data from disk!

	if (load_ret)
		Error("Couldn't load level file <%s>, error = %d",level_name,load_ret);

	Current_level_num=level_num;

	load_palette(Current_level_palette,1,1);		//don't change screen

	show_boxed_message(TXT_LOADING, 0);
#ifdef RELEASE
	timer_delay(F1_0);
#endif

	load_endlevel_data(level_num);

	if (EMULATING_D1)
		load_d1_bitmap_replacements();
	else
		load_bitmap_replacements(level_name);

	load_level_robots(level_num);

	if ( page_in_textures ) {
		piggy_load_level_data();
#ifdef OGL
		ogl_cache_level_textures();
#endif
	}

#ifdef NETWORK
	my_segments_checksum = netmisc_calc_checksum();

	reset_network_objects();
#endif

	Players[Player_num] = save_player;

	set_sound_sources();

	songs_play_level_song( Current_level_num, 0 );

	gr_palette_load(gr_palette);		//actually load the palette

//	WIN(HideCursorW());
}

//sets up Player_num & ConsoleObject
void InitPlayerObject()
{
	Assert(Player_num>=0 && Player_num<MAX_PLAYERS);

	if (Player_num != 0 )	{
		Players[0] = Players[Player_num];
		Player_num = 0;
	}

	Players[Player_num].objnum = 0;

	ConsoleObject = &Objects[Players[Player_num].objnum];

	ConsoleObject->type				= OBJ_PLAYER;
	ConsoleObject->id					= Player_num;
	ConsoleObject->control_type	= CT_FLYING;
	ConsoleObject->movement_type	= MT_PHYSICS;
}

extern void init_seismic_disturbances(void);

int truncateRanks(int rank)
{
	int newRank = rank;
	if (rank == 2 || rank == 4)
		newRank = 3;
	if (rank == 5 || rank == 7)
		newRank = 6;
	if (rank == 8 || rank == 10)
		newRank = 9;
	if (rank == 11 || rank == 13)
		newRank = 12;
	return newRank;
}

int calculateRank(int level_num, int update_warm_start_status)
{
	int levelHostages = 0;
	int levelPoints = 0;
	double parTime = 0;
	int playerPoints = 0;
	double secondsTaken = 0;
	int playerHostages = 0;
	double hostagePoints = 0;
	double difficulty = 0;
	int deathCount = 0;
	double missedRngSpawn = 0;
	double rankPoints2 = 0;
	char buffer[256];
	char filename[256];
	sprintf(filename, "ranks/%s/%s/level%d.hi", Players[Player_num].callsign, Current_mission->filename, level_num); // Find file for the requested level.
	if (level_num > Current_mission->last_level)
		sprintf(filename, "ranks/%s/%s/levelS%d.hi", Players[Player_num].callsign, Current_mission->filename, level_num - Current_mission->last_level);
	PHYSFS_file* fp = PHYSFS_openRead(filename);
	if (fp == NULL)
		rankPoints2 = -10; // If no data exists, just assume level never played and set rankPoints in the range that returns N/A.
	else {
		PHYSFSX_getsTerminated(fp, buffer); // Fetch level data starting here. If a parameter isn't present in the file, it'll default to 0 in calculation.
		levelHostages = atoi(buffer);
		if (levelHostages < 0) // If level is unplayed, we know because the first value in the file will be -1, which is impossible for the hostage count.
			rankPoints2 = -10;
		else {
			PHYSFSX_getsTerminated(fp, buffer);
			levelPoints = atoi(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			parTime = atof(buffer);
			PHYSFSX_getsTerminated(fp, buffer); // Fetch player data starting here.
			playerPoints = atoi(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			secondsTaken = atof(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			playerHostages = atoi(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			difficulty = atof(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			deathCount = atoi(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			missedRngSpawn = atof(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			if (update_warm_start_status)
				Ranking.warmStart = atoi(buffer);
		}
	}
	PHYSFS_close(fp);
	double maxScore = levelPoints * 3;
	maxScore = (int)maxScore;
	double skillPoints = ceil((playerPoints * (difficulty / 4)));
	double timePoints = (maxScore / 1.5) / pow(2, secondsTaken / parTime);
	if (secondsTaken < parTime)
		timePoints = (maxScore / 2.4) * (1 - (secondsTaken / parTime) * 0.2);
	if (!parTime)
		timePoints = 0;
	timePoints = (int)timePoints;
	hostagePoints = playerHostages * 2500 * ((difficulty + 8) / 12);
	if (playerHostages == levelHostages)
		hostagePoints *= 3;
	hostagePoints = round(hostagePoints); // Round this because I got 24999 hostage bonus once.
	double score = playerPoints + skillPoints + timePoints + missedRngSpawn + hostagePoints;
	maxScore += levelHostages * 7500;
	double deathPoints;
	if (deathCount == -1)
		deathPoints = ceil(maxScore / 12); // Round up instead of down for no damage bonus so score can't fall a point short and miss a rank.
	else
		deathPoints = -(maxScore * 0.4 - maxScore * (0.4 / pow(2, deathCount / (parTime / 360))));
	score += deathPoints;
	if (rankPoints2 > -5)
		if (maxScore)
			rankPoints2 = (score / maxScore) * 12;
		else
			rankPoints2 = 13;
	Ranking.calculatedScore = score;
	if (rankPoints2 < -5)
		Ranking.rank = 0;
	if (rankPoints2 > -5)
		Ranking.rank = 1;
	if (rankPoints2 >= 0)
		Ranking.rank = (int)rankPoints2 + 2;
	if (Ranking.rank > 15)
		Ranking.rank = 15;
	if (!PlayerCfg.RankShowPlusMinus)
		Ranking.rank = truncateRanks(Ranking.rank);
	return Ranking.rank;
}

void getLevelNameFromRankFile(int level_num, char* buffer)
{
	char filename[256];
	sprintf(filename, "ranks/%s/%s/level%d.hi", Players[Player_num].callsign, Current_mission->filename, level_num); // Find file for the requested level.
	if (level_num > Current_mission->last_level)
		sprintf(filename, "ranks/%s/%s/levelS%d.hi", Players[Player_num].callsign, Current_mission->filename, level_num - Current_mission->last_level);
	PHYSFS_file* fp = PHYSFS_openRead(filename);
	if (fp == NULL)
		sprintf(buffer, "???");
	else {
		for (int i = 0; i < 10; i++)
			PHYSFSX_getsTerminated(fp, buffer); // Get a line ten times because the tenth line has the level name.
	}
	PHYSFS_close(fp);
}

//starts a new game on the given level
void StartNewGame(int start_level)
{
	RestartLevel.restarts = 0;
	Ranking.warmStart = 0;
	Ranking.cheated = 0;
	Ranking.secret_hostages_on_board = 0; // If we don't set this to zero here, then getting hostages in a secret level, quitting out and starting a new game will cause the next level to never give full rescue bonus.
	PHYSFS_file* fp;
	char filename[256];
	int i = 1;
	char buffer[256];
	sprintf(buffer, "ranks/%s/%s", Players[Player_num].callsign, Current_mission->filename);
	PHYSFS_mkdir(buffer);
	while (i <= Current_mission->last_level - Current_mission->last_secret_level) {
		sprintf(filename, "ranks/%s/%s/level%d.hi", Players[Player_num].callsign, Current_mission->filename, i);
		if (i > Current_mission->last_level)
			sprintf(filename, "ranks/%s/%s/levelS%d.hi", Players[Player_num].callsign, Current_mission->filename, i - Current_mission->last_level);
		fp = PHYSFS_openRead(filename);
		if (fp == NULL) { // If this level's rank data file doesn't exist, create it now so it can be written to on the rank screen.
			fp = PHYSFS_openWrite(filename);
			PHYSFSX_printf(fp, "-1\n");
			PHYSFSX_printf(fp, "\n");
			PHYSFSX_printf(fp, "\n");
			PHYSFSX_printf(fp, "\n");
			PHYSFSX_printf(fp, "\n");
			PHYSFSX_printf(fp, "\n");
			PHYSFSX_printf(fp, "\n");
			PHYSFSX_printf(fp, "\n");
			PHYSFSX_printf(fp, "\n");
			PHYSFSX_printf(fp, "???");
		}
		PHYSFS_close(fp);
		i++;
	}
	
	state_quick_item = -1;	// for first blind save, pick slot to save in

	Game_mode = GM_NORMAL;

	Next_level_num = 0;

	InitPlayerObject();				//make sure player's object set up

	init_player_stats_game(Player_num);		//clear all stats

	N_players = 1;

	RestartLevel.updateRestartStuff = 1; // So the player doesn't restart with 0 in every field.
	if (start_level < 0)
		StartNewLevelSecret(start_level, 0);
	else
		StartNewLevel(start_level);

	Players[Player_num].starting_level = start_level;		// Mark where they started

	game_disable_cheats();

	init_seismic_disturbances();
}

// Arne's code, except for the image pos/size and fromBestRanksButton stuff.
int endlevel_current_rank;
extern grs_bitmap nm_background1;
grs_bitmap transparent;
int endlevel_handler(newmenu* menu, d_event* event, void* userdata) {
	if (!((Current_level_num > 0 && Ranking.quickload) || (Current_level_num < 0 && Ranking.secretQuickload))) {
		switch (event->type) {
		case EVENT_WINDOW_DRAW:
			gr_palette_load(gr_palette);
			show_fullscr(&nm_background1);
			grs_bitmap* bm = RankBitmaps[endlevel_current_rank];
			int x = grd_curscreen->sc_w * 0.4;
			int y = grd_curscreen->sc_h * 0.725;
			int h = grd_curscreen->sc_h * 0.1125;
			if (!endlevel_current_rank || endlevel_current_rank == 14)
				h *= 1.0806; // Make E-rank bigger to compensate for the tilt, and X too because it's just small lol.
			int w = 3 * h;
			if (!endlevel_current_rank || endlevel_current_rank == 2 || endlevel_current_rank == 5 || endlevel_current_rank == 8 || endlevel_current_rank == 11 || endlevel_current_rank == 13 || endlevel_current_rank == 14)
				x += w * 0.138888889; // Push the image right if it doesn't have a plus or minus, otherwise it won't be centered.
			ogl_ubitmapm_cs(x, y, w, h, bm, -1, F1_0);
			grs_bitmap oldbackground = nm_background1;
			nm_background1 = transparent;
			int newmenu_draw(window *wind, newmenu *menu);
			int ret = newmenu_draw(newmenu_get_window(menu), menu);
			transparent = nm_background1;
			nm_background1 = oldbackground;
			return ret;
		}
	}
	if (Ranking.fromBestRanksButton == 1) {
		switch (event->type) {
		case EVENT_NEWMENU_SELECTED:
			if (Ranking.startingLevel > Current_mission->last_level) {
				nm_messagebox(NULL, 1, "Ok", "Can't start on secret level!\nTry saving before teleporter.");
				return 1;
			}
			else {
				if (!do_difficulty_menu())
					return 1;
				StartNewGame(Ranking.startingLevel);
			}
			break;
		}
	}
	return 0;
}

//	-----------------------------------------------------------------------------
//	Does the bonus scoring.
//	Call with dead_flag = 1 if player died, but deserves some portion of bonus (only skill points), anyway.
void DoEndLevelScoreGlitz(int network)
{
	if (Ranking.level_time == 0)
		Ranking.level_time = (Players[Player_num].hours_level * 3600) + ((double)Players[Player_num].time_level / 65536); // Failsafe for if this isn't updated.
	RestartLevel.restartsCache = RestartLevel.restarts;
	RestartLevel.restarts = 0;
	RestartLevel.isResults = 1;
	
	int level_points, skill_points, death_points, shield_points, energy_points, time_points, hostage_points, all_hostage_points, endgame_points;
	double skill_points2, missed_rng_drops, hostage_points2;
	char	all_hostage_text[64];
	char	endgame_text[64];
#define N_GLITZITEMS 12
	char				m_str[N_GLITZITEMS][31];
	newmenu_item	m[N_GLITZITEMS + 1];
	int				i, c;
	char				title[128];
	int				is_last_level;
	int				mine_level;

	//	Compute level player is on, deal with secret levels (negative numbers)
	mine_level = Players[Player_num].level;
	if (mine_level < 0)
		mine_level *= -(Last_level / N_secret_levels);

	level_points = Players[Player_num].score - Players[Player_num].last_score;
	Ranking.rankScore = level_points - Ranking.excludePoints;

	skill_points = 0, skill_points2 = 0;
	if (Difficulty_level == 1)
		skill_points2 = Ranking.rankScore / 4;
	if (Difficulty_level > 1) {
		skill_points = level_points * (Difficulty_level / 4);
		skill_points2 = Ranking.rankScore * ((double)Difficulty_level / 4);
	}
	skill_points -= skill_points % 100;
	skill_points2 = ceil(skill_points2); // Round this up so you can't theoretically miss a rank by a point on levels or difficulties with weird scoring.

	shield_points = f2i(Players[Player_num].shields) * 5 * mine_level;
	shield_points -= shield_points % 50;
	energy_points = f2i(Players[Player_num].energy) * 2 * mine_level;
	energy_points -= energy_points % 50;
	time_points = ((Ranking.maxScore - Players[Player_num].hostages_level * 7500) / 1.5) / pow(2, Ranking.level_time / Ranking.parTime);
	if (Ranking.level_time < Ranking.parTime)
		time_points = ((Ranking.maxScore - Players[Player_num].hostages_level * 7500) / 2.4) * (1 - (Ranking.level_time / Ranking.parTime) * 0.2);
	hostage_points = Players[Player_num].hostages_on_board * 500 * (Difficulty_level + 1);
	hostage_points2 = Players[Player_num].hostages_on_board * 2500 * (((double)Difficulty_level + 8) / 12);

	all_hostage_text[0] = 0;
	endgame_text[0] = 0;
	all_hostage_points = 0;

	if (Players[Player_num].hostages_on_board == Players[Player_num].hostages_level)
		all_hostage_points = Players[Player_num].hostages_on_board * 1000 * (Difficulty_level + 1);

	if (!cheats.enabled && !(Game_mode & GM_MULTI) && (Players[Player_num].lives) && (Current_level_num == Last_level)) {		//player has finished the game!
		endgame_points = Players[Player_num].lives * 10000;
		is_last_level = 1;
	}
	else
		endgame_points = is_last_level = 0;
	if (!cheats.enabled)
		add_bonus_points_to_score(shield_points + energy_points + skill_points + hostage_points + all_hostage_points + endgame_points);
	if (Players[Player_num].hostages_on_board == Players[Player_num].hostages_level)
		hostage_points2 *= 3;
	hostage_points2 = round(hostage_points2); // Round this because I got 24999 hostage bonus once.
	death_points = -(Ranking.maxScore * 0.4 - Ranking.maxScore * (0.4 / pow(2, Ranking.deathCount / (Ranking.parTime / 360))));
	if (Ranking.noDamage)
		death_points = ceil(Ranking.maxScore / 12); // Round up instead of down for no damage bonus so score can't fall a point short and miss a rank.
	Ranking.missedRngSpawn *= ((double)Difficulty_level + 4) / 4; // Add would-be skill bonus into the penalty for ignored random offspring. This makes ignoring them on high difficulties more consistent and punishing.
	missed_rng_drops = Ranking.missedRngSpawn;
	Ranking.rankScore += skill_points2 + time_points + hostage_points2 + death_points + missed_rng_drops;

	int minutes = Ranking.level_time / 60;
	double seconds = Ranking.level_time - minutes * 60;
	int parMinutes = Ranking.parTime / 60;
	double parSeconds = Ranking.parTime - parMinutes * 60;
	char* diffname = 0;
	char timeText[256];
	char parTimeString[256];
	c = 0;
	if (Difficulty_level == 0)
		diffname = "Trainee";
	if (Difficulty_level == 1)
		diffname = "Rookie";
	if (Difficulty_level == 2)
		diffname = "Hotshot";
	if (Difficulty_level == 3)
		diffname = "Ace";
	if (Difficulty_level == 4)
		diffname = "Insane";
	if (Ranking.quickload == 0) { // Only print the modded result screen if the player didn't quickload. Print the vanilla one otherwise, because quickloading into a level causes missing crucial mod info.
		if (seconds < 10 || seconds == 60)
			sprintf(timeText, "%i:0%.3f", minutes, seconds);
		else
			sprintf(timeText, "%i:%.3f", minutes, seconds);
		if (parSeconds < 10 || parSeconds == 60)
			sprintf(parTimeString, "%i:0%.0f", parMinutes, parSeconds);
		else
			sprintf(parTimeString, "%i:%.0f", parMinutes, parSeconds);
		sprintf(m_str[c++], "Level score\t%.0f", level_points - Ranking.excludePoints);
		sprintf(m_str[c++], "Time: %s/%s\t%i", timeText, parTimeString, time_points);
		sprintf(m_str[c++], "Hostages: %i/%i\t%.0f", Players[Player_num].hostages_on_board, Players[Player_num].hostages_level, hostage_points2);
		sprintf(m_str[c++], "Skill: %s\t%.0f", diffname, skill_points2);
		if (Ranking.noDamage)
			sprintf(m_str[c++], "No damage\t%i", death_points);
		else
			sprintf(m_str[c++], "Deaths: %.0f\t%i", Ranking.deathCount, death_points);
		if (Ranking.missedRngSpawn < 0)
			sprintf(m_str[c++], "Missed RNG spawn: \t%.0f\n", Ranking.missedRngSpawn);
		else
			strcpy(m_str[c++], "\n");
		if (Ranking.warmStart)
			sprintf(m_str[c++], "Total score\t* %0.0f", Ranking.rankScore);
		else
			sprintf(m_str[c++], "Total score\t%0.0f", Ranking.rankScore);

		double rankPoints = (Ranking.rankScore / Ranking.maxScore) * 12;
		if (Ranking.maxScore == 0)
			rankPoints = 13;
		int rank = 0;
		if (rankPoints >= 0)
			rank = (int)rankPoints + 1;
		if (rank > 14)
			rank = 14;
		if (!PlayerCfg.RankShowPlusMinus)
			rank = truncateRanks(rank + 1) - 1;
		endlevel_current_rank = rank;
		if (Ranking.cheated) {
			strcpy(m_str[c++], "\n\n\n");
			sprintf(m_str[c++], "   Cheated, score not saved!   "); // Don't show vanilla score when cheating, as players already know it'll always be zero.
		}
		else {
			PHYSFS_File* fp;
			PHYSFS_File* temp;
			char filename[256];
			char temp_filename[256];
			sprintf(filename, "ranks/%s/%s/level%i.hi", Players[Player_num].callsign, Current_mission->filename, Current_level_num);
			if (Current_level_num < 0)
				sprintf(filename, "ranks/%s/%s/levelS%i.hi", Players[Player_num].callsign, Current_mission->filename, Current_level_num * -1);
			sprintf(temp_filename, "ranks/%s/%s/temp.hi", Players[Player_num].callsign, Current_mission->filename);
			fp = PHYSFS_openRead(filename);
			if (fp != NULL) {
				calculateRank(Current_level_num, 0);
				if (Ranking.rankScore > Ranking.calculatedScore || Ranking.rank == 0) {
					time_t timeOfScore = time(NULL);
					temp = PHYSFS_openWrite(temp_filename);
					PHYSFSX_printf(temp, "%i\n", Players[Player_num].hostages_level);
					PHYSFSX_printf(temp, "%.0f\n", (Ranking.maxScore - Players[Player_num].hostages_level * 7500) / 3);
					PHYSFSX_printf(temp, "%.0f\n", Ranking.parTime);
					PHYSFSX_printf(temp, "%.0f\n", level_points - Ranking.excludePoints);
					PHYSFSX_printf(temp, "%.3f\n", Ranking.level_time);
					PHYSFSX_printf(temp, "%i\n", Players[Player_num].hostages_on_board);
					PHYSFSX_printf(temp, "%i\n", Difficulty_level);
					if (Ranking.noDamage)
						PHYSFSX_printf(temp, "%i\n", -1);
					else
						PHYSFSX_printf(temp, "%.0f\n", Ranking.deathCount);
					PHYSFSX_printf(temp, "%.0f\n", Ranking.missedRngSpawn);
					PHYSFSX_printf(temp, "%s\n", Current_level_name);
					PHYSFSX_printf(temp, "%s", ctime(&timeOfScore));
					PHYSFSX_printf(temp, "%i", Ranking.warmStart);
					PHYSFS_close(temp);
					PHYSFS_close(fp);
					PHYSFS_delete(filename);
					PHYSFSX_rename(temp_filename, filename);
					if (Ranking.rank > 0)
						sprintf(m_str[c++], "New record!");
				}
				PHYSFS_close(fp);
				if (Ranking.rankScore > Ranking.calculatedScore && Ranking.rank > 0)
					strcpy(m_str[c++], "\n\n");
				else
					strcpy(m_str[c++], "\n\n\n");
			}
			else {
				sprintf(m_str[c++], "Saving error. Replay level.\n\n\n");
				PHYSFS_close(fp);
			}
			sprintf(m_str[c++], "Vanilla score:\t      %i", Players[Player_num].score); // Show players' base game score at the end of each level, so they can still compete with it when using the mod.
		}
	}
	else {
		if (cheats.enabled) { // Zero out all the bonuses when cheating while the mod's off.
			shield_points = 0;
			energy_points = 0;
			hostage_points = 0;
			skill_points = 0;
			all_hostage_points = 0;
			endgame_points = 0;
		}
		sprintf(m_str[c++], "%s%i", TXT_SHIELD_BONUS, shield_points);
		sprintf(m_str[c++], "%s%i", TXT_ENERGY_BONUS, energy_points);
		sprintf(m_str[c++], "%s%i", TXT_HOSTAGE_BONUS, hostage_points);
		sprintf(m_str[c++], "%s%i\n", TXT_SKILL_BONUS, skill_points);
		if (!cheats.enabled && (Players[Player_num].hostages_on_board == Players[Player_num].hostages_level))
			sprintf(all_hostage_text, "%s%i\n", TXT_FULL_RESCUE_BONUS, all_hostage_points);
		if (!cheats.enabled && !(Game_mode & GM_MULTI) && (Players[Player_num].lives) && (Current_level_num == Last_level))		//player has finished the game!
			sprintf(endgame_text, "%s%i\n", TXT_SHIP_BONUS, endgame_points);
		sprintf(m_str[c++], "%s%i\n", TXT_TOTAL_BONUS, shield_points + energy_points + hostage_points + skill_points + all_hostage_points + endgame_points);
		sprintf(m_str[c++], "%s%i", TXT_TOTAL_SCORE, Players[Player_num].score);
	}

		for (i = 0; i < c; i++) {
			m[i].type = NM_TYPE_TEXT;
			m[i].text = m_str[i];
		}

		// m[c].type = NM_TYPE_MENU;	m[c++].text = "Ok";

		if (!Ranking.quickload) 
			sprintf(title, "%s %i %s\n%s %s \nR restarts level", TXT_LEVEL, Current_level_num, TXT_COMPLETE, Current_level_name, TXT_DESTROYED);
		else
			sprintf(title, "%s %i %s\n%s %s \n", TXT_LEVEL, Current_level_num, TXT_COMPLETE, Current_level_name, TXT_DESTROYED);

		Assert(c <= N_GLITZITEMS);

		gr_init_bitmap_alloc(&transparent, BM_LINEAR, 0, 0, 1, 1, 1);
		transparent.bm_data[0] = 255;
		transparent.bm_flags |= BM_FLAG_TRANSPARENT;

#ifdef NETWORK
		if (network && (Game_mode & GM_NETWORK))
			newmenu_do2(NULL, title, c, m, multi_endlevel_poll1, NULL, 0, STARS_BACKGROUND);
		else
#endif
			// NOTE LINK TO ABOVE!!!
			newmenu_do2(NULL, title, c, m, endlevel_handler, NULL, 0, STARS_BACKGROUND);
		newmenu_free_background();
		
		gr_free_bitmap_data(&transparent);
}

void DoEndSecretLevelScoreGlitz()
{
	int level_points, skill_points, death_points, time_points;
	double missed_rng_drops, hostage_points;
#define N_GLITZITEMS 12
	char				m_str[N_GLITZITEMS][31];
	newmenu_item	m[N_GLITZITEMS + 1];
	int				i, c;
	char				title[128];
	int				is_last_level = 0;
	Ranking.secretlevel_time = Ranking.secretlevel_time / 65536;

	//	Compute level player is on, deal with secret levels (negative numbers)

	level_points = Players[Player_num].score - Ranking.secretlast_score;
	Ranking.secretRankScore = level_points - Ranking.secretExcludePoints;

	skill_points = ceil((Ranking.secretRankScore * ((double)Difficulty_level / 4))); // Round this up so you can't theoretically miss a rank by a point on levels or difficulties with weird scoring.

	time_points = ((Ranking.secretMaxScore - Ranking.hostages_secret_level * 7500) / 1.5) / pow(2, Ranking.secretlevel_time / Ranking.secretParTime);
	if (Ranking.secretlevel_time < Ranking.secretParTime)
		time_points = ((Ranking.secretMaxScore - Ranking.hostages_secret_level * 7500) / 2.4) * (1 - (Ranking.secretlevel_time / Ranking.secretParTime) * 0.2);
	hostage_points = Ranking.secret_hostages_on_board * 2500 * (((double)Difficulty_level + 8) / 12);
	if (Ranking.secret_hostages_on_board == Ranking.hostages_secret_level)
		hostage_points *= 3;
	hostage_points = round(hostage_points); // Round this because I got 24999 hostage bonus once.
	death_points = -(Ranking.secretMaxScore * 0.4 - Ranking.secretMaxScore * (0.4 / pow(2, Ranking.secretDeathCount / (Ranking.secretParTime / 360))));
	if (Ranking.secretNoDamage)
		death_points = ceil(Ranking.secretMaxScore / 12); // Round up instead of down for no damage bonus so score can't fall a point short and miss a rank.
	Ranking.secretMissedRngSpawn *= ((double)Difficulty_level + 4) / 4; // Add would-be skill bonus into the penalty for ignored random offspring. This makes ignoring them on high difficulties more consistent and punishing.
	missed_rng_drops = Ranking.secretMissedRngSpawn;
	Ranking.secretRankScore += skill_points + time_points + hostage_points + death_points + missed_rng_drops;

	int minutes = Ranking.secretlevel_time / 60;
	double seconds = Ranking.secretlevel_time - minutes * 60;
	int parMinutes = Ranking.secretParTime / 60;
	double parSeconds = Ranking.secretParTime - parMinutes * 60;
	char* diffname = 0;
	char timeText[256];
	char parTimeString[256];
	c = 0;
	if (Difficulty_level == 0)
		diffname = "Trainee";
	if (Difficulty_level == 1)
		diffname = "Rookie";
	if (Difficulty_level == 2)
		diffname = "Hotshot";
	if (Difficulty_level == 3)
		diffname = "Ace";
	if (Difficulty_level == 4)
		diffname = "Insane";
	if (Ranking.secretQuickload == 0) { // Only print the modded result screen if the player didn't quickload. Print the vanilla one otherwise, because quickloading into a level causes missing crucial mod info.
		if (seconds < 10 || seconds == 60)
			sprintf(timeText, "%i:0%.3f", minutes, seconds);
		else
			sprintf(timeText, "%i:%.3f", minutes, seconds);
		if (parSeconds < 10 || parSeconds == 60)
			sprintf(parTimeString, "%i:0%.0f", parMinutes, parSeconds);
		else
			sprintf(parTimeString, "%i:%.0f", parMinutes, parSeconds);
		sprintf(m_str[c++], "Level score\t%.0f", level_points - Ranking.secretExcludePoints);
		sprintf(m_str[c++], "Time: %s/%s\t%i", timeText, parTimeString, time_points);
		sprintf(m_str[c++], "Hostages: %i/%.0f\t%.0f", Ranking.secret_hostages_on_board, Ranking.hostages_secret_level, hostage_points);
		sprintf(m_str[c++], "Skill: %s\t%i", diffname, skill_points);
		if (Ranking.secretNoDamage)
			sprintf(m_str[c++], "No damage\t%i", death_points);
		else
			sprintf(m_str[c++], "Deaths: %.0f\t%i", Ranking.secretDeathCount, death_points);
		if (Ranking.secretMissedRngSpawn > 0)
			Ranking.secretMissedRngSpawn = 0; // Let's hope this penalty isn't actually needed on a secret level with a thief, or it won't function correctly.
		if (Ranking.secretMissedRngSpawn < 0)
			sprintf(m_str[c++], "Missed RNG spawn: \t%.0f\n", Ranking.secretMissedRngSpawn);
		else
			strcpy(m_str[c++], "\n");
		sprintf(m_str[c++], "Total score\t%0.0f", Ranking.secretRankScore);

		double rankPoints = (Ranking.secretRankScore / Ranking.secretMaxScore) * 12;
		if (Ranking.secretMaxScore == 0)
			rankPoints = 13;
		int rank = 0;
		if (rankPoints >= 0)
			rank = (int)rankPoints + 1;
		if (rank > 14)
			rank = 14;
		if (!PlayerCfg.RankShowPlusMinus)
			rank = truncateRanks(rank + 1) - 1;
   		endlevel_current_rank = rank;
		if (Ranking.cheated) {
			strcpy(m_str[c++], "\n\n\n");
			sprintf(m_str[c++], "   Cheated, score not saved!   "); // Don't show vanilla score when cheating, as players already know it'll always be zero.
		}
		else {
			PHYSFS_File* fp;
			PHYSFS_File* temp;
			char filename[256];
			char temp_filename[256];
			sprintf(filename, "ranks/%s/%s/levelS%i.hi", Players[Player_num].callsign, Current_mission->filename, Current_level_num * -1);
			sprintf(temp_filename, "ranks/%s/%s/temp.hi", Players[Player_num].callsign, Current_mission->filename);
			fp = PHYSFS_openRead(filename);
			if (!fp == NULL) {
				calculateRank(Current_mission->last_level - Current_level_num, 0);
				if (Ranking.secretRankScore > Ranking.calculatedScore || Ranking.rank == 0) {
					time_t timeOfScore = time(NULL);
					temp = PHYSFS_openWrite(temp_filename);
					PHYSFSX_printf(temp, "%.0f\n", Ranking.hostages_secret_level);
					PHYSFSX_printf(temp, "%.0f\n", (Ranking.secretMaxScore - Ranking.hostages_secret_level * 7500) / 3);
					PHYSFSX_printf(temp, "%.0f\n", Ranking.secretParTime);
					PHYSFSX_printf(temp, "%.0f\n", level_points - Ranking.secretExcludePoints);
					PHYSFSX_printf(temp, "%.3f\n", Ranking.secretlevel_time);
					PHYSFSX_printf(temp, "%i\n", Ranking.secret_hostages_on_board);
					PHYSFSX_printf(temp, "%i\n", Difficulty_level);
					if (Ranking.secretNoDamage)
						PHYSFSX_printf(temp, "%i\n", -1);
					else
						PHYSFSX_printf(temp, "%.0f\n", Ranking.secretDeathCount);
					PHYSFSX_printf(temp, "%.0f\n", Ranking.secretMissedRngSpawn);
					PHYSFSX_printf(temp, "%s\n", Current_level_name);
					PHYSFSX_printf(temp, "%s", ctime(&timeOfScore));
					PHYSFSX_printf(temp, "%i", 0);
					PHYSFS_close(temp);
					PHYSFS_close(fp);
					PHYSFS_delete(filename);
					PHYSFSX_rename(temp_filename, filename);
					if (Ranking.rank > 0)
						sprintf(m_str[c++], "New record!");
				}
				PHYSFS_close(fp);
				if (Ranking.rankScore > Ranking.calculatedScore && Ranking.rank > 0)
					strcpy(m_str[c++], "\n\n");
				else
					strcpy(m_str[c++], "\n\n\n");
			}
			else {
				sprintf(m_str[c++], "Saving error. Replay level.\n\n\n");
				PHYSFS_close(fp);
			}
			sprintf(m_str[c++], "Vanilla score:\t      %i", Players[Player_num].score); // Show players' base game score at the end of each level, so they can still compete with it when using the mod.
		}
	}

		for (i = 0; i < c; i++) {
			m[i].type = NM_TYPE_TEXT;
			m[i].text = m_str[i];
		}

		// m[c].type = NM_TYPE_MENU;	m[c++].text = "Ok";
		sprintf(title, "%s %i %s\n %s %s \n", TXT_SECRET_LEVEL, -Current_level_num, TXT_COMPLETE, Current_level_name, TXT_DESTROYED);

		Assert(c <= N_GLITZITEMS);

		gr_init_bitmap_alloc(&transparent, BM_LINEAR, 0, 0, 1, 1, 1);
		transparent.bm_data[0] = 255;
		transparent.bm_flags |= BM_FLAG_TRANSPARENT;

		newmenu_do2(NULL, title, c, m, endlevel_handler, NULL, 0, STARS_BACKGROUND);
		newmenu_free_background();
		
		gr_free_bitmap_data(&transparent);
}

void DoBestRanksScoreGlitz(int level_num)
{
#define N_GLITZITEMS 12
	char				m_str[N_GLITZITEMS][32];
	newmenu_item	m[N_GLITZITEMS + 1];
	int				i, c;
	char				title[128];
	int				is_last_level = 0;
	int levelHostages = 0;
	int levelPoints = 0;
	double parTime;
	int playerPoints = 0;
	double secondsTaken = 0;
	int playerHostages = 0;
	double hostagePoints = 0;
	double difficulty = 0;
	int deathCount = 0;
	double missedRngSpawn = 0;
	char buffer[256];
	char filename[256];
	char levelName[256];
	char timeOfScore[256];
	int warmStart;
	Ranking.quickload = 0; // Set this to 0 so the rank image loads if the player quickloaded on their last played level.
	Ranking.fromBestRanksButton = 1; // So exiting a level and immediately going back into one via best ranks menu doesn't cause a loop.
	sprintf(filename, "ranks/%s/%s/level%d.hi", Players[Player_num].callsign, Current_mission->filename, level_num); // Find file for the requested level.
	if (level_num > Current_mission->last_level)
		sprintf(filename, "ranks/%s/%s/levelS%d.hi", Players[Player_num].callsign, Current_mission->filename, level_num - Current_mission->last_level);
	PHYSFS_file* fp = PHYSFS_openRead(filename);
	if (fp == NULL)
		return;
	else {
		PHYSFSX_getsTerminated(fp, buffer); // Fetch level data starting here. If a parameter isn't present in the file, it'll default to 0 in calculation.
		levelHostages = atoi(buffer);
		if (levelHostages < 0) // If level is unplayed, we know because the first (and only) value in the file will be -1, which is impossible for the hostage count.
			return;
		else {
			PHYSFSX_getsTerminated(fp, buffer);
			levelPoints = atoi(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			parTime = atof(buffer);
			PHYSFSX_getsTerminated(fp, buffer); // Fetch player data starting here.
			playerPoints = atoi(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			secondsTaken = atof(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			playerHostages = atoi(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			difficulty = atof(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			deathCount = atoi(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			missedRngSpawn = atof(buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			sprintf(levelName, buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			sprintf(timeOfScore, buffer);
			PHYSFSX_getsTerminated(fp, buffer);
			warmStart = atoi(buffer);
		}
	}
	PHYSFS_close(fp);
	double maxScore = levelPoints * 3;
	maxScore = (int)maxScore;
	double skillPoints = ceil((playerPoints * (difficulty / 4)));
	double timePoints = (maxScore / 1.5) / pow(2, secondsTaken / parTime);
	if (secondsTaken < parTime)
		timePoints = (maxScore / 2.4) * (1 - (secondsTaken / parTime) * 0.2);
	if (!parTime)
		timePoints = 0;
	timePoints = (int)timePoints;
	hostagePoints = playerHostages * 2500 * ((difficulty + 8) / 12);
	if (playerHostages == levelHostages)
		hostagePoints *= 3;
	hostagePoints = round(hostagePoints); // Round this because I got 24999 hostage bonus once.
	double score = playerPoints + skillPoints + timePoints + missedRngSpawn + hostagePoints;
	maxScore += levelHostages * 7500;
	double deathPoints;
	if (deathCount == -1)
		deathPoints = ceil(maxScore / 12); // Round up instead of down for no damage bonus so score can't fall a point short and miss a rank.
	else
		deathPoints = -(maxScore * 0.4 - maxScore * (0.4 / pow(2, deathCount / (parTime / 360))));
	deathPoints = (int)deathPoints;
	score += deathPoints;

	int minutes = secondsTaken / 60;
	double seconds = secondsTaken - minutes * 60;
	int parMinutes = parTime / 60;
	double parSeconds = parTime - parMinutes * 60;
	char* diffname = 0;
	char timeText[256];
	char parTimeText[256];
	c = 0;
	if (difficulty == 0)
		diffname = "Trainee";
	if (difficulty == 1)
		diffname = "Rookie";
	if (difficulty == 2)
		diffname = "Hotshot";
	if (difficulty == 3)
		diffname = "Ace";
	if (difficulty == 4)
		diffname = "Insane";

	if (seconds < 10 || seconds == 60)
		sprintf(timeText, "%i:0%.3f", minutes, seconds);
	else
		sprintf(timeText, "%i:%.3f", minutes, seconds);
	if (parSeconds < 10 || parSeconds == 60)
		sprintf(parTimeText, "%i:0%.0f", parMinutes, parSeconds);
	else
		sprintf(parTimeText, "%i:%.0f", parMinutes, parSeconds);
	sprintf(m_str[c++], "Level score\t%i", playerPoints);
	sprintf(m_str[c++], "Time: %s/%s\t%.0f", timeText, parTimeText, timePoints);
	sprintf(m_str[c++], "Hostages: %i/%i\t%0.f", playerHostages, levelHostages, hostagePoints);
	sprintf(m_str[c++], "Skill: %s\t%.0f", diffname, skillPoints);
	if (deathCount > -1)
		sprintf(m_str[c++], "Deaths: %i\t%0.f", deathCount, deathPoints);
	else
		sprintf(m_str[c++], "No damage\t%.0f", deathPoints);
	if (missedRngSpawn < 0)
		sprintf(m_str[c++], "Missed RNG spawn: \t%.0f\n", missedRngSpawn);
	else
		strcpy(m_str[c++], "\n");
	if (warmStart)
		sprintf(m_str[c++], "Total score\t* %0.0f", score);
	else
		sprintf(m_str[c++], "Total score\t%0.0f", score);
	double rankPoints = (score / maxScore) * 12;
	if (maxScore == 0)
		rankPoints = 13;
	int rank = 0;
	if (rankPoints >= 0)
		rank = (int)rankPoints + 1;
	if (rank > 14)
		rank = 14;
	if (!PlayerCfg.RankShowPlusMinus)
		rank = truncateRanks(rank + 1) - 1;
	endlevel_current_rank = rank;
	strcpy(m_str[c++], "\n\n\n");
	sprintf(m_str[c++], "Set on %s", timeOfScore);

	for (i = 0; i < c; i++) {
		m[i].type = NM_TYPE_TEXT;
		m[i].text = m_str[i];
	}

	// m[c].type = NM_TYPE_MENU;	m[c++].text = "Ok";
		
	if (level_num > Current_mission->last_level)
		sprintf(title, "%s on %s\nlevel S%i: %s\nEnter plays, esc returns", Players[Player_num].callsign, Current_mission->mission_name, (Current_mission->last_level - level_num) * -1, levelName);
	else
		sprintf(title, "%s on %s\nlevel %i: %s\nEnter plays, esc returns", Players[Player_num].callsign, Current_mission->mission_name, level_num, levelName);

	Assert(c <= N_GLITZITEMS);

	gr_init_bitmap_alloc(&transparent, BM_LINEAR, 0, 0, 1, 1, 1);
	transparent.bm_data[0] = 255;
	transparent.bm_flags |= BM_FLAG_TRANSPARENT;

	Ranking.startingLevel = level_num;
	newmenu_do2(NULL, title, c, m, endlevel_handler, NULL, 0, STARS_BACKGROUND);
	newmenu_free_background();
}

//	-----------------------------------------------------------------------------------------------------
//called when the player is starting a level (new game or new ship)
void StartSecretLevel()
{
	Assert(!Player_is_dead);

	InitPlayerPosition(0);

	verify_console_object();

	ConsoleObject->control_type	= CT_FLYING;
	ConsoleObject->movement_type	= MT_PHYSICS;

	// -- WHY? -- disable_matcens();
	clear_transient_objects(0);		//0 means leave proximity bombs

	// create_player_appearance_effect(ConsoleObject);
	Do_appearance_effect = 1;

	ai_reset_all_paths();
	// -- NO? -- reset_time();

	reset_rear_view();
	Auto_fire_fusion_cannon_time = 0;
	Fusion_charge = 0;
}

extern void set_pos_from_return_segment(void);

//	Returns true if secret level has been destroyed.
int p_secret_level_destroyed(void)
{
	if (First_secret_visit) {
		return 0;		//	Never been there, can't have been destroyed.
	} else {
		if (PHYSFSX_exists(SECRETC_FILENAME,0))
		{
			return 0;
		} else {
			return 1;
		}
	}
}

//	-----------------------------------------------------------------------------------------------------
#define TXT_SECRET_RETURN  "Returning to level %i", Entered_from_level
#define TXT_SECRET_ADVANCE "Base level destroyed.\nAdvancing to level %i", Entered_from_level+1

int draw_stars_bg(newmenu *menu, d_event *event, grs_bitmap *background)
{
	menu = menu;
	
	switch (event->type)
	{
		case EVENT_WINDOW_DRAW:
			gr_set_current_canvas(NULL);
			show_fullscr(background);
			break;
			
		default:
			break;
	}
	
	return 0;
}

void do_screen_message(char *fmt, ...)
{
	va_list arglist;
	grs_bitmap background;
	char msg[1024];
	
	if (Game_mode & GM_MULTI)
		return;
	
	gr_init_bitmap_data(&background);
	if (pcx_read_bitmap(STARS_BACKGROUND, &background, BM_LINEAR, gr_palette) != PCX_ERROR_NONE)
		return;

	gr_palette_load(gr_palette);

	va_start(arglist, fmt);
	vsprintf(msg, fmt, arglist);
	va_end(arglist);
	
	nm_messagebox1(NULL, (int (*)(newmenu *, d_event *, void *))draw_stars_bg, &background, 1, TXT_OK, msg);
	gr_free_bitmap_data(&background);
}

// - - - - - - - - - - START OF PAR TIME ALGORITHM STUFF - - - - - - - - - - \\

// Account for custom ship properties.
#define SHIP_MOVE_SPEED (((double)Player_ship->max_thrust / ((double)Player_ship->mass * (double)Player_ship->drag) * (1 - (double)Player_ship->drag / (double)f1_0)) * pow(f1_0, 2))
// 2500 ammo. There may already be an existing macro related to the ammo cap but I couldn't find one.
#define STARTING_VULCAN_AMMO 2500 * f1_0
#define OBJECTIVE_TYPE_INVALID 0
#define OBJECTIVE_TYPE_OBJECT 1
#define OBJECTIVE_TYPE_TRIGGER 2
#define OBJECTIVE_TYPE_WALL 3

int find_connecting_side(int from, int to) // Sirius' function, but I made it take ints instead of point segs for easier use (also the old '94 function "find_connect_side" already did it with point segs anyway).
{
	for (int side = 0; side < MAX_SIDES_PER_SEGMENT; side++)
		if (Segments[from].children[side] == to)
			return side;
	// This shouldn't happen if consecutive nodes from a valid path were passed in
	Int3();
	return -1;
}

double calculate_combat_time_wall(int wall_num, int pathFinal) // Tell algo to use the weapon that's fastest for the destructible wall in the way.
{ // I was originally gonna ignore this since hostage doors added negligible time, but then thanks to Devil's Heart, I learned that they can have absurd HP! :D
	double thisWeaponCombatTime = -1; // How much time does this wall take to destroy with the current weapon?
	double lowestCombatTime = -1; // Track the time of the fastest weapon so far.
	double ammoUsed = 0; // Same thing but vulcan.
	int topWeapon; // So when depleting energy/ammo, the right one is depleted. Also so the console shows the right weapon.
	// Weapon values converted to a format human beings in 2024 can understand.
	double damage;
	double fire_rate;
	double ammo_usage;
	double splash_radius;
	double wall_health;
	for (int n = 0; n < 35; n++)
		if (!ParTime.heldWeapons[n]) {
			double gunpoints = 2;
			if ((!(n > LASER_ID_L4) || n == LASER_ID_L5 || n == LASER_ID_L6) && !ParTime.hasQuads) { // Account for increased damage of quads.
				if (n > LASER_ID_L4)
					gunpoints = 4;
				else
					gunpoints = 3; // For some reason only quad 1-4 gets 25% damage reduction while quad 5-6 gets none.
			}
			if (n == VULCAN_ID || n == GAUSS_ID || n == OMEGA_ID)
				gunpoints = 1;
			if (n == SPREADFIRE_ID)
				gunpoints = 3;
			if (n == HELIX_ID)
				gunpoints = 5;
			damage = f2fl(Weapon_info[n].strength[Difficulty_level]) * gunpoints;
			fire_rate = (double)f1_0 / Weapon_info[n].fire_wait;
			ammo_usage = Weapon_info[n].ammo_usage * 12.7554168701171875; // To scale with the ammo counter. SaladBadger found out this was the real multiplier after fixed point errors(?), not 13.
			splash_radius = f2fl(Weapon_info[n].damage_radius);
			wall_health = f2fl(Walls[wall_num].hps + 1) - WallAnims[Walls[wall_num].clip_num].num_frames; // For some reason the "real" health of a wall is its hps minus its frame count. Refer to the last line of dxx-redux-ranked commit cb1d724's description.
			if (wall_health < f2fl(1)) // So wall health isn't considered negative after subtracting frames.
				wall_health = f2fl(1);
			if (!(n > LASER_ID_L4) || n == LASER_ID_L5 || n == LASER_ID_L6) // For some reason, the game always uses laser 1's weapon data, even though other levels have a different fire rates in theirs.
				fire_rate = (double)f1_0 / Weapon_info[0].fire_wait;
			// Assume accuracy is always 100% for walls. They're big and don't move lol.
			int shots = ceil(wall_health / damage); // Split time into shots to reflect how players really fire. A 30 HP robot will take two laser 1 shots to kill, not one and a half.
			if (f2fl(ParTime.vulcanAmmo) >= shots * ammo_usage) // Make sure we have enough ammo for this robot before using vulcan.
				thisWeaponCombatTime = shots / fire_rate;
			else
				thisWeaponCombatTime = INFINITY; // Make vulcan's/gauss' time infinite so algo won't use it without ammo.
			if (thisWeaponCombatTime <= lowestCombatTime || lowestCombatTime == -1) { // If it should be used, update algo's weapon stats to the new one's for use in combat time calculation.
				lowestCombatTime = thisWeaponCombatTime;
				ammoUsed = ammo_usage * shots;
				topWeapon = n;
			}
		}
	if (pathFinal) { // Only announce we destroyed the wall (or drain energy/ammo) if we actually did, and aren't just simulating doing so when picking a path.
		if (topWeapon == VULCAN_ID || topWeapon == GAUSS_ID)
			ParTime.vulcanAmmo -= ammoUsed * f1_0;
		if (!(topWeapon > LASER_ID_L4) || topWeapon == LASER_ID_L5 || topWeapon == LASER_ID_L6) {
			if (!ParTime.hasQuads) {
				if (!(topWeapon > LASER_ID_L4))
					printf("Took %.3fs to fight wall %i with quad laser %i\n", lowestCombatTime + 1, wall_num, topWeapon + 1);
				else
					printf("Took %.3fs to fight wall %i with quad laser %i\n", lowestCombatTime + 1, wall_num, topWeapon - 25);
			}
			else {
				if (!(topWeapon > LASER_ID_L4))
					printf("Took %.3fs to fight wall %i with laser %i\n", lowestCombatTime + 1, wall_num, topWeapon + 1);
				else
					printf("Took %.3fs to fight wall %i with laser %i\n", lowestCombatTime + 1, wall_num, topWeapon - 25);
			}
		}
		if (topWeapon == FLARE_ID)
			printf("Took %.3fs to fight wall %i with flares\n", lowestCombatTime + 1, wall_num);
		if (topWeapon == VULCAN_ID)
			printf("Took %.3fs to fight wall %i with vulcan, now at %.0f vulcan ammo\n", lowestCombatTime + 1, wall_num, f2fl((int)ParTime.vulcanAmmo));
		if (topWeapon == SPREADFIRE_ID)
			printf("Took %.3fs to fight wall %i with spreadfire\n", lowestCombatTime + 1, wall_num);
		if (topWeapon == PLASMA_ID)
			printf("Took %.3fs to fight wall %i with plasma\n", lowestCombatTime + 1, wall_num);
		if (topWeapon == FUSION_ID)
			printf("Took %.3fs to fight wall %i with fusion\n", lowestCombatTime + 1, wall_num);
		if (topWeapon == GAUSS_ID)
			printf("Took %.3fs to fight wall %i with gauss, now at %.0f vulcan ammo\n", lowestCombatTime + 1, wall_num, f2fl((int)ParTime.vulcanAmmo));
		if (topWeapon == HELIX_ID)
			printf("Took %.3fs to fight wall %i with helix\n", lowestCombatTime + 1, wall_num);
		if (topWeapon == PHOENIX_ID)
			printf("Took %.3fs to fight wall %i with phoenix\n", lowestCombatTime + 1, wall_num);
		if (topWeapon == OMEGA_ID)
			printf("Took %.3fs to fight wall %i with omega\n", lowestCombatTime + 1, wall_num);
	}
	return lowestCombatTime + 1; // Give an extra second per wall to wait for the explosion to go down. Flying through it causes great damage.
}

double calculate_weapon_accuracy(weapon_info* weapon_info, int weapon_id, object* obj, robot_info* robInfo, int isObject)
{
	// Here we use various aspects of the combat situation to estimate what percentage of shots the player will hit.

	if (weapon_id == OMEGA_ID)
		return 1; // Omega always hits (within a certain distance but we'll fudge it). It'll still rarely get used due to the ultra low DPS Algo perceives it to have, but we can at least give it this, right?

	// First initialize weapon and enemy stuff. This is gonna be a long section.
	// Everything will be doubles due to the variables' involvement in division equations.
	// In D2, enemies can have two weapons, so we'll just take the better of the two for every stat if a second one exists.

	double projectile_speed = f2fl(Weapon_info[weapon_id].speed[Difficulty_level]);
	double player_size = f2fl(ConsoleObject->size);
	double projectile_size = 1;
	if (weapon_info->render_type)
		if (weapon_info->render_type == 2)
			projectile_size = f2fl(Polygon_models[weapon_info->model_num].rad) / f2fl(weapon_info->po_len_to_width_ratio);
		else
			projectile_size = f2fl(weapon_info->blob_size);
	double enemy_behavior = robInfo->behavior;
	double enemy_health = f2fl(robInfo->strength);
	double enemy_max_speed = f2fl(robInfo->max_speed[Difficulty_level]);
	if (isObject) {
		enemy_behavior = obj->ctype.ai_info.behavior;
		if (obj->type == OBJ_CNTRLCEN) // For some reason reactors have a speed of 120??? They don't actually move tho so mark it down as 0.
			enemy_max_speed = 0;
	}
	double enemy_size = f2fl(Polygon_models[robInfo->model_num].rad);
	double enemy_weapon_speed = f2fl(Weapon_info[robInfo->weapon_type2].speed[Difficulty_level]) > f2fl(Weapon_info[robInfo->weapon_type].speed[Difficulty_level]) ? f2fl(Weapon_info[robInfo->weapon_type2].speed[Difficulty_level]) : f2fl(Weapon_info[robInfo->weapon_type].speed[Difficulty_level]);
	double enemy_weapon_size = 1;
	if (Weapon_info[robInfo->weapon_type].render_type)
		if (Weapon_info[robInfo->weapon_type].render_type == 2)
			enemy_weapon_size = f2fl(Polygon_models[Weapon_info[robInfo->weapon_type].model_num].rad) / f2fl(Weapon_info[robInfo->weapon_type].po_len_to_width_ratio);
		else
			enemy_weapon_size = f2fl(Weapon_info[robInfo->weapon_type].blob_size);
	if (Weapon_info[robInfo->weapon_type2].render_type)
		if (Weapon_info[robInfo->weapon_type2].render_type == 2) {
			if (f2fl(Polygon_models[Weapon_info[robInfo->weapon_type2].model_num].rad) / f2fl(Weapon_info[robInfo->weapon_type2].po_len_to_width_ratio) > enemy_weapon_size)
				enemy_weapon_size = f2fl(Polygon_models[Weapon_info[robInfo->weapon_type2].model_num].rad) / f2fl(Weapon_info[robInfo->weapon_type2].po_len_to_width_ratio);
		}
		else {
			if (f2fl(Weapon_info[robInfo->weapon_type2].blob_size) > enemy_weapon_size)
			enemy_weapon_size = f2fl(Weapon_info[robInfo->weapon_type2].blob_size);
		}
	double enemy_attack_type = robInfo->attack_type;
	// Technically doing player splash radius and adding that to dodge_distance later would be consistent, but it would be unfair to the player in certain cases which we don't want.
	double enemy_splash_radius = f2fl(Weapon_info[robInfo->weapon_type2].damage_radius) > f2fl(Weapon_info[robInfo->weapon_type].damage_radius) ? f2fl(Weapon_info[robInfo->weapon_type2].damage_radius) : f2fl(Weapon_info[robInfo->weapon_type].damage_radius);
	double weapon_homing_flag = weapon_info->homing_flag;
	double enemy_weapon_homing_flag = (Weapon_info[robInfo->weapon_type].homing_flag || Weapon_info[robInfo->weapon_type2].homing_flag); // Smart missiles/mines have homing capabilities, but are significantly weaker than actual homing weapons.
	
	// Next, find the "optimal distance" for fighting the given enemy with the given weapon. This is the distance where the enemy's fire can be dodged off of pure reaction time, without any prediction.
	// Once the player's ship can start moving 250ms (avg human reaction time) after the enemy shoots, and get far enough out of the way for the enemy's shots to miss, it's at the optimal distance.
	// Any closer, and the player is put in too much danger. Any further, and the player faces potential accuracy loss due to the enemy having more time to dodge themselves.
	double optimal_distance;
	double player_dodge_distance = player_size + enemy_weapon_size > enemy_splash_radius ? player_size + enemy_weapon_size : enemy_splash_radius; // Stay further away from bots with splash attacks.
	if (enemy_attack_type) { // In the case of enemies that don't shoot at you, the optimal distance depends on their speed, as generally you wanna stand further back the quicker they can approach you.
		optimal_distance = enemy_max_speed / 4; // The /4 is in reference to the 250ms benchmark from earlier. When they start charging you, you've gotta react and start backing up.
		enemy_weapon_homing_flag = 0; // These bots can't actually shoot homing things at you, even if the weapon they would've had otherwise is.
	}
	else
		optimal_distance = (((player_dodge_distance * F1_0) / SHIP_MOVE_SPEED) + 0.25) * enemy_weapon_speed;
	if (robInfo->thief)
		optimal_distance += 80 + enemy_max_speed; // Thieves are the worst of both worlds.
	else {
		if (enemy_behavior == AIB_RUN_FROM) { // We don't want snipe robots to use this, as they actually shoot things.
			optimal_distance = 80; // In the case of enemies that run from you, we'll use the supposed maximum enemy dodge distance because it returns healthy chase time values.	
			enemy_weapon_homing_flag = 0; // These bots can't actually shoot homing things at you, even if the weapon they would've had otherwise is.
		}
		if (enemy_behavior == AIB_SNIPE)
			optimal_distance += enemy_max_speed; // These enemies can back away from you as you shoot. Can't be exact on this or else enemies faster than your weapons will return infinite optimal distance.
	}
	optimal_distance = optimal_distance > robInfo->badass ? optimal_distance : robInfo->badass; // Also ensure we avoid their blast radius.
	optimal_distance = optimal_distance > f2fl(Weapon_info[weapon_id].damage_radius) ? optimal_distance : f2fl(Weapon_info[weapon_id].damage_radius); // Don't stay close enough to get damaged by our own weapon!

	// Next, figure out how well the enemy will dodge a player attack of this weapon coming from the optimal distance away, then base accuracy off of that.
	// For simplicity, we assume enemies face longways and dodge sideways relative to player rotation, and that the player is shooting at the middle of the target from directly ahead.
	// The amount of distance required to move is based off of hard coded gunpoints on the ship, as well as the radius of the player projectile, so we'll have to set values per weapon ID.
	// For spreading weapons, the offset technically depends on which projectile we're talking about, but we'll set it to that of the middle one's starting point for now, then account for decreasing accuracy over distance later.
	double projectile_offsets[35] = { 2.2, 2.2, 2.2, 2.2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.2, 2.2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.2, 2.2, 0, 0, 2.2};
	// Quad lasers have a wider offset, making them a little harder to dodge. Account for this.
	// This makes their accuracy worse against small enemies than reality due to the offset of the inner lasers being ignored, but this is a rare occurence.
	if (!ParTime.hasQuads) {
		projectile_offsets[LASER_ID_L1] *= 1.5;
		projectile_offsets[LASER_ID_L2] *= 1.5;
		projectile_offsets[LASER_ID_L3] *= 1.5;
		projectile_offsets[LASER_ID_L4] *= 1.5;
		projectile_offsets[LASER_ID_L5] *= 1.5;
		projectile_offsets[LASER_ID_L6] *= 1.5;
	}
	
	double dodge_requirement = projectile_offsets[weapon_id] + projectile_size + enemy_size;
	double dodge_time = optimal_distance / projectile_speed;
	double dodge_distance = 0;
	// For running bots, we use a simple multiplication, but for everyone else, we have to account for how they dodge. They start at their evade speed, then slow down over time. Always using evade speed as a flat rate causes Algo to underestimate, and always using max speed does the opposite.
	double drag_multiplier = 1 - f2fl(robInfo->drag);
	double enemy_evade_speed = (robInfo->evade_speed[Difficulty_level] + 0.5) * 32; // The + 0.5 comes from move_around_player, where HP percentage is added to evade_speed. HP is assumed to degrade linearly here, so we add the average of all HP values: 0.5.
	if (!robInfo->evade_speed)
		enemy_evade_speed = 0; // Undo the + 0.5 if the initial value was 0, as per move_around_player.
	enemy_evade_speed /= 64; // We want speed per physics tick, not per second.
	if (robInfo->thief || enemy_behavior == AIB_RUN_FROM) // Mine layers and thieves always move at a set speed; they don't try to dodge.
		dodge_distance = enemy_max_speed * dodge_time;
	else {
		int num_frames = dodge_time * 64;
		for (int i = 0; i <= num_frames; i++) {
			dodge_distance += enemy_evade_speed;
			enemy_evade_speed *= drag_multiplier;
			if (enemy_evade_speed > enemy_max_speed / 64) // Robots traveling faster than their max speed receive an additional drag. This is crucial to prevent underestimation.
				enemy_evade_speed *= 0.75;
		}
	}

	// Now we reduce accuracy over distance for spreading weapons. Since that's also hard coded, we can just apply enemy size based multipliers per weapon ID.
	// At a certain distance, projectiles for Vulcan and Spreadfire will start missing from drifting so far off course.
	// For Vulcan we scale accuracy by the probability of a shot landing at a given distance (since the spread is random), and for Spreadfire/Helix we use a binary outcome.
	double accuracy_multiplier = 1;
	if (weapon_id == VULCAN_ID)
		if (optimal_distance / 32 > enemy_size + projectile_size) // This is the distance where Vulcan's accuracy dropoff starts.
			accuracy_multiplier *= (enemy_size + projectile_size) / (optimal_distance / 32);
	if (weapon_id == GAUSS_ID) // Gauss has 20% of Vulcan's spread... as if they thought it wasn't good enough or something. This is unlikely to ever influence the accuracy value but hey you never know.
		if (optimal_distance / 160 > enemy_size + projectile_size) // This is the distance where Gauss' accuracy dropoff starts.
			accuracy_multiplier *= (enemy_size + projectile_size) / (optimal_distance / 160);
	if (weapon_id == SPREADFIRE_ID)
		if (optimal_distance / 16 > enemy_size + projectile_size) // Divisor is gotten because Spreadfire projectiles move one unit outward for every 16 units forward.
			accuracy_multiplier /= 3;
	if (weapon_id == HELIX_ID) {
		if (optimal_distance / 8 > enemy_size + projectile_size) // Outer Helix projectiles move one unit outward for every eight units forward.
			accuracy_multiplier *= 0.6;
		if (optimal_distance / 16 > enemy_size + projectile_size) // Middle-left and middle-right Helix projectiles are equal to Spreadfire's outer projectiles in sideways movement.
			accuracy_multiplier *= 0.2;
	}
	// If the enemy is small enough to fit between projectiles, only one can hit at a time, so halve accuracy.
	// This makes Algo unlikely to use something like lasers against sidearm modula, which is good because it obeys real world expectations.
	if (enemy_size < projectile_offsets[weapon_id] - projectile_size)
		accuracy_multiplier *= 0.5;

	double accuracy = dodge_requirement / dodge_distance; // Estimated accuracy will be the factor by which a robot successfully dodges a player attack in the above simulation, with the homing adjustments and multiplier slapped on top.
	if (accuracy > 1) // Do this first so homing enemies always give a substantial time increase.
		accuracy = 1; // Accuracy can't be greater than 100%.
	if (weapon_homing_flag)
		accuracy = accuracy / 2 + 0.5; // Buff player accuracy if their weapon has homing. Average of acc and 100 is good enough since enemies are kinda stupid when it comes to dodging homing stuff.
	if (enemy_weapon_homing_flag) // We want the enemy's check to be last so both having homing still has a net negative effect. Homing enemies are hard to avoid and take extra time to kill.
		accuracy /= 2; // Conversely, do the opposite if the enemy's weapon has homing: Average of acc and 0.
	return accuracy * accuracy_multiplier;
}

double calculate_combat_time(object* obj, robot_info* robInfo, int isObject, int isMatcen) // Tell algo to use the weapon that's fastest for the current enemy.
{
	int weapon_id = 0; // Just a shortcut for the relevant index in algo's inventory.
	double thisWeaponCombatTime = -1; // How much time does this enemy take to kill with the current weapon?
	double lowestCombatTime = -1; // Track the time of the fastest weapon so far.
	double ammoUsed = 0; // Same thing but vulcan.
	int topWeapon = -1; // So when depleting energy/ammo, the right one is depleted. Also so the console shows the right weapon.
	double offspringHealth; // So multipliers done to offspring don't bleed into their parents' values.
	double accuracy; // Players are NOT perfect, and it's usually not their fault. We need to account for this if we want all par times to be reachable.
	double topAccuracy; // The percentage shown on the debug console.
	double adjustedRobotHealthNoAccuracy;
	int failsafe = 0; // For Maximum 16 type cases where we don't actually have a vulcan or gauss cannon to kill an energy-immune boss. Since Algo can't use missiles, we'll give it vulcan for the boss only to dodge a softlock.
	// Weapon values converted to a format human beings in 2024 can understand.
	for (int n = 0; n < 35; n++)
		if (!ParTime.heldWeapons[n]) {
			weapon_id = n;
			if (failsafe)
				weapon_id = VULCAN_ID; // Give Algo Vulcan in the event of a failsafe. This is because Vulcan is the weaker weapon by default, but someone could edit gauss to be weaker. idrc tbh.
			weapon_info* weapon_info = &Weapon_info[weapon_id];
			double gunpoints = 2;
			if ((!(weapon_id > LASER_ID_L4) || weapon_id == LASER_ID_L5 || weapon_id == LASER_ID_L6) && !ParTime.hasQuads) { // Account for increased damage of quads.
				if (weapon_id > LASER_ID_L4)
					gunpoints = 4;
				else
					gunpoints = 3; // For some reason only quad 1-4 gets 25% damage reduction while quad 5-6 gets none.
			}
			if (weapon_id == VULCAN_ID || weapon_id == GAUSS_ID || weapon_id == OMEGA_ID)
				gunpoints = 1;
			if (weapon_id == SPREADFIRE_ID)
				gunpoints = 3;
			if (weapon_id == HELIX_ID)
				gunpoints = 5;
			double damage = f2fl(weapon_info->strength[Difficulty_level]) * gunpoints;
			double fire_rate = (double)f1_0 / weapon_info->fire_wait;
			double ammo_usage = Weapon_info[n].ammo_usage * 12.7554168701171875; // To scale with the ammo counter. SaladBadger found out this was the real multiplier after fixed point errors(?), not 13.
			double splash_radius = f2fl(weapon_info->damage_radius);
			double enemy_health = f2fl(robInfo->strength + 1); // We do +1 to account for robots still being alive at exactly 0 HP.
			double enemy_size = f2fl(Polygon_models[robInfo->model_num].rad);
			if (!(weapon_id > LASER_ID_L4) || weapon_id == LASER_ID_L5 || weapon_id == LASER_ID_L6) // For some reason, the game always uses laser 1's weapon data, even though other levels have a different fire rates in theirs.
				fire_rate = (double)f1_0 / Weapon_info[0].fire_wait;
			if (isObject) {
				if (obj->type == OBJ_CNTRLCEN) {
					if (weapon_id == FUSION_ID)
						damage *= 2; // Fusion's damage is doubled against reactors in Redux.
					enemy_health = f2fl(obj->shields + 1);
					enemy_size = f2fl(obj->size);
				}
				if (robInfo->boss_flag)
					enemy_health = f2fl(obj->shields + 1); // Boss objects' health is overridden with a difficulty-based multiplier in D2 (0.25/0.625/0.75/0.875/1).
			}
			// If we're fighting a boss that's immune to the weapon we're about to calculate, skip the weapon.
			if (robInfo->boss_flag) {
				if (weapon_id == GAUSS_ID)
					damage *= 1 - ((double)Difficulty_level * 0.1); // Damage of gauss on bosses goes down as difficulty goes up.
				if (Boss_invulnerable_energy[robInfo->boss_flag - BOSS_D2]) {
					if (!(weapon_id == VULCAN_ID || weapon_id == GAUSS_ID)) {
						if (n == 34 && topWeapon == -1) { // We just finished the last weapon and haven't gotten a time, so Algo only has energy weapons against an energy-immune boss.
							failsafe = 1;
							n--; // Decrement so the loop works one more time.
						}
						continue;
					}
					else
						ammo_usage = 0; // Give Algo infinite vulcan ammo so it doesn't softlock fighting a boss that's immune to energy weapons. Do remember that Algo doesn't have access to missiles!
				}
				if (Boss_invulnerable_matter[robInfo->boss_flag - BOSS_D2])
					if (weapon_id == VULCAN_ID || weapon_id == GAUSS_ID)
						continue;
			}
			double adjustedRobotHealth = enemy_health;
			adjustedRobotHealth /= (splash_radius - enemy_size) / splash_radius >= 0 ? 1 + (splash_radius - enemy_size) / splash_radius : 1; // Divide the health value of the enemy instead of increasing damage when accounting for splash damage, since we'll potentially have multiple damage values.
			adjustedRobotHealthNoAccuracy = adjustedRobotHealth;
			adjustedRobotHealth /= calculate_weapon_accuracy(weapon_info, weapon_id, obj, robInfo, isObject);
			if (robInfo->thief)
				ParTime.combatTime += 2.5; // To account for the death tantrum they throw when they get their comeuppance for stealing your stuff.
			accuracy = adjustedRobotHealthNoAccuracy / adjustedRobotHealth;
			int shots = round(ceil(adjustedRobotHealthNoAccuracy / damage) / accuracy); // Split time into shots to reflect how players really fire. A 30 HP robot will take two laser 1 shots to kill, not one and a half.
			if (f2fl(ParTime.vulcanAmmo) >= shots * ammo_usage) // Make sure we have enough ammo for this robot before using vulcan.
				thisWeaponCombatTime = shots / fire_rate;
			else
				thisWeaponCombatTime = INFINITY; // Make vulcan's/gauss' time infinite so Algo won't use it without ammo.
			if (thisWeaponCombatTime <= lowestCombatTime || lowestCombatTime == -1) { // If it should be used, update algo's weapon stats to the new one's for use in combat time calculation.
				lowestCombatTime = thisWeaponCombatTime;
				ParTime.ammo_usage = shots * ammo_usage;
				ammoUsed = ammo_usage * shots;
				topWeapon = weapon_id;
				topAccuracy = accuracy * 100;
			}
		}
	if (lowestCombatTime == -1)
		lowestCombatTime = 0; // Prevent a softlock if no primaries work on a given boss.
	if ((topWeapon == VULCAN_ID || topWeapon == GAUSS_ID) && !failsafe)
		ParTime.vulcanAmmo -= ammoUsed * f1_0;
	if (isMatcen) {
		// Now account for RNG ammo drops from matcen bots and their robot spawn.
		if (robInfo->contains_type == OBJ_POWERUP && robInfo->contains_id == POW_VULCAN_AMMO)
			ParTime.ammo_usage -= f2fl(((double)robInfo->contains_count * ((double)robInfo->contains_prob / 16)) * (STARTING_VULCAN_AMMO / 2));
		if (robInfo->contains_type == OBJ_ROBOT) {
			if (Robot_info[robInfo->contains_id].contains_type == OBJ_POWERUP && Robot_info[robInfo->contains_id].contains_id == POW_VULCAN_AMMO)
				ParTime.ammo_usage -= f2fl(((double)Robot_info[robInfo->contains_id].contains_count * ((double)Robot_info[robInfo->contains_id].contains_prob / 16)) * (STARTING_VULCAN_AMMO / 2));
		}
	} else if (isObject) {
		if (!(topWeapon > LASER_ID_L4) || topWeapon == LASER_ID_L5 || topWeapon == LASER_ID_L6) {
			if (!ParTime.hasQuads) {
				if (!(topWeapon > LASER_ID_L4))
					printf("Took %.3fs to fight robot type %i with quad laser %i, %.2f accuracy\n", lowestCombatTime, obj->id, topWeapon + 1, topAccuracy);
				else
					printf("Took %.3fs to fight robot type %i with quad laser %i, %.2f accuracy\n", lowestCombatTime, obj->id, topWeapon - 25, topAccuracy);
			}
			else {
				if (!(topWeapon > LASER_ID_L4))
					printf("Took %.3fs to fight robot type %i with laser %i, %.2f accuracy\n", lowestCombatTime, obj->id, topWeapon + 1, topAccuracy);
				else
					printf("Took %.3fs to fight robot type %i with laser %i, %.2f accuracy\n", lowestCombatTime, obj->id, topWeapon - 25, topAccuracy);
			}
		}
		if (topWeapon == FLARE_ID)
			printf("Took %.3fs to fight robot type %i with flares, %.2f accuracy\n", lowestCombatTime, obj->id, topAccuracy);
		if (topWeapon == VULCAN_ID) {
			if (failsafe)
				printf("Took %.3fs to fight robot type %i with vulcan (FAILSAFE), %.2f accuracy\n", lowestCombatTime, obj->id, topAccuracy);
			else
				printf("Took %.3fs to fight robot type %i with vulcan, %.2f accuracy, now at %.0f vulcan ammo\n", lowestCombatTime, obj->id, topAccuracy, f2fl((int)ParTime.vulcanAmmo));
		}
		if (topWeapon == SPREADFIRE_ID)
			printf("Took %.3fs to fight robot type %i with spreadfire, %.2f accuracy\n", lowestCombatTime, obj->id, topAccuracy);
		if (topWeapon == PLASMA_ID)
			printf("Took %.3fs to fight robot type %i with plasma, %.2f accuracy\n", lowestCombatTime, obj->id, topAccuracy);
		if (topWeapon == FUSION_ID)
			printf("Took %.3fs to fight robot type %i with fusion, %.2f accuracy\n", lowestCombatTime, obj->id, topAccuracy);
		if (topWeapon == GAUSS_ID)
			printf("Took %.3fs to fight robot type %i with gauss, %.2f accuracy, now at %.0f vulcan ammo\n", lowestCombatTime, obj->id, topAccuracy, f2fl((int)ParTime.vulcanAmmo));
		if (topWeapon == HELIX_ID)
			printf("Took %.3fs to fight robot type %i with helix, %.2f accuracy\n", lowestCombatTime, obj->id, topAccuracy);
		if (topWeapon == PHOENIX_ID)
			printf("Took %.3fs to fight robot type %i with phoenix, %.2f accuracy\n", lowestCombatTime, obj->id, topAccuracy);
		if (topWeapon == OMEGA_ID)
			printf("Took %.3fs to fight robot type %i with omega, %.2f accuracy\n", lowestCombatTime, obj->id, topAccuracy);
	}
	return lowestCombatTime;
}

int getObjectiveSegnum(partime_objective objective)
{
	if (objective.type == OBJECTIVE_TYPE_OBJECT)
		return Objects[objective.ID].segnum;
	if (objective.type == OBJECTIVE_TYPE_TRIGGER || objective.type == OBJECTIVE_TYPE_WALL)
		return Walls[objective.ID].segnum;
	printf("Warning: Par time is going to an undefined segment!\n");
	return -1;
}

vms_vector getObjectivePosition(partime_objective objective)
{
	if (objective.type == OBJECTIVE_TYPE_OBJECT)
		return Objects[objective.ID].pos;
	vms_vector segmentCenter;
	compute_segment_center(&segmentCenter, &Segments[getObjectiveSegnum(objective)]);
	return segmentCenter;
}

int robotHasKey(object* obj) // Should cover all the shinanegans level authors can do with key holders, in one neat convenient spot.
{
	if (obj->type == OBJ_ROBOT && obj->contains_type == OBJ_POWERUP && (obj->contains_id == POW_KEY_BLUE || obj->contains_id == POW_KEY_GOLD || obj->contains_id == POW_KEY_RED))
		return obj->contains_id; // This specific robot contains a key.
	if (obj->type == OBJ_ROBOT && obj->contains_type == -1 && Robot_info[obj->id].contains_type == OBJ_POWERUP && (Robot_info[obj->id].contains_id == POW_KEY_BLUE || Robot_info[obj->id].contains_id == POW_KEY_GOLD || Robot_info[obj->id].contains_id == POW_KEY_RED))
		return Robot_info[obj->id].contains_id; // This robot type is hard coded to contain a key, and this specific robot contains nothing different.
	if (obj->contains_type == OBJ_ROBOT && Robot_info[obj->contains_id].contains_type == OBJ_POWERUP && (Robot_info[obj->contains_id].contains_id == POW_KEY_BLUE || Robot_info[obj->contains_id].contains_id == POW_KEY_GOLD || Robot_info[obj->contains_id].contains_id == POW_KEY_RED))
		return Robot_info[obj->contains_id].contains_id; // This specific robot contains a robot whose type is hard coded to contain a key.
	return 0; // This robot does not contain a key.
}

void robotHasPowerup(int robotID, double weight) {
	// The weight parameter is to determine how much of this probability should influence the total.
	// Since matcens are calculated by the average of all its robots, we have to account for the fact that there's only a certain chance to get the odds from that given one.
	robot_info* robInfo = &Robot_info[robotID];
	int weapon_id;
	int i;
	if (robInfo->contains_type == OBJ_POWERUP) {
		weapon_id = 0;
		if (robInfo->contains_id == POW_VULCAN_WEAPON)
			weapon_id = VULCAN_ID;
		if (robInfo->contains_id == POW_SPREADFIRE_WEAPON)
			weapon_id = SPREADFIRE_ID;
		if (robInfo->contains_id == POW_PLASMA_WEAPON)
			weapon_id = PLASMA_ID;
		if (robInfo->contains_id == POW_FUSION_WEAPON)
			weapon_id = FUSION_ID;
		if (robInfo->contains_id == POW_GAUSS_WEAPON)
			weapon_id = GAUSS_ID;
		if (robInfo->contains_id == POW_HELIX_WEAPON)
			weapon_id = HELIX_ID;
		if (robInfo->contains_id == POW_PHOENIX_WEAPON)
			weapon_id = PHOENIX_ID;
		if (robInfo->contains_id == POW_OMEGA_WEAPON)
			weapon_id = OMEGA_ID;
		if (weapon_id) {
			ParTime.heldWeapons[weapon_id] *= (1 - weight) + (pow(1 - ((double)robInfo->contains_prob / 16), robInfo->contains_count) * weight);
			if (ParTime.heldWeapons[weapon_id] <= 0.0625) {
				if ((weapon_id == VULCAN_ID || weapon_id == GAUSS_ID) && ParTime.heldWeapons[weapon_id])
					ParTime.vulcanAmmo += STARTING_VULCAN_AMMO;
				else
					ParTime.vulcanAmmo += STARTING_VULCAN_AMMO / 2;
				ParTime.heldWeapons[weapon_id] = 0;
			}
		}
		else {
			for (i = 0; i < round(robInfo->contains_count * (robInfo->contains_prob / 16)); i++) {
				if (robInfo->contains_id == POW_LASER)
					if (ParTime.laser_level < LASER_ID_L4)
						ParTime.laser_level++;
				if (robInfo->contains_id == POW_SUPER_LASER) {
					if (ParTime.laser_level < LASER_ID_L5)
						ParTime.laser_level = LASER_ID_L5;
					else
						ParTime.laser_level = LASER_ID_L6;
					ParTime.heldWeapons[1] = 0;
					ParTime.heldWeapons[2] = 0;
					ParTime.heldWeapons[3] = 0;
				}
				ParTime.heldWeapons[ParTime.laser_level] = 0;
			}
			if (robInfo->contains_id == POW_QUAD_FIRE) {
				ParTime.hasQuads *= (1 - weight) + (pow(1 - ((double)robInfo->contains_prob / 16), robInfo->contains_count) * weight);
				if (ParTime.hasQuads <= 0.0625)
					ParTime.hasQuads = 0;
			}
			if (robInfo->contains_id == POW_AFTERBURNER) {
				ParTime.hasAfterburner *= (1 - weight) + (pow(1 - ((double)robInfo->contains_prob / 16), robInfo->contains_count) * weight);
				if (ParTime.hasAfterburner <= 0.0625) {
					ParTime.hasAfterburner = 0;
					double afterburnerMultipliers[5] = { 1.2, 1.15, 1.11, 1.08, 1.05 };
					ParTime.afterburnerMultiplier = afterburnerMultipliers[Difficulty_level];
				}
			}
			if (robInfo->contains_id == POW_VULCAN_AMMO)
				ParTime.vulcanAmmo += (STARTING_VULCAN_AMMO / 2) * round(robInfo->contains_count * (robInfo->contains_prob / 16));
		}
	}
	if (robInfo->contains_type == OBJ_ROBOT) {
		robInfo = &Robot_info[Robot_info[robotID].contains_id];
		weapon_id = 0;
		if (robInfo->contains_id == POW_VULCAN_WEAPON)
			weapon_id = VULCAN_ID;
		if (robInfo->contains_id == POW_SPREADFIRE_WEAPON)
			weapon_id = SPREADFIRE_ID;
		if (robInfo->contains_id == POW_PLASMA_WEAPON)
			weapon_id = PLASMA_ID;
		if (robInfo->contains_id == POW_FUSION_WEAPON)
			weapon_id = FUSION_ID;
		if (robInfo->contains_id == POW_GAUSS_WEAPON)
			weapon_id = GAUSS_ID;
		if (robInfo->contains_id == POW_HELIX_WEAPON)
			weapon_id = HELIX_ID;
		if (robInfo->contains_id == POW_PHOENIX_WEAPON)
			weapon_id = PHOENIX_ID;
		if (robInfo->contains_id == POW_OMEGA_WEAPON)
			weapon_id = OMEGA_ID;
		if (weapon_id) {
			ParTime.heldWeapons[weapon_id] *= (1 - weight) + (pow(1 - ((double)robInfo->contains_prob / 16), robInfo->contains_count) * weight);
			if (ParTime.heldWeapons[weapon_id] <= 0.0625) {
				if ((weapon_id == VULCAN_ID || weapon_id == GAUSS_ID) && ParTime.heldWeapons[weapon_id])
					ParTime.vulcanAmmo += STARTING_VULCAN_AMMO;
				else
					ParTime.vulcanAmmo += STARTING_VULCAN_AMMO / 2;
				ParTime.heldWeapons[weapon_id] = 0;
			}
		}
		else {
			for (i = 0; i < round(robInfo->contains_count * (robInfo->contains_prob / 16)); i++) {
				if (robInfo->contains_id == POW_LASER)
					if (ParTime.laser_level < LASER_ID_L4)
						ParTime.laser_level++;
				if (robInfo->contains_id == POW_SUPER_LASER) {
					if (ParTime.laser_level < LASER_ID_L5)
						ParTime.laser_level = LASER_ID_L5;
					else
						ParTime.laser_level = LASER_ID_L6;
					ParTime.heldWeapons[1] = 0;
					ParTime.heldWeapons[2] = 0;
					ParTime.heldWeapons[3] = 0;
				}
				ParTime.heldWeapons[ParTime.laser_level] = 0;
			}
			if (robInfo->contains_id == POW_QUAD_FIRE) {
				ParTime.hasQuads *= (1 - weight) + (pow(1 - ((double)robInfo->contains_prob / 16), robInfo->contains_count) * weight);
				if (ParTime.heldWeapons[weapon_id] <= 0.0625)
					ParTime.heldWeapons[weapon_id] = 0;
			}
			if (robInfo->contains_id == POW_AFTERBURNER) {
				ParTime.hasAfterburner *= (1 - weight) + (pow(1 - ((double)robInfo->contains_prob / 16), robInfo->contains_count) * weight);
				if (ParTime.hasAfterburner <= 0.0625) {
					ParTime.hasAfterburner = 0;
					double afterburnerMultipliers[5] = { 1.2, 1.15, 1.11, 1.08, 1.05 };
					ParTime.afterburnerMultiplier = afterburnerMultipliers[Difficulty_level];
				}
			}
			if (robInfo->contains_id == POW_VULCAN_AMMO)
				ParTime.vulcanAmmo += (STARTING_VULCAN_AMMO / 2) * round(robInfo->contains_count * (robInfo->contains_prob / 16));
		}
	}
}

void addObjectiveToList(partime_objective objective, int isDoneList)
{
	int i;
	if (isDoneList) {
		// First make sure it's not already in there
		for (i = 0; i < ParTime.doneListSize; i++)
			if (ParTime.doneList[i].type == objective.type && ParTime.doneList[i].ID == objective.ID)
				return;
		ParTime.doneList[ParTime.doneListSize].type = objective.type;
		ParTime.doneList[ParTime.doneListSize].ID = objective.ID;
		ParTime.doneListSize++;
	}
	else {
		for (i = 0; i < ParTime.toDoListSize; i++)
			if (ParTime.toDoList[i].type == objective.type && ParTime.toDoList[i].ID == objective.ID)
				return;
		ParTime.toDoList[ParTime.toDoListSize].type = objective.type;
		ParTime.toDoList[ParTime.toDoListSize].ID = objective.ID;
		ParTime.toDoListSize++;
	}
}

int findKeyObjectID(int keyType, int dontCheckAccessibility, int unlockCheck)
{
	int powerupID;
	int foundKey = 0;
	switch (keyType)
	{
	case KEY_BLUE:
		powerupID = POW_KEY_BLUE;
		break;
	case KEY_GOLD:
		powerupID = POW_KEY_GOLD;
		break;
	case KEY_RED:
		powerupID = POW_KEY_RED;
		break;
	default:
		// That's not a key!
		Int3();
		return 0;
	}
	int powerupID2 = pow(2, powerupID - 3); // Translate the POW_KEY macros to KEY flags for door walls.
	if (ParTime.missingKeys & powerupID2 && unlockCheck)
		return 1; // Allow Algo through colored doors whose keys are missing from the level. This prevents softlocks on certain edge case levels.
	for (int i = 0; i <= Highest_object_index; i++) {
		if ((Objects[i].type == OBJ_POWERUP && Objects[i].id == powerupID) || robotHasKey(&Objects[i]) == powerupID) {
			partime_objective objective = { OBJECTIVE_TYPE_OBJECT, i };
			if (unlockCheck) {
				for (int n = 0; n < ParTime.doneListSize; n++)
					if (ParTime.doneList[n].type == OBJECTIVE_TYPE_OBJECT && ParTime.doneList[n].ID == i)
						foundKey = 1;
			}
			else {
				if (dontCheckAccessibility && ParTime.missingKeys & powerupID2)
					ParTime.missingKeys -= powerupID2;
				if (dontCheckAccessibility || ParTime.isSegmentAccessible[Objects[i].segnum]) { // Make sure the key or the robot that contains it can be physically flown to by the player.
					foundKey = 1;
					if (dontCheckAccessibility)
						addObjectiveToList(objective, 1);
					else
						addObjectiveToList(objective, 0);
				}
			}
		}
	}
	return foundKey;
}

int findConnectedWallNum(int wall_num)
{
	wall* wall = &Walls[wall_num];
	segment* connectedSegment = &Segments[Segments[wall->segnum].children[wall->sidenum]];
	if (connectedSegment == NULL)
		return -1;

	int connectedSide = find_connect_side(&Segments[wall->segnum], connectedSegment);
	if (connectedSide >= 0)
		return connectedSegment->sides[connectedSide].wall_num;

	return -1;
}

void removeObjectiveFromList(partime_objective objective)
{
	for (int i = 0; i < ParTime.toDoListSize; i++)
		if (ParTime.toDoList[i].type == objective.type && ParTime.toDoList[i].ID == objective.ID) {
			for (int n = i; n < ParTime.toDoListSize; n++)
				ParTime.toDoList[n] = ParTime.toDoList[n + 1];
			break;
		}
	ParTime.toDoListSize--;
}

void initLockedWalls(int removeUnlockableWalls)
{
	int i;
	if (removeUnlockableWalls) // Only count keys as missing if there's even a door of that color to unlock. Without this, levels using less than three key colors could break.
		for (i = 0; i < Num_walls; i++)
			if (Walls[i].type == WALL_DOOR) {
				if (Walls[i].keys & KEY_BLUE && !(ParTime.missingKeys & KEY_BLUE))
					ParTime.missingKeys |= KEY_BLUE;
				if (Walls[i].keys & KEY_GOLD && !(ParTime.missingKeys & KEY_GOLD))
					ParTime.missingKeys |= KEY_GOLD;
				if (Walls[i].keys & KEY_RED && !(ParTime.missingKeys & KEY_RED))
					ParTime.missingKeys |= KEY_RED;
			}
	int foundUnlock;
	ParTime.numTypeThreeWalls = 0;
	for (i = 0; i < Num_walls; i++) {
		foundUnlock = 0;
		// Is it opened by a key?
		if (Walls[i].type == WALL_DOOR && Walls[i].keys > 1)
			foundUnlock = findKeyObjectID(Walls[i].keys, removeUnlockableWalls, 0);
		if ((Walls[i].type == WALL_DOOR && Walls[i].flags & WALL_DOOR_LOCKED) || Walls[i].type == WALL_CLOSED || Walls[i].type == WALL_CLOAKED) {
			// ...or is it opened by a trigger?
			for (int t = 0; t < Num_triggers; t++) {
				if (Triggers[t].type == TT_OPEN_DOOR || Triggers[t].type == TT_OPEN_WALL || Triggers[t].type == TT_UNLOCK_DOOR || Triggers[t].type == TT_ILLUSORY_WALL) {
					partime_objective objective;
					objective.type = OBJECTIVE_TYPE_INVALID;
					objective.ID = 0;
					for (int w = 0; w < Num_walls; w++)
						if (Walls[w].trigger == t) {
							objective.type = OBJECTIVE_TYPE_TRIGGER;
							objective.ID = w;
							break;
						}
					if (!objective.type)
						break; // No wall was found. We have an orphaned trigger, skip it. (Obsidian 4 fix)
					for (int l = 0; l < Triggers[t].num_links; l++) {
						int connectedWallNum = findConnectedWallNum(i);
						if ((Triggers[t].seg[l] == Walls[i].segnum && Triggers[t].side[l] == Walls[i].sidenum) || (Triggers[t].type != TT_UNLOCK_DOOR && Triggers[t].seg[l] == Walls[connectedWallNum].segnum && Triggers[t].side[l] == Walls[connectedWallNum].sidenum)) {
							if (removeUnlockableWalls)
								addObjectiveToList(objective, 1);
							else
								addObjectiveToList(objective, 0);
							foundUnlock = 1;
							break;
						}
					}
				}
			}
		}
		// Now see if we have to add the other side of this wall as an unlock objective for this wall (for D1 S2 "shoot the unlocked side" type puzzles).
		// We do need to specify doors for this upcoming section, because unlike the previous ones, things CAN break otherwise.
		if (Walls[i].type == WALL_DOOR && (Walls[i].keys > 1 || Walls[i].flags & WALL_DOOR_LOCKED) && !foundUnlock) { // Only locked walls we didn't find anything for are eligible for having this put on their other side.
			int foundConnectedUnlock = 0;
			for (int t = 0; t < ControlCenterTriggers.num_links; t++)
				if (ControlCenterTriggers.seg[t] == Walls[i].segnum && ControlCenterTriggers.side[t] == Walls[i].sidenum)
					foundConnectedUnlock = 1;
			if (foundConnectedUnlock)
				continue; // Don't add these objectives on the back sides of reactor walls either.
			int connectedWallNum = findConnectedWallNum(i);
			if (Walls[connectedWallNum].keys > 1)
				foundUnlock = findKeyObjectID(Walls[i].keys, removeUnlockableWalls, 0);
			if (Walls[connectedWallNum].flags & WALL_DOOR_LOCKED)
				for (int t = 0; t < Num_triggers; t++)
					if (Triggers[t].type == TT_OPEN_DOOR || Triggers[t].type == TT_OPEN_WALL || Triggers[t].type == TT_UNLOCK_DOOR || Triggers[t].type == TT_ILLUSORY_WALL)
						for (int l = 0; l < Triggers[t].num_links; l++)
							if (Triggers[t].seg[l] == Walls[connectedWallNum].segnum && Triggers[t].side[l] == Walls[connectedWallNum].sidenum)
								foundConnectedUnlock = 1;
			if (!(Walls[connectedWallNum].keys > 1 || Walls[connectedWallNum].flags & WALL_DOOR_LOCKED) || foundConnectedUnlock) {
				partime_objective objective = { OBJECTIVE_TYPE_WALL, connectedWallNum };
				ParTime.typeThreeWalls[ParTime.numTypeThreeWalls] = i;
				ParTime.typeThreeUnlockIDs[ParTime.numTypeThreeWalls] = connectedWallNum;
				ParTime.numTypeThreeWalls++;
				if (removeUnlockableWalls)
					addObjectiveToList(objective, 1);
				else
					addObjectiveToList(objective, 0);
			}
		}
	}
}

// Find a path from a start segment to an objective.
// A lot of this is copied from the mark_player_path_to_segment function in game.c.
short create_path_partime(int start_seg, int target_seg, point_seg** path_start, int* path_count, partime_objective objective)
{
	object* objp = ConsoleObject;
	short player_path_length = 0;

	if (ParTime.isSegmentAccessible[target_seg])
		create_path_points(objp, start_seg, target_seg, Point_segs_free_ptr, &player_path_length, MAX_POINT_SEGS, 0, 0, -1, objective.type, objective.ID, 0);
	else
		create_path_points(objp, start_seg, target_seg, Point_segs_free_ptr, &player_path_length, MAX_POINT_SEGS, 0, 0, -1, objective.type, objective.ID, 1);

	*path_start = Point_segs_free_ptr;
	*path_count = player_path_length;

	if (Point_segs[player_path_length - 1].segnum != target_seg)
		return 0;

	return player_path_length;
}

double calculate_path_length_partime(point_seg* path, int path_count, partime_objective objective, int path_final)
{
	// Find length of path in units and return it.
	// Note: technically we should be using f2fl on the result of vm_vec_dist, but since the
	// multipliers are baked into the constants in calculateParTime already, maybe it's better to
	// leave it for now.
	double pathLength = 0;
	ParTime.pathObstructionTime = 0;
	if (path_count > 1) {
		for (int i = 0; i < path_count - 1; i++) {
			pathLength += vm_vec_dist(&path[i].point, &path[i + 1].point);
			// For objects, once we reach the target segment we move to the object to "pick it up".
			// Note: For now, this applies to robots, too.
			// Now, we account for the time it'd take to fight walls on the path (Abyss 1.0 par time hotfix lol). Originally I accounted for matcen fight time as well, but the change did more harm than good.
			int wall_num = Segments[path[i].segnum].sides[find_connecting_side(path[i].segnum, path[i + 1].segnum)].wall_num;
			if (wall_num > -1) {
				if (Walls[wall_num].type == WALL_BLASTABLE) {
					int skip = 0; // So it skips incrementing this path's time for a wall already marked done.
					for (int n = 0; n < ParTime.doneListSize; n++)
						if (ParTime.doneList[n].type == OBJECTIVE_TYPE_WALL && ParTime.doneList[n].ID == wall_num)
							skip = 1; // If algo's already destroyed this wall, don't account for it.
					if (!skip)
						ParTime.pathObstructionTime += calculate_combat_time_wall(wall_num, 0);
				}
			}
		}
		if (objective.type == OBJECTIVE_TYPE_OBJECT)
			pathLength += vm_vec_dist(&path[path_count - 1].point, &Objects[objective.ID].pos);
		// Paths to unreachable triggers will remain incomplete, but that's alright. They should never be unreachable anyway.
	}
	// Objective is in the same segment as the player. If it's an object, we still move to it.
	else if (objective.type == OBJECTIVE_TYPE_OBJECT)
		pathLength = vm_vec_dist(&ParTime.lastPosition, &Objects[objective.ID].pos);
	ParTime.pathObstructionTime *= SHIP_MOVE_SPEED; // Convert time to distance before adding.
	pathLength += ParTime.pathObstructionTime;
	return pathLength; // We still need pathLength, despite now adding to movementTime directly, because individual paths need compared.
}

int thisWallUnlocked(int wall_num, int currentObjectiveType, int currentObjectiveID, int warpBackPointCheck) // Does what the name says.
{
	int unlocked = 1;
	if (Walls[wall_num].type == WALL_DOOR && Walls[wall_num].keys > 1) {
		unlocked = findKeyObjectID(Walls[wall_num].keys, 0, 1);
		if (unlocked)
			return 1;
	}
	if ((Walls[wall_num].type == WALL_DOOR && Walls[wall_num].flags & WALL_DOOR_LOCKED) || Walls[wall_num].type == WALL_CLOSED || Walls[wall_num].type == WALL_CLOAKED) {
		unlocked = 0;
		for (int t = 0; t < Num_triggers; t++) {
			if (Triggers[t].type == TT_OPEN_DOOR || Triggers[t].type == TT_OPEN_WALL || Triggers[t].type == TT_UNLOCK_DOOR || Triggers[t].type == TT_ILLUSORY_WALL) {
				partime_objective objective;
				objective.type = OBJECTIVE_TYPE_INVALID;
				objective.ID = 0;
				for (int w = 0; w < Num_walls; w++)
					if (Walls[w].trigger == t) {
						objective.type = OBJECTIVE_TYPE_TRIGGER;
						objective.ID = w;
						break;
					}
				if (!objective.type)
					break; // No wall was found. We have an orphaned trigger, skip it. (Obsidian 4 fix)
				for (int l = 0; l < Triggers[t].num_links; l++) {
					int connectedWallNum = findConnectedWallNum(wall_num);
					if ((Triggers[t].seg[l] == Walls[wall_num].segnum && Triggers[t].side[l] == Walls[wall_num].sidenum) || (Triggers[t].type != TT_UNLOCK_DOOR && Triggers[t].seg[l] == Walls[connectedWallNum].segnum && Triggers[t].side[l] == Walls[connectedWallNum].sidenum))
						for (int n = 0; n < ParTime.doneListSize; n++)
							if (ParTime.doneList[n].type == objective.type && ParTime.doneList[n].ID == objective.ID)
								return 1;
				}
			}
		}
	}
	for (int i = 0; i < ParTime.numTypeThreeWalls; i++) {
		if (ParTime.typeThreeWalls[i] == wall_num) {
			unlocked = 0;
			for (int n = 0; n < ParTime.doneListSize; n++)
				if (ParTime.doneList[n].type == OBJECTIVE_TYPE_WALL && ParTime.doneList[n].ID == ParTime.typeThreeUnlockIDs[i])
					return 1;
		}
	}
	if (Walls[wall_num].type == WALL_DOOR && Walls[wall_num].flags & WALL_DOOR_LOCKED) {
		int connectedWallNum = findConnectedWallNum(wall_num);
		for (int i = 0; i < ControlCenterTriggers.num_links; i++)
			if ((ControlCenterTriggers.seg[i] == Walls[wall_num].segnum && ControlCenterTriggers.side[i] == Walls[wall_num].sidenum) || (ControlCenterTriggers.seg[i] == Walls[connectedWallNum].segnum && ControlCenterTriggers.side[i] == Walls[connectedWallNum].sidenum))
				unlocked = (ParTime.loops > 1);
	}
	if (!(unlocked || warpBackPointCheck)) // Let Algo through anyway if the wall is transparent and we're headed toward an unlock we don't have to go directly to (EG shooting through grate at unlocked side of door like S2).
		unlocked = (((currentObjectiveType == OBJECTIVE_TYPE_TRIGGER && Walls[currentObjectiveID].type != WALL_OPEN) ||
			currentObjectiveType == OBJECTIVE_TYPE_WALL ||
			(currentObjectiveType == OBJECTIVE_TYPE_OBJECT && (Objects[currentObjectiveID].type == OBJ_CNTRLCEN || (Objects[currentObjectiveID].type == OBJ_ROBOT && Robot_info[Objects[currentObjectiveID].id].boss_flag)))) &&
			check_transparency_partime(&Segments[Walls[wall_num].segnum], Walls[wall_num].sidenum));
	return unlocked;
}

int check_gap_size(int seg, int side) // Returns 1 if the gap can be fit through by the player, else returns 0.
{
	if (ParTime.sideSizes[seg][side] >= ConsoleObject->size * 2)
		return 1; // This side is big enough to fit through. We don't need to check the adjacent sides.
		
	// Each side has five adjacent sides: Above, below, left, right, and across. The only one we don't wanna check is the one directly across, as it's not connected to the original.
	// Since sidenums are always laid out the same, we know which one that is based on the original side.
	int skipSides[MAX_SIDES_PER_SEGMENT] = { 2, 3, 0, 1, 5, 4 };
	int skip = skipSides[side];
	int num_closed = 0;
	for (int i = 0; i < MAX_SIDES_PER_SEGMENT; i++)
		if (i != side && i != skip && Segments[seg].children[i] == -1) // Also skip the original side.
			num_closed++;
	return (num_closed < 4);
}

partime_objective find_nearest_objective_partime(int start_seg, point_seg** path_start, int* path_count, double* path_length)
{
	double pathLength;
	double shortestPathLength = -1;
	partime_objective nearestObjective;
	partime_objective objective;
	int i;
	int nearestSegnum = ParTime.segnum;
	int shortestDistance;
	int distance;
	int objectiveSegnum;
	short player_path_length;

	vms_vector start;
	compute_segment_center(&start, &Segments[start_seg]);

	for (i = 0; i < ParTime.toDoListSize; i++) {
		objective = ParTime.toDoList[i];
		objectiveSegnum = getObjectiveSegnum(objective);
		// Draw a path as far as we can to the objective, avoiding currently locked doors. If we don't make it all the way, ignore any closed walls. Primarily for shooting through grates, but prevents a softlock on actual uncompletable levels.
		player_path_length = create_path_partime(start_seg, objectiveSegnum, path_start, path_count, objective);
		// If we're shooting the unlockable side of a one-sided locked wall, make sure we have the keys needed to unlock it first.
		// Also CAN ignore this if it's a transparent door but I'm not too worried about this.
		if (objective.type == OBJECTIVE_TYPE_WALL && !thisWallUnlocked(objective.ID, OBJECTIVE_TYPE_WALL, objective.ID, 1))
			continue;
		if (!player_path_length)
			continue;
		pathLength = calculate_path_length_partime(*path_start, *path_count, objective, 0);
		if (pathLength < shortestPathLength || shortestPathLength < 0) {
			shortestPathLength = pathLength;
			nearestObjective = objective;
			ParTime.shortestPathObstructionTime = ParTime.pathObstructionTime; // So the wrong amount isn't subtracted when it's time to add the real path length to movement time.
		}
	}

	// Did we find a legal objective? Return that.
	if (shortestPathLength >= 0) {
		ParTime.warpBackPoint = -1;
		objectiveSegnum = getObjectiveSegnum(nearestObjective);
		// Regenerate the path since we may have checked something else in the meantime.
		player_path_length = create_path_partime(start_seg, objectiveSegnum, path_start, path_count, nearestObjective);
		*path_length = calculate_path_length_partime(*path_start, *path_count, nearestObjective, 1);
		// Now we need to find out where to place Algo for accessible objectives.
		// In the case of phasing through locked walls to get certain objectives, set it before the first transparent one. In the case of going into places that are too small, set it before that.
		int wall_num;
		int side_num;
		for (i = 0; i < player_path_length - 1; i++) {
			side_num = find_connecting_side(Point_segs[i].segnum, Point_segs[i + 1].segnum);
			wall_num = Segments[Point_segs[i].segnum].sides[side_num].wall_num;
			if (!ParTime.isSegmentAccessible[Point_segs[i + 1].segnum])
				if (ParTime.warpBackPoint == -1)
					ParTime.warpBackPoint = Point_segs[i].segnum;
			if (!thisWallUnlocked(wall_num, nearestObjective.type, nearestObjective.ID, 1))
				if (((nearestObjective.type == OBJECTIVE_TYPE_TRIGGER && Walls[nearestObjective.ID].type != WALL_OPEN) ||
					nearestObjective.type == OBJECTIVE_TYPE_WALL ||
					(nearestObjective.type == OBJECTIVE_TYPE_OBJECT && (Objects[nearestObjective.ID].type == OBJ_CNTRLCEN || (Objects[nearestObjective.ID].type == OBJ_ROBOT && Robot_info[Objects[nearestObjective.ID].id].boss_flag)))) &&
					check_transparency_partime(&Segments[Walls[wall_num].segnum], Walls[wall_num].sidenum))
					if (ParTime.warpBackPoint == -1)
						ParTime.warpBackPoint = Walls[wall_num].segnum;
			side_num = find_connecting_side(Point_segs[i + 1].segnum, Point_segs[i].segnum);
			if (!check_gap_size(Point_segs[i + 1].segnum, side_num) && ((nearestObjective.type == OBJECTIVE_TYPE_TRIGGER && Walls[nearestObjective.ID].type != WALL_OPEN) || nearestObjective.type == OBJECTIVE_TYPE_WALL || (nearestObjective.type == OBJECTIVE_TYPE_OBJECT && (Objects[nearestObjective.ID].type == OBJ_CNTRLCEN || (Objects[nearestObjective.ID].type == OBJ_ROBOT && Robot_info[Objects[nearestObjective.ID].id].boss_flag))) || !ParTime.isSegmentAccessible[objectiveSegnum]))
				if (ParTime.warpBackPoint == -1)
					ParTime.warpBackPoint = Point_segs[i].segnum;
			if (ParTime.warpBackPoint > -1)
				break; // We found where to put Algo. No need to go further.
		}
		if (ParTime.warpBackPoint == -1) {
			ParTime.segnum = objectiveSegnum;
			ParTime.lastPosition = getObjectivePosition(nearestObjective);
		}
		else {
			ParTime.segnum = ParTime.warpBackPoint;
			vms_vector segmentCenter;
			compute_segment_center(&segmentCenter, &Segments[ParTime.segnum]);
			ParTime.lastPosition = segmentCenter;
		}
		return nearestObjective;
	}
	else {
		// No reachable objectives in list.
		partime_objective emptyResult = { OBJECTIVE_TYPE_INVALID, 0 };
		return emptyResult;
	}
}

int getMatcenSegnum(int matcen_num)
{
	for (int i = 0; i < Highest_segment_index; i++)
		if (Segments[i].matcen_num == matcen_num)
			return i;
}

void examine_path_partime(point_seg* path, int path_count)
{
	// First, let's check if the ship can actually fit through this path. Thanks to Proxy for help with this part.
	int wall_num;
	int side_num;
	int adjacent_wall_num;
	// Find out if we have to fight a blastable wall on the way.
	int thisWallDestroyed = 0;
	for (int i = 0; i < path_count - 1; i++) {
		wall_num = Segments[path[i].segnum].sides[find_connecting_side(path[i].segnum, path[i + 1].segnum)].wall_num;
		adjacent_wall_num = Segments[path[i + 1].segnum].sides[find_connecting_side(path[i + 1].segnum, path[i].segnum)].wall_num;
		if (wall_num > -1) {
			if (Walls[wall_num].type == WALL_BLASTABLE) {
				for (int n = 0; n < ParTime.doneListSize; n++) {
					if (ParTime.doneList[n].type == OBJECTIVE_TYPE_WALL && ParTime.doneList[n].ID == wall_num) {
						thisWallDestroyed = 1;
						break; // If algo's already destroyed this wall, don't destroy it again.
					}
				}
				if (!thisWallDestroyed) {
					ParTime.combatTime += calculate_combat_time_wall(wall_num, 1);
					partime_objective objective = { OBJECTIVE_TYPE_WALL, wall_num };
					partime_objective adjacentObjective = { OBJECTIVE_TYPE_WALL, adjacent_wall_num };
					addObjectiveToList(objective, 1);
					addObjectiveToList(adjacentObjective, 1); // Mark the other side of the wall as done too, before algo gets to it. Only the hps of the side we're coming from applies, and it destroys both sides.
				}
			}
		}
		// How much time and ammo does it take to handle the matcens along the way? Let's find out!
		if (Num_robot_centers > 0) { // Don't bother constantly scanning the path for matcens on levels with no matcens.
			side_num = find_connecting_side(path[i].segnum, path[i + 1].segnum); // Find the side both segments share.
			wall_num = Segments[path[i].segnum].sides[side_num].wall_num; // Get its wall number.
			if (wall_num > -1) { // If that wall number is valid...
				if (Walls[wall_num].trigger > -1) { // If this wall has a trigger...
					if (Triggers[Walls[wall_num].trigger].type & TT_MATCEN) { // If this trigger is a matcen type...
						double matcenTime = 0;
						double totalMatcenTime = 0;
						for (int c = 0; c < Triggers[Walls[wall_num].trigger].num_links; c++) { // Repeat this loop for every segment linked to this trigger.
							if (Segments[Triggers[Walls[wall_num].trigger].seg[c]].special == SEGMENT_IS_ROBOTMAKER) { // Check them to see if they're matcens. 
								segment* segp = &Segments[Triggers[Walls[wall_num].trigger].seg[c]]; // Whenever one is, set this variable as a shortcut so we don't have to put that long string of text every time.
								if (RobotCenters[segp->matcen_num].robot_flags[0] + RobotCenters[segp->matcen_num].robot_flags[1] > 0 && ParTime.matcenLives[segp->matcen_num] > 0) { // If the matcen has robots in it, and isn't dead, consider it triggered...
									uint	flags;
									sbyte	legal_types[64];		//	64 bits, the width of robot_flags.
									int	num_types, robot_index, r;
									robot_index = 0;
									num_types = 0;
									for (r = 0; r < 2; r++) {
										robot_index = r * 32;
										flags = RobotCenters[segp->matcen_num].robot_flags[r];
										while (flags) {
											if (flags & 1)
												legal_types[num_types++] = robot_index;
											flags >>= 1;
											robot_index++;
										}
									}
									// Find the average fight time for the robots in this matcen and multiply that by the spawn count on this difficulty.
									int n;
									double totalRobotTime = 0;
									double totalAmmoUsage = 0;
									double averageRobotTime = 0;
									for (n = 0; n < num_types; n++) {
										robot_info* robInfo = &Robot_info[legal_types[n]];
										if (!(robInfo->behavior == AIB_RUN_FROM || robInfo->thief)) { // Skip running bots and thieves.
											totalRobotTime += calculate_combat_time(NULL, robInfo, 0, 1);
											if (robInfo->contains_type == OBJ_ROBOT) {
												totalRobotTime += calculate_combat_time(NULL, &Robot_info[robInfo->contains_id], 0, 1) * round((robInfo->contains_count * (robInfo->contains_prob / 16)));
												robotHasPowerup(robInfo->contains_id, (double)(1 / num_types));
											}
											else
												robotHasPowerup(legal_types[n], (double)(1 / num_types));
										}
										totalAmmoUsage += ParTime.ammo_usage;
									}
									averageRobotTime = totalRobotTime / num_types;
									matcenTime += averageRobotTime * (Difficulty_level + 3);
									if (Difficulty_level < 4) // Matcens in D2 have infinite lives on Insane.
										ParTime.matcenLives[segp->matcen_num]--;
									if (Triggers[Walls[wall_num].trigger].type & TF_ONE_SHOT) // So one shot triggers only work the first time.
										ParTime.matcenLives[segp->matcen_num] = 0;
									ParTime.vulcanAmmo -= ((totalAmmoUsage / num_types) * (f1_0 * (Difficulty_level + 3))); // and ammo, as those also change per matcen.
									if (ParTime.vulcanAmmo > STARTING_VULCAN_AMMO * 8) // Vulcan ammo can exceed 32768 and overflow if not capped properly. Prevent this from happening.
										ParTime.vulcanAmmo = STARTING_VULCAN_AMMO * 8;
									if (ParTime.vulcanAmmo < 0) // Just cap vulcan ammo at 0 if it goes negative here. This isn't the right way to handle things, but doing it right would get very complex.
										ParTime.vulcanAmmo = 0;
									if (matcenTime > 0)
										printf("Fought matcen %i at segment %i; lives left: %i\n", segp->matcen_num, getMatcenSegnum(segp->matcen_num), ParTime.matcenLives[segp->matcen_num]);
									totalMatcenTime += averageRobotTime; // Add up the average fight times of each link so we can add them to the minimum time later.
								}
							}
						}
						// There's a minimum time for all matcen robots spawned on this path to be killed.
						if (matcenTime > 0) {
							if (matcenTime < 3.5 * (Difficulty_level + 2) + totalMatcenTime)
								matcenTime = 3.5 * (Difficulty_level + 2) + totalMatcenTime;
							printf("Total fight time: %.3fs\n", matcenTime);
						}
						ParTime.combatTime += matcenTime;
						ParTime.matcenTime += matcenTime;
					}
				}
			}
		}
		// If there's ammo in this segment, collect it.
		// Technically this will be a little inaccurate when Algo hits the ammo cap, because ammo powerups in D2 have a "capicity" that gets used partially until it's gone, to prevent being wasted.
		// I don't mind this though, as Algo hitting the ammo cap typically means the level has far more than plenty to last it the entire time.
		for (int objNum = 0; objNum <= Highest_object_index; objNum++) {
			if (Objects[objNum].type == OBJ_POWERUP && (Objects[objNum].id == POW_VULCAN_AMMO || (Objects[objNum].id == POW_VULCAN_WEAPON && !ParTime.heldWeapons[VULCAN_ID]) || (Objects[objNum].id == POW_GAUSS_WEAPON && !ParTime.heldWeapons[GAUSS_ID])) && Objects[objNum].segnum == path[i].segnum) {
				// ...make sure we didn't already get this one
				int thisSourceCollected = 0;
				for (int j = 0; j < ParTime.doneListSize; j++)
					if (ParTime.doneList[j].type == OBJECTIVE_TYPE_OBJECT && ParTime.doneList[j].ID == objNum) {
						thisSourceCollected = 1;
						break;
					}
				if (!thisSourceCollected) {
					ParTime.vulcanAmmo += STARTING_VULCAN_AMMO / 2;
					partime_objective ammoObjective = { OBJECTIVE_TYPE_OBJECT, objNum };
					addObjectiveToList(ammoObjective, 1);
				}
			}
		}
	}
}

void respond_to_objective_partime(partime_objective objective)
{
	int weapon_id;
	int i;
	if (objective.type == OBJECTIVE_TYPE_OBJECT) { // We don't fight triggers.
		object* obj = &Objects[objective.ID];
		if (obj->type == OBJ_HOSTAGE)
			if (Current_level_num > 0)
				Ranking.maxScore += HOSTAGE_SCORE;
			else
				Ranking.secretMaxScore += HOSTAGE_SCORE;
		if (obj->type == OBJ_POWERUP) {
			weapon_id = 0;
			if (obj->id == POW_VULCAN_WEAPON)
				weapon_id = VULCAN_ID;
			if (obj->id == POW_SPREADFIRE_WEAPON)
				weapon_id = SPREADFIRE_ID;
			if (obj->id == POW_PLASMA_WEAPON)
				weapon_id = PLASMA_ID;
			if (obj->id == POW_FUSION_WEAPON)
				weapon_id = FUSION_ID;
			if (obj->id == POW_GAUSS_WEAPON)
				weapon_id = GAUSS_ID;
			if (obj->id == POW_HELIX_WEAPON)
				weapon_id = HELIX_ID;
			if (obj->id == POW_PHOENIX_WEAPON)
				weapon_id = PHOENIX_ID;
			if (obj->id == POW_OMEGA_WEAPON)
				weapon_id = OMEGA_ID;
			if (weapon_id) { // If the powerup we got is a new weapon, add it to the list of weapons algo has.
				if ((weapon_id == VULCAN_ID || weapon_id == GAUSS_ID) && ParTime.heldWeapons[weapon_id])
					ParTime.vulcanAmmo += STARTING_VULCAN_AMMO;
				else
					ParTime.vulcanAmmo += STARTING_VULCAN_AMMO / 2;
				ParTime.heldWeapons[weapon_id] = 0;
			}
			else {
				if (obj->id == POW_LASER)
					if (ParTime.laser_level < LASER_ID_L4)
						ParTime.laser_level++;
				if (obj->id == POW_QUAD_FIRE)
					ParTime.hasQuads = 0;
				if (obj->id == POW_SUPER_LASER) {
					if (ParTime.laser_level < LASER_ID_L5)
						ParTime.laser_level = LASER_ID_L5;
					else
						ParTime.laser_level = LASER_ID_L6;
					ParTime.heldWeapons[1] = 0;
					ParTime.heldWeapons[2] = 0;
					ParTime.heldWeapons[3] = 0;
				}
				ParTime.heldWeapons[ParTime.laser_level] = 0;
				if (obj->id == POW_AFTERBURNER) {
					ParTime.hasAfterburner = 0;
					double afterburnerMultipliers[5] = { 1.2, 1.15, 1.11, 1.08, 1.05 };
					ParTime.afterburnerMultiplier = afterburnerMultipliers[Difficulty_level];
				}
				if (obj->id == POW_EXTRA_LIFE)
					if (Current_level_num > 0)
						Ranking.maxScore += 10000;
					else
						Ranking.secretMaxScore += 10000;
			}
		}
		if (obj->type == OBJ_ROBOT || obj->type == OBJ_CNTRLCEN) { // We don't fight keys, hostages or weapons.
			robot_info* robInfo = &Robot_info[obj->id];
			double combatTime = 0;
			double fightTime;
			if (obj->type == OBJ_CNTRLCEN) {
				combatTime += calculate_combat_time(obj, robInfo, 1, 0);
				if (Current_level_num > 0)
					Ranking.maxScore += CONTROL_CEN_SCORE;
				else
					Ranking.secretMaxScore += CONTROL_CEN_SCORE;
			}
			else {
				combatTime += calculate_combat_time(obj, robInfo, 1, 0);
				if (Current_level_num > 0)
					Ranking.maxScore += robInfo->score_value;
				else
					Ranking.secretMaxScore += robInfo->score_value;
				double teleportDistance = 0;
				short Boss_path_length = 0;
				if (robInfo->boss_flag > 0) { // Bosses have special abilities that take additional time to counteract. Boss levels are unfair without this.
					if (Boss_teleports[robInfo->boss_flag - BOSS_D2]) {
						int num_teleports = combatTime / 8; // Bosses teleport on an eight second timer, meaning you can only get two seconds of damage in at a time before they move away.
						for (i = 0; i < Num_boss_teleport_segs; i++) { // Now we measure the distance between every possible pair of points the boss can teleport between.
							for (int n = 0; n < Num_boss_teleport_segs; n++) {
								create_path_points(obj, Boss_teleport_segs[i], Boss_teleport_segs[n], Point_segs_free_ptr, &Boss_path_length, MAX_POINT_SEGS, 0, 0, -1, 0, obj->id, 1); // Assume inaccesibility here so invalid paths don't get super long and drive up teleport time.
								for (int c = 0; c < Boss_path_length - 1; c++)
									teleportDistance += vm_vec_dist(&Point_segs[c].point, &Point_segs[c + 1].point);
							}
						}
						double teleportTime = ((teleportDistance / pow(Num_boss_teleport_segs, 2)) * num_teleports) / SHIP_MOVE_SPEED; // Account for the average teleport distance, not highest.
						// Use average teleport time, not total, for afterburner speed bonus. Each teleport is an individual move between shooting the boss.
						if (teleportTime) { // Not putting this causes a div 0 error on levels where bosses have low enough health.
							ParTime.movementTime += teleportTime / (teleportTime / num_teleports > 11 ? ParTime.afterburnerMultiplier : pow(ParTime.afterburnerMultiplier, (teleportTime / num_teleports) / 11));
							ParTime.movementTimeNoAB += teleportTime;
						}
						printf("Teleport time: %.3fs\n", teleportTime);
					}
				}
				if (obj->contains_type == OBJ_ROBOT && obj->contains_count) {
					robInfo = &Robot_info[obj->contains_id];
					fightTime = calculate_combat_time(obj, robInfo, 0, 0) * obj->contains_count;
					combatTime += fightTime;
					if (Current_level_num > 0)
						Ranking.maxScore += robInfo->score_value * obj->contains_count;
					else
						Ranking.secretMaxScore += robInfo->score_value * obj->contains_count;
					printf("Took %.3fs to fight %i of robot type %i\n", fightTime, obj->contains_count, obj->contains_id);
				}
				else if (robInfo->contains_type == OBJ_ROBOT) {
					int assumedOffSpringCount = round(((double)robInfo->contains_count * ((double)robInfo->contains_prob / 16)));
					fightTime = calculate_combat_time(obj, &Robot_info[robInfo->contains_id], 0, 0) * assumedOffSpringCount;
					combatTime += fightTime;
					printf("Took %.3fs to fight %i of robot type %i\n", fightTime, assumedOffSpringCount, robInfo->contains_id);
				}
			}
			ParTime.combatTime += combatTime;
			if (ParTime.warpBackPoint == -1) { // Don't get stuff from a robot if you have kill it from a distance (makes Hydro 17 less unfair).
				if (obj->contains_type == OBJ_POWERUP) {
					weapon_id = 0;
					if (obj->contains_id == POW_VULCAN_WEAPON)
						weapon_id = VULCAN_ID;
					if (obj->contains_id == POW_SPREADFIRE_WEAPON)
						weapon_id = SPREADFIRE_ID;
					if (obj->contains_id == POW_PLASMA_WEAPON)
						weapon_id = PLASMA_ID;
					if (obj->contains_id == POW_FUSION_WEAPON)
						weapon_id = FUSION_ID;
					if (obj->contains_id == POW_GAUSS_WEAPON)
						weapon_id = GAUSS_ID;
					if (obj->contains_id == POW_HELIX_WEAPON)
						weapon_id = HELIX_ID;
					if (obj->contains_id == POW_PHOENIX_WEAPON)
						weapon_id = PHOENIX_ID;
					if (obj->contains_id == POW_OMEGA_WEAPON)
						weapon_id = OMEGA_ID;
					if (weapon_id) { // If the powerup we got is a new weapon, add it to the list of weapons algo has.
						if ((weapon_id == VULCAN_ID || weapon_id == GAUSS_ID) && ParTime.heldWeapons[weapon_id])
							ParTime.vulcanAmmo += STARTING_VULCAN_AMMO;
						else
							ParTime.vulcanAmmo += STARTING_VULCAN_AMMO / 2;
						ParTime.heldWeapons[weapon_id] = 0;
					}
					else {
						int i;
						for (i = 0; i < obj->contains_count; i++) {
							if (obj->contains_id == POW_LASER)
								if (ParTime.laser_level < LASER_ID_L4)
									ParTime.laser_level++;
							if (obj->contains_id == POW_SUPER_LASER) {
								if (ParTime.laser_level < LASER_ID_L5)
									ParTime.laser_level = LASER_ID_L5;
								else
									ParTime.laser_level = LASER_ID_L6;
								ParTime.heldWeapons[1] = 0;
								ParTime.heldWeapons[2] = 0;
								ParTime.heldWeapons[3] = 0;
							}
							ParTime.heldWeapons[ParTime.laser_level] = 0;
						}
						if (obj->contains_id == POW_QUAD_FIRE)
							ParTime.hasQuads = 0;
						if (obj->contains_id == POW_AFTERBURNER) {
							ParTime.hasAfterburner = 0;
							double afterburnerMultipliers[5] = { 1.2, 1.15, 1.11, 1.08, 1.05 };
							ParTime.afterburnerMultiplier = afterburnerMultipliers[Difficulty_level];
						}
						if (obj->contains_id == POW_VULCAN_AMMO)
							ParTime.vulcanAmmo += (STARTING_VULCAN_AMMO / 2) * obj->contains_count;
						if (obj->contains_id == POW_EXTRA_LIFE)
							if (Current_level_num > 0)
								Ranking.maxScore += 10000 * obj->contains_count;
							else
								Ranking.secretMaxScore += 10000 * obj->contains_count;
					}
				}
				else
					robotHasPowerup(obj->id, 1); // This is where we automatically give Algo weapons based on probabilities.
			}
		}
	}
}

int getParTimeWeaponID(int index)
{
	int weaponIDs[10] = { 0, VULCAN_ID, SPREADFIRE_ID, PLASMA_ID, FUSION_ID, 0, GAUSS_ID, HELIX_ID, PHOENIX_ID, OMEGA_ID };
	if (Players[Player_num].laser_level < 5)
		weaponIDs[0] = Players[Player_num].laser_level;
	else
		weaponIDs[0] = Players[Player_num].laser_level + 25;
	return weaponIDs[index];
}

int determineSegmentAccessibility(int segnum)
{
	object* objp = ConsoleObject;
	short player_path_length = 0;
	create_path_points(objp, objp->segnum, segnum, Point_segs_free_ptr, &player_path_length, MAX_POINT_SEGS, 0, 0, -1, -1, -1, 0);
	if (Point_segs[player_path_length - 1].segnum != segnum) { // The segment is inaccessible if Algo doesn't end up at it when given the rules established by initLockedWalls.
		if (Current_level_num > 0) // Secret levels don't have secret return segments.
			create_path_points(objp, Secret_return_segment, segnum, Point_segs_free_ptr, &player_path_length, MAX_POINT_SEGS, 0, 0, -1, -1, -1, 0); // Try again from secret return seg.
		if (Point_segs[player_path_length - 1].segnum != segnum)
			return 0;
	}
	return 1;
}

void calculateParTime() // Here is where we have an algorithm run a simulated path through a level to determine how long the player should take, both flying around and fighting robots.
{ // January 2024 me would crap himself if he saw this actually working lol.
	// We'll call the par time algorithm Algo for short.
	fix64 start_timer_value, end_timer_value; // For tracking how long this algorithm takes to run.
	ParTime.movementTime = 0; // Variable to track how much distance it's travelled.
	ParTime.movementTimeNoAB = 0;
	ParTime.combatTime = 0; // Variable to track how much fighting it's done.
	// Now clear its checklists.
	ParTime.toDoListSize = 0;
	ParTime.doneListSize = 0;
	ParTime.segnum = ConsoleObject->segnum; // Start Algo off where the player spawns.
	ParTime.lastPosition = ConsoleObject->pos; // Both in segnum and in coordinates. (Shoutout to Maximum level 17's quads being at spawn for letting me catch this.)
	int lastSegnum = ConsoleObject->segnum; // So the printf showing paths to and from segments works.
	int i;
	int j;
	double pathLength; // Store create_path_partime's result in pathLength to compare to current shortest.
	double matcenTime = 0; // Debug variable to see how much time matcens are adding to the par time.
	point_seg* path_start; // The current path we are looking at (this is a pointer into somewhere in Point_segs).
	int path_count; // The number of segments in the path we're looking at.
	ParTime.vulcanAmmo = 0;
	// The values for each held weapon go by weapon ID, and are set based on how likely it is Algo doesn't have the weapon. It's only allowed to use a certain ID's weapon if its index is 0.
	// They start at 1, getting influenced by robot drop chances (automatically set to 0 if a weapon is picked up directly). Once it reaches a certain decimal value (1/16 chance), it automatically sets to 0.
	ParTime.heldWeapons[0] = 0;
	for (i = 1; i < 36; i++)
		ParTime.heldWeapons[i] = 1;
	ParTime.heldWeapons[FLARE_ID] = 0;
	// Quads and afterburner work the same.
	ParTime.hasQuads = 1;
	ParTime.hasAfterburner = 1;
	ParTime.laser_level = 0;
	ParTime.afterburnerMultiplier = 1;
	ParTime.thiefKeys = 0;
	ParTime.matcenTime = 0;
	ParTime.missingKeys = 0;
	if (Current_level_num > 0)
		Ranking.maxScore = 0;
	else
		Ranking.secretMaxScore = 0;

	// Calculate start time.
	timer_update();
	start_timer_value = timer_query();

	ParTime.loops = 2; // So the accessibility check doesn't trip up on reactor stuff.
	// Populate the locked walls list.
	initLockedWalls(1);

	for (i = 0; i <= Highest_segment_index; i++) // Iterate through every side of every segment, measuring their sizes.
		for (int s = 0; s < 6; s++)
			if (Segments[i].children[s] > -1) { // Don't measure closed sides. We can't go through them anyway.
				// Measure the distance between all of that side's verts to determine whether we can fit. ai_door_is_openable will use them later to disallow passage if we can't.
				int a = vm_vec_dist(&Vertices[Segments[i].verts[Side_to_verts[s][0]]], &Vertices[Segments[i].verts[Side_to_verts[s][1]]]);
				int b = vm_vec_dist(&Vertices[Segments[i].verts[Side_to_verts[s][1]]], &Vertices[Segments[i].verts[Side_to_verts[s][2]]]);
				int c = vm_vec_dist(&Vertices[Segments[i].verts[Side_to_verts[s][2]]], &Vertices[Segments[i].verts[Side_to_verts[s][3]]]);
				int d = vm_vec_dist(&Vertices[Segments[i].verts[Side_to_verts[s][3]]], &Vertices[Segments[i].verts[Side_to_verts[s][0]]]);
				int min_x = max(a, c);
				int min_y = max(b, d);
				ParTime.sideSizes[i][s] = min(min_x, min_y); // Thanks to ProxyOne for helping with the calculations.
			}
			else
				ParTime.sideSizes[i][s] = ConsoleObject->size * 2; // If a side is closed, mark it down as big enough.
	for (i = 0; i <= Highest_segment_index; i++) // Lay out the map for where the "inaccessible" territory is so we can mark objectives within it as such.
		ParTime.isSegmentAccessible[i] = determineSegmentAccessibility(i);

	initLockedWalls(0);
	ParTime.doneListSize = 0; // So locked walls can be detected correctly.
	ParTime.loops = 0; // How many times the pathmaking process has repeated. This determines what toDoList is populated with, to make sure things are gone to in the right order.

	// Initialize all matcens to 3 lives. On Insane, this counter won't decrement.
	for (i = 0; i < Num_robot_centers; i++)
		ParTime.matcenLives[i] = 3;

	while (ParTime.loops < 4) {
		// Collect our objectives at this stage...
		if (ParTime.loops == 0) {
			for (i = 0; i <= Highest_object_index; i++) { // Populate the to-do list with all robots, hostages, weapons, and laser powerups. Ignore robots not worth over zero, as the player isn't gonna go for those. This should never happen, but it's just a failsafe. Also ignore any thieves that aren't carrying keys.
				if ((Objects[i].type == OBJ_ROBOT && !Robot_info[Objects[i].id].boss_flag && !Robot_info[Objects[i].id].companion && !(Robot_info[Objects[i].id].thief && !robotHasKey(&Objects[i]))) || Objects[i].type == OBJ_HOSTAGE || (Objects[i].type == OBJ_POWERUP && (Objects[i].id == POW_EXTRA_LIFE || Objects[i].id == POW_LASER || Objects[i].id == POW_QUAD_FIRE || Objects[i].id == POW_VULCAN_WEAPON || Objects[i].id == POW_SPREADFIRE_WEAPON || Objects[i].id == POW_PLASMA_WEAPON || Objects[i].id == POW_FUSION_WEAPON || Objects[i].id == POW_SUPER_LASER || Objects[i].id == POW_GAUSS_WEAPON || Objects[i].id == POW_HELIX_WEAPON || Objects[i].id == POW_PHOENIX_WEAPON || Objects[i].id == POW_OMEGA_WEAPON || Objects[i].id == POW_AFTERBURNER))) {
					partime_objective objective = { OBJECTIVE_TYPE_OBJECT, i };
					addObjectiveToList(objective, 0);
				}
			}
			if (ParTime.missingKeys) // If there are missing keys, path to secret exit portals as well. The keys are probably there.
				for (i = 0; i <= Num_triggers; i++)
					if (Triggers[i].type == TT_SECRET_EXIT)
						for (j = 0; j < Num_walls; j++)
							if (Walls[j].trigger == i && !(Walls[j].type == WALL_CLOSED || Walls[j].type == WALL_CLOAKED)) { // Ignore exit triggers attached to walls we can't pass through.
								partime_objective objective = { OBJECTIVE_TYPE_TRIGGER, findConnectedWallNum(j) };
								addObjectiveToList(objective, 0);
							}
		}
		if (ParTime.loops == 1) {
			int levelHasReactor = 0;
			for (i = 0; i <= Highest_object_index; i++) { // Populate the to-do list with all reactors and bosses.
				if (Objects[i].type == OBJ_CNTRLCEN) {
					partime_objective objective = { OBJECTIVE_TYPE_OBJECT, i };
					addObjectiveToList(objective, 0);
					levelHasReactor = 1;
				}
			}
			if (!levelHasReactor) {
				int bossScore;
				int highestBossScore = 0;
				int targetedBossID = -1;
				for (i = 0; i <= Highest_object_index; i++) {
					if (Objects[i].type == OBJ_ROBOT && Robot_info[Objects[i].id].boss_flag) { // Look at every boss, adding only the highest point value.
						// Killing any boss in levels with multiple kills ALL of them, only giving points for the one directly killed, so the player needs to target the highest-scoring one for the best rank. Give them the time needed for that one.
						// This means players may have to replay levels a bunch to find which boss gives most out of custom bosses/values, but I don't thinks that's a cause for great concern.
						bossScore = Robot_info[Objects[i].id].score_value;
						if (Objects[i].contains_type == OBJ_ROBOT && ((Objects[i].id != Robot_info[Objects[i].id].contains_id) || (Objects[i].id != Robot_info[Objects[i].contains_id].contains_id))) // So points in infinite robot drop loops aren't counted past the parent bot.
							bossScore += Robot_info[Objects[i].contains_id].score_value * Objects[i].contains_count;
						if (Objects[i].contains_type == OBJ_POWERUP && Objects[i].contains_id == POW_EXTRA_LIFE)
							bossScore += Objects[i].contains_count * 10000;
						ParTime.combatTime += 6.2; // Each boss has its own deathroll that lasts six seconds one at a time.
						if (bossScore > highestBossScore) {
							highestBossScore = bossScore;
							targetedBossID = i;
						}
					}
				}
				for (int n = 0; n < ParTime.doneListSize; n++) // Don't get the boss if we already had to get him due to key stuff (LOTW The Pit).
					if (ParTime.doneList[n].type == OBJECTIVE_TYPE_OBJECT && ParTime.doneList[n].ID == targetedBossID)
						targetedBossID = -1;
				if (targetedBossID > -1) { // If a level doesn't have a reactor OR boss, don't add the non-existent boss to the to-do list.
					partime_objective objective = { OBJECTIVE_TYPE_OBJECT, targetedBossID };
					addObjectiveToList(objective, 0);
					// Let's hope no one makes a boss be worth negative points.
				}
			}
		}
		if (ParTime.loops == 3) { // Put the exit on the list.
			for (i = 0; i <= Num_triggers; i++)
				if (Triggers[i].type == TT_EXIT || Triggers[i].type == TT_SECRET_EXIT)
					for (j = 0; j < Num_walls; j++)
						if (Walls[j].trigger == i && !(Walls[j].type == WALL_CLOSED || Walls[j].type == WALL_CLOAKED)) { // Ignore exit triggers attached to walls we can't pass through.
							partime_objective objective;
							if (findConnectedWallNum(j) != -1) {
								objective.type = OBJECTIVE_TYPE_TRIGGER;
								objective.ID = findConnectedWallNum(j);
							}
							else {
								objective.type = OBJECTIVE_TYPE_TRIGGER;
								objective.ID = j;
							}
							addObjectiveToList(objective, 0);
						}
		}

		while (ParTime.toDoListSize > 0) {
			// Find which object on the to-do list is the closest, ignoring the reactor/boss if it's not the only thing left.
			partime_objective nearestObjective =
				find_nearest_objective_partime(ParTime.segnum, &path_start, &path_count, &pathLength);

			if (nearestObjective.type == OBJECTIVE_TYPE_INVALID) {
				// This should only happen if there are no reachable objectives left in the list.
				// If that happens, we're done with this phase.
				// Just to be sure though, make one last ditch effort to find objectives. It might save Algo from impending doom.
				// Teleport it to any accessible segments Algo hasn't visited yet, and try to find the nearest objective again. This should take care of Obsidian level 1 and levels like it.
				// In D2, however, we prioritize the secret level return segment.
				if (!ParTime.loops && Current_level_num > 0) // Only do this on the first loop before reactor is destroyed, as you can't come back from the secret level after it blows. Also secret levels still don't have secret return segments.
					for (i = 0; i < Num_triggers; i++) {
						if (Triggers[i].type == TT_SECRET_EXIT) {
							ParTime.segnum = Secret_return_segment;
							vms_vector segmentCenter;
							compute_segment_center(&segmentCenter, &Segments[Secret_return_segment]);
							ParTime.lastPosition = segmentCenter;
							nearestObjective =
								find_nearest_objective_partime(ParTime.segnum, &path_start, &path_count, &pathLength);
							if (!(nearestObjective.type == OBJECTIVE_TYPE_INVALID))
								printf("Can't reach all objectives! Teleporting to avoid softlock.\n");
							else
								ParTime.segnum = lastSegnum;
							break;
						}
					}
				break;
			}

			// Mark this objective as done.
			removeObjectiveFromList(nearestObjective);
			addObjectiveToList(nearestObjective, 1);

			// Track resource consumption and robot HP destroyed.
			// If there's no path and we're doing straight line distance, we have no idea what we'd
			// be crossing through, so tracking resources for the path would be meaningless.
			// We can still check the objective itself, though.
			int hasThisObjective = 0; // If the next object is a weapon/laser level/quads, and algo already has it/is maxed out, skip it. We don't wanna waste time getting redundant powerups.
			int key; // For thief key stuff.
			if (nearestObjective.type == OBJECTIVE_TYPE_OBJECT && Objects[nearestObjective.ID].type == OBJ_POWERUP) { // I'm splitting up the if conditions this time.
				if (!ParTime.isSegmentAccessible[Objects[nearestObjective.ID].segnum]) // Don't go to a powerup that's inaccessible. We have to touch powerups directly to collect them.
					hasThisObjective = 1;
				int weaponIDs[9] = { 0, VULCAN_ID, SPREADFIRE_ID, PLASMA_ID, FUSION_ID, GAUSS_ID, HELIX_ID, PHOENIX_ID, OMEGA_ID };
				int objectIDs[9] = { 0, POW_VULCAN_WEAPON, POW_SPREADFIRE_WEAPON, POW_PLASMA_WEAPON, POW_FUSION_WEAPON, POW_GAUSS_WEAPON, POW_HELIX_WEAPON, POW_PHOENIX_WEAPON, POW_OMEGA_WEAPON };
				for (int n = 1; n < 9; n++)
					if (Objects[nearestObjective.ID].id == objectIDs[n] && !ParTime.heldWeapons[weaponIDs[n]])
						hasThisObjective = 1;
				if (Objects[nearestObjective.ID].id == POW_LASER && !ParTime.heldWeapons[LASER_ID_L4])
					hasThisObjective = 1;
				if (Objects[nearestObjective.ID].id == POW_SUPER_LASER && !ParTime.heldWeapons[LASER_ID_L6])
					hasThisObjective = 1;
				if (Objects[nearestObjective.ID].id == POW_QUAD_FIRE && !ParTime.hasQuads)
					hasThisObjective = 1;
				if (Objects[nearestObjective.ID].id == POW_AFTERBURNER && !ParTime.hasAfterburner)
					hasThisObjective = 1;
				if (((Objects[nearestObjective.ID].id == POW_VULCAN_WEAPON && !ParTime.heldWeapons[VULCAN_ID]) || (Objects[nearestObjective.ID].id == POW_GAUSS_WEAPON && !ParTime.heldWeapons[GAUSS_ID])) && ParTime.vulcanAmmo == STARTING_VULCAN_AMMO * 8)
					hasThisObjective = 1;
			}
			if (Objects[nearestObjective.ID].type == OBJ_ROBOT) // Only allow one thief to count toward par time per contained key color. (Fixes Bahagad Outbreak level 8.)
				if (ParTime.isSegmentAccessible[Objects[nearestObjective.ID].segnum]) // Don't count inaccessible thieves toward anything related to keys.
					if (Robot_info[Objects[nearestObjective.ID].id].thief) {
						key = robotHasKey(&Objects[nearestObjective.ID]);
						if (key)
							if (ParTime.thiefKeys & key)
								hasThisObjective = 1;
							else
								ParTime.thiefKeys |= key;
					}
			if (!hasThisObjective) {
				double movementTimeIncrease = ((pathLength - ParTime.shortestPathObstructionTime) / SHIP_MOVE_SPEED);
				int nearestObjectiveSegnum = getObjectiveSegnum(nearestObjective);
				printf("Path from segment %i to %i: %.3fs (AB mult: %.3f)\n", lastSegnum, nearestObjectiveSegnum, pathLength / SHIP_MOVE_SPEED, (movementTimeIncrease > 11 ? ParTime.afterburnerMultiplier : pow(ParTime.afterburnerMultiplier, movementTimeIncrease / 11)));
				respond_to_objective_partime(nearestObjective);
				if (path_start != NULL)
					examine_path_partime(path_start, path_count);
				// Cap algo's energy and ammo like the player's.
				if (ParTime.vulcanAmmo > STARTING_VULCAN_AMMO * 8)
					ParTime.vulcanAmmo = STARTING_VULCAN_AMMO * 8;
				// Now move ourselves to the objective for the next pathfinding iteration, unless the objective wasn't reachable with just flight, in which case move ourselves as far as we COULD fly.
				// The move speed multiplier for the afterburner applies over time, so only long stretches of not fighting give a significant par time decrease. 11s is the time to fully use then replenish its charge.
				ParTime.movementTime += movementTimeIncrease / (movementTimeIncrease > 11 ? ParTime.afterburnerMultiplier : pow(ParTime.afterburnerMultiplier, movementTimeIncrease / 11));
				ParTime.movementTimeNoAB += movementTimeIncrease;
				lastSegnum = ParTime.segnum;
			}
			else
				ParTime.segnum = lastSegnum; // find_nearest_objective_partime just tried to set Algo's segnum to something, but it shouldn't be in this case, so force it back.
			if (ParTime.loops == 1) // An accessible reactor or boss gives us a result screen.
				Ranking.isRankable = 1;
			if (ParTime.loops == 3) {
				int wall_num = findConnectedWallNum(nearestObjective.ID);
				if (wall_num == -1)
					wall_num = nearestObjective.ID;
				if (Triggers[Walls[wall_num].trigger].type == TT_EXIT && !(Walls[wall_num].type == WALL_CLOSED || Walls[wall_num].type == WALL_CLOAKED)) // Ignore exit triggers attached to walls we can't pass through.
					Ranking.isRankable = 1; // An accessible exit trigger gives us a result screen. NOT a secret exit, as those only give us results when a reactor or boss is killed, which already sets isRankable to 1.
				break; // Automatically break after one objective during the exits loop. We only wanna get the nearest accessible exit.
			}
		}
		ParTime.loops++;
	}
	
	// To account for time and skill bonuses being equal to this, as well as hostage bonus.
	if (Current_level_num > 0) {
		Ranking.maxScore *= 3;
		Ranking.maxScore += Players[Player_num].hostages_level * 7500;
	}
	else {
		Ranking.secretMaxScore *= 3;
		Ranking.secretMaxScore += Ranking.hostages_secret_level * 7500;
	}
	
	// Calculate end time.
	timer_update();
	end_timer_value = timer_query();
 	printf("Par time: %.3fs (%.3f movement, %.3f combat) Matcen time: %.3fs, AB saved %.3fs (%.2f percent)\nCalculation time: %.3fs\n",
		ParTime.movementTime + ParTime.combatTime,
		ParTime.movementTime,
		ParTime.combatTime,
		ParTime.matcenTime,
		ParTime.movementTimeNoAB - ParTime.movementTime,
		((ParTime.movementTimeNoAB - ParTime.movementTime) / (ParTime.movementTimeNoAB + ParTime.combatTime)) * 100,
		f2fl(end_timer_value - start_timer_value));
	
	// Par time is rounded up to the nearest five seconds so it looks better/legible on the result screen, leaves room for the time bonus, and looks like a human set it.
	if (Current_level_num > 0)
		Ranking.parTime = 5 * (1 + (int)(ParTime.movementTime + ParTime.combatTime) / 5);
	else
		Ranking.secretParTime = 5 * (1 + (int)(ParTime.movementTime + ParTime.combatTime) / 5);
	// Also because Doom did five second increments.
	// Par time will vary based on difficulty, so the player will always have to go fast for a high time bonus, even on lower difficulties.
}

// - - - - - - - - - -  END OF PAR TIME ALGORITHM STUFF  - - - - - - - - - - \\

// called when the player is starting a new level for normal game mode and restore state
//	Need to deal with whether this is the first time coming to this level or not.  If not the
//	first time, instead of initializing various things, need to do a game restore for all the
//	robots, powerups, walls, doors, etc.
void StartNewLevelSecret(int level_num, int page_in_textures)
{
	ThisLevelTime = 0;

	last_drawn_cockpit = -1;

	if (Newdemo_state == ND_STATE_PAUSED)
		Newdemo_state = ND_STATE_RECORDING;

	if (Newdemo_state == ND_STATE_RECORDING) {
		newdemo_set_new_level(level_num);
		newdemo_record_start_frame(FrameTime);
	}
	else if (Newdemo_state != ND_STATE_PLAYBACK) {

		set_screen_mode(SCREEN_MENU);

		if (First_secret_visit) {
			do_screen_message(TXT_SECRET_EXIT);
		}
		else {
			if (PHYSFSX_exists(SECRETC_FILENAME, 0))
			{
				do_screen_message(TXT_SECRET_EXIT);
			}
			else {
				do_screen_message("Secret level already destroyed.\nAdvancing to level %i.", Current_level_num + 1);
			}
		}
	}

	LoadLevel(level_num, page_in_textures);

	Assert(Current_level_num == level_num); // make sure level set right

	gameseq_init_network_players(); // Initialize the Players array for this level

	HUD_clear_messages();

	automap_clear_visited();

	Viewer = &Objects[Players[Player_num].objnum];

	gameseq_remove_unused_players();

	Game_suspended = 0;

	Control_center_destroyed = 0;
	Ranking.freezeTimer = 0;

	init_cockpit();
	reset_palette_add();

	if (First_secret_visit || (Newdemo_state == ND_STATE_PLAYBACK)) {
		init_robots_for_level();
		init_ai_objects();
		init_smega_detonates();
		init_morphs();
		init_all_matcens();
		reset_special_effects();
		init_exploding_walls();
		StartSecretLevel();
	}
	else {
		if (PHYSFSX_exists(SECRETC_FILENAME, 0))
		{
			int	pw_save, sw_save;

			pw_save = Players[Player_num].primary_weapon;
			sw_save = Players[Player_num].secondary_weapon;
			state_restore_all(1, 1, SECRETC_FILENAME);
			Players[Player_num].primary_weapon = pw_save;
			Players[Player_num].secondary_weapon = sw_save;
			reset_special_effects();
			init_exploding_walls();
			StartSecretLevel();
			// -- No: This is only for returning to base level: set_pos_from_return_segment();
		}
		else {
			do_screen_message("Secret level already destroyed.\nAdvancing to level %i.", Current_level_num + 1);
			return;
		}
	}

	if (First_secret_visit) {
		copy_defaults_to_robot_all();
	}

	init_controlcen_for_level();

	// Say player can use FLASH cheat to mark path to exit.
	Last_level_path_created = -1;

	if (First_secret_visit) {
		Ranking.secretDeathCount = 0;
		Ranking.secretlevel_time = 0;
		Ranking.secretExcludePoints = 0;
		Ranking.secretRankScore = 0;
		Ranking.secretMissedRngSpawn = 0;
		Ranking.secretlast_score = Players[Player_num].score;
		Ranking.secret_hostages_on_board = 0;
		Ranking.secretQuickload = 0;
		if (Ranking.quickload)
			RestartLevel.updateRestartStuff = 1; // If we quickload into a normal level, then go into a secret level and back out to the next level, restarting will give default loadout without this.
		Ranking.hostages_secret_level = count_number_of_hostages();
		Ranking.fromBestRanksButton = 2; // We need this for starting secret levels too, since the normal start can be bypassed with a save.
		Ranking.secretNoDamage = 1;

		Ranking.secretAlreadyBeaten = 0;
		calculateParTime();
		if (calculateRank(Current_mission->last_level - level_num, 0) > 0)
			Ranking.secretAlreadyBeaten = 1;
		if (!Ranking.isRankable) { // If this level is not beatable, mark the level as beaten with zero points and an X-rank, so the mission can have an aggregate rank.
			PHYSFS_File* temp;
			char filename[256];
			char temp_filename[256];
			sprintf(filename, "ranks/%s/%s/levelS%i.hi", Players[Player_num].callsign, Current_mission->filename, Current_level_num * -1);
			sprintf(temp_filename, "ranks/%s/%s/temp.hi", Players[Player_num].callsign, Current_mission->filename);
			time_t timeOfScore = time(NULL);
			temp = PHYSFS_openWrite(temp_filename);
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "%i\n", Difficulty_level);
			PHYSFSX_printf(temp, "-1\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "%s\n", Current_level_name);
			PHYSFSX_printf(temp, "%s\n", ctime(&timeOfScore));
			PHYSFSX_printf(temp, "0");
			PHYSFS_close(temp);
			PHYSFS_delete(filename);
			PHYSFSX_rename(temp_filename, filename);
		}
	}
	First_secret_visit = 0;
}

int	Entered_from_level;

// ---------------------------------------------------------------------------------------------------------------
//	Called from switch.c when player is on a secret level and hits exit to return to base level.
void ExitSecretLevel(void)
{
	if (Newdemo_state == ND_STATE_PLAYBACK)
		return;

	if (Game_wind)
		window_set_visible(Game_wind, 0);

	if (!Control_center_destroyed)
		state_save_all(2, SECRETC_FILENAME, 0);
	else if (Ranking.secretQuickload == 0) // Don't show quickloaders a result screen for secret levels, as that's not in the vanilla game, and quickloaded levels have this mod disabled.
		DoEndSecretLevelScoreGlitz();

	if (PHYSFSX_exists(SECRETB_FILENAME,0))
	{
		int	pw_save, sw_save;

		do_screen_message(TXT_SECRET_RETURN);
		pw_save = Players[Player_num].primary_weapon;
		sw_save = Players[Player_num].secondary_weapon;
		state_restore_all(1, 1, SECRETB_FILENAME);
		Players[Player_num].primary_weapon = pw_save;
		Players[Player_num].secondary_weapon = sw_save;
	} else {
		// File doesn't exist, so can't return to base level.  Advance to next one.
		if (Entered_from_level == Last_level)
			DoEndGame();
		else {
			do_screen_message(TXT_SECRET_ADVANCE);
			StartNewLevel(Entered_from_level+1);
		}
	}

	if (Game_wind)
		window_set_visible(Game_wind, 1);
	reset_time();
	Ranking.rankScore = Players[Player_num].score - Players[Player_num].last_score - Ranking.excludePoints;
}

// ---------------------------------------------------------------------------------------------------------------
//	Set invulnerable_time and cloak_time in player struct to preserve amount of time left to
//	be invulnerable or cloaked.
void do_cloak_invul_secret_stuff(fix64 old_gametime)
{
	if (Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE) {
		fix64	time_used;

		time_used = old_gametime - Players[Player_num].invulnerable_time;
		Players[Player_num].invulnerable_time = GameTime64 - time_used;
	}

	if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
		fix	time_used;

		time_used = old_gametime - Players[Player_num].cloak_time;
		Players[Player_num].cloak_time = GameTime64 - time_used;
	}
}

// ---------------------------------------------------------------------------------------------------------------
//	Called from switch.c when player passes through secret exit.  That means he was on a non-secret level and he
//	is passing to the secret level.
//	Do a savegame.
void EnterSecretLevel(void)
{
	int i;

	Assert(! (Game_mode & GM_MULTI) );

	if (Game_wind)
		window_set_visible(Game_wind, 0);

	digi_play_sample( SOUND_SECRET_EXIT, F1_0 );	// after above call which stops all sounds
	
	Entered_from_level = Current_level_num;

	Ranking.level_time = (Players[Player_num].hours_level * 3600) + Players[Player_num].time_level;
	if (Control_center_destroyed) {
		Ranking.level_time = (Players[Player_num].hours_level * 3600) + ((double)Players[Player_num].time_level / 65536);
		DoEndLevelScoreGlitz(0);
	}

	if (Newdemo_state != ND_STATE_PLAYBACK)
		state_save_all(1, NULL, 0);	//	Not between levels (ie, save all), IS a secret level, NO filename override

	//	Find secret level number to go to, stuff in Next_level_num.
	for (i=0; i<-Last_secret_level; i++)
		if (Secret_level_table[i]==Current_level_num) {
			Next_level_num = -(i+1);
			break;
		} else if (Secret_level_table[i] > Current_level_num) {	//	Allows multiple exits in same group.
			Next_level_num = -i;
			break;
		}

	if (! (i<-Last_secret_level))		//didn't find level, so must be last
		Next_level_num = Last_secret_level;

	// NMN 04/09/07  Do a REAL start level routine if we are playing a D1 level so we have
	//               briefings
	if (EMULATING_D1)
	{
		set_screen_mode(SCREEN_MENU);
		do_screen_message("Alternate Exit Found!\n\nProceeding to Secret Level!");
		StartNewLevel(Next_level_num);
	} else {
		if (RestartLevel.isResults == 2) // We need restart stuff here to make it work on result screens going into a secret level, so the player doesn't get spat out there anyway.
			StartNewLevel(Current_level_num);
		else
		   	StartNewLevelSecret(Next_level_num, 1);
	}
	// END NMN

	// do_cloak_invul_stuff();
	if (Game_wind)
		window_set_visible(Game_wind, 1);
	Ranking.fromBestRanksButton = 0; // So loading a save doesn't cause the next result screen to let you select difficulty.
	if (Ranking.secretQuickload == 1) {
		HUD_init_message_literal(HM_DEFAULT, "You quickloaded! Ranking system mod disabled for this level.");
		digi_play_sample(SOUND_BAD_SELECTION, F1_0);
	}
	reset_time();
}

//called when the player has finished a level
void PlayerFinishedLevel(int secret_flag)
{
	Assert(!secret_flag);

	if (Game_wind)
		window_set_visible(Game_wind, 0);

	//credit the player for hostages
	Players[Player_num].hostages_rescued_total += Players[Player_num].hostages_on_board;

	if (Game_mode & GM_NETWORK)
	{
		Players[Player_num].connected = CONNECT_WAITING; // Finished but did not die
	}

	last_drawn_cockpit = -1;

	AdvanceLevel(secret_flag);				//now go on to the next one (if one)

	if (Game_wind)
		window_set_visible(Game_wind, 1);
	reset_time();
}

#if defined(D2_OEM) || defined(COMPILATION)
#define MOVIE_REQUIRED 0
#else
#define MOVIE_REQUIRED 1
#endif

#ifdef D2_OEM
#define ENDMOVIE "endo"
#else
#define ENDMOVIE "end"
#endif

void show_order_form();

//called when the player has finished the last level
void DoEndGame(void)
{
	if ((Newdemo_state == ND_STATE_RECORDING) || (Newdemo_state == ND_STATE_PAUSED))
		newdemo_stop_recording(0);

	set_screen_mode( SCREEN_MENU );

	gr_set_current_canvas(NULL);

	key_flush();

	if (PLAYING_BUILTIN_MISSION && !(Game_mode & GM_MULTI))
	{ //only built-in mission, & not multi
		int played=MOVIE_NOT_PLAYED;	//default is not played

		init_subtitles(ENDMOVIE ".tex");	//ingore errors
		played = PlayMovie(ENDMOVIE,MOVIE_REQUIRED);
		close_subtitles();
		if (!played)
		{
			do_end_briefing_screens(Ending_text_filename);
		}
   }
   else if (!(Game_mode & GM_MULTI))    //not multi
   {
		char tname[FILENAME_LEN];

		do_end_briefing_screens (Ending_text_filename);

		//try doing special credits
		sprintf(tname,"%s.ctb",Current_mission_filename);
		credits_show(tname);
	}

	key_flush();

#ifdef NETWORK
	if (Game_mode & GM_MULTI)
		multi_endlevel_score();
	else if (Current_level_num != Last_level)
#endif
		// NOTE LINK TO ABOVE
		DoEndLevelScoreGlitz(0);

	if (PLAYING_BUILTIN_MISSION && !((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP))) {
		gr_set_current_canvas( NULL );
		gr_clear_canvas(BM_XRGB(0,0,0));
		load_palette(D2_DEFAULT_PALETTE,0,1);
		scores_maybe_add_player(0);
	}

	if (Game_wind)
		window_close(Game_wind);		// Exit out of game loop
}

int checkForWarmStart()
{
	// First, check for too much shields, energy, concussion missiles, vulcan ammo or laser level. Also check for quads.
	if (f2fl(Players[Player_num].shields) > 100 || f2fl(Players[Player_num].energy) > 100 || Players[Player_num].secondary_ammo[CONCUSSION_INDEX] > 7 - Difficulty_level || Players[Player_num].primary_ammo[VULCAN_INDEX] || Players[Player_num].laser_level || Players[Player_num].flags & PLAYER_FLAGS_QUAD_LASERS)
		return 1;
	// In D2, we have to check for accessories.
	if (Players[Player_num].flags & PLAYER_FLAGS_HEADLIGHT || Players[Player_num].flags & PLAYER_FLAGS_AMMO_RACK || Players[Player_num].flags & PLAYER_FLAGS_MAP_ALL || Players[Player_num].flags & PLAYER_FLAGS_CONVERTER || Players[Player_num].flags & PLAYER_FLAGS_AFTERBURNER)
		return 1;
	// Next, check for the presence of any weapon besides lasers.
	for (int i = 1; i < 10; i++)
		if (Players[Player_num].primary_weapon_flags & HAS_FLAG(i))
			return 1;
	// Last, check for the presence of any missiles besides concussions.
	for (int i = 1; i < 10; i++)
		if (Players[Player_num].secondary_ammo[i])
			return 1;
	return 0; // Didn't find anything, not a warm start.
}

//called to go to the next level (if there is one)
//if secret_flag is true, advance to secret level, else next normal one
//	Return true if game over.
void AdvanceLevel(int secret_flag)
{
#ifdef NETWORK
	int result;
#endif

	Assert(!secret_flag);

	if (Current_level_num != Last_level) {
#ifdef NETWORK
		if (Game_mode & GM_MULTI)
			multi_endlevel_score();
#endif
	}
	if (!(Game_mode & GM_MULTI))
		// NOTE LINK TO ABOVE!!!
		DoEndLevelScoreGlitz(0);		//give bonuses

	Control_center_destroyed = 0;
	Ranking.freezeTimer = 0;

	#ifdef EDITOR
	if (Current_level_num == 0)
	{
		window_close(Game_wind);		//not a real level
	}
	#endif

#ifdef NETWORK
	if (Game_mode & GM_MULTI)	{
		result = multi_endlevel(&secret_flag); // Wait for other players to reach this point
		if (result) // failed to sync
		{
			if (Current_level_num == Last_level)		//player has finished the game!
				if (Game_wind)
					window_close(Game_wind);		// Exit out of game loop

			return;
		}
	}
#endif

	if (RestartLevel.isResults == 2)
		StartNewLevel(Current_level_num);
	else {
		RestartLevel.updateRestartStuff = 1; // Assume we can update this struct upon starting a level. The game will tell us not to if we're restarting the current level from the result screen.
		if (Current_level_num == Last_level) {		//player has finished the game!

			DoEndGame();

		}
		else {
			//NMN 04/08/07 If we are in a secret level and playing a D1
			// 	       level, then use Entered_from_level # instead
			if (Current_level_num < 0 && EMULATING_D1)
			{
				Next_level_num = Entered_from_level + 1;		//assume go to next normal level
			}
			else {
				Next_level_num = Current_level_num + 1;		//assume go to next normal level
			}
			// END NMN

			StartNewLevel(Next_level_num);
			Ranking.warmStart = checkForWarmStart(); // Don't consider the player to be warm starting unless they actually have more than the default loadout.
		}
	}
}

void DoPlayerDead()
{

	int cycle_window_vis = 1;
#ifdef NETWORK
	if ( (Game_mode & GM_MULTI) && (Netgame.SpawnStyle == SPAWN_STYLE_PREVIEW))  {
		cycle_window_vis = 0; 
	}
#endif

	reset_auto_select();

	if (Game_wind && cycle_window_vis)
		window_set_visible(Game_wind, 0);

	reset_palette_add();

	gr_palette_load (gr_palette);

	dead_player_end();		//terminate death sequence (if playing)

	#ifdef EDITOR
	if (Game_mode == GM_EDITOR) {			//test mine, not real level
		object * playerobj = &Objects[Players[Player_num].objnum];
		//nm_messagebox( "You're Dead!", 1, "Continue", "Not a real game, though." );
		if (Game_wind)
			window_set_visible(Game_wind, 1);
		load_level("gamesave.lvl");
		init_player_stats_new_ship(Player_num);
		playerobj->flags &= ~OF_SHOULD_BE_DEAD;
		StartLevel(0);
		return;
	}
	#endif

#ifdef NETWORK
	if ( Game_mode&GM_MULTI )
	{
		multi_do_death(Players[Player_num].objnum);
	}
	else
#endif
	{				//Note link to above else!
		Players[Player_num].lives--;
		if (Players[Player_num].lives == 0)
		{
			DoGameOver();
			return;
		}
	}

	if ( Control_center_destroyed ) {

		//clear out stuff so no bonus
		Players[Player_num].hostages_on_board = 0;
		Ranking.secret_hostages_on_board = 0;
		Players[Player_num].energy = 0;
		Players[Player_num].shields = 0;
		Players[Player_num].connected = CONNECT_DIED_IN_MINE;

		do_screen_message(TXT_DIED_IN_MINE); // Give them some indication of what happened

		if (Current_level_num < 0) {
			if (!Ranking.secretQuickload) // Don't show quickloaders a result screen for secret levels, as that's not in the vanilla game, and quickloaded levels have this mod disabled.
				DoEndSecretLevelScoreGlitz();
			if (PHYSFSX_exists(SECRETB_FILENAME,0))
			{
				do_screen_message(TXT_SECRET_RETURN);
				state_restore_all(1, 2, SECRETB_FILENAME);			//	2 means you died
				Ranking.rankScore = Players[Player_num].score - Players[Player_num].last_score - Ranking.excludePoints;
				set_pos_from_return_segment();
				Players[Player_num].lives--;						//	re-lose the life, Players[Player_num].lives got written over in restore.
			} else {
				if (Entered_from_level == Last_level)
					DoEndGame();
				else {
					do_screen_message(TXT_SECRET_ADVANCE);
					StartNewLevel(Entered_from_level+1);
					// Set restart stuff here so dying during self destruct correctly sets it for the next level. If this isn't done, the player can restart to get their "incinerated" stuff back.
					RestartLevel.primary_weapon = 0;
					RestartLevel.secondary_weapon = 0;
					RestartLevel.flags &= ~(PLAYER_FLAGS_QUAD_LASERS | PLAYER_FLAGS_CLOAKED | PLAYER_FLAGS_INVULNERABLE);
					RestartLevel.energy = INITIAL_ENERGY;
					RestartLevel.shields = StartingShields;
					RestartLevel.lives = Players[Player_num].lives;
					RestartLevel.laser_level = 0;
					RestartLevel.primary_weapon_flags = HAS_LASER_FLAG;
					RestartLevel.secondary_weapon_flags = HAS_CONCUSSION_FLAG;
					for (int i = 0; i < MAX_PRIMARY_WEAPONS; i++)
						RestartLevel.primary_ammo[i] = 0;
					for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++)
						RestartLevel.secondary_ammo[i] = 0;
					RestartLevel.secondary_ammo[0] = 2 + NDL - Difficulty_level;
					RestartLevel.afterburner_charge = Players[Player_num].afterburner_charge;
					RestartLevel.omega_charge = Omega_charge;
					Ranking.warmStart = 0; // Don't count next level as a warm start, since the player just lost all their stuff in self destruct.
					init_player_stats_new_ship(Player_num);	//	New, MK, 05/29/96!, fix bug with dying in secret level, advance to next level, keep powerups!
				}
			}
		} else {

			AdvanceLevel(0);			//if finished, go on to next level

			if (RestartLevel.updateRestartStuff) {
				// Set restart stuff here so dying during self destruct correctly sets it for the next level. If this isn't done, the player can restart to get their "incinerated" stuff back.
				RestartLevel.primary_weapon = 0;
				RestartLevel.secondary_weapon = 0;
				RestartLevel.flags &= ~(PLAYER_FLAGS_QUAD_LASERS | PLAYER_FLAGS_CLOAKED | PLAYER_FLAGS_INVULNERABLE);
				RestartLevel.energy = INITIAL_ENERGY;
				RestartLevel.shields = StartingShields;
				RestartLevel.lives = Players[Player_num].lives;
				RestartLevel.laser_level = 0;
				RestartLevel.primary_weapon_flags = HAS_LASER_FLAG;
				RestartLevel.secondary_weapon_flags = HAS_CONCUSSION_FLAG;
				for (int i = 0; i < MAX_PRIMARY_WEAPONS; i++)
					RestartLevel.primary_ammo[i] = 0;
				for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++)
					RestartLevel.secondary_ammo[i] = 0;
				RestartLevel.secondary_ammo[0] = 2 + NDL - Difficulty_level;
				RestartLevel.afterburner_charge = Players[Player_num].afterburner_charge;
				RestartLevel.omega_charge = Omega_charge;
				Ranking.warmStart = 0; // Don't count next level as a warm start, since the player just lost all their stuff in self destruct.
				init_player_stats_new_ship(Player_num);
			}
			last_drawn_cockpit = -1;
		}

	} else if (Current_level_num < 0) {
		if (PHYSFSX_exists(SECRETB_FILENAME,0))
		{
			do_screen_message(TXT_SECRET_RETURN);
			if (!Control_center_destroyed)
				state_save_all(2, SECRETC_FILENAME, 0);
			state_restore_all(1, 2, SECRETB_FILENAME);
			Ranking.rankScore = Players[Player_num].score - Players[Player_num].last_score - Ranking.excludePoints;
			set_pos_from_return_segment();
			Players[Player_num].lives--;						//	re-lose the life, Players[Player_num].lives got written over in restore.
		} else {
			do_screen_message(TXT_DIED_IN_MINE); // Give them some indication of what happened
			if (Entered_from_level == Last_level)
				DoEndGame();
			else {
				do_screen_message(TXT_SECRET_ADVANCE);
				StartNewLevel(Entered_from_level+1);
				init_player_stats_new_ship(Player_num);	//	New, MK, 05/29/96!, fix bug with dying in secret level, advance to next level, keep powerups!
			}
		}
	} else {
		init_player_stats_new_ship(Player_num);
		StartLevel(1);
	}

	digi_sync_sounds();

	if (Game_wind && cycle_window_vis)
		window_set_visible(Game_wind, 1);
	reset_time();
}

extern int BigWindowSwitch;

//called when the player is starting a new level for normal game mode and restore state
//	secret_flag set if came from a secret level
void StartNewLevelSub(int level_num, int page_in_textures, int secret_flag)
	{
	if (!(Game_mode & GM_MULTI)) {
		last_drawn_cockpit = -1;
	}
   BigWindowSwitch=0;

	if (GameArg.GameLogSplit) {
		// Include the mission name, level number and timestamp in the log filename
		char log_filename[PATH_MAX];
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		snprintf(log_filename, SDL_arraysize(log_filename), "gamelog-%s-%d-%04d%02d%02d-%02d%02d%02d.txt",
			Current_mission_filename, level_num, t->tm_year + 1900, t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

		con_switch_log(log_filename);
	}

	if (Newdemo_state == ND_STATE_PAUSED)
		Newdemo_state = ND_STATE_RECORDING;

	if (Newdemo_state == ND_STATE_RECORDING) {
		newdemo_set_new_level(level_num);
		newdemo_record_start_frame(FrameTime );
	}

	LoadLevel(level_num,page_in_textures);

	Assert(Current_level_num == level_num);	//make sure level set right

	gameseq_init_network_players(); // Initialize the Players array for
											  // this level

	Viewer = &Objects[Players[Player_num].objnum];

	Assert(N_players <= NumNetPlayerPositions);
		//If this assert fails, there's not enough start positions

#ifdef NETWORK
	if (Game_mode & GM_NETWORK)
	{
		if(multi_level_sync()) // After calling this, Player_num is set
		{
			songs_play_song( SONG_TITLE, 1 ); // level song already plays but we fail to start level...
			return;
		}

		// Reset the timestamp when we can send another ship status packet (for observer mode)
		Next_ship_status_time = GameTime64;

		if (imulti_new_game)
		{
			for (int i = 0; i < MAX_PLAYERS; i++)
			{
				init_player_stats_new_ship(i);
			}
		}
	}
#endif

	HUD_clear_messages();

	automap_clear_visited();

	init_player_stats_level(secret_flag);

	load_palette(Current_level_palette,0,1);
	gr_palette_load(gr_palette);

#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Network_rejoined)
	{
		int i;
		for (i = 0; i < N_players; i++)
			Players[i].flags |= Netgame.player_flags[i];
	}

	if (Game_mode & GM_MULTI)
	{
		multi_prep_level(); // Removes robots from level if necessary
	}
#endif

	gameseq_remove_unused_players();

	Game_suspended = 0;

	Control_center_destroyed = 0;
	Ranking.freezeTimer = 0;

	set_screen_mode(SCREEN_GAME);

	init_cockpit();
	init_robots_for_level();
	init_ai_objects();
	init_smega_detonates();
	init_morphs();
	init_all_matcens();
	reset_palette_add();
	init_thief_for_level();
	init_stuck_objects();
	if (!(Game_mode & GM_MULTI))
		filter_objects_from_level();

	if (!(Game_mode & GM_MULTI) && !cheats.enabled)
		set_highest_level(Current_level_num);
	else
		read_player_file();		//get window sizes

	reset_special_effects();
	init_exploding_walls();

#ifdef OGL
	gr_remap_mono_fonts();
#endif


#ifdef NETWORK
	if (Network_rejoined == 1)
	{
		Network_rejoined = 0;
		StartLevel(1);
	}
	else
#endif
		StartLevel(0);		// Note link to above if!

	copy_defaults_to_robot_all();
	init_controlcen_for_level();

	reset_respawnable_bots();

	set_homing_update_rate(Game_mode & GM_MULTI ? Netgame.HomingUpdateRate : 25);

	//	Say player can use FLASH cheat to mark path to exit.
	Last_level_path_created = -1;

	// Initialise for palette_restore()
	// Also takes care of nm_draw_background() possibly being called
	if (!((Game_mode & GM_MULTI) && (Newdemo_state != ND_STATE_PLAYBACK)))
		full_palette_save();

	int autodemo_enabled_for_game_mode =
		(!(Game_mode & GM_MULTI) && PlayerCfg.AutoDemoSp) ||
		((Game_mode & GM_MULTI) && PlayerCfg.AutoDemoMp);
	if (autodemo_enabled_for_game_mode && Newdemo_state != ND_STATE_RECORDING) {
		newdemo_start_recording(1);
	}

	if (!Game_wind)
		game();
}

#ifdef NETWORK
extern char PowerupsInMine[MAX_POWERUP_TYPES], MaxPowerupsAllowed[MAX_POWERUP_TYPES];
#endif
void bash_to_shield (int i,char *s)
{
#ifdef NETWORK
	int type=Objects[i].id;
#endif

#ifdef NETWORK
	PowerupsInMine[type]=MaxPowerupsAllowed[type]=0;
#endif

	Objects[i].id = POW_SHIELD_BOOST;
	Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
	Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;
	Objects[i].size = Powerup_info[POW_SHIELD_BOOST].size;
}


void filter_objects_from_level()
 {
  int i;

  for (i=0;i<=Highest_object_index;i++)
	{
	 if (Objects[i].type==OBJ_POWERUP)
     if (Objects[i].id==POW_FLAG_RED || Objects[i].id==POW_FLAG_BLUE)
	   bash_to_shield (i,"Flag!!!!");
   }

 }

struct {
	int	level_num;
	char	movie_name[FILENAME_LEN];
} intro_movie[] = { 	{ 1,"pla"},
							{ 5,"plb"},
							{ 9,"plc"},
							{13,"pld"},
							{17,"ple"},
							{21,"plf"},
							{24,"plg"}};

#define NUM_INTRO_MOVIES (sizeof(intro_movie) / sizeof(*intro_movie))

extern int robot_movies;	//0 means none, 1 means lowres, 2 means hires
extern int intro_played;	//true if big intro movie played

void ShowLevelIntro(int level_num)
{
	//if shareware, show a briefing?

	if (!(Game_mode & GM_MULTI)) {
		int i;

		ubyte save_pal[sizeof(gr_palette)];

		memcpy(save_pal,gr_palette,sizeof(gr_palette));

		if (PLAYING_BUILTIN_MISSION) {

			if (is_SHAREWARE || is_MAC_SHARE)
			{
				if (level_num==1)
					do_briefing_screens (Briefing_text_filename, 1);
			}
			else if (is_D2_OEM)
			{
				if (level_num == 1 && !intro_played)
					do_briefing_screens(Briefing_text_filename, 1);
			}
			else // full version
			{
				for (i=0;i<NUM_INTRO_MOVIES;i++)
				{
					if (intro_movie[i].level_num == level_num)
					{
						Screen_mode = -1;
						PlayMovie(intro_movie[i].movie_name,MOVIE_REQUIRED);
						break;
					}
				}

				do_briefing_screens (Briefing_text_filename,level_num);
			}
		}
		else	//not the built-in mission (maybe d1, too).  check for add-on briefing
		{
			do_briefing_screens(Briefing_text_filename, level_num);
		}

		memcpy(gr_palette,save_pal,sizeof(gr_palette));
	}
}

//	---------------------------------------------------------------------------
//	If starting a level which appears in the Secret_level_table, then set First_secret_visit.
//	Reason: On this level, if player goes to a secret level, he will be going to a different
//	secret level than he's ever been to before.
//	Sets the global First_secret_visit if necessary.  Otherwise leaves it unchanged.
void maybe_set_first_secret_visit(int level_num)
{
	int	i;

	for (i=0; i<N_secret_levels; i++) {
		if (Secret_level_table[i] == level_num) {
			First_secret_visit = 1;
		}
	}
}

//called when the player is starting a new level for normal game model
//	secret_flag if came from a secret level
void StartNewLevel(int level_num)
{
	hide_menus();

	GameTime64 = 0;
	ThisLevelTime = 0;

	Ranking.deathCount = 0;
	Ranking.excludePoints = 0;
	Ranking.rankScore = 0;
	Ranking.missedRngSpawn = 0;
	Ranking.quickload = 0;
	Ranking.level_time = 0; // Set this to 0 despite it going unused until set to time_level, so we can save a variable when telling the in-game timer which time variable to display.
	Ranking.fromBestRanksButton = 2; // So the result screen knows it's not just viewing record details.
	Ranking.noDamage = 1;
	if (RestartLevel.updateRestartStuff) {
		RestartLevel.primary_weapon = Players[Player_num].primary_weapon;
		RestartLevel.secondary_weapon = Players[Player_num].secondary_weapon;
		RestartLevel.flags = Players[Player_num].flags;
		RestartLevel.energy = Players[Player_num].energy;
		RestartLevel.shields = Players[Player_num].shields;
		RestartLevel.lives = Players[Player_num].lives;
		RestartLevel.laser_level = Players[Player_num].laser_level;
		RestartLevel.primary_weapon_flags = Players[Player_num].primary_weapon_flags;
		RestartLevel.secondary_weapon_flags = Players[Player_num].secondary_weapon_flags;
		for (int i = 0; i < MAX_PRIMARY_WEAPONS; i++)
			RestartLevel.primary_ammo[i] = Players[Player_num].primary_ammo[i];
		for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++)
			RestartLevel.secondary_ammo[i] = Players[Player_num].secondary_ammo[i];
		RestartLevel.afterburner_charge = Players[Player_num].afterburner_charge;
		RestartLevel.omega_charge = Omega_charge;
	}

	if (level_num > 0) {
		maybe_set_first_secret_visit(level_num);
	}

	if (!RestartLevel.restarts && RestartLevel.isResults < 2) // Skip briefing for restarts.
		ShowLevelIntro(level_num);
	else
		First_secret_visit = 1; // Reset a secret level upon restarting its base level, for continuity's sake.

	StartNewLevelSub(level_num, 1, 0);

	RestartLevel.isResults = 0;

	if (RestartLevel.restarts) {
		const char message[256];
		if (RestartLevel.restarts == 1)
			sprintf(message, "1 restart and counting...");
		else
			sprintf(message, "%i restarts and counting...", RestartLevel.restarts);
		powerup_basic(-10, -10, -10, 0, message);
	}
	else {
		calculateParTime();
		if (!Ranking.isRankable) { // If this level is not beatable, mark the level as beaten with zero points and an X-rank, so the mission can have an aggregate rank.
			PHYSFS_File* temp;
			char filename[256];
			char temp_filename[256];
			sprintf(filename, "ranks/%s/%s/level%i.hi", Players[Player_num].callsign, Current_mission->filename, Current_level_num);
			sprintf(temp_filename, "ranks/%s/%s/temp.hi", Players[Player_num].callsign, Current_mission->filename);
			time_t timeOfScore = time(NULL);
			temp = PHYSFS_openWrite(temp_filename);
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "%i\n", Difficulty_level);
			PHYSFSX_printf(temp, "-1\n");
			PHYSFSX_printf(temp, "0\n");
			PHYSFSX_printf(temp, "%s\n", Current_level_name);
			PHYSFSX_printf(temp, "%s\n", ctime(&timeOfScore));
			PHYSFSX_printf(temp, "0");
			PHYSFS_close(temp);
			PHYSFS_delete(filename);
			PHYSFSX_rename(temp_filename, filename);
		}
	}
	Ranking.alreadyBeaten = 0;
	if (calculateRank(level_num, 0) > 0)
		Ranking.alreadyBeaten = 1;
}

int previewed_spawn_point = 0;

//initialize the player object position & orientation (at start of game, or new ship)
void InitPlayerPosition(int random_flag)
{
	int NewPlayer=0;

	if (is_observer())
		NewPlayer = 0;
	else if (! ((Game_mode & GM_MULTI) && !(Game_mode&GM_MULTI_COOP)) ) // If not deathmatch
		NewPlayer = Player_num;
#ifdef NETWORK	
	else if ((Game_mode & GM_MULTI) && (Netgame.SpawnStyle == SPAWN_STYLE_PREVIEW) && Dead_player_camera != NULL)
		NewPlayer = previewed_spawn_point; 
#endif	
	else if (random_flag == 1)
	{
		int i, trys=0;
		fix closest_dist = 0x7ffffff, dist;

		timer_update();
		d_srand((fix)timer_query());
		do {
			trys++;
			NewPlayer = d_rand() % NumNetPlayerPositions;

			closest_dist = 0x7fffffff;

			for (i=0; i<N_players; i++ )	{
				if ( (i!=Player_num) && (Objects[Players[i].objnum].type == OBJ_PLAYER) )	{
					dist = find_connected_distance(&Objects[Players[i].objnum].pos, Objects[Players[i].objnum].segnum, &Player_init[NewPlayer].pos, Player_init[NewPlayer].segnum, 15, WID_FLY_FLAG ); // Used to be 5, search up to 15 segments
					if ( (dist < closest_dist) && (dist >= 0) )	{
						closest_dist = dist;
					}
				}
			}

		} while ( (closest_dist<i2f(15*20)) && (trys<MAX_PLAYERS*2) );
	}
	else {
		// If deathmatch and not random, positions were already determined by sync packet
		reset_player_object();
		reset_cruise();
		return;
	}
	Assert(NewPlayer >= 0);
	Assert(NewPlayer < NumNetPlayerPositions);
	ConsoleObject->pos = Player_init[NewPlayer].pos;
	ConsoleObject->orient = Player_init[NewPlayer].orient;
#ifdef NETWORK	
	if ((Game_mode & GM_MULTI) && (Netgame.SpawnStyle == SPAWN_STYLE_PREVIEW) && Dead_player_camera != NULL) {
		ConsoleObject->orient = Dead_player_camera->orient;  
		Dead_player_camera = NULL; 
	}

	if (is_observer()) {
		ConsoleObject->pos = Objects[Players[Current_obs_player].objnum].pos;
		ConsoleObject->orient = Objects[Players[Current_obs_player].objnum].orient;
		obj_relink_all();
	} else
#endif
		obj_relink(ConsoleObject-Objects,Player_init[NewPlayer].segnum);
	reset_player_object();
	reset_cruise();
}

//	-----------------------------------------------------------------------------------------------------
//	Initialize default parameters for one robot, copying from Robot_info to *objp.
//	What about setting size!?  Where does that come from?
void copy_defaults_to_robot(object *objp)
{
	robot_info	*robptr;
	int			objid;

	Assert(objp->type == OBJ_ROBOT);
	objid = objp->id;
	Assert(objid < N_robot_types);

	robptr = &Robot_info[objid];

	//	Boost shield for Thief and Buddy based on level.
	objp->shields = robptr->strength;

	if ((robptr->thief) || (robptr->companion)) {
		objp->shields = (objp->shields * (abs(Current_level_num)+7))/8;

		if (robptr->companion) {
			//	Now, scale guide-bot hits by skill level
			switch (Difficulty_level) {
				case 0:	objp->shields = i2f(20000);	break;		//	Trainee, basically unkillable
				case 1:	objp->shields *= 3;				break;		//	Rookie, pretty dang hard
				case 2:	objp->shields *= 2;				break;		//	Hotshot, a bit tough
				default:	break;
			}
		}
	} else if (robptr->boss_flag)	//	MK, 01/16/95, make boss shields lower on lower diff levels.
		objp->shields = objp->shields/(NDL+3) * (Difficulty_level+4);

	//	Additional wimpification of bosses at Trainee
	if ((robptr->boss_flag) && (Difficulty_level == 0))
		objp->shields /= 2;
}

//	-----------------------------------------------------------------------------------------------------
//	Copy all values from the robot info structure to all instances of robots.
//	This allows us to change bitmaps.tbl and have these changes manifested in existing robots.
//	This function should be called at level load time.
void copy_defaults_to_robot_all()
{
	int	i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT)
			copy_defaults_to_robot(&Objects[i]);

}

extern void clear_stuck_objects(void);

//	-----------------------------------------------------------------------------------------------------
//called when the player is starting a level (new game or new ship)
void StartLevel(int random_flag)
{
	Assert(!Player_is_dead);

	InitPlayerPosition(random_flag);

	verify_console_object();

	ConsoleObject->control_type	= CT_FLYING;
	ConsoleObject->movement_type	= MT_PHYSICS;

	// create_player_appearance_effect(ConsoleObject);
	Do_appearance_effect = 1;

	if (Game_mode & GM_MULTI)
	{
		if ((Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS))
			multi_send_score();
	 	multi_send_reappear();
		multi_do_protocol_frame(1, 1);
	}
	else // in Singleplayer, after we died ...
	{
		disable_matcens(); // ... disable matcens and ...
		clear_transient_objects(0); // ... clear all transient objects.
		clear_stuck_objects(); // and stuck ones.
	}

	ai_reset_all_paths();
	ai_init_boss_for_ship();

	reset_rear_view();
	Auto_fire_fusion_cannon_time = 0;
	Fusion_charge = 0;
}
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
 * Multiplayer code for network play.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "u_mem.h"
#include "strutil.h"
#include "game.h"
#include "multi.h"
#include "object.h"
#include "laser.h"
#include "fuelcen.h"
#include "scores.h"
#include "gauges.h"
#include "collide.h"
#include "dxxerror.h"
#include "fireball.h"
#include "newmenu.h"
#include "console.h"
#include "wall.h"
#include "cntrlcen.h"
#include "polyobj.h"
#include "bm.h"
#include "endlevel.h"
#include "key.h"
#include "playsave.h"
#include "timer.h"
#include "digi.h"
#include "sounds.h"
#include "kconfig.h"
#include "newdemo.h"
#include "text.h"
#include "kmatrix.h"
#include "multibot.h"
#include "gameseq.h"
#include "physics.h"
#include "config.h"
#include "ai.h"
#include "switch.h"
#include "textures.h"
#include "byteswap.h"
#include "sounds.h"
#include "args.h"
#include "effects.h"
#include "iff.h"
#include "state.h"
#include "automap.h"
#ifdef USE_UDP
#include "net_udp.h"
#endif

void multi_reset_player_object(object *objp);
void multi_reset_object_texture(object *objp);
void multi_add_lifetime_killed();
void multi_add_lifetime_kills();
void multi_send_play_by_play(int num,int spnum,int dpnum);
void multi_send_heartbeat();
void multi_powcap_cap_objects();
void multi_powcap_adjust_remote_cap(int pnum);
void multi_set_robot_ai(void);
void multi_send_powcap_update();
void bash_to_shield(int i,char *s);
void init_hoard_data();
void multi_apply_goal_textures();
int  find_goal_texture(ubyte t);
void multi_do_capture_bonus(const ubyte *buf);
void multi_do_orb_bonus(const ubyte *buf);
void multi_send_drop_flag(int objnum,int seed);
void multi_send_ranking();
void multi_do_play_by_play(const ubyte *buf);
void multi_new_bounty_target( int pnum );
void multi_do_bounty( const ubyte *buf );
void multi_save_game(ubyte slot, uint id, char *desc);
void multi_restore_game(ubyte slot, uint id);
void multi_do_save_game(const ubyte *buf);
void multi_do_restore_game(const ubyte *buf);
void multi_do_msgsend_state(const ubyte *buf);
void multi_send_msgsend_state(int state);
void multi_send_gmode_update();
void multi_do_gmode_update(const ubyte *buf);

//
// Local macros and prototypes
//

// LOCALIZE ME!!

#define vm_angvec_zero(v) (v)->p=(v)->b=(v)->h=0

void drop_player_eggs(object *player); // from collide.c
void drop_player_eggs_remote(object *playerobj, ubyte remote); // from collide.c


//
// Global variables
//

int multi_protocol=0; // set and determinate used protocol
int imulti_new_game=0; // to prep stuff for level only when starting new game

int who_killed_controlcen = -1;  // -1 = noone

//do we draw the kill list on the HUD?
int Show_kill_list = 1;
int Show_network_stats = 1; 
int Show_reticle_name = 1;
fix Show_kill_list_timer = 0;

char Multi_is_guided=0;
sbyte PKilledFlags[MAX_PLAYERS];
int Bounty_target = 0;


int multi_sending_message[MAX_PLAYERS] = { 0,0,0,0,0,0,0,0 };
int multi_defining_message = 0;
int multi_message_index = 0;

ubyte multibuf[MAX_MULTI_MESSAGE_LEN+4];            // This is where multiplayer message are built

short remote_to_local[MAX_PLAYERS][MAX_OBJECTS];  // Remote object number for each local object
short local_to_remote[MAX_OBJECTS];
sbyte object_owner[MAX_OBJECTS];   // Who created each object in my universe, -1 = loaded at start

int   Net_create_objnums[MAX_NET_CREATE_OBJECTS]; // For tracking object creation that will be sent to remote
int   Net_create_loc = 0;       // pointer into previous array
int   Network_status = 0;
char  Network_message[MAX_MESSAGE_LEN];
int   Network_message_reciever=-1;
int   sorted_kills[MAX_PLAYERS];
short kill_matrix[MAX_PLAYERS][MAX_PLAYERS];
int   multi_goto_secret = 0;
short team_kills[2];
int   multi_quit_game = 0;
const char GMNames[MULTI_GAME_TYPE_COUNT][MULTI_GAME_NAME_LENGTH]={
	"Anarchy",
	"Team Anarchy",
	"Robo Anarchy",
	"Cooperative",
	"Capture the Flag",
	"Hoard",
	"Team Hoard",
	"Bounty"
};
const char GMNamesShrt[MULTI_GAME_TYPE_COUNT][8]={
	"ANRCHY",
	"TEAM",
	"ROBO",
	"COOP",
	"FLAG",
	"HOARD",
	"TMHOARD",
	"BOUNTY"
};

int Current_obs_player = OBSERVER_PLAYER_ID; // Current player being observed. Defaults to the observer player ID.
bool Obs_at_distance = 0; // True if you're viewing the player from a cube back.
int Host_is_obs;

// For rejoin object syncing (used here and all protocols - globally)

int	Network_send_objects = 0;  // Are we in the process of sending objects to a player?
int	Network_send_object_mode = 0; // What type of objects are we sending, static or dynamic?
int 	Network_send_objnum = -1;   // What object are we sending next?
int     Network_rejoined = 0;       // Did WE rejoin this game?
int     Network_sending_extras=0;
int     VerifyPlayerJoined=-1;      // Player (num) to enter game before any ingame/extra stuff is being sent
int     Player_joining_extras=-1;  // This is so we know who to send 'latecomer' packets to.
int     Network_player_added = 0;   // Is this a new player or a returning player?

ubyte Send_ship_status = 0; // Whether we owe observers a ship status packet.
fix64 Next_ship_status_time = 0; // The next time we are allowed to send a ship status.

ushort          my_segments_checksum = 0;

netgame_info Netgame;

bitmap_index multi_player_textures[MAX_PLAYERS][N_PLAYER_SHIP_TEXTURES];
ubyte multi_player_tex_color[MAX_PLAYERS];

// Globals for protocol-bound Refuse-functions
char RefuseThisPlayer=0,WaitForRefuseAnswer=0,RefuseTeam,RefusePlayerName[12];
fix64 RefuseTimeLimit=0;
extern void init_player_stats_new_ship(ubyte pnum);

static const int message_length[] = {
#define define_message_length(NAME,SIZE)	(SIZE),
	for_each_multiplayer_command(, define_message_length, )
};

char PowerupsInMine[MAX_POWERUP_TYPES],MaxPowerupsAllowed[MAX_POWERUP_TYPES];
extern fix ThisLevelTime;

const char *const RankStrings[10]={"(unpatched) ","Cadet ","Ensign ","Lieutenant ","Lt.Commander ",
                     "Commander ","Captain ","Vice Admiral ","Admiral ","Demigod "};

char *multi_allow_powerup_text[MULTI_ALLOW_POWERUP_MAX] =
{
#define define_netflag_string(NAME,STR)	STR,
	for_each_netflag_value(define_netflag_string)
};

// The Observatory stat tracking
kill_event* First_event[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
kill_event* Last_event[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
kill_event* Last_kill[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
kill_event* Last_death[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
int Kill_streak[MAX_PLAYERS] = {0,0,0,0,0,0,0,0};
int Next_graph = 5;
fix64 Show_graph_until = -1;

player_status* First_status = NULL;

shield_status* First_current_shield_status[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
shield_status* Last_current_shield_status[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
shield_status* First_previous_shield_status[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
shield_status* Last_previous_shield_status[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

fix64 Show_death_until[MAX_PLAYERS] = { 0,0,0,0,0,0,0,0 };

damage_taken_totals* First_damage_taken_totals[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
damage_taken_totals* First_damage_taken_current_totals[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
damage_taken_totals* First_damage_taken_previous_totals[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

damage_done_totals* First_damage_done_totals[MAX_PLAYERS] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

kill_log_event* Kill_log = NULL;

void add_observatory_stat(int player_num, ubyte event_type) {
	kill_event* ev = (kill_event*)d_malloc(sizeof(kill_event));
	ev->timestamp = GameTime64;
	ev->obs_event = event_type;
	ev->score = ((Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS)) ? Players[player_num].score : Players[player_num].net_kills_total;
	ev->next = NULL;
	ev->prev = Last_event[player_num];
	if (ev->prev != NULL) {
		ev->prev->next = ev;
	}
	Last_event[player_num] = ev;
	if (First_event[player_num] == NULL) {
		First_event[player_num] = ev;
	}
	if ((event_type & OBSEV_KILL) != 0) {
		Last_kill[player_num] = ev;
		Kill_streak[player_num] += 1;
	}
	if ((event_type & OBSEV_DEATH) != 0) {
		Last_death[player_num] = ev;
		Kill_streak[player_num] = 0;
	}
}

void add_observatory_damage_stat(int player_num, fix shields_delta, fix new_shields, fix old_shields, ubyte killer_type, ubyte killer_id, ubyte damage_type, ubyte source_id) {
	bool death = 0;

	// Set shields delta to old_shields if there was a kill.
	if (new_shields <= 0 && shields_delta > old_shields) {
		shields_delta = old_shields;
		death = 1;
	}

	// Set source_id for ship explosions and collisions.
	if (killer_type == OBJ_PLAYER && damage_type == DAMAGE_BLAST && source_id == 0) {
		source_id = SHIP_EXPLOSION_DAMAGE;
	}

	if (killer_type == OBJ_PLAYER && damage_type == DAMAGE_COLLISION && source_id == 0) {
		source_id = SHIP_COLLISION_DAMAGE;
	}

	// Combine different laser levels into the same source_id. They aren't named differently in
	// weapon_id_to_name, so we don't want them split up in summaries.
	// There is a gap between normal laser levels and super laser levels, so we have to check
	// each range separately.
	if (killer_type == OBJ_PLAYER && damage_type == DAMAGE_WEAPON && 
		((source_id > LASER_ID_L1 && source_id <= LASER_ID_L4) || (source_id >= LASER_ID_L5 && source_id <= LASER_ID_L6))) {
		source_id = LASER_ID_L1;
	}

	// Record player's damage over time.
	shield_status* sta = (shield_status*)d_malloc(sizeof(shield_status));
	sta->timestamp = GameTime64;
	sta->shields = new_shields;
	sta->next = NULL;

	if (First_current_shield_status[player_num] == NULL) {
		First_current_shield_status[player_num] = sta;
	}

	if (Last_current_shield_status[player_num] != NULL) {
		Last_current_shield_status[player_num]->next = sta;
	}
	Last_current_shield_status[player_num] = sta;

	// Do not process further for shield pickups.
	if (damage_type == DAMAGE_SHIELD) {
		return;
	}

	// Record player's overall damage taken total.
	damage_taken_totals* dtt = First_damage_taken_totals[player_num];
	while (dtt != NULL && (dtt->killer_type != killer_type || dtt->killer_id != killer_id || dtt->damage_type != damage_type || dtt->source_id != source_id)) {
		dtt = dtt->next;
	}

	if (dtt == NULL) {
		dtt = (damage_taken_totals*)d_malloc(sizeof(damage_taken_totals));
		dtt->total_damage = shields_delta;
		dtt->killer_type = killer_type;
		dtt->killer_id = killer_id;
		dtt->damage_type = damage_type;
		dtt->source_id = source_id;
		dtt->next = First_damage_taken_totals[player_num];
		dtt->prev = NULL;
		if (First_damage_taken_totals[player_num] != NULL) {
			First_damage_taken_totals[player_num]->prev = dtt;
		}
		First_damage_taken_totals[player_num] = dtt;
	}
	else {
		dtt->total_damage += shields_delta;
	}

	// Record player's damage taken total for this point.
	dtt = First_damage_taken_current_totals[player_num];
	while (dtt != NULL && (dtt->killer_type != killer_type || dtt->killer_id != killer_id || dtt->damage_type != damage_type || dtt->source_id != source_id)) {
		dtt = dtt->next;
	}

	if (dtt == NULL) {
		dtt = (damage_taken_totals*)d_malloc(sizeof(damage_taken_totals));
		dtt->total_damage = shields_delta;
		dtt->killer_type = killer_type;
		dtt->killer_id = killer_id;
		dtt->damage_type = damage_type;
		dtt->source_id = source_id;
		dtt->next = First_damage_taken_current_totals[player_num];
		dtt->prev = NULL;
		if (First_damage_taken_current_totals[player_num] != NULL) {
			First_damage_taken_current_totals[player_num]->prev = dtt;
		}
		First_damage_taken_current_totals[player_num] = dtt;
	}
	else {
		dtt->total_damage += shields_delta;
	}

	// Record player's damage dealt total.
	damage_done_totals* ddt = NULL;
	if (killer_type == OBJ_PLAYER) {
		ddt = First_damage_done_totals[killer_id];
		while (ddt != NULL && ddt->source_id != source_id) {
			ddt = ddt->next;
		}

		if (ddt == NULL) {
			ddt = (damage_done_totals*)d_malloc(sizeof(damage_done_totals));
			ddt->total_damage = shields_delta;
			ddt->source_id = source_id;
			ddt->next = First_damage_done_totals[killer_id];
			ddt->prev = NULL;
			if (First_damage_done_totals[killer_id] != NULL) {
				First_damage_done_totals[killer_id]->prev = ddt;
			}
			First_damage_done_totals[killer_id] = ddt;
		}
		else {
			ddt->total_damage += shields_delta;
		}

		// Sort damage done totals by total damage.
		ddt = First_damage_done_totals[killer_id];
		damage_done_totals* next_ddt = NULL;
		damage_done_totals* ddt_a = NULL;
		damage_done_totals* ddt_b = NULL;
		while (ddt != NULL) {
			next_ddt = ddt->next;

			if (ddt->prev != NULL) {
				while (ddt != NULL && ddt->prev != NULL && ddt->total_damage > ddt->prev->total_damage) {
					ddt_a = ddt->prev;
					ddt_b = ddt;

					ddt_a->next = ddt_b->next;
					ddt_b->prev = ddt_a->prev;

					if (ddt_a->next != NULL) {
						ddt_a->next->prev = ddt_a;
					}

					if (ddt_b->prev != NULL) {
						ddt_b->prev->next = ddt_b;
					}

					ddt_b->next = ddt_a;
					ddt_a->prev = ddt_b;

					if (ddt_a == First_damage_done_totals[killer_id]) {
						First_damage_done_totals[killer_id] = ddt_b;
					}
				}
			}

			ddt = next_ddt;
		}
	}

	// Record death.
	if (death == 1) {
		Last_previous_shield_status[player_num] = NULL;
		while ((sta = First_previous_shield_status[player_num]) != NULL) {
			First_previous_shield_status[player_num] = sta->next;
			d_free(sta);
		}

		First_previous_shield_status[player_num] = First_current_shield_status[player_num];
		Last_previous_shield_status[player_num] = Last_current_shield_status[player_num];

		First_current_shield_status[player_num] = NULL;
		Last_current_shield_status[player_num] = NULL;

		while ((dtt = First_damage_taken_previous_totals[player_num]) != NULL) {
			First_damage_taken_previous_totals[player_num] = dtt->next;
			d_free(dtt);
		}

		First_damage_taken_previous_totals[player_num] = First_damage_taken_current_totals[player_num];

		First_damage_taken_current_totals[player_num] = NULL;

		// Sort damage taken previous totals by total damage.
		dtt = First_damage_taken_previous_totals[player_num];
		damage_taken_totals* next_dtt = NULL;
		damage_taken_totals* dtt_a = NULL;
		damage_taken_totals* dtt_b = NULL;
		while (dtt != NULL) {
			next_dtt = dtt->next;

			if (dtt->prev != NULL) {
				while (dtt != NULL && dtt->prev != NULL && dtt->total_damage > dtt->prev->total_damage) {
					dtt_a = dtt->prev;
					dtt_b = dtt;

					dtt_a->next = dtt_b->next;
					dtt_b->prev = dtt_a->prev;

					if (dtt_a->next != NULL) {
						dtt_a->next->prev = dtt_a;
					}

					if (dtt_b->prev != NULL) {
						dtt_b->prev->next = dtt_b;
					}

					dtt_b->next = dtt_a;
					dtt_a->prev = dtt_b;

					if (dtt_a == First_damage_taken_previous_totals[player_num]) {
						First_damage_taken_previous_totals[player_num] = dtt_b;
					}
				}
			}

			dtt = next_dtt;
		}

		// Add to the kill log.
		kill_log_event* kle = (kill_log_event*)d_malloc(sizeof(kill_log_event));
		kle->timestamp = GameTime64;
		kle->killed_id = player_num;
		kle->killer_type = killer_type;
		kle->killer_id = killer_id;
		kle->damage_type = damage_type;
		kle->source_id = source_id;
		kle->next = Kill_log;
		Kill_log = kle;

		// Show the player's most recent death until the time listed.
		Show_death_until[player_num] = GameTime64 + i2f(15);
	}
}

void reset_observatory_stats() {
	int i;
	kill_event* ev;

	for (i = 0; i < MAX_PLAYERS; i++) {
		Last_kill[i] = NULL;
		Last_death[i] = NULL;
		Kill_streak[i] = 0;
		while ((ev = Last_event[i]) != NULL) {
			if (ev->prev != NULL) {
				ev->prev->next = NULL;
				Last_event[i] = ev->prev;
			} else {
				Last_event[i] = NULL;
				First_event[i] = NULL;
			}
			d_free(ev);
			ev = NULL;
		}
	}
}

void add_player_status(ubyte pnum, game_status status) {
	status.next = NULL;

	player_status* p_status = NULL;
	player_status* last_p_status = NULL;
	game_status* g_status = NULL;
	game_status* last_g_status = NULL;

	for (p_status = First_status; p_status != NULL; p_status = (last_p_status = p_status)->next) {
		if (p_status->pnum == pnum) {
			for (g_status = p_status->statuses; g_status != NULL; g_status = (last_g_status = g_status)->next) {
				// Found an existing status for this player.
				if (g_status->type == status.type) {
					memcpy(g_status, &status, sizeof(game_status));
					return;
				}
			}

			// Add a new status for this player.
			g_status = (game_status*)d_malloc(sizeof(game_status));
			last_g_status->next = g_status;
			memcpy(g_status, &status, sizeof(game_status));
			return;
		}
	}

	// Add the player with their first status.
	p_status = (player_status*)d_malloc(sizeof(player_status));
	p_status->next = NULL;
	if (last_p_status == NULL) {
		First_status = p_status;
	}
	else {
		last_p_status->next = p_status;
	}
	p_status->pnum = pnum;

	g_status = (game_status*)d_malloc(sizeof(game_status));
	p_status->statuses = g_status;
	memcpy(g_status, &status, sizeof(game_status));
}

void remove_player_status(ubyte pnum, ubyte type) {
	player_status* p_status = First_status;
	player_status* last_p_status = NULL;
	game_status* g_status = NULL;
	game_status* last_g_status = NULL;

	while (p_status != NULL) {
		if (p_status->pnum == pnum) {
			g_status = p_status->statuses;
			while (g_status != NULL) {
				if (g_status->type == type) {
					// We found the pnum and type combination, remove the game status.
					if (last_g_status == NULL) {
						p_status->statuses = g_status->next;
					}
					else {
						last_g_status->next = g_status->next;
					}
					d_free(g_status);

					// If this is the last player status, remove the player status as well.
					if (p_status->statuses == NULL) {
						if (last_p_status == NULL) {
							First_status = p_status->next;
						}
						else {
							last_p_status->next = p_status->next;
						}
						d_free(p_status);
					}

					// We're done, bail.
					return;
				}

				last_g_status = g_status;
				g_status = g_status->next;
			}

			// Player found, but type wasn't, bail.
			return;
		}

		last_p_status = p_status;
		p_status = p_status->next;
	}
	// Didn't find the player, nothing to do!
}

int GetMyNetRanking()
{
	int rank, eff;

	if (PlayerCfg.NetlifeKills+PlayerCfg.NetlifeKilled==0)
		return (1);

	rank=(int) (((float)PlayerCfg.NetlifeKills/3000.0)*8.0);

	eff=(int)((float)((float)PlayerCfg.NetlifeKills/((float)PlayerCfg.NetlifeKilled+(float)PlayerCfg.NetlifeKills))*100.0);

	if (rank>8)
		rank=8;

	if (eff<0)
		eff=0;

	if (eff<60)
		rank-=((59-eff)/10);

	if (rank<0)
		rank=0;
	if (rank>8)
		rank=8;

	return (rank+1);
}

void ClipRank (ubyte *rank)
{
	// This function insures no crashes when dealing with D2 1.0
	if (*rank > 9)
		*rank = 0;
}

//
//  Functions that replace what used to be macros
//

int objnum_remote_to_local(int remote_objnum, int owner)
{
	// Map a remote object number from owner to a local object number

	int result;

	if ((owner >= N_players) || (owner < -1)) {
		Int3(); // Illegal!
		return(remote_objnum);
	}

	if (owner == -1)
		return(remote_objnum);

	if ((remote_objnum < 0) || (remote_objnum >= MAX_OBJECTS))
		return(-1);

	result = remote_to_local[owner][remote_objnum];

	if (result < 0)
	{
		return(-1);
	}

	return(result);
}

int objnum_local_to_remote(int local_objnum, sbyte *owner)
{
	// Map a local object number to a remote + owner

	int result;

	if ((local_objnum < 0) || (local_objnum > Highest_object_index)) {
		*owner = -1;
		return(-1);
	}

	*owner = object_owner[local_objnum];

	if (*owner == -1)
		return(local_objnum);

	if ((*owner >= N_players) || (*owner < -1)) {
		Int3(); // Illegal!
		*owner = -1;
		return local_objnum;
	}

	result = local_to_remote[local_objnum];

	if (result < 0)
	{
		Int3(); // See Rob, object has no remote number!
	}

	return(result);
}

void
map_objnum_local_to_remote(int local_objnum, int remote_objnum, int owner)
{
	// Add a mapping from a network remote object number to a local one

	Assert(local_objnum > -1);
	Assert(local_objnum < MAX_OBJECTS);
	Assert(remote_objnum > -1);
	Assert(remote_objnum < MAX_OBJECTS);
	Assert(owner > -1);
	Assert(owner != Player_num);

	object_owner[local_objnum] = owner;

	remote_to_local[owner][remote_objnum] = local_objnum;
	local_to_remote[local_objnum] = remote_objnum;

	return;
}

void
map_objnum_local_to_local(int local_objnum)
{
	// Add a mapping for our locally created objects

	Assert(local_objnum > -1);
	Assert(local_objnum < MAX_OBJECTS);

	object_owner[local_objnum] = Player_num;
	remote_to_local[Player_num][local_objnum] = local_objnum;
	local_to_remote[local_objnum] = local_objnum;

	return;
}

void reset_network_objects()
{
	memset(local_to_remote, -1, MAX_OBJECTS*sizeof(short));
	memset(remote_to_local, -1, MAX_PLAYERS*MAX_OBJECTS*sizeof(short));
	memset(object_owner, -1, MAX_OBJECTS);
}

int multi_objnum_is_past(int objnum)
{
	switch (multi_protocol)
	{
		case MULTI_PROTO_UDP:
#ifdef USE_UDP
			return net_udp_objnum_is_past(objnum);
			break;
#endif
		default:
			Error("Protocol handling missing in multi_objnum_is_past\n");
			break;
	}
}

//
// Part 1 : functions whose main purpose in life is to divert the flow
//          of execution to either network  specific code based
//          on the curretn Game_mode value.
//

// Show a score list to end of net players
void multi_endlevel_score(void)
{
	int i, old_connect=0, game_wind_visible = 0;

	// If there still is a Game_wind and it's suspended (usually both shoudl be the case), bring it up again so host can still take actions of the game
	if (Game_wind)
	{
		if (!window_is_visible(Game_wind))
		{
			game_wind_visible = 1;
			window_set_visible(Game_wind, 1);
		}
	}
	// Save connect state and change to new connect state
#ifdef NETWORK
	if (Game_mode & GM_NETWORK)
	{
		old_connect = Players[Player_num].connected;
		if (Players[Player_num].connected!=CONNECT_DIED_IN_MINE)
			Players[Player_num].connected = CONNECT_END_MENU;
		Network_status = NETSTAT_ENDLEVEL;
	}
#endif

	// Do the actual screen we wish to show
	kmatrix_view(Game_mode & GM_NETWORK);

	// Restore connect state

	if (Game_mode & GM_NETWORK)
		Players[Player_num].connected = old_connect;

	if (Game_mode & GM_MULTI_COOP)
	{
		for (i = 0; i < Netgame.max_numplayers; i++)
			// Reset keys
			Players[i].flags &= ~(PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_GOLD_KEY);
	}

	for (i = 0; i < Netgame.max_numplayers; i++)
		Players[i].flags &= ~(PLAYER_FLAGS_FLAG);  // Clear capture flag

	for (i=0;i<MAX_PLAYERS;i++)
		Players[i].KillGoalCount=0;

	for(i=0; i < 2; i++) {
		Netgame.TeamKillGoalCount[i] = 0; 
	}

	for (i=0;i<MAX_POWERUP_TYPES;i++)
	{
		MaxPowerupsAllowed[i]=0;
		PowerupsInMine[i]=0;
	}

	// hide Game_wind again if we brought it up
	if (Game_wind && game_wind_visible)
		window_set_visible(Game_wind, 0);
}

int
get_team(int pnum)
{
	if (Netgame.team_vector & (1 << pnum))
		return 1;
	else
		return 0;
}

int get_team_size(int team_num)
{
	int team_size = 0;
	for (int i = 0; i < Netgame.max_numplayers; i++)
	{
		if (is_observer() && Netgame.host_is_obs && i == 0)
			continue;
		if (get_team(i) == team_num)
			team_size++;
	}
	return team_size;
}

void
multi_new_game(void)
{
	int i;

	// Reset variables for a new net game

	for (i = 0; i < MAX_PLAYERS; i++)
		init_player_stats_game(i);

	memset(kill_matrix, 0, MAX_PLAYERS*MAX_PLAYERS*2); // Clear kill matrix

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		sorted_kills[i] = i;
		Players[i].connected = CONNECT_DISCONNECTED;

		if (Current_obs_player == i) {
			reset_obs();
		}

		multi_sending_message[i] = 0;
	}

	for(i=0; i < 2; i++) {
		Netgame.TeamKillGoalCount[i] = 0; 
	}

	for (i = 0; i < MAX_ROBOTS_CONTROLLED; i++)
	{
		robot_controlled[i] = -1;
		robot_agitation[i] = 0;
		robot_fired[i] = 0;
	}

	for (i=0;i<MAX_POWERUP_TYPES;i++)
	{
		MaxPowerupsAllowed[i]=0;
		PowerupsInMine[i]=0;
	}

	team_kills[0] = team_kills[1] = 0;
	imulti_new_game=1;
	multi_quit_game = 0;
	Show_kill_list = 1;
	game_disable_cheats();

	// The observatory stats reset
	reset_observatory_stats();
	for (i = 0; i < MAX_PLAYERS; i++) {
		add_observatory_stat(i, OBSEV_NONE);
	}
	Next_graph = 5;
	Show_graph_until = -1;

	Send_ship_status = 0;
	Next_ship_status_time = 0;

	player_status* p_status = NULL;
	game_status* g_status = NULL;
	while ((p_status = First_status) != NULL) {
		First_status = p_status->next;
		while ((g_status = p_status->statuses) != NULL) {
			p_status->statuses = g_status->next;
			d_free(g_status);
		}
		d_free(p_status);
	}

	shield_status* sta = NULL;
	for (i = 0; i < MAX_PLAYERS; i++) {
		while ((sta = First_current_shield_status[i]) != NULL) {
			First_current_shield_status[i] = sta->next;
			d_free(sta);
		}
		Last_current_shield_status[i] = NULL;

		while ((sta = First_previous_shield_status[i]) != NULL) {
			First_previous_shield_status[i] = sta->next;
			d_free(sta);
		}
		Last_previous_shield_status[i] = NULL;

		Show_death_until[i] = 0;
	}

	damage_taken_totals* dtt = NULL;
	damage_done_totals* ddt = NULL;
	for (i = 0; i < MAX_PLAYERS; i++) {
		while ((dtt = First_damage_taken_totals[i]) != NULL) {
			First_damage_taken_totals[i] = dtt->next;
			if (dtt->next != NULL) {
				dtt->next->prev = NULL;
			}
			d_free(dtt);
		}

		while ((dtt = First_damage_taken_current_totals[i]) != NULL) {
			First_damage_taken_current_totals[i] = dtt->next;
			if (dtt->next != NULL) {
				dtt->next->prev = NULL;
			}
			d_free(dtt);
		}

		while ((dtt = First_damage_taken_previous_totals[i]) != NULL) {
			First_damage_taken_previous_totals[i] = dtt->next;
			if (dtt->next != NULL) {
				dtt->next->prev = NULL;
			}
			d_free(dtt);
		}

		while ((ddt = First_damage_done_totals[i]) != NULL) {
			First_damage_done_totals[i] = ddt->next;
			d_free(ddt);
		}
	}

	kill_log_event* kle = NULL;
	while ((kle = Kill_log) != NULL) {
		Kill_log = kle->next;
		d_free(kle);
	}
}

void
multi_make_player_ghost(int playernum)
{
	object *obj;

	if ((playernum == Player_num) || (playernum >= MAX_PLAYERS) || (playernum < 0))
	{
		Int3(); // Non-terminal, see Rob
		return;
	}

	obj = &Objects[Players[playernum].objnum];

	obj->type = OBJ_GHOST;
	obj->render_type = RT_NONE;
	obj->movement_type = MT_NONE;
	multi_reset_player_object(obj);

	if (Game_mode & GM_MULTI_ROBOTS)
		multi_strip_robots(playernum);
}

void
multi_make_ghost_player(int playernum)
{
	object *obj;

	if ((playernum == Player_num) || (playernum >= MAX_PLAYERS))
	{
		Int3(); // Non-terminal, see rob
		return;
	}

	obj = &Objects[Players[playernum].objnum];

	obj->type = OBJ_PLAYER;
	obj->movement_type = MT_PHYSICS;
	multi_reset_player_object(obj);
	if (playernum != Player_num)
		init_player_stats_new_ship(playernum);
}

int multi_get_kill_list(int *plist)
{
	// Returns the number of active net players and their
	// sorted order of kills
	int i;
	int n = 0;

	for (i = 0; i < N_players; i++)
		//if (Players[sorted_kills[i]].connected)
		plist[n++] = sorted_kills[i];

	if (n == 0)
		Int3(); // SEE ROB OR MATT

	//memcpy(plist, sorted_kills, N_players*sizeof(int));

	return(n);
}

void
multi_sort_kill_list(void)
{
	// Sort the kills list each time a new kill is added

	int kills[MAX_PLAYERS];
	int i;
	int changed = 1;

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if ((Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS))
			kills[i] = Players[i].score;
		else
		if (Show_kill_list==2)
		{
			if (Players[i].net_killed_total+Players[i].net_kills_total==0)
				kills[i]=-1;  // always draw the ones without any ratio last
			else
				kills[i]=(int)((float)((float)Players[i].net_kills_total/((float)Players[i].net_killed_total+(float)Players[i].net_kills_total))*100.0);
		}
		else
			kills[i] = Players[i].net_kills_total;
	}

	while (changed)
	{
		changed = 0;
		for (i = 0; i < N_players-1; i++)
		{
			if (kills[sorted_kills[i]] < kills[sorted_kills[i+1]])
			{
				changed = sorted_kills[i];
				sorted_kills[i] = sorted_kills[i+1];
				sorted_kills[i+1] = changed;
				changed = 1;
			}
		}
	}
}

void robo_anarchy_suicide_penalty() {
	if(Game_mode & GM_MULTI_ROBOTS) {
		if(Players[Player_num].score > 1000) {
			Players[Player_num].score -= 1000;
		} else {
			Players[Player_num].score = 0; 
		}
		
		multi_send_score();
	}
}

extern object *obj_find_first_of_type (int);

void multi_compute_kill(int killer, int killed)
{
	// Figure out the results of a network kills and add it to the
	// appropriate player's tally.

	int killed_pnum, killed_type;
	int killer_pnum, killer_type,killer_id;
	int TheGoal;
	char killed_name[(CALLSIGN_LEN*2)+4];
	char killer_name[(CALLSIGN_LEN*2)+4];

	// Both object numbers are localized already!

	if ((killed < 0) || (killed > Highest_object_index) || (killer < 0) || (killer > Highest_object_index))
	{
		Int3(); // See Rob, illegal value passed to compute_kill;
		return;
	}

	killed_type = Objects[killed].type;
	killer_type = Objects[killer].type;
	killer_id = Objects[killer].id;

	if ((killed_type != OBJ_PLAYER) && (killed_type != OBJ_GHOST))
	{
		Int3(); // compute_kill passed non-player object!
		return;
	}

	killed_pnum = Objects[killed].id;

	Assert ((killed_pnum >= 0) && (killed_pnum < N_players));

	if (Game_mode & GM_TEAM)
		sprintf(killed_name, "%s (%s)", Players[killed_pnum].callsign, Netgame.team_name[get_team(killed_pnum)]);
	else
		sprintf(killed_name, "%s", Players[killed_pnum].callsign);

	if (Newdemo_state == ND_STATE_RECORDING)
		newdemo_record_multi_death(killed_pnum);

	digi_play_sample( SOUND_HUD_KILL, F3_0 );

	if (Control_center_destroyed)
		Players[killed_pnum].connected=CONNECT_DIED_IN_MINE;

	if (killer_type == OBJ_CNTRLCEN)
	{
		Players[killed_pnum].net_killed_total++;
		Players[killed_pnum].net_kills_total--;

		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_multi_kill(killed_pnum, -1);

		if (killed_pnum == Player_num)
		{
			HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s.", TXT_YOU_WERE, TXT_KILLED_BY_NONPLAY);
			multi_add_lifetime_killed ();

			robo_anarchy_suicide_penalty();
		}
		else
			HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s %s.", killed_name, TXT_WAS, TXT_KILLED_BY_NONPLAY );

		add_observatory_stat(killed_pnum, OBSEV_DEATH | OBSEV_REACTOR);

		return;
	}

	else if ((killer_type != OBJ_PLAYER) && (killer_type != OBJ_GHOST))
	{
		if (killer_id==PMINE_ID && killer_type!=OBJ_ROBOT)
		{
			if (killed_pnum == Player_num)
				HUD_init_message_literal(HM_MULTI | HM_KILLFEED, "You were killed by a mine!");
			else
				HUD_init_message(HM_MULTI | HM_KILLFEED, "%s was killed by a mine!",killed_name);
		}
		else
		{
			if (killed_pnum == Player_num)
			{
				HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s.", TXT_YOU_WERE, TXT_KILLED_BY_ROBOT);
				multi_add_lifetime_killed();

				robo_anarchy_suicide_penalty();
			}
			else
				HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s %s.", killed_name, TXT_WAS, TXT_KILLED_BY_ROBOT );
		}
		Players[killed_pnum].net_killed_total++;

		add_observatory_stat(killed_pnum, OBSEV_DEATH | OBSEV_ROBOT);

		return;
	}

	killer_pnum = Objects[killer].id;

	if (Game_mode & GM_TEAM)
		sprintf(killer_name, "%s (%s)", Players[killer_pnum].callsign, Netgame.team_name[get_team(killer_pnum)]);
	else
		sprintf(killer_name, "%s", Players[killer_pnum].callsign);

	// Beyond this point, it was definitely a player-player kill situation

	if ((killer_pnum < 0) || (killer_pnum >= N_players))
		Int3(); // See rob, tracking down bug with kill HUD messages

	if (killer_pnum == killed_pnum)
	{
		if (!(Game_mode & GM_HOARD))
		{
			if (Game_mode & GM_TEAM)
			{
				team_kills[get_team(killed_pnum)] -= 1;
				Netgame.TeamKillGoalCount[get_team(killed_pnum)] -= 1; 
			}

			robo_anarchy_suicide_penalty();

			Players[killed_pnum].net_killed_total += 1;
			Players[killed_pnum].net_kills_total -= 1;
			Players[killer_pnum].KillGoalCount -=1; // Suicides count against kill goal

			if (Newdemo_state == ND_STATE_RECORDING)
				newdemo_record_multi_kill(killed_pnum, -1);
		}
		kill_matrix[killed_pnum][killed_pnum] += 1; // # of suicides

		if (killer_pnum == Player_num)
		{
			HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s %s!", TXT_YOU, TXT_KILLED, TXT_YOURSELF );
			multi_add_lifetime_killed();
		}
		else
			HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s", killed_name, TXT_SUICIDE);

		/* Bounty mode needs some lovin' */
		if( Game_mode & GM_BOUNTY && killed_pnum == Bounty_target && multi_i_am_master() )
		{
			/* Select a random number */
			int n = d_rand() % MAX_PLAYERS;
			
			/* Make sure they're valid: Don't check against kill flags,
			* just in case everyone's dead! */
			while( !Players[n].connected )
				n = d_rand() % MAX_PLAYERS;
			
			/* Select new target  - it will be sent later when we're done with this function */
			multi_new_bounty_target( n );
		}

		add_observatory_stat(killed_pnum, OBSEV_DEATH | OBSEV_SELF);
	}

	else
	{
		if (!(Game_mode & GM_HOARD))
		{
			if (Game_mode & GM_TEAM)
			{
				if (get_team(killed_pnum) == get_team(killer_pnum))
				{
					team_kills[get_team(killed_pnum)] -= 1;
					Players[killer_pnum].net_kills_total -= 1;
					Netgame.TeamKillGoalCount[get_team(killer_pnum)] -= 1; 
				}
				else
				{
					team_kills[get_team(killer_pnum)] += 1;
					Players[killer_pnum].net_kills_total += 1;
					Players[killer_pnum].KillGoalCount +=1;
					Netgame.TeamKillGoalCount[get_team(killer_pnum)] += 1; 
				}
			}
			else if( Game_mode & GM_BOUNTY )
			{
				/* Did the target die?  Did the target get a kill? */
				if( killed_pnum == Bounty_target || killer_pnum == Bounty_target )
				{
					/* Increment kill counts */
					Players[killer_pnum].net_kills_total++;
					Players[killer_pnum].KillGoalCount++;
					
					/* Record the kill in a demo */
					if( Newdemo_state == ND_STATE_RECORDING )
						newdemo_record_multi_kill( killer_pnum, 1 );
					
					/* If the target died, the new one is set! */
					if( killed_pnum == Bounty_target )
						multi_new_bounty_target( killer_pnum );
				}
			}
			else
			{
				Players[killer_pnum].net_kills_total += 1;
				Players[killer_pnum].KillGoalCount+=1;
			}
			
			if (Newdemo_state == ND_STATE_RECORDING && !( Game_mode & GM_BOUNTY ) )
				newdemo_record_multi_kill(killer_pnum, 1);
		}

		kill_matrix[killer_pnum][killed_pnum] += 1;
		Players[killed_pnum].net_killed_total += 1;

		if (Players[killer_pnum].net_kills_total >= Next_graph) {
			Next_graph += (Next_graph < 20 || N_players > 2 ? 5 : 1);
			Show_graph_until = GameTime64 + 15 * F1_0;
		}

		if (killer_pnum == Player_num) {
			HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s %s!", TXT_YOU, TXT_KILLED, killed_name);
			multi_add_lifetime_kills();
			if ((Game_mode & GM_MULTI_COOP) && (Players[Player_num].score >= 1000))
				add_points_to_score(-1000);

			else if (Game_mode & GM_MULTI_ROBOTS)
				add_points_to_score(10000);
		}
		else if (killed_pnum == Player_num)
		{
			HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s %s!", killer_name, TXT_KILLED, TXT_YOU);
			multi_add_lifetime_killed();
			if (Game_mode & GM_HOARD)
			{
				if (Players[Player_num].secondary_ammo[PROXIMITY_INDEX]>3)
					multi_send_play_by_play (1,killer_pnum,Player_num);
				else if (Players[Player_num].secondary_ammo[PROXIMITY_INDEX]>0)
					multi_send_play_by_play (0,killer_pnum,Player_num);
			}
		}
		else
			HUD_init_message(HM_MULTI | HM_KILLFEED, "%s %s %s!", killer_name, TXT_KILLED, killed_name);

		add_observatory_stat(killed_pnum, OBSEV_DEATH | OBSEV_PLAYER);
		add_observatory_stat(killer_pnum, OBSEV_KILL | OBSEV_PLAYER);
	}

	TheGoal=Netgame.KillGoal*10;

	if (Netgame.KillGoal>0)
	{
		int someone_won = 0; 

		if (Game_mode & GM_TEAM)
		{
			if(Netgame.TeamKillGoalCount[get_team(killer_pnum)] >= TheGoal) {
				HUD_init_message(HM_MULTI, "Kill goal reached by %s!",Netgame.team_name[get_team(killer_pnum)]);
				someone_won = 1; 
			}
		} else {
			if (Players[killer_pnum].KillGoalCount>=TheGoal)
			{
				if (killer_pnum==Player_num)
				{
					HUD_init_message_literal(HM_MULTI, "You reached the kill goal!");
					//Players[Player_num].shields=i2f(200);
				}
				else
					HUD_init_message(HM_MULTI, "%s has reached the kill goal!",Players[killer_pnum].callsign);

				someone_won = 1; 
			}
		}

		if(someone_won) {
			HUD_init_message_literal(HM_MULTI, "The control center has been destroyed!");
			net_destroy_controlcen (obj_find_first_of_type (OBJ_CNTRLCEN));
		}
	}

	multi_sort_kill_list();
	multi_show_player_list();
	Players[killed_pnum].flags&=(~(PLAYER_FLAGS_HEADLIGHT_ON));  // clear the killed guys flags/headlights
}

void multi_do_protocol_frame(int force, int listen)
{
	switch (multi_protocol)
	{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			net_udp_do_frame(force, listen);
			break;
#endif
		default:
			Error("Protocol handling missing in multi_do_protocol_frame\n");
			break;
	}
}

void multi_do_frame(void)
{
	static int lasttime=0;
	static fix64 last_update_time = 0;
	int i;

	if (!(Game_mode & GM_MULTI) || Newdemo_state == ND_STATE_PLAYBACK)
	{
		Int3();
		return;
	}

	if ((Game_mode & GM_NETWORK) && Netgame.PlayTimeAllowed && lasttime!=f2i (ThisLevelTime))
	{
		for (i=0;i<N_players;i++)
			if (Players[i].connected)
			{
				if (i==Player_num)
				{
					multi_send_heartbeat();
					lasttime=f2i(ThisLevelTime);
				}
				break;
			}
	}

	// Send update about our game mode-specific variables every 2 secs (to keep in sync since delayed kills can invalidate these infos on Clients)
	if (multi_i_am_master() && timer_query() >= last_update_time + (F1_0*2))
	{
		multi_send_gmode_update();
		last_update_time = timer_query();
	}

	multi_send_message(); // Send any waiting messages

	if (Game_mode & GM_MULTI_ROBOTS)
	{
		multi_check_robot_timeout();
	}

	multi_do_protocol_frame(0, 1);

	if (multi_quit_game)
	{
		multi_quit_game = 0;
		if (Game_wind)
			window_close(Game_wind);
	}
}

void multi_send_data(unsigned char* buf, int len, int priority)
{
	if (buf == multibuf)
		Assert(len <= sizeof(multibuf));
	if (len != message_length[(int)buf[0]]) {
		//Error("multi_send_data: Packet type %i length: %i, expected: %i\n", buf[0], len, message_length[(int)buf[0]]);
		con_printf(CON_URGENT, "multi_send_data: Packet type %i length: %i priority %i, expected: %i\n", buf[0], len, priority, message_length[(int)buf[0]]);
		for(int i = 0; i < len; i++) {
			con_printf(CON_URGENT, "    %d: %d\n", i, buf[i]);
		}
		return;
	}

	if (buf[0] >= sizeof(message_length) / sizeof(message_length[0])) {
		//Error("multi_send_data: Illegal packet type %i\n", buf[0]);
		con_printf(CON_URGENT, "multi_send_data: Illegal packet type %i\n", buf[0]);
		return; 
	}

	if (Game_mode & GM_NETWORK)
	{
		switch (multi_protocol)
		{
#ifdef USE_UDP
			case MULTI_PROTO_UDP:
				net_udp_send_data(buf, len, priority);
				break;
#endif
			default:
				Error("Protocol handling missing in multi_send_data\n");
				break;
		}
	}
}

void multi_send_obs_data(unsigned char* buf, int len)
{
	if (len != message_length[(int)buf[0]]) {
		//Error("multi_send_data: Packet type %i length: %i, expected: %i\n", buf[0], len, message_length[(int)buf[0]]);
		con_printf(CON_NORMAL, "multi_send_obs_data: Packet type %i length: %i, expected: %i\n", buf[0], len, message_length[(int)buf[0]]);
		for (int i = 0; i < len; i++) {
			con_printf(CON_NORMAL, "    %d: %d\n", i, buf[i]);
		}
		return;
	}
	if (buf[0] >= sizeof(message_length) / sizeof(message_length[0])) {
		con_printf(CON_NORMAL, "multi_send_obs_data: Illegal packet type %i\n", buf[0]);
		return;
	}

	if (Game_mode & GM_NETWORK)
	{
		switch (multi_protocol)
		{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			net_udp_send_mdata_direct(buf, len, 0, 0);
			break;
#endif
		default:
			Error("Protocol handling missing in multi_send_obs_data\n");
			break;
		}
	}
}

void multi_send_data_direct(unsigned char *buf, int len, int pnum, int priority)
{
	if (len != message_length[(int)buf[0]])
		Error("multi_send_data_direct: Packet type %i length: %i, expected: %i\n", buf[0], len, message_length[(int)buf[0]]);
	if (buf[0] >= sizeof(message_length) / sizeof(message_length[0]))
		Error("multi_send_data_direct: Illegal packet type %i\n", buf[0]);
	if (pnum < 0 || pnum > MAX_PLAYERS)
		Error("multi_send_data_direct: Illegal player num: %i\n", pnum);

	switch (multi_protocol)
	{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			net_udp_send_mdata_direct((ubyte *)multibuf, len, pnum, priority);
			break;
#endif
		default:
			Error("Protocol handling missing in multi_send_data_direct\n");
			break;
	}
}

void
multi_leave_game(void)
{

	if (!(Game_mode & GM_MULTI))
		return;

	if (Game_mode & GM_NETWORK)
	{
		Net_create_loc = 0;
		multi_send_position(Players[Player_num].objnum);
		multi_powcap_cap_objects();
		if (!Player_eggs_dropped)
		{
			drop_player_eggs(ConsoleObject);
			Player_eggs_dropped = 1;
		}
		multi_send_player_explode(MULTI_PLAYER_DROP);
	}

	multi_send_quit(MULTI_QUIT);

	if (Game_mode & GM_NETWORK)
	{
		switch (multi_protocol)
		{
#ifdef USE_UDP
			case MULTI_PROTO_UDP:
				net_udp_leave_game();
				break;
#endif
			default:
				Error("Protocol handling missing in multi_leave_game\n");
				break;
		}
	}
}

void
multi_show_player_list()
{
	if (!(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_COOP))
		return;

	if (Show_kill_list)
		return;

	Show_kill_list_timer = F1_0*5; // 5 second timer
	Show_kill_list = 1;
}

int
multi_endlevel(int *secret)
{
	int result = 0;

	switch (multi_protocol)
	{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			result = net_udp_endlevel(secret);
			break;
#endif
		default:
			Error("Protocol handling missing in multi_endlevel\n");
			break;
	}

	return(result);
}

int multi_endlevel_poll1( newmenu *menu, d_event *event, void *userdata )
{
	switch (multi_protocol)
	{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			return net_udp_kmatrix_poll1( menu, event, userdata );
			break;
#endif
		default:
			Error("Protocol handling missing in multi_endlevel_poll1\n");
			break;
	}
	
	return 0;	// kill warning
}

int multi_endlevel_poll2( newmenu *menu, d_event *event, void *userdata )
{
	switch (multi_protocol)
	{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			return net_udp_kmatrix_poll2( menu, event, userdata );
			break;
#endif
		default:
			Error("Protocol handling missing in multi_endlevel_poll2\n");
			break;
	}
	
	return 0;
}

void multi_send_endlevel_packet()
{
	switch (multi_protocol)
	{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			net_udp_send_endlevel_packet();
			break;
#endif
		default:
			Error("Protocol handling missing in multi_send_endlevel_packet\n");
			break;
	}
}

//
// Part 2 : functions that act on network messages and change the
//          the state of the game in some way.
//

void
multi_define_macro(int key)
{
	if (!(Game_mode & GM_MULTI))
		return;

	key &= (~KEY_SHIFTED);

	switch(key)
	{
		case KEY_F9:
			multi_defining_message = 1; break;
		case KEY_F10:
			multi_defining_message = 2; break;
		case KEY_F11:
			multi_defining_message = 3; break;
		case KEY_F12:
			multi_defining_message = 4; break;
		default:
			Int3();
	}

	if (multi_defining_message)     {
		key_toggle_repeat(1);
		multi_message_index = 0;
		Network_message[multi_message_index] = 0;
	}

}

char feedback_result[200];

void
multi_message_feedback(void)
{
	char *colon;
	int found = 0;
	int i;

	if (!( ((colon = strstr(Network_message, ": ")) == NULL) || (colon-Network_message < 1) || (colon-Network_message > CALLSIGN_LEN) ))
	{
		sprintf(feedback_result, "%s ", TXT_MESSAGE_SENT_TO);
		if ((Game_mode & GM_TEAM) && (atoi(Network_message) > 0) && (atoi(Network_message) < 3))
		{
			sprintf(feedback_result+strlen(feedback_result), "%s '%s'", TXT_TEAM, Netgame.team_name[atoi(Network_message)-1]);
			found = 1;
		}
		if (Game_mode & GM_TEAM)
		{
			for (i = 0; i < N_players; i++)
			{
				if (!d_strnicmp(Netgame.team_name[i], Network_message, colon-Network_message))
				{
					if (found)
						strcat(feedback_result, ", ");
					found++;
					if (!(found % 4))
						strcat(feedback_result, "\n");
					sprintf(feedback_result+strlen(feedback_result), "%s '%s'", TXT_TEAM, Netgame.team_name[i]);
				}
			}
		}
		for (i = 0; i < N_players; i++)
		{
			if ((!d_strnicmp(Players[i].callsign, Network_message, colon-Network_message)) && (i != Player_num) && (Players[i].connected))
			{
				if (found)
					strcat(feedback_result, ", ");
				found++;
				if (!(found % 4))
					strcat(feedback_result, "\n");
				sprintf(feedback_result+strlen(feedback_result), "%s", Players[i].callsign);
			}
		}
		if (!found)
			strcat(feedback_result, TXT_NOBODY);
		else
			strcat(feedback_result, ".");

		digi_play_sample(SOUND_HUD_MESSAGE, F1_0);

		Assert(strlen(feedback_result) < 200);

		HUD_init_message_literal(HM_MULTI, feedback_result);
		//sprintf (temp,"%s",colon);
		//sprintf (Network_message,"%s",temp);

	}
}

void
multi_send_macro(int key)
{

	if (is_observer()) { return; }

	if (! (Game_mode & GM_MULTI) )
		return;

	switch(key)
	{
		case KEY_F9:
			key = 0; break;
		case KEY_F10:
			key = 1; break;
		case KEY_F11:
			key = 2; break;
		case KEY_F12:
			key = 3; break;
		default:
			Int3();
	}

	if (!PlayerCfg.NetworkMessageMacro[key][0])
	{
		HUD_init_message_literal(HM_MULTI, TXT_NO_MACRO);
		return;
	}

	strcpy(Network_message, PlayerCfg.NetworkMessageMacro[key]);
	Network_message_reciever = 100;

	HUD_init_message(HM_MULTI, "%s '%s'", TXT_SENDING, Network_message);
	multi_message_feedback();
}


void
multi_send_message_start()
{
	if (Game_mode&GM_MULTI) {
		multi_sending_message[Player_num] = 1;
		if (!is_observer()) {
			multi_send_msgsend_state(1);
		}
		multi_message_index = 0;
		Network_message[multi_message_index] = 0;
		key_toggle_repeat(1);
	}
}

extern fix StartingShields;

extern int multi_who_is_master();
extern char NameReturning;
extern int force_cockpit_redraw;

void multi_send_message_end()
{
	if (is_observer() && !PlayerCfg.ObsChat[get_observer_game_mode()]) {
		return;
	}

	char *mytempbuf;
	int i,t;

	if (is_observer()) {
		multi_message_index = 0;
		multi_sending_message[Player_num] = 0;
		multi_send_msgsend_state(0);
		key_toggle_repeat(0);

		multi_send_obs_message();
		multi_message_feedback();
		game_flush_inputs();
		return;
	}

	Network_message_reciever = 100;

	if (!d_strnicmp (Network_message,"/Handicap: ",11))
	{
		mytempbuf=&Network_message[11];
		StartingShields=atol (mytempbuf);
		if (StartingShields<10)
			StartingShields=10;
		if (StartingShields>100)
		{
			sprintf (Network_message,"%s has tried to cheat!",Players[Player_num].callsign);
			StartingShields=100;
		}
		else
			sprintf (Network_message,"%s handicap is now %d",Players[Player_num].callsign,StartingShields);

		HUD_init_message(HM_MULTI, "Telling others of your handicap of %d!",StartingShields);
		StartingShields=i2f(StartingShields);
	}
	else if (!d_strnicmp (Network_message,"/move: ",7))
	{
		if ((Game_mode & GM_NETWORK) && (Game_mode & GM_TEAM))
		{
			int name_index=7;
			if (strlen(Network_message) > 7)
				while (Network_message[name_index] == ' ')
					name_index++;

			if (!multi_i_am_master())
			{
				HUD_init_message(HM_MULTI, "Only %s can move players!",Players[multi_who_is_master()].callsign);
				return;
			}

			if (strlen(Network_message)<=name_index)
			{
				HUD_init_message_literal(HM_MULTI, "You must specify a name to move");
				return;
			}

			for (i = 0; i < N_players; i++)
				if ((!d_strnicmp(Players[i].callsign, &Network_message[name_index], strlen(Network_message)-name_index)) && (Players[i].connected))
				{
					if ((Game_mode & GM_CAPTURE) && (Players[i].flags & PLAYER_FLAGS_FLAG))
					{
						HUD_init_message_literal(HM_MULTI, "Can't move player because s/he has a flag!");
						return;
					}

					if (Netgame.team_vector & (1<<i))
						Netgame.team_vector&=(~(1<<i));
					else
						Netgame.team_vector|=(1<<i);

					for (t=0;t<N_players;t++)
						if (Players[t].connected)
							multi_reset_object_texture (&Objects[Players[t].objnum]);
					reset_cockpit();

					multi_send_gmode_update();

					sprintf (Network_message,"%s has changed teams!",Players[i].callsign);
					if (i==Player_num)
					{
						HUD_init_message_literal(HM_MULTI, "You have changed teams!");
						reset_cockpit();
					}
					else
						HUD_init_message(HM_MULTI, "Moving %s to other team.",Players[i].callsign);
					break;
				}
		}
	}

	else if (!d_strnicmp (Network_message,"/kick: ",7) && (Game_mode & GM_NETWORK))
	{
		int name_index=7;
		if (strlen(Network_message) > 7)
			while (Network_message[name_index] == ' ')
				name_index++;

		if (!multi_i_am_master())
		{
			HUD_init_message(HM_MULTI, "Only %s can kick others out!",Players[multi_who_is_master()].callsign);
			multi_message_index = 0;
			multi_sending_message[Player_num] = 0;
			multi_send_msgsend_state(0);
			return;
		}
		if (strlen(Network_message)<=name_index)
		{
			HUD_init_message_literal(HM_MULTI, "You must specify a name to kick");
			multi_message_index = 0;
			multi_sending_message[Player_num] = 0;
			multi_send_msgsend_state(0);
			return;
		}

		if (Network_message[name_index] == '#' && isdigit(Network_message[name_index+1])) {
			int players[MAX_PLAYERS];
			int listpos = Network_message[name_index+1] - '0';

			if (Show_kill_list==1 || Show_kill_list==2) {
				if (listpos == 0 || listpos >= N_players) {
					HUD_init_message_literal(HM_MULTI, "Invalid player number for kick.");
					multi_message_index = 0;
					multi_sending_message[Player_num] = 0;
					multi_send_msgsend_state(0);
					return;
				}
				multi_get_kill_list(players);
				i = players[listpos];
				if ((i != Player_num) && (Players[i].connected))
					goto kick_player;
			}
			else HUD_init_message_literal(HM_MULTI, "You cannot use # kicking with in team display.");


		    multi_message_index = 0;
		    multi_sending_message[Player_num] = 0;
		    multi_send_msgsend_state(0);
			return;
		}


		for (i = 0; i < N_players; i++)
		if ((!d_strnicmp(Players[i].callsign, &Network_message[name_index], strlen(Network_message)-name_index)) && (i != Player_num) && (Players[i].connected)) {
			kick_player:;
				switch (multi_protocol)
				{
#ifdef USE_UDP
					case MULTI_PROTO_UDP:
						net_udp_dump_player(Netgame.players[i].protocol.udp.addr, 0, DUMP_KICKED);
						break;
#endif
					default:
						Error("Protocol handling missing in multi_send_message_end\n");
						break;
				}

				HUD_init_message(HM_MULTI, "Dumping %s...",Players[i].callsign);
				multi_message_index = 0;
				multi_sending_message[Player_num] = 0;
				multi_send_msgsend_state(0);
				return;
			}
	}
	
	else if (!d_strnicmp (Network_message,"/killreactor",12) && (Game_mode & GM_NETWORK) && !Control_center_destroyed)
	{
		if (!multi_i_am_master())
			HUD_init_message(HM_MULTI, "Only %s can kill the reactor this way!",Players[multi_who_is_master()].callsign);
		else
		{
			net_destroy_controlcen(NULL);
			multi_send_destroy_controlcen(-1,Player_num);
		}
		multi_message_index = 0;
		multi_sending_message[Player_num] = 0;
		multi_send_msgsend_state(0);
		return;
	}

	else if (!d_strnicmp (Network_message,"/noobs",6) && (Game_mode & GM_NETWORK) )
	{
		if (!multi_i_am_master())
			HUD_init_message(HM_MULTI, "Only %s can disconnect observers!",Players[multi_who_is_master()].callsign);
		else
		{
			for (int i = 0; i < Netgame.max_numobservers; i++) {
				if (Netgame.observers[i].connected == 1) {
					net_udp_dump_player(Netgame.observers[i].protocol.udp.addr, 0, DUMP_KICKED);
				}
			}
			Netgame.numobservers = 0; 
			HUD_init_message(HM_MULTI, "All observers disconnected.");
			multi_send_obs_update(1, 0);
		}
		multi_message_index = 0;
		multi_sending_message[Player_num] = 0;
		multi_send_msgsend_state(0);
		return; 
	}

	else
		HUD_init_message(HM_MULTI, "%s '%s'", TXT_SENDING, Network_message);

	multi_send_message();
	multi_message_feedback();

	multi_message_index = 0;
	multi_sending_message[Player_num] = 0;
	multi_send_msgsend_state(0);
	key_toggle_repeat(0);
	game_flush_inputs();
}

void multi_define_macro_end()
{
	Assert( multi_defining_message > 0 );

	strcpy( PlayerCfg.NetworkMessageMacro[multi_defining_message-1], Network_message );
	write_player_file();

	multi_message_index = 0;
	multi_defining_message = 0;
	key_toggle_repeat(0);
	game_flush_inputs();
}

int multi_message_input_sub(int key)
{
	switch( key )
	{
		case KEY_F8:
		case KEY_ESC:
			multi_sending_message[Player_num] = 0;
			multi_send_msgsend_state(0);
			multi_defining_message = 0;
			key_toggle_repeat(0);
			game_flush_inputs();
			return 1;
		case KEY_LEFT:
		case KEY_BACKSP:
		case KEY_PAD4:
			if (multi_message_index > 0)
				multi_message_index--;
			Network_message[multi_message_index] = 0;
			return 1;
		case KEY_ENTER:
			if ( multi_sending_message[Player_num] )
				multi_send_message_end();
			else if ( multi_defining_message )
				multi_define_macro_end();
			game_flush_inputs();
			return 1;
		default:
		{
			int ascii = key_ascii();
			if ( ascii < 255 )     {
				if (multi_message_index < MAX_MESSAGE_LEN-2 )   {
					Network_message[multi_message_index++] = ascii;
					Network_message[multi_message_index] = 0;
				} else if ( multi_sending_message[Player_num] )     {
					int i;
					char * ptext, * pcolon;
					ptext = NULL;
					Network_message[multi_message_index++] = ascii;
					Network_message[multi_message_index] = 0;
					for (i=multi_message_index-1; i>=0; i-- )       {
						if ( Network_message[i]==32 )   {
							ptext = &Network_message[i+1];
							Network_message[i] = 0;
							break;
						}
					}
					multi_send_message_end();
					if ( ptext )    {
						multi_sending_message[Player_num] = 1;
						multi_send_msgsend_state(1);
						pcolon = strstr( Network_message, ": " );
						if ( pcolon )
							strcpy( pcolon+1, ptext );
						else
							strcpy( Network_message, ptext );
						multi_message_index = strlen( Network_message );
					}
				}
			}
		}
	}
	
	return 0;
}

void
multi_send_message_dialog(void)
{
	newmenu_item m[1];
	int choice;

	if (!(Game_mode&GM_MULTI))
		return;

	Network_message[0] = 0;             // Get rid of old contents

	m[0].type=NM_TYPE_INPUT; m[0].text = Network_message; m[0].text_len = MAX_MESSAGE_LEN-1;
	choice = newmenu_do( NULL, TXT_SEND_MESSAGE, 1, m, NULL, NULL );

	if ((choice > -1) && (strlen(Network_message) > 0)) {
		Network_message_reciever = 100;
		HUD_init_message(HM_MULTI, "%s '%s'", TXT_SENDING, Network_message);
		multi_message_feedback();
	}
}



void
multi_do_death(int objnum)
{
	// Do any miscellaneous stuff for a new network player after death

	objnum = objnum;

	if (!(Game_mode & GM_MULTI_COOP))
	{
		Players[Player_num].flags |= (PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_GOLD_KEY);
	}
}

void
multi_do_fire(const ubyte *buf)
{
	ubyte weapon;
	char pnum;
	sbyte flags;
	//static dum=0;

	// Act out the actual shooting
	pnum = buf[1];
	weapon = (int)buf[2];
	flags = buf[4];
	Network_laser_track = GET_INTEL_SHORT(buf + 6);

	/* CED sniperpackets */
	vms_vector shot_orientation;
	shot_orientation.x = (fix) GET_INTEL_INT(buf + 8); 
	shot_orientation.y = (fix) GET_INTEL_INT(buf + 12); 
	shot_orientation.z = (fix) GET_INTEL_INT(buf + 16); 

	Assert (pnum < N_players);

	if (Objects[Players[(int)pnum].objnum].type == OBJ_GHOST)
		multi_make_ghost_player(pnum);

	if (weapon == FLARE_ADJUST)
		/* CED sniperpackets */
		Laser_player_fire( Objects+Players[(int)pnum].objnum, FLARE_ID, 6, 1, 0, shot_orientation);
	else if (weapon >= MISSILE_ADJUST) {
		int weapon_id,weapon_gun;

		weapon_id = Secondary_weapon_to_weapon_info[weapon-MISSILE_ADJUST];
		weapon_gun = Secondary_weapon_to_gun_num[weapon-MISSILE_ADJUST] + (flags & 1);

		if (weapon-MISSILE_ADJUST==GUIDED_INDEX)
		{
			Multi_is_guided=1;
		}

		/* CED sniperpackets */
		Laser_player_fire( Objects+Players[(int)pnum].objnum, weapon_id, weapon_gun, 1, 0, shot_orientation);
	}
	else {
		fix save_charge = Fusion_charge;

		if (weapon == FUSION_INDEX) {
			Fusion_charge = flags << 12;
		}
		if (weapon == LASER_ID) {
			if (flags & LASER_QUAD)
				Players[(int)pnum].flags |= PLAYER_FLAGS_QUAD_LASERS;
			else
				Players[(int)pnum].flags &= ~PLAYER_FLAGS_QUAD_LASERS;
		}

		/* CED sniperpackets */
		do_laser_firing(Players[(int)pnum].objnum, weapon, (int)buf[3], flags, (int)buf[5], shot_orientation);

		if (weapon == FUSION_INDEX)
			Fusion_charge = save_charge;
	}
}

void multi_do_message(const ubyte* cbuf)
{
	const char *buf = (const char *)cbuf;

	if (is_observer() && !PlayerCfg.ObsPlayerChat[get_observer_game_mode()]) {
		multi_sending_message[(int)buf[1]] = 0;
		return;
	}

	const char *tilde;
	char *colon;
	char mesbuf[100];
	char dollarbuf[100];
	int tloc,t;

	int loc = 0;
	//buf += 2;

	if(Netgame.FairColors)
		selected_player_rgb = player_rgb_all_blue; 
	else if(Netgame.BlackAndWhitePyros) 
		selected_player_rgb = player_rgb_alt; 
	else
		selected_player_rgb = player_rgb;

	if ((tilde=strchr (buf+loc,'$')))  // do that stupid name stuff
	{											// why'd I put this in?  Probably for the
		tloc=tilde-(buf+loc);				// same reason you can name your guidebot
		snprintf(dollarbuf, sizeof(dollarbuf), "%.*s%s%s", tloc, buf, Players[Player_num].callsign, buf+tloc+1);
		buf = dollarbuf;
	}

	if (((colon = strstr(buf+loc, ": ")) == NULL) || (colon-(buf+loc) < 1) || (colon-(buf+loc) > CALLSIGN_LEN))
	{
		int color = 0;
		mesbuf[0] = CC_COLOR;
		if (Game_mode & GM_TEAM)
			color = get_team((int)buf[1]);
		else
			color = Netgame.players[(int)buf[1]].color;//(int)buf[1]
		mesbuf[1] = BM_XRGB(selected_player_rgb[color].r,selected_player_rgb[color].g,selected_player_rgb[color].b);
		strcpy(&mesbuf[2], Players[(int)buf[1]].callsign);
		t = strlen(mesbuf);
		mesbuf[t] = ':';
		mesbuf[t+1] = CC_COLOR;
		mesbuf[t+2] = BM_XRGB(0, 31, 0);
		mesbuf[t+3] = 0;

		if (!PlayerCfg.NoChatSound)
			digi_play_sample(SOUND_HUD_MESSAGE, F1_0);
		HUD_init_message(HM_MULTI, "%s %s", mesbuf, buf+2);
		multi_sending_message[(int)buf[1]] = 0;
	}
	else
	{
		if ( (!d_strnicmp(Players[Player_num].callsign, buf+loc, colon-(buf+loc))) ||
			 ((Game_mode & GM_TEAM) && ( (get_team(Player_num) == atoi(buf+loc)-1) || !d_strnicmp(Netgame.team_name[get_team(Player_num)], buf+loc, colon-(buf+loc)))) )
		{
			int color = 0;
			mesbuf[0] = CC_COLOR;
			if (Game_mode & GM_TEAM)
				color = get_team((int)buf[1]);
			else
				color = (int)buf[1];
			mesbuf[1] = BM_XRGB(selected_player_rgb[color].r,selected_player_rgb[color].g,selected_player_rgb[color].b);
			strcpy(&mesbuf[2], Players[(int)buf[1]].callsign);
			t = strlen(mesbuf);
			mesbuf[t] = ':';
			mesbuf[t+1] = CC_COLOR;
			mesbuf[t+2] = BM_XRGB(0, 31, 0);
			mesbuf[t+3] = 0;

			if (!PlayerCfg.NoChatSound)
				digi_play_sample(SOUND_HUD_MESSAGE, F1_0);
			HUD_init_message(HM_MULTI, "%s %s", mesbuf, colon+1);
			multi_sending_message[(int)buf[1]] = 0;
		}
	}
}

void multi_do_obs_message(const ubyte* cbuf)
{
	if (!PlayerCfg.ObsChat[get_observer_game_mode()]) {
		return;
	}

	const char* buf = (const char*)cbuf;

	HUD_init_message(HM_MULTI, "%c%c%s", CC_COLOR, BM_XRGB(8, 8, 32), buf + 2);
}

void
multi_do_position(const ubyte *buf)
{
	ubyte pnum = 0;
#ifdef WORDS_BIGENDIAN
	shortpos sp;
#endif

	pnum = buf[1];

#ifndef WORDS_BIGENDIAN
	extract_shortpos(&Objects[Players[pnum].objnum], (shortpos *)(buf + 2),0);
#else
	memcpy((ubyte *)(sp.bytemat), (ubyte *)(buf + 2), 9);
	memcpy((ubyte *)&(sp.xo), (ubyte *)(buf + 11), 14);
	extract_shortpos(&Objects[Players[pnum].objnum], &sp, 1);
#endif

	if (Objects[Players[pnum].objnum].movement_type == MT_PHYSICS)
		set_thrust_from_velocity(&Objects[Players[pnum].objnum]);
}

void
multi_do_reappear(const ubyte *buf)
{
	short objnum;
	ubyte pnum = buf[1];

	objnum = GET_INTEL_SHORT(buf + 2);

	Assert(objnum >= 0);
	if (pnum != Objects[objnum].id)
		return;

	if (PKilledFlags[pnum]<=0) // player was not reported dead, so do not accept this packet
	{
		PKilledFlags[pnum]--;
		return;
	}

	multi_make_ghost_player(Objects[objnum].id);
	create_player_appearance_effect(&Objects[objnum]);
	PKilledFlags[pnum]=0;
}

void
multi_do_player_explode(const ubyte *buf)
{
	// Only call this for players, not robots.  pnum is player number, not
	// Object number.

	object *objp;
	int count;
	int pnum;
	int i;
	char remote_created;

	pnum = buf[1];

#ifdef NDEBUG
	if ((pnum < 0) || (pnum >= N_players))
		return;
#else
	Assert(pnum >= 0);
	Assert(pnum < N_players);
#endif

#ifdef NETWORK
	// If we are in the process of sending objects to a new player, reset that process
	if (Network_send_objects)
	{
		Network_send_objnum = -1;
	}
#endif

	// Stuff the Players structure to prepare for the explosion

	count = 2;
	Players[pnum].primary_weapon_flags = GET_INTEL_SHORT(buf + count); count += 2;
	Players[pnum].secondary_weapon_flags = GET_INTEL_SHORT(buf + count); count += 2;
	Players[pnum].laser_level = buf[count];                                                 count++;
	Players[pnum].secondary_ammo[HOMING_INDEX] = buf[count];                count++;
	Players[pnum].secondary_ammo[CONCUSSION_INDEX] = buf[count];count++;
	Players[pnum].secondary_ammo[SMART_INDEX] = buf[count];         count++;
	Players[pnum].secondary_ammo[MEGA_INDEX] = buf[count];          count++;
	Players[pnum].secondary_ammo[PROXIMITY_INDEX] = buf[count]; count++;

	Players[pnum].secondary_ammo[SMISSILE1_INDEX] = buf[count]; count++;
	Players[pnum].secondary_ammo[GUIDED_INDEX]    = buf[count]; count++;
	Players[pnum].secondary_ammo[SMART_MINE_INDEX]= buf[count]; count++;
	Players[pnum].secondary_ammo[SMISSILE4_INDEX] = buf[count]; count++;
	Players[pnum].secondary_ammo[SMISSILE5_INDEX] = buf[count]; count++;

	if( (Netgame.GaussAmmoStyle == GAUSS_STYLE_STEADY_RECHARGING) ||
		(Netgame.GaussAmmoStyle == GAUSS_STYLE_STEADY_RESPAWNING) )
	 {
		VulcanAmmoBoxesOnBoard[pnum] = GET_INTEL_SHORT(buf + count); count += 2;
		VulcanAmmoBoxesOnBoard[pnum] = GET_INTEL_SHORT(buf + count); count += 2;
	} else {
		Players[pnum].primary_ammo[VULCAN_INDEX] = GET_INTEL_SHORT(buf + count); count += 2;
		Players[pnum].primary_ammo[GAUSS_INDEX] = GET_INTEL_SHORT(buf + count); count += 2;
	}

	Players[pnum].flags = GET_INTEL_INT(buf + count);               count += 4;

	multi_powcap_adjust_remote_cap (pnum);

	objp = Objects+Players[pnum].objnum;

	//      objp->phys_info.velocity = *(vms_vector *)(buf+16); // 12 bytes
	//      objp->pos = *(vms_vector *)(buf+28);                // 12 bytes

	remote_created = buf[count++]; // How many did the other guy create?

	Net_create_loc = 0;

	drop_player_eggs_remote(objp, 1);

	// Create mapping from remote to local numbering system

	// We now handle this situation gracefully, Int3 not required
	//      if (Net_create_loc != remote_created)
	//              Int3(); // Probably out of object array space, see Rob

	for (i = 0; i < remote_created; i++)
	{
		short s;

		s = GET_INTEL_SHORT(buf + count);

		if ((i < Net_create_loc) && (s > 0))
			map_objnum_local_to_remote((short)Net_create_objnums[i], s, pnum);
		count += 2;
	}
	for (i = remote_created; i < Net_create_loc; i++) {
		Objects[Net_create_objnums[i]].flags |= OF_SHOULD_BE_DEAD;
	}

	if (buf[0] == MULTI_PLAYER_EXPLODE)
	{
		explode_badass_player(objp);

		objp->flags &= ~OF_SHOULD_BE_DEAD;              //don't really kill player
		multi_make_player_ghost(pnum);
	}
	else
	{
		create_player_appearance_effect(objp);
	}

	Players[pnum].flags &= ~(PLAYER_FLAGS_CLOAKED | PLAYER_FLAGS_INVULNERABLE | PLAYER_FLAGS_FLAG);
	Players[pnum].cloak_time = 0;

	PKilledFlags[pnum]++;
	if (PKilledFlags[pnum] < 1) // seems we got reappear already so make him player again!
	{
		multi_make_ghost_player(Objects[Players[pnum].objnum].id);
		create_player_appearance_effect(&Objects[Players[pnum].objnum]);
		PKilledFlags[pnum] = 0;
	}
}

void multi_obs_check_all_escaped()
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (Players[i].connected == CONNECT_PLAYING)
			return;
	}

	PlayerFinishedLevel(0);
}

/*
 * Process can compute a kill. If I am a Client this might be my own one (see multi_send_kill()) but with more specific data so I can compute my kill correctly.
 */
void
multi_do_kill(const ubyte *buf)
{
	int killer, killed;
	int count = 1;
	int pnum = (int)(buf[count]);
	int type = (int)(buf[0]);

	if (multi_i_am_master() && type != MULTI_KILL_CLIENT)
		return;
	if (!multi_i_am_master() && type != MULTI_KILL_HOST)
		return;

	if ((pnum < 0) || (pnum >= N_players))
	{
		Int3(); // Invalid player number killed
		return;
	}

	// I am host, I know what's going on so take this packet, add game_mode related info which might be necessary for kill computation and send it to everyone so they can compute their kills correctly
	if (multi_i_am_master())
	{
		memcpy(multibuf, buf, 5);
		multibuf[0] = MULTI_KILL_HOST;
		multibuf[5] = Netgame.team_vector;
		multibuf[6] = Bounty_target;

		multi_send_data(multibuf, 7, 2);
	}

	killed = Players[pnum].objnum;
	count += 1;
	killer = GET_INTEL_SHORT(buf + count);
	if (killer > 0)
		killer = objnum_remote_to_local(killer, (sbyte)buf[count+2]);
	if (!multi_i_am_master())
	{
		Netgame.team_vector = buf[5];
		Bounty_target = buf[6];
	}

	multi_compute_kill(killer, killed);

	if (Game_mode & GM_BOUNTY && multi_i_am_master()) // update in case if needed... we could attach this to this packet but... meh...
		multi_send_bounty();

	if (Control_center_destroyed) {
		reset_obs();
		Players[pnum].connected = CONNECT_DIED_IN_MINE;

		if (is_observer()) {
			multi_obs_check_all_escaped();
		}
	}
}


//      Changed by MK on 10/20/94 to send NULL as object to net_destroy_controlcen if it got -1
// which means not a controlcen object, but contained in another object
void multi_do_controlcen_destroy(const ubyte *buf)
{
	sbyte who;
	short objnum;

	objnum = GET_INTEL_SHORT(buf + 1);
	who = buf[3];

	if (Control_center_destroyed != 1)
	{
		if ((who < N_players) && (who != Player_num)) {
			HUD_init_message(HM_MULTI, "%s %s", Players[who].callsign, TXT_HAS_DEST_CONTROL);
		}
		else if (who == Player_num)
			HUD_init_message_literal(HM_MULTI, TXT_YOU_DEST_CONTROL);
		else
			HUD_init_message_literal(HM_MULTI, TXT_CONTROL_DESTROYED);

		if (objnum != -1)
			net_destroy_controlcen(Objects+objnum);
		else
			net_destroy_controlcen(NULL);
	}
}

void multi_do_escape(const ubyte *buf)
{
	int objnum;

	objnum = Players[(int)buf[1]].objnum;

	digi_play_sample(SOUND_HUD_MESSAGE, F1_0);
	digi_kill_sound_linked_to_object (objnum);

	if (buf[2] == 0)
	{
		HUD_init_message(HM_MULTI, "%s %s", Players[(int)buf[1]].callsign, TXT_HAS_ESCAPED);
		if (Game_mode & GM_NETWORK) {
			Players[(int)buf[1]].connected = CONNECT_ESCAPE_TUNNEL;

			if (Current_obs_player == (int)buf[1]) {
				reset_obs();
			}
		}

		if (!multi_goto_secret)
			multi_goto_secret = 2;
	}
	else if (buf[2] == 1)
	{
		HUD_init_message(HM_MULTI, "%s %s", Players[(int)buf[1]].callsign, TXT_HAS_FOUND_SECRET);
		if (Game_mode & GM_NETWORK) {
			Players[(int)buf[1]].connected = CONNECT_FOUND_SECRET;

			if (Current_obs_player == (int)buf[1]) {
				reset_obs();
			}
		}

		if (!multi_goto_secret)
			multi_goto_secret = 1;
	}
	create_player_appearance_effect(&Objects[objnum]);
	multi_make_player_ghost(buf[1]);

	if (is_observer()) {
		multi_obs_check_all_escaped();
	}
}

#define MAX_PACKETS 200 // Memory's cheap ;)
int is_recent_duplicate(const ubyte *buf) {
	const fix64 timeout = F1_0*10; 
	static ubyte received_packets[MAX_PACKETS*5]; // old pickup packets
	static fix64 rxtime[MAX_PACKETS];
	static ubyte num_waiting = 0;

	ubyte num_now_waiting = 0; 

	fix64 now = timer_query(); 


	// Clear out old ones
	for(int i = 0; i < num_waiting; i++) {
		if(now - rxtime[i] <= timeout) {
			if(num_now_waiting != i) {
				memcpy(received_packets + num_now_waiting*5, received_packets + i*5, 5); 
				rxtime[num_now_waiting] = rxtime[i];
			}

			num_now_waiting++; 
		} 
	}

	num_waiting = num_now_waiting; 

	// Search for dups
	for(int i = 0; i < num_waiting; i++) {
		if(! memcmp(received_packets + i*5, buf, 5)) {			
			return 1; 
		} 
	}

	// Not a dup, hold on to this one
	if(num_waiting < MAX_PACKETS) {
		memcpy(received_packets + num_waiting*5, buf, 5);
		rxtime[num_waiting] = now; 
		num_waiting++; 
	} 

	return 0; 
}

void
multi_do_remobj(const ubyte *buf)
{
	short objnum; // which object to remove
	short local_objnum;
	sbyte obj_owner; // which remote list is it entered in

	objnum = GET_INTEL_SHORT(buf + 1);
	obj_owner = buf[3];
	//ubyte counter = buf[4]; 

	Assert(objnum >= 0);

	if(is_recent_duplicate(buf)) {
		return; 
	}

	if (objnum < 1)
		return;

	local_objnum = objnum_remote_to_local(objnum, obj_owner); // translate to local objnum

	if (local_objnum < 0)
	{
		return;
	}

	if ((Objects[local_objnum].type != OBJ_POWERUP) && (Objects[local_objnum].type != OBJ_HOSTAGE))
	{
		return;
	}

	if (Network_send_objects && multi_objnum_is_past(local_objnum))
	{
		Network_send_objnum = -1;
	}
	if (Objects[local_objnum].type==OBJ_POWERUP)
		if (Game_mode & GM_NETWORK)
		{
#ifdef OLDPOWCAP
			if (PowerupsInMine[Objects[local_objnum].id]>0)
				PowerupsInMine[Objects[local_objnum].id]--;

			if (multi_powerup_is_4pack (Objects[local_objnum].id))
			{
				if (PowerupsInMine[Objects[local_objnum].id-1]-4<0)
					PowerupsInMine[Objects[local_objnum].id-1]=0;
				else
					PowerupsInMine[Objects[local_objnum].id-1]-=4;
			}
#else
			if (multi_powerup_is_4pack (Objects[local_objnum].id))
			{
				if (PowerupsInMine[Objects[local_objnum].id-1]-4<0)
					PowerupsInMine[Objects[local_objnum].id-1]=0;
				else
					PowerupsInMine[Objects[local_objnum].id-1]-=4;
			}
			else
			{
				if (PowerupsInMine[Objects[local_objnum].id]>0)
					PowerupsInMine[Objects[local_objnum].id]--;
			}
#endif
		}

	Objects[local_objnum].flags |= OF_SHOULD_BE_DEAD; // quick and painless

}

void multi_disconnect_player(int pnum)
{
	int i, n = 0;

	if (!(Game_mode & GM_NETWORK))
		return;
	if (Players[pnum].connected == CONNECT_DISCONNECTED)
		return;

	if (Players[pnum].connected == CONNECT_PLAYING)
	{
		digi_play_sample( SOUND_HUD_MESSAGE, F1_0 );
		HUD_init_message(HM_MULTI,  "%s %s", Players[pnum].callsign, TXT_HAS_LEFT_THE_GAME);

		multi_sending_message[pnum] = 0;

		if (Network_status == NETSTAT_PLAYING)
		{
			multi_make_player_ghost(pnum);
			multi_strip_robots(pnum);
		}

		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_multi_disconnect(pnum);

		// Bounty target left - select a new one
		if( Game_mode & GM_BOUNTY && pnum == Bounty_target && multi_i_am_master() )
		{
			/* Select a random number */
			int n = d_rand() % MAX_PLAYERS;
			
			/* Make sure they're valid: Don't check against kill flags,
				* just in case everyone's dead! */
			while( !Players[n].connected )
				n = d_rand() % MAX_PLAYERS;
			
			/* Select new target */
			multi_new_bounty_target( n );
			
			/* Send this new data */
			multi_send_bounty();
		}
	}

	Players[pnum].connected = CONNECT_DISCONNECTED;
	if (Current_obs_player == pnum) {
		reset_obs();
	}
	Netgame.players[pnum].connected = CONNECT_DISCONNECTED;
	PKilledFlags[pnum] = 1;

	switch (multi_protocol)
	{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			net_udp_disconnect_player(pnum);
			break;
#endif
		default:
			Error("Protocol handling missing in multi_disconnect_player\n");
			break;
	}

	if (pnum == multi_who_is_master()) // Host has left - Quit game!
	{
		if (Network_status==NETSTAT_PLAYING)
			multi_leave_game();
		if (Game_wind)
			window_set_visible(Game_wind, 0);
		nm_messagebox(NULL, 1, TXT_OK, "Host left the game!");
		if (Game_wind)
			window_set_visible(Game_wind, 1);
		multi_quit_game = 1;
		game_leave_menus();
		multi_reset_stuff();
		return;
	}

	if (is_observer()) {
		multi_obs_check_all_escaped();
	} else {
		for (i = 0; i < N_players; i++)
			if (Players[i].connected) n++;
		if (n == 1)
		{
			HUD_init_message_literal(HM_MULTI, "You are the only person remaining in this netgame");
		}
	}
}

void
multi_do_quit(const ubyte *buf)
{

	if (!(Game_mode & GM_NETWORK))
		return;
	multi_disconnect_player((int)buf[1]);
}

void
multi_do_cloak(const ubyte *buf)
{
	int pnum;

	pnum = (int)(buf[1]);

	Assert(pnum < N_players);

	Players[pnum].flags |= PLAYER_FLAGS_CLOAKED;
	Players[pnum].cloak_time = GameTime64;
	ai_do_cloak_stuff();

	if (Game_mode & GM_MULTI_ROBOTS)
		multi_strip_robots(pnum);

	if (Newdemo_state == ND_STATE_RECORDING)
		newdemo_record_multi_cloak(pnum);
}

void
multi_do_decloak(const ubyte *buf)
{
	int pnum;

	pnum = (int)(buf[1]);

	if (Newdemo_state == ND_STATE_RECORDING)
		newdemo_record_multi_decloak(pnum);

}

void
multi_do_invuln(const ubyte *buf)
{
	int pnum;

	pnum = buf[1];

	Assert(pnum < N_players);

	Players[pnum].flags |= PLAYER_FLAGS_INVULNERABLE;
	Players[pnum].invulnerable_time = GameTime64;
}

void
multi_do_door_open(const ubyte *buf)
{
	int segnum;
	sbyte side;
	segment *seg;
	wall *w;
	ubyte flag;

	segnum = GET_INTEL_SHORT(buf + 1);
	side = buf[3];
	flag= buf[4];

	if ((segnum < 0) || (segnum > Highest_segment_index) || (side < 0) || (side > 5))
	{
		Int3();
		return;
	}

	seg = &Segments[segnum];

	if (seg->sides[side].wall_num == -1) {  //Opening door on illegal wall
		Int3();
		return;
	}

	w = &Walls[seg->sides[side].wall_num];

	if (w->type == WALL_BLASTABLE)
	{
		if (!(w->flags & WALL_BLASTED))
		{
			wall_destroy(seg, side);
		}
		return;
	}
	else if (w->state != WALL_DOOR_OPENING)
	{
		wall_open_door(seg, side);
		w->flags=flag;
	}
	else
		w->flags=flag;

}

void
multi_do_create_explosion(const ubyte *buf)
{
	int pnum;
	int count = 1;

	pnum = buf[count++];

	create_small_fireball_on_object(&Objects[Players[pnum].objnum], F1_0, 1);
}

void
multi_do_controlcen_fire(const ubyte *buf)
{
	vms_vector to_target;
	char gun_num;
	short objnum;
	int count = 1;

	memcpy(&to_target, buf+count, 12);          count += 12;
#ifdef WORDS_BIGENDIAN  // swap the vector to_target
	to_target.x = (fix)INTEL_INT((int)to_target.x);
	to_target.y = (fix)INTEL_INT((int)to_target.y);
	to_target.z = (fix)INTEL_INT((int)to_target.z);
#endif
	gun_num = buf[count];                       count += 1;
	objnum = GET_INTEL_SHORT(buf + count);      count += 2;

	Laser_create_new_easy(&to_target, &Objects[objnum].ctype.reactor_info.gun_pos[(int)gun_num], objnum, CONTROLCEN_WEAPON_NUM, 1);
}

void
multi_do_create_powerup(const ubyte *buf)
{
	short segnum;
	short objnum;
	int my_objnum;
	char pnum;
	int count = 1;
	vms_vector new_pos;
	char powerup_type;

	if (Endlevel_sequence || Control_center_destroyed)
		return;

	pnum = buf[count++];
	powerup_type = buf[count++];
	segnum = GET_INTEL_SHORT(buf + count); count += 2;
	objnum = GET_INTEL_SHORT(buf + count); count += 2;

	if ((segnum < 0) || (segnum > Highest_segment_index)) {
		Int3();
		return;
	}

	memcpy(&new_pos, buf+count, sizeof(vms_vector)); count+=sizeof(vms_vector);
#ifdef WORDS_BIGENDIAN
	new_pos.x = (fix)SWAPINT((int)new_pos.x);
	new_pos.y = (fix)SWAPINT((int)new_pos.y);
	new_pos.z = (fix)SWAPINT((int)new_pos.z);
#endif

	Net_create_loc = 0;
	my_objnum = call_object_create_egg(&Objects[Players[(int)pnum].objnum], 1, OBJ_POWERUP, powerup_type);

	if (my_objnum < 0) {
		return;
	}

	if (Network_send_objects && multi_objnum_is_past(my_objnum))
	{
		Network_send_objnum = -1;
	}

	Objects[my_objnum].pos = new_pos;

	vm_vec_zero(&Objects[my_objnum].mtype.phys_info.velocity);

	obj_relink(my_objnum, segnum);

	map_objnum_local_to_remote(my_objnum, objnum, pnum);

	object_create_explosion(segnum, &new_pos, i2f(5), VCLIP_POWERUP_DISAPPEARANCE);

#ifdef OLDPOWCAP
	if (Game_mode & GM_NETWORK)
		PowerupsInMine[(int)powerup_type]++;
#else
	if (Game_mode & GM_NETWORK)
	{
		if (multi_powerup_is_4pack((int)powerup_type))
			PowerupsInMine[(int)(powerup_type-1)]+=4;
		else
			PowerupsInMine[(int)powerup_type]++;
	}
#endif
}

void
multi_do_play_sound(const ubyte *buf)
{
	int pnum = (int)(buf[1]);
	int sound_num = (int)(buf[2]);
	fix volume = (int)(buf[3]) << 12;

	if (!Players[pnum].connected)
		return;

	Assert(Players[pnum].objnum >= 0);
	Assert(Players[pnum].objnum <= Highest_object_index);

	digi_link_sound_to_object( sound_num, Players[pnum].objnum, 0, volume);
}

void
multi_do_score(const ubyte *buf)
{
	int pnum = (int)(buf[1]);

	if ((pnum < 0) || (pnum >= N_players))
	{
		Int3(); // Non-terminal, see rob
		return;
	}

	if (Newdemo_state == ND_STATE_RECORDING) {
		int score;
		score = GET_INTEL_INT(buf + 2);
		newdemo_record_multi_score(pnum, score);
	}

	add_observatory_stat(pnum, OBSEV_NONE);

	Players[pnum].score = GET_INTEL_INT(buf + 2);

	multi_sort_kill_list();
}

void
multi_do_trigger(const ubyte *buf)
{
	int pnum = (int)(buf[1]);
	int trigger = (int)(buf[2]);

	if ((pnum < 0) || (pnum >= N_players) || (pnum == Player_num))
	{
		Int3(); // Got trigger from illegal playernum
		return;
	}
	if ((trigger < 0) || (trigger >= Num_triggers))
	{
		Int3(); // Illegal trigger number in multiplayer
		return;
	}
	check_trigger_sub(trigger, pnum,0);
}

void multi_do_drop_marker (const ubyte *buf)
{
	int i;
	int pnum=(int)(buf[1]);
	int mesnum=(int)(buf[2]);
	vms_vector position;

	if (pnum==Player_num)  // my marker? don't set it down cuz it might screw up the orientation
		return;

	position.x = GET_INTEL_INT(buf + 3);
	position.y = GET_INTEL_INT(buf + 7);
	position.z = GET_INTEL_INT(buf + 11);

	for (i=0;i<40;i++)
		MarkerMessage[(pnum*2)+mesnum][i]=buf[15+i];

	MarkerPoint[(pnum*2)+mesnum]=position;

	if (MarkerObject[(pnum*2)+mesnum] !=-1 && Objects[MarkerObject[(pnum*2)+mesnum]].type!=OBJ_NONE && MarkerObject[(pnum*2)+mesnum] !=0)
		obj_delete(MarkerObject[(pnum*2)+mesnum]);

	MarkerObject[(pnum*2)+mesnum] = drop_marker_object(&position,Objects[Players[Player_num].objnum].segnum,&Objects[Players[Player_num].objnum].orient,(pnum*2)+mesnum);
}


void multi_do_hostage_door_status(const ubyte *buf)
{
	// Update hit point status of a door

	int count = 1;
	int wallnum;
	fix hps;

	wallnum = GET_INTEL_SHORT(buf + count);     count += 2;
	hps = GET_INTEL_INT(buf + count);           count += 4;

	if ((wallnum < 0) || (wallnum > Num_walls) || (hps < 0) || (Walls[wallnum].type != WALL_BLASTABLE))
	{
		Int3(); // Non-terminal, see Rob
		return;
	}

	if (hps < Walls[wallnum].hps)
		wall_damage(&Segments[Walls[wallnum].segnum], Walls[wallnum].sidenum, Walls[wallnum].hps - hps);
}

void
multi_reset_stuff(void)
{
	// A generic, emergency function to solve problems that crop up
	// when a player exits quick-out from the game because of a
	// connection loss.  Fixes several weird bugs!

	dead_player_end();
	Players[Player_num].homing_object_dist = -F1_0; // Turn off homing sound.
	reset_rear_view();
}

void
multi_reset_player_object(object *objp)
{
	int i;

	//Init physics for a non-console player

	Assert(objp >= Objects);
	Assert(objp <= Objects+Highest_object_index);
	Assert((objp->type == OBJ_PLAYER) || (objp->type == OBJ_GHOST));

	vm_vec_zero(&objp->mtype.phys_info.velocity);
	vm_vec_zero(&objp->mtype.phys_info.thrust);
	vm_vec_zero(&objp->mtype.phys_info.rotvel);
	vm_vec_zero(&objp->mtype.phys_info.rotthrust);
	objp->mtype.phys_info.brakes = objp->mtype.phys_info.turnroll = 0;
	objp->mtype.phys_info.mass = Player_ship->mass;
	objp->mtype.phys_info.drag = Player_ship->drag;
	//if (objp->type == OBJ_PLAYER)
	//	objp->mtype.phys_info.flags |= PF_TURNROLL | PF_WIGGLE;
	//else
		objp->mtype.phys_info.flags &= ~(PF_TURNROLL | PF_LEVELLING | PF_WIGGLE);

	//Init render info

	objp->render_type = RT_POLYOBJ;
	objp->rtype.pobj_info.model_num = Player_ship->model_num;               //what model is this?
	objp->rtype.pobj_info.subobj_flags = 0;         //zero the flags
	for (i=0;i<MAX_SUBMODELS;i++)
		vm_angvec_zero(&objp->rtype.pobj_info.anim_angles[i]);

	//reset textures for this, if not player 0

	multi_reset_object_texture (objp);

	// Clear misc

	objp->flags = 0;

	if (objp->type == OBJ_GHOST)
		objp->render_type = RT_NONE;

}

void disable_faircolors_if_3_connected() {
	if(Game_mode & GM_MULTI && Netgame.FairColors) {
		int num_connected = 0; 
		for(int i = 0; i < MAX_PLAYERS; i++) {
			if(Players[i].connected != CONNECT_DISCONNECTED) {
				num_connected++;
			}
		}

		if(num_connected > 2) {
			Netgame.FairColors = 0; 
		}
	}
}

int get_color_for_player(int player, int missile) {
	if (Game_mode & GM_TEAM) {
		return get_color_for_team(get_team(player));
	}

	int color = 0;

	if((! PlayerCfg.ShowCustomColors) || (! Netgame.AllowPreferredColors)) {
		color = player;
	} else {
		if(missile) { color = Netgame.players[player].missilecolor; }
		else        { color = Netgame.players[player].color;  }
	}

	if(Game_mode & GM_MULTI && Netgame.FairColors && !is_observer()) {
		return 0;
	}

	return(color); 
}

int get_color_for_team(int team)
{
	int team_color = Netgame.team_color[team];
	int color = team_color;

	// Are we using the player's preferred team colors?
	if (!is_observer() && (team_color == 8 || PlayerCfg.PreferMyTeamColors)) {
		int same_team = get_team(Player_num) == team;
		color = same_team ? PlayerCfg.MyTeamColor : PlayerCfg.OtherTeamColor;
		// No color set? Revert to what the game creator specified
		if (color == 8) color = team_color;
	}

	// If the team color is default, 0 is blue, 1 is red
	if (color == 8) color = team;

	return color;
}

void multi_reset_object_texture (object *objp)
{
	disable_faircolors_if_3_connected();

	int wid = get_color_for_player(objp->id, 0);
	int mid = get_color_for_player(objp->id, 1);

	//con_printf(CON_NORMAL, "Custom color for player %d is %d,%d\n", objp->id, wid, mid);

	int id;
	if (Game_mode & GM_TEAM)
		id = get_team(objp->id);
	else
		id = objp->id;

	if (id == 0) {
		if(wid == 0 && mid == 0) {
			objp->rtype.pobj_info.alt_textures=0;
		} else {
			objp->rtype.pobj_info.alt_textures=8;
			// Initialize the other textures
			for(int i = 0; i<Polygon_models[objp->rtype.pobj_info.model_num].n_textures; i++) {
				multi_player_textures[7][i] = ObjBitmaps[ObjBitmapPtrs[Polygon_models[objp->rtype.pobj_info.model_num].first_texture+i]];
			}
			multi_player_textures[7][4] = ObjBitmaps[ObjBitmapPtrs[First_multi_bitmap_num+(mid-1)*2]];
			multi_player_textures[7][5] = ObjBitmaps[ObjBitmapPtrs[First_multi_bitmap_num+(wid-1)*2+1]];
			multi_player_tex_color[7] = wid;
		}
	} else {
		if (N_PLAYER_SHIP_TEXTURES < Polygon_models[objp->rtype.pobj_info.model_num].n_textures)
			Error("Too many player ship textures!\n");

		for (int i=0;i<Polygon_models[objp->rtype.pobj_info.model_num].n_textures;i++)
			multi_player_textures[id-1][i] = ObjBitmaps[ObjBitmapPtrs[Polygon_models[objp->rtype.pobj_info.model_num].first_texture+i]];

		multi_player_textures[id-1][4] = ObjBitmaps[ObjBitmapPtrs[First_multi_bitmap_num+(mid-1)*2]];
		multi_player_textures[id-1][5] = ObjBitmaps[ObjBitmapPtrs[First_multi_bitmap_num+(wid-1)*2+1]];
		multi_player_tex_color[id-1] = wid;

		objp->rtype.pobj_info.alt_textures = id;
	}
}

void
multi_process_bigdata(const ubyte *buf, unsigned len)
{
	// Takes a bunch of messages, check them for validity,
	// and pass them to multi_process_data.

	unsigned type, sub_len, bytes_processed = 0;

	while( bytes_processed < len )  {
		type = buf[bytes_processed];

		if ( (type>= sizeof(message_length)/sizeof(message_length[0])))
		{
			con_printf( CON_DEBUG,"multi_process_bigdata: Invalid packet type %d!\n", type );
			return;
		}
		sub_len = message_length[type];

		Assert(sub_len > 0);

		if ( (bytes_processed+sub_len) > len )  {
			con_printf(CON_DEBUG, "multi_process_bigdata: packet type %d too short (%d>%d)!\n", type, (bytes_processed+sub_len), len );
			Int3();
			return;
		}

		multi_process_data(&buf[bytes_processed], sub_len);
		bytes_processed += sub_len;
	}
}

//
// Part 2 : Functions that send communication messages to inform the other
//          players of something we did.
//

void multi_send_fire(int laser_gun, int laser_level, int laser_flags, int laser_fired, short laser_track)
{
	if (is_observer()) { return; }

	multi_do_protocol_frame(1, 0); // provoke positional update if possible

	multibuf[0] = (char)MULTI_FIRE;
	multibuf[1] = (char)Player_num;
	multibuf[2] = (char)laser_gun;
	multibuf[3] = (char)laser_level;
	multibuf[4] = (char)laser_flags;
	multibuf[5] = (char)laser_fired;
	PUT_INTEL_SHORT(multibuf+6, laser_track);

	/* CED sniperpackets */
	object* ownship = Objects + Players[Player_num].objnum;
	PUT_INTEL_INT(multibuf+8 , ownship->orient.fvec.x);
	PUT_INTEL_INT(multibuf+12, ownship->orient.fvec.y);
	PUT_INTEL_INT(multibuf+16, ownship->orient.fvec.z);

	//multi_send_data(multibuf, 8, 1);
	multi_send_data(multibuf, 20, 1);
}

void
multi_send_destroy_controlcen(int objnum, int player)
{
	if (is_observer()) { return; }

	if (player == Player_num)
		HUD_init_message_literal(HM_MULTI, TXT_YOU_DEST_CONTROL);
	else if ((player >= 0) && (player < N_players))
		HUD_init_message(HM_MULTI, "%s %s", Players[player].callsign, TXT_HAS_DEST_CONTROL);
	else
		HUD_init_message_literal(HM_MULTI, TXT_CONTROL_DESTROYED);

	multibuf[0] = (char)MULTI_CONTROLCEN;
	PUT_INTEL_SHORT(multibuf+1, objnum);
	multibuf[3] = player;
	multi_send_data(multibuf, 4, 2);
}

void multi_send_drop_marker (int player,vms_vector position,char messagenum,char text[])
{
	if (is_observer()) { return; }

	int i;

	if (player<N_players)
	{
		multibuf[0]=(char)MULTI_MARKER;
		multibuf[1]=(char)player;
		multibuf[2]=messagenum;
		PUT_INTEL_INT(multibuf+3, position.x);
		PUT_INTEL_INT(multibuf+7, position.y);
		PUT_INTEL_INT(multibuf+11, position.z);
		for (i=0;i<40;i++)
			multibuf[15+i]=text[i];
	}
	multi_send_data(multibuf, 55, 2);
}

void multi_send_markers()
{
	if (is_observer()) { return; }

	// send marker positions/text to new player
	int i;

	for (i = 0; i < N_players; i++)
	{
		if (MarkerObject[(i*2)]!=-1)
			multi_send_drop_marker (i,MarkerPoint[(i*2)],0,MarkerMessage[i*2]);
		if (MarkerObject[(i*2)+1]!=-1)
			multi_send_drop_marker (i,MarkerPoint[(i*2)+1],1,MarkerMessage[(i*2)+1]);
	}
}

void
multi_send_endlevel_start(int secret)
{
	if (is_observer()) { return; }

	multibuf[0] = (char)MULTI_ENDLEVEL_START;
	multibuf[1] = Player_num;
	multibuf[2] = (char)secret;

	if ((secret) && !multi_goto_secret)
		multi_goto_secret = 1;
	else if (!multi_goto_secret)
		multi_goto_secret = 2;

	multi_send_data(multibuf, 3, 2);
	if (Game_mode & GM_NETWORK)
	{
		Players[Player_num].connected = CONNECT_ESCAPE_TUNNEL;

		switch (multi_protocol)
		{
#ifdef USE_UDP
			case MULTI_PROTO_UDP:
				net_udp_send_endlevel_packet();
				break;
#endif
			default:
				Error("Protocol handling missing in multi_send_endlevel_start\n");
				break;
		}

		if (is_observer()) {
			multi_obs_check_all_escaped();
		}
	}
}

void
multi_send_player_explode(char type)
{
	if (is_observer()) { return; }

	int count = 0;
	int i;

	Assert( (type == MULTI_PLAYER_DROP) || (type == MULTI_PLAYER_EXPLODE) );

	if (Network_send_objects)
	{
		Network_send_objnum = -1;
	}

	multi_send_position(Players[Player_num].objnum);

	multibuf[count++] = type;
	multibuf[count++] = Player_num;

	PUT_INTEL_SHORT(multibuf+count, Players[Player_num].primary_weapon_flags);
	count += 2;
	PUT_INTEL_SHORT(multibuf+count, Players[Player_num].secondary_weapon_flags);
	count += 2;
	multibuf[count++] = (char)Players[Player_num].laser_level;

	multibuf[count++] = (char)Players[Player_num].secondary_ammo[HOMING_INDEX];
	if(Netgame.RespawnConcs) {
		multibuf[count++] = (char)RespawningConcussions[Player_num];
	} else {
		multibuf[count++] = (char)Players[Player_num].secondary_ammo[CONCUSSION_INDEX];
	}	
	multibuf[count++] = (char)Players[Player_num].secondary_ammo[SMART_INDEX];
	multibuf[count++] = (char)Players[Player_num].secondary_ammo[MEGA_INDEX];
	multibuf[count++] = (char)Players[Player_num].secondary_ammo[PROXIMITY_INDEX];

	multibuf[count++] = (char)Players[Player_num].secondary_ammo[SMISSILE1_INDEX];
	multibuf[count++] = (char)Players[Player_num].secondary_ammo[GUIDED_INDEX];
	multibuf[count++] = (char)Players[Player_num].secondary_ammo[SMART_MINE_INDEX];
	multibuf[count++] = (char)Players[Player_num].secondary_ammo[SMISSILE4_INDEX];
	multibuf[count++] = (char)Players[Player_num].secondary_ammo[SMISSILE5_INDEX];

	if( (Netgame.GaussAmmoStyle == GAUSS_STYLE_STEADY_RECHARGING) ||
		(Netgame.GaussAmmoStyle == GAUSS_STYLE_STEADY_RESPAWNING) )
	{
		PUT_INTEL_SHORT(multibuf+count, (short)(VulcanAmmoBoxesOnBoard[Player_num]) );
		count += 2;
		PUT_INTEL_SHORT(multibuf+count, (short)(VulcanAmmoBoxesOnBoard[Player_num]) );
		count += 2;
	} else {
		PUT_INTEL_SHORT(multibuf+count, Players[Player_num].primary_ammo[VULCAN_INDEX] );
		count += 2;
		PUT_INTEL_SHORT(multibuf+count, Players[Player_num].primary_ammo[GAUSS_INDEX] );
		count += 2;
	}
	PUT_INTEL_INT(multibuf+count, Players[Player_num].flags );
	count += 4;

	multibuf[count++] = Net_create_loc;

	Assert(Net_create_loc <= MAX_NET_CREATE_OBJECTS);

	memset(multibuf+count, -1, MAX_NET_CREATE_OBJECTS*sizeof(short));

	for (i = 0; i < Net_create_loc; i++)
	{
		if (Net_create_objnums[i] <= 0) {
			Int3(); // Illegal value in created egg object numbers
			count +=2;
			continue;
		}

		PUT_INTEL_SHORT(multibuf+count, Net_create_objnums[i]); count += 2;

		// We created these objs so our local number = the network number
		map_objnum_local_to_local((short)Net_create_objnums[i]);
	}

	Net_create_loc = 0;

	if (count > message_length[MULTI_PLAYER_EXPLODE])
	{
		Int3(); // See Rob
	}

	multi_send_data(multibuf, message_length[MULTI_PLAYER_EXPLODE], 2);
	if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED)
		multi_send_decloak();
	if (Game_mode & GM_MULTI_ROBOTS)
		multi_strip_robots(Player_num);
}

extern int Proximity_dropped, Smartmines_dropped;

/*
 * Powerup capping: Keep track of how many powerups are in level and kill these which would exceed initial limit.
 * NOTE: code encapsuled by OLDPOWCAP define is original and buggy Descent2 code. 
 */

// Count the initial amount of Powerups in the level
void multi_powcap_count_powerups_in_mine(void)
{
	int i;

	for (i=0;i<MAX_POWERUP_TYPES;i++)
		PowerupsInMine[i]=0;
		
	for (i=0;i<=Highest_object_index;i++) 
	{
		if (Objects[i].type==OBJ_POWERUP)
		{
#ifdef OLDPOWCAP
			PowerupsInMine[Objects[i].id]++;
			if (multi_powerup_is_4pack(Objects[i].id))
				PowerupsInMine[Objects[i].id-1]+=4;
#else
			if (multi_powerup_is_4pack(Objects[i].id))
				PowerupsInMine[Objects[i].id-1]+=4;
			else
				PowerupsInMine[Objects[i].id]++;
#endif
		}
	}
}

// We want to drop something. Kill every Powerup which exceeds the level limit
void multi_powcap_cap_objects()
{
	char type,flagtype;
	int index;

	if (!(Game_mode & GM_NETWORK))
		return;


	// Bad interaction with savegames -- CED -- coopfix
	if(Game_mode & GM_MULTI_COOP) {
		return;
	}

	if (!(Game_mode & GM_HOARD))
	  	Players[Player_num].secondary_ammo[PROXIMITY_INDEX]+=Proximity_dropped;
	Players[Player_num].secondary_ammo[SMART_MINE_INDEX]+=Smartmines_dropped;
	Proximity_dropped=0;
	Smartmines_dropped=0;

	// Don't even try.  TODO: There is no try, only do.
	if (Netgame.PrimaryDupFactor > 1 || Netgame.SecondaryDupFactor > 1 || Netgame.SecondaryCapFactor > 1) {
		return;
	}

	for (index=0;index<MAX_PRIMARY_WEAPONS;index++)
	{
		type=Primary_weapon_to_powerup[index];
		if (PowerupsInMine[(int)type]>=MaxPowerupsAllowed[(int)type])
			if(Players[Player_num].primary_weapon_flags & (1 << index))
			{
				con_printf(CON_VERBOSE,"PIM=%d MPA=%d\n",PowerupsInMine[(int)type],MaxPowerupsAllowed[(int)type]);
				con_printf(CON_VERBOSE,"Killing a primary cuz there's too many! (%d)\n",type);
				Players[Player_num].primary_weapon_flags&=(~(1 << index));
			}
	}


	// Don't do the adjustment stuff for Hoard mode
	if (!(Game_mode & GM_HOARD))
		Players[Player_num].secondary_ammo[2]/=4;

	Players[Player_num].secondary_ammo[7]/=4;

	for (index=0;index<MAX_SECONDARY_WEAPONS;index++)
	{
		if ((Game_mode & GM_HOARD) && index==PROXIMITY_INDEX)
			continue;

		type=Secondary_weapon_to_powerup[index];

		if ((Players[Player_num].secondary_ammo[index]+PowerupsInMine[(int)type])>MaxPowerupsAllowed[(int)type])
		{
			if (MaxPowerupsAllowed[(int)type]-PowerupsInMine[(int)type]<0)
				Players[Player_num].secondary_ammo[index]=0;
			else
				Players[Player_num].secondary_ammo[index]=(MaxPowerupsAllowed[(int)type]-PowerupsInMine[(int)type]);
			con_printf(CON_VERBOSE,"Hey! I killed secondary type %d because PIM=%d MPA=%d\n",type,PowerupsInMine[(int)type],MaxPowerupsAllowed[(int)type]);
		}
	}

	if (!(Game_mode & GM_HOARD))
		Players[Player_num].secondary_ammo[2]*=4;
	Players[Player_num].secondary_ammo[7]*=4;

	if (Players[Player_num].laser_level > MAX_LASER_LEVEL)
		if (PowerupsInMine[POW_SUPER_LASER]+1 > MaxPowerupsAllowed[POW_SUPER_LASER])
			Players[Player_num].laser_level=0;

	if (Players[Player_num].flags & PLAYER_FLAGS_QUAD_LASERS)
		if (PowerupsInMine[POW_QUAD_FIRE]+1 > MaxPowerupsAllowed[POW_QUAD_FIRE])
			Players[Player_num].flags&=(~PLAYER_FLAGS_QUAD_LASERS);

	if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED)
		if (PowerupsInMine[POW_CLOAK]+1 > MaxPowerupsAllowed[POW_CLOAK])
			Players[Player_num].flags&=(~PLAYER_FLAGS_CLOAKED);

	if (Players[Player_num].flags & PLAYER_FLAGS_MAP_ALL)
		if (PowerupsInMine[POW_FULL_MAP]+1 > MaxPowerupsAllowed[POW_FULL_MAP])
			Players[Player_num].flags&=(~PLAYER_FLAGS_MAP_ALL);

	if (Players[Player_num].flags & PLAYER_FLAGS_AFTERBURNER)
		if (PowerupsInMine[POW_AFTERBURNER]+1 > MaxPowerupsAllowed[POW_AFTERBURNER])
			Players[Player_num].flags&=(~PLAYER_FLAGS_AFTERBURNER);

	if (Players[Player_num].flags & PLAYER_FLAGS_AMMO_RACK)
		if (PowerupsInMine[POW_AMMO_RACK]+1 > MaxPowerupsAllowed[POW_AMMO_RACK])
			Players[Player_num].flags&=(~PLAYER_FLAGS_AMMO_RACK);

	if (Players[Player_num].flags & PLAYER_FLAGS_CONVERTER)
		if (PowerupsInMine[POW_CONVERTER]+1 > MaxPowerupsAllowed[POW_CONVERTER])
			Players[Player_num].flags&=(~PLAYER_FLAGS_CONVERTER);

	if (Players[Player_num].flags & PLAYER_FLAGS_HEADLIGHT)
		if (PowerupsInMine[POW_HEADLIGHT]+1 > MaxPowerupsAllowed[POW_HEADLIGHT])
			Players[Player_num].flags&=(~PLAYER_FLAGS_HEADLIGHT);

	if (Game_mode & GM_CAPTURE)
	{
		if (Players[Player_num].flags & PLAYER_FLAGS_FLAG)
		{
			if (get_team(Player_num)==TEAM_RED)
				flagtype=POW_FLAG_BLUE;
			else
				flagtype=POW_FLAG_RED;

			if (PowerupsInMine[(int)flagtype]+1 > MaxPowerupsAllowed[(int)flagtype])
				Players[Player_num].flags&=(~PLAYER_FLAGS_FLAG);
		}
	}

}

// Adds players inventory to multi cap
void multi_powcap_adjust_cap_for_player(int pnum)
{
	char type;

	int index;

	if (!(Game_mode & GM_NETWORK))
		return;

	for (index=0;index<MAX_PRIMARY_WEAPONS;index++)
	{
		type=Primary_weapon_to_powerup[index];
		if (Players[pnum].primary_weapon_flags & (1 << index))
		    MaxPowerupsAllowed[(int)type]++;
	}

	for (index=0;index<MAX_SECONDARY_WEAPONS;index++)
	{
		type=Secondary_weapon_to_powerup[index];
		MaxPowerupsAllowed[(int)type]+=Players[pnum].secondary_ammo[index];
	}

	if (Players[pnum].laser_level > MAX_LASER_LEVEL)
		MaxPowerupsAllowed[POW_SUPER_LASER]++;

	if (Players[pnum].flags & PLAYER_FLAGS_QUAD_LASERS)
		MaxPowerupsAllowed[POW_QUAD_FIRE]++;

	if (Players[pnum].flags & PLAYER_FLAGS_CLOAKED)
		MaxPowerupsAllowed[POW_CLOAK]++;

	if (Players[pnum].flags & PLAYER_FLAGS_MAP_ALL)
		MaxPowerupsAllowed[POW_FULL_MAP]++;

	if (Players[pnum].flags & PLAYER_FLAGS_AFTERBURNER)
		MaxPowerupsAllowed[POW_AFTERBURNER]++;

	if (Players[pnum].flags & PLAYER_FLAGS_AMMO_RACK)
		MaxPowerupsAllowed[POW_AMMO_RACK]++;

	if (Players[pnum].flags & PLAYER_FLAGS_CONVERTER)
		MaxPowerupsAllowed[POW_CONVERTER]++;

	if (Players[pnum].flags & PLAYER_FLAGS_HEADLIGHT)
		MaxPowerupsAllowed[POW_HEADLIGHT]++;
}

void multi_powcap_adjust_remote_cap(int pnum)
{
	char type;

	int index;

	if (!(Game_mode & GM_NETWORK))
		return;

	for (index=0;index<MAX_PRIMARY_WEAPONS;index++)
	{
		type=Primary_weapon_to_powerup[index];
		if (Players[pnum].primary_weapon_flags & (1 << index))
		    PowerupsInMine[(int)type]++;
	}

	for (index=0;index<MAX_SECONDARY_WEAPONS;index++)
	{
		type=Secondary_weapon_to_powerup[index];

		if ((Game_mode & GM_HOARD) && index==2)
			continue;

		if (index==2 || index==7) // PROX or SMARTMINES? Those bastards...
			PowerupsInMine[(int)type]+=(Players[pnum].secondary_ammo[index]/4);
		else
			PowerupsInMine[(int)type]+=Players[pnum].secondary_ammo[index];

	}

	if (Players[pnum].laser_level > MAX_LASER_LEVEL)
		PowerupsInMine[POW_SUPER_LASER]++;

	if (Players[pnum].flags & PLAYER_FLAGS_QUAD_LASERS)
		PowerupsInMine[POW_QUAD_FIRE]++;

	if (Players[pnum].flags & PLAYER_FLAGS_CLOAKED)
		PowerupsInMine[POW_CLOAK]++;

	if (Players[pnum].flags & PLAYER_FLAGS_MAP_ALL)
		PowerupsInMine[POW_FULL_MAP]++;

	if (Players[pnum].flags & PLAYER_FLAGS_AFTERBURNER)
		PowerupsInMine[POW_AFTERBURNER]++;

	if (Players[pnum].flags & PLAYER_FLAGS_AMMO_RACK)
		PowerupsInMine[POW_AMMO_RACK]++;

	if (Players[pnum].flags & PLAYER_FLAGS_CONVERTER)
		PowerupsInMine[POW_CONVERTER]++;

	if (Players[pnum].flags & PLAYER_FLAGS_HEADLIGHT)
		PowerupsInMine[POW_HEADLIGHT]++;

}

void
multi_send_message(void)
{
	int loc = 0;
	if (Network_message_reciever != -1)
	{
		multibuf[loc] = (char)MULTI_MESSAGE;            loc += 1;
		multibuf[loc] = (char)Player_num;                       loc += 1;
		strncpy((char *)(multibuf+loc), Network_message, MAX_MESSAGE_LEN); loc += MAX_MESSAGE_LEN;
		multibuf[loc-1] = '\0';
		multi_send_data(multibuf, loc, 0);
		Network_message_reciever = -1;
	}
}

void multi_send_obs_message(void)
{
	int loc = 0;
	multibuf[loc] = MULTI_OBS_MESSAGE; loc += 1;
	multibuf[loc] = OBSERVER_PLAYER_ID; loc += 1;

	strncpy((char*)multibuf + loc, Players[Player_num].callsign, strlen(Players[Player_num].callsign)); loc += strlen(Players[Player_num].callsign);
	multibuf[loc] = ':'; loc += 1;
	multibuf[loc] = ' '; loc += 1;
	strncpy((char*)multibuf + loc, Network_message, MAX_MESSAGE_LEN); loc += MAX_MESSAGE_LEN;
	multibuf[12 + MAX_MESSAGE_LEN - 1] = '\0';
	multi_send_obs_data(multibuf, 12 + MAX_MESSAGE_LEN);

	if (multi_i_am_master()) {
		HUD_init_message(HM_MULTI, "%c%c%s: %s", (char)CC_COLOR, (char)BM_XRGB(8, 8, 32), Players[Player_num].callsign, Network_message);
	}
}

void
multi_send_reappear()
{
	if (is_observer()) { return; }

	multi_send_position(Players[Player_num].objnum);
	
	multibuf[0] = (char)MULTI_REAPPEAR;
	multibuf[1] = (char)Player_num;
	PUT_INTEL_SHORT(multibuf+2, Players[Player_num].objnum);

	multi_send_data(multibuf, 4, 2);
	PKilledFlags[Player_num]=0;
}

void
multi_send_position(int objnum)
{
	if (is_observer()) { return; }

#ifdef WORDS_BIGENDIAN
	shortpos sp;
#endif
	int count=0;

	multibuf[count++] = (char)MULTI_POSITION;
	multibuf[count++] = (char)Player_num;
#ifndef WORDS_BIGENDIAN
	create_shortpos((shortpos *)(multibuf+count), Objects+objnum,0);
	count += sizeof(shortpos);
#else
	create_shortpos(&sp, Objects+objnum, 1);
	memcpy(&(multibuf[count]), (ubyte *)(sp.bytemat), 9);
	count += 9;
	memcpy(&(multibuf[count]), (ubyte *)&(sp.xo), 14);
	count += 14;
#endif
	if(Netgame.RetroProtocol) {
		multi_send_data(multibuf, count, 0);
	} else {
		// send twice while first has priority so the next one will be attached to the next bigdata packet
		multi_send_data(multibuf, count, 2);
		multi_send_data(multibuf, count, 0);
	}
}

/* 
 * I was killed. If I am host, send this info to everyone and compute kill. If I am just a Client I'll only send the kill but not compute it for me. I (Client) will wait for Host to send me my kill back together with updated game_mode related variables which are important for me to compute consistent kill.
 */
void
multi_send_kill(int objnum)
{
	if (is_observer()) { return; }

	// I died, tell the world.

	int killer_objnum;
	int count = 0;

	Assert(Objects[objnum].id == Player_num);
	killer_objnum = Players[Player_num].killer_objnum;

	if (multi_i_am_master())
		multibuf[count] = (char)MULTI_KILL_HOST;
	else
		multibuf[count] = (char)MULTI_KILL_CLIENT;
							count += 1;
	multibuf[count] = Player_num;			count += 1;

	if (killer_objnum > -1)
	{
		short s = (short)objnum_local_to_remote(killer_objnum, (sbyte *)&multibuf[count+2]); // do it with variable since INTEL_SHORT won't work on return val from function.
		PUT_INTEL_SHORT(multibuf+count, s);
	}
	else
	{
		PUT_INTEL_SHORT(multibuf+count, -1);
		multibuf[count+2] = (char)-1;
	}
	count += 3;
	// I am host - I know what's going on so attach game_mode related info which might be vital for correct kill computation
	if (multi_i_am_master())
	{
		multibuf[count] = Netgame.team_vector;	count += 1;
		multibuf[count] = Bounty_target;	count += 1;
	}

	if (multi_i_am_master())
	{
		multi_send_data(multibuf, count, 2);
		multi_compute_kill(killer_objnum, objnum); // THIS TRASHES THE MULTIBUF!!!
	}
	else
		multi_send_data_direct((ubyte*)multibuf, count, multi_who_is_master(), 2); // I am just a client so I'll only send my kill but not compute it, yet. I'll get response from host so I can compute it correctly

	if (Game_mode & GM_MULTI_ROBOTS)
		multi_strip_robots(Player_num);

	if (Game_mode & GM_BOUNTY && multi_i_am_master()) // update in case if needed... we could attach this to this packet but... meh...
		multi_send_bounty();
}

void
multi_send_remobj(int objnum)
{
	// Tell the other guy to remove an object from his list
	if (is_observer()) { return; }

	sbyte obj_owner;
	short remote_objnum;

	static char remove_obj_counter = 0; 

	if (Objects[objnum].type==OBJ_POWERUP && (Game_mode & GM_NETWORK))
	{
#ifdef OLDPOWCAP
		if (PowerupsInMine[Objects[objnum].id]>0)
			PowerupsInMine[Objects[objnum].id]--;

		if (multi_powerup_is_4pack (Objects[objnum].id))
		{
			if (PowerupsInMine[Objects[objnum].id-1]-4<0)
				PowerupsInMine[Objects[objnum].id-1]=0;
			else
				PowerupsInMine[Objects[objnum].id-1]-=4;
		}
#else
		if (multi_powerup_is_4pack (Objects[objnum].id))
		{
			if (PowerupsInMine[Objects[objnum].id-1]-4<0)
				PowerupsInMine[Objects[objnum].id-1]=0;
			else
				PowerupsInMine[Objects[objnum].id-1]-=4;
		}
		else
		{
			if (PowerupsInMine[Objects[objnum].id]>0)
				PowerupsInMine[Objects[objnum].id]--;
		}
#endif
	}

	multibuf[0] = (char)MULTI_REMOVE_OBJECT;

	remote_objnum = objnum_local_to_remote((short)objnum, &obj_owner);

	PUT_INTEL_SHORT(multibuf+1, remote_objnum); // Map to network objnums

	multibuf[3] = obj_owner;
	multibuf[4] = remove_obj_counter++; 

	if(Netgame.RetroProtocol) {
		int plr_count = 0;
		for(int i = 0; i < MAX_PLAYERS; i++) {
			if(Players[i].connected == CONNECT_PLAYING ) {
				plr_count += 1; 
			}
		}
		
		// In a two player game, we can get both speed and packet loss prevention
		if(plr_count <= 2) {
			multi_send_data(multibuf, 5, 2); 
		} else {
			// Otherwise, send via both paths -- dup will be dropped
			multi_send_data(multibuf, 5, 1); 
			multi_send_data(multibuf, 5, 2); 
		}
	} else {
		multi_send_data(multibuf, 5, 2);
	}

	if (Network_send_objects && multi_objnum_is_past(objnum))
	{
		Network_send_objnum = -1;
	}
}

void
multi_send_quit(int why)
{
	// I am quitting the game, tell the other guy the bad news.
	if (is_observer() && !Netgame.host_is_obs) {
		net_udp_send_obs_quit();
		return;
	}

	Assert (why == MULTI_QUIT);

	multibuf[0] = (char)why;
	multibuf[1] = Player_num;
	multi_send_data(multibuf, 2, 2);

}

void
multi_send_cloak(void)
{
	// Broadcast a change in our pflags (made to support cloaking)
	if (is_observer()) { return; }

	multibuf[0] = MULTI_CLOAK;
	multibuf[1] = (char)Player_num;

	if(Netgame.RetroProtocol) {
		multi_send_data(multibuf, 2, 1);
	}
	multi_send_data(multibuf, 2, 2);

	if (Game_mode & GM_MULTI_ROBOTS)
		multi_strip_robots(Player_num);
}

void
multi_send_decloak(void)
{
	// Broadcast a change in our pflags (made to support cloaking)
	if (is_observer()) { return; }

	multibuf[0] = MULTI_DECLOAK;
	multibuf[1] = (char)Player_num;

	if(Netgame.RetroProtocol) {
		multi_send_data(multibuf, 2, 1);
	}
	multi_send_data(multibuf, 2, 2);
}

void
multi_send_invuln(void)
{
	// Broadcast a change in our pflags (made to support invuln)
	if (is_observer() || (Netgame.max_numobservers == 0 && !Netgame.host_is_obs)) { return; }

	multibuf[0] = MULTI_INVULN;
	multibuf[1] = (char)Player_num;

	if (Netgame.RetroProtocol) {
		multi_send_data(multibuf, 2, 1);
	}
	multi_send_data(multibuf, 2, 2);
}

void
multi_send_door_open(int segnum, int side,ubyte flag)
{
	// When we open a door make sure everyone else opens that door
	if (is_observer()) { return; }

	multibuf[0] = MULTI_DOOR_OPEN;
	PUT_INTEL_SHORT(multibuf+1, segnum );
	multibuf[3] = (sbyte)side;
	multibuf[4] = flag;

	if(Netgame.RetroProtocol) {
		multi_send_data(multibuf, 5, 1);
	} else {
		multi_send_data(multibuf, 5, 2);
	}
}

void multi_send_door_open_specific(int pnum,int segnum, int side,ubyte flag)
{
	// For sending doors only to a specific person (usually when they're joining)
	if (is_observer()) { return; }

	Assert (Game_mode & GM_NETWORK);
	//   Assert (pnum>-1 && pnum<N_players);

	multibuf[0] = MULTI_DOOR_OPEN;
	PUT_INTEL_SHORT(multibuf+1, segnum);
	multibuf[3] = (sbyte)side;
	multibuf[4] = flag;

	multi_send_data_direct((ubyte *)multibuf, 5, pnum, 2);
}

//
// Part 3 : Functions that change or prepare the game for multiplayer use.
//          Not including functions needed to syncronize or start the
//          particular type of multiplayer game.  Includes preparing the
//                      mines, player structures, etc.

void
multi_send_create_explosion(int pnum)
{
	// Send all data needed to create a remote explosion
	if (is_observer()) { return; }

	int count = 0;

	multibuf[count] = MULTI_CREATE_EXPLOSION;       count += 1;
	multibuf[count] = (sbyte)pnum;                  count += 1;
	//                                                                                                      -----------
	//                                                                                                      Total size = 2

	multi_send_data(multibuf, count, 0);
}

void
multi_send_controlcen_fire(vms_vector *to_goal, int best_gun_num, int objnum)
{
	if (is_observer()) { return; }

#ifdef WORDS_BIGENDIAN
	vms_vector swapped_vec;
#endif
	int count = 0;

	multibuf[count] = MULTI_CONTROLCEN_FIRE;                count +=  1;
#ifndef WORDS_BIGENDIAN
	memcpy(multibuf+count, to_goal, 12);                    count += 12;
#else
	swapped_vec.x = (fix)INTEL_INT( (int)to_goal->x );
	swapped_vec.y = (fix)INTEL_INT( (int)to_goal->y );
	swapped_vec.z = (fix)INTEL_INT( (int)to_goal->z );
	memcpy(multibuf+count, &swapped_vec, 12);				count += 12;
#endif
	multibuf[count] = (char)best_gun_num;                   count +=  1;
	PUT_INTEL_SHORT(multibuf+count, objnum );     count +=  2;
	//                                                                                                                      ------------
	//                                                                                                                      Total  = 16
	multi_send_data(multibuf, count, 0);
}

void
multi_send_create_powerup(int powerup_type, int segnum, int objnum, vms_vector *pos)
{
	if (is_observer()) { return; }

	// Create a powerup on a remote machine, used for remote
	// placement of used powerups like missiles and cloaking
	// powerups.

#ifdef WORDS_BIGENDIAN
	vms_vector swapped_vec;
#endif
	int count = 0;

	// CED -- this makes no sense
	//multi_send_position(Players[Player_num].objnum);

	if (Game_mode & GM_NETWORK)
	{
#ifdef OLDPOWCAP
		PowerupsInMine[powerup_type]++;
#else
		if (multi_powerup_is_4pack(powerup_type))
			PowerupsInMine[powerup_type-1]+=4;
		else
			PowerupsInMine[powerup_type]++;
#endif
	}

	multibuf[count] = MULTI_CREATE_POWERUP;         count += 1;
	multibuf[count] = Player_num;                                      count += 1;
	multibuf[count] = powerup_type;                                 count += 1;
	PUT_INTEL_SHORT(multibuf+count, segnum );     count += 2;
	PUT_INTEL_SHORT(multibuf+count, objnum );     count += 2;
#ifndef WORDS_BIGENDIAN
	memcpy(multibuf+count, pos, sizeof(vms_vector));  count += sizeof(vms_vector);
#else
	swapped_vec.x = (fix)INTEL_INT( (int)pos->x );
	swapped_vec.y = (fix)INTEL_INT( (int)pos->y );
	swapped_vec.z = (fix)INTEL_INT( (int)pos->z );
	memcpy(multibuf+count, &swapped_vec, 12);				count += 12;
#endif
	//                                                                                                            -----------
	//                                                                                                            Total =  19
	multi_send_data(multibuf, count, 2);

	if (Network_send_objects && multi_objnum_is_past(objnum))
	{
		Network_send_objnum = -1;
	}

	map_objnum_local_to_local(objnum);
}

void
multi_send_play_sound(int sound_num, fix volume)
{
	if (is_observer()) { return; }

	int count = 0;
	multibuf[count] = MULTI_PLAY_SOUND;                     count += 1;
	multibuf[count] = Player_num;                                   count += 1;
	multibuf[count] = (char)sound_num;                      count += 1;
	multibuf[count] = (char)(volume >> 12); count += 1;
	//                                                                                                         -----------
	//                                                                                                         Total = 4
	multi_send_data(multibuf, count, 0);
}

void
multi_send_audio_taunt(int taunt_num)
{
	return; // Taken out, awaiting sounds..

#if 0
	int audio_taunts[4] = {
		SOUND_CONTROL_CENTER_WARNING_SIREN,
		SOUND_HOSTAGE_RESCUED,
		SOUND_REFUEL_STATION_GIVING_FUEL,
		SOUND_BAD_SELECTION
	};


	Assert(taunt_num >= 0);
	Assert(taunt_num < 4);

	digi_play_sample( audio_taunts[taunt_num], F1_0 );
	multi_send_play_sound(audio_taunts[taunt_num], F1_0);
#endif
}

void
multi_send_score(void)
{
	if (is_observer()) { return; }

	// Send my current score to all other players so it will remain
	// synced.
	int count = 0;

	if ((Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS)) {
		multi_sort_kill_list();
		multibuf[count] = MULTI_SCORE;                  count += 1;
		multibuf[count] = Player_num;                           count += 1;
		PUT_INTEL_INT(multibuf+count, Players[Player_num].score);  count += 4;
		multi_send_data(multibuf, count, 0);
	}
}

void
multi_send_trigger(int triggernum)
{
	// Send an even to trigger something in the mine
	if (is_observer()) { return; }

	int count = 0;

	multibuf[count] = MULTI_TRIGGER;                                count += 1;
	multibuf[count] = Player_num;                                   count += 1;
	multibuf[count] = (ubyte)triggernum;            count += 1;

	multi_send_data(multibuf, count, 2);
}

void
multi_send_hostage_door_status(int wallnum)
{
	// Tell the other player what the hit point status of a hostage door
	// should be

	if (is_observer()) { return; }

	int count = 0;

	Assert(Walls[wallnum].type == WALL_BLASTABLE);

	multibuf[count] = MULTI_HOSTAGE_DOOR;           count += 1;
	PUT_INTEL_SHORT(multibuf+count, wallnum );           count += 2;
	PUT_INTEL_INT(multibuf+count, Walls[wallnum].hps );  count += 4;

	multi_send_data(multibuf, count, 0);
}

extern int Drop_afterburner_blob_flag;
int PhallicLimit=0;
int PhallicMan=-1;

void multi_consistency_error(int reset)
{
	static int count = 0;

	if (reset)
		count = 0;

	if (++count < 10)
		return;

	if (Game_wind)
		window_set_visible(Game_wind, 0);
	nm_messagebox(NULL, 1, TXT_OK, TXT_CONSISTENCY_ERROR);
	if (Game_wind)
		window_set_visible(Game_wind, 1);
	count = 0;
	multi_quit_game = 1;
	game_leave_menus();
	multi_reset_stuff();
}

int is_dupable_primary(int id) {
	switch(id) {
		case POW_LASER:
		case POW_QUAD_FIRE:
		case POW_VULCAN_WEAPON: 
		case POW_VULCAN_AMMO: 
		case POW_SPREADFIRE_WEAPON: 
		case POW_PLASMA_WEAPON:
		case POW_FUSION_WEAPON:

		case POW_SUPER_LASER:
		case POW_GAUSS_WEAPON:
		case POW_HELIX_WEAPON:
		case POW_PHOENIX_WEAPON: 
		case POW_OMEGA_WEAPON: 

		case POW_AFTERBURNER: 
			return 1;
	}

	return 0; 

}

int is_dupable_secondary(int id) {
	switch(id) {
		case POW_MISSILE_1:
		case POW_MISSILE_4:
		case POW_HOMING_AMMO_1:
		case POW_HOMING_AMMO_4:
		case POW_PROXIMITY_WEAPON:
		case POW_SMARTBOMB_WEAPON:
		case POW_MEGA_WEAPON: 

		case POW_SMISSILE1_1:
		case POW_SMISSILE1_4:
		case POW_GUIDED_MISSILE_1:
		case POW_GUIDED_MISSILE_4:
		case POW_SMART_MINE:
		case POW_MERCURY_MISSILE_1:
		case POW_MERCURY_MISSILE_4:
		case POW_EARTHSHAKER_MISSILE:
			return 1;
	}
	return 0;
}

int multi_received_objects = 0; 

ubyte original_object_types[MAX_OBJECTS];
void save_original_objects() {
	for (int i=0; i<MAX_OBJECTS; i++) {
		original_object_types[i] = Objects[i].type; 
	}
}

int was_original_object(int i) {
	if(original_object_types[i] != OBJ_NONE) {
		return 1;
	}

	return 0; 
}

void multi_prep_level(void)
{
	// Do any special stuff to the level required for games
	// before we begin playing in it.

	// Player_num MUST be set before calling this procedure.

	// This function must be called before checksuming the Object array,
	// since the resulting checksum with depend on the value of Player_num
	// at the time this is called.

	int i;
	int     cloak_count, inv_count;

	Show_graph_until = -1;

	Assert(Game_mode & GM_MULTI);

	Assert(NumNetPlayerPositions > 0);

	PhallicLimit=0;
	PhallicMan=-1;
	Drop_afterburner_blob_flag=0;
	Bounty_target = 0;

	multi_consistency_error(1);

	for (i=0;i<MAX_PLAYERS;i++)
	{
		PKilledFlags[i]=1;
		multi_sending_message[i] = 0;
	}

	for (i = 0; i < NumNetPlayerPositions; i++)
	{
		if (i != Player_num)
			Objects[Players[i].objnum].control_type = CT_REMOTE;
		Objects[Players[i].objnum].movement_type = MT_PHYSICS;
		multi_reset_player_object(&Objects[Players[i].objnum]);
		Netgame.players[i].LastPacketTime = 0;
	}

	for (i = 0; i < MAX_ROBOTS_CONTROLLED; i++)
	{
		robot_controlled[i] = -1;
		robot_agitation[i] = 0;
		robot_fired[i] = 0;
	}

	Viewer = ConsoleObject = &Objects[Players[Player_num].objnum];

	if (!(Game_mode & GM_MULTI_COOP))
	{
		multi_delete_extra_objects(); // Removes monsters from level
	}

	if (Game_mode & GM_MULTI_ROBOTS)
	{
		multi_set_robot_ai(); // Set all Robot AI to types we can cope with
	}

	if (Game_mode & GM_NETWORK)
	{
		multi_powcap_adjust_cap_for_player(Player_num);
		multi_send_powcap_update();
	}

	inv_count = 0;
	cloak_count = 0;
	for (i=0; i<=Highest_object_index; i++)
	{
		int objnum;

		if ((Objects[i].type == OBJ_HOSTAGE) && !(Game_mode & GM_MULTI_COOP))
		{
			objnum = obj_create(OBJ_POWERUP, POW_SHIELD_BOOST, Objects[i].segnum, &Objects[i].pos, &vmd_identity_matrix, Powerup_info[POW_SHIELD_BOOST].size, CT_POWERUP, MT_PHYSICS, RT_POWERUP);
			obj_delete(i);
			if (objnum != -1)
			{
				Objects[objnum].rtype.vclip_info.vclip_num = Powerup_info[POW_SHIELD_BOOST].vclip_num;
				Objects[objnum].rtype.vclip_info.frametime = Vclip[Objects[objnum].rtype.vclip_info.vclip_num].frame_time;
				Objects[objnum].rtype.vclip_info.framenum = 0;
				Objects[objnum].mtype.phys_info.drag = 512;     //1024;
				Objects[objnum].mtype.phys_info.mass = F1_0;
				vm_vec_zero(&Objects[objnum].mtype.phys_info.velocity);
			}
			continue;
		}

		if (Objects[i].type == OBJ_POWERUP)
		{
			if (Objects[i].id == POW_EXTRA_LIFE)
			{
				if (!(Netgame.AllowedItems & NETFLAG_DOINVUL))
				{
					Objects[i].id = POW_SHIELD_BOOST;
					Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
					Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;
				}
				else
				{
					Objects[i].id = POW_INVULNERABILITY;
					Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
					Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;
				}

			}

			if (!(Game_mode & GM_MULTI_COOP))
				if ((Objects[i].id >= POW_KEY_BLUE) && (Objects[i].id <= POW_KEY_GOLD))
				{
					Objects[i].id = POW_SHIELD_BOOST;
					Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
					Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;
				}

			if (Objects[i].id == POW_INVULNERABILITY) {
				if (inv_count >= 3 || (!(Netgame.AllowedItems & NETFLAG_DOINVUL))) {
					Objects[i].id = POW_SHIELD_BOOST;
					Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
					Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;
				} else
					inv_count++;
			}

			if (Objects[i].id == POW_CLOAK) {
				if (cloak_count >= 3 || (!(Netgame.AllowedItems & NETFLAG_DOCLOAK))) {
					Objects[i].id = POW_SHIELD_BOOST;
					Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
					Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;
				} else
					cloak_count++;
			}

			if (Objects[i].id == POW_AFTERBURNER && !(Netgame.AllowedItems & NETFLAG_DOAFTERBURNER))
				bash_to_shield (i,"afterburner");

			if (Objects[i].id == POW_AFTERBURNER && Netgame.BornWithBurner)
				bash_to_shield (i,"afterburner");

			if (Objects[i].id == POW_FUSION_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOFUSION))
				bash_to_shield (i,"fusion");
			if (Objects[i].id == POW_PHOENIX_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOPHOENIX))
				bash_to_shield (i,"phoenix");

			if (Objects[i].id == POW_HELIX_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOHELIX))
				bash_to_shield (i,"helix");

			if (Objects[i].id == POW_MEGA_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOMEGA))
				bash_to_shield (i,"mega");

			if (Objects[i].id == POW_SMARTBOMB_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOSMART))
				bash_to_shield (i,"smartmissile");

			if (Objects[i].id == POW_GAUSS_WEAPON && Netgame.LowVulcan)
				Objects[i].ctype.powerup_info.count = VULCAN_WEAPON_AMMO_AMOUNT/2; //1250

			if (Objects[i].id == POW_GAUSS_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOGAUSS))
				bash_to_shield (i,"gauss");

			if (Objects[i].id == POW_VULCAN_WEAPON && Netgame.LowVulcan)
				Objects[i].ctype.powerup_info.count = VULCAN_WEAPON_AMMO_AMOUNT/2; //1250

			if (Objects[i].id == POW_VULCAN_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOVULCAN))
				bash_to_shield (i,"vulcan");

			if (Objects[i].id == POW_PLASMA_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOPLASMA))
				bash_to_shield (i,"plasma");

			if (Objects[i].id == POW_OMEGA_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOOMEGA))
				bash_to_shield (i,"omega");

			if (Objects[i].id == POW_SUPER_LASER && !(Netgame.AllowedItems & NETFLAG_DOSUPERLASER))
				bash_to_shield (i,"superlaser");

			if (Objects[i].id == POW_PROXIMITY_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOPROXIM))
				bash_to_shield (i,"proximity");

			// Special: Make all proximity bombs into shields if in
			// hoard mode because we use the proximity slot in the
			// player struct to signify how many orbs the player has.

			if (Objects[i].id == POW_PROXIMITY_WEAPON && (Game_mode & GM_HOARD))
				bash_to_shield (i,"proximity");

			if (Objects[i].id==POW_VULCAN_AMMO && 
				( (!(Netgame.AllowedItems & NETFLAG_DOVULCANAMMO)) ||
				  Netgame.LowVulcan 
				)
			   )	
				bash_to_shield(i,"vulcan ammo");

			if (Objects[i].id == POW_SPREADFIRE_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOSPREAD))
				bash_to_shield (i,"spread");
			if (Objects[i].id == POW_SMART_MINE && !(Netgame.AllowedItems & NETFLAG_DOSMARTMINE))
				bash_to_shield (i,"smartmine");
			if (Objects[i].id == POW_SMISSILE1_1 && !(Netgame.AllowedItems & NETFLAG_DOFLASH))
				bash_to_shield (i,"flash");
			if (Objects[i].id == POW_SMISSILE1_4 && !(Netgame.AllowedItems & NETFLAG_DOFLASH))
				bash_to_shield (i,"flash");
			if (Objects[i].id == POW_GUIDED_MISSILE_1 && !(Netgame.AllowedItems & NETFLAG_DOGUIDED))
				bash_to_shield (i,"guided");
			if (Objects[i].id == POW_GUIDED_MISSILE_4 && !(Netgame.AllowedItems & NETFLAG_DOGUIDED))
				bash_to_shield (i,"guided");
			if (Objects[i].id == POW_EARTHSHAKER_MISSILE && !(Netgame.AllowedItems & NETFLAG_DOSHAKER))
				bash_to_shield (i,"earth");
			if (Objects[i].id == POW_MERCURY_MISSILE_1 && !(Netgame.AllowedItems & NETFLAG_DOMERCURY))
				bash_to_shield (i,"Mercury");
			if (Objects[i].id == POW_MERCURY_MISSILE_4 && !(Netgame.AllowedItems & NETFLAG_DOMERCURY))
				bash_to_shield (i,"Mercury");
			if (Objects[i].id == POW_CONVERTER && !(Netgame.AllowedItems & NETFLAG_DOCONVERTER))
				bash_to_shield (i,"Converter");
			if (Objects[i].id == POW_AMMO_RACK && !(Netgame.AllowedItems & NETFLAG_DOAMMORACK))
				bash_to_shield (i,"Ammo rack");
			if (Objects[i].id == POW_HEADLIGHT && !(Netgame.AllowedItems & NETFLAG_DOHEADLIGHT))
				bash_to_shield (i,"Headlight");
			if (Objects[i].id == POW_LASER && !(Netgame.AllowedItems & NETFLAG_DOLASER))
				bash_to_shield (i,"Laser powerup");
			if (Objects[i].id == POW_HOMING_AMMO_1 && !(Netgame.AllowedItems & NETFLAG_DOHOMING))
				bash_to_shield (i,"Homing");
			if (Objects[i].id == POW_HOMING_AMMO_4 && !(Netgame.AllowedItems & NETFLAG_DOHOMING))
				bash_to_shield (i,"Homing");
			if (Objects[i].id == POW_QUAD_FIRE && !(Netgame.AllowedItems & NETFLAG_DOQUAD))
				bash_to_shield (i,"Quad Lasers");
			if (Objects[i].id == POW_FLAG_BLUE && !(Game_mode & GM_CAPTURE))
				bash_to_shield (i,"Blue flag");
			if (Objects[i].id == POW_FLAG_RED && !(Game_mode & GM_CAPTURE))
				bash_to_shield (i,"Red flag");
		}
	}

	if(! multi_received_objects ) {
		int old_highest_object = Highest_object_index;
		save_original_objects(); 


		for (i=0; i<=old_highest_object; i++)
		{
			if(! was_original_object(i)) { continue; }
			int objnum;

			if ((Objects[i].type == OBJ_HOSTAGE) && !(Game_mode & GM_MULTI_COOP))
			{
				objnum = obj_create(OBJ_POWERUP, POW_SHIELD_BOOST, Objects[i].segnum, &Objects[i].pos, &vmd_identity_matrix, Powerup_info[POW_SHIELD_BOOST].size, CT_POWERUP, MT_PHYSICS, RT_POWERUP);
				obj_delete(i);
				if (objnum != -1)
				{
					Objects[objnum].rtype.vclip_info.vclip_num = Powerup_info[POW_SHIELD_BOOST].vclip_num;
					Objects[objnum].rtype.vclip_info.frametime = Vclip[Objects[objnum].rtype.vclip_info.vclip_num].frame_time;
					Objects[objnum].rtype.vclip_info.framenum = 0;
					Objects[objnum].mtype.phys_info.drag = 512;     //1024;
					Objects[objnum].mtype.phys_info.mass = F1_0;
					vm_vec_zero(&Objects[objnum].mtype.phys_info.velocity);
				}
				continue;
			}

			if (Objects[i].type == OBJ_POWERUP)
			{
				if(Netgame.PrimaryDupFactor > 1) {
					if(is_dupable_primary(Objects[i].id)) {
						for(int dup = 0; dup < Netgame.PrimaryDupFactor - 1; dup++) {
							objnum = obj_create(OBJ_POWERUP, Objects[i].id, Objects[i].segnum, &Objects[i].pos, &vmd_identity_matrix, Powerup_info[Objects[i].id].size, CT_POWERUP, MT_PHYSICS, RT_POWERUP);
							if (objnum != -1)
							{
								Objects[objnum].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
								Objects[objnum].rtype.vclip_info.frametime = Vclip[Objects[objnum].rtype.vclip_info.vclip_num].frame_time;
								Objects[objnum].rtype.vclip_info.framenum = 0;
								Objects[objnum].mtype.phys_info.drag = 512;     //1024;
								Objects[objnum].mtype.phys_info.mass = F1_0;
								Objects[objnum].ctype.powerup_info.count = Objects[i].ctype.powerup_info.count;
								vm_vec_zero(&Objects[objnum].mtype.phys_info.velocity);
							}
						}
					
					}
				}

				if(Netgame.SecondaryDupFactor > 1) {
					if(is_dupable_secondary(Objects[i].id)) {					
						for(int dup = 0; dup < Netgame.SecondaryDupFactor - 1; dup++) {
							objnum = obj_create(OBJ_POWERUP, Objects[i].id, Objects[i].segnum, &Objects[i].pos, &vmd_identity_matrix, Powerup_info[Objects[i].id].size, CT_POWERUP, MT_PHYSICS, RT_POWERUP);

							if (objnum != -1)
							{
								Objects[objnum].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
								Objects[objnum].rtype.vclip_info.frametime = Vclip[Objects[objnum].rtype.vclip_info.vclip_num].frame_time;
								Objects[objnum].rtype.vclip_info.framenum = 0;
								Objects[objnum].mtype.phys_info.drag = 512;     //1024;
								Objects[objnum].mtype.phys_info.mass = F1_0;
								vm_vec_zero(&Objects[objnum].mtype.phys_info.velocity);
							}
						}
					
					}
				}			
				
			}
		}

		if(Netgame.SecondaryCapFactor > 0) {
			int max_homers = Netgame.SecondaryCapFactor == 1 ? 6 : 2; 
			int max_smarts = Netgame.SecondaryCapFactor == 1 ? 6 : 2; 
			int max_flashes = max_homers;
			int max_guideds = max_homers;
			int max_mercs = max_homers;

			int num_homers = 0;
			int num_smarts = 0; 
			int num_flashes = 0;
			int num_guideds = 0;
			int num_mercs = 0; 
			for (i=0; i<=Highest_object_index; i++)
			{
				if(Objects[i].id == POW_HOMING_AMMO_1) {
					if(num_homers < max_homers) {
						num_homers++;
					} else {
						bash_to_shield (i,"Homing");
					}
				}

				if(Objects[i].id == POW_HOMING_AMMO_4) {
					if(num_homers + 4 <= max_homers) {
						num_homers += 4;
					} else if(num_homers + 1 <= max_homers) {
						Objects[i].id = POW_HOMING_AMMO_1;
						Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
						Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;

						num_homers += 1; 
					} else {
						bash_to_shield (i,"Homing");
					}
				}

				if(Objects[i].id == POW_SMISSILE1_1) {
					if(num_flashes < max_flashes) {
						num_flashes++;
					} else {
						bash_to_shield (i,"Flash");
					}
				}

				if(Objects[i].id == POW_SMISSILE1_4) {
					if(num_flashes + 4 <= max_flashes) {
						num_flashes += 4;
					} else if(num_flashes + 1 <= max_flashes) {
						Objects[i].id = POW_SMISSILE1_1;
						Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
						Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;

						num_flashes += 1; 
					} else {
						bash_to_shield (i,"Flash");
					}
				}	

				if(Objects[i].id == POW_GUIDED_MISSILE_1) {
					if(num_guideds < max_guideds) {
						num_guideds++;
					} else {
						bash_to_shield (i,"Guided");
					}
				}

				if(Objects[i].id == POW_GUIDED_MISSILE_4) {
					if(num_guideds + 4 <= max_guideds) {
						num_guideds += 4;
					} else if(num_guideds + 1 <= max_guideds) {
						Objects[i].id = POW_GUIDED_MISSILE_1;
						Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
						Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;

						num_guideds += 1; 
					} else {
						bash_to_shield (i,"Guided");
					}
				}		

				if(Objects[i].id == POW_MERCURY_MISSILE_1) {
					if(num_mercs < max_mercs) {
						num_guideds++;
					} else {
						bash_to_shield (i,"Mercury");
					}
				}

				if(Objects[i].id == POW_MERCURY_MISSILE_4) {
					if(num_mercs + 4 <= max_mercs) {
						num_mercs += 4;
					} else if(num_mercs + 1 <= max_mercs) {
						Objects[i].id = POW_MERCURY_MISSILE_1;
						Objects[i].rtype.vclip_info.vclip_num = Powerup_info[Objects[i].id].vclip_num;
						Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;

						num_mercs += 1; 
					} else {
						bash_to_shield (i,"Mercury");
					}
				}											

				if(Objects[i].id == POW_SMARTBOMB_WEAPON) {
					if(num_smarts < max_smarts) {
						num_smarts++;
					} else {
						bash_to_shield (i,"smartmissile");
					}
				}
			}
		}
	}

	if (Game_mode & GM_HOARD)
		init_hoard_data();

	if ((Game_mode & GM_CAPTURE) || (Game_mode & GM_HOARD))
		multi_apply_goal_textures();

	multi_sort_kill_list();

	multi_show_player_list();

	ConsoleObject->control_type = CT_FLYING;

	reset_player_object();

	imulti_new_game=0;
}

int multi_level_sync(void)
{
	switch (multi_protocol)
	{
#ifdef USE_UDP
		case MULTI_PROTO_UDP:
			return net_udp_level_sync();
			break;
#endif
		default:
			Error("Protocol handling missing in multi_level_sync\n");
			break;
	}
}

int Goal_blue_segnum,Goal_red_segnum;

void multi_apply_goal_textures()
{
	int		i,j,tex;
	segment	*seg;
	segment2	*seg2;

	for (i=0; i <= Highest_segment_index; i++)
	{
		seg = &Segments[i];
		seg2 = &Segment2s[i];

		if (seg2->special==SEGMENT_IS_GOAL_BLUE)
		{

			Goal_blue_segnum = i;

			if (Game_mode & GM_HOARD)
				tex=find_goal_texture (TMI_GOAL_HOARD);
			else
				tex=find_goal_texture (TMI_GOAL_BLUE);

			if (tex>-1)
				for (j = 0; j < 6; j++) {
					int v;
					seg->sides[j].tmap_num=tex;
					for (v=0;v<4;v++)
						seg->sides[j].uvls[v].l = i2f(100);		//max out
				}

			seg2->static_light = i2f(100);	//make static light bright

		}

		if (seg2->special==SEGMENT_IS_GOAL_RED)
		{
			Goal_red_segnum = i;

			// Make both textures the same if Hoard mode

			if (Game_mode & GM_HOARD)
				tex=find_goal_texture (TMI_GOAL_HOARD);
			else
				tex=find_goal_texture (TMI_GOAL_RED);

			if (tex>-1)
				for (j = 0; j < 6; j++) {
					int v;
					seg->sides[j].tmap_num=tex;
					for (v=0;v<4;v++)
						seg->sides[j].uvls[v].l = i2f(1000);		//max out
				}

			seg2->static_light = i2f(100);	//make static light bright
		}
	}
}
int find_goal_texture (ubyte t)
{
	int i;

	for (i=0;i<NumTextures;i++)
		if (TmapInfo[i].flags & t)
			return i;

	Int3(); // Hey, there is no goal texture for this PIG!!!!
	// Edit bitmaps.tbl and designate two textures to be RED and BLUE
	// goal textures
	return (-1);
}


void multi_set_robot_ai(void)
{
	// Go through the objects array looking for robots and setting
	// them to certain supported types of NET AI behavior.

	//      int i;
	//
	//      for (i = 0; i <= Highest_object_index; i++)
	//      {
	//              if (Objects[i].type == OBJ_ROBOT) {
	//                      Objects[i].ai_info.REMOTE_OWNER = -1;
	//                      if (Objects[i].ai_info.behavior == AIB_STATION)
	//                              Objects[i].ai_info.behavior = AIB_NORMAL;
	//              }
	//      }
}

int multi_delete_extra_objects()
{
	int i;
	int nnp=0;
	object *objp;

	// Go through the object list and remove any objects not used in
	// 'Anarchy!' games.

	// This function also prints the total number of available multiplayer
	// positions in this level, even though this should always be 8 or more!

	objp = Objects;
	for (i=0;i<=Highest_object_index;i++) {
		if ((objp->type==OBJ_PLAYER) || (objp->type==OBJ_GHOST))
			nnp++;
		else if ((objp->type==OBJ_ROBOT) && (Game_mode & GM_MULTI_ROBOTS))
			;
		else if ( (objp->type!=OBJ_NONE) && (objp->type!=OBJ_PLAYER) && (objp->type!=OBJ_POWERUP) && (objp->type!=OBJ_CNTRLCEN) && (objp->type!=OBJ_HOSTAGE) && !(objp->type==OBJ_WEAPON && objp->id==PMINE_ID) ) {
			// Before deleting object, if it's a robot, drop it's special powerup, if any
			if (objp->type == OBJ_ROBOT)
				if (objp->contains_count && (objp->contains_type == OBJ_POWERUP))
					object_create_egg(objp);
			obj_delete(i);
		}
		objp++;
	}

	return nnp;
}

// Returns 1 if player is Master/Host of this game
int multi_i_am_master(void)
{
	return (Player_num == 0);
}

// Returns the Player_num of Master/Host of this game
int multi_who_is_master(void)
{
	return 0;
}

void change_playernum_to( int new_Player_num )
{
// 	if (Player_num > -1)
// 		memcpy( Players[new_Player_num].callsign, Players[Player_num].callsign, CALLSIGN_LEN+1 );
	if (Player_num > -1)
	{
		char *buf;
		MALLOC(buf,char,CALLSIGN_LEN+1);
		memcpy( buf, Players[Player_num].callsign, CALLSIGN_LEN+1 );
		strcpy(Players[new_Player_num].callsign,buf);
		d_free(buf);
	}

	Player_num = new_Player_num;
}

int multi_all_players_alive()
{
	int i;
	for (i=(Netgame.host_is_obs ? 1 : 0);i<N_players;i++)
	{
		if (PKilledFlags[i] && Players[i].connected)
			return (0);
	}
	return (1);
}

void multi_send_drop_weapon (int objnum,int seed)
{
	if (is_observer()) { return; }

	object *objp;
	int count=0;
	int ammo_count;

	multi_send_position(Players[Player_num].objnum);

	objp = &Objects[objnum];

	ammo_count = objp->ctype.powerup_info.count;

	if (objp->id == POW_OMEGA_WEAPON && ammo_count == F1_0)
		ammo_count = F1_0 - 1; //make fit in short

	Assert(ammo_count < F1_0); //make sure fits in short

	multibuf[count++]=(char)MULTI_DROP_WEAPON;
	multibuf[count++]=(char)objp->id;

	PUT_INTEL_SHORT(multibuf+count, Player_num); count += 2;
	PUT_INTEL_SHORT(multibuf+count, objnum); count += 2;
	PUT_INTEL_SHORT(multibuf+count, ammo_count); count += 2;
	PUT_INTEL_INT(multibuf+count, seed);

	map_objnum_local_to_local(objnum);

	if (Game_mode & GM_NETWORK)
	{
#ifdef OLDPOWCAP
		PowerupsInMine[objp->id]++;
#else
		if (multi_powerup_is_4pack(objp->id))
			PowerupsInMine[objp->id-1]+=4;
		else
			PowerupsInMine[objp->id]++;
#endif
	}

	multi_send_data(multibuf, 12, 2);
}

void multi_do_drop_weapon (const ubyte *buf)
{
	int pnum,ammo,objnum,remote_objnum,seed;
	object *objp;
	int powerup_id;

	powerup_id=(int)(buf[1]);
	pnum = GET_INTEL_SHORT(buf + 2);
	remote_objnum = GET_INTEL_SHORT(buf + 4);
	ammo = GET_INTEL_SHORT(buf + 6);
	seed = GET_INTEL_INT(buf + 8);

	objp = &Objects[Players[pnum].objnum];

	objnum = spit_powerup(objp, powerup_id, seed);

	map_objnum_local_to_remote(objnum, remote_objnum, pnum);

	if (objnum!=-1)
		Objects[objnum].ctype.powerup_info.count = ammo;

	if (Game_mode & GM_NETWORK)
	{
#ifdef OLDPOWCAP
		PowerupsInMine[powerup_id]++;
#else
		if (multi_powerup_is_4pack(powerup_id))
			PowerupsInMine[powerup_id-1]+=4;
		else
			PowerupsInMine[powerup_id]++;
#endif
	}

}

void multi_send_guided_info (object *miss,char done)
{
	if (is_observer()) { return; }

#ifdef WORDS_BIGENDIAN
	shortpos sp;
#endif
	int count=0;

	multibuf[count++]=(char)MULTI_GUIDED;
	multibuf[count++]=(char)Player_num;
	multibuf[count++]=done;

#ifndef WORDS_BIGENDIAN
	create_shortpos((shortpos *)(multibuf+count), miss,0);
	count+=sizeof(shortpos);
#else
	create_shortpos(&sp, miss, 1);
	memcpy(&(multibuf[count]), (ubyte *)(sp.bytemat), 9);
	count += 9;
	memcpy(&(multibuf[count]), (ubyte *)&(sp.xo), 14);
	count += 14;
#endif

	multi_send_data(multibuf, count, 0);
}

void multi_do_guided (const ubyte *buf)
{
	char pnum=buf[1];
	int count=3;
	static int fun=200;
#ifdef WORDS_BIGENDIAN
	shortpos sp;
#endif

	if (Guided_missile[(int)pnum]==NULL)
	{
		if (++fun>=50)
		{
			fun=0;
		}
		return;
	}
	else if (++fun>=50)
	{
		fun=0;
	}

	if (buf[2])
	{
		release_guided_missile(pnum);
		return;
	}


	if (Guided_missile[(int)pnum]-Objects<0 || Guided_missile[(int)pnum]-Objects > Highest_object_index)
	{
		Int3();  // Get Jason immediately!
		return;
	}

#ifndef WORDS_BIGENDIAN
	extract_shortpos(Guided_missile[(int)pnum], (shortpos *)(buf+count),0);
#else
	memcpy((ubyte *)(sp.bytemat), (ubyte *)(buf + count), 9);
	memcpy((ubyte *)&(sp.xo), (ubyte *)(buf + count + 9), 14);
	extract_shortpos(Guided_missile[(int)pnum], &sp, 1);
#endif

	count+=sizeof (shortpos);

	update_object_seg(Guided_missile[(int)pnum]);
}

void multi_send_stolen_items ()
{
	if (is_observer()) { return; }

	int i,count=1;
	multibuf[0]=MULTI_STOLEN_ITEMS;

	for (i=0;i<MAX_STOLEN_ITEMS;i++)
	{
		multibuf[i+1]=Stolen_items[i];
		count++;      // So I like to break my stuff into smaller chunks, so what?
	}
	multi_send_data(multibuf, count, 2);
}

void multi_do_stolen_items (const ubyte *buf)
{
	int i;

	for (i=0;i<MAX_STOLEN_ITEMS;i++)
	{
		Stolen_items[i]=buf[i+1];
	}
}

void multi_send_wall_status (int wallnum,ubyte type,ubyte flags,ubyte state)
{
	if (is_observer()) { return; }

	int count=0;
	multibuf[count]=MULTI_WALL_STATUS;        count++;
	PUT_INTEL_SHORT(multibuf+count, wallnum);   count+=2;
	multibuf[count]=type;                 count++;
	multibuf[count]=flags;                count++;
	multibuf[count]=state;                count++;

	multi_send_data(multibuf, count, 2);
}

void multi_send_wall_status_specific (int pnum,int wallnum,ubyte type,ubyte flags,ubyte state)
{
	// Send wall states a specific rejoining player
	if (is_observer()) { return; }

	int count=0;

	Assert (Game_mode & GM_NETWORK);
	//Assert (pnum>-1 && pnum<N_players);

	multibuf[count]=MULTI_WALL_STATUS;        count++;
	PUT_INTEL_SHORT(multibuf+count, wallnum);  count+=2;
	multibuf[count]=type;                 count++;
	multibuf[count]=flags;                count++;
	multibuf[count]=state;                count++;

	multi_send_data_direct(multibuf, count, pnum, 2);
}

void multi_do_wall_status (const ubyte *buf)
{
	short wallnum;
	ubyte flag,type,state;

	wallnum = GET_INTEL_SHORT(buf + 1);
	type=buf[3];
	flag=buf[4];
	state=buf[5];

	Assert (wallnum>=0);
	Walls[wallnum].type=type;
	Walls[wallnum].flags=flag;
	//Assert(state <= 4);
	Walls[wallnum].state=state;

	if (Walls[wallnum].type==WALL_OPEN)
	{
		digi_kill_sound_linked_to_segment(Walls[wallnum].segnum,Walls[wallnum].sidenum,SOUND_FORCEFIELD_HUM);
		//digi_kill_sound_linked_to_segment(csegp-Segments,cside,SOUND_FORCEFIELD_HUM);
	}
}

void multi_send_kill_goal_counts()
{
	int i,count=1;
	multibuf[0]=MULTI_KILLGOALS;

	for (i=0;i<MAX_PLAYERS;i++)
	{
		*(char *)(multibuf+count)=(char)Players[i].KillGoalCount;
		count++;
	}

	for(i=0; i < 2; i++) {
		*(char *)(multibuf+count)=(char)Netgame.TeamKillGoalCount[i];
		count++;
	}	

	multi_send_data(multibuf, count, 2);
}

void multi_do_kill_goal_counts(const ubyte *buf)
{
	int i,count=1;

	for (i=0;i<MAX_PLAYERS;i++)
	{
		Players[i].KillGoalCount=*(char *)(buf+count);
		count++;
	}

	for (i=0;i<2;i++)
	{
		Netgame.TeamKillGoalCount[i]=*(char *)(buf+count);
		count++;
	}	

}

void multi_send_heartbeat ()
{
	if (!Netgame.PlayTimeAllowed)
		return;

	multibuf[0]=MULTI_HEARTBEAT;
	PUT_INTEL_INT(multibuf+1, ThisLevelTime);
	multi_send_data(multibuf, 5, 0);
}

void multi_do_heartbeat (const ubyte *buf)
{
	fix num;

	num = GET_INTEL_INT(buf + 1);

	ThisLevelTime=num;
}

void multi_check_for_killgoal_winner ()
{
	int i,best=0,bestnum=0;
	object *objp;

	if (Control_center_destroyed)
		return;

	if (Game_mode & GM_TEAM)
	{
		int winner = 0;
		if(Netgame.TeamKillGoalCount[1] > Netgame.TeamKillGoalCount[0]) {
			winner = 1; 
		}

		HUD_init_message(HM_MULTI, "The winner is %s, with the most kills!",Netgame.team_name[winner]);

	} else {
		for (i=(Netgame.host_is_obs ? 1 : 0);i<N_players;i++)
		{
			if (Players[i].KillGoalCount>best)
			{
				best=Players[i].KillGoalCount;
				bestnum=i;
			}
		}

		if (bestnum==Player_num)
		{
			HUD_init_message(HM_MULTI, "You have the best score at %d kills!",best);
			//Players[Player_num].shields=i2f(200);
		}
		else
			HUD_init_message(HM_MULTI, "%s has the best score with %d kills!",Players[bestnum].callsign,best);
	}

	HUD_init_message_literal(HM_MULTI, "The control center has been destroyed!");

	objp=obj_find_first_of_type (OBJ_CNTRLCEN);
	net_destroy_controlcen (objp);
}

extern fix64 Seismic_disturbance_start_time;
extern fix64 Seismic_disturbance_end_time;

// Sync our seismic time with other players
void multi_send_seismic (fix64 t1,fix64 t2)
{
	if (is_observer()) { return; }

	int count=1;

	multibuf[0]=MULTI_SEISMIC;
	PUT_INTEL_INT(multibuf+count, t1); count+=(sizeof(fix));
	PUT_INTEL_INT(multibuf+count, t2); count+=(sizeof(fix));
	multi_send_data(multibuf, count, 2);
}

void multi_do_seismic (const ubyte *buf)
{
	fix duration = GET_INTEL_INT(buf + 5);
	Seismic_disturbance_start_time = GameTime64;
	Seismic_disturbance_end_time = GameTime64 + duration;
	digi_play_sample (SOUND_SEISMIC_DISTURBANCE_START, F1_0);
}

void multi_send_light (int segnum,ubyte val)
{
	if (is_observer()) { return; }

	int count=1,i;
	multibuf[0]=MULTI_LIGHT;
	PUT_INTEL_INT(multibuf+count, segnum); count+=(sizeof(int));
	*(char *)(multibuf+count)=val; count++;
	for (i=0;i<6;i++)
	{
		PUT_INTEL_SHORT(multibuf+count, Segments[segnum].sides[i].tmap_num2); count+=2;
	}
	multi_send_data(multibuf, count, 2);
}
void multi_send_light_specific (int pnum,int segnum,ubyte val)
{
	if (is_observer()) { return; }

	int count=1,i;

	Assert (Game_mode & GM_NETWORK);
	//  Assert (pnum>-1 && pnum<N_players);

	multibuf[0]=MULTI_LIGHT;
	PUT_INTEL_INT(multibuf+count, segnum); count+=(sizeof(int));
	*(char *)(multibuf+count)=val; count++;

	for (i=0;i<6;i++)
	{
		PUT_INTEL_SHORT(multibuf+count, Segments[segnum].sides[i].tmap_num2); count+=2;
	}

	multi_send_data_direct((ubyte *)multibuf, count, pnum, 2);
}

void multi_do_light (const ubyte *buf)
{
	int i, seg;
	ubyte sides=*(char *)(buf+5);

	seg = GET_INTEL_INT(buf + 1);
	for (i=0;i<6;i++)
	{
		if ((sides & (1<<i)))
		{
			subtract_light (seg,i);
			Segments[seg].sides[i].tmap_num2 = GET_INTEL_SHORT(buf + (6 + (2 * i)));
		}
	}
}

void multi_do_flags (const ubyte *buf)
{
	char pnum=buf[1];
	uint flags;

	flags = GET_INTEL_INT(buf + 2);
	if (pnum!=Player_num)
		Players[(int)pnum].flags=flags;
}

void multi_send_flags (char pnum)
{
	if (is_observer()) { return; }

	multibuf[0]=MULTI_FLAGS;
	multibuf[1]=pnum;
	PUT_INTEL_INT(multibuf+2, Players[(int)pnum].flags);
 
	multi_send_data(multibuf, 6, 2);
}

void multi_send_drop_blobs (char pnum)
{
	if (is_observer()) { return; }

	multibuf[0]=MULTI_DROP_BLOB;
	multibuf[1]=pnum;

	multi_send_data(multibuf, 2, 0);
}

void multi_do_drop_blob (const ubyte *buf)
{
	char pnum=buf[1];
	drop_afterburner_blobs (&Objects[Players[(int)pnum].objnum], 2, i2f(5)/2, -1);
}

void multi_send_powcap_update ()
{
	int i;


	multibuf[0]=MULTI_POWCAP_UPDATE;
	for (i=0;i<MAX_POWERUP_TYPES;i++)
		multibuf[i+1]=MaxPowerupsAllowed[i];

	multi_send_data(multibuf, MAX_POWERUP_TYPES+1, 2);
}
void multi_do_powcap_update (const ubyte *buf)
{
	int i;

	for (i=0;i<MAX_POWERUP_TYPES;i++)
		if (buf[i+1]>MaxPowerupsAllowed[i])
			MaxPowerupsAllowed[i]=buf[i+1];
}

extern active_door ActiveDoors[];
extern int Num_open_doors;          // Number of open doors


#if 0 // never used...
void multi_send_active_door (int i)
{
	int count;

	multibuf[0]=MULTI_ACTIVE_DOOR;
	multibuf[1]=i;
	multibuf[2]=Num_open_doors;
	count = 3;
#ifndef WORDS_BIGENDIAN
	memcpy ((char *)(&multibuf[3]),&ActiveDoors[(int)i],sizeof(struct active_door));
	count += sizeof(active_door);
#else
	PUT_INTEL_INT(multibuf + count, ActiveDoors[i].n_parts);                 count += 4;
	PUT_INTEL_SHORT(multibuf + count, ActiveDoors[i].front_wallnum[0]);    count += 2;
	PUT_INTEL_SHORT(multibuf + count, ActiveDoors[i].front_wallnum[1]);    count += 2;
	PUT_INTEL_SHORT(multibuf + count, ActiveDoors[i].back_wallnum[0]);     count += 2;
	PUT_INTEL_SHORT(multibuf + count, ActiveDoors[i].back_wallnum[1]);     count += 2;
	PUT_INTEL_INT(multibuf + count, ActiveDoors[i].time);                    count += 4;
#endif
	multi_send_data (multibuf,count,2);
}
#endif // 0 (never used)

void multi_do_active_door (const ubyte *buf)
{
	char i = multibuf[1];
	Num_open_doors = buf[2];

	memcpy(&ActiveDoors[(int)i], buf+3, sizeof(struct active_door));
#ifdef WORDS_BIGENDIAN
	{
		active_door *ad = &ActiveDoors[(int)i];
		ad->n_parts = INTEL_INT(ad->n_parts);
		ad->front_wallnum[0] = INTEL_SHORT(ad->front_wallnum[0]);
		ad->front_wallnum[1] = INTEL_SHORT(ad->front_wallnum[1]);
		ad->back_wallnum[0] = INTEL_SHORT(ad->back_wallnum[0]);
		ad->back_wallnum[1] = INTEL_SHORT(ad->back_wallnum[1]);
		ad->time = INTEL_INT(ad->time);
	}
#endif //WORDS_BIGENDIAN
}

void multi_send_sound_function (char whichfunc, char sound)
{
	if (is_observer()) { return; }

	int count=0;

	multibuf[0]=MULTI_SOUND_FUNCTION;   count++;
	multibuf[1]=Player_num;             count++;
	multibuf[2]=whichfunc;              count++;
#ifndef WORDS_BIGENDIAN
	*(uint *)(multibuf+count)=sound;    count++;
#else
	multibuf[3] = sound; count++;       // this would probably work on the PC as well.  Jason?
#endif
	multi_send_data (multibuf,4,2);
}

#define AFTERBURNER_LOOP_START  20098
#define AFTERBURNER_LOOP_END    25776

void multi_do_sound_function (const ubyte *buf)
{
	// for afterburner

	char pnum,whichfunc;
	int sound;

    if (Players[Player_num].connected != CONNECT_PLAYING && !is_observer())
		return;

	pnum=buf[1];
	whichfunc=buf[2];
	sound=buf[3];

	if (whichfunc==0)
		digi_kill_sound_linked_to_object (Players[(int)pnum].objnum);
	else if (whichfunc==3)
		digi_link_sound_to_object3( sound, Players[(int)pnum].objnum, 1,F1_0, i2f(256), AFTERBURNER_LOOP_START, AFTERBURNER_LOOP_END);
}

void multi_send_capture_bonus (char pnum)
{
	Assert (Game_mode & GM_CAPTURE);

	multibuf[0]=MULTI_CAPTURE_BONUS;
	multibuf[1]=pnum;

	multi_send_data (multibuf,2,2);
	multi_do_capture_bonus (multibuf);
}
void multi_send_orb_bonus (char pnum)
{
	Assert (Game_mode & GM_HOARD);

	multibuf[0]=MULTI_ORB_BONUS;
	multibuf[1]=pnum;
	multibuf[2]=Players[Player_num].secondary_ammo[PROXIMITY_INDEX];

	multi_send_data (multibuf,3,2);
	multi_do_orb_bonus (multibuf);
}
void multi_do_capture_bonus(const ubyte *buf)
{
	// Figure out the results of a network kills and add it to the
	// appropriate player's tally.

	char pnum=buf[1];
	int TheGoal;

	if (pnum==Player_num)
		HUD_init_message_literal(HM_MULTI, "You have Scored!");
	else
		HUD_init_message(HM_MULTI, "%s has Scored!",Players[(int)pnum].callsign);

	if (pnum==Player_num)
		digi_play_sample (SOUND_HUD_YOU_GOT_GOAL,F1_0*2);
	else if (get_team(pnum)==TEAM_RED)
		digi_play_sample (SOUND_HUD_RED_GOT_GOAL,F1_0*2);
	else
		digi_play_sample (SOUND_HUD_BLUE_GOT_GOAL,F1_0*2);

	Players[(int)pnum].flags &= ~(PLAYER_FLAGS_FLAG);  // Clear capture flag

	team_kills[get_team(pnum)] += 5;
	Players[(int)pnum].net_kills_total += 5;
	Players[(int)pnum].KillGoalCount+=5;

	if (Netgame.KillGoal>0)
	{
		TheGoal=Netgame.KillGoal*5;

		if (Players[(int)pnum].KillGoalCount>=TheGoal)
		{
			if (pnum==Player_num)
			{
				HUD_init_message_literal(HM_MULTI, "You reached the kill goal!");
				Players[Player_num].shields=i2f(200);
			}
			else
				HUD_init_message(HM_MULTI, "%s has reached the kill goal!",Players[(int)pnum].callsign);

			HUD_init_message_literal(HM_MULTI, "The control center has been destroyed!");
			net_destroy_controlcen (obj_find_first_of_type (OBJ_CNTRLCEN));
		}
	}

	multi_sort_kill_list();
	multi_show_player_list();
}

int GetOrbBonus (char num)
{
	int bonus;

	bonus=num*(num+1)/2;
	return (bonus);
}

void multi_do_orb_bonus(const ubyte *buf)
{
	// Figure out the results of a network kills and add it to the
	// appropriate player's tally.

	char pnum=buf[1];
	int TheGoal;
	int bonus=GetOrbBonus (buf[2]);

	if (pnum==Player_num)
		HUD_init_message(HM_MULTI, "You have scored %d points!",bonus);
	else
		HUD_init_message(HM_MULTI, "%s has scored with %d orbs!",Players[(int)pnum].callsign,buf[2]);

	if (pnum==Player_num)
		digi_start_sound_queued (SOUND_HUD_YOU_GOT_GOAL,F1_0*2);
	else if (Game_mode & GM_TEAM)
	{
		if (get_team(pnum)==TEAM_RED)
			digi_play_sample (SOUND_HUD_RED_GOT_GOAL,F1_0*2);
		else
			digi_play_sample (SOUND_HUD_BLUE_GOT_GOAL,F1_0*2);
	}
	else
		digi_play_sample (SOUND_OPPONENT_HAS_SCORED,F1_0*2);

	if (bonus>PhallicLimit)
	{
		if (pnum==Player_num)
			HUD_init_message(HM_MULTI, "You have the record with %d points!",bonus);
		else
			HUD_init_message(HM_MULTI, "%s has the record with %d points!",Players[(int)pnum].callsign,bonus);
		digi_play_sample (SOUND_BUDDY_MET_GOAL,F1_0*2);
		PhallicMan=pnum;
		PhallicLimit=bonus;
	}

	Players[(int)pnum].flags &= ~(PLAYER_FLAGS_FLAG);  // Clear orb flag

	team_kills[get_team(pnum)] += bonus;
	Players[(int)pnum].net_kills_total += bonus;
	Players[(int)pnum].KillGoalCount+=bonus;

	team_kills[get_team(pnum)]%=1000;
	Players[(int)pnum].net_kills_total%=1000;
	Players[(int)pnum].KillGoalCount%=1000;

	if (Netgame.KillGoal>0)
	{
		TheGoal=Netgame.KillGoal*5;

		if (Players[(int)pnum].KillGoalCount>=TheGoal)
		{
			if (pnum==Player_num)
			{
				HUD_init_message_literal(HM_MULTI, "You reached the kill goal!");
				Players[Player_num].shields=i2f(200);
			}
			else
				HUD_init_message(HM_MULTI, "%s has reached the kill goal!",Players[(int)pnum].callsign);

			HUD_init_message_literal(HM_MULTI, "The control center has been destroyed!");
			net_destroy_controlcen (obj_find_first_of_type (OBJ_CNTRLCEN));
		}
	}
	multi_sort_kill_list();
	multi_show_player_list();
}

void multi_send_got_flag (char pnum)
{
	multibuf[0]=MULTI_GOT_FLAG;
	multibuf[1]=pnum;

	digi_start_sound_queued (SOUND_HUD_YOU_GOT_FLAG,F1_0*2);

	multi_send_data (multibuf,2,2);
	multi_send_flags (Player_num);
}

int SoundHacked=0;
digi_sound ReversedSound;

void multi_send_got_orb (char pnum)
{
	multibuf[0]=MULTI_GOT_ORB;
	multibuf[1]=pnum;

	digi_play_sample (SOUND_YOU_GOT_ORB,F1_0*2);

	multi_send_data (multibuf,2,2);
	multi_send_flags (Player_num);
}

void multi_do_got_flag (const ubyte *buf)
{
	char pnum=buf[1];

	if (pnum==Player_num)
		digi_start_sound_queued (SOUND_HUD_YOU_GOT_FLAG,F1_0*2);
	else if (get_team(pnum)==TEAM_RED)
		digi_start_sound_queued (SOUND_HUD_RED_GOT_FLAG,F1_0*2);
	else
		digi_start_sound_queued (SOUND_HUD_BLUE_GOT_FLAG,F1_0*2);
	Players[(int)pnum].flags|=PLAYER_FLAGS_FLAG;
	HUD_init_message(HM_MULTI, "%s picked up a flag!",Players[(int)pnum].callsign);
}
void multi_do_got_orb (const ubyte *buf)
{
	char pnum=buf[1];

	Assert (Game_mode & GM_HOARD);

	if (Game_mode & GM_TEAM)
	{
		if (get_team(pnum)==get_team(Player_num))
			digi_play_sample (SOUND_FRIEND_GOT_ORB,F1_0*2);
		else
			digi_play_sample (SOUND_OPPONENT_GOT_ORB,F1_0*2);
    }
	else
		digi_play_sample (SOUND_OPPONENT_GOT_ORB,F1_0*2);

	Players[(int)pnum].flags|=PLAYER_FLAGS_FLAG;
	HUD_init_message(HM_MULTI, "%s picked up an orb!",Players[(int)pnum].callsign);
}


void DropOrb ()
{
	int objnum,seed;

	if (!(Game_mode & GM_HOARD))
		Int3(); // How did we get here? Get Leighton!

	if (!Players[Player_num].secondary_ammo[PROXIMITY_INDEX])
	{
		HUD_init_message_literal(HM_MULTI, "No orbs to drop!");
		return;
	}

	seed = d_rand();

	objnum = spit_powerup(ConsoleObject,POW_HOARD_ORB,seed);

	if (objnum<0)
		return;

	HUD_init_message_literal(HM_MULTI, "Orb dropped!");
	digi_play_sample (SOUND_DROP_WEAPON,F1_0);

	if ((Game_mode & GM_HOARD) && objnum>-1)
		multi_send_drop_flag(objnum,seed);

	Players[Player_num].secondary_ammo[PROXIMITY_INDEX]--;

	// If empty, tell everyone to stop drawing the box around me
	if (Players[Player_num].secondary_ammo[PROXIMITY_INDEX]==0)
	{
		Players[Player_num].flags &=~(PLAYER_FLAGS_FLAG);
		multi_send_flags (Player_num);
	}
}

void DropFlag ()
{
	int objnum,seed;

	if (!(Game_mode & GM_CAPTURE) && !(Game_mode & GM_HOARD))
		return;
	if (Game_mode & GM_HOARD)
	{
		DropOrb();
		return;
	}

	if (!(Players[Player_num].flags & PLAYER_FLAGS_FLAG))
	{
		HUD_init_message_literal(HM_MULTI, "No flag to drop!");
		return;
	}


	HUD_init_message_literal(HM_MULTI, "Flag dropped!");
	digi_play_sample (SOUND_DROP_WEAPON,F1_0);

	seed = d_rand();

	if (get_team (Player_num)==TEAM_RED)
		objnum = spit_powerup(ConsoleObject,POW_FLAG_BLUE,seed);
	else
		objnum = spit_powerup(ConsoleObject,POW_FLAG_RED,seed);

	if (objnum<0)
		return;

	if ((Game_mode & GM_CAPTURE) && objnum>-1)
		multi_send_drop_flag(objnum,seed);

	Players[Player_num].flags &=~(PLAYER_FLAGS_FLAG);
}


void multi_send_drop_flag (int objnum,int seed)
{
	object *objp;
	int count=0;

	objp = &Objects[objnum];

	multibuf[count++]=(char)MULTI_DROP_FLAG;
	multibuf[count++]=(char)objp->id;

	PUT_INTEL_SHORT(multibuf+count, Player_num); count += 2;
	PUT_INTEL_SHORT(multibuf+count, objnum); count += 2;
	PUT_INTEL_SHORT(multibuf+count, objp->ctype.powerup_info.count); count += 2;
	PUT_INTEL_INT(multibuf+count, seed);

	map_objnum_local_to_local(objnum);

	if (!(Game_mode & GM_HOARD))
		if (Game_mode & GM_NETWORK)
			PowerupsInMine[objp->id]++;

	multi_send_data(multibuf, 12, 2);
}

void multi_do_drop_flag (const ubyte *buf)
{
	int pnum,ammo,objnum,remote_objnum,seed;
	object *objp;
	int powerup_id;

	powerup_id=buf[1];
	pnum = GET_INTEL_SHORT(buf + 2);
	remote_objnum = GET_INTEL_SHORT(buf + 4);
	ammo = GET_INTEL_SHORT(buf + 6);
	seed = GET_INTEL_INT(buf + 8);

	objp = &Objects[Players[pnum].objnum];

	objnum = spit_powerup(objp, powerup_id, seed);

	map_objnum_local_to_remote(objnum, remote_objnum, pnum);

	if (objnum!=-1)
		Objects[objnum].ctype.powerup_info.count = ammo;

	if (!(Game_mode & GM_HOARD))
	{
		if (Game_mode & GM_NETWORK)
			PowerupsInMine[powerup_id]++;
		Players[pnum].flags &= ~(PLAYER_FLAGS_FLAG);
	}
}

#define POWERUPADJUSTS 5
int PowerupAdjustMapping[]={11,19,39,41,44};

int multi_powerup_is_4pack (int id)
{
	int i;

	for (i=0;i<POWERUPADJUSTS;i++)
		if (id==PowerupAdjustMapping[i])
			return (1);
	return (0);
}

int multi_powerup_is_allowed(int id)
{
	if (id == POW_INVULNERABILITY && !(Netgame.AllowedItems & NETFLAG_DOINVUL))
		return (0);
	if (id == POW_CLOAK && !(Netgame.AllowedItems & NETFLAG_DOCLOAK))
		return (0);
	if (id == POW_AFTERBURNER && !(Netgame.AllowedItems & NETFLAG_DOAFTERBURNER))
		return (0);
	if (id == POW_FUSION_WEAPON &&  !(Netgame.AllowedItems & NETFLAG_DOFUSION))
		return (0);
	if (id == POW_PHOENIX_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOPHOENIX))
		return (0);
	if (id == POW_HELIX_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOHELIX))
		return (0);
	if (id == POW_MEGA_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOMEGA))
		return (0);
	if (id == POW_SMARTBOMB_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOSMART))
		return (0);
	if (id == POW_GAUSS_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOGAUSS))
		return (0);
	if (id == POW_VULCAN_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOVULCAN))
		return (0);
	if (id == POW_PLASMA_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOPLASMA))
		return (0);
	if (id == POW_OMEGA_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOOMEGA))
		return (0);
	if (id == POW_SUPER_LASER && !(Netgame.AllowedItems & NETFLAG_DOSUPERLASER))
		return (0);
	if (id == POW_PROXIMITY_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOPROXIM))
		return (0);
	if (id==POW_VULCAN_AMMO && (!(Netgame.AllowedItems & NETFLAG_DOVULCAN) && !(Netgame.AllowedItems & NETFLAG_DOGAUSS)))
		return (0);
	if (id == POW_SPREADFIRE_WEAPON && !(Netgame.AllowedItems & NETFLAG_DOSPREAD))
		return (0);
	if (id == POW_SMART_MINE && !(Netgame.AllowedItems & NETFLAG_DOSMARTMINE))
		return (0);
	if (id == POW_SMISSILE1_1 &&  !(Netgame.AllowedItems & NETFLAG_DOFLASH))
		return (0);
	if (id == POW_SMISSILE1_4 &&  !(Netgame.AllowedItems & NETFLAG_DOFLASH))
		return (0);
	if (id == POW_GUIDED_MISSILE_1 &&  !(Netgame.AllowedItems & NETFLAG_DOGUIDED))
		return (0);
	if (id == POW_GUIDED_MISSILE_4 &&  !(Netgame.AllowedItems & NETFLAG_DOGUIDED))
		return (0);
	if (id == POW_EARTHSHAKER_MISSILE &&  !(Netgame.AllowedItems & NETFLAG_DOSHAKER))
		return (0);
	if (id == POW_MERCURY_MISSILE_1 &&  !(Netgame.AllowedItems & NETFLAG_DOMERCURY))
		return (0);
	if (id == POW_MERCURY_MISSILE_4 &&  !(Netgame.AllowedItems & NETFLAG_DOMERCURY))
		return (0);
	if (id == POW_CONVERTER &&  !(Netgame.AllowedItems & NETFLAG_DOCONVERTER))
		return (0);
	if (id == POW_AMMO_RACK &&  !(Netgame.AllowedItems & NETFLAG_DOAMMORACK))
		return (0);
	if (id == POW_HEADLIGHT &&  !(Netgame.AllowedItems & NETFLAG_DOHEADLIGHT))
		return (0);
	if (id == POW_LASER &&  !(Netgame.AllowedItems & NETFLAG_DOLASER))
		return (0);
	if (id == POW_HOMING_AMMO_1 &&  !(Netgame.AllowedItems & NETFLAG_DOHOMING))
		return (0);
	if (id == POW_HOMING_AMMO_4 &&  !(Netgame.AllowedItems & NETFLAG_DOHOMING))
		return (0);
	if (id == POW_QUAD_FIRE &&  !(Netgame.AllowedItems & NETFLAG_DOQUAD))
		return (0);
	if (id == POW_FLAG_BLUE && !(Game_mode & GM_CAPTURE))
		return (0);
	if (id == POW_FLAG_RED && !(Game_mode & GM_CAPTURE))
		return (0);

	return (1);
}

void multi_send_finish_game ()
{
	multibuf[0]=MULTI_FINISH_GAME;
	multibuf[1]=Player_num;

	multi_send_data (multibuf,2,2);
}


extern void do_final_boss_hacks();
void multi_do_finish_game (const ubyte *buf)
{
	if (buf[0]!=MULTI_FINISH_GAME)
		return;

	if (Current_level_num!=Last_level)
		return;

	do_final_boss_hacks();
}

void multi_send_trigger_specific (char pnum,char trig)
{
	multibuf[0] = MULTI_START_TRIGGER;
	multibuf[1] = trig;

	multi_send_data_direct((ubyte *)multibuf, 2, pnum, 2);
}
void multi_do_start_trigger (const ubyte *buf)
{
	Triggers[(int)buf[1]].flags |=TF_DISABLED;
}

void multi_add_lifetime_kills ()
{
	// This function adds a kill to lifetime stats of this player, and possibly
	// gives a promotion.  If so, it will tell everyone else

	int oldrank;

	if (!(Game_mode & GM_NETWORK))
		return;

	oldrank=GetMyNetRanking();

	PlayerCfg.NetlifeKills++;

	if (oldrank!=GetMyNetRanking())
	{
		multi_send_ranking();
		if (!PlayerCfg.NoRankings)
		{
			HUD_init_message(HM_MULTI, "You have been promoted to %s!",RankStrings[GetMyNetRanking()]);
			digi_play_sample (SOUND_BUDDY_MET_GOAL,F1_0*2);
			Netgame.players[Player_num].rank=GetMyNetRanking();
		}
	}
}

void multi_add_lifetime_killed ()
{
	// This function adds a "killed" to lifetime stats of this player, and possibly
	// gives a demotion.  If so, it will tell everyone else

	int oldrank;

	if (!(Game_mode & GM_NETWORK))
		return;

	oldrank=GetMyNetRanking();

	PlayerCfg.NetlifeKilled++;

	if (oldrank!=GetMyNetRanking())
	{
		multi_send_ranking();
		Netgame.players[Player_num].rank=GetMyNetRanking();

		if (!PlayerCfg.NoRankings)
			HUD_init_message(HM_MULTI, "You have been demoted to %s!",RankStrings[GetMyNetRanking()]);

	}
}

void multi_send_ranking ()
{
	multibuf[0]=(char)MULTI_RANK;
	multibuf[1]=(char)Player_num;
	multibuf[2]=(char)GetMyNetRanking();

	multi_send_data (multibuf,3,2);
}

void multi_do_ranking (const ubyte *buf)
{
	char rankstr[20];
	char pnum=buf[1];
	char rank=buf[2];

	if (Netgame.players[(int)pnum].rank<rank)
		strcpy (rankstr,"promoted");
	else if (Netgame.players[(int)pnum].rank>rank)
		strcpy (rankstr,"demoted");
	else
		return;

	Netgame.players[(int)pnum].rank=rank;

	if (!PlayerCfg.NoRankings)
		HUD_init_message(HM_MULTI, "%s has been %s to %s!",Players[(int)pnum].callsign,rankstr,RankStrings[(int)rank]);
}

void multi_quick_sound_hack (int num)
{
	int length,i;
	num = digi_xlat_sound(num);
	length=GameSounds[num].length;
	ReversedSound.data=(ubyte *)d_malloc (length);
	ReversedSound.length=length;

	for (i=0;i<length;i++)
		ReversedSound.data[i]=GameSounds[num].data[length-i-1];

	SoundHacked=1;
}

void multi_send_play_by_play (int num,int spnum,int dpnum)
{
	if (!(Game_mode & GM_HOARD))
		return;

	return;
	multibuf[0]=MULTI_PLAY_BY_PLAY;
	multibuf[1]=(char)num;
	multibuf[2]=(char)spnum;
	multibuf[3]=(char)dpnum;
	multi_send_data (multibuf,4,2);
	multi_do_play_by_play (multibuf);
}

void multi_do_play_by_play (const ubyte *buf)
{
	int whichplay=buf[1];
	int spnum=buf[2];
	int dpnum=buf[3];

	if (!(Game_mode & GM_HOARD))
	{
		Int3(); // Get Leighton, something bad has happened.
		return;
	}

	switch (whichplay)
	{
	case 0: // Smacked!
		HUD_init_message(HM_MULTI, "Ouch! %s has been smacked by %s!",Players[dpnum].callsign,Players[spnum].callsign);
		break;
	case 1: // Spanked!
		HUD_init_message(HM_MULTI, "Haha! %s has been spanked by %s!",Players[dpnum].callsign,Players[spnum].callsign);
		break;
	default:
		Int3();
	}
}

// Decide if fire from "killer" is friendly. If yes return 1 (no harm to me) otherwise 0 (damage me)
int multi_maybe_disable_friendly_fire(object *killer)
{
	if (!(Game_mode & GM_NETWORK)) // no Multiplayer game -> always harm me!
		return 0;
	if (!Netgame.NoFriendlyFire) // friendly fire is activated -> harm me!
		return 0;
	if (killer == NULL) // no actual killer -> harm me!
		return 0;
	if (killer->type != OBJ_PLAYER) // not a player -> harm me!
		return 0;
	if (killer == ConsoleObject) // it's me -> harm me! (That's not how you use concussion missiles!)
		return 0;
	if (Game_mode & GM_MULTI_COOP) // coop mode -> don't harm me!
		return 1;
	else if (Game_mode & GM_TEAM) // team mode - find out if killer is in my team
	{
		if (get_team(Player_num) == get_team(killer->id)) // in my team -> don't harm me!
			return 1;
		else // opposite team -> harm me!
			return 0;
	}
	return 0; // all other cases -> harm me!
}

void multi_do_request_status()
{
	if (is_observer()) { return; }

	if (Netgame.max_numobservers == 0 && !Netgame.host_is_obs) { return; }

	multi_send_repair(0, Players[Player_num].shields, 0);
	multi_send_ship_status();
}

void multi_send_damage(fix damage, fix shields, ubyte killer_type, ubyte killer_id, ubyte damage_type, object* source)
{
	if (is_observer()) { return; }

	// Sending damage to the host isn't interesting if there cannot be any observers.
	if (Netgame.max_numobservers == 0 && !Netgame.host_is_obs) { return; }

	if (Player_is_dead || ConsoleObject->flags & OF_SHOULD_BE_DEAD) { return; }

	// Calculate new shields amount.
	if (shields < damage)
		shields = 0;
	else
		shields -= damage;

	// Setup damage packet.
	multibuf[0] = MULTI_DAMAGE;
	multibuf[1] = Player_num;
	PUT_INTEL_INT(multibuf + 2, damage);
	PUT_INTEL_INT(multibuf + 6, shields);
	multibuf[10] = killer_type;
	multibuf[11] = killer_id;
	multibuf[12] = damage_type;
	if (source == NULL)
	{
		multibuf[13] = 0;
	}
	else if (source->type == OBJ_WEAPON)
	{
		multibuf[13] = source->id;
	}
	else
	{
		multibuf[13] = 0;
	}

	multi_send_data_direct( multibuf, 14, multi_who_is_master(), 2 );
}

void multi_do_damage( const ubyte *buf )
{
	if (is_observer())
	{
		fix old_shields = Players[buf[1]].shields;
		fix new_shields = GET_INTEL_INT(buf + 6);
		fix shields_delta = GET_INTEL_INT(buf + 2);
		Players[buf[1]].shields_certain = (new_shields <= 0 || Players[buf[1]].shields - shields_delta == new_shields) ? 1 : 0;
		Players[buf[1]].shields = (new_shields > 0) ? new_shields : 0;
		if (Players[Player_num].hours_total - Players[buf[1]].shields_time_hours > 1 || Players[Player_num].hours_total - Players[buf[1]].shields_time_hours == 1 && i2f(3600) + Players[Player_num].time_total - Players[buf[1]].shields_time > i2f(2) || Players[Player_num].time_total - Players[buf[1]].shields_time > i2f(2)) {
			Players[buf[1]].shields_delta = 0;
			Players[buf[1]].shields_certain = 1;
		}
		Players[buf[1]].shields_delta -= shields_delta;

		if (GET_INTEL_INT(buf + 2) != 0) {
			Players[buf[1]].shields_time = Players[Player_num].time_total;
			Players[buf[1]].shields_time_hours = Players[Player_num].hours_total;
		}

		if (shields_delta != 0) {
			add_observatory_damage_stat(buf[1], shields_delta, new_shields, old_shields, buf[10], buf[11], buf[12], buf[13]);
		}
	}
}

void multi_send_repair(fix repair, fix shields, ubyte sourcetype)
{
	if (is_observer()) { return; }

	// Sending repairs to the host isn't interesting if there cannot be any observers.
	if (Netgame.max_numobservers == 0 && !Netgame.host_is_obs) { return; }

	// Calculate new shields amount.
	if (shields + repair > MAX_SHIELDS)
		shields = MAX_SHIELDS;
	else
		shields += repair;
	
	// Setup repair packet.
	multibuf[0] = MULTI_REPAIR;
	multibuf[1] = Player_num;
	PUT_INTEL_INT(multibuf + 2, repair);
	PUT_INTEL_INT(multibuf + 6, shields);
	multibuf[10] = sourcetype;

	multi_send_data_direct( multibuf, 11, multi_who_is_master(), 2);
}

void multi_do_repair(const ubyte *buf)
{
	if (is_observer())
	{
		fix old_shields = Players[buf[1]].shields;
		fix new_shields = GET_INTEL_INT(buf + 6);
		fix shields_delta = GET_INTEL_INT(buf + 2);
		Players[buf[1]].shields_certain = (Players[buf[1]].shields + shields_delta == new_shields) ? 1 : 0;
		Players[buf[1]].shields = (new_shields > 0) ? new_shields : 0;

		if (shields_delta == 0) {
			Players[buf[1]].shields_certain = 1;
		} else {
			if (Players[Player_num].hours_total - Players[buf[1]].shields_time_hours > 1 || Players[Player_num].hours_total - Players[buf[1]].shields_time_hours == 1 && i2f(3600) + Players[Player_num].time_total - Players[buf[1]].shields_time > i2f(2) || Players[Player_num].time_total - Players[buf[1]].shields_time > i2f(2)) {
				Players[buf[1]].shields_delta = 0;
			}

			Players[buf[1]].shields_delta += shields_delta;
			Players[buf[1]].shields_time = Players[Player_num].time_total;
			Players[buf[1]].shields_time_hours = Players[Player_num].hours_total;
		}

		if (shields_delta != 0) {
			add_observatory_damage_stat(buf[1], shields_delta, new_shields, old_shields, 0, 0, DAMAGE_SHIELD, 0);
		}
	}
}

void multi_send_ship_status()
{
	if (is_observer()) { return; }

	// Sending ship status to the host isn't interesting if there cannot be any observers.
	if (Netgame.max_numobservers == 0 && !Netgame.host_is_obs) { return; }

	Send_ship_status = 1;
}

void multi_send_ship_status_for_frame()
{
	// Setup ship status packet.
	multibuf[0] = MULTI_SHIP_STATUS;
	multibuf[1] = Player_num;
	multibuf[2] = Players[Player_num].laser_level;
	PUT_INTEL_SHORT(multibuf + 3, Players[Player_num].flags);
	PUT_INTEL_SHORT(multibuf + 5, Players[Player_num].primary_ammo[1]);
	multibuf[7] = Players[Player_num].primary_weapon_flags;
	multibuf[8] = (ubyte)Players[Player_num].primary_weapon;
	PUT_INTEL_SHORT(multibuf + 9, Players[Player_num].secondary_ammo[0]);
	PUT_INTEL_SHORT(multibuf + 11, Players[Player_num].secondary_ammo[1]);
	PUT_INTEL_SHORT(multibuf + 13, Players[Player_num].secondary_ammo[2]);
	PUT_INTEL_SHORT(multibuf + 15, Players[Player_num].secondary_ammo[3]);
	PUT_INTEL_SHORT(multibuf + 17, Players[Player_num].secondary_ammo[4]);
	PUT_INTEL_SHORT(multibuf + 19, Players[Player_num].secondary_ammo[5]);
	PUT_INTEL_SHORT(multibuf + 21, Players[Player_num].secondary_ammo[6]);
	PUT_INTEL_SHORT(multibuf + 23, Players[Player_num].secondary_ammo[7]);
	PUT_INTEL_SHORT(multibuf + 25, Players[Player_num].secondary_ammo[8]);
	PUT_INTEL_SHORT(multibuf + 27, Players[Player_num].secondary_ammo[9]);
	multibuf[29] = Players[Player_num].secondary_weapon_flags;
	multibuf[30] = (ubyte)Players[Player_num].secondary_weapon;
	PUT_INTEL_INT(multibuf + 31, Players[Player_num].energy);
	PUT_INTEL_INT(multibuf + 35, Players[Player_num].homing_object_dist);
	PUT_INTEL_INT(multibuf + 39, Players[Player_num].afterburner_charge);

	multi_send_data_direct(multibuf, 43, multi_who_is_master(), 2);
}

void multi_do_ship_status( const ubyte *buf )
{
	if (is_observer())
	{
		Players[buf[1]].laser_level = buf[2];
		Players[buf[1]].flags = GET_INTEL_SHORT(buf + 3);
		Players[buf[1]].primary_ammo[1] = GET_INTEL_SHORT(buf + 5);
		Players[buf[1]].primary_weapon_flags = buf[7];
		Players[buf[1]].primary_weapon = (sbyte)buf[8];
		Players[buf[1]].secondary_ammo[0] = GET_INTEL_SHORT(buf + 9);
		Players[buf[1]].secondary_ammo[1] = GET_INTEL_SHORT(buf + 11);
		Players[buf[1]].secondary_ammo[2] = GET_INTEL_SHORT(buf + 13);
		Players[buf[1]].secondary_ammo[3] = GET_INTEL_SHORT(buf + 15);
		Players[buf[1]].secondary_ammo[4] = GET_INTEL_SHORT(buf + 17);
		Players[buf[1]].secondary_ammo[5] = GET_INTEL_SHORT(buf + 19);
		Players[buf[1]].secondary_ammo[6] = GET_INTEL_SHORT(buf + 21);
		Players[buf[1]].secondary_ammo[7] = GET_INTEL_SHORT(buf + 23);
		Players[buf[1]].secondary_ammo[8] = GET_INTEL_SHORT(buf + 25);
		Players[buf[1]].secondary_ammo[9] = GET_INTEL_SHORT(buf + 27);
		Players[buf[1]].secondary_weapon_flags = buf[29];
		Players[buf[1]].secondary_weapon = (sbyte)buf[30];
		Players[buf[1]].energy = GET_INTEL_INT(buf + 31);
		Players[buf[1]].homing_object_dist = GET_INTEL_INT(buf + 35);
		Players[buf[1]].afterburner_charge = GET_INTEL_INT(buf + 39);
	}
}

bool is_observing_player() {
	return is_observer() && (Current_obs_player != OBSERVER_PLAYER_ID && (!multi_i_am_master() || Current_obs_player != 0));
}

bool object_is_observer(object* obj)
{
	return is_observer() && obj == ConsoleObject;
}

int get_observer_game_mode()
{
	// Co-op - check for flag
	if (Game_mode & GM_MULTI_COOP)
		return 3;
	// Teams - check for flag
	else if (Game_mode & GM_TEAM)
		return 2;
	// We are some kind of free-for-all mode. Check the player count
	else if (N_players > 2)
		return 1;
	// This is a 1v1, or only one player is connected (which we'll treat like a 1v1)
	else {
		// This function should only be called in multiplayer.
		// Return 0 (1v1) in single-player to avoid a crash, but this indicates a bug.
		//Assert(Game_mode & GM_MULTI); // Uncomment this before releasing ranking mod.
		return 0;
	}
}

/* Bounty packer sender and handler */
void multi_send_bounty( void )
{
	if (is_observer()) { return; }

	/* Test game mode */
	if( !( Game_mode & GM_BOUNTY ) )
		return;
	if ( !multi_i_am_master() )
		return;
	
	/* Add opcode, target ID and how often we re-assigned */
	multibuf[0] = MULTI_DO_BOUNTY;
	multibuf[1] = (char)Bounty_target;
	
	/* Send data */
	multi_send_data( multibuf, 2, 2 );
}

void multi_do_bounty( const ubyte *buf )
{
	if ( multi_i_am_master() )
		return;
	
	multi_new_bounty_target( buf[1] );
}

void multi_new_bounty_target( int pnum )
{
	/* If it's already the same, don't do it */
	if( Bounty_target == pnum )
		return;
	
	/* Set the target */
	Bounty_target = pnum;

	if(Netgame.FairColors)
		selected_player_rgb = player_rgb_all_blue; 
	else if(Netgame.BlackAndWhitePyros) 
		selected_player_rgb = player_rgb_alt; 
	else
		selected_player_rgb = player_rgb;	
	
	/* Send a message */
	HUD_init_message( HM_MULTI, "%c%c%s is the new target!", CC_COLOR,
		BM_XRGB( selected_player_rgb[Bounty_target].r, selected_player_rgb[Bounty_target].g, selected_player_rgb[Bounty_target].b ),
		Players[Bounty_target].callsign );

	digi_play_sample( SOUND_BUDDY_MET_GOAL, F1_0 * 2 );
}

void multi_send_obs_update(ubyte event, ubyte event_data) {
	if(! multi_i_am_master()) { return; }

	multibuf[0] = MULTI_OBS_UPDATE; 
	multibuf[1] = event;
	multibuf[2] = event_data;
	multibuf[3] = Netgame.numobservers;

	for (int i = 0; i < Netgame.max_numobservers; i++) {
		if (Netgame.observers[i].connected == 1) {
			memcpy(&multibuf[4 + i * 8], &Netgame.observers[i].callsign, 8);
		}
		else {
			memset(&multibuf[4 + i * 8], 0, 8);
		}
	}

	multi_send_data( multibuf, 4 + 8*MAX_OBSERVERS, 2 );

	if (event == 0 && !is_observer())
	{
		multi_do_request_status();
	}
}

void multi_do_obs_update(const ubyte *buf) {
	if(multi_i_am_master()) { return; }

	Netgame.numobservers = buf[3];
	if(Netgame.max_numobservers < Netgame.numobservers) {
		Netgame.max_numobservers = Netgame.numobservers;
	}
	for(int i = 0; i < Netgame.max_numobservers; i++) {
		memcpy(&Netgame.observers[i].callsign, &buf[4+i*8], 8); 
	}

	// Someone joined
	if(buf[1] == 0 && buf[2] < Netgame.max_numobservers) {
		HUD_init_message(HM_MULTI, "%s is now observing.", Netgame.observers[buf[2]].callsign);

		if (!is_observer())
		{
			multi_do_request_status();
		}
	}
}

void multi_do_save_game(const ubyte *buf)
{
	int count = 1;
	ubyte slot;
	uint id;
	char desc[25];

	slot = *(ubyte *)(buf+count);			count += 1;
	id = GET_INTEL_INT(buf+count);			count += 4;
	memcpy( desc, &buf[count], 20 );		count += 20;

	multi_save_game( slot, id, desc );
}

void multi_do_restore_game(const ubyte *buf)
{
	int count = 1;
	ubyte slot;
	uint id;

	slot = *(ubyte *)(buf+count);			count += 1;
	id = GET_INTEL_INT(buf+count);			count += 4;

	multi_restore_game( slot, id );
}

void multi_send_save_game(ubyte slot, uint id, char * desc)
{
	if (is_observer()) { return; }

	int count = 0;
	
	multibuf[count] = MULTI_SAVE_GAME;		count += 1;
	multibuf[count] = slot;				count += 1; // Save slot=0
	PUT_INTEL_INT( multibuf+count, id );		count += 4; // Save id
	memcpy( &multibuf[count], desc, 20 );		count += 20;

	multi_send_data(multibuf, count, 2);
}

void multi_send_restore_game(ubyte slot, uint id)
{
	int count = 0;
	
	multibuf[count] = MULTI_RESTORE_GAME;		count += 1;
	multibuf[count] = slot;				count += 1; // Save slot=0
	PUT_INTEL_INT( multibuf+count, id );		count += 4; // Save id

	multi_send_data(multibuf, count, 2);
}

void multi_initiate_save_game()
{
	if (Netgame.host_is_obs) {
		HUD_init_message_literal(HM_MULTI, "Can't save with a host that is observing!");
		return;
	}

	fix game_id = 0;
	int i, j, slot;
	char filename[PATH_MAX];
	char desc[24];

	if ((Endlevel_sequence) || (Control_center_destroyed))
		return;

	if (!multi_i_am_master())
	{
		HUD_init_message_literal(HM_MULTI, "Only host is allowed to save a game!");
		return;
	}
	if (!multi_all_players_alive())
	{
		HUD_init_message_literal(HM_MULTI, "Can't save! All players must be alive!");
		return;
	}
	for (i = 0; i < N_players; i++)
	{
		for (j = 0; j < N_players; j++)
		{
			if (i != j && !d_stricmp(Players[i].callsign, Players[j].callsign))
			{
				HUD_init_message_literal(HM_MULTI, "Can't save! Multiple players with same callsign!");
				return;
			}
		}
	}

	memset(&filename, '\0', PATH_MAX);
	memset(&desc, '\0', 24);
	slot = state_get_save_file(filename, desc, 0 );
	if (!slot)
		return;
	slot--;

	// Make a unique game id
	game_id = ((fix)timer_query());
	game_id ^= N_players<<4;
	for (i = 0; i < N_players; i++ )
	{
		fix call2i;
		memcpy(&call2i, Players[i].callsign, sizeof(fix));
		game_id ^= call2i;
	}
	if ( game_id == 0 )
		game_id = 1; // 0 is invalid

	multi_send_save_game( slot, game_id, desc );
	multi_do_frame();
	multi_save_game( slot,game_id, desc );
}

extern int state_get_game_id(char *);

void multi_initiate_restore_game()
{
	if (Netgame.host_is_obs) {
		HUD_init_message_literal(HM_MULTI, "Can't load with a host that is observing!");
		return;
	}

	int i, j, slot;
	char filename[PATH_MAX];

	if ((Endlevel_sequence) || (Control_center_destroyed))
		return;

	if (!multi_i_am_master())
	{
		HUD_init_message_literal(HM_MULTI, "Only host is allowed to load a game!");
		return;
	}
	if (!multi_all_players_alive())
	{
		HUD_init_message_literal(HM_MULTI, "Can't load! All players must be alive!");
		return;
	}
	for (i = 0; i < N_players; i++)
	{
		for (j = 0; j < N_players; j++)
		{
			if (i != j && !d_stricmp(Players[i].callsign, Players[j].callsign))
			{
				HUD_init_message_literal(HM_MULTI, "Can't load! Multiple players with same callsign!");
				return;
			}
		}
	}
	slot = state_get_restore_file(filename);
	if (!slot)
		return;
	state_game_id = state_get_game_id(filename);
	if (!state_game_id)
		return;
	slot--;
	multi_send_restore_game(slot,state_game_id);
	multi_do_frame();
	multi_restore_game(slot,state_game_id);
}

void multi_save_game(ubyte slot, uint id, char *desc)
{
	char filename[PATH_MAX];

	if ((Endlevel_sequence) || (Control_center_destroyed))
		return;

	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%s.mg%d" : "%s.mg%d", Players[Player_num].callsign, slot);
	HUD_init_message(HM_MULTI,  "Saving game #%d, '%s'", slot, desc);
	stop_time();
	state_game_id = id;
	state_save_all_sub(filename, desc );
}

void multi_restore_game(ubyte slot, uint id)
{
	char filename[PATH_MAX];
	int i;
	int thisid;

	if ((Endlevel_sequence) || (Control_center_destroyed))
		return;

	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%s.mg%d" : "%s.mg%d", Players[Player_num].callsign, slot);
   
	for (i = 0; i < N_players; i++)
		multi_strip_robots(i);
	if (multi_i_am_master()) // put all players to wait-state again so we can sync up properly
		for (i = 0; i < MAX_PLAYERS; i++)
			if (Players[i].connected == CONNECT_PLAYING && i != Player_num) {
				Players[i].connected = CONNECT_WAITING;

				if (Current_obs_player == i) {
					reset_obs();
				}
			}
   
	thisid=state_get_game_id(filename);
	if (thisid!=id)
	{
		nm_messagebox(NULL, 1, TXT_OK, "A multi-save game was restored\nthat you are missing or does not\nmatch that of the others.\nYou must rejoin if you wish to\ncontinue.");
		return;
	}
  
	state_restore_all_sub( filename, 0 );
	multi_send_score(); // send my restored scores. I sent 0 when I loaded the level anyways...
}

void multi_do_msgsend_state(const ubyte *buf)
{
	multi_sending_message[(int)buf[1]] = (int)buf[2];
}

void multi_send_msgsend_state(int state)
{
	if (is_observer()) { return; }

	multibuf[0] = (char)MULTI_TYPING_STATE;
	multibuf[1] = Player_num;
	multibuf[2] = (char)state;
	
	multi_send_data(multibuf, 3, 2);
}

// Specific variables related to our game mode we want the clients to know about
void multi_send_gmode_update()
{
	if (!multi_i_am_master())
		return;
	if (!(Game_mode & GM_TEAM || Game_mode & GM_BOUNTY)) // expand if necessary
		return;
	multibuf[0] = (char)MULTI_GMODE_UPDATE;
	multibuf[1] = Netgame.team_vector;
	multibuf[2] = Bounty_target;
	
	multi_send_data(multibuf, 3, 0);
}

void multi_do_gmode_update(const ubyte *buf)
{
	if (multi_i_am_master())
		return;
	if (Game_mode & GM_TEAM)
	{
		if (buf[1] != Netgame.team_vector)
		{
			int t;
			Netgame.team_vector = buf[1];
			for (t=0;t<N_players;t++)
				if (Players[t].connected)
					multi_reset_object_texture (&Objects[Players[t].objnum]);
			reset_cockpit();
		}
	}
	if (Game_mode & GM_BOUNTY)
	{
		Bounty_target = buf[2]; // accept silently - message about change we SHOULD have gotten due to kill computation
	}
}

///
/// CODE TO LOAD HOARD DATA
///

int HoardEquipped()
{
	static int checked=-1;

	if (checked==-1)
	{
		if (PHYSFSX_exists("hoard.ham",1))
			checked=1;
		else
			checked=0;
	}
	return (checked);
}

grs_bitmap Orb_icons[2];
int Hoard_goal_eclip, Hoard_bm_idx, Hoard_snd_idx;

void free_hoard_data()
{
	int i;

	d_free(GameBitmaps[Hoard_bm_idx].bm_data);
	for (i = Hoard_snd_idx; i < Hoard_snd_idx+4; i++)
		d_free(GameSounds[i].data);
	for (i = 0; i < 2; i++)
		d_free(Orb_icons[i].bm_data);
}

void init_hoard_data()
{
	static int first_time=1;
	static int orb_vclip;
	int n_orb_frames,n_goal_frames;
	int orb_w,orb_h;
	int icon_w,icon_h;
	ubyte palette[256*3];
	PHYSFS_file *ifile;
	ubyte *bitmap_data1;
	int i,save_pos;
	extern int Num_bitmap_files,Num_effects,Num_sound_files;
	int bitmap_num=Hoard_bm_idx=Num_bitmap_files;

	if (!first_time)
		free_hoard_data();

	ifile = PHYSFSX_openReadBuffered("hoard.ham");
	if (ifile == NULL)
		Error("can't open <hoard.ham>");

	n_orb_frames = PHYSFSX_readShort(ifile);
	orb_w = PHYSFSX_readShort(ifile);
	orb_h = PHYSFSX_readShort(ifile);
	save_pos = PHYSFS_tell(ifile);
	PHYSFSX_fseek(ifile,sizeof(palette)+n_orb_frames*orb_w*orb_h,SEEK_CUR);
	n_goal_frames = PHYSFSX_readShort(ifile);
	PHYSFSX_fseek(ifile,save_pos,SEEK_SET);

	//Allocate memory for bitmaps
	MALLOC( bitmap_data1, ubyte, n_orb_frames*orb_w*orb_h + n_goal_frames*64*64 );

	//Create orb vclip
	orb_vclip = Num_vclips++;
	Assert(Num_vclips <= VCLIP_MAXNUM);
	Vclip[orb_vclip].play_time = F1_0/2;
	Vclip[orb_vclip].num_frames = n_orb_frames;
	Vclip[orb_vclip].frame_time = Vclip[orb_vclip].play_time / Vclip[orb_vclip].num_frames;
	Vclip[orb_vclip].flags = 0;
	Vclip[orb_vclip].sound_num = -1;
	Vclip[orb_vclip].light_value = F1_0;
	for (i=0;i<n_orb_frames;i++) {
		Vclip[orb_vclip].frames[i].index = bitmap_num;
		gr_init_bitmap(&GameBitmaps[bitmap_num],BM_LINEAR,0,0,orb_w,orb_h,orb_w,bitmap_data1);
		gr_set_transparent (&GameBitmaps[bitmap_num], 1);
		bitmap_data1 += orb_w*orb_h;
		bitmap_num++;
		Assert(bitmap_num < MAX_BITMAP_FILES);
	}

	//Create obj powerup
	Powerup_info[POW_HOARD_ORB].vclip_num = orb_vclip;
	Powerup_info[POW_HOARD_ORB].hit_sound = -1; //Powerup_info[POW_SHIELD_BOOST].hit_sound;
	Powerup_info[POW_HOARD_ORB].size = Powerup_info[POW_SHIELD_BOOST].size;
	Powerup_info[POW_HOARD_ORB].light = Powerup_info[POW_SHIELD_BOOST].light;

	//Create orb goal wall effect
	Hoard_goal_eclip = Num_effects++;
	Assert(Num_effects < MAX_EFFECTS);
	Effects[Hoard_goal_eclip] = Effects[94];        //copy from blue goal
	Effects[Hoard_goal_eclip].changing_wall_texture = NumTextures;
	Effects[Hoard_goal_eclip].vc.num_frames=n_goal_frames;

	TmapInfo[NumTextures] = TmapInfo[find_goal_texture(TMI_GOAL_BLUE)];
	TmapInfo[NumTextures].eclip_num = Hoard_goal_eclip;
	TmapInfo[NumTextures].flags = TMI_GOAL_HOARD;
	NumTextures++;
	Assert(NumTextures < MAX_TEXTURES);
	for (i=0;i<n_goal_frames;i++) {
		Effects[Hoard_goal_eclip].vc.frames[i].index = bitmap_num;
		gr_init_bitmap(&GameBitmaps[bitmap_num],BM_LINEAR,0,0,64,64,64,bitmap_data1);
		bitmap_data1 += 64*64;
		bitmap_num++;
		Assert(bitmap_num < MAX_BITMAP_FILES);
	}

	//Load and remap bitmap data for orb
	PHYSFS_read(ifile,palette,3,256);
	for (i=0;i<n_orb_frames;i++) {
		grs_bitmap *bm = &GameBitmaps[Vclip[orb_vclip].frames[i].index];
		PHYSFS_read(ifile,bm->bm_data,1,orb_w*orb_h);
		gr_remap_bitmap_good( bm, palette, 255, -1 );
	}

	//Load and remap bitmap data for goal texture
	PHYSFSX_readShort(ifile);        //skip frame count
	PHYSFS_read(ifile,palette,3,256);
	for (i=0;i<n_goal_frames;i++) {
		grs_bitmap *bm = &GameBitmaps[Effects[Hoard_goal_eclip].vc.frames[i].index];
		PHYSFS_read(ifile,bm->bm_data,1,64*64);
		gr_remap_bitmap_good( bm, palette, 255, -1 );
	}

	//Load and remap bitmap data for HUD icons
	for (i=0;i<2;i++) {
		ubyte *bitmap_data2;
		icon_w = PHYSFSX_readShort(ifile);
		icon_h = PHYSFSX_readShort(ifile);
		MALLOC( bitmap_data2, ubyte, icon_w*icon_h );
		gr_init_bitmap(&Orb_icons[i],BM_LINEAR,0,0,icon_w,icon_h,icon_w,bitmap_data2);
		gr_set_transparent (&Orb_icons[i], 1);
		PHYSFS_read(ifile,palette,3,256);
		PHYSFS_read(ifile,Orb_icons[i].bm_data,1,icon_w*icon_h);
		gr_remap_bitmap_good( &Orb_icons[i], palette, 255, -1 );
	}

	//Load sounds for orb game
	Hoard_snd_idx = Num_sound_files;
	for (i=0;i<4;i++) {
		int len;

		len = PHYSFSX_readInt(ifile);        //get 11k len

		if (GameArg.SndDigiSampleRate == SAMPLE_RATE_22K) {
			PHYSFSX_fseek(ifile,len,SEEK_CUR);     //skip over 11k sample
			len = PHYSFSX_readInt(ifile);    //get 22k len
		}

		GameSounds[Num_sound_files+i].length = len;
		GameSounds[Num_sound_files+i].data = d_malloc(len);
		PHYSFS_read(ifile,GameSounds[Num_sound_files+i].data,1,len);

		if (GameArg.SndDigiSampleRate == SAMPLE_RATE_11K) {
			len = PHYSFSX_readInt(ifile);    //get 22k len
			PHYSFSX_fseek(ifile,len,SEEK_CUR);     //skip over 22k sample
		}

		Sounds[SOUND_YOU_GOT_ORB+i] = Num_sound_files+i;
		AltSounds[SOUND_YOU_GOT_ORB+i] = Sounds[SOUND_YOU_GOT_ORB+i];
	}

	PHYSFS_close(ifile);
	if (first_time)
		atexit(free_hoard_data);

	first_time = 0;
}

#ifdef EDITOR
void save_hoard_data(void)
{
	#define MAX_BITMAPS_PER_BRUSH 30
	grs_bitmap * bm[MAX_BITMAPS_PER_BRUSH];
	grs_bitmap icon;
	int nframes;
	ubyte palette[256*3];
	PHYSFS_file *ofile;
	int iff_error,i;
	char *sounds[] = {"selforb.raw","selforb.r22",          //SOUND_YOU_GOT_ORB
				"teamorb.raw","teamorb.r22",    //SOUND_FRIEND_GOT_ORB
				"enemyorb.raw","enemyorb.r22",  //SOUND_OPPONENT_GOT_ORB
				"OPSCORE1.raw","OPSCORE1.r22"}; //SOUND_OPPONENT_HAS_SCORED
		
	ofile = PHYSFSX_openWriteBuffered("hoard.ham");

	iff_error = iff_read_animbrush("orb.abm",bm,MAX_BITMAPS_PER_BRUSH,&nframes,palette);
	Assert(iff_error == IFF_NO_ERROR);
	PHYSFS_writeULE16(ofile, nframes);
	PHYSFS_writeULE16(ofile, bm[0]->bm_w);
	PHYSFS_writeULE16(ofile, bm[0]->bm_h);
	PHYSFS_write(ofile, palette, 3, 256);
	for (i=0;i<nframes;i++)
		PHYSFS_write(ofile, bm[i]->bm_data, bm[i]->bm_w*bm[i]->bm_h, 1);

	iff_error = iff_read_animbrush("orbgoal.abm",bm,MAX_BITMAPS_PER_BRUSH,&nframes,palette);
	Assert(iff_error == IFF_NO_ERROR);
	Assert(bm[0]->bm_w == 64 && bm[0]->bm_h == 64);
	PHYSFS_writeULE16(ofile, nframes);
	PHYSFS_write(ofile, palette, 3, 256);
	for (i=0;i<nframes;i++)
		PHYSFS_write(ofile, bm[i]->bm_data, bm[i]->bm_w*bm[i]->bm_h, 1);

	for (i=0;i<2;i++)
	{
		iff_error = iff_read_bitmap(i?"orbb.bbm":"orb.bbm",&icon,BM_LINEAR,palette);
		Assert(iff_error == IFF_NO_ERROR);
		PHYSFS_writeULE16(ofile, icon.bm_w);
		PHYSFS_writeULE16(ofile, icon.bm_h);
		PHYSFS_write(ofile, palette, 3, 256);
		PHYSFS_write(ofile, icon.bm_data, icon.bm_w*icon.bm_h, 1);
	}
	(void)iff_error;
		
	for (i=0;i<sizeof(sounds)/sizeof(*sounds);i++) {
		PHYSFS_file *ifile;
		int size;
		ubyte *buf;

		ifile = PHYSFS_openRead(sounds[i]);
		Assert(ifile != NULL);
		size = PHYSFS_fileLength(ifile);
		buf = d_malloc(size);
		PHYSFS_read(ifile, buf, size, 1);
		PHYSFS_writeULE32(ofile, size);
		PHYSFS_write(ofile, buf, size, 1);
		d_free(buf);
		PHYSFS_close(ifile);
	}

	PHYSFS_close(ofile);
}
#endif

void
multi_process_data(const ubyte *buf, int len)
{
	// Take an entire message (that has already been checked for validity,
	// if necessary) and act on it.

	int type;
	len = len;

	type = buf[0];

	if (type >= sizeof(message_length) / sizeof(message_length[0]))
	{
		Int3();
		return;
	}

	switch(type)
	{
		case MULTI_POSITION:
			if (!Endlevel_sequence) multi_do_position(buf); break;
		case MULTI_REAPPEAR:
			if (!Endlevel_sequence) multi_do_reappear(buf); break;
		case MULTI_FIRE:
			if (!Endlevel_sequence) multi_do_fire(buf); break;
		case MULTI_KILL:
			multi_do_kill(buf); break;
		case MULTI_REMOVE_OBJECT:
			if (!Endlevel_sequence) multi_do_remobj(buf); break;
		case MULTI_PLAYER_DROP:
		case MULTI_PLAYER_EXPLODE:
			if (!Endlevel_sequence) multi_do_player_explode(buf); break;
		case MULTI_MESSAGE:
			if (!Endlevel_sequence) multi_do_message(buf); break;
		case MULTI_OBS_MESSAGE:
			if (!Endlevel_sequence) multi_do_obs_message(buf); break;
		case MULTI_QUIT:
			if (!Endlevel_sequence) multi_do_quit(buf); break;
		case MULTI_BEGIN_SYNC:
			break;
		case MULTI_CONTROLCEN:
			if (!Endlevel_sequence) multi_do_controlcen_destroy(buf); break;
		case MULTI_POWCAP_UPDATE:
			if (!Endlevel_sequence) multi_do_powcap_update(buf); break;
		case MULTI_SOUND_FUNCTION:
			multi_do_sound_function(buf); break;
		case MULTI_MARKER:
			if (!Endlevel_sequence) multi_do_drop_marker (buf); break;
		case MULTI_DROP_WEAPON:
			if (!Endlevel_sequence) multi_do_drop_weapon(buf); break;
		case MULTI_DROP_FLAG:
			if (!Endlevel_sequence) multi_do_drop_flag(buf); break;
		case MULTI_GUIDED:
			if (!Endlevel_sequence) multi_do_guided (buf); break;
		case MULTI_STOLEN_ITEMS:
			if (!Endlevel_sequence) multi_do_stolen_items(buf); break;
		case MULTI_WALL_STATUS:
			if (!Endlevel_sequence) multi_do_wall_status(buf); break;
		case MULTI_HEARTBEAT:
			if (!Endlevel_sequence) multi_do_heartbeat (buf); break;
		case MULTI_SEISMIC:
			if (!Endlevel_sequence) multi_do_seismic (buf); break;
		case MULTI_LIGHT:
			if (!Endlevel_sequence) multi_do_light (buf); break;
		case MULTI_KILLGOALS:
			if (!Endlevel_sequence) multi_do_kill_goal_counts (buf); break;
		case MULTI_ENDLEVEL_START:
			if (!Endlevel_sequence) multi_do_escape(buf); break;
		case MULTI_END_SYNC:
			break;
		case MULTI_CLOAK:
			if (!Endlevel_sequence) multi_do_cloak(buf); break;
		case MULTI_DECLOAK:
			if (!Endlevel_sequence) multi_do_decloak(buf); break;
		case MULTI_INVULN:
			if (!Endlevel_sequence) multi_do_invuln(buf); break;
		case MULTI_DOOR_OPEN:
			if (!Endlevel_sequence) multi_do_door_open(buf); break;
		case MULTI_CREATE_EXPLOSION:
			if (!Endlevel_sequence) multi_do_create_explosion(buf); break;
		case MULTI_CONTROLCEN_FIRE:
			if (!Endlevel_sequence) multi_do_controlcen_fire(buf); break;
		case MULTI_CREATE_POWERUP:
			if (!Endlevel_sequence) multi_do_create_powerup(buf); break;
		case MULTI_PLAY_SOUND:
			if (!Endlevel_sequence) multi_do_play_sound(buf); break;
		case MULTI_CAPTURE_BONUS:
			if (!Endlevel_sequence) multi_do_capture_bonus(buf); break;
		case MULTI_ORB_BONUS:
			if (!Endlevel_sequence) multi_do_orb_bonus(buf); break;
		case MULTI_GOT_FLAG:
			if (!Endlevel_sequence) multi_do_got_flag(buf); break;
		case MULTI_GOT_ORB:
			if (!Endlevel_sequence) multi_do_got_orb(buf); break;
		case MULTI_PLAY_BY_PLAY:
			if (!Endlevel_sequence) multi_do_play_by_play(buf); break;
		case MULTI_RANK:
			if (!Endlevel_sequence) multi_do_ranking (buf); break;
		case MULTI_FINISH_GAME:
			multi_do_finish_game(buf); break;  // do this one regardless of endsequence
		case MULTI_ROBOT_CONTROLS:
			break;
		case MULTI_ROBOT_CLAIM:
			if (!Endlevel_sequence) multi_do_claim_robot(buf); break;
		case MULTI_ROBOT_POSITION:
			if (!Endlevel_sequence) multi_do_robot_position(buf); break;
		case MULTI_ROBOT_EXPLODE:
			if (!Endlevel_sequence) multi_do_robot_explode(buf); break;
		case MULTI_ROBOT_RELEASE:
			if (!Endlevel_sequence) multi_do_release_robot(buf); break;
		case MULTI_ROBOT_FIRE:
			if (!Endlevel_sequence) multi_do_robot_fire(buf); break;
		case MULTI_RESPAWN_ROBOT:
			if(! Endlevel_sequence) multi_do_respawn_robot(buf); break;
		case MULTI_SCORE:
			if (!Endlevel_sequence) multi_do_score(buf); break;
		case MULTI_CREATE_ROBOT:
			if (!Endlevel_sequence) multi_do_create_robot(buf); break;
		case MULTI_TRIGGER:
			if (!Endlevel_sequence) multi_do_trigger(buf); break;
		case MULTI_START_TRIGGER:
			if (!Endlevel_sequence) multi_do_start_trigger(buf); break;
		case MULTI_FLAGS:
			if (!Endlevel_sequence) multi_do_flags(buf); break;
		case MULTI_DROP_BLOB:
			if (!Endlevel_sequence) multi_do_drop_blob(buf); break;
		case MULTI_ACTIVE_DOOR:
			if (!Endlevel_sequence) multi_do_active_door(buf); break;
		case MULTI_BOSS_ACTIONS:
			if (!Endlevel_sequence) multi_do_boss_actions(buf); break;
		case MULTI_CREATE_ROBOT_POWERUPS:
			if (!Endlevel_sequence) multi_do_create_robot_powerups(buf); break;
		case MULTI_HOSTAGE_DOOR:
			if (!Endlevel_sequence) multi_do_hostage_door_status(buf); break;
		case MULTI_SAVE_GAME:
			if (!Endlevel_sequence) multi_do_save_game(buf); break;
		case MULTI_RESTORE_GAME:
			if (!Endlevel_sequence) multi_do_restore_game(buf); break;
		case MULTI_DO_BOUNTY:
			if( !Endlevel_sequence ) multi_do_bounty( buf ); break;
		case MULTI_TYPING_STATE:
			multi_do_msgsend_state( buf ); break;
		case MULTI_GMODE_UPDATE:
			multi_do_gmode_update( buf ); break;
		case MULTI_KILL_HOST:
			multi_do_kill(buf); break;
		case MULTI_KILL_CLIENT:
			multi_do_kill(buf); break;
		case MULTI_OBS_UPDATE:
			multi_do_obs_update(buf); break;
		case MULTI_DAMAGE:
			multi_do_damage(buf); break;
		case MULTI_REPAIR:
			multi_do_repair(buf); break;
		case MULTI_SHIP_STATUS:
			multi_do_ship_status(buf); break;
		case MULTI_CREATE_EXPLOSION2:
			multi_do_create_explosion2(buf); break;
		default:
			Int3();
	}
}

// Following functions convert object to object_rw and back.
// turn object to object_rw for sending
void multi_object_to_object_rw(object *obj, object_rw *obj_rw)
{
	obj_rw->signature     = obj->signature;
	obj_rw->type          = obj->type;
	obj_rw->id            = obj->id;
	obj_rw->next          = obj->next;
	obj_rw->prev          = obj->prev;
	obj_rw->control_type  = obj->control_type;
	obj_rw->movement_type = obj->movement_type;
	obj_rw->render_type   = obj->render_type;
	obj_rw->flags         = obj->flags;
	obj_rw->segnum        = obj->segnum;
	obj_rw->attached_obj  = obj->attached_obj;
	obj_rw->pos.x         = obj->pos.x;
	obj_rw->pos.y         = obj->pos.y;
	obj_rw->pos.z         = obj->pos.z;
	obj_rw->orient.rvec.x = obj->orient.rvec.x;
	obj_rw->orient.rvec.y = obj->orient.rvec.y;
	obj_rw->orient.rvec.z = obj->orient.rvec.z;
	obj_rw->orient.fvec.x = obj->orient.fvec.x;
	obj_rw->orient.fvec.y = obj->orient.fvec.y;
	obj_rw->orient.fvec.z = obj->orient.fvec.z;
	obj_rw->orient.uvec.x = obj->orient.uvec.x;
	obj_rw->orient.uvec.y = obj->orient.uvec.y;
	obj_rw->orient.uvec.z = obj->orient.uvec.z;
	obj_rw->size          = obj->size;
	obj_rw->shields       = obj->shields;
	obj_rw->last_pos.x    = obj->last_pos.x;
	obj_rw->last_pos.y    = obj->last_pos.y;
	obj_rw->last_pos.z    = obj->last_pos.z;
	obj_rw->contains_type = obj->contains_type;
	obj_rw->contains_id   = obj->contains_id;
	obj_rw->contains_count= obj->contains_count;
	obj_rw->matcen_creator= obj->matcen_creator;
	obj_rw->lifeleft      = obj->lifeleft;
	
	switch (obj_rw->movement_type)
	{
		case MT_PHYSICS:
			obj_rw->mtype.phys_info.velocity.x  = obj->mtype.phys_info.velocity.x;
			obj_rw->mtype.phys_info.velocity.y  = obj->mtype.phys_info.velocity.y;
			obj_rw->mtype.phys_info.velocity.z  = obj->mtype.phys_info.velocity.z;
			obj_rw->mtype.phys_info.thrust.x    = obj->mtype.phys_info.thrust.x;
			obj_rw->mtype.phys_info.thrust.y    = obj->mtype.phys_info.thrust.y;
			obj_rw->mtype.phys_info.thrust.z    = obj->mtype.phys_info.thrust.z;
			obj_rw->mtype.phys_info.mass        = obj->mtype.phys_info.mass;
			obj_rw->mtype.phys_info.drag        = obj->mtype.phys_info.drag;
			obj_rw->mtype.phys_info.brakes      = obj->mtype.phys_info.brakes;
			obj_rw->mtype.phys_info.rotvel.x    = obj->mtype.phys_info.rotvel.x;
			obj_rw->mtype.phys_info.rotvel.y    = obj->mtype.phys_info.rotvel.y;
			obj_rw->mtype.phys_info.rotvel.z    = obj->mtype.phys_info.rotvel.z;
			obj_rw->mtype.phys_info.rotthrust.x = obj->mtype.phys_info.rotthrust.x;
			obj_rw->mtype.phys_info.rotthrust.y = obj->mtype.phys_info.rotthrust.y;
			obj_rw->mtype.phys_info.rotthrust.z = obj->mtype.phys_info.rotthrust.z;
			obj_rw->mtype.phys_info.turnroll    = obj->mtype.phys_info.turnroll;
			obj_rw->mtype.phys_info.flags       = obj->mtype.phys_info.flags;
			break;
			
		case MT_SPINNING:
			obj_rw->mtype.spin_rate.x = obj->mtype.spin_rate.x;
			obj_rw->mtype.spin_rate.y = obj->mtype.spin_rate.y;
			obj_rw->mtype.spin_rate.z = obj->mtype.spin_rate.z;
			break;
	}
	
	switch (obj_rw->control_type)
	{
		case CT_WEAPON:
			obj_rw->ctype.laser_info.parent_type      = obj->ctype.laser_info.parent_type;
			obj_rw->ctype.laser_info.parent_num       = obj->ctype.laser_info.parent_num;
			obj_rw->ctype.laser_info.parent_signature = obj->ctype.laser_info.parent_signature;
			if (obj->ctype.laser_info.creation_time - GameTime64 < F1_0*(-18000))
				obj_rw->ctype.laser_info.creation_time = F1_0*(-18000);
			else
				obj_rw->ctype.laser_info.creation_time = obj->ctype.laser_info.creation_time - GameTime64;
			obj_rw->ctype.laser_info.last_hitobj      = obj->ctype.laser_info.last_hitobj;
			obj_rw->ctype.laser_info.track_goal       = obj->ctype.laser_info.track_goal;
			obj_rw->ctype.laser_info.multiplier       = obj->ctype.laser_info.multiplier;
			break;
			
		case CT_EXPLOSION:
			obj_rw->ctype.expl_info.spawn_time    = obj->ctype.expl_info.spawn_time;
			obj_rw->ctype.expl_info.delete_time   = obj->ctype.expl_info.delete_time;
			obj_rw->ctype.expl_info.delete_objnum = obj->ctype.expl_info.delete_objnum;
			obj_rw->ctype.expl_info.attach_parent = obj->ctype.expl_info.attach_parent;
			obj_rw->ctype.expl_info.prev_attach   = obj->ctype.expl_info.prev_attach;
			obj_rw->ctype.expl_info.next_attach   = obj->ctype.expl_info.next_attach;
			break;
			
		case CT_AI:
		{
			int i;
			obj_rw->ctype.ai_info.behavior               = obj->ctype.ai_info.behavior; 
			for (i = 0; i < MAX_AI_FLAGS; i++)
				obj_rw->ctype.ai_info.flags[i]       = obj->ctype.ai_info.flags[i]; 
			obj_rw->ctype.ai_info.hide_segment           = obj->ctype.ai_info.hide_segment;
			obj_rw->ctype.ai_info.hide_index             = obj->ctype.ai_info.hide_index;
			obj_rw->ctype.ai_info.path_length            = obj->ctype.ai_info.path_length;
			obj_rw->ctype.ai_info.cur_path_index         = obj->ctype.ai_info.cur_path_index;
			obj_rw->ctype.ai_info.dying_sound_playing    = obj->ctype.ai_info.dying_sound_playing;
			obj_rw->ctype.ai_info.danger_laser_num       = obj->ctype.ai_info.danger_laser_num;
			obj_rw->ctype.ai_info.danger_laser_signature = obj->ctype.ai_info.danger_laser_signature;
			if (obj->ctype.ai_info.dying_start_time == 0) // if bot not dead, anything but 0 will kill it
				obj_rw->ctype.ai_info.dying_start_time = 0;
			else
				obj_rw->ctype.ai_info.dying_start_time = obj->ctype.ai_info.dying_start_time - GameTime64;
			break;
		}
			
		case CT_LIGHT:
			obj_rw->ctype.light_info.intensity = obj->ctype.light_info.intensity;
			break;
			
		case CT_POWERUP:
			obj_rw->ctype.powerup_info.count         = obj->ctype.powerup_info.count;
			if (obj->ctype.powerup_info.creation_time - GameTime64 < F1_0*(-18000))
				obj_rw->ctype.powerup_info.creation_time = F1_0*(-18000);
			else
				obj_rw->ctype.powerup_info.creation_time = obj->ctype.powerup_info.creation_time - GameTime64;
			obj_rw->ctype.powerup_info.flags         = obj->ctype.powerup_info.flags;
			break;
	}
	
	switch (obj_rw->render_type)
	{
		case RT_MORPH:
		case RT_POLYOBJ:
		case RT_NONE: // HACK below
		{
			int i;
			if (obj->render_type == RT_NONE && obj->type != OBJ_GHOST) // HACK: when a player is dead or not connected yet, clients still expect to get polyobj data - even if render_type == RT_NONE at this time.
				break;
			obj_rw->rtype.pobj_info.model_num                = obj->rtype.pobj_info.model_num;
			for (i=0;i<MAX_SUBMODELS;i++)
			{
				obj_rw->rtype.pobj_info.anim_angles[i].p = obj->rtype.pobj_info.anim_angles[i].p;
				obj_rw->rtype.pobj_info.anim_angles[i].b = obj->rtype.pobj_info.anim_angles[i].b;
				obj_rw->rtype.pobj_info.anim_angles[i].h = obj->rtype.pobj_info.anim_angles[i].h;
			}
			obj_rw->rtype.pobj_info.subobj_flags             = obj->rtype.pobj_info.subobj_flags;
			obj_rw->rtype.pobj_info.tmap_override            = obj->rtype.pobj_info.tmap_override;
			obj_rw->rtype.pobj_info.alt_textures             = obj->rtype.pobj_info.alt_textures;
			break;
		}
			
		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_FIREBALL:
			obj_rw->rtype.vclip_info.vclip_num = obj->rtype.vclip_info.vclip_num;
			obj_rw->rtype.vclip_info.frametime = obj->rtype.vclip_info.frametime;
			obj_rw->rtype.vclip_info.framenum  = obj->rtype.vclip_info.framenum;
			break;
			
		case RT_LASER:
			break;
			
	}
}

// turn object_rw to object after receiving
void multi_object_rw_to_object(object_rw *obj_rw, object *obj)
{
	obj->signature     = obj_rw->signature;
	obj->type          = obj_rw->type;
	obj->id            = obj_rw->id;
	obj->next          = obj_rw->next;
	obj->prev          = obj_rw->prev;
	obj->control_type  = obj_rw->control_type;
	obj->movement_type = obj_rw->movement_type;
	obj->render_type   = obj_rw->render_type;
	obj->flags         = obj_rw->flags;
	obj->segnum        = obj_rw->segnum;
	obj->attached_obj  = obj_rw->attached_obj;
	obj->pos.x         = obj_rw->pos.x;
	obj->pos.y         = obj_rw->pos.y;
	obj->pos.z         = obj_rw->pos.z;
	obj->orient.rvec.x = obj_rw->orient.rvec.x;
	obj->orient.rvec.y = obj_rw->orient.rvec.y;
	obj->orient.rvec.z = obj_rw->orient.rvec.z;
	obj->orient.fvec.x = obj_rw->orient.fvec.x;
	obj->orient.fvec.y = obj_rw->orient.fvec.y;
	obj->orient.fvec.z = obj_rw->orient.fvec.z;
	obj->orient.uvec.x = obj_rw->orient.uvec.x;
	obj->orient.uvec.y = obj_rw->orient.uvec.y;
	obj->orient.uvec.z = obj_rw->orient.uvec.z;
	obj->size          = obj_rw->size;
	obj->shields       = obj_rw->shields;
	obj->last_pos.x    = obj_rw->last_pos.x;
	obj->last_pos.y    = obj_rw->last_pos.y;
	obj->last_pos.z    = obj_rw->last_pos.z;
	obj->contains_type = obj_rw->contains_type;
	obj->contains_id   = obj_rw->contains_id;
	obj->contains_count= obj_rw->contains_count;
	obj->matcen_creator= obj_rw->matcen_creator;
	obj->lifeleft      = obj_rw->lifeleft;
	
	switch (obj->movement_type)
	{
		case MT_PHYSICS:
			obj->mtype.phys_info.velocity.x  = obj_rw->mtype.phys_info.velocity.x;
			obj->mtype.phys_info.velocity.y  = obj_rw->mtype.phys_info.velocity.y;
			obj->mtype.phys_info.velocity.z  = obj_rw->mtype.phys_info.velocity.z;
			obj->mtype.phys_info.thrust.x    = obj_rw->mtype.phys_info.thrust.x;
			obj->mtype.phys_info.thrust.y    = obj_rw->mtype.phys_info.thrust.y;
			obj->mtype.phys_info.thrust.z    = obj_rw->mtype.phys_info.thrust.z;
			obj->mtype.phys_info.mass        = obj_rw->mtype.phys_info.mass;
			obj->mtype.phys_info.drag        = obj_rw->mtype.phys_info.drag;
			obj->mtype.phys_info.brakes      = obj_rw->mtype.phys_info.brakes;
			obj->mtype.phys_info.rotvel.x    = obj_rw->mtype.phys_info.rotvel.x;
			obj->mtype.phys_info.rotvel.y    = obj_rw->mtype.phys_info.rotvel.y;
			obj->mtype.phys_info.rotvel.z    = obj_rw->mtype.phys_info.rotvel.z;
			obj->mtype.phys_info.rotthrust.x = obj_rw->mtype.phys_info.rotthrust.x;
			obj->mtype.phys_info.rotthrust.y = obj_rw->mtype.phys_info.rotthrust.y;
			obj->mtype.phys_info.rotthrust.z = obj_rw->mtype.phys_info.rotthrust.z;
			obj->mtype.phys_info.turnroll    = obj_rw->mtype.phys_info.turnroll;
			obj->mtype.phys_info.flags       = obj_rw->mtype.phys_info.flags;
			break;
			
		case MT_SPINNING:
			obj->mtype.spin_rate.x = obj_rw->mtype.spin_rate.x;
			obj->mtype.spin_rate.y = obj_rw->mtype.spin_rate.y;
			obj->mtype.spin_rate.z = obj_rw->mtype.spin_rate.z;
			break;
	}
	
	switch (obj->control_type)
	{
		case CT_WEAPON:
			obj->ctype.laser_info.parent_type      = obj_rw->ctype.laser_info.parent_type;
			obj->ctype.laser_info.parent_num       = obj_rw->ctype.laser_info.parent_num;
			obj->ctype.laser_info.parent_signature = obj_rw->ctype.laser_info.parent_signature;
			obj->ctype.laser_info.creation_time    = obj_rw->ctype.laser_info.creation_time;
			obj->ctype.laser_info.last_hitobj      = obj_rw->ctype.laser_info.last_hitobj;
			obj->ctype.laser_info.track_goal       = obj_rw->ctype.laser_info.track_goal;
			obj->ctype.laser_info.multiplier       = obj_rw->ctype.laser_info.multiplier;
			obj->ctype.laser_info.creation_framecount = 0;
			break;
			
		case CT_EXPLOSION:
			obj->ctype.expl_info.spawn_time    = obj_rw->ctype.expl_info.spawn_time;
			obj->ctype.expl_info.delete_time   = obj_rw->ctype.expl_info.delete_time;
			obj->ctype.expl_info.delete_objnum = obj_rw->ctype.expl_info.delete_objnum;
			obj->ctype.expl_info.attach_parent = obj_rw->ctype.expl_info.attach_parent;
			obj->ctype.expl_info.prev_attach   = obj_rw->ctype.expl_info.prev_attach;
			obj->ctype.expl_info.next_attach   = obj_rw->ctype.expl_info.next_attach;
			break;
			
		case CT_AI:
		{
			int i;
			obj->ctype.ai_info.behavior               = obj_rw->ctype.ai_info.behavior; 
			for (i = 0; i < MAX_AI_FLAGS; i++)
				obj->ctype.ai_info.flags[i]       = obj_rw->ctype.ai_info.flags[i]; 
			obj->ctype.ai_info.hide_segment           = obj_rw->ctype.ai_info.hide_segment;
			obj->ctype.ai_info.hide_index             = obj_rw->ctype.ai_info.hide_index;
			obj->ctype.ai_info.path_length            = obj_rw->ctype.ai_info.path_length;
			obj->ctype.ai_info.cur_path_index         = obj_rw->ctype.ai_info.cur_path_index;
			obj->ctype.ai_info.dying_sound_playing    = obj_rw->ctype.ai_info.dying_sound_playing;
			obj->ctype.ai_info.danger_laser_num       = obj_rw->ctype.ai_info.danger_laser_num;
			obj->ctype.ai_info.danger_laser_signature = obj_rw->ctype.ai_info.danger_laser_signature;
			obj->ctype.ai_info.dying_start_time       = obj_rw->ctype.ai_info.dying_start_time;
			break;
		}
			
		case CT_LIGHT:
			obj->ctype.light_info.intensity = obj_rw->ctype.light_info.intensity;
			break;
			
		case CT_POWERUP:
			obj->ctype.powerup_info.count         = obj_rw->ctype.powerup_info.count;
			obj->ctype.powerup_info.creation_time = obj_rw->ctype.powerup_info.creation_time;
			obj->ctype.powerup_info.flags         = obj_rw->ctype.powerup_info.flags;
			break;
		case CT_CNTRLCEN:
			if (obj->type != OBJ_GHOST) // don't bother getting gunpoints for deleted reactors (e.g. boss levels)
			{
				// gun points of reactor now part of the object but of course not saved in object_rw. Let's just recompute them.
				int i = 0;
				reactor *reactor = get_reactor_definition(obj->id);
				for (i=0; i<reactor->n_guns; i++)
					calc_controlcen_gun_point(reactor, obj, i);
			}
			break;
	}
	
	switch (obj->render_type)
	{
		case RT_MORPH:
		case RT_POLYOBJ:
		case RT_NONE: // HACK below
		{
			int i;
			if (obj->render_type == RT_NONE && obj->type != OBJ_GHOST) // HACK: when a player is dead or not connected yet, clients still expect to get polyobj data - even if render_type == RT_NONE at this time.
				break;
			obj->rtype.pobj_info.model_num                = obj_rw->rtype.pobj_info.model_num;
			for (i=0;i<MAX_SUBMODELS;i++)
			{
				obj->rtype.pobj_info.anim_angles[i].p = obj_rw->rtype.pobj_info.anim_angles[i].p;
				obj->rtype.pobj_info.anim_angles[i].b = obj_rw->rtype.pobj_info.anim_angles[i].b;
				obj->rtype.pobj_info.anim_angles[i].h = obj_rw->rtype.pobj_info.anim_angles[i].h;
			}
			obj->rtype.pobj_info.subobj_flags             = obj_rw->rtype.pobj_info.subobj_flags;
			obj->rtype.pobj_info.tmap_override            = obj_rw->rtype.pobj_info.tmap_override;
			obj->rtype.pobj_info.alt_textures             = obj_rw->rtype.pobj_info.alt_textures;
			break;
		}
			
		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_FIREBALL:
			obj->rtype.vclip_info.vclip_num = obj_rw->rtype.vclip_info.vclip_num;
			obj->rtype.vclip_info.frametime = obj_rw->rtype.vclip_info.frametime;
			obj->rtype.vclip_info.framenum  = obj_rw->rtype.vclip_info.framenum;
			break;
			
		case RT_LASER:
			break;
			
	}
}

// returns 1 if changed
int multi_change_weapon_info(void)
{
	int changed = 0;
	if (!(Game_mode & GM_MULTI))
		return 0;
	if (Netgame.DisableGaussSplash) {
		Weapon_info[GAUSS_ID].mass = F1_0 / 100;
		Weapon_info[GAUSS_ID].damage_radius = 0;
		for (int i = 0; i < NDL; i++)
			Weapon_info[GAUSS_ID].strength[i] = i2f(12) + F0_5;
		changed = 1;
	}
	return changed;
}

void
multi_send_create_explosion2(int segnum, vms_vector *pos, fix size, int type)
{
	if(is_observer()) { return; }

	// Create an explosion on a remote machine

#ifdef WORDS_BIGENDIAN
	vms_vector swapped_vec;
#endif
	int count = 0;

	multibuf[count] = MULTI_CREATE_EXPLOSION2; count += 1;
	multibuf[count] = Player_num;              count += 1;
	PUT_INTEL_SHORT(multibuf+count, segnum );  count += 2;
#ifndef WORDS_BIGENDIAN
	memcpy(multibuf+count, pos, sizeof(vms_vector));  count += sizeof(vms_vector);
#else
	swapped_vec.x = (fix)INTEL_INT( (int)pos->x );
	swapped_vec.y = (fix)INTEL_INT( (int)pos->y );
	swapped_vec.z = (fix)INTEL_INT( (int)pos->z );
	memcpy(multibuf+count, &swapped_vec, 12); count += 12;
#endif
	PUT_INTEL_INT(multibuf+count, size );     count += 4;
	PUT_INTEL_INT(multibuf+count, type );     count += 4;

	multi_send_data(multibuf, count, 2);
}

void
multi_do_create_explosion2(const ubyte *buf)
{
	short segnum;
	int pnum;
	int count = 1;
	vms_vector new_pos;
	fix size;
	int type;

	pnum = buf[count++];
	segnum = GET_INTEL_SHORT(buf + count); count += 2;

	if ((segnum < 0) || (segnum > Highest_segment_index)) {
		Int3();
		return;
	}

	new_pos = *(vms_vector *)(buf+count); count+=sizeof(vms_vector);

#ifdef WORDS_BIGENDIAN
	new_pos.x = (fix)SWAPINT((int)new_pos.x);
	new_pos.y = (fix)SWAPINT((int)new_pos.y);
	new_pos.z = (fix)SWAPINT((int)new_pos.z);
#endif

	size = GET_INTEL_INT(buf + count); count += 4;
	type = GET_INTEL_INT(buf + count); count += 4;

	object_create_explosion(segnum, &new_pos, size, type);
}

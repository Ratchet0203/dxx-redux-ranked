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
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Structure information for the player
 *
 */



#ifndef _PLAYER_H
#define _PLAYER_H

#include "inferno.h"
#include "fix.h"
#include "vecmat.h"
#include "weapon.h"
#include "fuelcen.h"
#include "wall.h"
#include "mission.h"

#define MAX_PLAYERS 8
#define OBSERVER_PLAYER_ID 7
#define MAX_MULTI_PLAYERS MAX_PLAYERS+3
#define MAX_OBSERVERS 16

// Initial player stat values
#define INITIAL_ENERGY  i2f(100)    // 100% energy to start
#define INITIAL_SHIELDS i2f(100)    // 100% shields to start

#define MAX_ENERGY      i2f(200)    // go up to 200
#define MAX_SHIELDS     i2f(200)

#define INITIAL_LIVES               3   // start off with 3 lives

// Values for special flags
#define PLAYER_FLAGS_INVULNERABLE   1       // Player is invincible
#define PLAYER_FLAGS_BLUE_KEY       2       // Player has blue key
#define PLAYER_FLAGS_RED_KEY        4       // Player has red key
#define PLAYER_FLAGS_GOLD_KEY       8       // Player has gold key
#define PLAYER_FLAGS_MAP_ALL        64      // Player can see unvisited areas on map
#define PLAYER_FLAGS_QUAD_LASERS    1024    // Player shoots 4 at once
#define PLAYER_FLAGS_CLOAKED        2048    // Player is cloaked for awhile

#define CALLSIGN_LEN                8       // so can use as filename (was: 12)

// Amount of time player is cloaked.
#define CLOAK_TIME_MAX          (F1_0*30)
#define INVULNERABLE_TIME_MAX   (F1_0*30)

#define PLAYER_STRUCT_VERSION 	17		//increment this every time player struct changes

// defines for teams
#define TEAM_BLUE   0
#define TEAM_RED    1

//When this structure changes, increment the constant SAVE_FILE_VERSION
//in playsave.c
typedef struct player {
	// Who am I data
	char    callsign[CALLSIGN_LEN+1];   // The callsign of this player, for net purposes.
	ubyte   net_address[6];         // The network address of the player.
	sbyte   connected;              // Is the player connected or not?
	int     objnum;                 // What object number this player is. (made an int by mk because it's very often referenced)
	int     n_packets_got;          // How many packets we got from them
	int     n_packets_sent;         // How many packets we sent to them

	//  -- make sure you're 4 byte aligned now!

	// Game data
	uint    flags;                  // Powerup flags, see below...
	fix     energy;                 // Amount of energy remaining.
	fix     shields;                // shields remaining (protection)
	fix     shields_delta;          // recent change to shields
	fix     shields_time;           // the time the shields recently changed
	sbyte   shields_time_hours;     // the time the shields recently changed (hours)
	ubyte   shields_certain;        // Whether or not the observer is certain about the player's shield value.
	ubyte   lives;                  // Lives remaining, 0 = game over.
	sbyte   level;                  // Current level player is playing. (must be signed for secret levels)
	ubyte   laser_level;            // Current level of the laser.
	sbyte   starting_level;         // What level the player started on.
	short   killer_objnum;          // Who killed me.... (-1 if no one)
	ubyte		primary_weapon_flags;					//	bit set indicates the player has this weapon.
	ubyte		secondary_weapon_flags;					//	bit set indicates the player has this weapon.
	ushort  primary_ammo[MAX_PRIMARY_WEAPONS]; // How much ammo of each type.
	ushort  secondary_ammo[MAX_SECONDARY_WEAPONS]; // How much ammo of each type.
	sbyte	primary_weapon;			// The currently selected primary weapon.
	sbyte	secondary_weapon;		// The currently selected secondary weapon.

	//  -- make sure you're 4 byte aligned now

	// Statistics...
	int     last_score;             // Score at beginning of current level.
	int     score;                  // Current score.
	fix     time_level;             // Level time played
	fix     time_total;             // Game time played (high word = seconds)

	fix64   cloak_time;             // Time cloaked
	fix64   invulnerable_time;      // Time invulnerable

	short   KillGoalCount;          // Num of players killed this level
	short   net_killed_total;       // Number of times killed total
	short   net_kills_total;        // Number of net kills total
	short   num_kills_level;        // Number of kills this level
	short   num_kills_total;        // Number of kills total
	short   num_robots_level;       // Number of initial robots this level
	short   num_robots_total;       // Number of robots total
	ushort  hostages_rescued_total; // Total number of hostages rescued.
	ushort  hostages_total;         // Total number of hostages.
	ubyte   hostages_on_board;      // Number of hostages on ship.
	ubyte   hostages_level;         // Number of hostages on this level.
	fix     homing_object_dist;     // Distance of nearest homing object.
	sbyte   hours_level;            // Hours played (since time_total can only go up to 9 hours)
	sbyte   hours_total;            // Hours played (since time_total can only go up to 9 hours)
} __pack__ player;

typedef struct ranking { // This struct contains variables for the ranking system mod. Most of them don't have to be doubles. It's either for math compatibility or consistency.
	double     deathCount;                 // Number of times the player died during the level.
	double     rankScore;                  // The version of score used for this mod, as to not disturb the vanilla score system.
	double     excludePoints;              // Number of points gotten from sources we still want to contribute to vanilla score, but not count toward rank calculation.
	double     maxScore;                   // The current level's S-rank score. Won't include hostage points until the result screen, to make calculation of other bonuses easier.
	double     level_time;                 // Time variable used in rank calculation. Updates to match Players[Player_num].time_level at specific points to protect players from being penalized for not skipping things.
	int        quickload;                  // Whether the player has quickloaded into the current level.
	double     parTime;                    // The algorithmically-generated required time for the current level.
	double     calculatedScore;            // Stores the score determined in calculateRank.
	int        rank;                       // Stores the rank determined in calculateRank.
	double     missedRngSpawn;             // Tracks the points from randomly-dropped robots that were ignored by the player, so they're subtracted at the end.
	int        alreadyBeaten;              // Tracks whether the current level has been beaten before, so points remaining and par time HUD elements are not shown on a new level.
	int		   fromBestRanksButton;        // Tracks whether the mission list was accessed from the best ranks button for not, to know whether to show aggregate ranks and allow record deleting. 0 means no, 1 means yes.
	int        startingLevel;              // I hate making a ranking variable for this, but endlevel_handler doesn't support a level_num parameter due to the way it's called, so I have to use this for when levels are started from the record details screen.
	int        lastSelectedItem;           // So the best ranks listbox doesn't keep putting you back at level 1 when you're retrying stuff. (Edit: This was back when the restart level button didn't exist, so it's a lot less important now but still a bit useful.)
	int        missionRanks[MAX_MISSIONS]; // A struct for the aggregate ranks on the missions list because the userdata field for the list is already used by something.
	int        warmStart;                  // If the player enters a level with anything non-default, this becomes 1. If this is 1 when a new record is set, the score will be marked as warm started, and won't be visible if their display is disabled.
	double     freezeTimer;                // Tells levels' in-game timer whether it should be frozen or not (this is in D1 to keep par time text from turning red during the exit cutscene).
	int        noDamage;                   // A new bonus I had to add. Thanks Marvin... 4/22/2025
	int        isRankable;                 // Tells the mod whether to give an automatic X-rank on levels where a result screen can't be reached.
} __pack__ ranking;

typedef struct restartLevel { // Recreate and store certain info from player to be restored when the restart button is hit, so the player is properly reset.
	uint    flags;
	fix     energy;
	fix     shields;
	ubyte   lives;
	ubyte   laser_level;
	sbyte   primary_weapon;
	sbyte   secondary_weapon;
	ushort  primary_weapon_flags;
	ushort  secondary_weapon_flags;
	ushort  primary_ammo[MAX_PRIMARY_WEAPONS];
	ushort  secondary_ammo[MAX_PRIMARY_WEAPONS];
	int     Missile_gun;
	int     restarts; // Used for the restart counter, and whether to skip briefings or not.
	int     restartsCache; // Just one of the many hoops needed to keep the restart counter accurate when restarting at result screen.
	int     isResults; // So pressing R on a wide array of menus doesn't try to restart a level you aren't on.
	int     updateRestartStuff; // Self explanatory.
} __pack__ restartLevel;

// Same as above but structure how Savegames expect
typedef struct player_rw {
	// Who am I data
	char    callsign[CALLSIGN_LEN+1];   // The callsign of this player, for net purposes.
	ubyte   net_address[6];         // The network address of the player.
	sbyte   connected;              // Is the player connected or not?
	int     objnum;                 // What object number this player is. (made an int by mk because it's very often referenced)
	int     n_packets_got;          // How many packets we got from them
	int     n_packets_sent;         // How many packets we sent to them

	//  -- make sure you're 4 byte aligned now!

	// Game data
	uint    flags;                  // Powerup flags, see below...
	fix     energy;                 // Amount of energy remaining.
	fix     shields;                // shields remaining (protection)
	ubyte   lives;                  // Lives remaining, 0 = game over.
	sbyte   level;                  // Current level player is playing. (must be signed for secret levels)
	ubyte   laser_level;            // Current level of the laser.
	sbyte   starting_level;         // What level the player started on.
	short   killer_objnum;          // Who killed me.... (-1 if no one)
	ubyte		primary_weapon_flags;					//	bit set indicates the player has this weapon.
	ubyte		secondary_weapon_flags;					//	bit set indicates the player has this weapon.
	ushort  primary_ammo[MAX_PRIMARY_WEAPONS]; // How much ammo of each type.
	ushort  secondary_ammo[MAX_SECONDARY_WEAPONS]; // How much ammo of each type.

	//	-- make sure you're 4 byte aligned now

	// Statistics...
	int     last_score;             // Score at beginning of current level.
	int     score;                  // Current score.
	fix     time_level;             // Level time played
	fix     time_total;             // Game time played (high word = seconds)

	fix     cloak_time;             // Time cloaked
	fix     invulnerable_time;      // Time invulnerable

	short   net_killed_total;       // Number of times killed total
	short   net_kills_total;        // Number of net kills total
	short   num_kills_level;        // Number of kills this level
	short   num_kills_total;        // Number of kills total
	short   num_robots_level;       // Number of initial robots this level
	short   num_robots_total;       // Number of robots total
	ushort  hostages_rescued_total; // Total number of hostages rescued.
	ushort  hostages_total;         // Total number of hostages.
	ubyte   hostages_on_board;      // Number of hostages on ship.
	ubyte   hostages_level;         // Number of hostages on this level.
	fix     homing_object_dist;     // Distance of nearest homing object.
	sbyte   hours_level;            // Hours played (since time_total can only go up to 9 hours)
	sbyte   hours_total;            // Hours played (since time_total can only go up to 9 hours)
} __pack__ player_rw;

#define N_PLAYER_GUNS 8
#define N_PLAYER_SHIP_TEXTURES 32

typedef struct player_ship {
	int     model_num;
	int     expl_vclip_num;
	fix     mass,drag;
	fix     max_thrust,reverse_thrust,brakes;       //low_thrust
	fix     wiggle;
	fix     max_rotthrust;
	vms_vector gun_points[N_PLAYER_GUNS];
} __pack__ player_ship;

extern int N_players;   // Number of players ( >1 means a net game, eh?)
extern int Player_num;  // The player number who is on the console.

extern player Players[MAX_PLAYERS];				// Misc player info
extern ranking Ranking; // Ranking system mod variables.
extern player_ship *Player_ship;

// Probably should go in player struct, but I don't want to break savegames for this
extern int RespawningConcussions[MAX_PLAYERS]; 
extern int VulcanAmmoBoxesOnBoard[MAX_PLAYERS];
extern int VulcanBoxAmmo[MAX_PLAYERS];

extern vms_vector Last_pos; // Saved position of observer prior to following a player.
extern vms_matrix Last_orient; // Saved orientation of observer prior to following a player.
extern fix Last_real_update; // How long ago in seconds the observed player's real position got updated.
extern vms_vector Real_pos; // The observed player's position.
extern vms_matrix Real_orient; // The observed player's orientation.

/*
 * reads a player_ship structure from a PHYSFS_file
 */
void player_ship_read(player_ship *ps, PHYSFS_file *fp);

void player_rw_swap(player_rw *p, int swap);

// Observer setting.
void reset_obs();
void set_obs(int pnum);

#endif
 

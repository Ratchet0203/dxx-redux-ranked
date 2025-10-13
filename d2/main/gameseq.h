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
 * Prototypes for functions for game sequencing.
 *
 */


#ifndef _GAMESEQ_H
#define _GAMESEQ_H

#include "player.h"
#include "mission.h"
#include "object.h"
#include "fuelcen.h"

#define SUPER_MISSILE       0
#define SUPER_SEEKER        1
#define SUPER_SMARTBOMB     2
#define SUPER_SHOCKWAVE     3
#define MAX_OBJECTIVES      MAX_OBJECTS + MAX_WALLS // The highest number of par time objectives a level can have.

#define LEVEL_NAME_LEN 36       //make sure this is multiple of 4!

typedef struct
{
	int type;
	int ID;
} partime_objective;

typedef struct parTime {
	double movementTime;
	double omittedMovementTime;
	partime_objective toDoList[MAX_OBJECTIVES]; // 10032 bytes
	int toDoListSize;
	partime_objective doneList[MAX_OBJECTIVES]; // 10032 bytes
	int doneListSize;
	vms_vector lastPosition; // Tracks the last place algo went to within the same segment.
	int matcenLives[MAX_ROBOT_CENTERS]; // We need to track how many times we trip matcens, since each one can only be tripped three times. 80 bytes
	double matcenTriggeredAt[MAX_ROBOT_CENTERS]; // So Algo respects the cooldown matcens have. 160 bytes
	// Time spent clearing matcens.
	double matcenTime;
	double energyTime;
	double simulatedEnergy; // What it sounds like.
	fix vulcanAmmo; // Also what it sounds like.
	// How much robot HP we've had to destroy to this point.
	double combatTime;
	// Info about the weapon algo currently has equipped.
	double energy_usage;
	double ammo_usage;
	double heldWeapons[36]; // Which weapons algo has. 288 bytes
	int laser_level; // It's possible to make things work without this, but just tracking laser level directly makes things a lot easier.
	int hasQuads;
	int segnum; // What segment Algo is in.
	bool isSegmentAccessible[MAX_SEGMENTS]; // 9000 bytes
	bool lockedWallsAt[MAX_OBJECTIVES][MAX_WALLS]; // 318516 bytes
	short segmentVisitedFrom[MAX_SEGMENTS][MAX_SIDES_PER_SEGMENT]; // 108000 bytes
	int loops; // Which stage of the par time calculation process are we on?
	int typeThreeWalls[MAX_WALLS];
	int numTypeThreeWalls;
	int typeThreeUnlockIDs[MAX_WALLS];
	bool shipFitsThroughSide[MAX_SEGMENTS][MAX_SIDES_PER_SEGMENT]; // So we can cache this and avoid having millions upon millions of vm_vec_dist calls in par time. 54000 bytes
	int warpBackPoint;
	ubyte thiefKeys;
	int missingKeys; // Tells Also which keys are missing from a level, allowing it to go through cooresponding colored doors to prevent softlocks.
	int objectives; // Keeps track of how many things we've gotten so far. Important for backtracking omission and energy stuff.
	int energy_gained_per_pickup;
	double objectiveEnergies[MAX_OBJECTIVES]; // For use in energy time calculation, which uses energy after each objective on a node graph. 10032 bytes
	double objectiveFuelcenTripTimes[MAX_OBJECTIVES]; // Gathers distance to nearest accessible fuelcen (if any) throughout the run for use in energy time. 5016 bytes
	double energyUsed;
	// In total, arrays take 527188 bytes, or about 515 KB, which stays under the usual minimum stack limit of 1 MB on modern machines, while still leaving plenty of room for any temp arrays needed throughout the par time calculation and level loading processes.
} __pack__ parTime;

// Current_level_num starts at 1 for the first level
// -1,-2,-3 are secret levels
// 0 means not a real level loaded
extern int Current_level_num, Next_level_num;
extern char Current_level_name[LEVEL_NAME_LEN];
extern obj_position Player_init[MAX_PLAYERS];
extern parTime ParTime; // Par time algorithm variables.


// This is the highest level the player has ever reached
extern int Player_highest_level;

//
// New game sequencing functions
//

// starts a new game on the given level
void StartNewGame(int start_level);

// starts the next level
void StartNewLevel(int level_num);

// Actually does the work to start new level
void StartNewLevelSub(int level_num, int page_in_textures, int secret_flag);

void InitPlayerObject();            //make sure player's object set up
void init_player_stats_game(ubyte pnum);      //clear all stats

// called when the player has finished a level
// if secret flag is true, advance to secret level, else next normal level
void PlayerFinishedLevel(int secret_flag);

// called when the player has died
void DoPlayerDead(void);

// load just the hxm file
void load_level_robots(int level_num);

// load a level off disk. level numbers start at 1.
// Secret levels are -1,-2,-3
void LoadLevel(int level_num, int page_in_textures);

extern void gameseq_remove_unused_players();

extern void update_player_stats();

// from scores.c

extern void show_high_scores(int place);
extern void draw_high_scores(int place);
extern int add_player_to_high_scores(player *pp);
extern void input_name (int place);
extern int reset_high_scores();
extern void init_player_stats_level(int secret_flag);

extern int calculateRank(int level_num, int update_warm_start_status);
extern int truncateRanks(int rank);
extern void getLevelNameFromRankFile(int level_num, char* buffer);

void open_message_window(void);
void close_message_window(void);

// create flash for player appearance
extern void create_player_appearance_effect(object *player_obj);

// goto whatever secrect level is appropriate given the current level
extern void goto_secret_level();

// reset stuff so game is semi-normal when playing from editor
void editor_reset_stuff_on_level();

// Show endlevel bonus scores
extern void DoEndLevelScoreGlitz(int network);
extern void DoEndSecretLevelScoreGlitz();
extern void DoBestRanksScoreGlitz(int level_num);

extern int thisWallUnlocked(int wall_num, int currentObjectiveType, int currentObjectiveID, int warpBackPointCheck);
extern int check_gap_size(int seg, int side);

// stuff for multiplayer
extern int NumNetPlayerPositions;

void bash_to_shield(int, char *);

#endif /* _GAMESEQ_H */

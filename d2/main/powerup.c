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
 * Code for powerup objects.
 *
 */


#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"
#include "dxxerror.h"
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "key.h"
#include "fireball.h"
#include "powerup.h"
#include "gauges.h"
#include "sounds.h"
#include "player.h"
#include "wall.h"
#include "text.h"
#include "weapon.h"
#include "laser.h"
#include "scores.h"
#include "multi.h"
#include "lighting.h"
#include "controls.h"
#include "kconfig.h"
#include "newdemo.h"
#include "escort.h"
#ifdef EDITOR
#include "gr.h"	//	for powerup outline drawing
#include "editor/editor.h"
#endif
#include "playsave.h"

int N_powerup_types = 0;
powerup_type_info Powerup_info[MAX_POWERUP_TYPES];

//process this powerup for this frame
void do_powerup_frame(object *obj)
{
	fix fudge;
	vclip_info *vci = &obj->rtype.vclip_info;
	vclip *vc = &Vclip[vci->vclip_num];

	fudge = (FrameTime * ((obj-Objects)&3)) >> 4;
	
	vci->frametime -= FrameTime+fudge;
	
	while (vci->frametime < 0 ) {

		vci->frametime += vc->frame_time;
		
		if ((obj-Objects)&1)
			vci->framenum--;
		else
			vci->framenum++;

		if (vci->framenum >= vc->num_frames)
			vci->framenum=0;

		if (vci->framenum < 0)
			vci->framenum = vc->num_frames-1;
	}

	if (obj->lifeleft <= 0) {
		object_create_explosion(obj->segnum, &obj->pos, F1_0*7/2, VCLIP_POWERUP_DISAPPEARANCE );

		if ( Vclip[VCLIP_POWERUP_DISAPPEARANCE].sound_num > -1 )
			digi_link_sound_to_object( Vclip[VCLIP_POWERUP_DISAPPEARANCE].sound_num, obj-Objects, 0, F1_0);
	}
}

#ifdef EDITOR
extern fix blob_vertices[];

//	blob_vertices has 3 vertices in it, 4th must be computed
void draw_blob_outline(void)
{
	fix	v3x, v3y;

	v3x = blob_vertices[4] - blob_vertices[2] + blob_vertices[0];
	v3y = blob_vertices[5] - blob_vertices[3] + blob_vertices[1];

	gr_setcolor(BM_XRGB(63, 63, 63));

	gr_line(blob_vertices[0], blob_vertices[1], blob_vertices[2], blob_vertices[3]);
	gr_line(blob_vertices[2], blob_vertices[3], blob_vertices[4], blob_vertices[5]);
	gr_line(blob_vertices[4], blob_vertices[5], v3x, v3y);

	gr_line(v3x, v3y, blob_vertices[0], blob_vertices[1]);
}
#endif

void draw_powerup(object *obj)
{
	#ifdef EDITOR
	blob_vertices[0] = 0x80000;
	#endif

	draw_object_blob(obj, Vclip[obj->rtype.vclip_info.vclip_num].frames[obj->rtype.vclip_info.framenum] );

	#ifdef EDITOR
	if (EditorWindow && (Cur_object_index == obj-Objects))
		if (blob_vertices[0] != 0x80000)
			draw_blob_outline();
	#endif

}

void powerup_basic(int redadd, int greenadd, int blueadd, int score, const char *format, ...)
{
	va_list	args;

	va_start(args, format );
	HUD_init_message_va(HM_DEFAULT, format, args);
	va_end(args);

	if (!(Game_mode & GM_MULTI) || !Netgame.ReducedFlash)
		PALETTE_FLASH_ADD(redadd,greenadd,blueadd);
}

//#ifndef RELEASE
//	Give the megawow powerup!
void do_megawow_powerup(int quantity)
{
	int i;

	powerup_basic(30, 0, 30, 1, "MEGA-WOWIE-ZOWIE!");
	Players[Player_num].primary_weapon_flags = 0xffff ^ HAS_FLAG(SUPER_LASER_INDEX);		//no super laser
	Players[Player_num].secondary_weapon_flags = 0xffff;

	for (i=0; i<MAX_PRIMARY_WEAPONS; i++)
		Players[Player_num].primary_ammo[i] = VULCAN_AMMO_MAX;

	for (i=0; i<3; i++)
		Players[Player_num].secondary_ammo[i] = quantity;

	for (i=3; i<MAX_SECONDARY_WEAPONS; i++)
		Players[Player_num].secondary_ammo[i] = quantity/5;

	if (Newdemo_state == ND_STATE_RECORDING)
		newdemo_record_laser_level(Players[Player_num].laser_level, MAX_LASER_LEVEL);

	Players[Player_num].energy = F1_0*200;
	Players[Player_num].shields = F1_0*200;
	Players[Player_num].flags |= PLAYER_FLAGS_QUAD_LASERS;
	Players[Player_num].laser_level = MAX_SUPER_LASER_LEVEL;

	if (Game_mode & GM_HOARD)
		Players[Player_num].secondary_ammo[PROXIMITY_INDEX] = 12;


	update_laser_weapon_info();

}
//#endif

int pick_up_energy(void)
{
	int	used=0;

	if (Players[Player_num].energy < MAX_ENERGY) {
		fix boost;
		boost = 3*F1_0 + 3*F1_0*(NDL - Difficulty_level);
		if (Difficulty_level == 0)
			boost += boost/2;
		Players[Player_num].energy += boost;
		if (Players[Player_num].energy > MAX_ENERGY)
			Players[Player_num].energy = MAX_ENERGY;
		powerup_basic(15,15,7, ENERGY_SCORE, "%s %s %d",TXT_ENERGY,TXT_BOOSTED_TO,f2i(Players[Player_num].energy));
		used=1;

		if (Game_mode & GM_MULTI)
			multi_send_ship_status();
	} else
		HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, TXT_MAXED_OUT,TXT_ENERGY);

	return used;
}

int pick_up_vulcan_ammo(void)
{
	int	used=0,max;

	int	pwsave = Players[Player_num].primary_weapon;		// Ugh, save selected primary weapon around the picking up of the ammo.  I apologize for this code.  Matthew A. Toschlog
	if (pick_up_ammo(CLASS_PRIMARY, VULCAN_INDEX, VULCAN_AMMO_AMOUNT)) {
		VulcanAmmoBoxesOnBoard[Player_num] += 1; 
		VulcanBoxAmmo[Player_num] += VULCAN_AMMO_AMOUNT;
		powerup_basic(7, 14, 21, VULCAN_AMMO_SCORE, "%s!", TXT_VULCAN_AMMO);
		used = 1;
	} else {
		max = Primary_ammo_max[VULCAN_INDEX];
		if (Players[Player_num].flags & PLAYER_FLAGS_AMMO_RACK)
			max *= 2;
		HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %d %s!",TXT_ALREADY_HAVE,f2i((unsigned) VULCAN_AMMO_SCALE * (unsigned) max),TXT_VULCAN_ROUNDS);
		used = 0;
	}
	Players[Player_num].primary_weapon = pwsave;

	return used;
}

extern void invalidate_escort_goal(void);
extern char GetKeyValue(char);
extern void check_to_use_primary(int);
extern void multi_send_got_flag (char);

//	returns true if powerup consumed
int do_powerup(object *obj)
{
	int used=0;
	int special_used=0;		//for when hitting vulcan cannon gets vulcan ammo
	int id=obj->id;

	if ((Player_is_dead) || (ConsoleObject->type == OBJ_GHOST) || (Players[Player_num].shields < 0))
		return 0;

	if ((obj->ctype.powerup_info.flags & PF_SPAT_BY_PLAYER) && obj->ctype.powerup_info.creation_time>0 && GameTime64<obj->ctype.powerup_info.creation_time+i2f(2))
		return 0;		//not enough time elapsed

	//if (Game_mode & GM_MULTI)
	//{
		/*
		 * The fact: Collecting a powerup is decided Client-side and due to PING it takes time for other players to know if one collected a powerup actually. This may lead to the case two players collect the same powerup!
		 * The solution: Let us check if someone else is closer to a powerup and if so, do not collect it.
		 * NOTE: Player positions computed by 'shortpos' and PING can still cause a small margin of error.
		 */
		 // CED -- causes more problems than it solves.  
		 /*
		int i = 0;
		vms_vector tvec;
		fix mydist = vm_vec_normalized_dir(&tvec, &obj->pos, &ConsoleObject->pos);

		for (i = 0; i < MAX_PLAYERS; i++)
		{
			if (i == Player_num || Players[i].connected != CONNECT_PLAYING)
				continue;
			if (Objects[Players[i].objnum].type == OBJ_GHOST || Players[i].shields < 0)
				continue;
			if (mydist > vm_vec_normalized_dir(&tvec, &obj->pos, &Objects[Players[i].objnum].pos))
				return 0;
		}
		*/
	//}

	switch (obj->id) {
		case POW_EXTRA_LIFE:
			Players[Player_num].lives++;
			powerup_basic(15, 15, 15, 0, "%s", TXT_EXTRA_LIFE);
			if (!(obj->flags & OF_OBJECT_DROPPED)) { // Random dropped 1ups won't give points.
				if (Current_level_num > 0) // 1up powerups will be worth 10k in the mod, since lives are worth 10k a piece on vanilla last level result screens.
					Ranking.excludePoints -= 10000;
				else
					Ranking.secretExcludePoints -= 10000;
				add_points_to_score(0);
			}
			used=1;
			break;
		case POW_ENERGY:
			used = pick_up_energy();
			break;
		case POW_SHIELD_BOOST:
			if (Players[Player_num].shields < MAX_SHIELDS) {
				fix boost = 3*F1_0 + 3*F1_0*(NDL - Difficulty_level);
				if (Difficulty_level == 0)
					boost += boost/2;

				if (Game_mode & GM_MULTI)
					multi_send_repair(boost, Players[Player_num].shields, OBJ_POWERUP);

				Players[Player_num].shields += boost;
				if (Players[Player_num].shields > MAX_SHIELDS)
					Players[Player_num].shields = MAX_SHIELDS;
				powerup_basic(0, 0, 15, SHIELD_SCORE, "%s %s %d",TXT_SHIELD,TXT_BOOSTED_TO,f2i(Players[Player_num].shields));
				if (Game_mode & GM_MULTI && PlayerCfg.MultiMessages)
					con_printf(CON_NORMAL, "You picked up %.1f shields, shields now %.1f\n", f2fl(boost), f2fl(Players[Player_num].shields));
				used=1;
			} else
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, TXT_MAXED_OUT,TXT_SHIELD);
			break;
		case POW_LASER:
			if (Players[Player_num].laser_level >= MAX_LASER_LEVEL) {
				//Players[Player_num].laser_level = MAX_LASER_LEVEL;
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, TXT_MAXED_OUT,TXT_LASER);
			} else {
				if (Newdemo_state == ND_STATE_RECORDING)
					newdemo_record_laser_level(Players[Player_num].laser_level, Players[Player_num].laser_level + 1);
				Players[Player_num].laser_level++;

				powerup_basic(10, 0, 10, LASER_SCORE, "%s %s %d",TXT_LASER,TXT_BOOSTED_TO, Players[Player_num].laser_level+1);
				update_laser_weapon_info();
				pick_up_primary (LASER_INDEX);
				used=1;

				if (Game_mode & GM_MULTI)
					multi_send_ship_status();
			}
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;
		case POW_MISSILE_1:
			used=pick_up_secondary(CONCUSSION_INDEX,1);
			break;
		case POW_MISSILE_4:
			used=pick_up_secondary(CONCUSSION_INDEX,4);
			break;

		case POW_KEY_BLUE:
			if (Players[Player_num].flags & PLAYER_FLAGS_BLUE_KEY)
				break;
#ifdef NETWORK
			multi_send_play_sound(Powerup_info[obj->id].hit_sound, F1_0);
#endif
			digi_play_sample( Powerup_info[obj->id].hit_sound, F1_0 );
			Players[Player_num].flags |= PLAYER_FLAGS_BLUE_KEY;
			powerup_basic(0, 0, 15, KEY_SCORE, "%s %s",TXT_BLUE,TXT_ACCESS_GRANTED);
			if (Game_mode & GM_MULTI)
				used=0;
			else
				used=1;
			invalidate_escort_goal();

			if (Game_mode & GM_MULTI)
				multi_send_ship_status();

			break;
		case POW_KEY_RED:
			if (Players[Player_num].flags & PLAYER_FLAGS_RED_KEY)
				break;
#ifdef NETWORK
			multi_send_play_sound(Powerup_info[obj->id].hit_sound, F1_0);
#endif
			digi_play_sample( Powerup_info[obj->id].hit_sound, F1_0 );
			Players[Player_num].flags |= PLAYER_FLAGS_RED_KEY;
			powerup_basic(15, 0, 0, KEY_SCORE, "%s %s",TXT_RED,TXT_ACCESS_GRANTED);
			if (Game_mode & GM_MULTI)
				used=0;
			else
				used=1;
			invalidate_escort_goal();

			if (Game_mode & GM_MULTI)
				multi_send_ship_status();

			break;
		case POW_KEY_GOLD:
			if (Players[Player_num].flags & PLAYER_FLAGS_GOLD_KEY)
				break;
#ifdef NETWORK
			multi_send_play_sound(Powerup_info[obj->id].hit_sound, F1_0);
#endif
			digi_play_sample( Powerup_info[obj->id].hit_sound, F1_0 );
			Players[Player_num].flags |= PLAYER_FLAGS_GOLD_KEY;
			powerup_basic(15, 15, 7, KEY_SCORE, "%s %s",TXT_YELLOW,TXT_ACCESS_GRANTED);
			if (Game_mode & GM_MULTI)
				used=0;
			else
				used=1;
			invalidate_escort_goal();

			if (Game_mode & GM_MULTI)
				multi_send_ship_status();

			break;
		case POW_QUAD_FIRE:
			if (!(Players[Player_num].flags & PLAYER_FLAGS_QUAD_LASERS)) {
				Players[Player_num].flags |= PLAYER_FLAGS_QUAD_LASERS;
				powerup_basic(15, 15, 7, QUAD_FIRE_SCORE, "%s!",TXT_QUAD_LASERS);
				update_laser_weapon_info();
				used=1;

				if (Game_mode & GM_MULTI)
					multi_send_ship_status();
			} else
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %s!",TXT_ALREADY_HAVE,TXT_QUAD_LASERS);
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;

		case	POW_VULCAN_WEAPON:
		case	POW_GAUSS_WEAPON: {
			int ammo = obj->ctype.powerup_info.count;

			used = pick_up_primary((obj->id==POW_VULCAN_WEAPON)?VULCAN_INDEX:GAUSS_INDEX);

			//didn't get the weapon (because we already have it), but
			//maybe snag some of the ammo.  if single-player, grab all the ammo
			//and remove the powerup.  If multi-player take ammo in excess of
			//the amount in a powerup, and leave the rest.
			if (! used)
				if ((Game_mode & GM_MULTI) ) {
					if(Netgame.GaussAmmoStyle == GAUSS_STYLE_DUPLICATING) {
						ammo -= VULCAN_AMMO_AMOUNT;
					} else {
						ammo = 0; // Forgot to tell other players we took ammo, it dups, very bad
					}
				}
			if (ammo > 0) {
				int ammo_used;
				ammo_used = pick_up_ammo(CLASS_PRIMARY, VULCAN_INDEX, ammo);
				obj->ctype.powerup_info.count -= ammo_used;
				if (!used && ammo_used) {
					powerup_basic(7, 14, 21, VULCAN_AMMO_SCORE, "%s!", TXT_VULCAN_AMMO);
					special_used = 1;
					id = POW_VULCAN_AMMO;		//set new id for making sound at end of this function
					if (obj->ctype.powerup_info.count == 0)
						used = 1;		//say used if all ammo taken
				}
			}

			break;
		}

		case	POW_SPREADFIRE_WEAPON:
			used = pick_up_primary(SPREADFIRE_INDEX);
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;
		case	POW_PLASMA_WEAPON:
			used = pick_up_primary(PLASMA_INDEX);
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;
		case	POW_FUSION_WEAPON:
			used = pick_up_primary(FUSION_INDEX);
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;

		case	POW_HELIX_WEAPON:
			used = pick_up_primary(HELIX_INDEX);
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;

		case	POW_PHOENIX_WEAPON:
			used = pick_up_primary(PHOENIX_INDEX);
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;

		case	POW_OMEGA_WEAPON:
			used = pick_up_primary(OMEGA_INDEX);
			if (used)
				Omega_charge = obj->ctype.powerup_info.count;
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;

		case	POW_PROXIMITY_WEAPON:
			used=pick_up_secondary(PROXIMITY_INDEX,4);
			break;
		case	POW_SMARTBOMB_WEAPON:
			used=pick_up_secondary(SMART_INDEX,1);
			break;
		case	POW_MEGA_WEAPON:
			used=pick_up_secondary(MEGA_INDEX,1);
			break;
		case	POW_SMISSILE1_1:
			used=pick_up_secondary(SMISSILE1_INDEX,1);
			break;
		case	POW_SMISSILE1_4:
			used=pick_up_secondary(SMISSILE1_INDEX,4);
			break;
		case	POW_GUIDED_MISSILE_1:
			used=pick_up_secondary(GUIDED_INDEX,1);
			break;
		case	POW_GUIDED_MISSILE_4:
			used=pick_up_secondary(GUIDED_INDEX,4);
			break;
		case	POW_SMART_MINE:
			used=pick_up_secondary(SMART_MINE_INDEX,4);
			break;
		case	POW_MERCURY_MISSILE_1:
			used=pick_up_secondary(SMISSILE4_INDEX,1);
			break;
		case	POW_MERCURY_MISSILE_4:
			used=pick_up_secondary(SMISSILE4_INDEX,4);
			break;
		case	POW_EARTHSHAKER_MISSILE:
			used=pick_up_secondary(SMISSILE5_INDEX,1);
			break;
		case	POW_VULCAN_AMMO:
			used = pick_up_vulcan_ammo();
			break;
		case	POW_HOMING_AMMO_1:
			used=pick_up_secondary(HOMING_INDEX,1);
			break;
		case	POW_HOMING_AMMO_4:
			used=pick_up_secondary(HOMING_INDEX,4);
			break;
		case	POW_CLOAK:
			if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %s!",TXT_ALREADY_ARE,TXT_CLOAKED);
				break;
			} else {
				Players[Player_num].cloak_time = GameTime64;	//	Not! changed by awareness events (like player fires laser).
				Players[Player_num].flags |= PLAYER_FLAGS_CLOAKED;
				ai_do_cloak_stuff();
				#ifdef NETWORK
				if (Game_mode & GM_MULTI)
					multi_send_cloak();
				#endif
				powerup_basic(-10,-10,-10, CLOAK_SCORE, "%s!",TXT_CLOAKING_DEVICE);
				used = 1;

				if (Game_mode & GM_MULTI)
					multi_send_ship_status();

				break;
			}
		case	POW_INVULNERABILITY:
			if (Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE) {
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %s!",TXT_ALREADY_ARE,TXT_INVULNERABLE);
				break;
			} else {
				Players[Player_num].invulnerable_time = GameTime64;
				Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
				#ifdef NETWORK
				if (Game_mode & GM_MULTI)
					multi_send_invuln();
				#endif
				powerup_basic(7, 14, 21, INVULNERABILITY_SCORE, "%s!",TXT_INVULNERABILITY);
				used = 1;

				if (Game_mode & GM_MULTI)
					multi_send_ship_status();

				break;
			}
	#ifndef RELEASE
		case	POW_MEGAWOW:
			do_megawow_powerup(50);
			used = 1;
			break;
	#endif

		case POW_FULL_MAP:
			if (Players[Player_num].flags & PLAYER_FLAGS_MAP_ALL) {
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %s!",TXT_ALREADY_HAVE,"the FULL MAP");
				if (!(Game_mode & GM_MULTI) )
					used = pick_up_energy();
			} else {
				Players[Player_num].flags |= PLAYER_FLAGS_MAP_ALL;
				powerup_basic(15, 0, 15, 0, "FULL MAP!");
				used=1;
			}
			break;

		case POW_CONVERTER:
			if (Players[Player_num].flags & PLAYER_FLAGS_CONVERTER) {
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %s!",TXT_ALREADY_HAVE,"the Converter");
				if (!(Game_mode & GM_MULTI) )
					used = pick_up_energy();
			} else {
				Players[Player_num].flags |= PLAYER_FLAGS_CONVERTER;
			    	powerup_basic(15, 0, 15, 0, "Energy -> shield converter!");


				used=1;
			}
			break;

		case POW_SUPER_LASER:
			if (Players[Player_num].laser_level >= MAX_SUPER_LASER_LEVEL) {
				Players[Player_num].laser_level = MAX_SUPER_LASER_LEVEL;
				HUD_init_message_literal(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "SUPER LASER MAXED OUT!");
			} else {
				int old_level=Players[Player_num].laser_level;

				if (Players[Player_num].laser_level <= MAX_LASER_LEVEL)
					Players[Player_num].laser_level = MAX_LASER_LEVEL;
				Players[Player_num].laser_level++;
				if (Newdemo_state == ND_STATE_RECORDING)
					newdemo_record_laser_level(old_level, Players[Player_num].laser_level);
				powerup_basic(10, 0, 10, LASER_SCORE, "Super Boost to Laser level %d",Players[Player_num].laser_level+1);
				update_laser_weapon_info();
				if (Players[Player_num].primary_weapon!=LASER_INDEX)
			      check_to_use_primary (SUPER_LASER_INDEX);
				used=1;
			}
			if (!used && !(Game_mode & GM_MULTI) )
				used = pick_up_energy();
			break;

		case POW_AMMO_RACK:
			if (Players[Player_num].flags & PLAYER_FLAGS_AMMO_RACK) {
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %s!",TXT_ALREADY_HAVE,"the Ammo rack");
				if (!(Game_mode & GM_MULTI) )
					used = pick_up_energy();
			}
			else {
				Players[Player_num].flags |= PLAYER_FLAGS_AMMO_RACK;
#ifdef NETWORK
				multi_send_play_sound(Powerup_info[obj->id].hit_sound, F1_0);
#endif
				digi_play_sample( Powerup_info[obj->id].hit_sound, F1_0 );
				powerup_basic(15, 0, 15, 0, "AMMO RACK!");
				used=1;
			}
			break;

		case POW_AFTERBURNER:
			if (Players[Player_num].flags & PLAYER_FLAGS_AFTERBURNER) {
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %s!",TXT_ALREADY_HAVE,"the Afterburner");
				if (!(Game_mode & GM_MULTI) )
					used = pick_up_energy();
			}
			else {
				Players[Player_num].flags |= PLAYER_FLAGS_AFTERBURNER;
#ifdef NETWORK
				multi_send_play_sound(Powerup_info[obj->id].hit_sound, F1_0);
#endif
				digi_play_sample( Powerup_info[obj->id].hit_sound, F1_0 );
				powerup_basic(15, 15, 15, 0, "AFTERBURNER!");
				Players[Player_num].afterburner_charge = f1_0;
				used=1;
#ifdef NETWORK
				if (Game_mode & GM_MULTI)
					multi_send_ship_status();
#endif
			}
			break;

		case POW_HEADLIGHT:
			if (Players[Player_num].flags & PLAYER_FLAGS_HEADLIGHT) {
				HUD_init_message(HM_DEFAULT|HM_REDUNDANT|HM_MAYDUPL, "%s %s!",TXT_ALREADY_HAVE,"the Headlight boost");
				if (!(Game_mode & GM_MULTI) )
					used = pick_up_energy();
			}
			else {
				Players[Player_num].flags |= PLAYER_FLAGS_HEADLIGHT;
#ifdef NETWORK
				multi_send_play_sound(Powerup_info[obj->id].hit_sound, F1_0);
#endif
				digi_play_sample( Powerup_info[obj->id].hit_sound, F1_0 );
				powerup_basic(15, 0, 15, 0, "HEADLIGHT BOOST! (Headlight is %s)",PlayerCfg.HeadlightActiveDefault?"ON":"OFF");
				if (PlayerCfg.HeadlightActiveDefault)
					Players[Player_num].flags |= PLAYER_FLAGS_HEADLIGHT_ON;
				used=1;
#ifdef NETWORK
			   if (Game_mode & GM_MULTI)
					multi_send_flags (Player_num);
#endif
			}
			break;

#ifdef NETWORK
		case POW_FLAG_BLUE:
			if (Game_mode & GM_CAPTURE)			
				if (get_team(Player_num) == TEAM_RED) {
					powerup_basic(15, 0, 15, 0, "BLUE FLAG!");
					Players[Player_num].flags |= PLAYER_FLAGS_FLAG;
					used=1;
					multi_send_got_flag (Player_num);
				}
		   break;

		case POW_HOARD_ORB:
			if (Game_mode & GM_HOARD)			
				if (Players[Player_num].secondary_ammo[PROXIMITY_INDEX]<12) {
					powerup_basic(15, 0, 15, 0, "Orb!!!");
					Players[Player_num].secondary_ammo[PROXIMITY_INDEX]++;
					Players[Player_num].flags |= PLAYER_FLAGS_FLAG;
					used=1;
					multi_send_got_orb (Player_num);
				}
		  break;	

		case POW_FLAG_RED:
			if (Game_mode & GM_CAPTURE)			
				if (get_team(Player_num) == TEAM_BLUE) {
					powerup_basic(15, 0, 15, 0, "RED FLAG!");
					Players[Player_num].flags |= PLAYER_FLAGS_FLAG;
					used=1;
					multi_send_got_flag (Player_num);
				}
		   break;
#endif

//		case POW_HOARD_ORB:


		default:
			break;
		}

//always say used, until physics problem (getting stuck on unused powerup)
//is solved.  Note also the break statements above that are commented out
//!!	used=1;

	if ((used || special_used) && Powerup_info[id].hit_sound  > -1 ) {
		#ifdef NETWORK
		if (Game_mode & GM_MULTI) // Added by Rob, take this out if it turns out to be not good for net games!
			multi_send_play_sound(Powerup_info[id].hit_sound, F1_0);
		#endif
		digi_play_sample( Powerup_info[id].hit_sound, F1_0 );
		detect_escort_goal_accomplished(obj-Objects);
	}

	return used;

}

/*
 * reads n powerup_type_info structs from a PHYSFS_file
 */
extern int powerup_type_info_read_n(powerup_type_info *pti, int n, PHYSFS_file *fp)
{
	int i;

	for (i = 0; i < n; i++) {
		pti[i].vclip_num = PHYSFSX_readInt(fp);
		pti[i].hit_sound = PHYSFSX_readInt(fp);
		pti[i].size = PHYSFSX_readFix(fp);
		pti[i].light = PHYSFSX_readFix(fp);
	}
	return i;
}

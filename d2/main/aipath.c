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
 * AI path forming stuff.
 *
 */

#include <stdio.h>		//	for printf()
#include <stdlib.h>		// for d_rand() and qsort()
#include <string.h>		// for memset()

#include "inferno.h"
#include "console.h"
#include "3d.h"

#include "object.h"
#include "dxxerror.h"
#include "ai.h"
#include "robot.h"
#include "fvi.h"
#include "physics.h"
#include "wall.h"
#ifdef EDITOR
#include "editor/editor.h"
#include "editor/esegment.h"
#endif
#include "player.h"
#include "fireball.h"
#include "game.h"
#include "gameseq.h"

#define	PARALLAX	0		//	If !0, then special debugging for Parallax eyes enabled.

//	Length in segments of avoidance path
#define	AVOID_SEG_LENGTH	7

#ifdef NDEBUG
#define	PATH_VALIDATION	0
#else
#define	PATH_VALIDATION	1
#endif

void validate_all_paths(void);
void ai_path_set_orient_and_vel(object *objp, vms_vector* goal_point, int player_visibility, vms_vector *vec_to_player);
void maybe_ai_path_garbage_collect(void);
void ai_path_garbage_collect(void);
#if PATH_VALIDATION
int validate_path(int debug_flag, point_seg* psegs, int num_points);
#endif

//	------------------------------------------------------------------------
void create_random_xlate(sbyte *xt)
{
	int	i;

	for (i=0; i<MAX_SIDES_PER_SEGMENT; i++)
		xt[i] = i;

	for (i=0; i<MAX_SIDES_PER_SEGMENT; i++) {
		int	j = (d_rand()*MAX_SIDES_PER_SEGMENT)/(D_RAND_MAX+1);
		sbyte temp_byte;
		Assert((j >= 0) && (j < MAX_SIDES_PER_SEGMENT));

		temp_byte = xt[j];
		xt[j] = xt[i];
		xt[i] = temp_byte;
	}

}

//	-----------------------------------------------------------------------------------------------------------
//	Insert the point at the center of the side connecting two segments between the two points.
// This is messy because we must insert into the list.  The simplest (and not too slow) way to do this is to start
// at the end of the list and go backwards.
void insert_center_points(point_seg *psegs, int *num_points)
{
	int	i, j, last_point;
	int	count=*num_points;

	last_point = *num_points-1;

	for (i=last_point; i>0; i--) {
		int			connect_side, temp_segnum;
		vms_vector	center_point, new_point;

		psegs[2*i] = psegs[i];
		connect_side = find_connect_side(&Segments[psegs[i].segnum], &Segments[psegs[i-1].segnum]);
		Assert(connect_side != -1);	//	Impossible!  These two segments must be connected, they were created by create_path_points (which was created by mk!)
		if (connect_side == -1)			//	Try to blow past the assert, this should at least prevent a hang.
			connect_side = 0;
		compute_center_point_on_side(&center_point, &Segments[psegs[i-1].segnum], connect_side);
		vm_vec_sub(&new_point, &psegs[i-1].point, &center_point);
		new_point.x /= 16;
		new_point.y /= 16;
		new_point.z /= 16;
		vm_vec_sub(&psegs[2*i-1].point, &center_point, &new_point);
		temp_segnum = find_point_seg(&psegs[2*i-1].point, psegs[2*i].segnum);
		if (temp_segnum == -1) {
			psegs[2*i-1].point = center_point;
			find_point_seg(&psegs[2*i-1].point, psegs[2*i].segnum);
		}

		psegs[2*i-1].segnum = psegs[2*i].segnum;
		count++;
	}

	//	Now, remove unnecessary center points.
	//	A center point is unnecessary if it is close to the line between the two adjacent points.
	//	MK, OPTIMIZE!  Can get away with about half the math since every vector gets computed twice.
	for (i=1; i<count-1; i+=2) {
		vms_vector	temp1, temp2;
		fix			dot;

		dot = vm_vec_dot(vm_vec_sub(&temp1, &psegs[i].point, &psegs[i-1].point), vm_vec_sub(&temp2, &psegs[i+1].point, &psegs[i].point));

		if (dot * 9/8 > fixmul(vm_vec_mag(&temp1), vm_vec_mag(&temp2)))
			psegs[i].segnum = -1;

	}

	//	Now, scan for points with segnum == -1
	j = 0;
	for (i=0; i<count; i++)
		if (psegs[i].segnum != -1)
			psegs[j++] = psegs[i];

	*num_points = j;
}

#ifdef EDITOR
int	Safety_flag_override = 0;
int	Random_flag_override = 0;
int	Ai_path_debug=0;
#endif

//	-----------------------------------------------------------------------------------------------------------
//	Move points halfway to outside of segment.
void move_towards_outside(point_seg *psegs, int *num_points, object *objp, int rand_flag)
{
	int	i;
	point_seg	new_psegs[200];

	Assert(*num_points < 200);

	for (i=1; i<*num_points-1; i++) {
		int			new_segnum;
		fix			segment_size;
		int			segnum;
		vms_vector	a, b, c, d, e;
		vms_vector	goal_pos;
		int			count;
		int			temp_segnum;

		// -- psegs[i].segnum = find_point_seg(&psegs[i].point, psegs[i].segnum);
		temp_segnum = find_point_seg(&psegs[i].point, psegs[i].segnum);
		//Assert(temp_segnum != -1);
		psegs[i].segnum = temp_segnum;
		segnum = psegs[i].segnum;

		vm_vec_sub(&a, &psegs[i].point, &psegs[i-1].point);
		vm_vec_sub(&b, &psegs[i+1].point, &psegs[i].point);
		vm_vec_sub(&c, &psegs[i+1].point, &psegs[i-1].point);
		//	I don't think we can use quick version here and this is _very_ rarely called. --MK, 07/03/95
		vm_vec_normalize_quick(&a);
		vm_vec_normalize_quick(&b);
		if (abs(vm_vec_dot(&a, &b)) > 3*F1_0/4 ) {
			if (abs(a.z) < F1_0/2) {
				if (rand_flag) {
					e.x = (d_rand()-16384)/2;
					e.y = (d_rand()-16384)/2;
					e.z = abs(e.x) + abs(e.y) + 1;
					vm_vec_normalize_quick(&e);
				} else {
					e.x = 0;
					e.y = 0;
					e.z = F1_0;
				}
			} else {
				if (rand_flag) {
					e.y = (d_rand()-16384)/2;
					e.z = (d_rand()-16384)/2;
					e.x = abs(e.y) + abs(e.z) + 1;
					vm_vec_normalize_quick(&e);
				} else {
					e.x = F1_0;
					e.y = 0;
					e.z = 0;
				}
			}
		} else {
			vm_vec_cross(&d, &a, &b);
			vm_vec_cross(&e, &c, &d);
			vm_vec_normalize_quick(&e);
		}

if (vm_vec_mag_quick(&e) < F1_0/2)
	Int3();

		segment_size = vm_vec_dist_quick(&Vertices[Segments[segnum].verts[0]], &Vertices[Segments[segnum].verts[6]]);
		if (segment_size > F1_0*40)
			segment_size = F1_0*40;

		vm_vec_scale_add(&goal_pos, &psegs[i].point, &e, segment_size/4);

		count = 3;
		while (count) {
			fvi_query	fq;
			fvi_info		hit_data;
			int			hit_type;
	
			fq.p0						= &psegs[i].point;
			fq.startseg				= psegs[i].segnum;
			fq.p1						= &goal_pos;
			fq.rad					= objp->size;
			fq.thisobjnum			= objp-Objects;
			fq.ignore_obj_list	= NULL;
			fq.flags					= 0;
	
			hit_type = find_vector_intersection(&fq, &hit_data);
	
			if (hit_type == HIT_NONE)
				count = 0;
			else {
				if ((count == 3) && (hit_type == HIT_BAD_P0))
					Int3();
				goal_pos.x = (fq.p0->x + hit_data.hit_pnt.x)/2;
				goal_pos.y = (fq.p0->y + hit_data.hit_pnt.y)/2;
				goal_pos.z = (fq.p0->z + hit_data.hit_pnt.z)/2;
				count--;
				if (count == 0) {	//	Couldn't move towards outside, that's ok, sometimes things can't be moved.
					goal_pos = psegs[i].point;
				}
			}
		}

		//	Only move towards outside if remained inside segment.
		new_segnum = find_point_seg(&goal_pos, psegs[i].segnum);
		if (new_segnum == psegs[i].segnum) {
			new_psegs[i].point = goal_pos;
			new_psegs[i].segnum = new_segnum;
		} else {
			new_psegs[i].point = psegs[i].point;
			new_psegs[i].segnum = psegs[i].segnum;
		}

	}

	for (i=1; i<*num_points-1; i++)
		psegs[i] = new_psegs[i];
}


//	-----------------------------------------------------------------------------------------------------------
//	Create a path from objp->pos to the center of end_seg.
//	Return a list of (segment_num, point_locations) at psegs
//	Return number of points in *num_points.
//	if max_depth == -1, then there is no maximum depth.
//	If unable to create path, return -1, else return 0.
//	If random_flag !0, then introduce randomness into path by looking at sides in random order.  This means
//	that a path between two segments won't always be the same, unless it is unique.
//	If safety_flag is set, then additional points are added to "make sure" that points are reachable.  I would
//	like to say that it ensures that the object can move between the points, but that would require knowing what
//	the object is (which isn't passed, right?) and making fvi calls (slow, right?).  So, consider it the more_or_less_safe_flag.
//	If end_seg == -2, then end seg will never be found and this routine will drop out due to depth (probably called by create_n_segment_path).
int create_path_points(object *objp, int start_seg, int end_seg, point_seg *psegs, short *num_points, int max_depth, int random_flag, int safety_flag, int avoid_seg, int currentObjectiveType, int currentObjectiveID, int objectiveInaccessible)
{
	int		cur_seg;
	int		sidenum;
	int		qtail = 0, qhead = 0;
	int		i;
	sbyte   visited[MAX_SEGMENTS];
	seg_seg	seg_queue[MAX_SEGMENTS];
	short		depth[MAX_SEGMENTS];
	int		cur_depth;
	sbyte   random_xlate[MAX_SIDES_PER_SEGMENT];
	point_seg	*original_psegs = psegs;
	int		l_num_points;

#if PATH_VALIDATION
	validate_all_paths();
#endif

if ((objp->type == OBJ_ROBOT) && (objp->ctype.ai_info.behavior == AIB_RUN_FROM)) {
	random_flag = 1;
	avoid_seg = ConsoleObject->segnum;
	// Int3();
}

	if (max_depth == -1)
		max_depth = MAX_PATH_LENGTH;

	l_num_points = 0;
//random_flag = Random_flag_override; //!! debug!!
//safety_flag = Safety_flag_override; //!! debug!!

//	for (i=0; i<=Highest_segment_index; i++) {
//		visited[i] = 0;
//		depth[i] = 0;
//	}
	memset(visited, 0, sizeof(visited[0])*(Highest_segment_index+1));
	memset(depth, 0, sizeof(depth[0])*(Highest_segment_index+1));

	//	If there is a segment we're not allowed to visit, mark it.
	if (avoid_seg != -1) {
		Assert(avoid_seg <= Highest_segment_index);
		if ((start_seg != avoid_seg) && (end_seg != avoid_seg)) {
			visited[avoid_seg] = 1;
			depth[avoid_seg] = 0;
		}
	}

	if (random_flag)
		create_random_xlate(random_xlate);

	cur_seg = start_seg;
	visited[cur_seg] = 1;
	cur_depth = 0;

	while (cur_seg != end_seg) {
		segment	*segp = &Segments[cur_seg];

		if (random_flag)
			if (d_rand() < 8192)
				create_random_xlate(random_xlate);

		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {

			int	snum = sidenum;

			if (random_flag)
				snum = random_xlate[sidenum];

			if (check_gap_size(cur_seg, snum) || ((currentObjectiveType == 2 && Walls[currentObjectiveID].type != WALL_OPEN) ||
			currentObjectiveType == 3 ||
			(currentObjectiveType == 1 && (Objects[currentObjectiveID].type == OBJ_CNTRLCEN || (Objects[currentObjectiveID].type == OBJ_ROBOT && Robot_info[Objects[currentObjectiveID].id].boss_flag)))))
				if (IS_CHILD(segp->children[snum]) && ((WALL_IS_DOORWAY(segp, snum) & WID_FLY_FLAG) || (ai_door_is_openable(objp, segp, snum, currentObjectiveType, currentObjectiveID, objectiveInaccessible, cur_seg)))) {
					int			this_seg = segp->children[snum];
					Assert(this_seg != -1);
					if (((cur_seg == avoid_seg) || (this_seg == avoid_seg)) && (ConsoleObject->segnum == avoid_seg)) {
						vms_vector	center_point;
	
						fvi_query	fq;
						fvi_info		hit_data;
						int			hit_type;
	
						compute_center_point_on_side(&center_point, segp, snum);

						fq.p0						= &objp->pos;
						fq.startseg				= objp->segnum;
						fq.p1						= &center_point;
						fq.rad					= objp->size;
						fq.thisobjnum			= objp-Objects;
						fq.ignore_obj_list	= NULL;
						fq.flags					= 0;

						hit_type = find_vector_intersection(&fq, &hit_data);
						if (hit_type != HIT_NONE) {
							goto dont_add;
						}
					}

					if (!visited[this_seg]) {
						seg_queue[qtail].start = cur_seg;
						seg_queue[qtail].end = this_seg;
						visited[this_seg] = 1;
						depth[qtail++] = cur_depth+1;
						if (depth[qtail-1] == max_depth) {
							end_seg = seg_queue[qtail-1].end;
							goto cpp_done1;
						}	// end if (depth[...
					}	// end if (!visited...
				}	// if (WALL_IS_DOORWAY(...
dont_add: ;
		}	//	for (sidenum...

		if (qtail <= 0)
			break;

		if (qhead >= qtail) {
			//	Couldn't get to goal, return a path as far as we got, which probably acceptable to the unparticular caller.
			end_seg = seg_queue[qtail-1].end;
			break;
		}

		cur_seg = seg_queue[qhead].end;
		cur_depth = depth[qhead];
		qhead++;

cpp_done1: ;
	}	//	while (cur_seg ...

	if (qtail > 0)
	{
		//	Set qtail to the segment which ends at the goal.
		while (seg_queue[--qtail].end != end_seg)
			if (qtail < 0) {
				*num_points = l_num_points;
				return -1;
			}
	}
	else
		qtail = -1;

	#ifdef EDITOR
	// -- N_selected_segs = 0;
	#endif

	while (qtail >= 0) {
		int	parent_seg, this_seg;

		this_seg = seg_queue[qtail].end;
		parent_seg = seg_queue[qtail].start;
		Assert((this_seg >= 0) && (this_seg <= Highest_segment_index));
		psegs->segnum = this_seg;
		compute_segment_center(&psegs->point,&Segments[this_seg]);
		psegs++;
		l_num_points++;
		#ifdef EDITOR
		// -- Selected_segs[N_selected_segs++] = this_seg;
		#endif

		if (parent_seg == start_seg)
			break;

		while (seg_queue[--qtail].end != parent_seg)
			Assert(qtail >= 0);
	}

	Assert((start_seg >= 0) && (start_seg <= Highest_segment_index));
	psegs->segnum = start_seg;
	compute_segment_center(&psegs->point,&Segments[start_seg]);
	psegs++;
	l_num_points++;

#if PATH_VALIDATION
	validate_path(1, original_psegs, l_num_points);
#endif

	//	Now, reverse point_segs in place.
	for (i=0; i< l_num_points/2; i++) {
		point_seg		temp_point_seg = *(original_psegs + i);
		*(original_psegs + i) = *(original_psegs + l_num_points - i - 1);
		*(original_psegs + l_num_points - i - 1) = temp_point_seg;
	}
#if PATH_VALIDATION
	validate_path(2, original_psegs, l_num_points);
#endif

	//	Now, if safety_flag set, then insert the point at the center of the side connecting two segments
	//	between the two points.  This is messy because we must insert into the list.  The simplest (and not too slow)
	//	way to do this is to start at the end of the list and go backwards.
	if (safety_flag) {
		if (psegs - Point_segs + l_num_points + 2 > MAX_POINT_SEGS) {
			//	Ouch!  Cannot insert center points in path.  So return unsafe path.
			ai_reset_all_paths();
			*num_points = l_num_points;
			return -1;
		} else {
			// int	old_num_points = l_num_points;
			insert_center_points(original_psegs, &l_num_points);
		}
	}

#if PATH_VALIDATION
	validate_path(3, original_psegs, l_num_points);
#endif

// -- MK, 10/30/95 -- This code causes apparent discontinuities in the path, moving a point
//	into a new segment.  It is not necessarily bad, but it makes it hard to track down actual
//	discontinuity problems.
	if (objp->type == OBJ_ROBOT)
		if (Robot_info[objp->id].companion)
			move_towards_outside(original_psegs, &l_num_points, objp, 0);

#if PATH_VALIDATION
	validate_path(4, original_psegs, l_num_points);
#endif

	*num_points = l_num_points;
	return 0;
}

int	Last_buddy_polish_path_tick;

//	-------------------------------------------------------------------------------------------------------
//	polish_path
//	Takes an existing path and makes it nicer.
//	Drops as many leading points as possible still maintaining direct accessibility
//	from current position to first point.
//	Will not shorten path to fewer than 3 points.
//	Returns number of points.
//	Starting position in psegs doesn't change.
//	Changed, MK, 10/18/95.  I think this was causing robots to get hung up on walls.
//				Only drop up to the first three points.
int polish_path(object *objp, point_seg *psegs, int num_points)
{
	int	i, first_point=0;

	if (num_points <= 4)
		return num_points;

	//	Prevent the buddy from polishing his path twice in one tick, which can cause him to get hung up.  Pretty ugly, huh?
	if (Robot_info[objp->id].companion)
	{
		if (d_tick_count == Last_buddy_polish_path_tick)
			return num_points;
		else
			Last_buddy_polish_path_tick = d_tick_count;
	}

	// -- MK: 10/18/95: for (i=0; i<num_points-3; i++) {
	for (i=0; i<2; i++) {
		fvi_query	fq;
		fvi_info		hit_data;
		int			hit_type;
	
		fq.p0						= &objp->pos;
		fq.startseg				= objp->segnum;
		fq.p1						= &psegs[i].point;
		fq.rad					= objp->size;
		fq.thisobjnum			= objp-Objects;
		fq.ignore_obj_list	= NULL;
		fq.flags					= 0;

		hit_type = find_vector_intersection(&fq, &hit_data);
	
		if (hit_type == HIT_NONE)
			first_point = i+1;
		else
			break;		
	}

	if (first_point) {
		//	Scrunch down all the psegs.
		for (i=first_point; i<num_points; i++)
			psegs[i-first_point] = psegs[i];
	}

	return num_points - first_point;
}

#ifndef NDEBUG
//	-------------------------------------------------------------------------------------------------------
//	Make sure that there are connections between all segments on path.
//	Note that if path has been optimized, connections may not be direct, so this function is useless, or worse.
//	Return true if valid, else return false.
int validate_path(int debug_flag, point_seg *psegs, int num_points)
{
#if PATH_VALIDATION
	int		i, curseg;

	curseg = psegs->segnum;
	if ((curseg < 0) || (curseg > Highest_segment_index)) {
		Int3();		//	Contact Mike: Debug trap for elusive, nasty bug.
		return 0;
	}

if (num_points == 0)
	return 1;

	for (i=1; i<num_points; i++) {
		int	sidenum;
		int	nextseg = psegs[i].segnum;

		if ((nextseg < 0) || (nextseg > Highest_segment_index)) {
			Int3();		//	Contact Mike: Debug trap for elusive, nasty bug.
			return 0;
		}

		if (curseg != nextseg) {
			for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++)
				if (Segments[curseg].children[sidenum] == nextseg)
					break;

			// Assert(sidenum != MAX_SIDES_PER_SEGMENT);	//	Hey, created path is not contiguous, why!?
			if (sidenum == MAX_SIDES_PER_SEGMENT) {
				Int3();
				return 0;
			}
			curseg = nextseg;
		}
	}
#endif
	return 1;

}
#endif

#ifndef NDEBUG
//	-----------------------------------------------------------------------------------------------------------
void validate_all_paths(void)
{

#if PATH_VALIDATION
	int	i;

	for (i=0; i<=Highest_object_index; i++) {
		if (Objects[i].type == OBJ_ROBOT) {
			object		*objp = &Objects[i];
			ai_static	*aip = &objp->ctype.ai_info;
			//ai_local		*ailp = &Ai_local_info[i];

			if (objp->control_type == CT_AI) {
				if ((aip->hide_index != -1) && (aip->path_length > 0))
					if (!validate_path(4, &Point_segs[aip->hide_index], aip->path_length)) {
						Int3();	//	This path is bogus!  Who corrupted it!  Danger! Danger!
									//	Contact Mike, he caused this mess.
						//force_dump_ai_objects_all("Error in validate_all_paths");
						aip->path_length=0;	//	This allows people to resume without harm...
					}
			}
		}
	}
#endif

}
#endif

//	-------------------------------------------------------------------------------------------------------
//	Creates a path from the objects current segment (objp->segnum) to the specified segment for the object to
//	hide in Ai_local_info[objnum].goal_segment.
//	Sets	objp->ctype.ai_info.hide_index,		a pointer into Point_segs, the first point_seg of the path.
//			objp->ctype.ai_info.path_length,		length of path
//			Point_segs_free_ptr				global pointer into Point_segs array
//	Change, 10/07/95: Used to create path to ConsoleObject->pos.  Now creates path to Believed_player_pos.
void create_path_to_player(object *objp, int max_length, int safety_flag)
{
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	int			start_seg, end_seg;

	if (max_length == -1)
		max_length = MAX_DEPTH_TO_SEARCH_FOR_PLAYER;

	ailp->time_player_seen = GameTime64;			//	Prevent from resetting path quickly.
	ailp->goal_segment = Believed_player_seg;

	start_seg = objp->segnum;
	end_seg = ailp->goal_segment;

	if (end_seg == -1) {
		;
	} else {
		create_path_points(objp, start_seg, end_seg, Point_segs_free_ptr, &aip->path_length, max_length, 1, safety_flag, -1, 0, NULL, NULL);
		aip->path_length = polish_path(objp, Point_segs_free_ptr, aip->path_length);
		aip->hide_index = Point_segs_free_ptr - Point_segs;
		aip->cur_path_index = 0;
		Point_segs_free_ptr += aip->path_length;
		if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
			//Int3();	//	Contact Mike: This is stupid.  Should call maybe_ai_garbage_collect before the add.
			//force_dump_ai_objects_all("Error in create_path_to_player");
			ai_reset_all_paths();
			return;
		}
//		Assert(Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 < MAX_POINT_SEGS);
		aip->PATH_DIR = 1;		//	Initialize to moving forward.
		// -- UNUSED! aip->SUBMODE = AISM_GOHIDE;		//	This forces immediate movement.
		ailp->mode = AIM_FOLLOW_PATH;
		ailp->player_awareness_type = 0;		//	If robot too aware of player, will set mode to chase
	}

	maybe_ai_path_garbage_collect();

}

//	-------------------------------------------------------------------------------------------------------
//	Creates a path from the object's current segment (objp->segnum) to segment goalseg.
void create_path_to_segment(object *objp, int goalseg, int max_length, int safety_flag)
{
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	int			start_seg, end_seg;

	if (max_length == -1)
		max_length = MAX_DEPTH_TO_SEARCH_FOR_PLAYER;

	ailp->time_player_seen = GameTime64;			//	Prevent from resetting path quickly.
	ailp->goal_segment = goalseg;

	start_seg = objp->segnum;
	end_seg = ailp->goal_segment;

	if (end_seg == -1) {
		;
	} else {
		create_path_points(objp, start_seg, end_seg, Point_segs_free_ptr, &aip->path_length, max_length, 1, safety_flag, -1, 0, NULL, NULL);
		aip->hide_index = Point_segs_free_ptr - Point_segs;
		aip->cur_path_index = 0;
		Point_segs_free_ptr += aip->path_length;
		if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
			ai_reset_all_paths();
			return;
		}

		aip->PATH_DIR = 1;		//	Initialize to moving forward.
		// -- UNUSED! aip->SUBMODE = AISM_GOHIDE;		//	This forces immediate movement.
		ailp->player_awareness_type = 0;		//	If robot too aware of player, will set mode to chase
	}

	maybe_ai_path_garbage_collect();

}

//	-------------------------------------------------------------------------------------------------------
//	Creates a path from the objects current segment (objp->segnum) to the specified segment for the object to
//	hide in Ai_local_info[objnum].goal_segment
//	Sets	objp->ctype.ai_info.hide_index,		a pointer into Point_segs, the first point_seg of the path.
//			objp->ctype.ai_info.path_length,		length of path
//			Point_segs_free_ptr				global pointer into Point_segs array
void create_path_to_station(object *objp, int max_length)
{
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	int			start_seg, end_seg;

	if (max_length == -1)
		max_length = MAX_DEPTH_TO_SEARCH_FOR_PLAYER;

	ailp->time_player_seen = GameTime64;			//	Prevent from resetting path quickly.

	start_seg = objp->segnum;
	end_seg = aip->hide_segment;

	if (end_seg == -1) {
		;
	} else {
		create_path_points(objp, start_seg, end_seg, Point_segs_free_ptr, &aip->path_length, max_length, 1, 1, -1, 0, NULL, NULL);
		aip->path_length = polish_path(objp, Point_segs_free_ptr, aip->path_length);
		aip->hide_index = Point_segs_free_ptr - Point_segs;
		aip->cur_path_index = 0;

		Point_segs_free_ptr += aip->path_length;
		if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
			//Int3();	//	Contact Mike: Stupid.
			//force_dump_ai_objects_all("Error in create_path_to_station");
			ai_reset_all_paths();
			return;
		}
//		Assert(Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 < MAX_POINT_SEGS);
		aip->PATH_DIR = 1;		//	Initialize to moving forward.
		// aip->SUBMODE = AISM_GOHIDE;		//	This forces immediate movement.
		ailp->mode = AIM_FOLLOW_PATH;
		ailp->player_awareness_type = 0;
	}


	maybe_ai_path_garbage_collect();

}


//	-------------------------------------------------------------------------------------------------------
//	Create a path of length path_length for an object, stuffing info in ai_info field.
void create_n_segment_path(object *objp, int path_length, int avoid_seg)
{
	ai_static	*aip=&objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objp-Objects];

	if (create_path_points(objp, objp->segnum, -2, Point_segs_free_ptr, &aip->path_length, path_length, 1, 0, avoid_seg, 0, NULL, NULL) == -1) {
		Point_segs_free_ptr += aip->path_length;
		while ((create_path_points(objp, objp->segnum, -2, Point_segs_free_ptr, &aip->path_length, --path_length, 1, 0, -1, 0, NULL, NULL) == -1)) {
			Assert(path_length);
		}
	}

	aip->hide_index = Point_segs_free_ptr - Point_segs;
	aip->cur_path_index = 0;
#if PATH_VALIDATION
	validate_path(8, Point_segs_free_ptr, aip->path_length);
#endif
	Point_segs_free_ptr += aip->path_length;
	if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
		//Int3();	//	Contact Mike: This is curious, though not deadly. /eip++;g
		//force_dump_ai_objects_all("Error in crete_n_segment_path 2");
		ai_reset_all_paths();
	}

	aip->PATH_DIR = 1;		//	Initialize to moving forward.
	// -- UNUSED! aip->SUBMODE = -1;		//	Don't know what this means.
	ailp->mode = AIM_FOLLOW_PATH;

	//	If this robot is visible (player_visibility is not available) and it's running away, move towards outside with
	//	randomness to prevent a stream of bots from going away down the center of a corridor.
	if (Ai_local_info[objp-Objects].previous_visibility) {
		if (aip->path_length) {
			int	t_num_points = aip->path_length;
			move_towards_outside(&Point_segs[aip->hide_index], &t_num_points, objp, 1);
			aip->path_length = t_num_points;
		}
	}

	maybe_ai_path_garbage_collect();

}

//	-------------------------------------------------------------------------------------------------------
void create_n_segment_path_to_door(object *objp, int path_length, int avoid_seg)
{
	create_n_segment_path(objp, path_length, avoid_seg);
}

extern int Connected_segment_distance;

#define Int3_if(cond) if (!cond) Int3();

//	----------------------------------------------------------------------------------------------------
void move_object_to_goal(object *objp, vms_vector *goal_point, int goal_seg)
{
	ai_static	*aip = &objp->ctype.ai_info;
	int			segnum;

	if (aip->path_length < 2)
		return;

	Assert(objp->segnum != -1);

	Assert(aip->path_length >= 2);

	if (aip->cur_path_index <= 0) {
		if (aip->behavior == AIB_STATION) {
			create_path_to_station(objp, 15);
			return;
		}
		aip->cur_path_index = 1;
		aip->PATH_DIR = 1;
	} else if (aip->cur_path_index >= aip->path_length - 1) {
		if (aip->behavior == AIB_STATION) {
			create_path_to_station(objp, 15);
			if (aip->path_length == 0) {
				ai_local		*ailp = &Ai_local_info[objp-Objects];
				ailp->mode = AIM_STILL;
			}
			return;
		}
		Assert(aip->path_length != 0);
		aip->cur_path_index = aip->path_length-2;
		aip->PATH_DIR = -1;
	} else
		aip->cur_path_index += aip->PATH_DIR;

	//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));

	objp->pos = *goal_point;
	segnum = find_object_seg(objp);

	if (segnum == -1) {
		Int3();	//	Oops, object is not in any segment.
					// Contact Mike: This is impossible.
		//	Hack, move object to center of segment it used to be in.
		compute_segment_center(&objp->pos, &Segments[objp->segnum]);
	} else
		obj_relink(objp-Objects, segnum);
}

// -- too much work -- //	----------------------------------------------------------------------------------------------------------
// -- too much work -- //	Return true if the object the companion wants to kill is reachable.
// -- too much work -- int attack_kill_object(object *objp)
// -- too much work -- {
// -- too much work -- 	object		*kill_objp;
// -- too much work -- 	fvi_info		hit_data;
// -- too much work -- 	int			fate;
// -- too much work -- 	fvi_query	fq;
// -- too much work --
// -- too much work -- 	if (Escort_kill_object == -1)
// -- too much work -- 		return 0;
// -- too much work --
// -- too much work -- 	kill_objp = &Objects[Escort_kill_object];
// -- too much work --
// -- too much work -- 	fq.p0						= &objp->pos;
// -- too much work -- 	fq.startseg				= objp->segnum;
// -- too much work -- 	fq.p1						= &kill_objp->pos;
// -- too much work -- 	fq.rad					= objp->size;
// -- too much work -- 	fq.thisobjnum			= objp-Objects;
// -- too much work -- 	fq.ignore_obj_list	= NULL;
// -- too much work -- 	fq.flags					= 0;
// -- too much work --
// -- too much work -- 	fate = find_vector_intersection(&fq,&hit_data);
// -- too much work --
// -- too much work -- 	if (fate == HIT_NONE)
// -- too much work -- 		return 1;
// -- too much work -- 	else
// -- too much work -- 		return 0;
// -- too much work -- }

//	----------------------------------------------------------------------------------------------------------
//	Optimization: If current velocity will take robot near goal, don't change velocity
void ai_follow_path(object *objp, int player_visibility, int previous_visibility, vms_vector *vec_to_player)
{
	ai_static		*aip = &objp->ctype.ai_info;

	vms_vector	goal_point, new_goal_point;
	fix			dist_to_goal;
	robot_info	*robptr = &Robot_info[objp->id];
	int			forced_break, original_dir, original_index;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	fix			threshold_distance;


	if ((aip->hide_index == -1) || (aip->path_length == 0))
	{
		if (ailp->mode == AIM_RUN_FROM_OBJECT) {
			create_n_segment_path(objp, 5, -1);
			//--Int3_if((aip->path_length != 0));
			ailp->mode = AIM_RUN_FROM_OBJECT;
		} else {
			create_n_segment_path(objp, 5, -1);
			//--Int3_if((aip->path_length != 0));
		}
	}

	if ((aip->hide_index + aip->path_length > Point_segs_free_ptr - Point_segs) && (aip->path_length>0)) {
		Int3();	//	Contact Mike: Bad.  Path goes into what is believed to be free space.
		//	This is debugging code.  Figure out why garbage collection
		//	didn't compress this object's path information.
		ai_path_garbage_collect();
		//force_dump_ai_objects_all("Error in ai_follow_path");
		ai_reset_all_paths();
	}

	if (aip->path_length < 2) {
		if ((aip->behavior == AIB_SNIPE) || (ailp->mode == AIM_RUN_FROM_OBJECT)) {
			if (ConsoleObject->segnum == objp->segnum) {
				create_n_segment_path(objp, AVOID_SEG_LENGTH, -1);			//	Can't avoid segment player is in, robot is already in it! (That's what the -1 is for)
				//--Int3_if((aip->path_length != 0));
			} else {
				create_n_segment_path(objp, AVOID_SEG_LENGTH, ConsoleObject->segnum);
				//--Int3_if((aip->path_length != 0));
			}
			if (aip->behavior == AIB_SNIPE) {
				if (robptr->thief)
					ailp->mode = AIM_THIEF_ATTACK;	//	It gets bashed in create_n_segment_path
				else
					ailp->mode = AIM_SNIPE_FIRE;	//	It gets bashed in create_n_segment_path
			} else {
				ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
			}
		} else if (robptr->companion == 0) {
			ailp->mode = AIM_STILL;
			aip->path_length = 0;
			return;
		}
	}

	//--Int3_if(((aip->PATH_DIR == -1) || (aip->PATH_DIR == 1)));
	//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));

	goal_point = Point_segs[aip->hide_index + aip->cur_path_index].point;
	dist_to_goal = vm_vec_dist_quick(&goal_point, &objp->pos);

	//	If running from player, only run until can't be seen.
	if (ailp->mode == AIM_RUN_FROM_OBJECT) {
		if ((player_visibility == 0) && (ailp->player_awareness_type == 0)) {
			fix	vel_scale;

			vel_scale = F1_0 - FrameTime/2;
			if (vel_scale < F1_0/2)
				vel_scale = F1_0/2;

			vm_vec_scale(&objp->mtype.phys_info.velocity, vel_scale);

			return;
		} else if (!(d_tick_count ^ ((objp-Objects) & 0x07))) {		//	Done 1/8 ticks.
			//	If player on path (beyond point robot is now at), then create a new path.
			point_seg	*curpsp = &Point_segs[aip->hide_index];
			int			player_segnum = ConsoleObject->segnum;
			int			i;

			//	This is probably being done every frame, which is wasteful.
			for (i=aip->cur_path_index; i<aip->path_length; i++) {
				if (curpsp[i].segnum == player_segnum) {
					if (player_segnum != objp->segnum) {
						create_n_segment_path(objp, AVOID_SEG_LENGTH, player_segnum);
					} else {
						create_n_segment_path(objp, AVOID_SEG_LENGTH, -1);
					}
					Assert(aip->path_length != 0);
					ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
					break;
				}
			}
			if (player_visibility) {
				ailp->player_awareness_type = 1;
				ailp->player_awareness_time = F1_0;
			}
		}
	}

	//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));

	if (aip->cur_path_index < 0) {
		aip->cur_path_index = 0;
	} else if (aip->cur_path_index >= aip->path_length) {
		if (ailp->mode == AIM_RUN_FROM_OBJECT) {
			create_n_segment_path(objp, AVOID_SEG_LENGTH, ConsoleObject->segnum);
			ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
			Assert(aip->path_length != 0);
		} else {
			aip->cur_path_index = aip->path_length-1;
		}
	}

	goal_point = Point_segs[aip->hide_index + aip->cur_path_index].point;

	//	If near goal, pick another goal point.
	forced_break = 0;		//	Gets set for short paths.
	original_dir = aip->PATH_DIR;
	original_index = aip->cur_path_index;
	threshold_distance = fixmul(vm_vec_mag_quick(&objp->mtype.phys_info.velocity), FrameTime)*2 + F1_0*2;

	new_goal_point = Point_segs[aip->hide_index + aip->cur_path_index].point;

	//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));

	while ((dist_to_goal < threshold_distance) && !forced_break) {

		//	Advance to next point on path.
		aip->cur_path_index += aip->PATH_DIR;

		//	See if next point wraps past end of path (in either direction), and if so, deal with it based on mode.
		if ((aip->cur_path_index >= aip->path_length) || (aip->cur_path_index < 0)) {
			//	Buddy bot.  If he's in mode to get away from player and at end of line,
			//	if player visible, then make a new path, else just return.
			if (robptr->companion) {
				if (Escort_special_goal == ESCORT_GOAL_SCRAM)
				{
					if (player_visibility) {
						create_n_segment_path(objp, 16 + d_rand() * 16, -1);
						aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
						Assert(aip->path_length != 0);
						ailp->mode = AIM_WANDER;	//	Special buddy mode.
						//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
						return;
					} else {
						ailp->mode = AIM_WANDER;	//	Special buddy mode.
						vm_vec_zero(&objp->mtype.phys_info.velocity);
						vm_vec_zero(&objp->mtype.phys_info.rotvel);
						//!!Assert((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length));
						return;
					}
				}
			}

			if (aip->behavior == AIB_FOLLOW) {
				create_n_segment_path(objp, 10, ConsoleObject->segnum);
				//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
			} else if (aip->behavior == AIB_STATION) {
				create_path_to_station(objp, 15);
				if ((aip->hide_segment != Point_segs[aip->hide_index+aip->path_length-1].segnum) || (aip->path_length == 0)) {
					ailp->mode = AIM_STILL;
				} else {
					//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
				}
				return;
			} else if (ailp->mode == AIM_FOLLOW_PATH) {
				create_path_to_player(objp, 10, 1);
				if (aip->hide_segment != Point_segs[aip->hide_index+aip->path_length-1].segnum) {
					ailp->mode = AIM_STILL;
					return;
				} else {
					//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
				}
			} else if (ailp->mode == AIM_RUN_FROM_OBJECT) {
				create_n_segment_path(objp, AVOID_SEG_LENGTH, ConsoleObject->segnum);
				ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
				if (aip->path_length < 1) {
					create_n_segment_path(objp, AVOID_SEG_LENGTH, ConsoleObject->segnum);
					ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
					if (aip->path_length < 1) {
						aip->behavior = AIB_NORMAL;
						ailp->mode = AIM_STILL;
						return;
					}
				}
				//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
			} else {
				//	Reached end of the line.  First see if opposite end point is reachable, and if so, go there.
				//	If not, turn around.
				int			opposite_end_index;
				vms_vector	*opposite_end_point;
				fvi_info		hit_data;
				int			fate;
				fvi_query	fq;

				// See which end we're nearer and look at the opposite end point.
				if (abs(aip->cur_path_index - aip->path_length) < aip->cur_path_index) {
					//	Nearer to far end (ie, index not 0), so try to reach 0.
					opposite_end_index = 0;
				} else {
					//	Nearer to 0 end, so try to reach far end.
					opposite_end_index = aip->path_length-1;
				}

				//--Int3_if(((opposite_end_index >= 0) && (opposite_end_index < aip->path_length)));

				opposite_end_point = &Point_segs[aip->hide_index + opposite_end_index].point;

				fq.p0						= &objp->pos;
				fq.startseg				= objp->segnum;
				fq.p1						= opposite_end_point;
				fq.rad					= objp->size;
				fq.thisobjnum			= objp-Objects;
				fq.ignore_obj_list	= NULL;
				fq.flags					= 0; 				//what about trans walls???

				fate = find_vector_intersection(&fq,&hit_data);

				if (fate != HIT_WALL) {
					//	We can be circular!  Do it!
					//	Path direction is unchanged.
					aip->cur_path_index = opposite_end_index;
				} else {
					aip->PATH_DIR = -aip->PATH_DIR;
					aip->cur_path_index += aip->PATH_DIR;
				}
				//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
			}
			break;
		} else {
			new_goal_point = Point_segs[aip->hide_index + aip->cur_path_index].point;
			goal_point = new_goal_point;
			dist_to_goal = vm_vec_dist_quick(&goal_point, &objp->pos);
			//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
		}

		//	If went all the way around to original point, in same direction, then get out of here!
		if ((aip->cur_path_index == original_index) && (aip->PATH_DIR == original_dir)) {
			create_path_to_player(objp, 3, 1);
			//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
			forced_break = 1;
		}
		//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
	}	//	end while

	//	Set velocity (objp->mtype.phys_info.velocity) and orientation (objp->orient) for this object.
	//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));
	ai_path_set_orient_and_vel(objp, &goal_point, player_visibility, vec_to_player);
	//--Int3_if(((aip->cur_path_index >= 0) && (aip->cur_path_index < aip->path_length)));

}

typedef struct {
	short	path_start, objnum;
} obj_path;

int path_index_compare(obj_path *i1, obj_path *i2)
{
	if (i1->path_start < i2->path_start)
		return -1;
	else if (i1->path_start == i2->path_start)
		return 0;
	else
		return 1;
}

//	----------------------------------------------------------------------------------------------------------
//	Set orientation matrix and velocity for objp based on its desire to get to a point.
void ai_path_set_orient_and_vel(object *objp, vms_vector *goal_point, int player_visibility, vms_vector *vec_to_player)
{
	vms_vector	cur_vel = objp->mtype.phys_info.velocity;
	vms_vector	norm_cur_vel;
	vms_vector	norm_vec_to_goal;
	vms_vector	cur_pos = objp->pos;
	vms_vector	norm_fvec;
	fix			speed_scale;
	fix			dot;
	robot_info	*robptr = &Robot_info[objp->id];
	fix			max_speed;

	//	If evading player, use highest difficulty level speed, plus something based on diff level
	max_speed = robptr->max_speed[Difficulty_level];
	if ((Ai_local_info[objp-Objects].mode == AIM_RUN_FROM_OBJECT) || (objp->ctype.ai_info.behavior == AIB_SNIPE))
		max_speed = max_speed*3/2;

	vm_vec_sub(&norm_vec_to_goal, goal_point, &cur_pos);
	vm_vec_normalize_quick(&norm_vec_to_goal);

	norm_cur_vel = cur_vel;
	vm_vec_normalize_quick(&norm_cur_vel);

	norm_fvec = objp->orient.fvec;
	vm_vec_normalize_quick(&norm_fvec);

	dot = vm_vec_dot(&norm_vec_to_goal, &norm_fvec);

	//	If very close to facing opposite desired vector, perturb vector
	if (dot < -15*F1_0/16) {
		norm_cur_vel = norm_vec_to_goal;
	} else {
		norm_cur_vel.x += norm_vec_to_goal.x/2/((float)(F1_0/30)/FrameTime);
		norm_cur_vel.y += norm_vec_to_goal.y/2/((float)(F1_0/30)/FrameTime);
		norm_cur_vel.z += norm_vec_to_goal.z/2/((float)(F1_0/30)/FrameTime);
	}

	vm_vec_normalize_quick(&norm_cur_vel);

	//	Set speed based on this robot type's maximum allowed speed and how hard it is turning.
	//	How hard it is turning is based on the dot product of (vector to goal) and (current velocity vector)
	//	Note that since 3*F1_0/4 is added to dot product, it is possible for the robot to back up.

	//	Set speed and orientation.
	if (dot < 0)
		dot /= -4;

	//	If in snipe mode, can move fast even if not facing that direction.
	if (objp->ctype.ai_info.behavior == AIB_SNIPE)
		if (dot < F1_0/2)
			dot = (dot + F1_0)/2;

	speed_scale = fixmul(max_speed, dot);
	vm_vec_scale(&norm_cur_vel, speed_scale);
	objp->mtype.phys_info.velocity = norm_cur_vel;

	if ((Ai_local_info[objp-Objects].mode == AIM_RUN_FROM_OBJECT) || (robptr->companion == 1) || (objp->ctype.ai_info.behavior == AIB_SNIPE)) {
		if (Ai_local_info[objp-Objects].mode == AIM_SNIPE_RETREAT_BACKWARDS) {
			if ((player_visibility) && (vec_to_player != NULL))
				norm_vec_to_goal = *vec_to_player;
			else
				vm_vec_negate(&norm_vec_to_goal);
		}
		ai_turn_towards_vector(&norm_vec_to_goal, objp, robptr->turn_time[NDL-1]/2);
	} else
		ai_turn_towards_vector(&norm_vec_to_goal, objp, robptr->turn_time[Difficulty_level]);

}

int	Last_tick_garbage_collected = 0;

//	----------------------------------------------------------------------------------------------------------
//	Garbage colledion -- Free all unused records in Point_segs and compress all paths.
void ai_path_garbage_collect(void)
{
	int	free_path_index = 0;
	int	num_path_objects = 0;
	int	objnum;
	int	objind;
	obj_path		object_list[MAX_OBJECTS];

#ifndef NDEBUG
	force_dump_ai_objects_all("***** Start ai_path_garbage_collect *****");
#endif

	Last_tick_garbage_collected = d_tick_count;

#if PATH_VALIDATION
	validate_all_paths();
#endif
	//	Create a list of objects which have paths of length 1 or more.
	for (objnum=0; objnum <= Highest_object_index; objnum++) {
		object	*objp = &Objects[objnum];

		if ((objp->type == OBJ_ROBOT) && ((objp->control_type == CT_AI) || (objp->control_type == CT_MORPH))) {
			ai_static	*aip = &objp->ctype.ai_info;

			if (aip->path_length) {
				object_list[num_path_objects].path_start = aip->hide_index;
				object_list[num_path_objects++].objnum = objnum;
			}
		}
	}

	qsort(object_list, num_path_objects, sizeof(object_list[0]),
			(int (*)(void const *,void const *))path_index_compare);

	for (objind=0; objind < num_path_objects; objind++) {
		object		*objp;
		ai_static	*aip;
		int			i;
		int			old_index;

		objnum = object_list[objind].objnum;
		objp = &Objects[objnum];
		aip = &objp->ctype.ai_info;
		old_index = aip->hide_index;

		aip->hide_index = free_path_index;
		for (i=0; i<aip->path_length; i++)
			Point_segs[free_path_index++] = Point_segs[old_index++];
	}

	Point_segs_free_ptr = &Point_segs[free_path_index];

#ifndef NDEBUG
	{
	int i;

	force_dump_ai_objects_all("***** Finish ai_path_garbage_collect *****");

	for (i=0; i<=Highest_object_index; i++) {
		ai_static	*aip = &Objects[i].ctype.ai_info;

		if ((Objects[i].type == OBJ_ROBOT) && (Objects[i].control_type == CT_AI))
			if ((aip->hide_index + aip->path_length > Point_segs_free_ptr - Point_segs) && (aip->path_length>0))
				Int3();		//	Contact Mike: Debug trap for nasty, elusive bug.
	}

	validate_all_paths();
	}
#endif

}

//	-----------------------------------------------------------------------------
//	Do garbage collection if not been done for awhile, or things getting really critical.
void maybe_ai_path_garbage_collect(void)
{
	if (Point_segs_free_ptr - Point_segs > MAX_POINT_SEGS - MAX_PATH_LENGTH) {
		if (Last_tick_garbage_collected+1 >= d_tick_count) {
			//	This is kind of bad.  Garbage collected last frame or this frame.
			//	Just destroy all paths.  Too bad for the robots.  They are memory wasteful.
			ai_reset_all_paths();
		} else {
			//	We are really close to full, but didn't just garbage collect, so maybe this is recoverable.
			ai_path_garbage_collect();
		}
	} else if (Point_segs_free_ptr - Point_segs > 3*MAX_POINT_SEGS/4) {
		if (Last_tick_garbage_collected + 16 < d_tick_count) {
			ai_path_garbage_collect();
		}
	} else if (Point_segs_free_ptr - Point_segs > MAX_POINT_SEGS/2) {
		if (Last_tick_garbage_collected + 256 < d_tick_count) {
			ai_path_garbage_collect();
		}
	}
}

//	-----------------------------------------------------------------------------
//	Reset all paths.  Do garbage collection.
//	Should be called at the start of each level.
void ai_reset_all_paths(void)
{
	int	i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].control_type == CT_AI) {
			Objects[i].ctype.ai_info.hide_index = -1;
			Objects[i].ctype.ai_info.path_length = 0;
		}

	ai_path_garbage_collect();

}

//	---------------------------------------------------------------------------------------------------------
//	Probably called because a robot bashed a wall, getting a bunch of retries.
//	Try to resume path.
void attempt_to_resume_path(object *objp)
{
	ai_static *aip = &objp->ctype.ai_info;
	int new_path_index;

	if ((aip->behavior == AIB_STATION) && (Robot_info[objp->id].companion != 1))
		if (d_rand() > 8192) {
			ai_local *ailp = &Ai_local_info[objp-Objects];

			aip->hide_segment = objp->segnum;
			ailp->mode = AIM_STILL;
		}

	new_path_index = aip->cur_path_index - aip->PATH_DIR;

	if ((new_path_index >= 0) && (new_path_index < aip->path_length)) {
		aip->cur_path_index = new_path_index;
	} else {
		// At end of line and have nowhere to go.
		move_towards_segment_center(objp);
		create_path_to_station(objp, 15);
	}
}

//	----------------------------------------------------------------------------------------------------------
//					DEBUG FUNCTIONS FOLLOW
//	----------------------------------------------------------------------------------------------------------

#ifdef EDITOR
int	Test_size = 1000;

void test_create_path_many(void)
{
	point_seg	point_segs[200];
	short			num_points;

	int			i;

	for (i=0; i<Test_size; i++) {
		Cursegp = &Segments[(d_rand() * (Highest_segment_index + 1)) / D_RAND_MAX];
		Markedsegp = &Segments[(d_rand() * (Highest_segment_index + 1)) / D_RAND_MAX];
		create_path_points(&Objects[0], Cursegp-Segments, Markedsegp-Segments, point_segs, &num_points, -1, 0, 0, -1, 0, NULL);
	}

}

void test_create_path(void)
{
	point_seg	point_segs[200];
	short			num_points;

	create_path_points(&Objects[0], Cursegp-Segments, Markedsegp-Segments, point_segs, &num_points, -1, 0, 0, -1, 0, NULL);

}

//	For all segments in mine, create paths to all segments in mine, print results.
void test_create_all_paths(void)
{
	int	start_seg, end_seg;
	short	resultant_length;

	Point_segs_free_ptr = Point_segs;

	for (start_seg=0; start_seg<=Highest_segment_index-1; start_seg++) {
		if (Segments[start_seg].segnum != -1) {
			for (end_seg=start_seg+1; end_seg<=Highest_segment_index; end_seg++) {
				if (Segments[end_seg].segnum != -1) {
					create_path_points(&Objects[0], start_seg, end_seg, Point_segs_free_ptr, &resultant_length, -1, 0, 0, -1, 0, NULL);
				}
			}
		}
	}
}

short	Player_path_length=0;
int	Player_hide_index=-1;
int	Player_cur_path_index=0;
int	Player_following_path_flag=0;

//	------------------------------------------------------------------------------------------------------------------
//	Set orientation matrix and velocity for objp based on its desire to get to a point.
void player_path_set_orient_and_vel(object *objp, vms_vector *goal_point)
{
	vms_vector	cur_vel = objp->mtype.phys_info.velocity;
	vms_vector	norm_cur_vel;
	vms_vector	norm_vec_to_goal;
	vms_vector	cur_pos = objp->pos;
	vms_vector	norm_fvec;
	fix			speed_scale;
	fix			dot;
	fix			max_speed;

	max_speed = Robot_info[objp->id].max_speed[Difficulty_level];

	vm_vec_sub(&norm_vec_to_goal, goal_point, &cur_pos);
	vm_vec_normalize_quick(&norm_vec_to_goal);

	norm_cur_vel = cur_vel;
	vm_vec_normalize_quick(&norm_cur_vel);

	norm_fvec = objp->orient.fvec;
	vm_vec_normalize_quick(&norm_fvec);

	dot = vm_vec_dot(&norm_vec_to_goal, &norm_fvec);
	if (Ai_local_info[objp-Objects].mode == AIM_SNIPE_RETREAT_BACKWARDS) {
		dot = -dot;
	}

	//	If very close to facing opposite desired vector, perturb vector
	if (dot < -15*F1_0/16) {
		norm_cur_vel = norm_vec_to_goal;
	} else {
		norm_cur_vel.x += norm_vec_to_goal.x/2/((float)(F1_0/30)/FrameTime);
		norm_cur_vel.y += norm_vec_to_goal.y/2/((float)(F1_0/30)/FrameTime);
		norm_cur_vel.z += norm_vec_to_goal.z/2/((float)(F1_0/30)/FrameTime);
	}

	vm_vec_normalize_quick(&norm_cur_vel);

	//	Set speed based on this robot type's maximum allowed speed and how hard it is turning.
	//	How hard it is turning is based on the dot product of (vector to goal) and (current velocity vector)
	//	Note that since 3*F1_0/4 is added to dot product, it is possible for the robot to back up.

	//	Set speed and orientation.
	if (dot < 0)
		dot /= 4;

	speed_scale = fixmul(max_speed, dot);
	vm_vec_scale(&norm_cur_vel, speed_scale);
	objp->mtype.phys_info.velocity = norm_cur_vel;
	ai_turn_towards_vector(&norm_vec_to_goal, objp, F1_0);

}

//	----------------------------------------------------------------------------------------------------------
//	Optimization: If current velocity will take robot near goal, don't change velocity
void player_follow_path(object *objp)
{
	vms_vector	goal_point;
	fix			dist_to_goal;
	int			count, forced_break, original_index;
	int			goal_seg;
	fix			threshold_distance;

	if (!Player_following_path_flag)
		return;

	if (Player_hide_index == -1)
		return;

	if (Player_path_length < 2)
		return;

	goal_point = Point_segs[Player_hide_index + Player_cur_path_index].point;
	goal_seg = Point_segs[Player_hide_index + Player_cur_path_index].segnum;
	Assert((goal_seg >= 0) && (goal_seg <= Highest_segment_index));
	(void)goal_seg;
	dist_to_goal = vm_vec_dist_quick(&goal_point, &objp->pos);

	if (Player_cur_path_index < 0)
		Player_cur_path_index = 0;
	else if (Player_cur_path_index >= Player_path_length)
		Player_cur_path_index = Player_path_length-1;

	goal_point = Point_segs[Player_hide_index + Player_cur_path_index].point;

	count=0;

	//	If near goal, pick another goal point.
	forced_break = 0;		//	Gets set for short paths.
	//original_dir = 1;
	original_index = Player_cur_path_index;
	threshold_distance = fixmul(vm_vec_mag_quick(&objp->mtype.phys_info.velocity), FrameTime)*2 + F1_0*2;

	while ((dist_to_goal < threshold_distance) && !forced_break) {

		//	----- Debug stuff -----
		if (count++ > 20) {
			break;
		}

		//	Advance to next point on path.
		Player_cur_path_index += 1;

		//	See if next point wraps past end of path (in either direction), and if so, deal with it based on mode.
		if ((Player_cur_path_index >= Player_path_length) || (Player_cur_path_index < 0)) {
			Player_following_path_flag = 0;
			forced_break = 1;
		}

		//	If went all the way around to original point, in same direction, then get out of here!
		if (Player_cur_path_index == original_index) {
			Player_following_path_flag = 0;
			forced_break = 1;
		}

		goal_point = Point_segs[Player_hide_index + Player_cur_path_index].point;
		dist_to_goal = vm_vec_dist_quick(&goal_point, &objp->pos);

	}	//	end while

	//	Set velocity (objp->mtype.phys_info.velocity) and orientation (objp->orient) for this object.
	player_path_set_orient_and_vel(objp, &goal_point);

}


//	------------------------------------------------------------------------------------------------------------------
//	Create path for player from current segment to goal segment.
void create_player_path_to_segment(int segnum)
{
	object		*objp = ConsoleObject;

	Player_path_length=0;
	Player_hide_index=-1;
	Player_cur_path_index=0;
	Player_following_path_flag=0;

	if (create_path_points(objp, objp->segnum, segnum, Point_segs_free_ptr, &Player_path_length, 100, 0, 0, -1, 0, NULL) == -1)
		con_printf(CON_DEBUG,"Unable to form path of length %i for myself\n", 100);

	Player_following_path_flag = 1;

	Player_hide_index = Point_segs_free_ptr - Point_segs;
	Player_cur_path_index = 0;
	Point_segs_free_ptr += Player_path_length;
	if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
		//Int3();	//	Contact Mike: This is curious, though not deadly. /eip++;g
		ai_reset_all_paths();
	}

}
int	Player_goal_segment = -1;

void check_create_player_path(void)
{
	if (Player_goal_segment != -1)
		create_player_path_to_segment(Player_goal_segment);

	Player_goal_segment = -1;
}

#endif

//	----------------------------------------------------------------------------------------------------------
//					DEBUG FUNCTIONS ENDED
//	----------------------------------------------------------------------------------------------------------


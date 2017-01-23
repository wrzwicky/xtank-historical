/*
** Xtank
**
** Copyright 1988 by Terry Donahue
**
** hit.c
*/

/*
$Author: aahz $
$Id: hit.c,v 2.4 1991/08/22 03:30:51 aahz Exp $

$Log: hit.c,v $
 * Revision 2.4  1991/08/22  03:30:51  aahz
 * avoid warning for the i860.
 *
 * Revision 2.3  1991/02/10  13:50:41  rpotter
 * bug fixes, display tweaks, non-restart fixes, header reorg.
 *
 * Revision 2.2  91/01/20  09:57:56  rpotter
 * complete rewrite of vehicle death, other tweaks
 * 
 * Revision 2.1  91/01/17  07:11:38  rpotter
 * lint warnings and a fix to update_vector()
 * 
 * Revision 2.0  91/01/17  02:09:34  rpotter
 * small changes
 * 
 * Revision 1.1  90/12/29  21:02:28  aahz
 * Initial revision
 * 
*/

#include "malloc.h"
#include <stdio.h>
#include "xtank.h"
#include "loc.h"
#include "disc.h"
#include "vstructs.h"
#include "sysdep.h"
#include "message.h"
#include "bullet.h"
#include "terminal.h"
#include "cosell.h"
#include "globals.h"


extern Map real_map;
extern Settings settings;


Side find_affected_side(v, angle)    
Vehicle *v;                     
float angle;                    
{                               
    Side s;                      
    float rel_angle;            

    
    rel_angle = drem(v->vector.heading + PI / 4 - angle, 2 * PI);       
    
    if (rel_angle < -PI / 2)    
        s = BACK;               
    else
      if (rel_angle < 0)        
        s = RIGHT;              
    else
      if (rel_angle < PI / 2)   
        s = FRONT;              
    else                        
        s = LEFT;               
    
    return (s);                 
}                               


/*
** Bounces the vehicles off each other and damages them both.
** Elasticity of bounce determined from bumper elasticities of vehicles.
** Damage based on kinetic energy and elasticity of collision.
** Better bumpers result in less damage in general and less to that vehicle.
*/
vehicle_hit_vehicle(v1, v2)
Vehicle *v1, *v2;
{
    float ang, bump1, bump2, elast;
    int dx, dy, damage;
    int damage1, damage2;
    Side side;
    int has_ramplate1, has_ramplate2;

#ifdef GDEBUG
    int itemp1, itemp2;
    static FILE *statfile = NULL;

    if (!statfile)
    {
	statfile = fopen("/tmp/xtank.stats", "w");
	if (!statfile)
	    rorre("cant open statfile");
    }
#endif

    /* Compute delta position */
    dx = v2->loc->x - v1->loc->x;
    dy = v2->loc->y - v1->loc->y;
    ang = ATAN2(dy, dx);

    /* Compute the elasticity of the collision, based on bumpers */
    bump1 = bumper_stat[v1->vdesc->bumpers].elasticity;
    bump2 = bumper_stat[v2->vdesc->bumpers].elasticity;

    /* Check to see if their are ram_plates */
    has_ramplate1 = FALSE;      

    if (v1->special[(int)RAMPLATE].status != SP_nonexistent)
    {                           
        side = find_affected_side(v1, ang);     
	if (side == FRONT)
        {                       
            has_ramplate1 = TRUE;       
            bump1 = -0.15;      /* ram plate is not flexible  */
        }                       
    }                           
    has_ramplate2 = FALSE;      
    if (v2->special[(int)RAMPLATE].status != SP_nonexistent)
    {                           
        side = find_affected_side(v2, PI - ang);        
	if (side == FRONT)
        {                       
            has_ramplate2 = TRUE;       
            bump2 = -0.15;      /* ram plate is not flexible */
        }                       
    }                           
    elast = .5 + bump1 + bump2;

    /* Bounce the vehicles off each other */
    bounce_vehicles(v1, v2, dx, dy, elast);

    /* Damage vehicle on the side (height = 0) */
    damage = bounce_damage(v1->vector.xspeed - v2->vector.xspeed,
			   v1->vector.yspeed - v2->vector.yspeed, elast);
    damage1 = damage2 = damage; 
    if (has_ramplate1)
    {                           
        damage1 /= 2;           
        damage2 *= 2;           
    }                           
    if (has_ramplate2)
    {                           
        damage2 /= 2;           
        damage1 *= 2;           
    }                           
    damage1 *= (1 - bump1);     
    damage2 *= (1 - bump2);     

#ifdef GDEBUG
    if (statfile)
    {
	itemp1 = damage * (1 - bump1);
	itemp2 = damage * (1 - bump2);

	fprintf(statfile, "\n\nVechicle #1:  '%s'  has_ramplate = %d  damage oldway - %d, damage newway - %d", v1->name, has_ramplate1, itemp1, damage1);
	fprintf(statfile, "\nVechicle #2:  '%s'  has_ramplate = %d  damage oldway - %d, damage newway - %d", v2->name, has_ramplate2, itemp2, damage2);
    }
#endif

    damage_vehicle(v1, v2, damage1, ang, 0);    
    damage_vehicle(v2, v1, damage2, PI - ang, 0);       

    if (settings.commentator)
	comment(COS_BIG_SMASH, damage * 3, v1, v2);
}

/*
** Bounces vehicle off of wall and damages them both.
** Damage based on kinetic energy and elasticity of collision.
** Better bumpers result in less damage in general and less to the vehicle.
** Wall in box (grid_x,grid_y) at direction dir.
*/
vehicle_hit_wall(v, grid_x, grid_y, dir)
Vehicle *v;
int grid_x, grid_y;
WallSide dir;
{
	float ang, bump, elast;
	int dx, dy, damage, damage1, damage2;
	int has_ramplate = FALSE;

#ifdef GDEBUG
	int itemp1, itemp2;
	static FILE *statfile = NULL;

	if (!statfile)
	{
		statfile = fopen("/tmp/xtank.wall.stats", "w");
		if (!statfile)
			rorre("cant open statfile");
	}
#endif

	/* Compute relative angle and location of the wall */
	switch (dir)
	{
		case NORTH:
			ang = -PI / 2;
			dx = 0;
			dy = -1;
			break;
		case SOUTH:
			ang = PI / 2;
			dx = 0;
			dy = 1;
			break;
		case WEST:
			ang = PI;
			dx = -1;
			dy = 0;
			break;
		case EAST:
			ang = 0;
			dx = 1;
			dy = 0;
			break;
	}

	/* Compute elasticity of collision based on bumpers */
	bump = bumper_stat[v->vdesc->bumpers].elasticity;

    if (v->special[(int)RAMPLATE].status != SP_nonexistent)
    {                           
		if (find_affected_side(v, ang) == FRONT)
        {                       
            has_ramplate = TRUE;
            bump = -0.15;       /* ram plate is not flexible  */
        }                       
    }                           
	/* Bounce vehicle off wall */
	elast = .5 + bump;
	bounce_vehicle_wall(v, dx, dy, elast);

	/* Damage vehicle and wall based on proper component of velocity */
	damage = bounce_damage(dx * v->vector.xspeed, dy * v->vector.yspeed, elast);

    damage1 = damage * (1 - bump);      
    damage2 = damage;           

#ifdef GDEBUG
	itemp1 = damage1;
	itemp2 = damage2;
#endif

	if (has_ramplate)
    {                           
        damage1 /= 2;           
        damage2 *= 2;           
	}

#ifdef GDEBUG
	if (statfile)
	{
		fprintf(statfile, "\n\nVechicle:  '%s' has_ramplate = %d  damage oldway - %d, damage newway - %d", v->name, has_ramplate, itemp1, damage1);
		fprintf(statfile, "\nWALL:  damage oldway - %d, damage newway - %d", itemp2, damage2);
	}
#endif

	if (settings.si.shocker_walls)
	{
		explode_location(v->loc, 1, EXP_ELECTRIC);
		damage1 += settings.si.shocker_walls;
	}

    damage_vehicle(v, (Vehicle *) NULL, damage1, ang, 0);       
    damage_wall(grid_x, grid_y, dir, damage2);  
}

/*
** Returns damage prop to kinetic energy, inversely prop to elasticity.
*/
bounce_damage(xspeed, yspeed, elast)
float xspeed, yspeed, elast;
{
	return (int) ((xspeed * xspeed + yspeed * yspeed) / (elast * 40.0));
}

/*
** Determines the result of bullet b hitting vehicle v at relative
** location (dx,dy).
**
** If the bullet is a disk, then the vehicle is made owner of the disc.
** Otherwise, the bullet explodes, and the vehicle is damaged.
*/
bul_hit_vehicle(v, b, dx, dy)
Vehicle *v;
Bullet *b;
int dx, dy;
{
    float angle;
    int damage, height;

    switch (b->type)
    {
      case DISC:
	set_disc_owner(b, v);
	if (settings.commentator)
	    comment(COS_OWNER_CHANGE, COS_IGNORE, v, (Vehicle *) NULL);
	break;
      case SLICK:
	if (v->vdesc->treads != HOVER_TREAD)
	{
	    v->status |= VS_sliding;
	    v->slide_count = 16;
	    if (settings.commentator)
		comment(COS_BEEN_SLICKED, 0, v, (Vehicle *) NULL);
	}
	break;
      default:
	/* Determine height of damage */
	if (b->type == MINE)
	{
	    height = -1;
	    if (v->vdesc->treads == HOVER_TREAD)
		--height;
	}
	else if (b->type == SEEKER)
	    height = 1;
	else
	    height = 0;

	if (height > -2)
	{
	    Box *bx;
	    int ShouldDamageVehicle = 1;
	    Loc *loc = v->loc;

	    bx = &real_map[loc->grid_x][loc->grid_y];
	    if (bx->type == PEACE)
	    {
		/* if the vehicle is close enough */
		if (! (loc->box_x < BOX_WIDTH / 2 - LANDMARK_WIDTH / 2 ||
		       loc->box_x > BOX_WIDTH / 2 + LANDMARK_WIDTH / 2 ||
		       loc->box_y < BOX_HEIGHT / 2 - LANDMARK_HEIGHT / 2 ||
		       loc->box_y > BOX_HEIGHT / 2 + LANDMARK_HEIGHT / 2))
		{
		    /* if the peace square is for YOUR team */
		    if (bx->team == v->team || bx->team == NEUTRAL)
		    {
			ShouldDamageVehicle = 0;
		    }
		}
	    }

	    if (ShouldDamageVehicle)
	    {
		/* Compute angle from center of vehicle that */
		/* the bullet hits */
		angle = ATAN2(dy, dx);

		/* Damage the vehicle, finding out how much */
		/* damage was done */
		damage = damage_vehicle(v, b->owner,
                                        weapon_stat[(int)b->type].damage,
					angle, height);
	    }
	    else
	    {
		damage = 0;
	    }

	    /* Make an explosion of the appropriate type */
	    /* for the damage */
	    explode(b, damage);
	}
    }
}

/*
** Explodes the bullet and damages the outpost if the bullet isn't a disc.
** Gives some points and money to the owner of the bullet, if any.
*/
bul_hit_outpost(b, bbox, grid_x, grid_y)
Bullet *b;
Box *bbox;
int grid_x, grid_y;
{
	int damage;

	if (b->type != DISC)
	{
        /* Compute damage of the bullet, award points, and damage the outpost
	   */
        damage = weapon_stat[(int)b->type].damage;

		/* Give points if shooter is neutral or on different team than
		   outpost */
		if (b->owner != (Vehicle *) NULL &&
				(b->owner->team == 0 || b->owner->team != bbox->team))
		{
			b->owner->owner->score += damage << 6;
			b->owner->owner->money += damage << 8;
		}
		if (damage > 0 && change_box(bbox, grid_x, grid_y))
		{
			/* Decrease the outpost's strength;  If it runs out, blow it up */
			if ((int)bbox->strength > damage)
            {
				bbox->strength -= damage;
            }
			else
			{
				bbox->type = NORMAL;
				explode_location(b->loc, 1, EXP_TANK);
			}
		}
		explode(b, damage);
	}
}

#define WALL_THICKNESS 0.5

/*
** Computes the location of the collision point, and determines
** the fate of bullet and wall depending on game settings.
**
** now returns TRUE if the bullet still exists, else false
*/
bul_hit_wall(b, grid_x, grid_y, dir)
    Bullet *b;
    int grid_x, grid_y;		/* coordinates of box containing wall flag */
    WallSide dir;		/* direction bullet hit wall */
{
    float dx, dy;
    int dam;

    /* Compute x and y distances from current location to point of contact */
    switch (dir)
    {
      case NORTH:
	dy = (BOX_HEIGHT - b->loc->box_y) + WALL_THICKNESS;
	dx = dy * b->xspeed / b->yspeed;
	break;
      case SOUTH:
	dy = -(b->loc->box_y + WALL_THICKNESS);
	dx = dy * b->xspeed / b->yspeed;
	break;
      case WEST:
	dx = (BOX_WIDTH - b->loc->box_x) + WALL_THICKNESS;
	dy = dx * b->yspeed / b->xspeed;
	break;
      case EAST:
	dx = -(b->loc->box_x + WALL_THICKNESS);
	dy = dx * b->yspeed / b->xspeed;
	break;
    }

    /* Change the bullet's location to the point of contact with the wall */
    update_loc(b->loc, b->loc, dx, dy);

    /* When a disc hits a wall, it stops orbiting its owner */
    if (b->type == DISC)
    {
	/* Notify the commentator if a vehicle lost the disc on a wall */
	if (settings.commentator && b->owner != (Vehicle *) NULL)
	    comment(COS_OWNER_CHANGE, COS_WALL_HIT, (Vehicle *) NULL,
		    (Vehicle *) NULL);
	if (b->owner != (Vehicle *) NULL)
	    b->thrower = b->owner->color;
	set_disc_owner(b, (Vehicle *) NULL);
    }
    /* If bullet is a disc, bounce the bullet. If wall isn't damaged by bullet,
       and ricochet is on, bounce the bullet, otherwise explode the bullet. */
    if (b->type == DISC)
    {
	bounce_bullet(b, dir, dx, dy);
	return TRUE;
    }
    else
    {
        dam = damage_wall(grid_x, grid_y, dir,
			  weapon_stat[(int)b->type].damage);
	if (dam == 0 && settings.si.ricochet == TRUE)
	{
	    bounce_bullet(b, dir, dx, dy);
	    return TRUE;
	}
	else
	{
	    explode(b, dam);
	    return FALSE;
	}
    }
}

/*
** Bounces bullet b against an obstacle at direction dir.  Causes bullet
** to hurt its owner.
**
** In the old days, this didn't do the update.  Thus even if the disk
** bounced it ended up on the wall in question.  Now it actually reflects,
** and thus can correctly be called a second time for the second wall.
*/
bounce_bullet(b, dir, dx, dy)
Bullet *b;
WallSide dir;
float dx, dy;
{
	b->hurt_owner = TRUE;
	switch (dir)
	{
		case NORTH:
		case SOUTH:
			update_loc(b->loc, b->loc, -dx, dy);
			b->yspeed = -b->yspeed;
			break;
		case WEST:
		case EAST:
			update_loc(b->loc, b->loc, dx, -dy);
			b->xspeed = -b->xspeed;
	}
}

/*
** Damages a wall in box at (x,y) depending on direction of damage
** Returns amount of damage done to wall.
*/
damage_wall(x, y, dir, damage)
    int x, y;
    WallSide dir;
    int damage;
{
    Box *b;
    int dest, wl;

    b = &real_map[x][y];

    /* See if it is a destructible wall */
    switch (dir)
    {
      case NORTH:
      case SOUTH:
	dest = b->flags & NORTH_DEST;
	if (dest == 0)
	    return 0;
	wl = NORTH_WALL;
	break;
      case WEST:
      case EAST:
	dest = b->flags & WEST_DEST;
	if (dest == 0)
	    return 0;
	wl = WEST_WALL;
    }

    /* Percent chance of destruction equals twice the damage */
    if (rnd(50) < damage && change_box(b, x, y)) {
	/* note: we do _not_ clear the destructible bit; it is needed to
	   correctly update the screen */
	b->flags &= ~wl;
    }

    return damage;
}

/*
** Damages the vehicle v in the location determined by the following:
**
** Height  Result
**   -1    damage bottom of vehicle
**   0     damage side of vehicle (depending on angle of damage to vehicle)
**   1     damage top of vehicle
**
** The vehicle is killed (by the damager) if the affected armor drops
** below 0.
*/
damage_vehicle(v, damager, damage, angle, height)
Vehicle *v, *damager;
int damage;
float angle;
int height;
{
    static int hits_off[7] = {0, 0, 1, 1, 2, 2, 3};
    int *side;
    Side s;

    /* Vehicles don't take damage if no_wear is set */
    if (settings.si.no_wear)
	return 0;

    side = v->armor.side;
    /* Find out which side the damage affects */
    switch (height)
    {
      case 0:
	/* Damage hit either FRONT, BACK, LEFT, or RIGHT side of vehicle */
	s = find_affected_side(v, angle);   
	break;
      case -1:
	/* Damage hit BOTTOM side of vehicle */
	s = BOTTOM;
	break;
      case 1:
	/* Damage hit TOP side of vehicle */
	s = TOP;
	break;
    }

    /* Subtract the hits off of the armor, and apply the damage */
    damage = MAX(0, damage - hits_off[v->armor.type]);
    if (damage > 0)
    {
        side[(int)s] -= damage;

	/* See if the armor is pierced */
        if (side[(int)s] < 0)
	{
            side[(int)s] = 0;
	    kill_vehicle(v, damager);
	}
    }
    /* Return the number of points of damage done to the vehicle */
    return damage;
}


/*
** Computes new velocities for the vehicles after a collision with
** v2 having a relative position of (dx,dy) with respect to v1.
*/
bounce_vehicles(v1, v2, dx, dy, elast)
Vehicle *v1, *v2;
int dx, dy;
float elast;
{
	Vehicle *temp;
	float v1x, v1y, v2x, v2y;
	float slope, mratio, c;

	/* Make v1 be on the left */
	if (dx < 0)
	{
		dx = -dx;
		dy = -dy;
		temp = v1;
		v1 = v2;
		v2 = temp;
	}
	v1x = v1->vector.xspeed;
	v1y = v1->vector.yspeed;
	v2x = v2->vector.xspeed;
	v2y = v2->vector.yspeed;

	if (dx == 0)
		slope = (dy > 0) ? 999.0 : -999.0;
	else
		slope = (float) dy / (float) dx;

	if (v2->vdesc->weight == 0)
		mratio = 999.0;
	else
		mratio = v1->vdesc->weight / v2->vdesc->weight;

	c = ((v1x - v2x) + slope * (v1y - v2y)) /
		((1.0 + mratio) * (slope * slope + 1.0));
	c *= 1.0 + elast;

	assign_speed(&v1->vector, v1x - c, v1y - c * slope);
	assign_speed(&v2->vector, v2x + c * mratio, v2y + c * mratio * slope);
}

/*
** Computes new velocity for the vehicle after a collision with a wall
** at a relative position of (dx,dy).
*/
bounce_vehicle_wall(v, dx, dy, elast)
Vehicle *v;
int dx, dy;
float elast;
{
	float vx, vy;
	float slope, c;

	vx = v->vector.xspeed;
	vy = v->vector.yspeed;

	if (dx < 0)
	{
		dx = -dx;
		dy = -dy;
	}
	if (dx == 0)
		slope = (dy > 0) ? 999.0 : -999.0;
	else
		slope = (float) dy / (float) dx;

	c = (vx + slope * vy) / (slope * slope + 1.0);
	c *= 1.0 + elast;

	assign_speed(&v->vector, vx - c, vy - c * slope);
}

/*
** Computes the speed and angle from the xspeed and yspeed and sets the
** the xspeed, yspeed, speed, and angle of the vector.
*/
assign_speed(vec, xsp, ysp)
Vector *vec;
float xsp, ysp;
{
	vec->xspeed = xsp;
	vec->yspeed = ysp;
	vec->speed = hypot(ysp, xsp);
	vec->angle = ATAN2(ysp, xsp);
}
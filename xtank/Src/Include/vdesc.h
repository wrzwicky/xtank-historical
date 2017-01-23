/* vdesc.h - part of Xtank */

#ifndef _VDESC_H_
#define _VDESC_H_

#include "types.h"
#include "object.h"
#include "vehicleparts.h"


typedef struct {
    Angle angle;		/* angle that the turret is pointing */
    Angle desired_angle;	/* angle driver wants turret to point */
    Spin  angle_flag;		/* which way the turret is rotating */
    Angle turn_rate;		/* how fast the turret can rotate */
    int   rot;			/* picture to show on the screen */
    int   old_rot;		/* picture to erase from the screen */
    Object *obj;		/* pointer to object for the turret */
} Turret;

typedef struct {
    WeaponType type;		/* weapon type (determines bullet type) */
    int   hits;			/* # hit points left in weapon */
    MountLocation mount;	/* location where weapon is mounted */
    int   reload_counter;	/* # frames until next shot can be fired */
    int   ammo;			/* number of ammo units left in weapon */
    Flag  status;		/* status of weapon (on/off,no_ammo) */
} Weapon;

typedef struct {
    int   type;
    int   side[MAX_SIDES];
    int   max_side;
} Armor;

/* describes a vehicle design */
typedef struct {
    char  name[MAX_STRING];
    char  designer[MAX_STRING];
    int   body;
    int   engine;
    int   num_weapons;
    WeaponType weapon[MAX_WEAPONS];
    MountLocation mount[MAX_WEAPONS];
    Armor armor;
    Flag  specials;
    int   heat_sinks;
    int   suspension;
    int   treads;
    int   bumpers;
    /* the following values are derived from the above */
    float max_speed;		/* speed limit imposed by body drag and engine
				   power */
    float engine_acc;		/* acceleration limit imposed by weight and
				   engine power (only affects speeding up) */
    float tread_acc;		/* acceleration limit imposed by the tread
				   friction (ground friction is NOT taken into
				   account here) */
    float acc;			/* the minimum of the above two (here for
				   compatibility, not very useful) */
    int   handling;
    int   weight;		/* total */
    int   space;		/* total */
    int   cost;			/* total */
} Vdesc;


#endif ndef _VDESC_H_
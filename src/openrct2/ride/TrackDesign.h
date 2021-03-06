/*****************************************************************************
 * Copyright (c) 2014-2018 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef _TRACK_DESIGN_H_
#define _TRACK_DESIGN_H_

#include "../common.h"
#include "../object/Object.h"
#include "../rct12/RCT12.h"
#include "../rct2/RCT2.h"
#include "../world/Map.h"
#include "Vehicle.h"

#define TRACK_PREVIEW_IMAGE_SIZE (370 * 217)

#pragma pack(push, 1)
/* Maze Element entry   size: 0x04 */
struct rct_td6_maze_element {
    union {
        uint32 all;
        struct {
            sint8 x;
            sint8 y;
            union {
                uint16 maze_entry;
                struct{
                    uint8 direction;
                    uint8 type;
                };
            };
        };
    };
};
assert_struct_size(rct_td6_maze_element, 0x04);

/* Track Element entry  size: 0x02 */
struct rct_td6_track_element {
    uint8 type;                         // 0x00
    uint8 flags;                        // 0x01
};
assert_struct_size(rct_td6_track_element, 0x02);

/* Track Entrance entry size: 0x06 */
struct rct_td6_entrance_element {
    sint8 z;                            // 0x00
    uint8 direction;                    // 0x01
    sint16 x;                           // 0x02
    sint16 y;                           // 0x04
};
assert_struct_size(rct_td6_entrance_element, 0x06);

/* Track Scenery entry  size: 0x16 */
struct rct_td6_scenery_element {
    rct_object_entry scenery_object;    // 0x00
    sint8 x;                            // 0x10
    sint8 y;                            // 0x11
    sint8 z;                            // 0x12
    uint8 flags;                        // 0x13 direction quadrant tertiary colour
    uint8 primary_colour;               // 0x14
    uint8 secondary_colour;             // 0x15
};
assert_struct_size(rct_td6_scenery_element, 0x16);

/**
 * Track design structure.
 * size: 0x4E72B
 */
struct rct_track_td6 {
    uint8 type;                                     // 0x00
    uint8 vehicle_type;
    union{
        // After loading the track this is converted to
        // a cost but before its a flags register
        money32 cost;                               // 0x02
        uint32 flags;                               // 0x02
    };
    union{
        // After loading the track this is converted to
        // a flags register
        uint8 ride_mode;                            // 0x06
        uint8 track_flags;                          // 0x06
    };
    uint8 version_and_colour_scheme;                // 0x07 0b0000_VVCC
    rct_vehicle_colour vehicle_colours[RCT2_MAX_CARS_PER_TRAIN]; // 0x08
    union{
        uint8 pad_48;
        uint8 track_spine_colour_rct1;              // 0x48
    };
    union{
        uint8 entrance_style;                       // 0x49
        uint8 track_rail_colour_rct1;               // 0x49
    };
    union{
        uint8 total_air_time;                       // 0x4A
        uint8 track_support_colour_rct1;            // 0x4A
    };
    uint8 depart_flags;                             // 0x4B
    uint8 number_of_trains;                         // 0x4C
    uint8 number_of_cars_per_train;                 // 0x4D
    uint8 min_waiting_time;                         // 0x4E
    uint8 max_waiting_time;                         // 0x4F
    uint8 operation_setting;
    sint8 max_speed;                                // 0x51
    sint8 average_speed;                            // 0x52
    uint16 ride_length;                             // 0x53
    uint8 max_positive_vertical_g;                  // 0x55
    sint8 max_negative_vertical_g;                  // 0x56
    uint8 max_lateral_g;                            // 0x57
    union {
        uint8 inversions;                           // 0x58
        uint8 holes;                                // 0x58
    };
    uint8 drops;                                    // 0x59
    uint8 highest_drop_height;                      // 0x5A
    uint8 excitement;                               // 0x5B
    uint8 intensity;                                // 0x5C
    uint8 nausea;                                   // 0x5D
    money16 upkeep_cost;                            // 0x5E
    uint8 track_spine_colour[RCT12_NUM_COLOUR_SCHEMES]; // 0x60
    uint8 track_rail_colour[RCT12_NUM_COLOUR_SCHEMES]; // 0x64
    uint8 track_support_colour[RCT12_NUM_COLOUR_SCHEMES]; // 0x68
    uint32 flags2;                                  // 0x6C
    rct_object_entry vehicle_object;                // 0x70
    uint8 space_required_x;                         // 0x80
    uint8 space_required_y;                         // 0x81
    uint8 vehicle_additional_colour[RCT2_MAX_CARS_PER_TRAIN]; // 0x82
    uint8 lift_hill_speed_num_circuits;             // 0xA2 0bCCCL_LLLL
    void *elements;                                 // 0xA3 (data starts here in file)
    size_t elementsSize;

    rct_td6_maze_element        *maze_elements;
    rct_td6_track_element       *track_elements;
    rct_td6_entrance_element    *entrance_elements;
    rct_td6_scenery_element     *scenery_elements;

    utf8 *name;
};
//Warning: improper struct size in comment
#ifdef PLATFORM_32BIT
assert_struct_size(rct_track_td6, 0xbf);
#endif
#pragma pack(pop)

// Only written to in RCT2, not used in OpenRCT2. All of these are elements that had to be invented in RCT1.
enum : uint32
{
    TRACK_FLAGS_CONTAINS_VERTICAL_LOOP = (1 << 7),
    TRACK_FLAGS_CONTAINS_INLINE_TWIST = (1 << 17),
    TRACK_FLAGS_CONTAINS_HALF_LOOP = (1 << 18),
    TRACK_FLAGS_CONTAINS_CORKSCREW = (1 << 19),
    TRACK_FLAGS_CONTAINS_WATER_SPLASH = (1 << 27),
    TRACK_FLAGS_CONTAINS_BARREL_ROLL = (1 << 29),
    TRACK_FLAGS_CONTAINS_POWERED_LIFT = (1 << 30),
    TRACK_FLAGS_CONTAINS_LARGE_HALF_LOOP = (1u << 31),
};

enum : uint32
{
    TRACK_FLAGS2_CONTAINS_LOG_FLUME_REVERSER = (1 << 1),
    TRACK_FLAGS2_SIX_FLAGS_RIDE_DEPRECATED = (1u << 31)     // Not used anymore.
};

enum
{
    TDPF_PLACE_SCENERY = 1 << 0,
};

enum
{
    TRACK_DESIGN_FLAG_SCENERY_UNAVAILABLE = (1 << 0),
    TRACK_DESIGN_FLAG_HAS_SCENERY = (1 << 1),
    TRACK_DESIGN_FLAG_VEHICLE_UNAVAILABLE = (1 << 2),
};

enum {
    PTD_OPERATION_DRAW_OUTLINES,
    PTD_OPERATION_1,
    PTD_OPERATION_2,
    PTD_OPERATION_GET_PLACE_Z,
    PTD_OPERATION_4,
    PTD_OPERATION_GET_COST,
    PTD_OPERATION_CLEAR_OUTLINES,
};

enum {
    MAZE_ELEMENT_TYPE_MAZE_TRACK = 0,
    MAZE_ELEMENT_TYPE_ENTRANCE = (1 << 3),
    MAZE_ELEMENT_TYPE_EXIT = (1 << 7)
};

extern rct_track_td6 *gActiveTrackDesign;
extern bool gTrackDesignSceneryToggle;
extern LocationXYZ16 gTrackPreviewMin;
extern LocationXYZ16 gTrackPreviewMax;
extern LocationXYZ16 gTrackPreviewOrigin;

extern bool byte_9D8150;

extern bool gTrackDesignSaveMode;
extern uint8 gTrackDesignSaveRideIndex;

rct_track_td6 *track_design_open(const utf8 *path);
void track_design_dispose(rct_track_td6 *td6);

void track_design_mirror(rct_track_td6 *td6);

sint32 place_virtual_track(rct_track_td6 *td6, uint8 ptdOperation, bool placeScenery, uint8 rideIndex, sint16 x, sint16 y, sint16 z);

void game_command_place_track_design(sint32* eax, sint32* ebx, sint32* ecx, sint32* edx, sint32* esi, sint32* edi, sint32* ebp);
void game_command_place_maze_design(sint32* eax, sint32* ebx, sint32* ecx, sint32* edx, sint32* esi, sint32* edi, sint32* ebp);

///////////////////////////////////////////////////////////////////////////////
// Track design preview
///////////////////////////////////////////////////////////////////////////////
void track_design_draw_preview(rct_track_td6 *td6, uint8 *pixels);

///////////////////////////////////////////////////////////////////////////////
// Track design saving
///////////////////////////////////////////////////////////////////////////////
void track_design_save_init();
void track_design_save_reset_scenery();
bool track_design_save_contains_tile_element(const rct_tile_element * tileElement);
void track_design_save_select_nearby_scenery(sint32 rideIndex);
void track_design_save_select_tile_element(sint32 interactionType, sint32 x, sint32 y, rct_tile_element *tileElement, bool collect);
bool track_design_save(uint8 rideIndex);
bool track_design_save_to_file(const utf8 *path);

bool track_design_are_entrance_and_exit_placed();

#endif

/*****************************************************************************
 * Copyright (c) 2014-2018 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <cstring>

#include "../config/Config.h"
#include "../Context.h"
#include "../core/Math.hpp"
#include "../drawing/Drawing.h"
#include "../Game.h"
#include "../Input.h"
#include "../OpenRCT2.h"
#include "../paint/Paint.h"
#include "../peep/Staff.h"
#include "../ride/Ride.h"
#include "../ride/TrackDesign.h"
#include "../world/Climate.h"
#include "../world/Map.h"
#include "../world/Sprite.h"
#include "Colour.h"
#include "Viewport.h"
#include "Window.h"
#include "Window_internal.h"

//#define DEBUG_SHOW_DIRTY_BOX
uint8 gShowGridLinesRefCount;
uint8 gShowLandRightsRefCount;
uint8 gShowConstuctionRightsRefCount;

rct_viewport g_viewport_list[MAX_VIEWPORT_COUNT];
rct_viewport *g_music_tracking_viewport;

static rct_tile_element *_interaction_element = nullptr;

sint16 gSavedViewX;
sint16 gSavedViewY;
uint8 gSavedViewZoom;
uint8 gSavedViewRotation;

paint_entry *gNextFreePaintStruct;
uint8 gCurrentRotation;
uint32 gCurrentViewportFlags = 0;

static uint32 _currentImageType;

static rct_drawpixelinfo _viewportDpi1;
static rct_drawpixelinfo _viewportDpi2;
static uint8 _interactionSpriteType;
static sint16 _interactionMapX;
static sint16 _interactionMapY;
static uint16 _unk9AC154;

static void viewport_paint_column(rct_drawpixelinfo * dpi, uint32 viewFlags);
static void viewport_paint_weather_gloom(rct_drawpixelinfo * dpi);

/**
 * This is not a viewport function. It is used to setup many variables for
 * multiple things.
 *  rct2: 0x006E6EAC
 */
void viewport_init_all()
{
    if (!gOpenRCT2NoGraphics)
    {
        colours_init_maps();
    }

    window_init_all();

    // Setting up viewports
    for (sint32 i = 0; i < MAX_VIEWPORT_COUNT; i++) {
        g_viewport_list[i].width = 0;
    }

    // ?
    input_reset_flags();
    input_set_state(INPUT_STATE_RESET);
    gPressedWidget.window_classification = 255;
    gPickupPeepImage = UINT32_MAX;
    reset_tooltip_not_shown();
    gMapSelectFlags = 0;
    gStaffDrawPatrolAreas = 0xFFFF;
    textinput_cancel();
}

/**
 * Converts between 3d point of a sprite to 2d coordinates for centring on that
 * sprite
 *  rct2: 0x006EB0C1
 * x : ax
 * y : bx
 * z : cx
 * out_x : ax
 * out_y : bx
 */
void centre_2d_coordinates(sint32 x, sint32 y, sint32 z, sint32 * out_x, sint32 * out_y, rct_viewport * viewport){
    sint32 start_x = x;

    LocationXYZ16 coord_3d = {
        /* .x = */ (sint16)x,
        /* .y = */ (sint16)y,
        /* .z = */ (sint16)z
    };

    LocationXY16 coord_2d = coordinate_3d_to_2d(&coord_3d, get_current_rotation());

    // If the start location was invalid
    // propagate the invalid location to the output.
    // This fixes a bug that caused the game to enter an infinite loop.
    if (start_x == LOCATION_NULL)
    {
        *out_x = LOCATION_NULL;
        *out_y = 0;
        return;
    }

    *out_x = coord_2d.x - viewport->view_width / 2;
    *out_y = coord_2d.y - viewport->view_height / 2;
}

/**
* Viewport will look at sprite or at coordinates as specified in flags 0b_1X
* for sprite 0b_0X for coordinates
*
*  rct2: 0x006EB009
*  x:      ax
*  y:      eax (top 16)
*  width:  bx
*  height: ebx (top 16)
*  zoom:   cl (8 bits)
*  centre_x: edx lower 16 bits
*  centre_y: edx upper 16 bits
*  centre_z: ecx upper 16 bits
*  sprite: edx lower 16 bits
*  flags:  edx top most 2 bits 0b_X1 for zoom clear see below for 2nd bit.
*  w:      esi
*/
void viewport_create(rct_window *w, sint32 x, sint32 y, sint32 width, sint32 height, sint32 zoom, sint32 centre_x, sint32 centre_y, sint32 centre_z, char flags, sint16 sprite)
{
    rct_viewport* viewport = nullptr;
    for (sint32 i = 0; i < MAX_VIEWPORT_COUNT; i++) {
        if (g_viewport_list[i].width == 0) {
            viewport = &g_viewport_list[i];
            break;
        }
    }
    if (viewport == nullptr) {
        log_error("No more viewport slots left to allocate.");
        return;
    }

    viewport->x = x;
    viewport->y = y;
    viewport->width = width;
    viewport->height = height;

    if (!(flags & VIEWPORT_FOCUS_TYPE_COORDINATE)){
        zoom = 0;
    }

    viewport->view_width = width << zoom;
    viewport->view_height = height << zoom;
    viewport->zoom = zoom;
    viewport->flags = 0;

    if (gConfigGeneral.always_show_gridlines)
        viewport->flags |= VIEWPORT_FLAG_GRIDLINES;
    w->viewport = viewport;

    if (flags & VIEWPORT_FOCUS_TYPE_SPRITE){
        w->viewport_target_sprite = sprite;
        rct_sprite* centre_sprite = get_sprite(sprite);
        centre_x = centre_sprite->unknown.x;
        centre_y = centre_sprite->unknown.y;
        centre_z = centre_sprite->unknown.z;
    }
    else{
        w->viewport_target_sprite = SPRITE_INDEX_NULL;
    }

    sint32 view_x, view_y;
    centre_2d_coordinates(centre_x, centre_y, centre_z, &view_x, &view_y, viewport);

    w->saved_view_x = view_x;
    w->saved_view_y = view_y;
    viewport->view_x = view_x;
    viewport->view_y = view_y;

    viewport_update_pointers();
}

/**
 *
 *  rct2: 0x006EE510
 */
void viewport_update_pointers()
{

}

/**
 *
 *  rct2: 0x00689174
 * edx is assumed to be (and always is) the current rotation, so it is not
 * needed as parameter.
 */
void viewport_adjust_for_map_height(sint16* x, sint16* y, sint16 *z)
{
    sint16 start_x = *x;
    sint16 start_y = *y;
    sint16 height = 0;

    uint32 rotation = get_current_rotation();
    LocationXY16 pos;
    for (sint32 i = 0; i < 6; i++) {
        pos = viewport_coord_to_map_coord(start_x, start_y, height);
        height = tile_element_height((0xFFFF) & pos.x, (0xFFFF) & pos.y);

        // HACK: This is to prevent the x and y values being set to values outside
        // of the map. This can happen when the height is larger than the map size.
        sint16 max = gMapSizeMinus2;
        if (pos.x > max && pos.y > max) {
            sint32 x_corr[] = { -1, 1, 1, -1 };
            sint32 y_corr[] = { -1, -1, 1, 1 };
            pos.x += x_corr[rotation] * height;
            pos.y += y_corr[rotation] * height;
        }
    }

    *x = pos.x;
    *y = pos.y;
    *z = height;
}

/*
*  rct2: 0x006E7FF3
*/
static void viewport_redraw_after_shift(rct_drawpixelinfo *dpi, rct_window *window, rct_viewport *viewport, sint32 x, sint32 y)
{
    // sub-divide by intersecting windows
    if (window < gWindowNextSlot)
    {
        // skip current window and non-intersecting windows
        if (viewport == window->viewport                                ||
            viewport->x + viewport->width  <= window->x                 ||
            viewport->x                    >= window->x + window->width ||
            viewport->y + viewport->height <= window->y                 ||
            viewport->y                    >= window->y + window->height){
            viewport_redraw_after_shift(dpi, window + 1, viewport, x, y);
            return;
        }

        // save viewport
        rct_viewport view_copy;
        memcpy(&view_copy, viewport, sizeof(rct_viewport));

        if (viewport->x < window->x)
        {
            viewport->width = window->x - viewport->x;
            viewport->view_width = viewport->width << viewport->zoom;
            viewport_redraw_after_shift(dpi, window, viewport, x, y);

            viewport->x += viewport->width;
            viewport->view_x += viewport->width << viewport->zoom;
            viewport->width = view_copy.width - viewport->width;
            viewport->view_width = viewport->width << viewport->zoom;
            viewport_redraw_after_shift(dpi, window, viewport, x, y);
        }
        else if (viewport->x + viewport->width > window->x + window->width)
        {
            viewport->width = window->x + window->width - viewport->x;
            viewport->view_width = viewport->width << viewport->zoom;
            viewport_redraw_after_shift(dpi, window, viewport, x, y);

            viewport->x += viewport->width;
            viewport->view_x += viewport->width << viewport->zoom;
            viewport->width = view_copy.width - viewport->width;
            viewport->view_width = viewport->width << viewport->zoom;
            viewport_redraw_after_shift(dpi, window, viewport, x, y);
        }
        else if (viewport->y < window->y)
        {
            viewport->height = window->y - viewport->y;
            viewport->view_width = viewport->width << viewport->zoom;
            viewport_redraw_after_shift(dpi, window, viewport, x, y);

            viewport->y += viewport->height;
            viewport->view_y += viewport->height << viewport->zoom;
            viewport->height = view_copy.height - viewport->height;
            viewport->view_width = viewport->width << viewport->zoom;
            viewport_redraw_after_shift(dpi, window, viewport, x, y);
        }
        else if (viewport->y + viewport->height > window->y + window->height)
        {
            viewport->height = window->y + window->height - viewport->y;
            viewport->view_width = viewport->width << viewport->zoom;
            viewport_redraw_after_shift(dpi, window, viewport, x, y);

            viewport->y += viewport->height;
            viewport->view_y += viewport->height << viewport->zoom;
            viewport->height = view_copy.height - viewport->height;
            viewport->view_width = viewport->width << viewport->zoom;
            viewport_redraw_after_shift(dpi, window, viewport, x, y);
        }

        // restore viewport
        memcpy(viewport, &view_copy, sizeof(rct_viewport));
    }
    else
    {
        sint16 left = viewport->x;
        sint16 right = viewport->x + viewport->width;
        sint16 top = viewport->y;
        sint16 bottom = viewport->y + viewport->height;

        // if moved more than the viewport size
        if (abs(x) < viewport->width && abs(y) < viewport->height)
        {
            // update whole block ?
            drawing_engine_copy_rect(viewport->x, viewport->y, viewport->width, viewport->height, x, y);

            if (x > 0)
            {
                // draw left
                sint16 _right = viewport->x + x;
                window_draw_all(dpi, left, top, _right, bottom);
                left += x;
            }
            else if (x < 0)
            {
                // draw right
                sint16 _left = viewport->x + viewport->width + x;
                window_draw_all(dpi, _left, top, right, bottom);
                right += x;
            }

            if (y > 0)
            {
                // draw top
                bottom = viewport->y + y;
                window_draw_all(dpi, left, top, right, bottom);
            }
            else if (y < 0)
            {
                // draw bottom
                top = viewport->y + viewport->height + y;
                window_draw_all(dpi, left, top, right, bottom);
            }
        }
        else
        {
            // redraw whole viewport
            window_draw_all(dpi, left, top, right, bottom);
        }
    }
}

static void viewport_shift_pixels(rct_drawpixelinfo *dpi, rct_window* w, rct_viewport* viewport, sint16 x_diff, sint16 y_diff)
{
    rct_window* orignal_w = w;
    sint32 left = 0, right = 0, top = 0, bottom = 0;

    for (; w < gWindowNextSlot; w++){
        if (!(w->flags & WF_TRANSPARENT)) continue;
        if (w->viewport == viewport) continue;

        if (viewport->x + viewport->width <= w->x)continue;
        if (w->x + w->width <= viewport->x) continue;

        if (viewport->y + viewport->height <= w->y)continue;
        if (w->y + w->height <= viewport->y) continue;

        left = w->x;
        right = w->x + w->width;
        top = w->y;
        bottom = w->y + w->height;

        if (left < viewport->x)left = viewport->x;
        if (right > viewport->x + viewport->width) right = viewport->x + viewport->width;

        if (top < viewport->y)top = viewport->y;
        if (bottom > viewport->y + viewport->height) bottom = viewport->y + viewport->height;

        if (left >= right) continue;
        if (top >= bottom) continue;

        window_draw_all(dpi, left, top, right, bottom);
    }

    w = orignal_w;
    viewport_redraw_after_shift(dpi, w, viewport, x_diff, y_diff);
}

static void viewport_move(sint16 x, sint16 y, rct_window* w, rct_viewport* viewport)
{
    uint8 zoom = (1 << viewport->zoom);

    // Note: do not do the subtraction and then divide!
    // Note: Due to arithmetic shift != /zoom a shift will have to be used
    // hopefully when 0x006E7FF3 is finished this can be converted to /zoom.
    sint16 x_diff = (viewport->view_x >> viewport->zoom) - (x >> viewport->zoom);
    sint16 y_diff = (viewport->view_y >> viewport->zoom) - (y >> viewport->zoom);

    viewport->view_x = x;
    viewport->view_y = y;

    // If no change in viewing area
    if ((!x_diff) && (!y_diff))return;

    if (w->flags & WF_7){
        sint32 left = std::max<sint32>(viewport->x, 0);
        sint32 top = std::max<sint32>(viewport->y, 0);
        sint32 right = std::min<sint32>(viewport->x + viewport->width, context_get_width());
        sint32 bottom = std::min<sint32>(viewport->y + viewport->height, context_get_height());

        if (left >= right) return;
        if (top >= bottom) return;

        if (drawing_engine_has_dirty_optimisations()) {
            rct_drawpixelinfo *dpi = drawing_engine_get_dpi();
            window_draw_all(dpi, left, top, right, bottom);
            return;
        }
    }

    rct_viewport view_copy;
    memcpy(&view_copy, viewport, sizeof(rct_viewport));

    if (viewport->x < 0){
        viewport->width += viewport->x;
        viewport->view_width += viewport->x * zoom;
        viewport->view_x -= viewport->x * zoom;
        viewport->x = 0;
    }

    sint32 eax = viewport->x + viewport->width - context_get_width();
    if (eax > 0){
        viewport->width -= eax;
        viewport->view_width -= eax * zoom;
    }

    if (viewport->width <= 0){
        memcpy(viewport, &view_copy, sizeof(rct_viewport));
        return;
    }

    if (viewport->y < 0){
        viewport->height += viewport->y;
        viewport->view_height += viewport->y * zoom;
        viewport->view_y -= viewport->y * zoom;
        viewport->y = 0;
    }

    eax = viewport->y + viewport->height - context_get_height();
    if (eax > 0){
        viewport->height -= eax;
        viewport->view_height -= eax * zoom;
    }

    if (viewport->height <= 0){
        memcpy(viewport, &view_copy, sizeof(rct_viewport));
        return;
    }

    if (drawing_engine_has_dirty_optimisations()) {
        rct_drawpixelinfo *dpi = drawing_engine_get_dpi();
        viewport_shift_pixels(dpi, w, viewport, x_diff, y_diff);
    }

    memcpy(viewport, &view_copy, sizeof(rct_viewport));
}

//rct2: 0x006E7A15
static void viewport_set_underground_flag(sint32 underground, rct_window* window, rct_viewport* viewport)
{
    if (window->classification != WC_MAIN_WINDOW)
    {
        if (!underground)
        {
            sint32 bit = viewport->flags & VIEWPORT_FLAG_UNDERGROUND_INSIDE;
            viewport->flags &= ~VIEWPORT_FLAG_UNDERGROUND_INSIDE;
            if (!bit) return;
        }
        else
        {
            sint32 bit = viewport->flags & VIEWPORT_FLAG_UNDERGROUND_INSIDE;
            viewport->flags |= VIEWPORT_FLAG_UNDERGROUND_INSIDE;
            if (bit) return;
        }
        window_invalidate(window);
    }
}

/**
 *
 *  rct2: 0x006E7A3A
 */
void viewport_update_position(rct_window * window)
{
    window_event_resize_call(window);

    rct_viewport * viewport = window->viewport;
    if (!viewport) return;

    if (window->viewport_smart_follow_sprite != SPRITE_INDEX_NULL)
    {
        viewport_update_smart_sprite_follow(window);
    }

    if (window->viewport_target_sprite != SPRITE_INDEX_NULL)
    {
        viewport_update_sprite_follow(window);
        return;
    }

    viewport_set_underground_flag(0, window, viewport);

    sint16 x = window->saved_view_x + viewport->view_width / 2;
    sint16 y = window->saved_view_y + viewport->view_height / 2;
    LocationXY16 mapCoord;

    mapCoord = viewport_coord_to_map_coord(x, y, 0);

    // Clamp to the map minimum value
    sint32 at_map_edge = 0;
    if (mapCoord.x < MAP_MINIMUM_X_Y)
    {
        mapCoord.x = MAP_MINIMUM_X_Y;
        at_map_edge = 1;
    }
    if (mapCoord.y < MAP_MINIMUM_X_Y)
    {
        mapCoord.y = MAP_MINIMUM_X_Y;
        at_map_edge = 1;
    }

    // Clamp to the map maximum value (scenario specific)
    if (mapCoord.x > gMapSizeMinus2)
    {
        mapCoord.x = gMapSizeMinus2;
        at_map_edge = 1;
    }
    if (mapCoord.y > gMapSizeMinus2)
    {
        mapCoord.y = gMapSizeMinus2;
        at_map_edge = 1;
    }

    if (at_map_edge)
    {
        sint32 _2d_x, _2d_y;
        centre_2d_coordinates(mapCoord.x, mapCoord.y, 0, &_2d_x, &_2d_y, viewport);

        window->saved_view_x = _2d_x;
        window->saved_view_y = _2d_y;
    }

    x = window->saved_view_x;
    y = window->saved_view_y;
    if (window->flags & WF_SCROLLING_TO_LOCATION)
    {
        // Moves the viewport if focusing in on an item
        uint8 flags = 0;
        x -= viewport->view_x;
        if (x < 0)
        {
            x = -x;
            flags |= 1;
        }
        y -= viewport->view_y;
        if (y < 0)
        {
            y = -y;
            flags |= 2;
        }
        x = (x + 7) / 8;
        y = (y + 7) / 8;

        //If we are at the final zoom position
        if (!x && !y)
        {
            window->flags &= ~WF_SCROLLING_TO_LOCATION;
        }
        if (flags & 1)
        {
            x = -x;
        }
        if (flags & 2)
        {
            y = -y;
        }
        x += viewport->view_x;
        y += viewport->view_y;
    }

    viewport_move(x, y, window, viewport);
}

void viewport_update_sprite_follow(rct_window *window)
{
    if (window->viewport_target_sprite != SPRITE_INDEX_NULL && window->viewport) {
        rct_sprite* sprite = get_sprite(window->viewport_target_sprite);

        sint32 height = (tile_element_height(0xFFFF & sprite->unknown.x, 0xFFFF & sprite->unknown.y) & 0xFFFF) - 16;
        sint32 underground = sprite->unknown.z < height;

        viewport_set_underground_flag(underground, window, window->viewport);

        sint32 centre_x, centre_y;
        centre_2d_coordinates(sprite->unknown.x, sprite->unknown.y, sprite->unknown.z, &centre_x, &centre_y,
                              window->viewport);

        window->saved_view_x = centre_x;
        window->saved_view_y = centre_y;
        viewport_move(centre_x, centre_y, window, window->viewport);
    }
}

void viewport_update_smart_sprite_follow(rct_window * window)
{
    rct_sprite * sprite = try_get_sprite(window->viewport_smart_follow_sprite);
    if (sprite == nullptr)
    {
        window->viewport_smart_follow_sprite = SPRITE_INDEX_NULL;
        window->viewport_target_sprite = SPRITE_INDEX_NULL;
    }
    else if (sprite->unknown.sprite_identifier == SPRITE_IDENTIFIER_PEEP)
    {
        rct_peep * peep = GET_PEEP(window->viewport_smart_follow_sprite);

        if (peep->type == PEEP_TYPE_GUEST)
            viewport_update_smart_guest_follow(window, peep);
        else if (peep->type == PEEP_TYPE_STAFF)
            viewport_update_smart_staff_follow(window, peep);
    }
    else if (sprite->unknown.sprite_identifier == SPRITE_IDENTIFIER_VEHICLE)
    {
        viewport_update_smart_vehicle_follow(window);
    }
    else if (sprite->unknown.sprite_identifier == SPRITE_IDENTIFIER_MISC ||
             sprite->unknown.sprite_identifier == SPRITE_IDENTIFIER_LITTER)
    {
        window->viewport_focus_sprite.sprite_id = window->viewport_smart_follow_sprite;
        window->viewport_target_sprite = window->viewport_smart_follow_sprite;
    }
    else
    {
        window->viewport_smart_follow_sprite = SPRITE_INDEX_NULL;
        window->viewport_target_sprite = SPRITE_INDEX_NULL;
    }
}

void viewport_update_smart_guest_follow(rct_window * window, rct_peep * peep)
{
    union
    {
        sprite_focus sprite;
        coordinate_focus coordinate;
    } focus = {}; // The focus will be either a sprite or a coordinate.

    focus.sprite.sprite_id = window->viewport_smart_follow_sprite;

    if (peep->state == PEEP_STATE_PICKED)
    {
        //focus.sprite.sprite_id = SPRITE_INDEX_NULL;
        window->viewport_smart_follow_sprite = SPRITE_INDEX_NULL;
        window->viewport_target_sprite = SPRITE_INDEX_NULL;
        return;
    }
    else
    {
        uint8 final_check = 1;
        if (peep->state == PEEP_STATE_ON_RIDE
            || peep->state == PEEP_STATE_ENTERING_RIDE
            || (peep->state == PEEP_STATE_LEAVING_RIDE && peep->x == LOCATION_NULL))
        {

            Ride * ride = get_ride(peep->current_ride);
            if (ride->lifecycle_flags & RIDE_LIFECYCLE_ON_TRACK)
            {
                rct_vehicle * train = GET_VEHICLE(ride->vehicles[peep->current_train]);
                sint32 car = peep->current_car;

                for (; car != 0; car--)
                {
                    train = GET_VEHICLE(train->next_vehicle_on_train);
                }

                focus.sprite.sprite_id = train->sprite_index;
                final_check = 0;
            }
        }
        if (peep->x == LOCATION_NULL && final_check)
        {
            Ride * ride = get_ride(peep->current_ride);
            sint32 x = ride->overall_view.x * 32 + 16;
            sint32 y = ride->overall_view.y * 32 + 16;
            sint32 height = tile_element_height(x, y);
            height += 32;
            focus.coordinate.x = x;
            focus.coordinate.y = y;
            focus.coordinate.z = height;
            focus.sprite.type |= VIEWPORT_FOCUS_TYPE_COORDINATE;
        }
        else
        {
            focus.sprite.type |= VIEWPORT_FOCUS_TYPE_SPRITE | VIEWPORT_FOCUS_TYPE_COORDINATE;
            focus.sprite.pad_486 &= 0xFFFF;
        }
        focus.coordinate.rotation = get_current_rotation();
    }

    window->viewport_focus_sprite = focus.sprite;
    window->viewport_target_sprite = window->viewport_focus_sprite.sprite_id;
}

void viewport_update_smart_staff_follow(rct_window * window, rct_peep * peep)
{
    sprite_focus focus = {};

    focus.sprite_id = window->viewport_smart_follow_sprite;

    if (peep->state == PEEP_STATE_PICKED)
    {
        //focus.sprite.sprite_id = SPRITE_INDEX_NULL;
        window->viewport_smart_follow_sprite = SPRITE_INDEX_NULL;
        window->viewport_target_sprite = SPRITE_INDEX_NULL;
        return;
    }
    else
    {
        focus.type |= VIEWPORT_FOCUS_TYPE_SPRITE | VIEWPORT_FOCUS_TYPE_COORDINATE;
    }

    window->viewport_focus_sprite = focus;
    window->viewport_target_sprite = window->viewport_focus_sprite.sprite_id;
}

void viewport_update_smart_vehicle_follow(rct_window * window)
{
    // Can be expanded in the future if needed
    sprite_focus focus = {};

    focus.sprite_id = window->viewport_smart_follow_sprite;

    window->viewport_focus_sprite = focus;
    window->viewport_target_sprite = window->viewport_focus_sprite.sprite_id;
}

/**
 *
 *  rct2: 0x00685C02
 *  ax: left
 *  bx: top
 *  dx: right
 *  esi: viewport
 *  edi: dpi
 *  ebp: bottom
 */
void viewport_render(rct_drawpixelinfo *dpi, rct_viewport *viewport, sint32 left, sint32 top, sint32 right, sint32 bottom)
{
    if (right <= viewport->x) return;
    if (bottom <= viewport->y) return;
    if (left >= viewport->x + viewport->width)return;
    if (top >= viewport->y + viewport->height)return;

#ifdef DEBUG_SHOW_DIRTY_BOX
    sint32 l = left, t = top, r = right, b = bottom;
#endif

    left = std::max<sint32>(left - viewport->x, 0);
    right = std::min<sint32>(right - viewport->x, viewport->width);
    top = std::max<sint32>(top - viewport->y, 0);
    bottom = std::min<sint32>(bottom - viewport->y, viewport->height);

    left <<= viewport->zoom;
    right <<= viewport->zoom;
    top <<= viewport->zoom;
    bottom <<= viewport->zoom;

    left += viewport->view_x;
    right += viewport->view_x;
    top += viewport->view_y;
    bottom += viewport->view_y;

    viewport_paint(viewport, dpi, left, top, right, bottom);

#ifdef DEBUG_SHOW_DIRTY_BOX
    if (viewport != g_viewport_list){
        gfx_fill_rect_inset(dpi, l, t, r-1, b-1, 0x2, INSET_RECT_F_30);
        return;
    }
#endif
}

/**
 *
 *  rct2: 0x00685CBF
 *  eax: left
 *  ebx: top
 *  edx: right
 *  esi: viewport
 *  edi: dpi
 *  ebp: bottom
 */
void viewport_paint(rct_viewport* viewport, rct_drawpixelinfo* dpi, sint16 left, sint16 top, sint16 right, sint16 bottom)
{
    uint32 viewFlags = viewport->flags;
    uint16 width = right - left;
    uint16 height = bottom - top;
    uint16 bitmask = 0xFFFF & (0xFFFF << viewport->zoom);

    width &= bitmask;
    height &= bitmask;
    left &= bitmask;
    top &= bitmask;
    right = left + width;
    bottom = top + height;

    sint16 x = (sint16)(left - (sint16)(viewport->view_x & bitmask));
    x >>= viewport->zoom;
    x += viewport->x;

    sint16 y = (sint16)(top - (sint16)(viewport->view_y & bitmask));
    y >>= viewport->zoom;
    y += viewport->y;

    rct_drawpixelinfo dpi1;
    dpi1.bits = dpi->bits + (x - dpi->x) + ((y - dpi->y) * (dpi->width + dpi->pitch));
    dpi1.x = left;
    dpi1.y = top;
    dpi1.width = width;
    dpi1.height = height;
    dpi1.pitch = (dpi->width + dpi->pitch) - (width >> viewport->zoom);
    dpi1.zoom_level = viewport->zoom;

    // make sure, the compare operation is done in sint16 to avoid the loop becoming an infiniteloop.
    // this as well as the [x += 32] in the loop causes signed integer overflow -> undefined behaviour.
    sint16 rightBorder = dpi1.x + dpi1.width;

    // Splits the area into 32 pixel columns and renders them
    for (x = floor2(dpi1.x, 32); x < rightBorder; x += 32) {
        rct_drawpixelinfo dpi2 = dpi1;
        if (x >= dpi2.x) {
            sint16 leftPitch = x - dpi2.x;
            dpi2.width -= leftPitch;
            dpi2.bits += leftPitch >> dpi2.zoom_level;
            dpi2.pitch += leftPitch >> dpi2.zoom_level;
            dpi2.x = x;
        }

        sint16 paintRight = dpi2.x + dpi2.width;
        if (paintRight >= x + 32) {
            sint16 rightPitch = paintRight - x - 32;
            paintRight -= rightPitch;
            dpi2.pitch += rightPitch >> dpi2.zoom_level;
        }
        dpi2.width = paintRight - dpi2.x;

        viewport_paint_column(&dpi2, viewFlags);
    }
}

static void viewport_paint_column(rct_drawpixelinfo * dpi, uint32 viewFlags)
{
    gCurrentViewportFlags = viewFlags;

    if (viewFlags & (VIEWPORT_FLAG_HIDE_VERTICAL | VIEWPORT_FLAG_HIDE_BASE | VIEWPORT_FLAG_UNDERGROUND_INSIDE | VIEWPORT_FLAG_CLIP_VIEW)) {
        uint8 colour = 10;
        if (viewFlags & VIEWPORT_FLAG_INVISIBLE_SPRITES) {
            colour = 0;
        }
        gfx_clear(dpi, colour);
    }

    paint_session * session = paint_session_alloc(dpi);
    paint_session_generate(session);
    paint_struct ps = paint_session_arrange(session);
    paint_draw_structs(dpi, &ps, viewFlags);
    paint_session_free(session);

    if (gConfigGeneral.render_weather_gloom &&
        !gTrackDesignSaveMode &&
        !(viewFlags & VIEWPORT_FLAG_INVISIBLE_SPRITES) &&
        !(viewFlags & VIEWPORT_FLAG_HIGHLIGHT_PATH_ISSUES)
    ) {
        viewport_paint_weather_gloom(dpi);
    }

    if (session->PSStringHead != nullptr) {
        paint_draw_money_structs(dpi, session->PSStringHead);
    }
}

static void viewport_paint_weather_gloom(rct_drawpixelinfo * dpi)
{
    auto paletteId = climate_get_weather_gloom_palette_id(gClimateCurrent);
    if (paletteId != PALETTE_NULL)
    {
        gfx_filter_rect(
            dpi,
            dpi->x,
            dpi->y,
            dpi->width + dpi->x - 1,
            dpi->height + dpi->y - 1,
            paletteId);
    }
}

/**
 *
 *  rct2: 0x0068958D
 */
void screen_pos_to_map_pos(sint16 *x, sint16 *y, sint32 *direction)
{
    screen_get_map_xy(*x, *y, x, y, nullptr);
    if (*x == LOCATION_NULL)
        return;

    sint32 my_direction;
    sint32 dist_from_centre_x = abs(*x % 32);
    sint32 dist_from_centre_y = abs(*y % 32);
    if (dist_from_centre_x > 8 && dist_from_centre_x < 24 &&
        dist_from_centre_y > 8 && dist_from_centre_y < 24) {
        my_direction = 4;
    } else {
        sint16 mod_x = *x & 0x1F;
        sint16 mod_y = *y & 0x1F;
        if (mod_x <= 16) {
            if (mod_y < 16) {
                my_direction = 2;
            } else {
                my_direction = 3;
            }
        } else {
            if (mod_y < 16) {
                my_direction = 1;
            } else {
                my_direction = 0;
            }
        }
    }

    *x = *x & ~0x1F;
    *y = *y & ~0x1F;
    if (direction != nullptr) *direction = my_direction;
}

LocationXY16 screen_coord_to_viewport_coord(rct_viewport *viewport, uint16 x, uint16 y)
{
    LocationXY16 ret;
    ret.x = ((x - viewport->x) << viewport->zoom) + viewport->view_x;
    ret.y = ((y - viewport->y) << viewport->zoom) + viewport->view_y;
    return ret;
}

LocationXY16 viewport_coord_to_map_coord(sint32 x, sint32 y, sint32 z)
{
    LocationXY16 ret = {};
    switch (get_current_rotation()) {
    case 0:
        ret.x = -x / 2 + y + z;
        ret.y = x / 2 + y + z;
        break;
    case 1:
        ret.x = -x / 2 - y - z;
        ret.y = -x / 2 + y + z;
        break;
    case 2:
        ret.x = x / 2 - y - z;
        ret.y = -x / 2 - y - z;
        break;
    case 3:
        ret.x = x / 2 + y + z;
        ret.y = x / 2 - y - z;
        break;
    }
    return ret;
}

/**
 *
 *  rct2: 0x00664689
 */
void show_gridlines()
{
    if (gShowGridLinesRefCount == 0) {
        rct_window *mainWindow = window_get_main();
        if (mainWindow != nullptr) {
            if (!(mainWindow->viewport->flags & VIEWPORT_FLAG_GRIDLINES)) {
                mainWindow->viewport->flags |= VIEWPORT_FLAG_GRIDLINES;
                window_invalidate(mainWindow);
            }
        }
    }
    gShowGridLinesRefCount++;
}

/**
 *
 *  rct2: 0x006646B4
 */
void hide_gridlines()
{
    gShowGridLinesRefCount--;
    if (gShowGridLinesRefCount == 0) {
        rct_window *mainWindow = window_get_main();
        if (mainWindow != nullptr) {
            if (!gConfigGeneral.always_show_gridlines) {
                mainWindow->viewport->flags &= ~VIEWPORT_FLAG_GRIDLINES;
                window_invalidate(mainWindow);
            }
        }
    }
}

/**
 *
 *  rct2: 0x00664E8E
 */
void show_land_rights()
{
    if (gShowLandRightsRefCount == 0) {
        rct_window *mainWindow = window_get_main();
        if (mainWindow != nullptr) {
            if (!(mainWindow->viewport->flags & VIEWPORT_FLAG_LAND_OWNERSHIP)) {
                mainWindow->viewport->flags |= VIEWPORT_FLAG_LAND_OWNERSHIP;
                window_invalidate(mainWindow);
            }
        }
    }
    gShowLandRightsRefCount++;
}

/**
 *
 *  rct2: 0x00664EB9
 */
void hide_land_rights()
{
    gShowLandRightsRefCount--;
    if (gShowLandRightsRefCount == 0) {
        rct_window *mainWindow = window_get_main();
        if (mainWindow != nullptr) {
            if (mainWindow->viewport->flags & VIEWPORT_FLAG_LAND_OWNERSHIP) {
                mainWindow->viewport->flags &= ~VIEWPORT_FLAG_LAND_OWNERSHIP;
                window_invalidate(mainWindow);
            }
        }
    }
}

/**
 *
 *  rct2: 0x00664EDD
 */
void show_construction_rights()
{
    if (gShowConstuctionRightsRefCount == 0) {
        rct_window *mainWindow = window_get_main();
        if (mainWindow != nullptr) {
            if (!(mainWindow->viewport->flags & VIEWPORT_FLAG_CONSTRUCTION_RIGHTS)) {
                mainWindow->viewport->flags |= VIEWPORT_FLAG_CONSTRUCTION_RIGHTS;
                window_invalidate(mainWindow);
            }
        }
    }
    gShowConstuctionRightsRefCount++;
}

/**
 *
 *  rct2: 0x00664F08
 */
void hide_construction_rights()
{
    gShowConstuctionRightsRefCount--;
    if (gShowConstuctionRightsRefCount == 0) {
        rct_window *mainWindow = window_get_main();
        if (mainWindow != nullptr) {
            if (mainWindow->viewport->flags & VIEWPORT_FLAG_CONSTRUCTION_RIGHTS) {
                mainWindow->viewport->flags &= ~VIEWPORT_FLAG_CONSTRUCTION_RIGHTS;
                window_invalidate(mainWindow);
            }
        }
    }
}

/**
 *
 *  rct2: 0x006CB70A
 */
void viewport_set_visibility(uint8 mode)
{
    rct_window* window = window_get_main();

    if (window != nullptr) {
        rct_viewport* edi = window->viewport;
        uint32 invalidate = 0;

        switch (mode) {
        case 0: { //Set all these flags to 0, and invalidate if any were active
            uint32 mask = VIEWPORT_FLAG_UNDERGROUND_INSIDE | VIEWPORT_FLAG_SEETHROUGH_RIDES |
                VIEWPORT_FLAG_SEETHROUGH_SCENERY | VIEWPORT_FLAG_SEETHROUGH_PATHS | VIEWPORT_FLAG_INVISIBLE_SUPPORTS |
                VIEWPORT_FLAG_LAND_HEIGHTS | VIEWPORT_FLAG_TRACK_HEIGHTS |
                VIEWPORT_FLAG_PATH_HEIGHTS | VIEWPORT_FLAG_INVISIBLE_PEEPS |
                VIEWPORT_FLAG_HIDE_BASE | VIEWPORT_FLAG_HIDE_VERTICAL;

            invalidate += edi->flags & mask;
            edi->flags &= ~mask;
            break;
        }
        case 1: //6CB79D
        case 4: //6CB7C4
            //Set underground on, invalidate if it was off
            invalidate += !(edi->flags & VIEWPORT_FLAG_UNDERGROUND_INSIDE);
            edi->flags |= VIEWPORT_FLAG_UNDERGROUND_INSIDE;
            break;
        case 2: //6CB7EB
            //Set track heights on, invalidate if off
            invalidate += !(edi->flags & VIEWPORT_FLAG_TRACK_HEIGHTS);
            edi->flags |= VIEWPORT_FLAG_TRACK_HEIGHTS;
            break;
        case 3: //6CB7B1
        case 5: //6CB7D8
            //Set underground off, invalidate if it was on
            invalidate += edi->flags & VIEWPORT_FLAG_UNDERGROUND_INSIDE;
            edi->flags &= ~((uint16)VIEWPORT_FLAG_UNDERGROUND_INSIDE);
            break;
        }
        if (invalidate != 0)
            window_invalidate(window);
    }
}

/**
 * Stores some info about the element pointed at, if requested for this particular type through the interaction mask.
 * Originally checked 0x0141F569 at start
 *  rct2: 0x00688697
 */
static void store_interaction_info(paint_struct *ps)
{
    if (ps->sprite_type == VIEWPORT_INTERACTION_ITEM_NONE
        || ps->sprite_type == 11 // 11 as a type seems to not exist, maybe part of the typo mentioned later on.
        || ps->sprite_type > VIEWPORT_INTERACTION_ITEM_BANNER) return;

    uint16 mask;
    if (ps->sprite_type == VIEWPORT_INTERACTION_ITEM_BANNER)
        // I think CS made a typo here. Let's replicate the original behaviour.
        mask = 1 << (ps->sprite_type - 3);
    else
        mask = 1 << (ps->sprite_type - 1);

    if (!(_unk9AC154 & mask)) {
        _interactionSpriteType = ps->sprite_type;
        _interactionMapX = ps->map_x;
        _interactionMapY = ps->map_y;
        _interaction_element = ps->tileElement;
    }
}

/**
 * rct2: 0x00679236, 0x00679662, 0x00679B0D, 0x00679FF1
 */
static bool pixel_is_present_bmp(uint32 imageType, const rct_g1_element * g1, const uint8 * index, const uint8 * palette)
{
    // Probably used to check for corruption
    if (!(g1->flags & G1_FLAG_BMP)) {
        return false;
    }

    if (imageType & IMAGE_TYPE_REMAP) {
        return palette[*index] != 0;
    }

    if (imageType & IMAGE_TYPE_TRANSPARENT) {
        return false;
    }

    return (*index != 0);
}

/**
 * rct2: 0x0067933B, 0x00679788, 0x00679C4A, 0x0067A117
 */
static bool is_pixel_present_rle(const uint8 *esi, sint16 x_start_point, sint16 y_start_point, sint32 round) {
    const uint8 *ebx = esi + ((uint16 *) esi)[y_start_point];

    uint8 last_data_line = 0;
    while (!last_data_line) {
        sint32 no_pixels = *ebx++;
        uint8 gap_size = *ebx++;

        last_data_line = no_pixels & 0x80;

        no_pixels &= 0x7F;

        ebx += no_pixels;

        if (round > 1) {
            if (gap_size % 2) {
                gap_size++;
                no_pixels--;
                if (no_pixels == 0) {
                    continue;
                }
            }
        }

        if (round == 4) {
            if (gap_size % 4) {
                gap_size += 2;
                no_pixels -= 2;
                if (no_pixels <= 0) {
                    continue;
                }
            }
        }

        sint32 x_start = gap_size - x_start_point;
        if (x_start <= 0) {
            no_pixels += x_start;
            if (no_pixels <= 0) {
                continue;
            }

            x_start = 0;
        } else {
            // Do nothing?
        }

        x_start += no_pixels;
        x_start--;
        if (x_start > 0) {
            no_pixels -= x_start;
            if (no_pixels <= 0) {
                continue;
            }
        }

        if (round > 1) {
            // This matches the original implementation, but allows empty lines to cause false positives on zoom 0
            if (ceil2(no_pixels, round) == 0) continue;
        }

        return true;
    }

    return false;
}

/**
 * rct2: 0x00679074
 *
 * @param dpi (edi)
 * @param imageId (ebx)
 * @param x (cx)
 * @param y (dx)
 * @return value originally stored in 0x00141F569
 */
static bool sub_679074(rct_drawpixelinfo *dpi, sint32 imageId, sint16 x, sint16 y, const uint8 * palette)
{
    const rct_g1_element * g1 = gfx_get_g1_element(imageId & 0x7FFFF);
    if (g1 == nullptr)
    {
        return false;
    }

    if (dpi->zoom_level != 0) {
        if (g1->flags & G1_FLAG_NO_ZOOM_DRAW) {
            return false;
        }

        if (g1->flags & G1_FLAG_HAS_ZOOM_SPRITE) {
            // TODO: SAR in dpi done with `>> 1`, in coordinates with `/ 2`
            rct_drawpixelinfo zoomed_dpi = {
                /* .bits = */ dpi->bits,
                /* .x = */ (sint16)(dpi->x >> 1),
                /* .y = */ (sint16)(dpi->y >> 1),
                /* .height = */ dpi->height,
                /* .width = */ dpi->width,
                /* .pitch = */ dpi->pitch,
                /* .zoom_level = */ (uint16)(dpi->zoom_level - 1)
            };

            return sub_679074(&zoomed_dpi, imageId - g1->zoomed_offset, x / 2, y / 2, palette);
        }
    }

    sint32 round = 1 << dpi->zoom_level;

    if (g1->flags & G1_FLAG_RLE_COMPRESSION) {
        y -= (round - 1);
    }

    y += g1->y_offset;
    sint16 yStartPoint = 0;
    sint16 height = g1->height;
    if (dpi->zoom_level != 0) {
        if (height % 2) {
            height--;
            yStartPoint++;
        }

        if (dpi->zoom_level == 2) {
            if (height % 4) {
                height -= 2;
                yStartPoint += 2;
            }
        }

        if (height == 0) {
            return false;
        }
    }

    y = floor2(y, round);
    sint16 yEndPoint = height;
    y -= dpi->y;
    if (y < 0) {
        yEndPoint += y;
        if (yEndPoint <= 0) {
            return false;
        }

        yStartPoint -= y;
        y = 0;
    }

    y += yEndPoint;
    y--;
    if (y > 0) {
        yEndPoint -= y;
        if (yEndPoint <= 0) {
            return false;
        }
    }

    sint16 xStartPoint = 0;
    sint16 xEndPoint = g1->width;

    x += g1->x_offset;
    x = floor2(x, round);
    x -= dpi->x;
    if (x < 0) {
        xEndPoint += x;
        if (xEndPoint <= 0) {
            return false;
        }

        xStartPoint -= x;
        x = 0;
    }

    x += xEndPoint;
    x--;
    if (x > 0) {
        xEndPoint -= x;
        if (xEndPoint <= 0) {
            return false;
        }
    }

    if (g1->flags & G1_FLAG_RLE_COMPRESSION) {
        return is_pixel_present_rle(g1->offset, xStartPoint, yStartPoint, round);
    }

    uint8 *offset = g1->offset + (yStartPoint * g1->width) + xStartPoint;
    uint32 imageType = _currentImageType;

    if (!(g1->flags & G1_FLAG_1)) {
        return pixel_is_present_bmp(imageType, g1, offset, palette);
    }

    // Adding assert here, possibly dead code below. Remove after some time.
    assert(false);

    // The code below is untested.
    sint32 total_no_pixels = g1->width * g1->height;
    uint8 *source_pointer = g1->offset;
    uint8 *new_source_pointer_start = (uint8 *)malloc(total_no_pixels);
    uint8 *new_source_pointer = (*&new_source_pointer_start);// 0x9E3D28;
    intptr_t ebx1;
    sint32 ecx;
    while (total_no_pixels > 0) {
        sint8 no_pixels = *source_pointer;
        if (no_pixels >= 0) {
            source_pointer++;
            total_no_pixels -= no_pixels;
            memcpy((char *) new_source_pointer, (char *) source_pointer, no_pixels);
            new_source_pointer += no_pixels;
            source_pointer += no_pixels;
            continue;
        }
        ecx = no_pixels;
        no_pixels &= 0x7;
        ecx >>= 3;//SAR
        uintptr_t eax = ((sint32) no_pixels) << 8;
        ecx = -ecx;//Odd
        eax = (eax & 0xFF00) + *(source_pointer + 1);
        total_no_pixels -= ecx;
        source_pointer += 2;
        ebx1 = (uintptr_t) new_source_pointer - eax;
        eax = (uintptr_t) source_pointer;
        source_pointer = (uint8 *) ebx1;
        ebx1 = eax;
        eax = 0;
        memcpy((char *) new_source_pointer, (char *) source_pointer, ecx);
        new_source_pointer += ecx;
        source_pointer = (uint8 *) ebx1;
    }

    bool output = pixel_is_present_bmp(imageType, g1, new_source_pointer_start + (uintptr_t) offset, palette);
    free(new_source_pointer_start);

    return output;
}

/**
 *
 *  rct2: 0x00679023
 */
static bool sub_679023(rct_drawpixelinfo *dpi, sint32 imageId, sint32 x, sint32 y)
{
    const uint8 * palette = nullptr;
    imageId &= ~IMAGE_TYPE_TRANSPARENT;
    if (imageId & IMAGE_TYPE_REMAP) {
        _currentImageType = IMAGE_TYPE_REMAP;
        sint32 index = (imageId >> 19) & 0x7F;
        if (imageId & IMAGE_TYPE_REMAP_2_PLUS) {
            index &= 0x1F;
        }
        sint32 g1Index = palette_to_g1_offset[index];
        const rct_g1_element * g1 = gfx_get_g1_element(g1Index);
        if (g1 != nullptr)
        {
            palette = g1->offset;
        }
    } else {
        _currentImageType = 0;
    }
    return sub_679074(dpi, imageId, x, y, palette);
}

/**
 *
 *  rct2: 0x0068862C
 */
void sub_68862C(rct_drawpixelinfo * dpi, paint_struct * ps)
{
    while ((ps = ps->next_quadrant_ps) != nullptr) {
        paint_struct * old_ps = ps;
        paint_struct * next_ps = ps;
        while (next_ps != nullptr) {
            ps = next_ps;
            if (sub_679023(dpi, ps->image_id, ps->x, ps->y))
                store_interaction_info(ps);

            next_ps = ps->var_20;
        }

        for (attached_paint_struct *attached_ps = ps->attached_ps; attached_ps != nullptr; attached_ps = attached_ps->next) {
            if (sub_679023(
                dpi,
                attached_ps->image_id,
                (attached_ps->x + ps->x) & 0xFFFF,
                (attached_ps->y + ps->y) & 0xFFFF
            )) {
                store_interaction_info(ps);
            }
        }

        ps = old_ps;
    }
}

/**
 *
 *  rct2: 0x00685ADC
 * screenX: eax
 * screenY: ebx
 * flags: edx
 * x: ax
 * y: cx
 * interactionType: bl
 * tileElement: edx
 * viewport: edi
 */
void get_map_coordinates_from_pos(sint32 screenX, sint32 screenY, sint32 flags, sint16 *x, sint16 *y, sint32 *interactionType, rct_tile_element **tileElement, rct_viewport **viewport)
{
    rct_window* window = window_find_from_point(screenX, screenY);
    get_map_coordinates_from_pos_window(window, screenX, screenY, flags, x, y, interactionType, tileElement, viewport);
}

void get_map_coordinates_from_pos_window(rct_window * window, sint32 screenX, sint32 screenY, sint32 flags, sint16 * x, sint16 * y,
    sint32 * interactionType, rct_tile_element ** tileElement, rct_viewport ** viewport)
{
    _unk9AC154 = flags & 0xFFFF;
    _interactionSpriteType = 0;
    if (window != nullptr && window->viewport != nullptr)
    {
        rct_viewport* myviewport = window->viewport;
        screenX -= (sint32)myviewport->x;
        screenY -= (sint32)myviewport->y;
        if (screenX >= 0 && screenX < (sint32)myviewport->width && screenY >= 0 && screenY < (sint32)myviewport->height)
        {
            screenX <<= myviewport->zoom;
            screenY <<= myviewport->zoom;
            screenX += (sint32)myviewport->view_x;
            screenY += (sint32)myviewport->view_y;
            _viewportDpi1.zoom_level = myviewport->zoom;
            screenX &= (0xFFFF << myviewport->zoom) & 0xFFFF;
            screenY &= (0xFFFF << myviewport->zoom) & 0xFFFF;
            _viewportDpi1.x = screenX;
            _viewportDpi1.y = screenY;
            rct_drawpixelinfo* dpi = &_viewportDpi2;
            dpi->y = _viewportDpi1.y;
            dpi->height = 1;
            dpi->zoom_level = _viewportDpi1.zoom_level;
            dpi->x = _viewportDpi1.x;
            dpi->width = 1;

            paint_session * session = paint_session_alloc(dpi);
            paint_session_generate(session);
            paint_struct ps = paint_session_arrange(session);
            sub_68862C(dpi, &ps);
            paint_session_free(session);
        }
        if (viewport != nullptr) *viewport = myviewport;
    }
    if (interactionType != nullptr) *interactionType = _interactionSpriteType;
    if (x != nullptr) *x = _interactionMapX;
    if (y != nullptr) *y = _interactionMapY;
    if (tileElement != nullptr) *tileElement = _interaction_element;
}

/**
 * Left, top, right and bottom represent 2D map coordinates at zoom 0.
 */
void viewport_invalidate(rct_viewport *viewport, sint32 left, sint32 top, sint32 right, sint32 bottom)
{
    // if unknown viewport visibility, use the containing window to discover the status
    if (viewport->visibility == VC_UNKNOWN)
    {
        for (rct_window *w = g_window_list; w < gWindowNextSlot; w++)
        {
            if (w->classification != WC_MAIN_WINDOW && w->viewport != nullptr && w->viewport == viewport)
            {
                // note, window_is_visible will update viewport->visibility, so this should have a low hit count
                if (!window_is_visible(w)) {
                    return;
                }
            }
        }
    }

    if (viewport->visibility == VC_COVERED) return;

    sint32 viewportLeft = viewport->view_x;
    sint32 viewportTop = viewport->view_y;
    sint32 viewportRight = viewport->view_x + viewport->view_width;
    sint32 viewportBottom = viewport->view_y + viewport->view_height;
    if (right > viewportLeft && bottom > viewportTop) {
        left = std::max(left, viewportLeft);
        top = std::max(top, viewportTop);
        right = std::min(right, viewportRight);
        bottom = std::min(bottom, viewportBottom);

        uint8 zoom = 1 << viewport->zoom;
        left -= viewportLeft;
        top -= viewportTop;
        right -= viewportLeft;
        bottom -= viewportTop;
        left /= zoom;
        top /= zoom;
        right /= zoom;
        bottom /= zoom;
        left += viewport->x;
        top += viewport->y;
        right += viewport->x;
        bottom += viewport->y;
        gfx_set_dirty_blocks(left, top, right, bottom);
    }
}

static rct_viewport *viewport_find_from_point(sint32 screenX, sint32 screenY)
{
    rct_window *w = window_find_from_point(screenX, screenY);
    if (w == nullptr)
        return nullptr;

    rct_viewport *viewport = w->viewport;
    if (viewport == nullptr)
        return nullptr;

    if (screenX < viewport->x || screenY < viewport->y)
        return nullptr;
    if (screenX >= viewport->x + viewport->width || screenY >= viewport->y + viewport->height)
        return nullptr;

    return viewport;
}

/**
 *
 *  rct2: 0x00688972
 * In:
 *      screen_x: eax
 *      screen_y: ebx
 * Out:
 *      x: ax
 *      y: bx
 *      tile_element: edx ?
 *      viewport: edi
 */
void screen_get_map_xy(sint32 screenX, sint32 screenY, sint16 *x, sint16 *y, rct_viewport **viewport) {
    sint16 my_x, my_y;
    sint32 interactionType;
    rct_viewport *myViewport = nullptr;
    get_map_coordinates_from_pos(screenX, screenY, VIEWPORT_INTERACTION_MASK_TERRAIN, &my_x, &my_y, &interactionType, nullptr, &myViewport);
    if (interactionType == VIEWPORT_INTERACTION_ITEM_NONE) {
        *x = LOCATION_NULL;
        return;
    }

    LocationXY16 start_vp_pos = screen_coord_to_viewport_coord(myViewport, screenX, screenY);
    LocationXY16 map_pos = { (sint16)(my_x + 16), (sint16)(my_y + 16) };

    for (sint32 i = 0; i < 5; i++) {
        sint32 z = tile_element_height(map_pos.x, map_pos.y);
        map_pos = viewport_coord_to_map_coord(start_vp_pos.x, start_vp_pos.y, z);
        map_pos.x = Math::Clamp<sint16>(map_pos.x, my_x, my_x + 31);
        map_pos.y = Math::Clamp<sint16>(map_pos.y, my_y, my_y + 31);
    }

    *x = map_pos.x;
    *y = map_pos.y;

    if (viewport != nullptr) *viewport = myViewport;
}

/**
 *
 *  rct2: 0x006894D4
 */
void screen_get_map_xy_with_z(sint16 screenX, sint16 screenY, sint16 z, sint16 *mapX, sint16 *mapY)
{
    rct_viewport *viewport = viewport_find_from_point(screenX, screenY);
    if (viewport == nullptr) {
        *mapX = LOCATION_NULL;
        return;
    }

    screenX = viewport->view_x + ((screenX - viewport->x) << viewport->zoom);
    screenY = viewport->view_y + ((screenY - viewport->y) << viewport->zoom);

    LocationXY16 mapPosition = viewport_coord_to_map_coord(screenX, screenY + z, 0);
    if (mapPosition.x < 0 || mapPosition.x >= (256 * 32) || mapPosition.y < 0 || mapPosition.y >(256 * 32)) {
        *mapX = LOCATION_NULL;
        return;
    }

    *mapX = mapPosition.x;
    *mapY = mapPosition.y;
}

/**
 *
 *  rct2: 0x00689604
 */
void screen_get_map_xy_quadrant(sint16 screenX, sint16 screenY, sint16 *mapX, sint16 *mapY, uint8 *quadrant)
{
    screen_get_map_xy(screenX, screenY, mapX, mapY, nullptr);
    if (*mapX == LOCATION_NULL)
        return;

    *quadrant = map_get_tile_quadrant(*mapX, *mapY);
    *mapX = floor2(*mapX, 32);
    *mapY = floor2(*mapY, 32);
}

/**
 *
 *  rct2: 0x0068964B
 */
void screen_get_map_xy_quadrant_with_z(sint16 screenX, sint16 screenY, sint16 z, sint16 *mapX, sint16 *mapY, uint8 *quadrant)
{
    screen_get_map_xy_with_z(screenX, screenY, z, mapX, mapY);
    if (*mapX == LOCATION_NULL)
        return;

    *quadrant = map_get_tile_quadrant(*mapX, *mapY);
    *mapX = floor2(*mapX, 32);
    *mapY = floor2(*mapY, 32);
}

/**
 *
 *  rct2: 0x00689692
 */
void screen_get_map_xy_side(sint16 screenX, sint16 screenY, sint16 *mapX, sint16 *mapY, uint8 *side)
{
    screen_get_map_xy(screenX, screenY, mapX, mapY, nullptr);
    if (*mapX == LOCATION_NULL)
        return;

    *side = map_get_tile_side(*mapX, *mapY);
    *mapX = floor2(*mapX, 32);
    *mapY = floor2(*mapY, 32);
}

/**
 *
 *  rct2: 0x006896DC
 */
void screen_get_map_xy_side_with_z(sint16 screenX, sint16 screenY, sint16 z, sint16 *mapX, sint16 *mapY, uint8 *side)
{
    screen_get_map_xy_with_z(screenX, screenY, z, mapX, mapY);
    if (*mapX == LOCATION_NULL)
        return;

    *side = map_get_tile_side(*mapX, *mapY);
    *mapX = floor2(*mapX, 32);
    *mapY = floor2(*mapY, 32);
}

/**
 * Get current viewport rotation.
 *
 * If an invalid rotation is detected and DEBUG_LEVEL_1 is enabled, an error
 * will be reported.
 *
 * @returns rotation in range 0-3 (inclusive)
 */
uint8 get_current_rotation()
{
    uint8 rotation = gCurrentRotation;
    uint8 rotation_masked = rotation & 3;
#if defined(DEBUG_LEVEL_1) && DEBUG_LEVEL_1
    if (rotation != rotation_masked) {
        log_error("Found wrong rotation %d! Will return %d instead.", (uint32)rotation, (uint32)rotation_masked);
    }
#endif // DEBUG_LEVEL_1
    return rotation_masked;
}

sint16 get_height_marker_offset()
{
    // Height labels in units
    if (gConfigGeneral.show_height_as_units)
        return 0;

    // Height labels in feet
    if (gConfigGeneral.measurement_format == MEASUREMENT_FORMAT_IMPERIAL)
        return 1 * 256;

    // Height labels in metres
    return 2 * 256;
}

void viewport_set_saved_view()
{
    rct_window * w = window_get_main();
    if (w != nullptr)
    {
        rct_viewport *viewport = w->viewport;

        gSavedViewX = viewport->view_width / 2 + viewport->view_x;
        gSavedViewY = viewport->view_height / 2 + viewport->view_y;

        gSavedViewZoom = viewport->zoom;
        gSavedViewRotation = get_current_rotation();
    }
}

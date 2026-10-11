// Copyright (c) 2026 Sirdankz
// SPDX-License-Identifier: MPL-2.0
// Original netplay/crossplay portions: see NETPLAY-LICENSE.md.
#ifndef MK64_NETPLAY_H
#define MK64_NETPLAY_H
#ifdef __cplusplus
extern "C" {
#endif
int x360_net_boot_menu(void);
/* MK64_R45_CONTROLS_PREMENU */
int x360_return_chord_pressed(void);
int x360_net_return_requested(void);
void x360_net_request_return(void);
void x360_net_clear_return_request(void);
void x360_net_full_restart(void);
void x360_controls_load(void);
int x360_controls_save(void);
const char *x360_control_action(int action);
const char *x360_control_binding(int player,int action);
void x360_control_bind(int player,int action,int source);
void x360_control_defaults(int player);
int x360_control_stick(int player,int change);
int x360_control_deadzone(int player,int change);
int x360_control_sensitivity(int player,int change);
unsigned int x360_controls_down(void);
/* Local presentation setting; never part of the synchronized game state. */
int x360_music_enabled(void);
int x360_music_set_enabled(int enabled);
int x360_display_widescreen(void);
float x360_display_aspect(void);
int x360_net8_active(void);
int x360_net_extended_lobby(void);
int x360_net_active(void);
int x360_net_crossplay(void);
int x360_net_player_count(void);
int x360_net_local_slot(void);
int x360_net_local_count(void);
int x360_net_is_local(int slot);
unsigned int x360_net_frame(void);
void x360_net_controllers(void *pads,int count);
void x360_net_set_menu_sync(int enabled);
int x360_net_menu_sync_active(void);
unsigned int x360_net_state_hash(void);
int x360_logging_enabled(void);
void x360_set_logging(int enabled);
/* MK64_ASTRA_TRACE_R17 */
void x360_net_trace(const char *fmt,...);
#ifdef __cplusplus
}
#endif
#endif

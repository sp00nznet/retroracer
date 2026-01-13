/*
 * RetroRacer - Main Game Logic Implementation
 * Game state management and main loop
 */

#include "game.h"
#include "physics.h"
#include "audio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DREAMCAST
#include <kos.h>
#endif

static game_t game;
static float delta_time = 1.0f / 60.0f;

/* Vehicle colors for AI */
static const uint32_t ai_colors[] = {
    PACK_COLOR(255, 255, 50, 50),    /* Red */
    PACK_COLOR(255, 50, 50, 255),    /* Blue */
    PACK_COLOR(255, 50, 255, 50),    /* Green */
    PACK_COLOR(255, 255, 255, 50),   /* Yellow */
    PACK_COLOR(255, 255, 128, 0),    /* Orange */
    PACK_COLOR(255, 128, 0, 255),    /* Purple */
    PACK_COLOR(255, 0, 255, 255),    /* Cyan */
};

void game_init(void) {
    memset(&game, 0, sizeof(game_t));

    /* Initialize subsystems */
    render_init();
    input_init();
    track_init();
    vehicle_init();
    ai_init();
    physics_init();
    audio_init();
    menu_init();

    /* Setup camera */
    game.camera.position = vec3_create(0, 10, -20);
    game.camera.target = vec3_create(0, 0, 0);
    game.camera.up = vec3_create(0, 1, 0);
    game.camera.fov = 60.0f;
    game.camera.aspect = 640.0f / 480.0f;
    game.camera.near_plane = 0.1f;
    game.camera.far_plane = 1000.0f;
    game.camera_distance = 8.0f;   /* Closer to vehicle */
    game.camera_height = 3.0f;     /* Lower camera to see more ground */

    game.state = GAME_STATE_MENU;
    game.num_laps = 3;
    game.best_time = 999999.0f;
}

void game_shutdown(void) {
    /* Clean up audio */
    audio_shutdown();

    /* Clean up track */
    if (game.track) {
        track_destroy(game.track);
        game.track = NULL;
    }

    /* Clean up vehicles and AI */
    for (int i = 0; i < game.vehicle_count; i++) {
        if (game.ai_controllers[i]) {
            ai_destroy(game.ai_controllers[i]);
            game.ai_controllers[i] = NULL;
        }
        if (game.vehicles[i]) {
            vehicle_destroy(game.vehicles[i]);
            game.vehicles[i] = NULL;
        }
    }
    game.vehicle_count = 0;
}

game_t *game_get_instance(void) {
    return &game;
}

static void spawn_vehicles(int num_ai, int include_player) {
    menu_state_t *menu = menu_get_state();

    /* Clean up existing vehicles */
    for (int i = 0; i < game.vehicle_count; i++) {
        if (game.ai_controllers[i]) ai_destroy(game.ai_controllers[i]);
        if (game.vehicles[i]) vehicle_destroy(game.vehicles[i]);
        game.ai_controllers[i] = NULL;
        game.vehicles[i] = NULL;
    }
    game.vehicle_count = 0;

    int vehicle_idx = 0;

    /* Spawn player vehicle */
    if (include_player) {
        vehicle_t *player = vehicle_create(
            (vehicle_class_t)menu->selected_vehicle,
            PACK_COLOR(255, 255, 200, 0),  /* Player is gold */
            1
        );
        player->total_laps = game.num_laps;

        vec3_t start_pos = game.track->start_position;
        start_pos.x += 2.0f;  /* Offset to grid position */
        vehicle_reset(player, start_pos, 0);

        game.vehicles[vehicle_idx] = player;
        game.ai_controllers[vehicle_idx] = NULL;
        game.player_vehicle_index = vehicle_idx;
        vehicle_idx++;
    }

    /* Spawn AI vehicles */
    for (int i = 0; i < num_ai && vehicle_idx < MAX_VEHICLES; i++) {
        vehicle_t *ai_vehicle = vehicle_create(
            (vehicle_class_t)(i % 4),  /* Vary vehicle types */
            ai_colors[i % 7],
            0
        );
        ai_vehicle->total_laps = game.num_laps;

        /* Grid position */
        vec3_t start_pos = game.track->start_position;
        start_pos.x += (i % 2 == 0) ? -2.0f : 2.0f;
        start_pos.z -= (i / 2 + 1) * 5.0f;
        vehicle_reset(ai_vehicle, start_pos, 0);

        game.vehicles[vehicle_idx] = ai_vehicle;
        game.ai_controllers[vehicle_idx] = ai_create(
            ai_vehicle,
            (ai_difficulty_t)menu->selected_difficulty
        );
        vehicle_idx++;
    }

    game.vehicle_count = vehicle_idx;
}

void game_start_race(game_mode_t mode, int num_laps, int num_opponents) {
    game.mode = mode;
    game.num_laps = num_laps;
    game.race_time = 0;
    game.countdown_timer = 3.0f;
    game.countdown_value = 3;

    /* Generate new track */
    if (game.track) {
        track_destroy(game.track);
    }
    game.track_params = track_default_params();
    game.track_params.difficulty = menu_get_state()->selected_difficulty + 1;
    game.track = track_generate(&game.track_params);

    /* Spawn vehicles based on mode */
    switch (mode) {
        case MODE_AI_RACE:
            spawn_vehicles(num_opponents + 1, 0);  /* AI only */
            game.player_vehicle_index = -1;
            break;

        case MODE_SINGLE_TRACK:
            spawn_vehicles(num_opponents, 1);  /* Player + AI */
            break;

        case MODE_TIME_TRIAL:
            spawn_vehicles(0, 1);  /* Player only */
            break;

        case MODE_GRAND_PRIX:
            spawn_vehicles(num_opponents, 1);  /* Player + AI */
            game.grand_prix.current_race = 0;
            game.grand_prix.total_races = 4;
            game.grand_prix.finished = 0;
            memset(game.grand_prix.points, 0, sizeof(game.grand_prix.points));
            break;
    }

    game.state = GAME_STATE_COUNTDOWN;
}

void game_end_race(void) {
    game.state = GAME_STATE_FINISHED;

    /* Calculate places */
    for (int i = 0; i < game.vehicle_count; i++) {
        int place = 1;
        for (int j = 0; j < game.vehicle_count; j++) {
            if (i != j) {
                /* Compare progress */
                float prog_i = game.vehicles[i]->current_lap + game.vehicles[i]->track_progress;
                float prog_j = game.vehicles[j]->current_lap + game.vehicles[j]->track_progress;
                if (prog_j > prog_i) place++;
            }
        }
        game.vehicles[i]->place = place;
    }

    /* Award Grand Prix points */
    if (game.mode == MODE_GRAND_PRIX) {
        int points_table[] = {10, 8, 6, 5, 4, 3, 2, 1};
        for (int i = 0; i < game.vehicle_count; i++) {
            int place = game.vehicles[i]->place - 1;
            if (place < 8) {
                game.grand_prix.points[i] += points_table[place];
            }
        }
    }

    /* Update best time for time trial */
    if (game.mode == MODE_TIME_TRIAL && game.player_vehicle_index >= 0) {
        float time = game.vehicles[game.player_vehicle_index]->total_time;
        if (time < game.best_time) {
            game.best_time = time;
        }
    }

    game.state = GAME_STATE_RESULTS;
}

void game_pause(void) {
    if (game.state == GAME_STATE_RACING) {
        game.state = GAME_STATE_PAUSED;
        menu_show_pause();
    }
}

void game_resume(void) {
    game.state = GAME_STATE_RACING;
}

void game_restart_race(void) {
    game_start_race(game.mode, game.num_laps, game.vehicle_count - 1);
}

void game_return_to_menu(void) {
    game_shutdown();
    game.state = GAME_STATE_MENU;
    menu_set_screen(MENU_MAIN);
}

void game_start_grand_prix(void) {
    game.grand_prix.current_race = 0;
    game.grand_prix.total_races = 4;
    game.grand_prix.finished = 0;
    memset(game.grand_prix.points, 0, sizeof(game.grand_prix.points));
    game_start_race(MODE_GRAND_PRIX, 3, 5);
}

void game_next_grand_prix_race(void) {
    game.grand_prix.current_race++;

    if (game.grand_prix.current_race >= game.grand_prix.total_races) {
        game.grand_prix.finished = 1;
        menu_show_standings(game.grand_prix.points, game.vehicle_count);
    } else {
        game_start_race(MODE_GRAND_PRIX, 3, game.vehicle_count - 1);
    }
}

void game_update_camera(float dt) {
    if (game.vehicle_count == 0) return;

    /* Determine which vehicle to follow */
    int follow_idx = game.player_vehicle_index;
    if (follow_idx < 0) {
        /* AI race mode - follow lead vehicle */
        follow_idx = 0;
        float best_progress = 0;
        for (int i = 0; i < game.vehicle_count; i++) {
            float prog = game.vehicles[i]->current_lap + game.vehicles[i]->track_progress;
            if (prog > best_progress) {
                best_progress = prog;
                follow_idx = i;
            }
        }
    }

    vehicle_t *target = game.vehicles[follow_idx];
    if (!target) return;

    /* Calculate desired camera position */
    vec3_t forward = vehicle_get_forward(target);
    vec3_t cam_offset = vec3_scale(forward, -game.camera_distance);
    cam_offset.y = game.camera_height;

    vec3_t desired_pos = vec3_add(target->position, cam_offset);

    /* Smooth camera movement */
    game.camera.position = vec3_lerp(game.camera.position, desired_pos, 5.0f * dt);

    /* Look at vehicle */
    vec3_t look_target = target->position;
    look_target.y += 1.0f;
    game.camera.target = vec3_lerp(game.camera.target, look_target, 8.0f * dt);

    /* Update camera matrices */
    camera_update(&game.camera);
}

static void update_countdown(float dt) {
    game.countdown_timer -= dt;

    if (game.countdown_timer <= 2.0f && game.countdown_value == 3) {
        game.countdown_value = 2;
    }
    if (game.countdown_timer <= 1.0f && game.countdown_value == 2) {
        game.countdown_value = 1;
    }
    if (game.countdown_timer <= 0) {
        game.countdown_value = 0;
        game.state = GAME_STATE_RACING;
    }
}

static void update_racing(float dt) {
    input_state_t *input = input_get_state(0);

    /* Update player vehicle */
    if (game.player_vehicle_index >= 0) {
        vehicle_t *player = game.vehicles[game.player_vehicle_index];

        float steering = input_get_steering(input);
        float throttle = input_get_throttle(input);
        float brake = input_get_brake(input);

        vehicle_set_steering(player, steering);
        vehicle_set_throttle(player, throttle);
        vehicle_set_brake(player, brake);
    }

    /* Update all vehicles */
    for (int i = 0; i < game.vehicle_count; i++) {
        /* Update AI */
        if (game.ai_controllers[i]) {
            ai_update(game.ai_controllers[i], game.track, game.vehicles, game.vehicle_count, dt);
        }

        /* Update vehicle physics */
        vehicle_update(game.vehicles[i], game.track, dt);
    }

    /* Check vehicle collisions */
    for (int i = 0; i < game.vehicle_count; i++) {
        for (int j = i + 1; j < game.vehicle_count; j++) {
            if (vehicle_check_collision(game.vehicles[i], game.vehicles[j])) {
                vehicle_resolve_collision(game.vehicles[i], game.vehicles[j]);
            }
        }
    }

    /* Update race time */
    game.race_time += dt;

    /* Check for race finish */
    int all_finished = 1;
    for (int i = 0; i < game.vehicle_count; i++) {
        if (!game.vehicles[i]->finished) {
            all_finished = 0;
            break;
        }
    }

    /* End race when player finishes (or all in AI mode) */
    if (game.player_vehicle_index >= 0) {
        if (game.vehicles[game.player_vehicle_index]->finished) {
            game_end_race();
        }
    } else if (all_finished) {
        game_end_race();
    }

    /* Pause game */
    if (input_button_pressed(input, BTN_START)) {
        game_pause();
    }
}

void game_update(float dt) {
    delta_time = dt;
    input_update();

    switch (game.state) {
        case GAME_STATE_MENU:
            menu_update(input_get_state(0), dt);
            if (!menu_is_active()) {
                menu_state_t *menu = menu_get_state();
                int num_ai = (menu->selected_mode == MODE_TIME_TRIAL) ? 0 : 5;
                game_start_race(menu->selected_mode, 3, num_ai);
            }
            break;

        case GAME_STATE_COUNTDOWN:
            update_countdown(dt);
            game_update_camera(dt);
            break;

        case GAME_STATE_RACING:
            update_racing(dt);
            game_update_camera(dt);
            break;

        case GAME_STATE_PAUSED:
            menu_update(input_get_state(0), dt);
            if (!menu_is_active()) {
                game_resume();
            }
            break;

        case GAME_STATE_RESULTS:
            menu_update(input_get_state(0), dt);
            if (!menu_is_active()) {
                if (game.mode == MODE_GRAND_PRIX && !game.grand_prix.finished) {
                    game_next_grand_prix_race();
                } else {
                    game_return_to_menu();
                }
            }
            break;

        default:
            break;
    }
}

static void render_hud(void) {
    char buf[64];

    /* Race time */
    int mins = (int)(game.race_time / 60);
    int secs = (int)game.race_time % 60;
    int msecs = (int)((game.race_time - (int)game.race_time) * 100);
    sprintf(buf, "Time: %02d:%02d.%02d", mins, secs, msecs);
    render_draw_text(20, 20, COLOR_WHITE, buf);

    /* Player info */
    if (game.player_vehicle_index >= 0) {
        vehicle_t *player = game.vehicles[game.player_vehicle_index];

        /* Speed */
        sprintf(buf, "Speed: %.0f km/h", player->speed * 3.6f);
        render_draw_text(20, 50, COLOR_WHITE, buf);

        /* Lap */
        sprintf(buf, "Lap: %d / %d", player->current_lap + 1, player->total_laps);
        render_draw_text(20, 80, COLOR_WHITE, buf);

        /* Position */
        sprintf(buf, "Position: %d / %d", player->place, game.vehicle_count);
        render_draw_text(20, 110, COLOR_WHITE, buf);

        /* Best lap */
        if (player->best_lap_time < 999999.0f) {
            mins = (int)(player->best_lap_time / 60);
            secs = (int)player->best_lap_time % 60;
            msecs = (int)((player->best_lap_time - (int)player->best_lap_time) * 100);
            sprintf(buf, "Best: %02d:%02d.%02d", mins, secs, msecs);
            render_draw_text(500, 20, COLOR_YELLOW, buf);
        }
    } else {
        /* AI race mode - show leader */
        render_draw_text(20, 50, COLOR_WHITE, "AI RACE MODE");
    }

    /* Mode indicator */
    sprintf(buf, "Mode: %s", menu_mode_name(game.mode));
    render_draw_text(500, 50, COLOR_GRAY, buf);

    /* Grand Prix info */
    if (game.mode == MODE_GRAND_PRIX) {
        sprintf(buf, "Race %d of %d", game.grand_prix.current_race + 1, game.grand_prix.total_races);
        render_draw_text(500, 80, COLOR_GRAY, buf);
    }

    /* DEBUG: Show camera and position info */
    sprintf(buf, "Cam: %.0f,%.0f,%.0f", game.camera.position.x, game.camera.position.y, game.camera.position.z);
    render_draw_text(20, 420, COLOR_CYAN, buf);
    sprintf(buf, "Tgt: %.0f,%.0f,%.0f", game.camera.target.x, game.camera.target.y, game.camera.target.z);
    render_draw_text(20, 440, COLOR_CYAN, buf);
    if (game.vehicle_count > 0 && game.vehicles[0]) {
        sprintf(buf, "Car0: %.0f,%.0f,%.0f", game.vehicles[0]->position.x, game.vehicles[0]->position.y, game.vehicles[0]->position.z);
        render_draw_text(20, 460, COLOR_CYAN, buf);
    }
}

static void render_countdown(void) {
    char buf[16];

    /* Always show "START" during countdown */
    if (game.countdown_value > 0) {
        render_draw_text(270, 160, COLOR_WHITE, "START");
        sprintf(buf, "%d", game.countdown_value);
        render_draw_text(310, 220, COLOR_YELLOW, buf);
    } else {
        render_draw_text(295, 200, COLOR_GREEN, "GO!");
    }
}

void game_render(void) {
    switch (game.state) {
        case GAME_STATE_MENU:
            /* Menu renders directly to VRAM, no PVR */
            menu_render();
            break;

        case GAME_STATE_PAUSED:
        case GAME_STATE_RESULTS:
            /* Render game in background, then menu overlay */
            render_begin_frame();
            render_clear(PACK_COLOR(255, 100, 150, 200));
            if (game.track) {
                track_render(game.track, &game.camera);
            }
            for (int i = 0; i < game.vehicle_count; i++) {
                vehicle_render(game.vehicles[i], &game.camera);
            }
            render_end_frame();
            /* Now draw menu on top */
            menu_render();
            break;

        case GAME_STATE_COUNTDOWN:
        case GAME_STATE_RACING:
        case GAME_STATE_FINISHED:
            /* Render 3D scene */
            render_begin_frame();
            render_clear(PACK_COLOR(255, 100, 150, 200));  /* Sky color */

            if (game.track) {
                track_render(game.track, &game.camera);
            }

            for (int i = 0; i < game.vehicle_count; i++) {
                vehicle_render(game.vehicles[i], &game.camera);
            }

            render_end_frame();

            /* Render HUD after PVR completes */
            render_hud();

            /* Countdown overlay */
            if (game.state == GAME_STATE_COUNTDOWN) {
                render_countdown();
            }
            break;

        default:
            break;
    }
}

float game_get_delta_time(void) {
    return delta_time;
}

int game_is_racing(void) {
    return game.state == GAME_STATE_RACING;
}

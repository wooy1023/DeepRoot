/*
 * =============================================================================
 * Project : DeepRoot
 * File    : main.c
 * Desc    : °ÔÀÓ ÀüÃ¼ Èå¸§ ¿¬°á ¹× ¸ÞÀÎ ·çÇÁ
 *           - ´øÀü ÃÊ±âÈ­ (±×·¡ÇÁ)
 *           - ÇÃ·¹ÀÌ¾î »ý¼º
 *           - ¸ó½ºÅÍ ¸Å´ÏÀú »ý¼º (Å¥ + ¿ì¼±¼øÀ§ Å¥)
 *           - ¹«±â Æ®¸® ÃÊ±âÈ­
 *           - JSON »óÅÂ Ãâ·Â (export.c)
 *           - °ÔÀÓ ·çÇÁ (ÀÔ·Â ¡æ ¾÷µ¥ÀÌÆ® ¡æ Ãâ·Â)
 * =============================================================================
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>   /* Sleep(), GetAsyncKeyState() */
#include <conio.h>     /* _kbhit(), _getch() */
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#endif

#include "dungeon.h"
#include "player.h"
#include "monster.h"
#include "weapon.h"
#include "export.h"

 /* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ »ó¼ö ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

#define MAP_WIDTH        800    /* °ÔÀÓ ¸Ê °¡·Î (ÇÈ¼¿ ´ÜÀ§)                 */
#define MAP_HEIGHT       600    /* °ÔÀÓ ¸Ê ¼¼·Î (ÇÈ¼¿ ´ÜÀ§)                 */
#define DELTA_MS         16     /* °ÔÀÓ ·çÇÁ °£°Ý (~60fps)                  */
#define SPAWN_INTERVAL   60     /* ¸ó½ºÅÍ ½ºÆù °£°Ý (ÇÁ·¹ÀÓ)                */
#define MAX_SCREEN_MON   10     /* È­¸é µ¿½Ã ÃÖ´ë ¸ó½ºÅÍ ¼ö (4 + level*2)  */
#define JSON_PATH        "data/state.json"  /* JSON Ãâ·Â °æ·Î               */

/* ·¹º§¾÷ Ã³Ä¡ ¼ö ±âÁØ */
#define KILL_LV2         10
#define KILL_LV3         30
#define KILL_BOSS        60

/* º¸½º ½ºÅÈ */
#define BOSS_HP          1000
#define BOSS_ATK         20
#define BOSS_SPEED       1

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ °ÔÀÓ ÀüÃ¼ »óÅÂ ±¸Á¶Ã¼ ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

typedef enum GamePhase {
    PHASE_NORMAL = 0,   /* MAP 1: ÀÏ¹Ý ¸ó½ºÅÍ ÀüÅõ  */
    PHASE_BOSS = 1,   /* MAP 2: º¸½º ÀüÅõ         */
    PHASE_CLEAR = 2,   /* °ÔÀÓ Å¬¸®¾î              */
    PHASE_OVER = 3    /* °ÔÀÓ ¿À¹ö                */
} GamePhase;

typedef struct BossState {
    int     hp;
    int     max_hp;
    int     atk;
    int     is_active;
    char    dash_state[16];   /* "idle" / "warning" / "dashing" / "cooldown" */
    int     missile_count;
    Vec2f   position;
    int     monster_id;       /* MonsterManager ³» º¸½º ID */
} BossState;

typedef struct GameState {
    Dungeon* dungeon;
    Player* player;
    MonsterManager* mgr;
    WeaponTree* weapon_tree;
    WeaponNode* current_weapon;  /* ÇöÀç ÀåÂø ¹«±â ³ëµå */

    GamePhase        phase;
    int              kills;
    int              wave;
    int              frame;
    int              spawn_timer;
    int              level;

    BossState        boss;
    char             message[64];
} GameState;

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  ³»ºÎ À¯Æ¿                                                                  */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

/* ¸Ê °¡ÀåÀÚ¸® 4¹æÇâ Áß ·£´ý ½ºÆù À§Ä¡ »ý¼º */
static Vec2f random_spawn_pos(void)
{
    Vec2f pos;
    int side = rand() % 4;
    switch (side) {
    case 0: pos.x = (float)(rand() % MAP_WIDTH);  pos.y = -30.0f;            break;
    case 1: pos.x = (float)(rand() % MAP_WIDTH);  pos.y = (float)(MAP_HEIGHT + 30); break;
    case 2: pos.x = -30.0f;                        pos.y = (float)(rand() % MAP_HEIGHT); break;
    default: pos.x = (float)(MAP_WIDTH + 30);     pos.y = (float)(rand() % MAP_HEIGHT); break;
    }
    return pos;
}

/* ÇöÀç ·¹º§ ±âÁØ ¸ó½ºÅÍ HP/ATK °è»ê */
static int calc_mon_hp(int level) { return 25 + level * 12; }
static int calc_mon_atk(int level) { return 5 + level * 2; }

/* È­¸é µ¿½Ã ÃÖ´ë ¸ó½ºÅÍ ¼ö */
static int max_monsters(int level) { return 4 + level * 2; }

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  JSON Ãâ·Â (UI ¿¬µ¿ ÇÙ½É)                                                   */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

/*
 * export_json
 * ÇöÀç GameState¸¦ UI¿Í È®Á¤µÈ JSON Çü½ÄÀ¸·Î data/state.json ¿¡ Ãâ·ÂÇÑ´Ù.
 * UIÀÇ reloadState() fetch ÄÚµå°¡ ÀÌ ÆÄÀÏÀ» ÀÐ´Â´Ù.
 */
static void export_json(const GameState* gs)
{
    FILE* fp;
    int   i;
    int   first;

    if (!gs) return;

    fp = fopen(JSON_PATH, "w");
    if (!fp) {
        fprintf(stderr, "[export_json] ÆÄÀÏ ¿­±â ½ÇÆÐ: %s\n", JSON_PATH);
        return;
    }

    /* ¦¡¦¡ player ¦¡¦¡ */
    {
        const Player* p = gs->player;
        const WeaponNode* w = gs->current_weapon;

        /* ¹æÇâ ¹®ÀÚ¿­ º¯È¯ */
        const char* dir_str = "right";
        switch (p->last_face_dir) {
        case FACE_UP:         dir_str = "up";         break;
        case FACE_DOWN:       dir_str = "down";       break;
        case FACE_LEFT:       dir_str = "left";       break;
        case FACE_RIGHT:      dir_str = "right";      break;
        case FACE_UP_LEFT:    dir_str = "up_left";    break;
        case FACE_UP_RIGHT:   dir_str = "up_right";   break;
        case FACE_DOWN_LEFT:  dir_str = "down_left";  break;
        case FACE_DOWN_RIGHT: dir_str = "down_right"; break;
        default:              dir_str = "right";      break;
        }

        fprintf(fp, "{\n");
        fprintf(fp, "  \"player\": {\n");
        fprintf(fp, "    \"hp\": %d,\n", p->hp);
        fprintf(fp, "    \"max_hp\": %d,\n", p->max_hp);
        fprintf(fp, "    \"attack\": %d,\n", w ? w->stat.damage : 15);
        fprintf(fp, "    \"level\": %d,\n", gs->level);
        fprintf(fp, "    \"x\": %.0f,\n", p->position.x);
        fprintf(fp, "    \"y\": %.0f,\n", p->position.y);
        fprintf(fp, "    \"direction\": \"%s\",\n", dir_str);
        fprintf(fp, "    \"is_shooting\": %s\n",
            (p->state == PSTATE_MOVE) ? "true" : "false");
        fprintf(fp, "  },\n");
    }

    /* ¦¡¦¡ weapon ¦¡¦¡ */
    {
        const WeaponNode* w = gs->current_weapon;
        const char* key_str = "root";
        const char* name_str = "±âº» È°";
        int tier = 1, damage = 15, pierce = 0, multi = 1;
        float fire_rate = 1.2f;

        if (w) {
            switch (w->weapon_id) {
            case WEAPON_BOW_BASIC:  key_str = "root";   name_str = "±âº» È°"; tier = 1; break;
            case WEAPON_SHORT_BOW:  key_str = "´Ü±Ã";   name_str = "´Ü±Ã";    tier = 2; break;
            case WEAPON_CROSSBOW:   key_str = "¼®±Ã";   name_str = "¼®±Ã";    tier = 2; break;
            case WEAPON_FIRE_ARROW: key_str = "ºÒÈ­»ì"; name_str = "ºÒÈ­»ì";  tier = 3; break;
            case WEAPON_RAPID_BOW:  key_str = "¿¬¹ß";   name_str = "¿¬¹ß";    tier = 3; break;
            default: break;
            }
            damage = w->stat.damage;
            fire_rate = (float)w->stat.fire_rate_ms / 1000.0f;
            pierce = (w->stat.effect == PROJ_PIERCE || w->stat.effect == PROJ_FIRE) ? 1 : 0;
            multi = w->stat.proj_count;
        }

        fprintf(fp, "  \"weapon\": {\n");
        fprintf(fp, "    \"key\": \"%s\",\n", key_str);
        fprintf(fp, "    \"name\": \"%s\",\n", name_str);
        fprintf(fp, "    \"tier\": %d,\n", tier);
        fprintf(fp, "    \"damage\": %d,\n", damage);
        fprintf(fp, "    \"fire_rate\": %.2f,\n", fire_rate);
        fprintf(fp, "    \"pierce\": %d,\n", pierce);
        fprintf(fp, "    \"multi_shot\": %d,\n", multi);

        /* upgrades_available: ÇöÀç ¹«±âÀÇ ÀÚ½Ä ³ëµå ÀÌ¸§ */
        fprintf(fp, "    \"upgrades_available\": [");
        if (w && w->left) {
            const char* ln = "?";
            switch (w->left->weapon_id) {
            case WEAPON_SHORT_BOW:  ln = "´Ü±Ã";   break;
            case WEAPON_FIRE_ARROW: ln = "ºÒÈ­»ì"; break;
            default: break;
            }
            fprintf(fp, "\"%s\"", ln);
            if (w->right) fprintf(fp, ", ");
        }
        if (w && w->right) {
            const char* rn = "?";
            switch (w->right->weapon_id) {
            case WEAPON_CROSSBOW:  rn = "¼®±Ã"; break;
            case WEAPON_RAPID_BOW: rn = "¿¬¹ß"; break;
            default: break;
            }
            fprintf(fp, "\"%s\"", rn);
        }
        fprintf(fp, "]\n");
        fprintf(fp, "  },\n");
    }

    /* ¦¡¦¡ monsters ¦¡¦¡ */
    fprintf(fp, "  \"monsters\": [");
    first = 1;
    for (i = 0; i < MONSTER_QUEUE_MAX; i++) {
        const Monster* m = &gs->mgr->pool[i];
        if (!m->is_active || m->state == MSTATE_DEAD) continue;
        if (m->type == MON_BOSS) continue;  /* º¸½º´Â º°µµ Ã³¸® */

        if (!first) fprintf(fp, ",");
        fprintf(fp, "\n    { \"id\": %d, \"name\": \"%s\","
            " \"hp\": %d, \"max_hp\": %d,"
            " \"x\": %.0f, \"y\": %.0f, \"atk\": %d }",
            m->id, m->name, m->hp, m->max_hp,
            m->position.x, m->position.y, m->attack_power);
        first = 0;
    }
    fprintf(fp, "\n  ],\n");

    /* ¦¡¦¡ projectiles ¦¡¦¡ */
    fprintf(fp, "  \"projectiles\": [");
    first = 1;
    for (i = 0; i < PLAYER_MAX_PROJ; i++) {
        const Projectile* pr = &gs->player->proj_pool[i];
        if (!pr->is_active) continue;

        float len = sqrtf(pr->velocity.x * pr->velocity.x +
            pr->velocity.y * pr->velocity.y);
        float dx = (len > 0.0f) ? pr->velocity.x / len : 0.0f;
        float dy = (len > 0.0f) ? pr->velocity.y / len : 0.0f;

        if (!first) fprintf(fp, ",");
        fprintf(fp, "\n    { \"id\": %d, \"x\": %.0f, \"y\": %.0f,"
            " \"dx\": %.2f, \"dy\": %.2f }",
            i, pr->position.x, pr->position.y, dx, dy);
        first = 0;
    }
    fprintf(fp, "\n  ],\n");

    /* ¦¡¦¡ rooms ¦¡¦¡ */
    fprintf(fp, "  \"rooms\": [\n");
    fprintf(fp, "    { \"id\": 1, \"visited\": %s, \"is_boss_room\": false },\n",
        (gs->phase >= PHASE_BOSS) ? "true" : "true");
    fprintf(fp, "    { \"id\": 2, \"visited\": %s, \"is_boss_room\": true }\n",
        (gs->phase == PHASE_BOSS) ? "true" : "false");
    fprintf(fp, "  ],\n");

    /* ¦¡¦¡ boss ¦¡¦¡ */
    fprintf(fp, "  \"boss\": {\n");
    fprintf(fp, "    \"hp\": %d,\n", gs->boss.hp);
    fprintf(fp, "    \"max_hp\": %d,\n", gs->boss.max_hp);
    fprintf(fp, "    \"atk\": %d,\n", gs->boss.atk);
    fprintf(fp, "    \"is_active\": %s,\n", gs->boss.is_active ? "true" : "false");
    fprintf(fp, "    \"dash_state\": \"%s\",\n", gs->boss.dash_state);
    fprintf(fp, "    \"missile_count\": %d\n", gs->boss.missile_count);
    fprintf(fp, "  },\n");

    /* ¦¡¦¡ kills / wave / message ¦¡¦¡ */
    fprintf(fp, "  \"kills\": %d,\n", gs->kills);
    fprintf(fp, "  \"wave\": %d,\n", gs->wave);
    fprintf(fp, "  \"message\": \"%s\"\n", gs->message);
    fprintf(fp, "}\n");

    fclose(fp);
}

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  Å° ÀÔ·Â Ã³¸® (Windows ±âÁØ)                                                */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

static FaceDir get_input(void)
{
    FaceDir dir = FACE_NONE;

#ifdef _WIN32
    if (GetAsyncKeyState('W') & 0x8000) dir = (FaceDir)(dir | FACE_UP);
    if (GetAsyncKeyState('S') & 0x8000) dir = (FaceDir)(dir | FACE_DOWN);
    if (GetAsyncKeyState('A') & 0x8000) dir = (FaceDir)(dir | FACE_LEFT);
    if (GetAsyncKeyState('D') & 0x8000) dir = (FaceDir)(dir | FACE_RIGHT);
    if (GetAsyncKeyState(VK_UP) & 0x8000) dir = (FaceDir)(dir | FACE_UP);
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) dir = (FaceDir)(dir | FACE_DOWN);
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) dir = (FaceDir)(dir | FACE_LEFT);
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) dir = (FaceDir)(dir | FACE_RIGHT);
#endif

    return dir;
}

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  ·¹º§¾÷ / ¹«±â ¼±ÅÃ                                                         */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

/*
 * ÄÜ¼Ö¿¡¼­ ¾÷±×·¹ÀÌµå ¼±ÅÃ ÆË¾÷ (1 ¶Ç´Â 2 ÀÔ·Â)
 * ¼±ÅÃµÈ WeaponNode Æ÷ÀÎÅÍ ¹ÝÈ¯
 */
static WeaponNode* weapon_upgrade_prompt(WeaponNode* cur)
{
    WeaponNode* left = cur ? cur->left : NULL;
    WeaponNode* right = cur ? cur->right : NULL;
    int choice;

    if (!left && !right) return cur;  /* ´õ ÀÌ»ó ¾÷±×·¹ÀÌµå ¾øÀ½ */

    printf("\n=== ¹«±â ¾÷±×·¹ÀÌµå ===\n");
    if (left)  printf("  1. %s  (DMG:%d / RATE:%dms)\n",
        left->name, left->stat.damage, left->stat.fire_rate_ms);
    if (right) printf("  2. %s  (DMG:%d / RATE:%dms)\n",
        right->name, right->stat.damage, right->stat.fire_rate_ms);
    printf("¼±ÅÃ (1/2): ");

    choice = 0;
    scanf("%d", &choice);

    if (choice == 1 && left) {
        weapon_unlock(left);
        printf(">> %s ÀåÂø!\n", left->name);
        return left;
    }
    else if (choice == 2 && right) {
        weapon_unlock(right);
        printf(">> %s ÀåÂø!\n", right->name);
        return right;
    }

    /* Àß¸øµÈ ÀÔ·Â ½Ã ¿ÞÂÊ ±âº» ¼±ÅÃ */
    if (left) { weapon_unlock(left); return left; }
    return right;
}

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  º¸½º ½ºÆù                                                                  */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

static void spawn_boss(GameState* gs)
{
    Vec2f boss_pos;
    int   boss_id;

    boss_pos.x = (float)(MAP_WIDTH / 2);
    boss_pos.y = (float)(MAP_HEIGHT / 2);

    boss_id = monster_spawn(gs->mgr, MON_BOSS, boss_pos, BOSS_HP, BOSS_ATK);

    gs->boss.hp = BOSS_HP;
    gs->boss.max_hp = BOSS_HP;
    gs->boss.atk = BOSS_ATK;
    gs->boss.is_active = 1;
    gs->boss.missile_count = 0;
    gs->boss.position = boss_pos;
    gs->boss.monster_id = boss_id;
    strncpy(gs->boss.dash_state, "idle", sizeof(gs->boss.dash_state) - 1);

    snprintf(gs->message, sizeof(gs->message), "º¸½º µîÀå! ÃÖÈÄÀÇ ÀüÅõ!");
    printf("\n[BOSS] º¸½º µîÀå! HP: %d\n", BOSS_HP);
}

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  °ÔÀÓ ÃÊ±âÈ­                                                                */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

static GameState* game_init(void)
{
    GameState* gs;
    Vec2f      start_pos;
    int        room0, room1;

    srand((unsigned int)time(NULL));

    gs = (GameState*)calloc(1, sizeof(GameState));
    if (!gs) return NULL;

    /* ¦¡¦¡ ´øÀü ÃÊ±âÈ­ (±×·¡ÇÁ) ¦¡¦¡ */
    gs->dungeon = dungeon_create();
    if (!gs->dungeon) goto fail;

    room0 = dungeon_add_room(gs->dungeon, ROOM_NORMAL, 25, 18, "ÀÏ¹Ý ¹æ");
    room1 = dungeon_add_room(gs->dungeon, ROOM_BOSS, 20, 20, "º¸½º ¹æ");

    /* ¹æ 0 ¡æ ¹æ 1 (ºÏÂÊ Åë·Î, º¸½º Àá±Ý) */
    dungeon_add_passage(gs->dungeon, room0, room1, DIR_NORTH, LOCK_BOSS);
    dungeon_add_passage(gs->dungeon, room1, room0, DIR_SOUTH, LOCK_OPEN);

    /* ¦¡¦¡ ÇÃ·¹ÀÌ¾î ÃÊ±âÈ­ ¦¡¦¡ */
    start_pos.x = (float)(MAP_WIDTH / 2);
    start_pos.y = (float)(MAP_HEIGHT / 2);

    gs->player = player_create("Player", start_pos, room0);
    if (!gs->player) goto fail;

    /* ¦¡¦¡ ¸ó½ºÅÍ ¸Å´ÏÀú ÃÊ±âÈ­ (Å¥ + ¿ì¼±¼øÀ§ Å¥) ¦¡¦¡ */
    gs->mgr = monster_manager_create(room0);
    if (!gs->mgr) goto fail;

    /* ¦¡¦¡ ¹«±â Æ®¸® ÃÊ±âÈ­ (ÀÌÁø Æ®¸®) ¦¡¦¡ */
    gs->weapon_tree = weapon_tree_create();
    if (!gs->weapon_tree) goto fail;

    gs->current_weapon = weapon_tree_get_root(gs->weapon_tree);
    player_equip_weapon(gs->player, gs->current_weapon);

    /* ¦¡¦¡ °ÔÀÓ »óÅÂ ÃÊ±âÈ­ ¦¡¦¡ */
    gs->phase = PHASE_NORMAL;
    gs->kills = 0;
    gs->wave = 1;
    gs->frame = 0;
    gs->spawn_timer = 0;
    gs->level = 1;

    gs->boss.hp = BOSS_HP;
    gs->boss.max_hp = BOSS_HP;
    gs->boss.atk = BOSS_ATK;
    gs->boss.is_active = 0;
    gs->boss.missile_count = 0;
    strncpy(gs->boss.dash_state, "idle", sizeof(gs->boss.dash_state) - 1);

    snprintf(gs->message, sizeof(gs->message), "°ÔÀÓ ½ÃÀÛ!");

    printf("=== DeepRoot ===\n");
    printf("WASD ÀÌµ¿ / ÀÌµ¿ ¹æÇâÀ¸·Î ÀÚµ¿ ¹ß»ç\n");
    printf("10¸¶¸® Ã³Ä¡: Lv.2 / 30¸¶¸®: Lv.3 / 60¸¶¸®: º¸½º ÀÔÀå\n\n");

    return gs;

fail:
    /* ÃÊ±âÈ­ ½ÇÆÐ ½Ã ÇÒ´çµÈ ¸Þ¸ð¸® ¸ðµÎ ÇØÁ¦ */
    if (gs->dungeon)      dungeon_destroy(gs->dungeon);
    if (gs->player)       player_destroy(gs->player);
    if (gs->mgr)          monster_manager_destroy(gs->mgr);
    if (gs->weapon_tree)  weapon_tree_destroy(gs->weapon_tree);
    free(gs);
    return NULL;
}

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  °ÔÀÓ ÇØÁ¦                                                                  */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

static void game_destroy(GameState* gs)
{
    if (!gs) return;
    if (gs->dungeon)     dungeon_destroy(gs->dungeon);
    if (gs->player)      player_destroy(gs->player);
    if (gs->mgr)         monster_manager_destroy(gs->mgr);
    if (gs->weapon_tree) weapon_tree_destroy(gs->weapon_tree);
    free(gs);
}

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  °ÔÀÓ ·çÇÁ ´Ü°è                                                             */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

/* Åõ»çÃ¼ ¡ê ¸ó½ºÅÍ Ãæµ¹ ÆÇÁ¤ */
static void check_proj_collision(GameState* gs)
{
    int i, j;
    for (i = 0; i < PLAYER_MAX_PROJ; i++) {
        Projectile* pr = player_get_active_proj(gs->player, i);
        if (!pr) continue;

        for (j = 0; j < MONSTER_QUEUE_MAX; j++) {
            Monster* m = &gs->mgr->pool[j];
            if (!m->is_active || m->state == MSTATE_DEAD) continue;

            float dx = pr->position.x - m->position.x;
            float dy = pr->position.y - m->position.y;
            float dist = sqrtf(dx * dx + dy * dy);
            float hitR = (m->type == MON_BOSS) ? 30.0f : 20.0f;

            if (dist < hitR) {
                int result = monster_take_damage(gs->mgr, m->id, pr->damage);

                /* º¸½º HP µ¿±âÈ­ */
                if (m->type == MON_BOSS) {
                    gs->boss.hp = m->hp;
                    if (gs->boss.hp < 0) gs->boss.hp = 0;
                }

                if (result == 0) {
                    /* ¸ó½ºÅÍ »ç¸Á */
                    if (m->type == MON_BOSS) {
                        gs->phase = PHASE_CLEAR;
                        snprintf(gs->message, sizeof(gs->message), "º¸½º Ã³Ä¡! °ÔÀÓ Å¬¸®¾î!");
                        printf("\n[CLEAR] º¸½º Ã³Ä¡! °ÔÀÓ Å¬¸®¾î!\n");
                    }
                    else {
                        gs->kills++;
                        gs->wave++;
                        snprintf(gs->message, sizeof(gs->message),
                            "%s Ã³Ä¡! (%d¸¶¸®)", m->name, gs->kills);
                        printf("[Ã³Ä¡] %s | ÃÑ %d¸¶¸®\n", m->name, gs->kills);
                    }
                }

                /* °üÅë ¾Æ´Ñ Åõ»çÃ¼´Â ºñÈ°¼ºÈ­ */
                if (gs->current_weapon &&
                    gs->current_weapon->stat.effect != PROJ_PIERCE &&
                    gs->current_weapon->stat.effect != PROJ_FIRE) {
                    pr->is_active = 0;
                    break;
                }
            }
        }
    }
}

/* ¸ó½ºÅÍ ¡ê ÇÃ·¹ÀÌ¾î ±ÙÁ¢ µ¥¹ÌÁö */
static void check_melee_collision(GameState* gs)
{
    int j;
    static int melee_timer = 0;
    melee_timer++;
    if (melee_timer < 60) return;  /* 1ÃÊ(60ÇÁ·¹ÀÓ)¸¶´Ù ÇÑ ¹ø */
    melee_timer = 0;

    for (j = 0; j < MONSTER_QUEUE_MAX; j++) {
        Monster* m = &gs->mgr->pool[j];
        if (!m->is_active || m->state == MSTATE_DEAD) continue;
        if (m->state != MSTATE_ATTACK) continue;

        float dx = gs->player->position.x - m->position.x;
        float dy = gs->player->position.y - m->position.y;
        float dist = sqrtf(dx * dx + dy * dy);
        float hitR = (m->type == MON_BOSS) ? 38.0f : 24.0f;

        if (dist < hitR) {
            int dmg = (m->type == MON_BOSS &&
                strcmp(gs->boss.dash_state, "dashing") == 0)
                ? m->attack_power * 2 : m->attack_power;

            player_take_damage(gs->player, dmg);
            snprintf(gs->message, sizeof(gs->message),
                "[ÇÇ°Ý] %sÀÇ °ø°Ý -%d HP", m->name, dmg);

            if (!player_is_alive(gs->player)) {
                gs->phase = PHASE_OVER;
                snprintf(gs->message, sizeof(gs->message), "°ÔÀÓ ¿À¹ö");
                printf("\n[OVER] °ÔÀÓ ¿À¹ö. Ã³Ä¡: %d¸¶¸®\n", gs->kills);
                return;
            }
        }
    }
}

/* ·¹º§¾÷ / º¸½º ÁøÀÔ Ã¼Å© */
static void check_levelup(GameState* gs)
{
    if (gs->phase != PHASE_NORMAL) return;

    if (gs->level < 2 && gs->kills >= KILL_LV2) {
        gs->level = 2;
        printf("\n[LEVEL UP] Lv.2 ´Þ¼º! ¹«±â ¾÷±×·¹ÀÌµå\n");
        gs->current_weapon = weapon_upgrade_prompt(gs->current_weapon);
        player_equip_weapon(gs->player, gs->current_weapon);
        snprintf(gs->message, sizeof(gs->message), "Lv.2! %s ÀåÂø", gs->current_weapon->name);
    }

    if (gs->level < 3 && gs->kills >= KILL_LV3) {
        gs->level = 3;
        printf("\n[LEVEL UP] Lv.3 ´Þ¼º! ¹«±â ¾÷±×·¹ÀÌµå\n");
        gs->current_weapon = weapon_upgrade_prompt(gs->current_weapon);
        player_equip_weapon(gs->player, gs->current_weapon);
        snprintf(gs->message, sizeof(gs->message), "Lv.3! %s ÀåÂø", gs->current_weapon->name);
    }

    if (gs->level >= 3 && gs->kills >= KILL_BOSS && gs->phase == PHASE_NORMAL) {
        printf("\n[BOSS] 60¸¶¸® Ã³Ä¡! º¸½º¹æ ÀÔÀå\n");
        gs->phase = PHASE_BOSS;
        gs->mgr->room_id = gs->dungeon->boss_room_id;
        dungeon_unlock_passage(gs->dungeon, 0, DIR_NORTH);
        dungeon_set_cleared(gs->dungeon, 0);

        /* ÃÖÁ¾ ¹«±â ¾÷±×·¹ÀÌµå (tier3 ¹Ì¼±ÅÃ ½Ã) */
        if (weapon_can_upgrade(gs->current_weapon)) {
            printf("[º¸½º ÁøÀÔ] ÃÖÁ¾ ¹«±â ¾÷±×·¹ÀÌµå\n");
            gs->current_weapon = weapon_upgrade_prompt(gs->current_weapon);
            player_equip_weapon(gs->player, gs->current_weapon);
        }

        spawn_boss(gs);
    }
}

/* ¸ó½ºÅÍ ½ºÆù */
static void try_spawn(GameState* gs)
{
    MonsterType types[5] = { MON_GOBLIN, MON_GOBLIN, MON_SKELETON,
                              MON_GOBLIN, MON_SKELETON };
    MonsterType type;
    Vec2f       pos;
    int         hp, atk, id;

    if (gs->phase != PHASE_NORMAL) return;
    if (gs->mgr->alive_count >= max_monsters(gs->level)) return;

    gs->spawn_timer++;
    if (gs->spawn_timer < SPAWN_INTERVAL) return;
    gs->spawn_timer = 0;

    type = types[rand() % 5];
    pos = random_spawn_pos();
    hp = calc_mon_hp(gs->level);
    atk = calc_mon_atk(gs->level);

    id = monster_spawn(gs->mgr, type, pos, hp, atk);
    if (id != MONSTER_INVALID_ID) {
        printf("[½ºÆù] %s (id=%d) HP:%d\n",
            gs->mgr->pool[id].name, id, hp);
    }
}

/* ÀÚµ¿ ¹ß»ç */
static void try_shoot(GameState* gs)
{
    static int shoot_timer = 0;
    int fire_rate_frames;

    if (!gs->current_weapon) return;

    fire_rate_frames = gs->current_weapon->stat.fire_rate_ms / DELTA_MS;
    if (fire_rate_frames < 1) fire_rate_frames = 1;

    shoot_timer++;
    if (shoot_timer >= fire_rate_frames) {
        shoot_timer = 0;
        player_shoot(gs->player);

        /* ¿¬¹ß(multi_shot): Ãß°¡ Åõ»çÃ¼ ¹ß»ç */
        if (gs->current_weapon->stat.proj_count > 1) {
            int extra = gs->current_weapon->stat.proj_count - 1;
            int k;
            for (k = 0; k < extra; k++) {
                player_shoot(gs->player);
            }
        }
    }
}

/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */
/*  ¸ÞÀÎ ÇÔ¼ö                                                                  */
/* ¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡¦¡ */

int main(void)
{
    GameState* gs;
    FaceDir    dir;

    gs = game_init();
    if (!gs) {
        fprintf(stderr, "[main] °ÔÀÓ ÃÊ±âÈ­ ½ÇÆÐ\n");
        return 1;
    }

    /* µð¹ö±×: ÀÚ·á±¸Á¶ ÃÊ±â »óÅÂ Ãâ·Â */
    dungeon_print_info(gs->dungeon);
    dungeon_print_adj_list(gs->dungeon);
    weapon_tree_print(gs->weapon_tree);

    /* ¦¡¦¡ °ÔÀÓ ·çÇÁ ¦¡¦¡ */
    while (gs->phase != PHASE_CLEAR && gs->phase != PHASE_OVER) {
        /* 1. ÀÔ·Â Ã³¸® */
        dir = get_input();

        /* 2. ÇÃ·¹ÀÌ¾î ÀÌµ¿ */
        player_move(gs->player, dir, MAP_WIDTH, MAP_HEIGHT);

        /* 3. ¹«Àû Å¸ÀÌ¸Ó °»½Å */
        player_update_invincible(gs->player, DELTA_MS);

        /* 4. ÀÚµ¿ ¹ß»ç */
        try_shoot(gs);

        /* 5. Åõ»çÃ¼ ÀÌµ¿ */
        player_update_projectiles(gs->player);

        /* 6. ¸ó½ºÅÍ ½ºÆù */
        try_spawn(gs);

        /* 7. ¸ó½ºÅÍ AI ¾÷µ¥ÀÌÆ® */
        monster_manager_update(gs->mgr, &gs->player->position, DELTA_MS);

        /* 8. Ãæµ¹ ÆÇÁ¤ */
        check_proj_collision(gs);
        check_melee_collision(gs);

        /* 9. ·¹º§¾÷ / º¸½º ÁøÀÔ Ã¼Å© */
        check_levelup(gs);

        /* 10. JSON Ãâ·Â (UI ¿¬µ¿) */
        export_json(gs);

        /* 11. ÇÁ·¹ÀÓ Ä«¿îÅÍ */
        gs->frame++;

        /* 12. ÇÁ·¹ÀÓ µô·¹ÀÌ (~60fps) */
#ifdef _WIN32
        Sleep(DELTA_MS);
#else
        usleep(DELTA_MS * 1000);
#endif
    }

    /* ¦¡¦¡ °á°ú JSON ÃÖÁ¾ Ãâ·Â ¦¡¦¡ */
    export_json(gs);

    if (gs->phase == PHASE_CLEAR) {
        printf("\n=== GAME CLEAR! Ã³Ä¡: %d¸¶¸® ===\n", gs->kills);
    }
    else {
        printf("\n=== GAME OVER. Ã³Ä¡: %d¸¶¸® ===\n", gs->kills);
    }

    game_destroy(gs);
    return 0;
}
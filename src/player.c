/*
 * =============================================================================
 * Project : DeepRoot
 * File    : player.c
 * Desc    : 플레이어 구현
 *           - 생성 / 해제 / 이동 / 방향 / 투사체 / 스탯 / 카메라
 * =============================================================================
 */
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "player.h"

/* ─────────────────────────── 내부 유틸 ────────────────────────────────── */

/* 방향 벡터를 정규화하여 속력 배율 적용 후 반환 */
static Vec2f dir_to_velocity(FaceDir dir, float speed)
{
    Vec2f v = {0.0f, 0.0f};
    float raw_x = 0.0f;
    float raw_y = 0.0f;

    if (dir & FACE_LEFT)  raw_x -= 1.0f;
    if (dir & FACE_RIGHT) raw_x += 1.0f;
    if (dir & FACE_UP)    raw_y -= 1.0f;  /* 화면 Y: 위가 음수           */
    if (dir & FACE_DOWN)  raw_y += 1.0f;

    /* 영벡터 방지 */
    if (raw_x == 0.0f && raw_y == 0.0f) {
        return v;
    }

    /* 대각선 정규화 */
    float len = sqrtf(raw_x * raw_x + raw_y * raw_y);
    v.x = (raw_x / len) * speed;
    v.y = (raw_y / len) * speed;
    return v;
}

/* 투사체 풀에서 비활성 슬롯 인덱스 반환, 없으면 -1 */
static int find_inactive_proj(const Player *player)
{
    int i;
    for (i = 0; i < PLAYER_MAX_PROJ; i++) {
        if (!player->proj_pool[i].is_active) {
            return i;
        }
    }
    return -1;
}

/* ─────────────────────────── 초기화 / 해제 ────────────────────────────── */

/*
 * player_create
 * 플레이어를 힙에 생성하고 초기값을 설정한다.
 * 반환값: 성공 시 Player 포인터, 실패 시 NULL
 */
Player *player_create(const char *name, Vec2f start_pos, int start_room_id)
{
    Player *player;
    int     i;

    if (name == NULL) {
        return NULL;
    }

    player = (Player *)malloc(sizeof(Player));
    if (player == NULL) {
        return NULL;
    }

    /* 기본값 초기화 */
    player->id              = 0;
    strncpy(player->name, name, sizeof(player->name) - 1);
    player->name[sizeof(player->name) - 1] = '\0';

    /* 위치 / 방향 */
    player->position        = start_pos;
    player->face_dir        = FACE_DOWN;          /* 기본 바라보기: 남     */
    player->last_face_dir   = FACE_DOWN;

    /* 스탯 */
    player->hp              = PLAYER_MAX_HP;
    player->max_hp          = PLAYER_MAX_HP;
    player->move_speed      = PLAYER_MOVE_SPEED;
    player->invincible_timer = 0;

    /* 상태 */
    player->state           = PSTATE_IDLE;
    player->current_room_id = start_room_id;

    /* 무기 미장착 */
    player->equipped_weapon = NULL;

    /* 투사체 풀 초기화 */
    for (i = 0; i < PLAYER_MAX_PROJ; i++) {
        memset(&player->proj_pool[i], 0, sizeof(Projectile));
        player->proj_pool[i].is_active = 0;
        player->proj_pool[i].owner_id  = 0;   /* 플레이어 소유           */
    }

    /* 카메라 초기화 */
    player->camera.position    = start_pos;
    player->camera.view_width  = 800;            /* 기본 뷰포트 크기       */
    player->camera.view_height = 600;
    player->camera.zoom        = 1.0f;

    return player;
}

/*
 * player_destroy
 * 플레이어 메모리를 해제한다.
 */
void player_destroy(Player *player)
{
    if (player == NULL) {
        return;
    }
    free(player);
}

/* ─────────────────────────── 이동 / 방향 ──────────────────────────────── */

/*
 * player_move
 * dir 방향으로 플레이어를 이동시킨다.
 * 맵 경계(0 ~ map_width/map_height)를 벗어나지 않도록 클램프.
 * FACE_NONE 이면 이동하지 않는다 (마지막 방향은 유지).
 */
void player_move(Player *player, FaceDir dir, int map_width, int map_height)
{
    Vec2f delta;

    if (player == NULL) {
        return;
    }

    /* 방향 갱신 (face_dir, last_face_dir) */
    player_update_face(player, dir);

    if (dir == FACE_NONE) {
        player->state = PSTATE_IDLE;
        return;
    }

    /* 이동 벡터 계산 */
    delta = dir_to_velocity(dir, (float)player->move_speed);

    player->position.x += delta.x;
    player->position.y += delta.y;

    /* 맵 경계 클램프 */
    if (player->position.x < 0.0f)               player->position.x = 0.0f;
    if (player->position.y < 0.0f)               player->position.y = 0.0f;
    if (player->position.x > (float)map_width)   player->position.x = (float)map_width;
    if (player->position.y > (float)map_height)  player->position.y = (float)map_height;

    player->state = PSTATE_MOVE;
}

/*
 * player_update_face
 * 현재 방향을 갱신한다.
 * dir이 FACE_NONE이 아닐 때만 last_face_dir을 덮어쓴다 (마지막 방향 유지).
 */
void player_update_face(Player *player, FaceDir dir)
{
    if (player == NULL) {
        return;
    }

    player->face_dir = dir;

    if (dir != FACE_NONE) {
        player->last_face_dir = dir;
    }
}

/*
 * player_get_last_dir
 * 마지막으로 입력된 이동 방향을 반환한다.
 */
FaceDir player_get_last_dir(const Player *player)
{
    if (player == NULL) {
        return FACE_NONE;
    }
    return player->last_face_dir;
}

/* ─────────────────────────── 투사체 ───────────────────────────────────── */

/*
 * player_shoot
 * 마지막 이동 방향으로 투사체를 발사한다.
 * 장착된 무기의 데미지를 사용하며, 무기 미장착 시 기본 데미지(1) 적용.
 *
 * 확장 포인트:
 *   - equipped_weapon->multi_shot > 1 일 때 반복 호출하여 다중 발사 가능.
 *   - pierce 여부는 Projectile 단에서 충돌 처리 시 참조할 수 있도록
 *     owner_id 또는 별도 플래그를 추가하는 방식으로 확장.
 *
 * 반환값: 발사 성공 시 슬롯 인덱스, 실패 시 -1
 */
int player_shoot(Player *player)
{
    int        slot;
    Projectile *proj;
    int        damage;
    FaceDir    shoot_dir;

    if (player == NULL) {
        return -1;
    }

    /* 발사 방향: 마지막 이동 방향 사용 */
    shoot_dir = player->last_face_dir;
    if (shoot_dir == FACE_NONE) {
        return -1;   /* 방향 없음: 발사 불가 */
    }

    /* 비활성 슬롯 확보 */
    slot = find_inactive_proj(player);
    if (slot < 0) {
        return -1;   /* 풀 가득 참 */
    }

    /* 데미지 결정 */
    damage = 1;
    if (player->equipped_weapon != NULL) {
        damage = player->equipped_weapon->stat.damage;
    }

    proj            = &player->proj_pool[slot];
    proj->position  = player->position;
    proj->velocity  = dir_to_velocity(shoot_dir, (float)PLAYER_PROJ_SPEED);
    proj->damage    = damage;
    proj->is_active = 1;
    proj->owner_id  = 0;   /* 플레이어 소유 */

    return slot;
}

/*
 * player_update_projectiles
 * 활성화된 모든 투사체의 위치를 velocity만큼 이동시킨다.
 * 화면 밖으로 나간 투사체는 비활성화한다.
 */
void player_update_projectiles(Player *player)
{
    int        i;
    Projectile *proj;

    if (player == NULL) {
        return;
    }

    for (i = 0; i < PLAYER_MAX_PROJ; i++) {
        proj = &player->proj_pool[i];
        if (!proj->is_active) {
            continue;
        }

        proj->position.x += proj->velocity.x;
        proj->position.y += proj->velocity.y;

        /*
         * 간단한 범위 이탈 비활성화.
         * 실제 게임에서는 맵/방 경계를 던전 정보에서 받아 판정한다.
         * 현재는 넓은 범위(±4096)로 처리.
         */
        if (proj->position.x < -4096.0f || proj->position.x > 4096.0f ||
            proj->position.y < -4096.0f || proj->position.y > 4096.0f) {
            proj->is_active = 0;
        }
    }
}

/*
 * player_get_active_proj
 * index 위치의 투사체 포인터를 반환한다.
 * 비활성이거나 범위를 벗어나면 NULL 반환.
 */
Projectile *player_get_active_proj(Player *player, int index)
{
    if (player == NULL) {
        return NULL;
    }
    if (index < 0 || index >= PLAYER_MAX_PROJ) {
        return NULL;
    }
    if (!player->proj_pool[index].is_active) {
        return NULL;
    }
    return &player->proj_pool[index];
}

/* ─────────────────────────── 스탯 / 상태 ──────────────────────────────── */

/*
 * player_take_damage
 * 무적 상태가 아닐 때 데미지를 입힌다.
 * 사망 시 PSTATE_DEAD로 전이.
 * 반환값: 실제로 받은 데미지 (무적이면 0)
 */
int player_take_damage(Player *player, int damage)
{
    if (player == NULL) {
        return 0;
    }
    if (player_is_invincible(player)) {
        return 0;
    }
    if (player->state == PSTATE_DEAD) {
        return 0;
    }

    player->hp -= damage;

    if (player->hp <= 0) {
        player->hp    = 0;
        player->state = PSTATE_DEAD;
    } else {
        player->state            = PSTATE_HIT;
        player->invincible_timer = PLAYER_INVINCIBLE_MS;
    }

    return damage;
}

/*
 * player_heal
 * HP를 회복한다. 최대 체력 초과 금지.
 * 반환값: 실제 회복량
 */
int player_heal(Player *player, int amount)
{
    int actual;

    if (player == NULL || amount <= 0) {
        return 0;
    }
    if (player->state == PSTATE_DEAD) {
        return 0;
    }

    actual = amount;
    if (player->hp + amount > player->max_hp) {
        actual = player->max_hp - player->hp;
    }
    player->hp += actual;
    return actual;
}

/*
 * player_is_alive
 * 반환값: 살아있으면 1, 사망이면 0
 */
int player_is_alive(const Player *player)
{
    if (player == NULL) {
        return 0;
    }
    return (player->state != PSTATE_DEAD) ? 1 : 0;
}

/*
 * player_is_invincible
 * 반환값: 무적 상태이면 1, 아니면 0
 */
int player_is_invincible(const Player *player)
{
    if (player == NULL) {
        return 0;
    }
    return (player->invincible_timer > 0) ? 1 : 0;
}

/*
 * player_update_invincible
 * 프레임마다 무적 타이머를 감소시킨다.
 * 타이머 만료 시 HIT 상태였다면 IDLE로 복귀.
 */
void player_update_invincible(Player *player, int delta_ms)
{
    if (player == NULL) {
        return;
    }
    if (player->invincible_timer <= 0) {
        return;
    }

    player->invincible_timer -= delta_ms;

    if (player->invincible_timer <= 0) {
        player->invincible_timer = 0;
        if (player->state == PSTATE_HIT) {
            player->state = PSTATE_IDLE;
        }
    }
}

/* ─────────────────────────── 무기 장착 ────────────────────────────────── */

/*
 * player_equip_weapon
 * 무기 트리에서 선택된 노드를 장착한다.
 * 반환값: 성공 1, 실패 0
 */
int player_equip_weapon(Player *player, WeaponNode *weapon_node)
{
    if (player == NULL || weapon_node == NULL) {
        return 0;
    }
    player->equipped_weapon = weapon_node;
    return 1;
}

/* ─────────────────────────── 카메라 ───────────────────────────────────── */

/*
 * camera_follow
 * 카메라를 target 위치(플레이어 중심)로 부드럽게 추적하고
 * 맵 경계를 넘지 않도록 클램프한다.
 */
void camera_follow(Camera *camera, Vec2f target, int map_w, int map_h)
{
    float half_w;
    float half_h;
    float min_x, max_x;
    float min_y, max_y;

    if (camera == NULL) {
        return;
    }

    half_w = (float)(camera->view_width)  / (2.0f * camera->zoom);
    half_h = (float)(camera->view_height) / (2.0f * camera->zoom);

    /* 카메라 이동 허용 범위 */
    min_x = half_w;
    max_x = (float)map_w - half_w;
    min_y = half_h;
    max_y = (float)map_h - half_h;

    camera->position.x = target.x;
    camera->position.y = target.y;

    /* 클램프: 맵이 뷰포트보다 작을 때 중앙 고정 */
    if (max_x < min_x) {
        camera->position.x = (float)map_w / 2.0f;
    } else {
        if (camera->position.x < min_x) camera->position.x = min_x;
        if (camera->position.x > max_x) camera->position.x = max_x;
    }

    if (max_y < min_y) {
        camera->position.y = (float)map_h / 2.0f;
    } else {
        if (camera->position.y < min_y) camera->position.y = min_y;
        if (camera->position.y > max_y) camera->position.y = max_y;
    }
}

/*
 * camera_world_to_screen
 * 월드 좌표를 스크린 좌표로 변환한다.
 * screen = (world - camera_pos) * zoom + viewport_center
 */
Vec2f camera_world_to_screen(const Camera *camera, Vec2f world_pos)
{
    Vec2f screen = {0.0f, 0.0f};

    if (camera == NULL) {
        return screen;
    }

    screen.x = (world_pos.x - camera->position.x) * camera->zoom
               + (float)(camera->view_width)  / 2.0f;
    screen.y = (world_pos.y - camera->position.y) * camera->zoom
               + (float)(camera->view_height) / 2.0f;

    return screen;
}

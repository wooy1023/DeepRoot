#ifndef PLAYER_H
#define PLAYER_H

/*
 * =============================================================================
 * Project : DeepRoot
 * File    : player.h
 * Desc    : 플레이어 구조 정의
 *           - 플레이어 상태, 위치, 방향
 *           - 플레이어 중심 카메라
 *           - 이동 방향으로 자동 투사체 발사 (마지막 방향 유지, 대각선 가능)
 * =============================================================================
 */

#include "weapon.h"   /* WeaponNode 참조 */

/* ─────────────────────────── 상수 / 매크로 ────────────────────────────── */

#define PLAYER_MAX_HP        100    /* 플레이어 최대 체력                  */
#define PLAYER_MOVE_SPEED    4      /* 기본 이동 속력 (단위: 픽셀/프레임)  */
#define PLAYER_PROJ_SPEED    8      /* 투사체 속력                         */
#define PLAYER_MAX_PROJ      32     /* 동시 존재 가능한 투사체 최대 수     */
#define PLAYER_INVINCIBLE_MS 800    /* 피격 후 무적 시간 (밀리초)          */

/* ─────────────────────────── 열거형 ───────────────────────────────────── */

/* 8방향 이동 / 조준 방향 (대각선 포함) */
typedef enum FaceDir {
    FACE_NONE       = 0x00,   /* 정지 상태                               */
    FACE_UP         = 0x01,   /* 북 (↑)                                  */
    FACE_DOWN       = 0x02,   /* 남 (↓)                                  */
    FACE_LEFT       = 0x04,   /* 서 (←)                                  */
    FACE_RIGHT      = 0x08,   /* 동 (→)                                  */
    FACE_UP_LEFT    = 0x05,   /* 북서 (↖) FACE_UP | FACE_LEFT            */
    FACE_UP_RIGHT   = 0x09,   /* 북동 (↗) FACE_UP | FACE_RIGHT           */
    FACE_DOWN_LEFT  = 0x06,   /* 남서 (↙) FACE_DOWN | FACE_LEFT          */
    FACE_DOWN_RIGHT = 0x0A    /* 남동 (↘) FACE_DOWN | FACE_RIGHT         */
} FaceDir;

/* 플레이어 상태 */
typedef enum PlayerState {
    PSTATE_IDLE    = 0,   /* 대기                                         */
    PSTATE_MOVE    = 1,   /* 이동 중                                      */
    PSTATE_HIT     = 2,   /* 피격 (무적 프레임)                           */
    PSTATE_DEAD    = 3,   /* 사망                                         */
    PSTATE_WIN     = 4    /* 클리어                                       */
} PlayerState;

/* ─────────────────────────── 구조체 ───────────────────────────────────── */

/* 2D 실수 좌표 */
typedef struct Vec2f {
    float x;
    float y;
} Vec2f;

/* 2D 정수 좌표 (타일 단위) */
typedef struct Vec2i {
    int x;
    int y;
} Vec2i;

/* 플레이어 중심 카메라 */
typedef struct Camera {
    Vec2f  position;     /* 카메라 월드 좌표 (플레이어 중심으로 추적)    */
    int    view_width;   /* 뷰포트 가로 크기 (픽셀)                      */
    int    view_height;  /* 뷰포트 세로 크기 (픽셀)                      */
    float  zoom;         /* 줌 배율 (기본 1.0)                           */
} Camera;

/* 투사체 (이동 방향 자동 발사) */
typedef struct Projectile {
    Vec2f     position;     /* 현재 위치                                 */
    Vec2f     velocity;     /* 방향 벡터 (정규화 후 속력 적용)           */
    int       damage;       /* 투사체 데미지                             */
    int       is_active;    /* 활성 여부 (0 = 비활성, 재사용 가능)       */
    int       owner_id;     /* 발사 주체 ID (플레이어=0, 몬스터=1+)      */
} Projectile;

/* 플레이어 */
typedef struct Player {
    int         id;                          /* 플레이어 ID (확장용)     */
    char        name[32];                    /* 플레이어 이름            */

    /* 위치 / 방향 */
    Vec2f       position;                    /* 월드 좌표                */
    FaceDir     face_dir;                    /* 현재 바라보는 방향       */
    FaceDir     last_face_dir;               /* 마지막 이동 방향 유지    */

    /* 스탯 */
    int         hp;                          /* 현재 체력                */
    int         max_hp;                      /* 최대 체력                */
    int         move_speed;                  /* 이동 속력                */
    int         invincible_timer;            /* 무적 타이머 (ms)         */

    /* 상태 */
    PlayerState state;                       /* 현재 상태                */
    int         current_room_id;             /* 현재 있는 방 ID          */

    /* 무기 (이진 트리에서 선택된 노드 포인터) */
    WeaponNode *equipped_weapon;             /* 장착 중인 무기 노드      */

    /* 투사체 풀 */
    Projectile  proj_pool[PLAYER_MAX_PROJ];  /* 투사체 오브젝트 풀       */

    /* 카메라 */
    Camera      camera;                      /* 플레이어 중심 카메라     */
} Player;

/* ─────────────────────────── 함수 원형 ────────────────────────────────── */

/*
 * 초기화 / 해제
 */
Player      *player_create(const char *name, Vec2f start_pos, int start_room_id);
void         player_destroy(Player *player);

/*
 * 이동 / 방향
 */
void         player_move(Player *player, FaceDir dir, int map_width, int map_height);
void         player_update_face(Player *player, FaceDir dir);
FaceDir      player_get_last_dir(const Player *player);

/*
 * 투사체
 */
int          player_shoot(Player *player);                  /* 마지막 방향으로 자동 발사 */
void         player_update_projectiles(Player *player);     /* 투사체 위치 갱신          */
Projectile  *player_get_active_proj(Player *player, int index); /* 활성 투사체 접근      */

/*
 * 스탯 / 상태
 */
int          player_take_damage(Player *player, int damage);
int          player_heal(Player *player, int amount);
int          player_is_alive(const Player *player);
int          player_is_invincible(const Player *player);
void         player_update_invincible(Player *player, int delta_ms);

/*
 * 무기 장착
 */
int          player_equip_weapon(Player *player, WeaponNode *weapon_node);

/*
 * 카메라
 */
void         camera_follow(Camera *camera, Vec2f target, int map_w, int map_h);
Vec2f        camera_world_to_screen(const Camera *camera, Vec2f world_pos);

#endif /* PLAYER_H */

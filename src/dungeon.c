/*
 * =============================================================================
 * Project : DeepRoot
 * File    : dungeon.c
 * Desc    : 던전 구조 구현 - 인접 리스트 기반 방향 그래프
 *           방(노드)과 연결 통로(간선)를 그래프로 표현한다.
 *           방 0 : 일반 방 (가로 < 세로, 세로로 긴 구조)
 *           방 1 : 보스 방
 * Note    : MSVC(Visual Studio) 환경 호환
 *           - 문자열 리터럴은 ASCII 사용 (한국어 주석은 UTF-8 유지)
 *           - strncpy_s / strncpy 분기 처리
 * =============================================================================
 */

/* MSVC strncpy 보안 경고 억제 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "dungeon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─────────────────────────── 내부 유틸리티 ────────────────────────────── */

/* 방 ID가 던전 내에서 유효한지 검사한다. */
static int is_valid_room_id(const Dungeon *dungeon, int room_id)
{
    if (dungeon == NULL)                  return 0;
    if (room_id < 0)                      return 0;
    if (room_id >= dungeon->room_count)   return 0;
    if (dungeon->rooms[room_id].type == ROOM_EMPTY) return 0;
    return 1;
}

/* 방의 인접 리스트에서 모든 PassageNode를 해제한다. */
static void free_adj_list(PassageNode *head)
{
    PassageNode *cur = head;
    while (cur != NULL) {
        PassageNode *next = cur->next;
        free(cur);
        cur = next;
    }
}

/* ─────────────────────────── 초기화 / 해제 ────────────────────────────── */

/*
 * dungeon_create
 * 던전 그래프를 동적으로 생성하고 초기화한다.
 * 성공 시 Dungeon 포인터 반환, 실패 시 NULL 반환.
 */
Dungeon *dungeon_create(void)
{
    int i;
    Dungeon *dungeon = (Dungeon *)malloc(sizeof(Dungeon));
    if (dungeon == NULL) {
        /* 메모리 할당 실패 */
        return NULL;
    }

    dungeon->room_count    = 0;
    dungeon->start_room_id = DUNGEON_INVALID_ID;
    dungeon->boss_room_id  = DUNGEON_INVALID_ID;

    /* 모든 방 슬롯을 미사용 상태로 초기화 */
    for (i = 0; i < DUNGEON_MAX_ROOMS; i++) {
        dungeon->rooms[i].id          = i;
        dungeon->rooms[i].type        = ROOM_EMPTY;
        dungeon->rooms[i].size.width  = 0;
        dungeon->rooms[i].size.height = 0;
        dungeon->rooms[i].is_cleared  = 0;
        dungeon->rooms[i].adj_list    = NULL;
        memset(dungeon->rooms[i].name, 0, DUNGEON_ROOM_NAME_LEN);
    }

    return dungeon;
}

/*
 * dungeon_destroy
 * 던전 그래프에 할당된 모든 메모리를 해제한다.
 * - 각 방의 인접 리스트(PassageNode 체인) 순차 해제
 * - 던전 구조체 자체 해제
 */
void dungeon_destroy(Dungeon *dungeon)
{
    int i;
    if (dungeon == NULL) return;

    /* 모든 방의 인접 리스트 해제 */
    for (i = 0; i < DUNGEON_MAX_ROOMS; i++) {
        if (dungeon->rooms[i].adj_list != NULL) {
            free_adj_list(dungeon->rooms[i].adj_list);
            dungeon->rooms[i].adj_list = NULL;
        }
    }

    free(dungeon);
}

/* ─────────────────────────── 방 관리 ──────────────────────────────────── */

/*
 * dungeon_add_room
 * 새 방을 던전에 추가한다.
 * - room_count를 ID로 사용 (0-based)
 * - 보스 방이면 boss_room_id 갱신
 * - 첫 번째 방이면 start_room_id 갱신
 * 성공 시 새 방의 ID 반환, 실패 시 DUNGEON_INVALID_ID 반환.
 */
int dungeon_add_room(Dungeon *dungeon, RoomType type,
                     int width, int height, const char *name)
{
    int   new_id;
    Room *room;

    /* NULL 및 유효성 검사 */
    if (dungeon == NULL)                          return DUNGEON_INVALID_ID;
    if (name    == NULL)                          return DUNGEON_INVALID_ID;
    if (dungeon->room_count >= DUNGEON_MAX_ROOMS) return DUNGEON_INVALID_ID;
    if (width <= 0 || height <= 0)                return DUNGEON_INVALID_ID;

    new_id = dungeon->room_count;
    room   = &dungeon->rooms[new_id];

    room->id          = new_id;
    room->type        = type;
    room->size.width  = width;
    room->size.height = height;
    room->is_cleared  = 0;
    room->adj_list    = NULL;

    /* 문자열 복사: MSVC / GCC 분기 */
#ifdef _MSC_VER
    strncpy_s(room->name, DUNGEON_ROOM_NAME_LEN, name, _TRUNCATE);
#else
    strncpy(room->name, name, DUNGEON_ROOM_NAME_LEN - 1);
    room->name[DUNGEON_ROOM_NAME_LEN - 1] = '\0';
#endif

    /* 시작 방 ID 설정 (처음 추가된 방) */
    if (dungeon->room_count == 0) {
        dungeon->start_room_id = new_id;
    }

    /* 보스 방 ID 설정 */
    if (type == ROOM_BOSS) {
        dungeon->boss_room_id = new_id;
    }

    dungeon->room_count++;
    return new_id;
}

/*
 * dungeon_get_room
 * 방 ID로 Room 포인터를 반환한다.
 * 유효하지 않은 ID이면 NULL 반환.
 */
Room *dungeon_get_room(Dungeon *dungeon, int room_id)
{
    if (dungeon == NULL)                      return NULL;
    if (!is_valid_room_id(dungeon, room_id))  return NULL;

    return &dungeon->rooms[room_id];
}

/*
 * dungeon_set_cleared
 * 방의 클리어 여부를 1로 설정한다.
 * 성공 시 0, 실패 시 -1 반환.
 */
int dungeon_set_cleared(Dungeon *dungeon, int room_id)
{
    if (dungeon == NULL)                     return -1;
    if (!is_valid_room_id(dungeon, room_id)) return -1;

    dungeon->rooms[room_id].is_cleared = 1;
    return 0;
}

/* ─────────────────────────── 통로(간선) 관리 ──────────────────────────── */

/*
 * dungeon_add_passage
 * src_id 방에서 dest_id 방으로 향하는 단방향 통로를 추가한다.
 * - 같은 방향(dir)으로 이미 통로가 있으면 -1 반환 (중복 방지)
 * - 새 PassageNode를 인접 리스트 앞에 삽입 (O(1))
 * 성공 시 0, 실패 시 -1 반환.
 */
int dungeon_add_passage(Dungeon *dungeon,
                        int src_id, int dest_id,
                        PassageDir dir, PassageLock lock)
{
    PassageNode *cur;
    PassageNode *node;

    if (dungeon == NULL)                      return -1;
    if (!is_valid_room_id(dungeon, src_id))   return -1;
    if (!is_valid_room_id(dungeon, dest_id))  return -1;
    if (src_id == dest_id)                    return -1;

    /* 동일 방향 통로 중복 여부 확인 */
    cur = dungeon->rooms[src_id].adj_list;
    while (cur != NULL) {
        if (cur->direction == dir) {
            return -1; /* 중복 방향 */
        }
        cur = cur->next;
    }

    /* 새 통로 노드 동적 할당 */
    node = (PassageNode *)malloc(sizeof(PassageNode));
    if (node == NULL) return -1;

    node->dest_room_id = dest_id;
    node->direction    = dir;
    node->lock         = lock;
    node->next         = dungeon->rooms[src_id].adj_list; /* 앞에 삽입 */

    dungeon->rooms[src_id].adj_list = node;
    return 0;
}

/*
 * dungeon_remove_passage
 * src_id 방의 인접 리스트에서 dest_id 를 향하는 통로를 제거한다.
 * 성공 시 0, 실패(없는 통로 포함) 시 -1 반환.
 */
int dungeon_remove_passage(Dungeon *dungeon, int src_id, int dest_id)
{
    PassageNode *prev;
    PassageNode *cur;

    if (dungeon == NULL)                      return -1;
    if (!is_valid_room_id(dungeon, src_id))   return -1;
    if (!is_valid_room_id(dungeon, dest_id))  return -1;

    prev = NULL;
    cur  = dungeon->rooms[src_id].adj_list;

    while (cur != NULL) {
        if (cur->dest_room_id == dest_id) {
            /* 노드 연결 해제 */
            if (prev == NULL) {
                /* 헤드 노드 제거 */
                dungeon->rooms[src_id].adj_list = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            return 0;
        }
        prev = cur;
        cur  = cur->next;
    }

    /* 해당 통로 없음 */
    return -1;
}

/*
 * dungeon_get_passage
 * src_id 방의 인접 리스트에서 지정 방향(dir)에 해당하는 통로 노드를 반환한다.
 * 없으면 NULL 반환.
 */
PassageNode *dungeon_get_passage(const Dungeon *dungeon,
                                 int src_id, PassageDir dir)
{
    PassageNode *cur;

    if (dungeon == NULL)                     return NULL;
    if (!is_valid_room_id(dungeon, src_id))  return NULL;

    cur = dungeon->rooms[src_id].adj_list;
    while (cur != NULL) {
        if (cur->direction == dir) return cur;
        cur = cur->next;
    }

    return NULL;
}

/*
 * dungeon_unlock_passage
 * src_id 방의 지정 방향(dir) 통로를 LOCK_OPEN 으로 변경한다.
 * 성공 시 0, 해당 통로 없으면 -1 반환.
 */
int dungeon_unlock_passage(Dungeon *dungeon, int src_id, PassageDir dir)
{
    PassageNode *node;

    if (dungeon == NULL)                     return -1;
    if (!is_valid_room_id(dungeon, src_id))  return -1;

    node = dungeon_get_passage(dungeon, src_id, dir);
    if (node == NULL) return -1;

    node->lock = LOCK_OPEN;
    return 0;
}

/* ─────────────────────────── 탐색 / 유틸리티 ──────────────────────────── */

/*
 * dungeon_is_connected
 * src_id 에서 dest_id 로 향하는 통로가 인접 리스트에 존재하는지 확인한다.
 * (직접 연결 여부만 확인, BFS/DFS 미사용)
 * 연결되어 있으면 1, 아니면 0 반환.
 */
int dungeon_is_connected(const Dungeon *dungeon, int src_id, int dest_id)
{
    PassageNode *cur;

    if (dungeon == NULL)                      return 0;
    if (!is_valid_room_id(dungeon, src_id))   return 0;
    if (!is_valid_room_id(dungeon, dest_id))  return 0;

    cur = dungeon->rooms[src_id].adj_list;
    while (cur != NULL) {
        if (cur->dest_room_id == dest_id) return 1;
        cur = cur->next;
    }

    return 0;
}

/*
 * dungeon_get_neighbor_id
 * room_id 방에서 지정 방향(dir)에 있는 이웃 방의 ID를 반환한다.
 * 통로가 없으면 DUNGEON_INVALID_ID 반환.
 */
int dungeon_get_neighbor_id(const Dungeon *dungeon,
                             int room_id, PassageDir dir)
{
    PassageNode *node;

    if (dungeon == NULL)                      return DUNGEON_INVALID_ID;
    if (!is_valid_room_id(dungeon, room_id))  return DUNGEON_INVALID_ID;

    node = dungeon_get_passage(dungeon, room_id, dir);
    if (node == NULL) return DUNGEON_INVALID_ID;

    return node->dest_room_id;
}

/*
 * dungeon_all_cleared
 * 던전 내 모든 유효한 방이 클리어 되었는지 확인한다.
 * 모두 클리어 시 1, 하나라도 미클리어 시 0 반환.
 */
int dungeon_all_cleared(const Dungeon *dungeon)
{
    int i;

    if (dungeon == NULL) return 0;

    for (i = 0; i < dungeon->room_count; i++) {
        if (dungeon->rooms[i].type == ROOM_EMPTY) continue;
        if (dungeon->rooms[i].is_cleared == 0)    return 0;
    }

    return 1;
}

/* ─────────────────────────── 디버그 / 출력 ────────────────────────────── */

/* 방 종류를 문자열로 변환하는 내부 헬퍼 (ASCII만 사용) */
static const char *room_type_str(RoomType type)
{
    switch (type) {
        case ROOM_NORMAL: return "NORMAL";
        case ROOM_BOSS:   return "BOSS  ";
        case ROOM_EMPTY:  return "EMPTY ";
        default:          return "UNKNWN";
    }
}

/* 통로 방향을 문자열로 변환하는 내부 헬퍼 (ASCII만 사용) */
static const char *passage_dir_str(PassageDir dir)
{
    switch (dir) {
        case DIR_NORTH: return "N";
        case DIR_SOUTH: return "S";
        case DIR_EAST:  return "E";
        case DIR_WEST:  return "W";
        default:        return "?";
    }
}

/* 통로 잠금 상태를 문자열로 변환하는 내부 헬퍼 (ASCII만 사용) */
static const char *passage_lock_str(PassageLock lock)
{
    switch (lock) {
        case LOCK_OPEN:   return "OPEN  ";
        case LOCK_CLOSED: return "CLOSED";
        case LOCK_BOSS:   return "BOSS  ";
        default:          return "??????";
    }
}

/*
 * dungeon_print_info
 * 던전 전체 요약 정보를 출력한다.
 */
void dungeon_print_info(const Dungeon *dungeon)
{
    int i;

    if (dungeon == NULL) {
        printf("[dungeon_print_info] dungeon ptr is NULL.\n");
        return;
    }

    printf("========================================\n");
    printf(" Dungeon Info\n");
    printf("========================================\n");
    printf(" Room count    : %d\n", dungeon->room_count);
    printf(" Start room ID : %d\n", dungeon->start_room_id);
    printf(" Boss  room ID : %d\n", dungeon->boss_room_id);
    printf("----------------------------------------\n");

    for (i = 0; i < dungeon->room_count; i++) {
        const Room *r = &dungeon->rooms[i];
        if (r->type == ROOM_EMPTY) continue;

        printf(" [Room %2d] %-32s | Type: %s | Size: %2dx%2d | Clear: %s\n",
               r->id,
               r->name,
               room_type_str(r->type),
               r->size.width,
               r->size.height,
               r->is_cleared ? "Y" : "N");
    }
    printf("========================================\n");
}

/*
 * dungeon_print_adj_list
 * 모든 방의 인접 리스트(통로 연결 정보)를 출력한다.
 */
void dungeon_print_adj_list(const Dungeon *dungeon)
{
    int i;

    if (dungeon == NULL) {
        printf("[dungeon_print_adj_list] dungeon ptr is NULL.\n");
        return;
    }

    printf("========================================\n");
    printf(" Adjacency List (Passages)\n");
    printf("========================================\n");

    for (i = 0; i < dungeon->room_count; i++) {
        const Room    *r   = &dungeon->rooms[i];
        PassageNode   *cur = r->adj_list;

        if (r->type == ROOM_EMPTY) continue;

        printf(" Room %d (%s)", r->id, r->name);

        if (cur == NULL) {
            printf(" -> (no passage)\n");
            continue;
        }

        printf("\n");
        while (cur != NULL) {
            printf("   +-- [%s] -> Room %d  Lock: %s\n",
                   passage_dir_str(cur->direction),
                   cur->dest_room_id,
                   passage_lock_str(cur->lock));
            cur = cur->next;
        }
    }
    printf("========================================\n");
}

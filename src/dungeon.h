#ifndef DUNGEON_H
#define DUNGEON_H

/*
 * =============================================================================
 * Project : DeepRoot
 * File    : dungeon.h
 * Desc    : 던전 구조 정의 - 인접 리스트 기반 방향 그래프
 *           방(노드)과 연결 통로(간선)를 그래프로 표현한다.
 *           방 1 : 일반 방 (가로 < 세로, 더 큰 공간)
 *           방 2 : 보스 방
 * =============================================================================
 */

#include <stdint.h>

/* ─────────────────────────── 상수 / 매크로 ────────────────────────────── */

#define DUNGEON_MAX_ROOMS     16    /* 최대 방 개수                         */
#define DUNGEON_ROOM_NAME_LEN 32    /* 방 이름 최대 길이                    */
#define DUNGEON_INVALID_ID    -1    /* 유효하지 않은 방 ID                  */

/* ─────────────────────────── 열거형 ───────────────────────────────────── */

/* 방의 종류 */
typedef enum RoomType {
    ROOM_NORMAL = 0,   /* 일반 방 (방 1, 세로로 긴 구조)                   */
    ROOM_BOSS   = 1,   /* 보스 방 (방 2)                                   */
    ROOM_EMPTY  = 2    /* 미사용 슬롯                                       */
} RoomType;

/* 통로(간선) 방향 */
typedef enum PassageDir {
    DIR_NORTH = 0,
    DIR_SOUTH = 1,
    DIR_EAST  = 2,
    DIR_WEST  = 3,
    DIR_COUNT = 4      /* 방향 수 (배열 크기 용도)                         */
} PassageDir;

/* 통로 잠금 상태 */
typedef enum PassageLock {
    LOCK_OPEN   = 0,   /* 통과 가능                                        */
    LOCK_CLOSED = 1,   /* 잠김 (조건 미충족)                               */
    LOCK_BOSS   = 2    /* 보스 방 전용 잠금                                 */
} PassageLock;

/* ─────────────────────────── 구조체 ───────────────────────────────────── */

/* 인접 리스트의 간선 노드 (방과 방을 잇는 통로 하나) */
typedef struct PassageNode {
    int              dest_room_id;  /* 연결된 목적지 방 ID                 */
    PassageDir       direction;     /* 통로 방향                           */
    PassageLock      lock;          /* 잠금 상태                           */
    struct PassageNode *next;       /* 다음 간선 노드 (연결 리스트)        */
} PassageNode;

/* 방의 크기 (타일 단위) */
typedef struct RoomSize {
    int width;    /* 가로 크기 (타일)                                      */
    int height;   /* 세로 크기 (타일) - 방 1은 width < height 보장        */
} RoomSize;

/* 방(그래프 노드) */
typedef struct Room {
    int          id;                        /* 방 고유 ID (0-based)       */
    RoomType     type;                      /* 방 종류                     */
    RoomSize     size;                      /* 방 크기                     */
    char         name[DUNGEON_ROOM_NAME_LEN]; /* 방 이름                  */
    int          is_cleared;               /* 클리어 여부 (0 / 1)         */
    PassageNode *adj_list;                 /* 인접 리스트 헤드 포인터     */
} Room;

/* 던전 그래프 전체 */
typedef struct Dungeon {
    Room  rooms[DUNGEON_MAX_ROOMS];  /* 방 배열 (노드 집합)               */
    int   room_count;                /* 현재 방 개수                      */
    int   start_room_id;             /* 시작 방 ID                        */
    int   boss_room_id;              /* 보스 방 ID                        */
} Dungeon;

/* ─────────────────────────── 함수 원형 ────────────────────────────────── */

/*
 * 초기화 / 해제
 */
Dungeon     *dungeon_create(void);
void         dungeon_destroy(Dungeon *dungeon);

/*
 * 방 관리
 */
int          dungeon_add_room(Dungeon *dungeon, RoomType type,
                              int width, int height, const char *name);
Room        *dungeon_get_room(Dungeon *dungeon, int room_id);
int          dungeon_set_cleared(Dungeon *dungeon, int room_id);

/*
 * 통로(간선) 관리
 */
int          dungeon_add_passage(Dungeon *dungeon,
                                 int src_id, int dest_id,
                                 PassageDir dir, PassageLock lock);
int          dungeon_remove_passage(Dungeon *dungeon,
                                    int src_id, int dest_id);
PassageNode *dungeon_get_passage(const Dungeon *dungeon,
                                 int src_id, PassageDir dir);
int          dungeon_unlock_passage(Dungeon *dungeon,
                                    int src_id, PassageDir dir);

/*
 * 탐색 / 유틸리티
 */
int          dungeon_is_connected(const Dungeon *dungeon,
                                   int src_id, int dest_id);
int          dungeon_get_neighbor_id(const Dungeon *dungeon,
                                      int room_id, PassageDir dir);
int          dungeon_all_cleared(const Dungeon *dungeon);

/*
 * 디버그 / 출력 (export.h 와 연동 가능)
 */
void         dungeon_print_info(const Dungeon *dungeon);
void         dungeon_print_adj_list(const Dungeon *dungeon);

#endif /* DUNGEON_H */

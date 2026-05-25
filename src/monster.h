#ifndef MONSTER_H
#define MONSTER_H

/*
 * =============================================================================
 * Project : DeepRoot
 * File    : monster.h
 * Desc    : 몬스터 구조 정의
 *           - 일반 몬스터 스폰 관리: 원형 큐 (FIFO)
 *           - 우선순위 기반 행동 스케줄링: 최소 힙 우선순위 큐
 *             (우선순위 값이 낮을수록 먼저 행동)
 *           - 보스 몬스터는 별도 구조체
 * =============================================================================
 */

#include "player.h"   /* Vec2f, Vec2i 참조 */

/* ─────────────────────────── 상수 / 매크로 ────────────────────────────── */

#define MONSTER_QUEUE_MAX     64    /* 스폰 대기 큐 최대 수                */
#define MONSTER_HEAP_MAX      64    /* 우선순위 힙 최대 크기               */
#define MONSTER_NAME_LEN      24    /* 몬스터 이름 최대 길이               */
#define MONSTER_INVALID_ID    -1    /* 유효하지 않은 ID                    */

/* ─────────────────────────── 열거형 ───────────────────────────────────── */

/* 몬스터 종류 */
typedef enum MonsterType {
    MON_SLIME      = 0,   /* 슬라임 (기본)                                */
    MON_GOBLIN     = 1,   /* 고블린                                       */
    MON_SKELETON   = 2,   /* 스켈레톤                                     */
    MON_BOSS       = 3,   /* 보스 (방 2 전용)                             */
    MON_TYPE_COUNT = 4    /* 종류 수                                       */
} MonsterType;

/* 몬스터 현재 상태 */
typedef enum MonsterState {
    MSTATE_IDLE    = 0,   /* 대기                                         */
    MSTATE_PATROL  = 1,   /* 순찰                                         */
    MSTATE_CHASE   = 2,   /* 추격                                         */
    MSTATE_ATTACK  = 3,   /* 공격                                         */
    MSTATE_HIT     = 4,   /* 피격                                         */
    MSTATE_DEAD    = 5    /* 사망                                         */
} MonsterState;

/* 행동 이벤트 종류 (우선순위 큐에서 처리) */
typedef enum ActionType {
    ACTION_MOVE    = 0,   /* 이동                                         */
    ACTION_ATTACK  = 1,   /* 공격                                         */
    ACTION_SPAWN   = 2,   /* 스폰 처리                                    */
    ACTION_DIE     = 3    /* 사망 처리                                     */
} ActionType;

/* ─────────────────────────── 구조체 ───────────────────────────────────── */

/* 몬스터 개체 */
typedef struct Monster {
    int          id;                    /* 고유 ID                        */
    MonsterType  type;                  /* 종류                           */
    MonsterState state;                 /* 현재 상태                      */
    char         name[MONSTER_NAME_LEN];/* 이름                          */

    /* 위치 / 이동 */
    Vec2f        position;             /* 월드 좌표                       */
    Vec2f        velocity;             /* 이동 벡터                       */
    int          move_speed;           /* 이동 속력                       */

    /* 스탯 */
    int          hp;                   /* 현재 체력                       */
    int          max_hp;               /* 최대 체력                       */
    int          attack_power;         /* 공격력                          */
    int          attack_range;         /* 공격 사거리 (픽셀)              */
    int          detect_range;         /* 감지 사거리 (픽셀)              */

    /* 소속 */
    int          room_id;              /* 스폰된 방 ID                    */
    int          is_active;            /* 활성 여부                       */
} Monster;

/* ─────────── 자료구조 1 : 스폰 대기 원형 큐 ─────────── */

/*
 * 원형 큐 (FIFO)
 * 방에 입장 시 스폰될 몬스터 ID를 순서대로 관리한다.
 */
typedef struct MonsterQueue {
    int data[MONSTER_QUEUE_MAX];   /* 몬스터 ID 저장 배열                 */
    int front;                     /* 큐 앞 인덱스                        */
    int rear;                      /* 큐 뒤 인덱스                        */
    int size;                      /* 현재 원소 수                        */
} MonsterQueue;

/* ─────────── 자료구조 2 : 행동 스케줄 최소 힙 ─────────── */

/*
 * 우선순위 큐 원소
 * priority 값이 낮을수록 먼저 처리 (최소 힙).
 * tick: 처리될 게임 틱 시간으로 사용 가능.
 */
typedef struct HeapNode {
    int        monster_id;    /* 행동 대상 몬스터 ID                      */
    ActionType action;        /* 실행할 행동                              */
    int        priority;      /* 우선순위 (낮을수록 먼저)                 */
} HeapNode;

/* 최소 힙 (배열 기반) */
typedef struct MonsterHeap {
    HeapNode data[MONSTER_HEAP_MAX];  /* 힙 배열                          */
    int      size;                    /* 현재 원소 수                     */
} MonsterHeap;

/* ─────────── 몬스터 매니저 (방 단위) ─────────── */

/*
 * 방 하나의 몬스터 전체 상태를 관리하는 컨테이너.
 * pool: 실제 몬스터 개체 배열
 * spawn_queue: 스폰 대기 큐
 * action_heap: 행동 우선순위 큐
 */
typedef struct MonsterManager {
    Monster      pool[MONSTER_QUEUE_MAX]; /* 몬스터 오브젝트 풀           */
    int          pool_count;              /* 풀 내 총 몬스터 수           */

    MonsterQueue spawn_queue;             /* 스폰 대기 원형 큐            */
    MonsterHeap  action_heap;             /* 행동 스케줄 최소 힙          */

    int          room_id;                 /* 소속 방 ID                   */
    int          alive_count;            /* 현재 살아있는 몬스터 수       */
} MonsterManager;

/* ─────────────────────────── 함수 원형 ────────────────────────────────── */

/*
 * MonsterManager 초기화 / 해제
 */
MonsterManager *monster_manager_create(int room_id);
void            monster_manager_destroy(MonsterManager *mgr);

/*
 * 몬스터 개체 관리
 */
int             monster_spawn(MonsterManager *mgr, MonsterType type,
                              Vec2f pos, int hp, int atk);
Monster        *monster_get(MonsterManager *mgr, int monster_id);
int             monster_kill(MonsterManager *mgr, int monster_id);
int             monster_take_damage(MonsterManager *mgr, int monster_id, int dmg);

/*
 * 원형 큐 (스폰 대기)
 */
int             mqueue_enqueue(MonsterQueue *q, int monster_id);
int             mqueue_dequeue(MonsterQueue *q, int *out_id);
int             mqueue_is_empty(const MonsterQueue *q);
int             mqueue_is_full(const MonsterQueue *q);
int             mqueue_size(const MonsterQueue *q);

/*
 * 최소 힙 (행동 우선순위 큐)
 */
int             mheap_push(MonsterHeap *heap, HeapNode node);
int             mheap_pop(MonsterHeap *heap, HeapNode *out_node);
int             mheap_peek(const MonsterHeap *heap, HeapNode *out_node);
int             mheap_is_empty(const MonsterHeap *heap);
int             mheap_is_full(const MonsterHeap *heap);
void            mheap_clear(MonsterHeap *heap);

/*
 * 업데이트 / AI
 */
void            monster_manager_update(MonsterManager *mgr,
                                       const Vec2f *player_pos,
                                       int delta_ms);
void            monster_process_actions(MonsterManager *mgr,
                                        const Vec2f *player_pos);
int             monster_manager_all_dead(const MonsterManager *mgr);

#endif /* MONSTER_H */

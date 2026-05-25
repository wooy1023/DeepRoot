/*
 * =============================================================================
 * Project : DeepRoot
 * File    : monster.c
 * Desc    : 몬스터 구조 구현
 *           - 원형 큐: 스폰 대기 관리 (FIFO)
 *           - 최소 힙: 행동 우선순위 큐 (priority 값 낮을수록 먼저)
 *           - MonsterManager: 방 단위 몬스터 전체 관리
 * =============================================================================
 */
#define _CRT_SECURE_NO_WARNINGS

#include "monster.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ─────────────────────────── 내부 상수 ────────────────────────────────── */

/* 몬스터 종류별 기본 스탯 테이블 [type] = { base_hp, base_atk, speed, atk_range, detect_range } */
static const int BASE_HP[MON_TYPE_COUNT]         = { 30,  50, 40, 200 }; /* 슬라임/고블린/스켈레톤/보스 */
static const int BASE_ATK[MON_TYPE_COUNT]        = {  5,  10,  8,  30 };
static const int BASE_SPEED[MON_TYPE_COUNT]      = {  1,   2,  2,   1 };
static const int BASE_ATK_RANGE[MON_TYPE_COUNT]  = { 30,  40, 50,  80 };
static const int BASE_DETECT[MON_TYPE_COUNT]     = {120, 160,140, 300 };

/* 몬스터 종류별 이름 */
static const char * const MON_NAME[MON_TYPE_COUNT] = {
    "Slime", "Goblin", "Skeleton", "Boss"
};

/* ─────────────────────────── 내부 헬퍼 함수 ────────────────────────────── */

/*
 * 두 Vec2f 사이의 유클리드 거리 반환
 */
static float vec2f_dist(Vec2f a, Vec2f b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

/*
 * 풀에서 비활성 슬롯 인덱스 탐색
 * 없으면 -1 반환
 */
static int pool_find_free_slot(const MonsterManager *mgr)
{
    int i;
    for (i = 0; i < MONSTER_QUEUE_MAX; i++) {
        if (mgr->pool[i].is_active == 0) {
            return i;
        }
    }
    return -1; /* 빈 슬롯 없음 */
}

/*
 * 풀에서 monster_id로 인덱스 탐색
 * 없으면 -1 반환
 */
static int pool_find_by_id(const MonsterManager *mgr, int monster_id)
{
    int i;
    for (i = 0; i < MONSTER_QUEUE_MAX; i++) {
        if (mgr->pool[i].is_active && mgr->pool[i].id == monster_id) {
            return i;
        }
    }
    return -1;
}

/* ─────────────────────────── 원형 큐 구현 ─────────────────────────────── */

/*
 * 원형 큐 내부 초기화
 */
static void mqueue_init(MonsterQueue *q)
{
    if (q == NULL) return;
    q->front = 0;
    q->rear  = 0;
    q->size  = 0;
}

/*
 * 큐가 비어 있으면 1, 아니면 0 반환
 */
int mqueue_is_empty(const MonsterQueue *q)
{
    if (q == NULL) return 1;
    return (q->size == 0);
}

/*
 * 큐가 가득 찼으면 1, 아니면 0 반환
 */
int mqueue_is_full(const MonsterQueue *q)
{
    if (q == NULL) return 1;
    return (q->size >= MONSTER_QUEUE_MAX);
}

/*
 * 현재 큐 원소 수 반환
 */
int mqueue_size(const MonsterQueue *q)
{
    if (q == NULL) return 0;
    return q->size;
}

/*
 * 큐 뒤에 monster_id 삽입
 * 성공 1 / 실패(NULL 또는 만석) 0
 */
int mqueue_enqueue(MonsterQueue *q, int monster_id)
{
    if (q == NULL) return 0;
    if (mqueue_is_full(q)) return 0; /* 만석 */

    q->data[q->rear] = monster_id;
    q->rear = (q->rear + 1) % MONSTER_QUEUE_MAX; /* 원형 인덱스 증가 */
    q->size++;
    return 1;
}

/*
 * 큐 앞에서 monster_id 꺼내기
 * out_id 에 값을 저장 후 성공 1 반환
 * 실패(NULL 또는 빈 큐) 0 반환
 */
int mqueue_dequeue(MonsterQueue *q, int *out_id)
{
    if (q == NULL || out_id == NULL) return 0;
    if (mqueue_is_empty(q)) return 0; /* 빈 큐 */

    *out_id  = q->data[q->front];
    q->front = (q->front + 1) % MONSTER_QUEUE_MAX; /* 원형 인덱스 증가 */
    q->size--;
    return 1;
}

/* ─────────────────────────── 최소 힙 구현 ─────────────────────────────── */

/*
 * 최소 힙 내부 초기화
 */
static void mheap_init(MonsterHeap *heap)
{
    if (heap == NULL) return;
    heap->size = 0;
}

/*
 * 힙이 비어 있으면 1, 아니면 0 반환
 */
int mheap_is_empty(const MonsterHeap *heap)
{
    if (heap == NULL) return 1;
    return (heap->size == 0);
}

/*
 * 힙이 가득 찼으면 1, 아니면 0 반환
 */
int mheap_is_full(const MonsterHeap *heap)
{
    if (heap == NULL) return 1;
    return (heap->size >= MONSTER_HEAP_MAX);
}

/*
 * 힙 전체 초기화 (사이즈를 0으로 리셋)
 */
void mheap_clear(MonsterHeap *heap)
{
    if (heap == NULL) return;
    heap->size = 0;
}

/*
 * 힙 배열 내 두 노드 교환
 */
static void heap_swap(MonsterHeap *heap, int i, int j)
{
    HeapNode tmp   = heap->data[i];
    heap->data[i]  = heap->data[j];
    heap->data[j]  = tmp;
}

/*
 * Sift-Up: 삽입 후 부모와 비교하며 위로 올라감 (최소 힙 유지)
 */
static void heap_sift_up(MonsterHeap *heap, int idx)
{
    int parent;
    while (idx > 0) {
        parent = (idx - 1) / 2;
        if (heap->data[parent].priority > heap->data[idx].priority) {
            heap_swap(heap, parent, idx);
            idx = parent;
        } else {
            break;
        }
    }
}

/*
 * Sift-Down: 루트 제거 후 자식과 비교하며 아래로 내려감 (최소 힙 유지)
 */
static void heap_sift_down(MonsterHeap *heap, int idx)
{
    int smallest, left, right;
    int n = heap->size;

    while (1) {
        smallest = idx;
        left     = 2 * idx + 1;
        right    = 2 * idx + 2;

        /* 왼쪽 자식이 더 작으면 교환 후보 */
        if (left < n && heap->data[left].priority < heap->data[smallest].priority) {
            smallest = left;
        }
        /* 오른쪽 자식이 더 작으면 교환 후보 */
        if (right < n && heap->data[right].priority < heap->data[smallest].priority) {
            smallest = right;
        }

        if (smallest == idx) break; /* 힙 조건 만족, 종료 */

        heap_swap(heap, idx, smallest);
        idx = smallest;
    }
}

/*
 * 힙에 노드 삽입
 * 성공 1 / 실패(NULL 또는 만석) 0
 */
int mheap_push(MonsterHeap *heap, HeapNode node)
{
    if (heap == NULL) return 0;
    if (mheap_is_full(heap)) return 0; /* 공간 없음 */

    heap->data[heap->size] = node; /* 마지막에 삽입 */
    heap->size++;
    heap_sift_up(heap, heap->size - 1); /* 힙 성질 복원 */
    return 1;
}

/*
 * 최솟값(루트) 꺼내기
 * out_node 에 값을 저장 후 성공 1 반환
 * 실패(NULL 또는 빈 힙) 0 반환
 */
int mheap_pop(MonsterHeap *heap, HeapNode *out_node)
{
    if (heap == NULL || out_node == NULL) return 0;
    if (mheap_is_empty(heap)) return 0;

    *out_node = heap->data[0]; /* 루트(최솟값) 복사 */

    /* 마지막 원소를 루트로 이동 후 크기 감소 */
    heap->size--;
    if (heap->size > 0) {
        heap->data[0] = heap->data[heap->size];
        heap_sift_down(heap, 0); /* 힙 성질 복원 */
    }
    return 1;
}

/*
 * 최솟값(루트) 조회 (꺼내지 않음)
 * 성공 1 / 실패 0
 */
int mheap_peek(const MonsterHeap *heap, HeapNode *out_node)
{
    if (heap == NULL || out_node == NULL) return 0;
    if (mheap_is_empty(heap)) return 0;

    *out_node = heap->data[0];
    return 1;
}

/* ─────────────────────────── MonsterManager ────────────────────────────── */

/*
 * MonsterManager 동적 생성 및 초기화
 * room_id: 소속 방 ID
 * 반환: 초기화된 포인터 / NULL (메모리 부족)
 */
MonsterManager *monster_manager_create(int room_id)
{
    MonsterManager *mgr = (MonsterManager *)malloc(sizeof(MonsterManager));
    if (mgr == NULL) return NULL; /* 메모리 할당 실패 */

    memset(mgr, 0, sizeof(MonsterManager));

    mgr->room_id     = room_id;
    mgr->pool_count  = 0;
    mgr->alive_count = 0;

    mqueue_init(&mgr->spawn_queue);
    mheap_init(&mgr->action_heap);

    /* 풀 전체를 비활성 상태로 초기화 */
    {
        int i;
        for (i = 0; i < MONSTER_QUEUE_MAX; i++) {
            mgr->pool[i].id        = MONSTER_INVALID_ID;
            mgr->pool[i].is_active = 0;
        }
    }

    return mgr;
}

/*
 * MonsterManager 메모리 해제
 */
void monster_manager_destroy(MonsterManager *mgr)
{
    if (mgr == NULL) return;
    free(mgr);
}

/* ─────────────────────────── 몬스터 개체 관리 ─────────────────────────── */

/*
 * 새 몬스터 스폰
 * - 풀에서 빈 슬롯을 찾아 초기화
 * - 스폰 대기 큐에 ID 등록
 * 반환: 부여된 monster_id / MONSTER_INVALID_ID (실패)
 */
int monster_spawn(MonsterManager *mgr, MonsterType type,
                  Vec2f pos, int hp, int atk)
{
    int      slot;
    Monster *m;

    if (mgr == NULL) return MONSTER_INVALID_ID;
    if (type < 0 || type >= MON_TYPE_COUNT) return MONSTER_INVALID_ID;

    slot = pool_find_free_slot(mgr);
    if (slot < 0) return MONSTER_INVALID_ID; /* 풀 가득 참 */

    m = &mgr->pool[slot];

    /* 기본 스탯 설정 (hp/atk 이 0 이하이면 종류별 기본값 사용) */
    m->id           = slot; /* 슬롯 인덱스를 ID로 사용 */
    m->type         = type;
    m->state        = MSTATE_IDLE;
    m->position     = pos;
    m->velocity.x   = 0.0f;
    m->velocity.y   = 0.0f;
    m->move_speed   = BASE_SPEED[type];
    m->max_hp       = (hp  > 0) ? hp  : BASE_HP[type];
    m->hp           = m->max_hp;
    m->attack_power = (atk > 0) ? atk : BASE_ATK[type];
    m->attack_range = BASE_ATK_RANGE[type];
    m->detect_range = BASE_DETECT[type];
    m->room_id      = mgr->room_id;
    m->is_active    = 1;

    strncpy(m->name, MON_NAME[type], MONSTER_NAME_LEN - 1);
    m->name[MONSTER_NAME_LEN - 1] = '\0'; /* NULL 종단 보장 */

    mgr->pool_count++;
    mgr->alive_count++;

    /* 스폰 큐에 등록 (실패해도 몬스터 자체는 활성 상태 유지) */
    mqueue_enqueue(&mgr->spawn_queue, m->id);

    return m->id;
}

/*
 * ID로 Monster 포인터 반환
 * 없으면 NULL
 */
Monster *monster_get(MonsterManager *mgr, int monster_id)
{
    int idx;
    if (mgr == NULL) return NULL;
    if (monster_id == MONSTER_INVALID_ID) return NULL;

    idx = pool_find_by_id(mgr, monster_id);
    if (idx < 0) return NULL;

    return &mgr->pool[idx];
}

/*
 * 몬스터 제거 (사망 처리 후 비활성화)
 * 성공 1 / 실패 0
 */
int monster_kill(MonsterManager *mgr, int monster_id)
{
    int      idx;
    Monster *m;

    if (mgr == NULL) return 0;

    idx = pool_find_by_id(mgr, monster_id);
    if (idx < 0) return 0;

    m = &mgr->pool[idx];
    if (m->state == MSTATE_DEAD) return 0; /* 이미 사망 */

    m->state     = MSTATE_DEAD;
    m->hp        = 0;
    m->is_active = 0;

    /* 사망 행동을 힙에 삽입 (우선순위 0 = 즉시 처리) */
    {
        HeapNode node;
        node.monster_id = monster_id;
        node.action     = ACTION_DIE;
        node.priority   = 0;
        mheap_push(&mgr->action_heap, node);
    }

    mgr->alive_count--;
    if (mgr->alive_count < 0) mgr->alive_count = 0;

    return 1;
}

/*
 * 데미지 적용
 * hp가 0 이하가 되면 monster_kill 호출
 * 성공(생존) 1 / 성공(사망) 0 / 실패 -1
 */
int monster_take_damage(MonsterManager *mgr, int monster_id, int dmg)
{
    Monster *m;

    if (mgr == NULL) return -1;
    if (dmg < 0) dmg = 0;

    m = monster_get(mgr, monster_id);
    if (m == NULL) return -1;
    if (m->state == MSTATE_DEAD) return -1; /* 이미 사망 */

    m->hp    -= dmg;
    m->state  = MSTATE_HIT;

    if (m->hp <= 0) {
        m->hp = 0;
        monster_kill(mgr, monster_id); /* 사망 처리 */
        return 0; /* 사망 */
    }
    return 1; /* 생존 */
}

/* ─────────────────────────── AI / 업데이트 ────────────────────────────── */

/*
 * 단일 몬스터 AI: 플레이어 위치에 따라 상태 전환 및 이동 벡터 계산
 */
static void monster_ai_step(Monster *m, const Vec2f *player_pos)
{
    float dist;
    float dx, dy, len;

    if (m == NULL || player_pos == NULL) return;
    if (m->state == MSTATE_DEAD) return;

    dist = vec2f_dist(m->position, *player_pos);

    if (dist <= (float)m->attack_range) {
        /* 공격 범위 내: 공격 상태 전환, 이동 정지 */
        m->state      = MSTATE_ATTACK;
        m->velocity.x = 0.0f;
        m->velocity.y = 0.0f;
    } else if (dist <= (float)m->detect_range) {
        /* 감지 범위 내: 플레이어 추격 */
        m->state = MSTATE_CHASE;

        dx  = player_pos->x - m->position.x;
        dy  = player_pos->y - m->position.y;
        len = sqrtf(dx * dx + dy * dy);

        if (len > 0.0f) {
            /* 방향 벡터 정규화 후 속력 적용 */
            m->velocity.x = (dx / len) * (float)m->move_speed;
            m->velocity.y = (dy / len) * (float)m->move_speed;
        }
    } else {
        /* 감지 범위 밖: 대기 */
        m->state      = MSTATE_IDLE;
        m->velocity.x = 0.0f;
        m->velocity.y = 0.0f;
    }
}

/*
 * 단일 몬스터 위치 업데이트
 * delta_ms: 경과 밀리초
 */
static void monster_apply_velocity(Monster *m, int delta_ms)
{
    float dt;
    if (m == NULL) return;
    if (m->state == MSTATE_DEAD) return;

    /* 픽셀/프레임 단위 속도를 ms 기준으로 보정 (60fps 기준: 1frame ≒ 16.67ms) */
    dt = (float)delta_ms / 16.667f;

    m->position.x += m->velocity.x * dt;
    m->position.y += m->velocity.y * dt;
}

/*
 * 행동 우선순위 큐에 이동/공격 행동 예약
 * 추격 중인 몬스터만 이동 행동 삽입, 공격 범위면 공격 행동 삽입
 */
static void monster_schedule_actions(MonsterManager *mgr)
{
    int i;
    HeapNode node;

    if (mgr == NULL) return;

    for (i = 0; i < MONSTER_QUEUE_MAX; i++) {
        Monster *m = &mgr->pool[i];
        if (!m->is_active || m->state == MSTATE_DEAD) continue;

        if (m->state == MSTATE_CHASE) {
            /* 이동 행동: 속력이 빠를수록 우선순위 높게(값 낮게) */
            node.monster_id = m->id;
            node.action     = ACTION_MOVE;
            node.priority   = 100 - m->move_speed; /* 빠른 몬스터 먼저 이동 */
            mheap_push(&mgr->action_heap, node);
        } else if (m->state == MSTATE_ATTACK) {
            /* 공격 행동: 공격력이 강할수록 우선순위 높게 */
            node.monster_id = m->id;
            node.action     = ACTION_ATTACK;
            node.priority   = 200 - m->attack_power; /* 강한 몬스터 먼저 공격 */
            mheap_push(&mgr->action_heap, node);
        }
    }
}

/*
 * 우선순위 큐에서 행동 꺼내 처리
 * (이동: 위치 반영 / 공격, 사망: 외부 로직 트리거용 상태 유지)
 */
void monster_process_actions(MonsterManager *mgr, const Vec2f *player_pos)
{
    HeapNode node;
    Monster *m;

    if (mgr == NULL || player_pos == NULL) return;

    /* 힙에 남은 모든 행동 처리 */
    while (!mheap_is_empty(&mgr->action_heap)) {
        if (!mheap_pop(&mgr->action_heap, &node)) break;

        m = monster_get(mgr, node.monster_id);
        if (m == NULL || m->state == MSTATE_DEAD) continue;

        switch (node.action) {
            case ACTION_MOVE:
                /* 위치 적용은 update 단계에서 처리됨, 여기서는 상태 확인만 */
                break;

            case ACTION_ATTACK:
                /* 공격 상태 유지 (실제 데미지는 main/game loop에서 처리) */
                m->state = MSTATE_ATTACK;
                break;

            case ACTION_SPAWN:
                /* 스폰 큐에서 꺼내 활성화 (이미 spawn에서 처리됨) */
                m->state = MSTATE_IDLE;
                break;

            case ACTION_DIE:
                /* 사망 처리 확인, 비활성 상태 유지 */
                m->is_active = 0;
                break;

            default:
                break;
        }
    }
}

/*
 * MonsterManager 전체 업데이트 (매 게임 루프 호출)
 * 1. 스폰 큐에서 대기 몬스터 꺼내 스폰 행동 예약
 * 2. 각 몬스터 AI 실행
 * 3. 행동 스케줄 등록
 * 4. 행동 처리
 * 5. 위치 갱신
 */
void monster_manager_update(MonsterManager *mgr,
                            const Vec2f *player_pos,
                            int delta_ms)
{
    int i;
    int spawn_id;

    if (mgr == NULL || player_pos == NULL) return;

    /* ── 1. 스폰 대기 큐 처리 ── */
    while (!mqueue_is_empty(&mgr->spawn_queue)) {
        if (!mqueue_dequeue(&mgr->spawn_queue, &spawn_id)) break;

        /* 스폰 행동을 힙에 삽입 */
        {
            HeapNode node;
            node.monster_id = spawn_id;
            node.action     = ACTION_SPAWN;
            node.priority   = 50; /* 스폰은 중간 우선순위 */
            mheap_push(&mgr->action_heap, node);
        }
    }

    /* ── 2. 각 몬스터 AI 판단 ── */
    for (i = 0; i < MONSTER_QUEUE_MAX; i++) {
        Monster *m = &mgr->pool[i];
        if (!m->is_active || m->state == MSTATE_DEAD) continue;

        /* 피격 상태는 한 틱 유지 후 추격/대기로 전환 */
        if (m->state == MSTATE_HIT) {
            m->state = (vec2f_dist(m->position, *player_pos) <= (float)m->detect_range)
                       ? MSTATE_CHASE : MSTATE_IDLE;
        }

        monster_ai_step(m, player_pos);
    }

    /* ── 3. 행동 스케줄 등록 ── */
    monster_schedule_actions(mgr);

    /* ── 4. 행동 큐 처리 ── */
    monster_process_actions(mgr, player_pos);

    /* ── 5. 위치 갱신 ── */
    for (i = 0; i < MONSTER_QUEUE_MAX; i++) {
        Monster *m = &mgr->pool[i];
        if (!m->is_active || m->state == MSTATE_DEAD) continue;

        monster_apply_velocity(m, delta_ms);
    }
}

/*
 * 방 내 모든 몬스터가 사망했는지 확인
 * 모두 사망(또는 없음): 1 / 생존자 있음: 0
 */
int monster_manager_all_dead(const MonsterManager *mgr)
{
    if (mgr == NULL) return 1;
    return (mgr->alive_count == 0);
}

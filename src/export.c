#define _CRT_SECURE_NO_WARNINGS
#pragma execution_character_set("utf-8")

/*
 * =============================================================================
 * Project : DeepRoot
 * File    : export.c
 * Desc    : 게임 데이터 내보내기 / 저장 / 출력 모듈 구현
 *           - 던전 그래프 (인접 행렬 / 인접 리스트)
 *           - 무기 이진 트리 ASCII 렌더링
 *           - 몬스터 원형 큐 / 최소 힙 출력
 *           - 플레이어 상태 출력
 *           - 게임 스냅샷 / CSV 직렬화
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "export.h"

/* =========================================================================
 * 섹션 1 : 내부 전용 헬퍼 함수 (static)
 * ========================================================================= */

/*
 * PassageDir 열거형 → ASCII 문자열 변환
 * 범위를 벗어나면 "?" 반환
 */
static const char *priv_dir_to_str(PassageDir dir)
{
    /* DIR_NORTH / DIR_SOUTH / DIR_EAST / DIR_WEST 순서 */
    static const char *names[DIR_COUNT] = { "N", "S", "E", "W" };
    if (dir < DIR_COUNT)
        return names[dir];
    return "?";
}

/*
 * PassageLock 잠금 상태 → ASCII 문자열 변환
 */
static const char *priv_lock_to_str(PassageLock lock)
{
    switch (lock) {
        case LOCK_OPEN:   return "OPEN";
        case LOCK_CLOSED: return "CLOSED";
        case LOCK_BOSS:   return "BOSS_LOCK";
        default:          return "?";
    }
}

/*
 * RoomType 방 종류 → ASCII 문자열 변환
 */
static const char *priv_room_type_to_str(RoomType type)
{
    switch (type) {
        case ROOM_NORMAL: return "NORMAL";
        case ROOM_BOSS:   return "BOSS";
        case ROOM_EMPTY:  return "EMPTY";
        default:          return "?";
    }
}

/*
 * MonsterState 몬스터 상태 → ASCII 문자열 변환
 */
static const char *priv_mstate_to_str(MonsterState state)
{
    switch (state) {
        case MSTATE_IDLE:   return "IDLE";
        case MSTATE_PATROL: return "PATROL";
        case MSTATE_CHASE:  return "CHASE";
        case MSTATE_ATTACK: return "ATTACK";
        case MSTATE_HIT:    return "HIT";
        case MSTATE_DEAD:   return "DEAD";
        default:            return "?";
    }
}

/*
 * ActionType 행동 종류 → ASCII 문자열 변환
 */
static const char *priv_action_to_str(ActionType action)
{
    switch (action) {
        case ACTION_MOVE:   return "MOVE";
        case ACTION_ATTACK: return "ATTACK";
        case ACTION_SPAWN:  return "SPAWN";
        case ACTION_DIE:    return "DIE";
        default:            return "?";
    }
}

/*
 * PlayerState 플레이어 상태 → ASCII 문자열 변환
 */
static const char *priv_pstate_to_str(PlayerState state)
{
    switch (state) {
        case PSTATE_IDLE: return "IDLE";
        case PSTATE_MOVE: return "MOVE";
        case PSTATE_HIT:  return "HIT";
        case PSTATE_DEAD: return "DEAD";
        case PSTATE_WIN:  return "WIN";
        default:          return "?";
    }
}

/*
 * FaceDir 방향 비트 플래그 → ASCII 문자열 변환
 * 대각선(비트 OR) 조합까지 처리한다.
 */
static const char *priv_facedir_to_str(FaceDir dir)
{
    switch (dir) {
        case FACE_NONE:       return "NONE";
        case FACE_UP:         return "UP";
        case FACE_DOWN:       return "DOWN";
        case FACE_LEFT:       return "LEFT";
        case FACE_RIGHT:      return "RIGHT";
        case FACE_UP_LEFT:    return "UP_LEFT";
        case FACE_UP_RIGHT:   return "UP_RIGHT";
        case FACE_DOWN_LEFT:  return "DOWN_LEFT";
        case FACE_DOWN_RIGHT: return "DOWN_RIGHT";
        default:              return "?";
    }
}

/*
 * WeaponLock 잠금 상태 → ASCII 문자열 변환
 */
static const char *priv_wlock_to_str(WeaponLock lock)
{
    return (lock == WLOCK_UNLOCKED) ? "[UNLOCKED]" : "[LOCKED]";
}

/*
 * 무기 이진 트리 전위순회 재귀 출력 헬퍼
 * depth  : 현재 깊이 (들여쓰기 단계)
 * is_left: 1 = 왼쪽 자식, 0 = 오른쪽 자식
 */
static void priv_weapon_print_recursive(const WeaponNode *node, FILE *out,
                                        int depth, int is_left)
{
    int i;
    if (!node || !out)
        return;

    /* 들여쓰기 및 가지 문자 출력 */
    for (i = 0; i < depth; i++) {
        if (i == depth - 1)
            fprintf(out, (is_left ? "    |-- " : "    +-- "));
        else
            fprintf(out, "    |   ");
    }

    /* 노드 정보 출력 (ASCII 전용) */
    fprintf(out, "%s  %s  (DMG:%d / SPD:%d / RATE:%dms / CNT:%d)\n",
            node->name,
            priv_wlock_to_str(node->lock),
            node->stat.damage,
            node->stat.proj_speed,
            node->stat.fire_rate_ms,
            node->stat.proj_count);

    /* 왼쪽 → 오른쪽 순서로 재귀 */
    priv_weapon_print_recursive(node->left,  out, depth + 1, 1);
    priv_weapon_print_recursive(node->right, out, depth + 1, 0);
}

/*
 * 스냅샷 텍스트를 FILE* 하나에 기록하는 내부 공통 함수
 * export_snapshot() 에서 stdout / 파일에 각각 호출한다.
 */
static ExportResult priv_write_snapshot(const GameSnapshot *snap, FILE *out)
{
    ExportResult res = EXPORT_OK;

    if (!snap || !out)
        return EXPORT_ERR_NULL;

    export_print_separator(out, '=', 64);
    fprintf(out, "  DeepRoot -- GameSnapshot  [ tick: %d ]\n", snap->game_tick);
    export_print_separator(out, '=', 64);

    /* ── 플레이어 요약 ── */
    if (snap->player) {
        const Player *p = snap->player;
        fprintf(out, "\n[PLAYER]\n");
        export_print_separator(out, '-', 40);
        fprintf(out, "  name     : %s  (id=%d)\n",  p->name, p->id);
        fprintf(out, "  pos      : (%.2f, %.2f)\n", p->position.x, p->position.y);
        fprintf(out, "  hp       : %d / %d\n",      p->hp, p->max_hp);
        fprintf(out, "  state    : %s\n",  priv_pstate_to_str(p->state));
        fprintf(out, "  dir      : %s\n",  priv_facedir_to_str(p->last_face_dir));
        fprintf(out, "  room_id  : %d\n",  p->current_room_id);

        if (p->equipped_weapon)
            fprintf(out, "  weapon   : %s  (DMG=%d)\n",
                    p->equipped_weapon->name,
                    p->equipped_weapon->stat.damage);
        else
            fprintf(out, "  weapon   : none\n");
    }

    /* ── 던전 ── */
    if (snap->dungeon) {
        fprintf(out, "\n");
        res = export_dungeon_adj_list(snap->dungeon, out);
        if (res != EXPORT_OK) return res;
    }

    /* ── 무기 트리 ── */
    if (snap->weapon_tree) {
        fprintf(out, "\n");
        res = export_weapon_tree_ascii(snap->weapon_tree, out);
        if (res != EXPORT_OK) return res;
    }

    /* ── 몬스터 큐 / 힙 ── */
    if (snap->monster_mgr) {
        fprintf(out, "\n");
        res = export_monster_queue(&snap->monster_mgr->spawn_queue,
                                   snap->monster_mgr, out);
        if (res != EXPORT_OK) return res;

        fprintf(out, "\n");
        res = export_monster_heap(&snap->monster_mgr->action_heap,
                                  snap->monster_mgr, out);
        if (res != EXPORT_OK) return res;
    }

    export_print_separator(out, '=', 64);
    return EXPORT_OK;
}

/* =========================================================================
 * 섹션 2 : 유틸리티 함수
 * ========================================================================= */

/*
 * export_open_file
 * filepath / mode NULL 체크 후 fopen 을 감싸 반환한다.
 */
FILE *export_open_file(const char *filepath, const char *mode)
{
    if (!filepath || !mode)
        return NULL;
    return fopen(filepath, mode);
}

/*
 * export_close_file
 * stdout / stderr 는 닫지 않는다.
 */
void export_close_file(FILE *fp)
{
    if (fp && fp != stdout && fp != stderr)
        fclose(fp);
}

/*
 * export_print_separator
 * ch 문자를 width 개 출력하고 개행한다.
 */
void export_print_separator(FILE *out, char ch, int width)
{
    int i;
    if (!out || width <= 0)
        return;
    for (i = 0; i < width; i++)
        fputc(ch, out);
    fputc('\n', out);
}

/*
 * export_print_indent
 * depth * EXPORT_INDENT_UNIT 개의 공백을 출력한다.
 */
void export_print_indent(FILE *out, int depth)
{
    int i;
    if (!out || depth <= 0)
        return;
    for (i = 0; i < depth * EXPORT_INDENT_UNIT; i++)
        fputc(' ', out);
}

/* =========================================================================
 * 섹션 3 : 던전 그래프 출력
 * ========================================================================= */

/*
 * export_dungeon_adj_matrix
 * 방 간 연결 여부를 2차원 행렬로 출력한다.
 * 연결 있음 = 1, 없음 = 0
 */
ExportResult export_dungeon_adj_matrix(const Dungeon *dungeon, FILE *out)
{
    int i, j;

    if (!dungeon || !out)
        return EXPORT_ERR_NULL;

    export_print_separator(out, '=', 60);
    fprintf(out, "[ Dungeon Adj-Matrix ]  rooms: %d\n", dungeon->room_count);
    export_print_separator(out, '-', 60);

    /* 열 헤더 */
    fprintf(out, "     ");
    for (i = 0; i < dungeon->room_count; i++)
        fprintf(out, " R%-2d", i);
    fprintf(out, "\n");

    for (i = 0; i < dungeon->room_count; i++) {
        fprintf(out, " R%-2d ", i);
        for (j = 0; j < dungeon->room_count; j++) {
            /* 인접 리스트를 순회하여 i -> j 연결 여부 확인 */
            int connected = 0;
            const PassageNode *p = dungeon->rooms[i].adj_list;
            while (p) {
                if (p->dest_room_id == j) { connected = 1; break; }
                p = p->next;
            }
            fprintf(out, "  %d ", connected);
        }
        fprintf(out, "\n");
    }

    export_print_separator(out, '=', 60);
    return EXPORT_OK;
}

/*
 * export_dungeon_adj_list
 * 각 방의 인접 리스트를 계층적 텍스트로 출력한다.
 */
ExportResult export_dungeon_adj_list(const Dungeon *dungeon, FILE *out)
{
    int i;

    if (!dungeon || !out)
        return EXPORT_ERR_NULL;

    export_print_separator(out, '=', 60);
    fprintf(out,
            "[ Dungeon Adj-List ]  rooms:%d  start:%d  boss:%d\n",
            dungeon->room_count,
            dungeon->start_room_id,
            dungeon->boss_room_id);
    export_print_separator(out, '-', 60);

    for (i = 0; i < dungeon->room_count; i++) {
        const Room        *room = &dungeon->rooms[i];
        const PassageNode *p    = room->adj_list;

        /* 방 헤더 출력 */
        fprintf(out, "  Room[%d] \"%s\"  type:%-6s  size:%dx%d  %s\n",
                room->id,
                room->name,
                priv_room_type_to_str(room->type),
                room->size.width,
                room->size.height,
                room->is_cleared ? "[CLEARED]" : "");

        if (!p) {
            fprintf(out, "         +-- (no passage)\n");
            continue;
        }

        /* 인접 통로 목록 출력 */
        while (p) {
            fprintf(out, "         %s Room[%d]  dir:%-1s  lock:%s\n",
                    p->next ? "|--" : "+--",
                    p->dest_room_id,
                    priv_dir_to_str(p->direction),
                    priv_lock_to_str(p->lock));
            p = p->next;
        }
    }

    export_print_separator(out, '=', 60);
    return EXPORT_OK;
}

/*
 * export_dungeon_graph
 * ExportConfig 에 따라 stdout / 파일 / 양쪽에 그래프를 출력한다.
 * verbose = 1 이면 인접 행렬도 함께 출력한다.
 */
ExportResult export_dungeon_graph(const Dungeon *dungeon,
                                   const ExportConfig *cfg)
{
    ExportResult res = EXPORT_OK;
    FILE        *fp  = NULL;

    if (!dungeon || !cfg)
        return EXPORT_ERR_NULL;

    /* ── 표준 출력 ── */
    if (cfg->target == EXPORT_TO_STDOUT || cfg->target == EXPORT_TO_BOTH) {
        res = export_dungeon_adj_list(dungeon, stdout);
        if (res != EXPORT_OK) return res;
        if (cfg->verbose) {
            res = export_dungeon_adj_matrix(dungeon, stdout);
            if (res != EXPORT_OK) return res;
        }
    }

    /* ── 파일 출력 ── */
    if (cfg->target == EXPORT_TO_FILE || cfg->target == EXPORT_TO_BOTH) {
        fp = export_open_file(cfg->filepath, "w");
        if (!fp) return EXPORT_ERR_FILE;
        res = export_dungeon_adj_list(dungeon, fp);
        if (res == EXPORT_OK && cfg->verbose)
            res = export_dungeon_adj_matrix(dungeon, fp);
        export_close_file(fp);
    }

    return res;
}

/* =========================================================================
 * 섹션 4 : 무기 이진 트리 출력
 * ========================================================================= */

/*
 * export_weapon_tree_ascii
 * 이진 트리를 전위순회하며 들여쓰기 기반 ASCII 트리로 출력한다.
 *
 * 출력 예:
 *   BasicBow  [UNLOCKED]  (DMG:10 / SPD:8 / RATE:500ms / CNT:1)
 *       |-- ShortBow  [LOCKED]  ...
 *       |       |-- FireArrow  [LOCKED]  ...
 *       |       +-- RapidBow   [LOCKED]  ...
 *       +-- Crossbow  [LOCKED]  ...
 */
ExportResult export_weapon_tree_ascii(const WeaponTree *tree, FILE *out)
{
    if (!tree || !out)
        return EXPORT_ERR_NULL;

    export_print_separator(out, '=', 64);
    fprintf(out, "[ Weapon Tree ]  nodes: %d\n", tree->node_count);
    export_print_separator(out, '-', 64);

    if (!tree->root) {
        fprintf(out, "  (empty tree)\n");
        export_print_separator(out, '=', 64);
        return EXPORT_OK;
    }

    /* 루트 노드는 들여쓰기 없이 직접 출력 */
    fprintf(out, "%s  %s  (DMG:%d / SPD:%d / RATE:%dms / CNT:%d)\n",
            tree->root->name,
            priv_wlock_to_str(tree->root->lock),
            tree->root->stat.damage,
            tree->root->stat.proj_speed,
            tree->root->stat.fire_rate_ms,
            tree->root->stat.proj_count);

    /* 자식 노드들을 재귀적으로 출력 */
    priv_weapon_print_recursive(tree->root->left,  out, 1, 1);
    priv_weapon_print_recursive(tree->root->right, out, 1, 0);

    export_print_separator(out, '=', 64);
    return EXPORT_OK;
}

/*
 * export_weapon_tree
 * ExportConfig 에 따라 stdout / 파일 / 양쪽에 트리를 출력한다.
 */
ExportResult export_weapon_tree(const WeaponTree *tree,
                                 const ExportConfig *cfg)
{
    ExportResult res = EXPORT_OK;
    FILE        *fp  = NULL;

    if (!tree || !cfg)
        return EXPORT_ERR_NULL;

    /* ── 표준 출력 ── */
    if (cfg->target == EXPORT_TO_STDOUT || cfg->target == EXPORT_TO_BOTH) {
        res = export_weapon_tree_ascii(tree, stdout);
        if (res != EXPORT_OK) return res;
    }

    /* ── 파일 출력 ── */
    if (cfg->target == EXPORT_TO_FILE || cfg->target == EXPORT_TO_BOTH) {
        fp = export_open_file(cfg->filepath, "w");
        if (!fp) return EXPORT_ERR_FILE;
        res = export_weapon_tree_ascii(tree, fp);
        export_close_file(fp);
    }

    return res;
}

/* =========================================================================
 * 섹션 5 : 몬스터 자료구조 출력
 * ========================================================================= */

/*
 * export_monster_queue
 * 원형 큐를 front -> rear 순서로 순회하며 출력한다.
 * 큐 내부 배열을 수정하지 않는다.
 */
ExportResult export_monster_queue(const MonsterQueue *q,
                                   const MonsterManager *mgr,
                                   FILE *out)
{
    int i, j;

    if (!q || !mgr || !out)
        return EXPORT_ERR_NULL;

    export_print_separator(out, '-', 48);
    fprintf(out,
            "[ Spawn Queue ]  size:%d/%d  (front=%d, rear=%d)\n",
            q->size, MONSTER_QUEUE_MAX, q->front, q->rear);

    if (q->size == 0) {
        fprintf(out, "  (empty)\n");
        export_print_separator(out, '-', 48);
        return EXPORT_OK;
    }

    for (i = 0; i < q->size; i++) {
        int            arr_idx = (q->front + i) % MONSTER_QUEUE_MAX;
        int            mon_id  = q->data[arr_idx];
        const Monster *m       = NULL;

        /* 오브젝트 풀에서 ID 탐색 */
        for (j = 0; j < mgr->pool_count; j++) {
            if (mgr->pool[j].id == mon_id) { m = &mgr->pool[j]; break; }
        }

        if (m)
            fprintf(out, "  [%2d] id=%-3d  %-16s  hp=%3d/%-3d  %s\n",
                    i, mon_id, m->name,
                    m->hp, m->max_hp,
                    priv_mstate_to_str(m->state));
        else
            /* 풀에서 찾지 못한 경우 ID 만 표시 */
            fprintf(out, "  [%2d] id=%-3d  (not found in pool)\n", i, mon_id);
    }

    export_print_separator(out, '-', 48);
    return EXPORT_OK;
}

/*
 * export_monster_heap
 * 최소 힙 배열을 인덱스 순서로 출력한다.
 * 각 노드의 힙 깊이를 계산하여 들여쓰기로 계층 구조를 시각화한다.
 * (인덱스 i 의 부모 = (i-1)/2, 깊이 = floor(log2(i+1)))
 */
ExportResult export_monster_heap(const MonsterHeap *heap,
                                  const MonsterManager *mgr,
                                  FILE *out)
{
    int i;

    if (!heap || !mgr || !out)
        return EXPORT_ERR_NULL;

    export_print_separator(out, '-', 48);
    fprintf(out,
            "[ Action Heap (min) ]  size:%d/%d\n",
            heap->size, MONSTER_HEAP_MAX);

    if (heap->size == 0) {
        fprintf(out, "  (empty)\n");
        export_print_separator(out, '-', 48);
        return EXPORT_OK;
    }

    for (i = 0; i < heap->size; i++) {
        const HeapNode *hn    = &heap->data[i];
        int             depth = 0;
        int             pos   = i + 1;  /* 1-based 인덱스 */

        /* depth = floor(log2(i+1)) 을 비트 시프트로 계산 */
        while (pos > 1) { pos >>= 1; depth++; }

        export_print_indent(out, depth);
        fprintf(out, "[%2d] monster_id=%-3d  action=%-6s  priority=%d\n",
                i,
                hn->monster_id,
                priv_action_to_str(hn->action),
                hn->priority);
    }

    export_print_separator(out, '-', 48);
    return EXPORT_OK;
}

/* =========================================================================
 * 섹션 6 : 플레이어 상태 출력
 * ========================================================================= */

/*
 * export_player_status
 * ExportConfig 에 따라 플레이어 상태를 상세 출력한다.
 * verbose = 1 이면 활성 투사체 목록도 함께 출력한다.
 */
ExportResult export_player_status(const Player *player,
                                   const ExportConfig *cfg)
{
    int          t, i;
    int          target_count = 0;
    FILE        *targets[2]   = { NULL, NULL };
    FILE        *fp            = NULL;
    ExportResult res           = EXPORT_OK;

    if (!player || !cfg)
        return EXPORT_ERR_NULL;

    /* ── 출력 대상 FILE* 배열 구성 ── */
    if (cfg->target == EXPORT_TO_STDOUT || cfg->target == EXPORT_TO_BOTH)
        targets[target_count++] = stdout;

    if (cfg->target == EXPORT_TO_FILE || cfg->target == EXPORT_TO_BOTH) {
        fp = export_open_file(cfg->filepath, "w");
        if (!fp) return EXPORT_ERR_FILE;
        targets[target_count++] = fp;
    }

    /* ── 각 출력 대상에 동일한 내용 기록 ── */
    for (t = 0; t < target_count; t++) {
        FILE *out = targets[t];

        export_print_separator(out, '=', 52);
        fprintf(out, "[ Player Status ]\n");
        export_print_separator(out, '-', 52);

        fprintf(out, "  name         : %s  (id=%d)\n",
                player->name, player->id);
        fprintf(out, "  position     : (%.2f, %.2f)\n",
                player->position.x, player->position.y);
        fprintf(out, "  hp           : %d / %d\n",
                player->hp, player->max_hp);
        fprintf(out, "  state        : %s\n",
                priv_pstate_to_str(player->state));
        fprintf(out, "  face_dir     : %s  (0x%02X)\n",
                priv_facedir_to_str(player->face_dir),
                (unsigned int)player->face_dir);
        fprintf(out, "  last_dir     : %s  (0x%02X)\n",
                priv_facedir_to_str(player->last_face_dir),
                (unsigned int)player->last_face_dir);
        fprintf(out, "  room_id      : %d\n",
                player->current_room_id);
        fprintf(out, "  move_speed   : %d\n",
                player->move_speed);
        fprintf(out, "  inv_timer    : %d ms\n",
                player->invincible_timer);
        fprintf(out, "  camera       : pos=(%.2f,%.2f)  %dx%d  zoom=%.2f\n",
                player->camera.position.x, player->camera.position.y,
                player->camera.view_width, player->camera.view_height,
                player->camera.zoom);

        /* ── 장착 무기 ── */
        if (player->equipped_weapon) {
            const WeaponNode *w = player->equipped_weapon;
            fprintf(out, "  weapon       : %s  %s\n",
                    w->name, priv_wlock_to_str(w->lock));
            fprintf(out, "    dmg=%d  spd=%d  rate=%dms  cnt=%d  spread=%.1f\n",
                    w->stat.damage, w->stat.proj_speed,
                    w->stat.fire_rate_ms, w->stat.proj_count,
                    w->stat.spread_angle);
        } else {
            fprintf(out, "  weapon       : none\n");
        }

        /* ── 활성 투사체 목록 (verbose 모드) ── */
        if (cfg->verbose) {
            int active_count = 0;
            fprintf(out, "  proj_pool    : (max %d)\n", PLAYER_MAX_PROJ);
            for (i = 0; i < PLAYER_MAX_PROJ; i++) {
                const Projectile *pr = &player->proj_pool[i];
                if (pr->is_active) {
                    fprintf(out,
                            "    [%2d] pos=(%.2f,%.2f)"
                            "  vel=(%.3f,%.3f)"
                            "  dmg=%d\n",
                            i,
                            pr->position.x, pr->position.y,
                            pr->velocity.x, pr->velocity.y,
                            pr->damage);
                    active_count++;
                }
            }
            if (active_count == 0)
                fprintf(out, "    (no active projectile)\n");
        }

        export_print_separator(out, '=', 52);
    }

    if (fp) export_close_file(fp);
    return res;
}

/* =========================================================================
 * 섹션 7 : 스냅샷 출력
 * ========================================================================= */

/*
 * export_snapshot
 * 게임 전체 상태를 ExportConfig 에 따라 직렬화하여 출력한다.
 * 각 서브모듈(던전, 무기, 몬스터)을 순서대로 기록한다.
 */
ExportResult export_snapshot(const GameSnapshot *snap,
                              const ExportConfig *cfg)
{
    ExportResult res = EXPORT_OK;
    FILE        *fp  = NULL;

    if (!snap || !cfg)
        return EXPORT_ERR_NULL;

    /* ── 표준 출력 ── */
    if (cfg->target == EXPORT_TO_STDOUT || cfg->target == EXPORT_TO_BOTH) {
        res = priv_write_snapshot(snap, stdout);
        if (res != EXPORT_OK) return res;
    }

    /* ── 파일 출력 ── */
    if (cfg->target == EXPORT_TO_FILE || cfg->target == EXPORT_TO_BOTH) {
        fp = export_open_file(cfg->filepath, "w");
        if (!fp) return EXPORT_ERR_FILE;
        res = priv_write_snapshot(snap, fp);
        export_close_file(fp);
    }

    return res;
}

/*
 * export_snapshot_csv
 * 게임 상태를 섹션 헤더(#SECTION) 구분 CSV 형식으로 파일에 저장한다.
 *
 * 파일 구조:
 *   #PLAYER       : 플레이어 기본 정보 1행
 *   #PROJECTILES  : 활성 투사체 목록
 *   #ROOMS        : 던전 방 목록
 *   #PASSAGES     : 통로(간선) 목록
 *   #WEAPONS      : 무기 트리 (전위순회, 스택 기반)
 *   #MONSTERS     : 몬스터 오브젝트 풀 전체
 *   #HEAP         : 행동 우선순위 힙
 *   #QUEUE        : 스폰 대기 큐
 */
ExportResult export_snapshot_csv(const GameSnapshot *snap,
                                  const char *filepath)
{
    FILE *fp = NULL;
    int   i;

    if (!snap || !filepath)
        return EXPORT_ERR_NULL;

    fp = export_open_file(filepath, "w");
    if (!fp) return EXPORT_ERR_FILE;

    /* ── #PLAYER ── */
    fprintf(fp, "#PLAYER\n");
    fprintf(fp, "name,id,pos_x,pos_y,hp,max_hp,state,"
                "face_dir,last_dir,room_id,move_speed,"
                "inv_timer,cam_x,cam_y,equipped_weapon,tick\n");
    if (snap->player) {
        const Player *p = snap->player;
        fprintf(fp, "%s,%d,%.4f,%.4f,%d,%d,%d,%d,%d,%d,%d,%d,"
                    "%.4f,%.4f,%s,%d\n",
                p->name, p->id,
                p->position.x,  p->position.y,
                p->hp, p->max_hp,
                (int)p->state,
                (int)p->face_dir,
                (int)p->last_face_dir,
                p->current_room_id,
                p->move_speed,
                p->invincible_timer,
                p->camera.position.x, p->camera.position.y,
                p->equipped_weapon ? p->equipped_weapon->name : "none",
                snap->game_tick);
    }

    /* ── #PROJECTILES ── */
    fprintf(fp, "#PROJECTILES\n");
    fprintf(fp, "index,pos_x,pos_y,vel_x,vel_y,damage,owner_id\n");
    if (snap->player) {
        for (i = 0; i < PLAYER_MAX_PROJ; i++) {
            const Projectile *pr = &snap->player->proj_pool[i];
            if (pr->is_active)
                fprintf(fp, "%d,%.4f,%.4f,%.4f,%.4f,%d,%d\n",
                        i,
                        pr->position.x, pr->position.y,
                        pr->velocity.x, pr->velocity.y,
                        pr->damage, pr->owner_id);
        }
    }

    /* ── #ROOMS ── */
    fprintf(fp, "#ROOMS\n");
    fprintf(fp, "id,name,type,width,height,cleared\n");
    if (snap->dungeon) {
        for (i = 0; i < snap->dungeon->room_count; i++) {
            const Room *r = &snap->dungeon->rooms[i];
            fprintf(fp, "%d,%s,%d,%d,%d,%d\n",
                    r->id, r->name, (int)r->type,
                    r->size.width, r->size.height, r->is_cleared);
        }
    }

    /* ── #PASSAGES ── */
    fprintf(fp, "#PASSAGES\n");
    fprintf(fp, "src_id,dest_id,direction,lock\n");
    if (snap->dungeon) {
        for (i = 0; i < snap->dungeon->room_count; i++) {
            const PassageNode *p = snap->dungeon->rooms[i].adj_list;
            while (p) {
                fprintf(fp, "%d,%d,%d,%d\n",
                        i, p->dest_room_id,
                        (int)p->direction, (int)p->lock);
                p = p->next;
            }
        }
    }

    /* ── #WEAPONS (전위순회, 고정 크기 스택 사용) ── */
    fprintf(fp, "#WEAPONS\n");
    fprintf(fp, "weapon_id,name,desc,damage,proj_speed,fire_rate_ms,"
                "proj_count,spread_angle,effect,effect_value,lock\n");
    if (snap->weapon_tree && snap->weapon_tree->root) {
        /* C99: VLA 대신 WEAPON_ID_COUNT 크기 고정 스택 */
        WeaponNode *stack[WEAPON_ID_COUNT];
        int top = 0;
        stack[top++] = snap->weapon_tree->root;
        while (top > 0) {
            WeaponNode *node = stack[--top];
            if (!node) continue;
            fprintf(fp, "%d,%s,%s,%d,%d,%d,%d,%.2f,%d,%d,%d\n",
                    (int)node->weapon_id,
                    node->name, node->desc,
                    node->stat.damage,
                    node->stat.proj_speed,
                    node->stat.fire_rate_ms,
                    node->stat.proj_count,
                    node->stat.spread_angle,
                    (int)node->stat.effect,
                    node->stat.effect_value,
                    (int)node->lock);
            /* 오른쪽 먼저 push -> 왼쪽이 먼저 pop (전위순회 보장) */
            if (node->right) stack[top++] = node->right;
            if (node->left)  stack[top++] = node->left;
        }
    }

    /* ── #MONSTERS ── */
    fprintf(fp, "#MONSTERS\n");
    fprintf(fp, "id,name,type,state,pos_x,pos_y,vel_x,vel_y,"
                "hp,max_hp,atk,atk_range,detect_range,room_id,active\n");
    if (snap->monster_mgr) {
        for (i = 0; i < snap->monster_mgr->pool_count; i++) {
            const Monster *m = &snap->monster_mgr->pool[i];
            fprintf(fp, "%d,%s,%d,%d,%.4f,%.4f,%.4f,%.4f,"
                        "%d,%d,%d,%d,%d,%d,%d\n",
                    m->id, m->name,
                    (int)m->type, (int)m->state,
                    m->position.x, m->position.y,
                    m->velocity.x, m->velocity.y,
                    m->hp, m->max_hp,
                    m->attack_power, m->attack_range,
                    m->detect_range, m->room_id, m->is_active);
        }
    }

    /* ── #HEAP ── */
    fprintf(fp, "#HEAP\n");
    fprintf(fp, "heap_index,monster_id,action,priority\n");
    if (snap->monster_mgr) {
        const MonsterHeap *h = &snap->monster_mgr->action_heap;
        for (i = 0; i < h->size; i++)
            fprintf(fp, "%d,%d,%d,%d\n",
                    i, h->data[i].monster_id,
                    (int)h->data[i].action, h->data[i].priority);
    }

    /* ── #QUEUE ── */
    fprintf(fp, "#QUEUE\n");
    fprintf(fp, "queue_index,monster_id\n");
    if (snap->monster_mgr) {
        const MonsterQueue *q = &snap->monster_mgr->spawn_queue;
        for (i = 0; i < q->size; i++) {
            int arr_idx = (q->front + i) % MONSTER_QUEUE_MAX;
            fprintf(fp, "%d,%d\n", i, q->data[arr_idx]);
        }
    }

    export_close_file(fp);
    return EXPORT_OK;
}

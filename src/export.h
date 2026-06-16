#ifndef EXPORT_H
#define EXPORT_H

/*
 * =============================================================================
 * Project : DeepRoot
 * File    : export.h
 * Desc    : 게임 데이터 내보내기 / 저장 / 출력 모듈
 *           - 게임 상태 직렬화 (텍스트 / CSV)
 *           - 자료구조 시각화 출력 (디버그 / 보고서용)
 *           - 그래프, 트리, 힙 구조 텍스트 렌더링
 * =============================================================================
 */

#include <stdio.h>
#include "dungeon.h"
#include "player.h"
#include "monster.h"
#include "weapon.h"

/* ─────────────────────────── 상수 / 매크로 ────────────────────────────── */

#define EXPORT_PATH_LEN      256    /* 파일 경로 최대 길이                 */
#define EXPORT_INDENT_UNIT   4      /* 들여쓰기 단위 (공백 수)             */

/* ─────────────────────────── 열거형 ───────────────────────────────────── */

/* 내보내기 결과 코드 */
typedef enum ExportResult {
    EXPORT_OK        =  0,   /* 성공                                      */
    EXPORT_ERR_NULL  = -1,   /* NULL 포인터 전달                          */
    EXPORT_ERR_FILE  = -2,   /* 파일 열기 실패                            */
    EXPORT_ERR_WRITE = -3,   /* 쓰기 실패                                 */
    EXPORT_ERR_FMT   = -4    /* 포맷 오류                                 */
} ExportResult;

/* 출력 대상 */
typedef enum ExportTarget {
    EXPORT_TO_STDOUT = 0,   /* 표준 출력                                  */
    EXPORT_TO_FILE   = 1,   /* 파일 저장                                  */
    EXPORT_TO_BOTH   = 2    /* 둘 다                                      */
} ExportTarget;

/* ─────────────────────────── 구조체 ───────────────────────────────────── */

/* 내보내기 설정 */
typedef struct ExportConfig {
    ExportTarget target;              /* 출력 대상                        */
    char         filepath[EXPORT_PATH_LEN]; /* 파일 경로 (파일 출력 시) */
    int          include_stats;       /* 스탯 포함 여부 (1 = 포함)        */
    int          include_dead;        /* 사망 개체 포함 여부              */
    int          verbose;             /* 상세 출력 여부                   */
} ExportConfig;

/* 게임 전체 스냅샷 (한 번에 직렬화할 때 사용) */
typedef struct GameSnapshot {
    const Dungeon         *dungeon;
    const Player          *player;
    const MonsterManager  *monster_mgr;
    const WeaponTree      *weapon_tree;
    int                    game_tick;    /* 현재 게임 틱                  */
} GameSnapshot;

/* ─────────────────────────── 함수 원형 ────────────────────────────────── */

/*
 * ── 던전 그래프 출력 ──
 */
ExportResult export_dungeon_graph(const Dungeon *dungeon,
                                   const ExportConfig *cfg);
ExportResult export_dungeon_adj_matrix(const Dungeon *dungeon,
                                        FILE *out);      /* 인접 행렬 출력 */
ExportResult export_dungeon_adj_list(const Dungeon *dungeon,
                                      FILE *out);        /* 인접 리스트 출력 */

/*
 * ── 무기 트리 출력 ──
 */
ExportResult export_weapon_tree(const WeaponTree *tree,
                                 const ExportConfig *cfg);
ExportResult export_weapon_tree_ascii(const WeaponTree *tree,
                                       FILE *out);       /* ASCII 트리 그리기 */

/*
 * ── 몬스터 큐 / 힙 출력 ──
 */
ExportResult export_monster_queue(const MonsterQueue *q,
                                   const MonsterManager *mgr,
                                   FILE *out);
ExportResult export_monster_heap(const MonsterHeap *heap,
                                  const MonsterManager *mgr,
                                  FILE *out);

/*
 * ── 플레이어 상태 출력 ──
 */
ExportResult export_player_status(const Player *player,
                                   const ExportConfig *cfg);

/*
 * ── 스냅샷 / CSV ──
 */
ExportResult export_snapshot(const GameSnapshot *snap,
                              const ExportConfig *cfg);
ExportResult export_snapshot_csv(const GameSnapshot *snap,
                                  const char *filepath);

/*
 * ── 유틸리티 ──
 */
FILE        *export_open_file(const char *filepath, const char *mode);
void         export_close_file(FILE *fp);
void         export_print_separator(FILE *out, char ch, int width);
void         export_print_indent(FILE *out, int depth);

#endif /* EXPORT_H */

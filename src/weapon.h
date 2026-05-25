#ifndef WEAPON_H
#define WEAPON_H

/*
 * =============================================================================
 * Project : DeepRoot
 * File    : weapon.h
 * Desc    : 무기 구조 정의 - 이진 트리 기반 무기 진화 트리
 *
 *  무기 트리 구조:
 *
 *              기본 활 (WEAPON_BOW_BASIC)
 *              ├─ 단궁 (WEAPON_SHORT_BOW)
 *              │   ├─ 불화살 (WEAPON_FIRE_ARROW)   [왼쪽 자식]
 *              │   └─ 연발   (WEAPON_RAPID_BOW)    [오른쪽 자식]
 *              └─ 석궁 (WEAPON_CROSSBOW)            [오른쪽 자식]
 *
 *  이진 트리 규칙:
 *   - left  child  : 해당 무기의 첫 번째 업그레이드/파생 경로
 *   - right child  : 두 번째 업그레이드/파생 경로
 *   - 루트          : 기본 활
 * =============================================================================
 */

/* ─────────────────────────── 상수 / 매크로 ────────────────────────────── */

#define WEAPON_NAME_LEN   24    /* 무기 이름 최대 길이                     */
#define WEAPON_DESC_LEN   64    /* 무기 설명 최대 길이                     */

/* ─────────────────────────── 열거형 ───────────────────────────────────── */

/* 무기 고유 종류 ID */
typedef enum WeaponID {
    WEAPON_NONE        = 0,   /* 무기 없음 (더미)                         */
    WEAPON_BOW_BASIC   = 1,   /* 기본 활 (루트)                           */
    WEAPON_SHORT_BOW   = 2,   /* 단궁 (기본 활 → 왼쪽)                   */
    WEAPON_CROSSBOW    = 3,   /* 석궁 (기본 활 → 오른쪽)                 */
    WEAPON_FIRE_ARROW  = 4,   /* 불화살 (단궁 → 왼쪽)                    */
    WEAPON_RAPID_BOW   = 5,   /* 연발 (단궁 → 오른쪽)                    */
    WEAPON_ID_COUNT    = 6    /* 무기 종류 수 (배열 크기 용도)            */
} WeaponID;

/* 투사체 특성 */
typedef enum ProjEffect {
    PROJ_NORMAL  = 0,   /* 일반 투사체                                   */
    PROJ_FIRE    = 1,   /* 화염 (지속 데미지)                            */
    PROJ_MULTI   = 2,   /* 다중 발사 (연발)                              */
    PROJ_PIERCE  = 3    /* 관통 (확장용)                                 */
} ProjEffect;

/* 잠금 상태 (해금 여부) */
typedef enum WeaponLock {
    WLOCK_LOCKED   = 0,   /* 아직 잠김                                   */
    WLOCK_UNLOCKED = 1    /* 해금됨 (장착 가능)                          */
} WeaponLock;

/* ─────────────────────────── 구조체 ───────────────────────────────────── */

/* 무기 스탯 */
typedef struct WeaponStat {
    int        damage;         /* 기본 데미지                             */
    int        proj_speed;     /* 투사체 속력                             */
    int        fire_rate_ms;   /* 발사 간격 (밀리초)                      */
    int        proj_count;     /* 1회 발사 시 투사체 수 (연발용)          */
    float      spread_angle;   /* 발사 퍼짐 각도 (도, 단발=0.0)          */
    ProjEffect effect;         /* 투사체 특수 효과                        */
    int        effect_value;   /* 효과 수치 (화염 지속 데미지 등)         */
} WeaponStat;

/* 이진 트리 노드 (무기 하나) */
typedef struct WeaponNode {
    WeaponID         weapon_id;              /* 무기 종류 식별자          */
    char             name[WEAPON_NAME_LEN];  /* 무기 이름                 */
    char             desc[WEAPON_DESC_LEN];  /* 무기 설명                 */
    WeaponStat       stat;                   /* 무기 스탯                 */
    WeaponLock       lock;                   /* 잠금 상태                 */

    struct WeaponNode *left;   /* 왼쪽 자식 (첫 번째 파생 경로)          */
    struct WeaponNode *right;  /* 오른쪽 자식 (두 번째 파생 경로)        */
    struct WeaponNode *parent; /* 부모 노드 (트리 역방향 탐색용)         */
} WeaponNode;

/* 무기 이진 트리 전체 */
typedef struct WeaponTree {
    WeaponNode *root;          /* 트리 루트 (기본 활)                    */
    int         node_count;    /* 총 노드 수                             */
} WeaponTree;

/* ─────────────────────────── 함수 원형 ────────────────────────────────── */

/*
 * 트리 초기화 / 해제
 */
WeaponTree  *weapon_tree_create(void);        /* 기본 트리 구조 생성      */
void         weapon_tree_destroy(WeaponTree *tree);

/*
 * 노드 삽입 / 제거
 */
WeaponNode  *weapon_node_create(WeaponID id, const char *name,
                                 const char *desc, WeaponStat stat);
int          weapon_tree_insert(WeaponTree *tree,
                                WeaponNode *parent, WeaponNode *child,
                                int is_left_child);
void         weapon_node_destroy(WeaponNode *node);

/*
 * 탐색
 */
WeaponNode  *weapon_tree_find(const WeaponTree *tree, WeaponID id);
WeaponNode  *weapon_tree_get_root(const WeaponTree *tree);
WeaponNode  *weapon_get_parent(const WeaponNode *node);
WeaponNode  *weapon_get_left(const WeaponNode *node);
WeaponNode  *weapon_get_right(const WeaponNode *node);

/*
 * 해금 / 장착
 */
int          weapon_unlock(WeaponNode *node);
int          weapon_is_unlocked(const WeaponNode *node);
int          weapon_can_upgrade(const WeaponNode *node);   /* 자식 존재 여부 */

/*
 * 순회 (트리 전체 처리 시 사용)
 */
void         weapon_tree_preorder(const WeaponTree *tree,
                                  void (*visit)(WeaponNode *));
void         weapon_tree_inorder(const WeaponTree *tree,
                                 void (*visit)(WeaponNode *));
void         weapon_tree_postorder(const WeaponTree *tree,
                                   void (*visit)(WeaponNode *));

/*
 * 유틸리티
 */
int          weapon_tree_height(const WeaponTree *tree);
void         weapon_tree_print(const WeaponTree *tree);   /* 디버그용 출력 */

#endif /* WEAPON_H */

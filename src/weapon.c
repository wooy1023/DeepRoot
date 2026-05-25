#define _CRT_SECURE_NO_WARNINGS

/*
 * =============================================================================
 * Project : DeepRoot
 * File    : weapon.c
 * Desc    : 무기 이진 트리 구현
 *           - 무기 노드 생성 / 삽입 / 탐색 / 해제
 *           - 기본 트리 구조 (기본 활 → 단궁/석궁 → 불화살/연발)
 *           - tier 기반 업그레이드 구조
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "weapon.h"

/* ─────────────────────────── 내부 헬퍼 함수 선언 ──────────────────────── */

static void        preorder_recursive(WeaponNode *node, void (*visit)(WeaponNode *));
static void        inorder_recursive(WeaponNode *node, void (*visit)(WeaponNode *));
static void        postorder_recursive(WeaponNode *node, void (*visit)(WeaponNode *));
static int         height_recursive(const WeaponNode *node);
static WeaponNode *find_recursive(WeaponNode *node, WeaponID id);
static void        destroy_recursive(WeaponNode *node);
static void        print_recursive(const WeaponNode *node, int depth, int is_right);
static WeaponStat  make_stat(int damage, int proj_speed, int fire_rate_ms,
                              int proj_count, float spread_angle,
                              ProjEffect effect, int effect_value);


/* ─────────────────────────── 트리 초기화 / 해제 ───────────────────────── */

/*
 * weapon_tree_create
 * - 기본 무기 트리를 생성하고 초기화한다.
 * - 루트(기본 활)부터 단궁/석궁/불화살/연발을 순서대로 삽입한다.
 * - 기본 활(루트)만 잠금 해제 상태로 생성한다.
 * - 반환: 성공 시 WeaponTree*, 실패 시 NULL
 */
WeaponTree *weapon_tree_create(void)
{
    WeaponTree *tree      = NULL;
    WeaponNode *bow_basic  = NULL;
    WeaponNode *short_bow  = NULL;
    WeaponNode *crossbow   = NULL;
    WeaponNode *fire_arrow = NULL;
    WeaponNode *rapid_bow  = NULL;

    /* 트리 구조체 할당 */
    tree = (WeaponTree *)malloc(sizeof(WeaponTree));
    if (tree == NULL)
        return NULL;

    tree->root       = NULL;
    tree->node_count = 0;

    /* ── 기본 활 (루트) ─────────────────────────────────────────────────── */
    bow_basic = weapon_node_create(
        WEAPON_BOW_BASIC, "Basic Bow", "",
        make_stat(10, 8, 1200, 1, 0.0f, PROJ_NORMAL, 0)
    );
    if (bow_basic == NULL)
        goto cleanup;

    /* 루트 직접 등록 (부모 없음) */
    bow_basic->lock  = WLOCK_UNLOCKED;  /* 시작부터 해금 */
    tree->root       = bow_basic;
    tree->node_count = 1;

    /* ── 단궁 (기본 활 → 왼쪽) ─────────────────────────────────────────── */
    short_bow = weapon_node_create(
        WEAPON_SHORT_BOW, "Short Bow", "",
        make_stat(22, 10, 600, 1, 0.0f, PROJ_NORMAL, 0)
    );
    if (short_bow == NULL)
        goto cleanup;

    if (weapon_tree_insert(tree, bow_basic, short_bow, 1) != 0)
        goto cleanup;

    /* ── 석궁 (기본 활 → 오른쪽) ───────────────────────────────────────── */
    crossbow = weapon_node_create(
        WEAPON_CROSSBOW, "Cross Bow", "",
        make_stat(42, 12, 1800, 1, 0.0f, PROJ_PIERCE, 0)
    );
    if (crossbow == NULL)
        goto cleanup;

    if (weapon_tree_insert(tree, bow_basic, crossbow, 0) != 0)
        goto cleanup;

    /* ── 불화살 (단궁 → 왼쪽) ──────────────────────────────────────────── */
    fire_arrow = weapon_node_create(
        WEAPON_FIRE_ARROW, "Fire Bow", "",
        make_stat(35, 9, 650, 1, 0.0f, PROJ_FIRE, 3)
    );
    if (fire_arrow == NULL)
        goto cleanup;

    if (weapon_tree_insert(tree, short_bow, fire_arrow, 1) != 0)
        goto cleanup;

    /* ── 연발 (단궁 → 오른쪽) ──────────────────────────────────────────── */
    rapid_bow = weapon_node_create(
        WEAPON_RAPID_BOW, "Rapid Bow", "",
        make_stat(20, 10, 700, 3, 15.0f, PROJ_MULTI, 0)
    );
    if (rapid_bow == NULL)
        goto cleanup;

    if (weapon_tree_insert(tree, short_bow, rapid_bow, 0) != 0)
        goto cleanup;

    return tree;

cleanup:
    /* 할당 도중 실패 시 지금까지 만들어진 트리 전체 해제 */
    weapon_tree_destroy(tree);
    return NULL;
}

/*
 * weapon_tree_destroy
 * - 트리의 모든 노드 메모리를 재귀적으로 해제한다.
 * - tree 자체도 해제한다.
 */
void weapon_tree_destroy(WeaponTree *tree)
{
    if (tree == NULL)
        return;

    /* 루트부터 후위 순서로 모든 노드 해제 */
    destroy_recursive(tree->root);

    tree->root       = NULL;
    tree->node_count = 0;

    free(tree);
}


/* ─────────────────────────── 노드 삽입 / 제거 ─────────────────────────── */

/*
 * weapon_node_create
 * - 새로운 무기 노드를 힙에 할당하고 초기화한다.
 * - 생성 직후 lock = WLOCK_LOCKED, left/right/parent = NULL.
 * - 반환: 성공 시 WeaponNode*, 실패 시 NULL
 */
WeaponNode *weapon_node_create(WeaponID id, const char *name,
                                const char *desc, WeaponStat stat)
{
    WeaponNode *node = NULL;

    /* NULL 입력 방어 */
    if (name == NULL || desc == NULL)
        return NULL;

    node = (WeaponNode *)malloc(sizeof(WeaponNode));
    if (node == NULL)
        return NULL;

    node->weapon_id = id;

    /* 이름 / 설명 안전 복사 (버퍼 오버플로 방지) */
    strncpy(node->name, name, WEAPON_NAME_LEN - 1);
    node->name[WEAPON_NAME_LEN - 1] = '\0';

    strncpy(node->desc, desc, WEAPON_DESC_LEN - 1);
    node->desc[WEAPON_DESC_LEN - 1] = '\0';

    node->stat   = stat;
    node->lock   = WLOCK_LOCKED;   /* 기본값: 잠김 상태 */
    node->left   = NULL;
    node->right  = NULL;
    node->parent = NULL;

    return node;
}

/*
 * weapon_tree_insert
 * - parent의 왼쪽(is_left_child=1) 또는 오른쪽(is_left_child=0) 자식으로
 *   child를 삽입한다.
 * - 이미 해당 방향에 자식이 존재하면 실패(-1)를 반환한다.
 * - 반환: 성공 0, 실패 -1
 */
int weapon_tree_insert(WeaponTree *tree,
                       WeaponNode *parent, WeaponNode *child,
                       int is_left_child)
{
    /* NULL 방어 */
    if (tree == NULL || parent == NULL || child == NULL)
        return -1;

    if (is_left_child)
    {
        /* 왼쪽 자식 슬롯이 이미 채워진 경우 삽입 불가 */
        if (parent->left != NULL)
            return -1;

        parent->left  = child;
    }
    else
    {
        /* 오른쪽 자식 슬롯이 이미 채워진 경우 삽입 불가 */
        if (parent->right != NULL)
            return -1;

        parent->right = child;
    }

    child->parent = parent;   /* 역방향 포인터 설정 */
    tree->node_count++;

    return 0;
}

/*
 * weapon_node_destroy
 * - 단일 노드만 해제한다 (자식 노드는 건드리지 않는다).
 * - 트리 전체 해제가 필요하면 weapon_tree_destroy 사용.
 */
void weapon_node_destroy(WeaponNode *node)
{
    if (node == NULL)
        return;

    free(node);
}


/* ─────────────────────────── 탐색 ─────────────────────────────────────── */

/*
 * weapon_tree_find
 * - 트리 전체에서 id와 일치하는 첫 번째 노드를 반환한다 (전위 순회).
 * - 반환: 찾으면 WeaponNode*, 없으면 NULL
 */
WeaponNode *weapon_tree_find(const WeaponTree *tree, WeaponID id)
{
    if (tree == NULL)
        return NULL;

    return find_recursive(tree->root, id);
}

/*
 * weapon_tree_get_root
 * - 트리의 루트 노드를 반환한다.
 */
WeaponNode *weapon_tree_get_root(const WeaponTree *tree)
{
    if (tree == NULL)
        return NULL;

    return tree->root;
}

/*
 * weapon_get_parent
 * - 해당 노드의 부모 노드를 반환한다.
 * - 루트이거나 node가 NULL이면 NULL 반환.
 */
WeaponNode *weapon_get_parent(const WeaponNode *node)
{
    if (node == NULL)
        return NULL;

    return node->parent;
}

/*
 * weapon_get_left
 * - 해당 노드의 왼쪽 자식을 반환한다.
 */
WeaponNode *weapon_get_left(const WeaponNode *node)
{
    if (node == NULL)
        return NULL;

    return node->left;
}

/*
 * weapon_get_right
 * - 해당 노드의 오른쪽 자식을 반환한다.
 */
WeaponNode *weapon_get_right(const WeaponNode *node)
{
    if (node == NULL)
        return NULL;

    return node->right;
}


/* ─────────────────────────── 해금 / 장착 ──────────────────────────────── */

/*
 * weapon_unlock
 * - 해당 노드의 잠금 상태를 WLOCK_UNLOCKED로 변경한다.
 * - 반환: 성공 1, 실패(NULL 또는 이미 해금) 0
 */
int weapon_unlock(WeaponNode *node)
{
    if (node == NULL)
        return 0;

    if (node->lock == WLOCK_UNLOCKED)
        return 0;   /* 이미 해금된 상태 */

    node->lock = WLOCK_UNLOCKED;
    return 1;
}

/*
 * weapon_is_unlocked
 * - 해당 노드가 해금 상태인지 확인한다.
 * - 반환: 해금이면 1, 잠금이거나 NULL이면 0
 */
int weapon_is_unlocked(const WeaponNode *node)
{
    if (node == NULL)
        return 0;

    return (node->lock == WLOCK_UNLOCKED) ? 1 : 0;
}

/*
 * weapon_can_upgrade
 * - 해당 노드에 자식이 하나라도 존재하는지 확인한다.
 * - 자식이 있으면 업그레이드 경로가 존재한다는 의미.
 * - 반환: 업그레이드 가능 1, 불가 0
 */
int weapon_can_upgrade(const WeaponNode *node)
{
    if (node == NULL)
        return 0;

    return (node->left != NULL || node->right != NULL) ? 1 : 0;
}


/* ─────────────────────────── 순회 ─────────────────────────────────────── */

/*
 * weapon_tree_preorder
 * - 전위 순회: 루트 → 왼쪽 → 오른쪽
 */
void weapon_tree_preorder(const WeaponTree *tree,
                           void (*visit)(WeaponNode *))
{
    if (tree == NULL || visit == NULL)
        return;

    preorder_recursive(tree->root, visit);
}

/*
 * weapon_tree_inorder
 * - 중위 순회: 왼쪽 → 루트 → 오른쪽
 */
void weapon_tree_inorder(const WeaponTree *tree,
                          void (*visit)(WeaponNode *))
{
    if (tree == NULL || visit == NULL)
        return;

    inorder_recursive(tree->root, visit);
}

/*
 * weapon_tree_postorder
 * - 후위 순회: 왼쪽 → 오른쪽 → 루트 (메모리 해제 순서와 동일)
 */
void weapon_tree_postorder(const WeaponTree *tree,
                            void (*visit)(WeaponNode *))
{
    if (tree == NULL || visit == NULL)
        return;

    postorder_recursive(tree->root, visit);
}


/* ─────────────────────────── 유틸리티 ─────────────────────────────────── */

/*
 * weapon_tree_height
 * - 트리의 높이(레벨 수)를 반환한다.
 * - 빈 트리이면 0, 루트만 있으면 1.
 */
int weapon_tree_height(const WeaponTree *tree)
{
    if (tree == NULL)
        return 0;

    return height_recursive(tree->root);
}

/*
 * weapon_tree_print
 * - 트리 구조를 콘솔에 시각적으로 출력한다 (디버그용).
 * - 오른쪽 자식이 위, 왼쪽 자식이 아래에 표시되는 회전된 트리 형태.
 */
void weapon_tree_print(const WeaponTree *tree)
{
    if (tree == NULL)
    {
        printf("[weapon_tree_print] tree is NULL.\n");
        return;
    }

    printf("=== 무기 트리 (노드 수: %d, 높이: %d) ===\n",
           tree->node_count, weapon_tree_height(tree));

    if (tree->root == NULL)
    {
        printf("  (empty tree)\n");
        return;
    }

    print_recursive(tree->root, 0, 0);
    printf("==========================================\n");
}


/* ─────────────────────────── 내부 헬퍼 구현 ───────────────────────────── */

/*
 * make_stat
 * - WeaponStat 구조체를 값 기반으로 초기화하여 반환한다.
 */
static WeaponStat make_stat(int damage, int proj_speed, int fire_rate_ms,
                             int proj_count, float spread_angle,
                             ProjEffect effect, int effect_value)
{
    WeaponStat s;
    s.damage       = damage;
    s.proj_speed   = proj_speed;
    s.fire_rate_ms = fire_rate_ms;
    s.proj_count   = proj_count;
    s.spread_angle = spread_angle;
    s.effect       = effect;
    s.effect_value = effect_value;
    return s;
}

/*
 * preorder_recursive
 * - 전위 순회 재귀 구현.
 */
static void preorder_recursive(WeaponNode *node, void (*visit)(WeaponNode *))
{
    if (node == NULL)
        return;

    visit(node);
    preorder_recursive(node->left,  visit);
    preorder_recursive(node->right, visit);
}

/*
 * inorder_recursive
 * - 중위 순회 재귀 구현.
 */
static void inorder_recursive(WeaponNode *node, void (*visit)(WeaponNode *))
{
    if (node == NULL)
        return;

    inorder_recursive(node->left,  visit);
    visit(node);
    inorder_recursive(node->right, visit);
}

/*
 * postorder_recursive
 * - 후위 순회 재귀 구현.
 */
static void postorder_recursive(WeaponNode *node, void (*visit)(WeaponNode *))
{
    if (node == NULL)
        return;

    postorder_recursive(node->left,  visit);
    postorder_recursive(node->right, visit);
    visit(node);
}

/*
 * height_recursive
 * - 노드를 루트로 하는 서브트리의 높이를 반환한다.
 */
static int height_recursive(const WeaponNode *node)
{
    int left_h  = 0;
    int right_h = 0;

    if (node == NULL)
        return 0;

    left_h  = height_recursive(node->left);
    right_h = height_recursive(node->right);

    return 1 + (left_h > right_h ? left_h : right_h);
}

/*
 * find_recursive
 * - 전위 순회를 이용해 id와 일치하는 노드를 탐색한다.
 */
static WeaponNode *find_recursive(WeaponNode *node, WeaponID id)
{
    WeaponNode *found = NULL;

    if (node == NULL)
        return NULL;

    /* 현재 노드 확인 */
    if (node->weapon_id == id)
        return node;

    /* 왼쪽 서브트리 탐색 */
    found = find_recursive(node->left, id);
    if (found != NULL)
        return found;

    /* 오른쪽 서브트리 탐색 */
    return find_recursive(node->right, id);
}

/*
 * destroy_recursive
 * - 후위 순회로 모든 노드를 해제한다.
 * - 자식을 먼저 해제한 뒤 부모를 해제한다.
 */
static void destroy_recursive(WeaponNode *node)
{
    if (node == NULL)
        return;

    destroy_recursive(node->left);
    destroy_recursive(node->right);

    free(node);
}

/*
 * print_recursive
 * - 트리를 들여쓰기 기반으로 콘솔 출력한다.
 * - depth  : 현재 깊이 (들여쓰기 수준)
 * - is_right: 오른쪽 자식이면 1, 왼쪽 자식 또는 루트이면 0
 */
static void print_recursive(const WeaponNode *node, int depth, int is_right)
{
    int i = 0;

    if (node == NULL)
        return;

    /* 오른쪽 자식 먼저 출력 (콘솔에서 위에 표시) */
    print_recursive(node->right, depth + 1, 1);

    /* 깊이만큼 들여쓰기 */
    for (i = 0; i < depth; i++)
        printf("    ");

    /* 분기 기호 */
    if (depth > 0)
        printf("%s", is_right ? "┌─ " : "└─ ");

    /* 노드 정보: ID / 이름 / 해금 여부 / 데미지 / 발사 간격 */
    printf("[%d] %s %s (DMG:%d / FR:%dms)\n",
           node->weapon_id,
           node->name,
           node->lock == WLOCK_UNLOCKED ? "[unlocked]" : "[locked]",
           node->stat.damage,
           node->stat.fire_rate_ms);

    /* 왼쪽 자식 출력 (콘솔에서 아래에 표시) */
    print_recursive(node->left, depth + 1, 0);
}

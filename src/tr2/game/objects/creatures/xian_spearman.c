#include "game/objects/creatures/xian_spearman.h"

#include "game/creature.h"
#include "game/items.h"
#include "game/lara/control.h"
#include "game/lot.h"
#include "game/objects/common.h"
#include "game/objects/creatures/xian_common.h"
#include "game/random.h"
#include "game/sound.h"
#include "global/const.h"
#include "global/funcs.h"
#include "global/vars.h"

#include <libtrx/debug.h>
#include <libtrx/utils.h>

// clang-format off
#define XIAN_SPEARMAN_HITPOINTS    100
#define XIAN_SPEARMAN_HIT_1_DAMAGE 75
#define XIAN_SPEARMAN_HIT_2_DAMAGE 75
#define XIAN_SPEARMAN_HIT_5_DAMAGE 75
#define XIAN_SPEARMAN_HIT_6_DAMAGE 120
#define XIAN_SPEARMAN_RADIUS       (WALL_L / 5) // = 204
#define XIAN_SPEARMAN_TOUCH_L_BITS 0b00000000'00001000'00000000 // = 0x00800
#define XIAN_SPEARMAN_TOUCH_R_BITS 0b00000100'00000000'00000000 // = 0x40000
#define XIAN_WALK_TURN             (PHD_DEGREE * 3) // = 546
#define XIAN_RUN_TURN              (PHD_DEGREE * 5) // = 910
#define XIAN_ATTACK_1_RANGE        SQUARE(WALL_L) // = 1048576
#define XIAN_ATTACK_2_RANGE        SQUARE(WALL_L * 3 / 2) // = 2359296
#define XIAN_ATTACK_3_RANGE        SQUARE(WALL_L * 2) // = 4194304
#define XIAN_ATTACK_4_RANGE        SQUARE(WALL_L * 2) // = 4194304
#define XIAN_ATTACK_5_RANGE        SQUARE(WALL_L) // = 1048576
#define XIAN_ATTACK_6_RANGE        SQUARE(WALL_L * 2) // = 4194304
#define XIAN_RUN_RANGE             SQUARE(WALL_L * 3) // = 9437184
#define XIAN_STOP_CHANCE           0x200
#define XIAN_WALK_CHANCE           (XIAN_STOP_CHANCE + 0x200) // = 0x400
// clang-format on

typedef enum {
    // clang-format off
    XIAN_SPEARMAN_STATE_EMPTY  = 0,
    XIAN_SPEARMAN_STATE_STOP   = 1,
    XIAN_SPEARMAN_STATE_STOP_2 = 2,
    XIAN_SPEARMAN_STATE_WALK   = 3,
    XIAN_SPEARMAN_STATE_RUN    = 4,
    XIAN_SPEARMAN_STATE_AIM_1  = 5,
    XIAN_SPEARMAN_STATE_HIT_1  = 6,
    XIAN_SPEARMAN_STATE_AIM_2  = 7,
    XIAN_SPEARMAN_STATE_HIT_2  = 8,
    XIAN_SPEARMAN_STATE_AIM_3  = 9,
    XIAN_SPEARMAN_STATE_HIT_3  = 10,
    XIAN_SPEARMAN_STATE_AIM_4  = 11,
    XIAN_SPEARMAN_STATE_HIT_4  = 12,
    XIAN_SPEARMAN_STATE_AIM_5  = 13,
    XIAN_SPEARMAN_STATE_HIT_5  = 14,
    XIAN_SPEARMAN_STATE_AIM_6  = 15,
    XIAN_SPEARMAN_STATE_HIT_6  = 16,
    XIAN_SPEARMAN_STATE_DEATH  = 17,
    XIAN_SPEARMAN_STATE_START  = 18,
    XIAN_SPEARMAN_STATE_KILL   = 19,
    // clang-format on
} XIAN_SPEARMAN_STATE;

typedef enum {
    // clang-format off
    XIAN_SPEARMAN_ANIM_DEATH = 0,
    XIAN_SPEARMAN_ANIM_START = 48,
    XIAN_SPEARMAN_ANIM_KILL  = 49,
    // clang-format on
} XIAN_SPEARMAN_ANIM;

static const BITE m_XianSpearmanLeftSpear = {
    .pos = { .x = 0, .y = 0, .z = 920 },
    .mesh_num = 11,
};

static const BITE m_XianSpearmanRightSpear = {
    .pos = { .x = 0, .y = 0, .z = 920 },
    .mesh_num = 18,
};

void XianSpearman_Setup(void)
{
    OBJECT *const obj = &g_Objects[O_XIAN_SPEARMAN];
    if (!obj->loaded) {
        return;
    }

    ASSERT(g_Objects[O_XIAN_SPEARMAN_STATUE].loaded);
    obj->initialise = XianKnight_Initialise;
    obj->draw_routine = XianWarrior_Draw;
    obj->control = XianSpearman_Control;
    obj->collision = Creature_Collision;

    obj->hit_points = XIAN_SPEARMAN_HITPOINTS;
    obj->radius = XIAN_SPEARMAN_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = 0;

    obj->intelligent = 1;
    obj->save_position = 1;
    obj->save_hitpoints = 1;
    obj->save_flags = 1;
    obj->save_anim = 1;

    g_AnimBones[obj->bone_idx + 6 * 4] |= BF_ROT_Y;
    g_AnimBones[obj->bone_idx + 12 * 4] |= BF_ROT_Y;
}

void __cdecl XianSpearman_DoDamage(
    const ITEM *const item, CREATURE *const creature, const int32_t damage)
{
    if ((creature->flags & 1) == 0
        && (item->touch_bits & XIAN_SPEARMAN_TOUCH_R_BITS) != 0) {
        Lara_TakeDamage(damage, true);
        Creature_Effect(item, &m_XianSpearmanRightSpear, DoBloodSplat);
        creature->flags |= 1;
        Sound_Effect(SFX_CRUNCH_2, &item->pos, SPM_NORMAL);
    }

    if ((creature->flags & 2) == 0
        && (item->touch_bits & XIAN_SPEARMAN_TOUCH_L_BITS) != 0) {
        Lara_TakeDamage(damage, true);
        Creature_Effect(item, &m_XianSpearmanLeftSpear, DoBloodSplat);
        creature->flags |= 2;
        Sound_Effect(SFX_CRUNCH_2, &item->pos, SPM_NORMAL);
    }
}

void __cdecl XianSpearman_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->data;

    int16_t head = 0;
    int16_t neck = 0;
    int16_t angle = 0;
    const bool lara_alive = g_LaraItem->hit_points > 0;

    if (item->hit_points <= 0) {
        item->current_anim_state = XIAN_SPEARMAN_STATE_DEATH;
        item->mesh_bits >>= 1;
        if (item->mesh_bits == 0) {
            Sound_Effect(SFX_EXPLOSION_1, NULL, SPM_NORMAL);
            item->mesh_bits = -1;
            item->object_id = O_XIAN_SPEARMAN_STATUE;
            Effect_ExplodingDeath(item_num, -1, 0);
            item->object_id = O_XIAN_SPEARMAN;
            LOT_DisableBaddieAI(item_num);
            Item_Kill(item_num);
            item->status = IS_DEACTIVATED;
            item->flags |= IF_ONE_SHOT;
        }
        return;
    }

    AI_INFO info;
    Creature_AIInfo(&g_Items[item_num], &info);
    Creature_Mood(item, &info, MOOD_ATTACK);

    angle = Creature_Turn(item, creature->maximum_turn);
    if (item->current_anim_state != XIAN_SPEARMAN_STATE_START) {
        item->mesh_bits = -1;
    }

    switch (item->current_anim_state) {
    case XIAN_SPEARMAN_STATE_START:
        if (creature->flags == 0) {
            item->mesh_bits = (item->mesh_bits << 1) | 1;
            creature->flags = 3;
        } else {
            creature->flags--;
        }
        break;

    case XIAN_SPEARMAN_STATE_STOP:
        if (info.ahead != 0) {
            neck = info.angle;
        }
        creature->maximum_turn = 0;
        if (creature->mood == MOOD_BORED) {
            const int32_t random = Random_GetControl();
            if (random < XIAN_STOP_CHANCE) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP_2;
            } else if (random < XIAN_WALK_CHANCE) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
            }
        } else if (info.ahead != 0 && info.distance < XIAN_ATTACK_1_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_AIM_1;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
        }
        break;

    case XIAN_SPEARMAN_STATE_STOP_2:
        if (info.ahead != 0) {
            neck = info.angle;
        }
        creature->maximum_turn = 0;
        if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
        } else if (creature->mood == MOOD_BORED) {
            const int32_t random = Random_GetControl();
            if (random < XIAN_STOP_CHANCE) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP;
            } else if (random < XIAN_WALK_CHANCE) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
            }
        } else if (info.ahead != 0 && info.distance < XIAN_ATTACK_5_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_AIM_5;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
        }
        break;

    case XIAN_SPEARMAN_STATE_WALK:
        if (info.ahead != 0) {
            neck = info.angle;
        }
        creature->maximum_turn = XIAN_WALK_TURN;
        if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_RUN;
        } else if (creature->mood == MOOD_BORED) {
            const int32_t random = Random_GetControl();
            if (random < XIAN_STOP_CHANCE) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP;
            } else if (random < XIAN_WALK_CHANCE) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP_2;
            }
        } else if (info.ahead != 0 && info.distance < XIAN_ATTACK_4_RANGE) {
            if (info.distance < XIAN_ATTACK_2_RANGE) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_AIM_2;
            } else {
                if (Random_GetControl() < 0x4000) {
                    item->goal_anim_state = XIAN_SPEARMAN_STATE_AIM_3;
                } else {
                    item->goal_anim_state = XIAN_SPEARMAN_STATE_AIM_4;
                }
            }
        } else if (info.ahead == 0 || info.distance > XIAN_RUN_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_RUN;
        }
        break;

    case XIAN_SPEARMAN_STATE_RUN:
        if (info.ahead != 0) {
            neck = info.angle;
        }
        creature->maximum_turn = XIAN_RUN_TURN;
        if (creature->mood == MOOD_ESCAPE) {
        } else if (creature->mood == MOOD_BORED) {
            if (Random_GetControl() < 0x4000) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP;
            } else {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP_2;
            }
        } else if (info.ahead != 0 && info.distance < XIAN_ATTACK_6_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_AIM_6;
        }
        break;

    case XIAN_SPEARMAN_STATE_AIM_1:
        if (info.ahead != 0) {
            head = info.angle;
        }
        creature->flags = 0;
        if (info.ahead == 0 || info.distance > XIAN_ATTACK_1_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_HIT_1;
        }
        break;

    case XIAN_SPEARMAN_STATE_AIM_2:
        if (info.ahead != 0) {
            head = info.angle;
        }
        creature->flags = 0;
        if (info.ahead == 0 || info.distance > XIAN_ATTACK_2_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_HIT_2;
        }
        break;

    case XIAN_SPEARMAN_STATE_AIM_3:
        if (info.ahead != 0) {
            head = info.angle;
        }
        creature->flags = 0;
        if (info.ahead == 0 || info.distance > XIAN_ATTACK_3_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_HIT_2;
        }
        break;

    case XIAN_SPEARMAN_STATE_AIM_4:
        if (info.ahead != 0) {
            head = info.angle;
        }
        creature->flags = 0;
        if (info.ahead == 0 || info.distance > XIAN_ATTACK_4_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_HIT_2;
        }
        break;

    case XIAN_SPEARMAN_STATE_AIM_5:
        if (info.ahead != 0) {
            head = info.angle;
        }
        creature->flags = 0;
        if (info.ahead == 0 || info.distance > XIAN_ATTACK_5_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP_2;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_HIT_5;
        }
        break;

    case XIAN_SPEARMAN_STATE_AIM_6:
        if (info.ahead != 0) {
            head = info.angle;
        }
        creature->flags = 0;
        if (info.ahead == 0 || info.distance > XIAN_ATTACK_6_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_RUN;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_HIT_6;
        }
        break;

    case XIAN_SPEARMAN_STATE_HIT_1:
        XianSpearman_DoDamage(item, creature, XIAN_SPEARMAN_HIT_1_DAMAGE);
        break;

    case XIAN_SPEARMAN_STATE_HIT_2:
    case XIAN_SPEARMAN_STATE_HIT_3:
    case XIAN_SPEARMAN_STATE_HIT_4:
        if (info.ahead != 0) {
            head = info.angle;
        }
        XianSpearman_DoDamage(item, creature, XIAN_SPEARMAN_HIT_2_DAMAGE);
        if (info.ahead != 0 && info.distance < XIAN_ATTACK_1_RANGE) {
            const int32_t random = Random_GetControl();
            if (random < 0x4000) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP;
            } else {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP_2;
            }
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
        }
        break;

    case XIAN_SPEARMAN_STATE_HIT_5:
        if (info.ahead != 0) {
            head = info.angle;
        }
        XianSpearman_DoDamage(item, creature, XIAN_SPEARMAN_HIT_5_DAMAGE);
        if (info.ahead != 0 && info.distance < XIAN_ATTACK_1_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP_2;
        }
        break;

    case XIAN_SPEARMAN_STATE_HIT_6:
        if (info.ahead != 0) {
            head = info.angle;
        }
        XianSpearman_DoDamage(item, creature, XIAN_SPEARMAN_HIT_6_DAMAGE);
        if (info.ahead != 0 && info.distance < XIAN_ATTACK_1_RANGE) {
            const int32_t random = Random_GetControl();
            if (random < 0x4000) {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP;
            } else {
                item->goal_anim_state = XIAN_SPEARMAN_STATE_STOP_2;
            }
        } else if (info.ahead != 0 && info.distance < XIAN_ATTACK_4_RANGE) {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_WALK;
        } else {
            item->goal_anim_state = XIAN_SPEARMAN_STATE_RUN;
        }
        break;

    default:
        break;
    }

    if (lara_alive && g_LaraItem->hit_points <= 0) {
        Creature_Kill(
            item, XIAN_SPEARMAN_ANIM_KILL, XIAN_SPEARMAN_STATE_KILL,
            LA_EXTRA_YETI_KILL);
        return;
    }

    Creature_Tilt(item, 0);
    Creature_Head(item, head);
    Creature_Neck(item, neck);
    Creature_Animate(item_num, angle, 0);
}

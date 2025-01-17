#include "game/phase/phase.h"

#include "game/clock.h"
#include "game/interpolation.h"
#include "game/output.h"
#include "game/phase/phase_cutscene.h"
#include "game/phase/phase_demo.h"
#include "game/phase/phase_game.h"
#include "game/phase/phase_inventory.h"
#include "game/phase/phase_stats.h"
#include "global/types.h"
#include "global/vars.h"

#include <libtrx/log.h>
#include <libtrx/memory.h>

#include <stdbool.h>
#include <stddef.h>

static PHASE_ENUM m_Phase = PHASE_NULL;
static PHASER *m_Phaser = NULL;

static bool m_Running = false;
static PHASE_ENUM m_PhaseToSet = PHASE_NULL;
static const void *m_PhaseToSetArgs = NULL;

static PHASE_CONTROL M_Control(int32_t nframes);
static void M_Draw(void);
static int32_t M_Wait(void);
static void M_SetUnconditionally(const PHASE_ENUM phase, const void *args);

static PHASE_CONTROL M_Control(int32_t nframes)
{
    if (g_GameInfo.override_gf_command.action != GF_NOOP) {
        const GAME_FLOW_COMMAND override = g_GameInfo.override_gf_command;
        g_GameInfo.override_gf_command =
            (GAME_FLOW_COMMAND) { .action = GF_NOOP };
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = override,
        };
    }

    if (m_Phaser && m_Phaser->control) {
        return m_Phaser->control(nframes);
    }
    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_Draw(void)
{
    Output_BeginScene();
    if (m_Phaser && m_Phaser->draw) {
        m_Phaser->draw();
    }
    Output_EndScene();
}

static void M_SetUnconditionally(const PHASE_ENUM phase, const void *args)
{
    if (m_Phaser && m_Phaser->end) {
        m_Phaser->end();
    }

    LOG_DEBUG("phase=%d", phase);
    switch (phase) {
    case PHASE_NULL:
        m_Phaser = NULL;
        break;

    case PHASE_GAME:
        m_Phaser = &g_GamePhaser;
        break;

    case PHASE_DEMO:
        m_Phaser = &g_DemoPhaser;
        break;

    case PHASE_CUTSCENE:
        m_Phaser = &g_CutscenePhaser;
        break;
    }

    if (m_Phaser && m_Phaser->start != NULL) {
        m_Phaser->start(args);
        Memory_FreePointer(&args);
    }

    // set it at the end, so that the start callbacks can retrieve the old phase
    m_Phase = phase;

    Clock_WaitTick();
}

PHASE_ENUM Phase_Get(void)
{
    return m_Phase;
}

void Phase_Set(const PHASE_ENUM phase, const void *const args)
{
    // changing the phase in the middle of rendering is asking for trouble,
    // so instead we schedule to run the change on the next iteration
    if (m_Running) {
        m_PhaseToSet = phase;
        m_PhaseToSetArgs = args;
        return;
    }
    M_SetUnconditionally(phase, args);
}

static int32_t M_Wait(void)
{
    if (m_Phaser && m_Phaser->wait) {
        return m_Phaser->wait();
    } else {
        return Clock_WaitTick();
    }
}

GAME_FLOW_COMMAND Phase_Run(void)
{
    int32_t nframes = Clock_WaitTick();
    PHASE_CONTROL control = { .action = PHASE_ACTION_CONTINUE };

    m_Running = true;
    LOG_DEBUG("phase start, phase=%d", m_Phase);

    while (1) {
        control = M_Control(nframes);

        if (m_PhaseToSet != PHASE_NULL) {
            if (control.action != PHASE_ACTION_NO_WAIT) {
                Interpolation_SetRate(1.0);
                M_Draw();
            }

            M_SetUnconditionally(m_PhaseToSet, m_PhaseToSetArgs);
            m_PhaseToSet = PHASE_NULL;
            m_PhaseToSetArgs = NULL;
            if (control.action == PHASE_ACTION_END) {
                M_Draw();
                break;
            }
            nframes = 2;
            // immediately advance to the next logic frame without any wait
            continue;
        }

        if (control.action == PHASE_ACTION_END) {
            break;
        }

        if (control.action != PHASE_ACTION_NO_WAIT) {
            if (Interpolation_IsEnabled()) {
                Interpolation_SetRate(0.5);
                M_Draw();
                M_Wait();
            }

            Interpolation_SetRate(1.0);
            M_Draw();
            nframes = M_Wait();
        }
    }

    m_Running = false;
    Phase_Set(PHASE_NULL, NULL);

    LOG_DEBUG(
        "phase end, action=%d, param=%d", control.gf_cmd.action,
        control.gf_cmd.param);
    return control.gf_cmd;
}

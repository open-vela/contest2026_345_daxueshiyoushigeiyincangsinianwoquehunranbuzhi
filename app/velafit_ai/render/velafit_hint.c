/****************************************************************************
 * app/velafit_ai/render/velafit_hint.c
 *
 * Hint table and display policy for the VelaFit product pages.
 *
 * Real mode only selects hints whose evidence exists today:
 *   - body check status (confidence, boundary points, measured body height,
 *     orientation, stability)
 *   - counting admission (tiny pose ready)
 *   - squat FSM phase
 *   - session state and counted repetitions
 *   - advice request / speech playback status
 * Missing signals (screen brightness, out-of-frame evidence outside body
 * check, distance during a set) never produce an automatic hint; their
 * wording exists only for ui_page layout tests.
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "velafit_hint.h"
#include "velafit_ui_theme.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Rows are looked up by id, so order never changes the meaning. */

static const velafit_hint_desc_t g_hint_table[] =
{
  {
    VELAFIT_HINT_ADVICE_FAILED,
    "建议获取失败 请重试",
    VELAFIT_HINT_PRIO_BLOCKING,
    "cloud advice request returned an error",
    "a retry succeeds or the user returns to the summary",
    0,
    true
  },
  {
    VELAFIT_HINT_TTS_FAILED,
    "语音失败 请看文字",
    VELAFIT_HINT_PRIO_BLOCKING,
    "speech playback failed while the advice body is readable",
    "a later cue plays or the user leaves the advice page",
    0,
    true
  },
  {
    VELAFIT_HINT_SUMMARY,
    "准备好后开始下一组",
    VELAFIT_HINT_PRIO_BLOCKING,
    "session reached FINISHED, including an early stop",
    "the next set starts or the user leaves the summary",
    0,
    true
  },
  {
    VELAFIT_HINT_STEP_IN,
    "请站入画面",
    VELAFIT_HINT_PRIO_UNAVAILABLE,
    "no usable pose is located during positioning",
    "a pose that passes body check appears",
    0,
    true
  },
  {
    VELAFIT_HINT_POSE_UNAVAILABLE,
    "姿态丢失 请调整位置",
    VELAFIT_HINT_PRIO_UNAVAILABLE,
    "pose admission failed during the set (cause unproven)",
    "a frame passes counting admission",
    0,
    true
  },
  {
    VELAFIT_HINT_FULL_BODY,
    "请让肩到脚完整入镜",
    VELAFIT_HINT_PRIO_UNAVAILABLE,
    "body check reports not fully visible from boundary evidence",
    "every required point is inside the frame margin",
    0,
    true
  },
  {
    VELAFIT_HINT_MOVE_BACK,
    "请后退一点",
    VELAFIT_HINT_PRIO_UNAVAILABLE,
    "body check reports too close from measured body height",
    "body height falls inside the accepted range",
    0,
    true
  },
  {
    VELAFIT_HINT_MOVE_CLOSER,
    "请靠近一点",
    VELAFIT_HINT_PRIO_UNAVAILABLE,
    "body check reports too far from measured body height",
    "body height falls inside the accepted range",
    0,
    true
  },
  {
    VELAFIT_HINT_ORIENTATION,
    "面向镜头 站直身体",
    VELAFIT_HINT_PRIO_UNAVAILABLE,
    "body check orientation gate reports rotated or invalid",
    "an upright orientation is confirmed",
    0,
    true
  },
  {
    VELAFIT_HINT_HOLD_STILL,
    "请保持静止",
    VELAFIT_HINT_PRIO_UNAVAILABLE,
    "body check reports stabilizing",
    "stable frames reach the configured threshold",
    0,
    true
  },
  {
    VELAFIT_HINT_STAND_READY,
    "准备就绪",
    VELAFIT_HINT_PRIO_GUIDE,
    "body check reports ready",
    "the session leaves positioning",
    0,
    true
  },
  {
    VELAFIT_HINT_STAND_TALL,
    "站直并保持静止",
    VELAFIT_HINT_PRIO_GUIDE,
    "session READY hold",
    "the READY hold elapses and countdown starts",
    0,
    true
  },
  {
    VELAFIT_HINT_GET_SET,
    "各就位",
    VELAFIT_HINT_PRIO_GUIDE,
    "session COUNTDOWN",
    "countdown reaches zero",
    0,
    true
  },
  {
    VELAFIT_HINT_GO,
    "开始蹲起",
    VELAFIT_HINT_PRIO_GUIDE,
    "first window of the running phase",
    "the running banner window closes",
    VF_UI_GO_BANNER_MS,
    true
  },
  {
    VELAFIT_HINT_KEEP_RISING,
    "请继续起身",
    VELAFIT_HINT_PRIO_GUIDE,
    "squat FSM reports the ascending phase",
    "the ascending phase ends",
    0,
    true
  },
  {
    VELAFIT_HINT_REP_DONE,
    "完成一次",
    VELAFIT_HINT_PRIO_COMPLETE,
    "a new repetition was counted; consumed once per increment",
    "the feedback validity window ends",
    2000,
    true
  },
  {
    VELAFIT_HINT_ENCOURAGE,
    "继续保持",
    VELAFIT_HINT_PRIO_ENCOURAGE,
    "pose admitted and no stronger hint applies",
    "any higher-priority condition appears",
    0,
    true
  },
  {
    VELAFIT_HINT_IDLE,
    "准备好了就开始",
    VELAFIT_HINT_PRIO_ENCOURAGE,
    "no active session",
    "a wake event starts positioning",
    0,
    true
  },
  {
    VELAFIT_HINT_LIGHT_DARK,
    "光线不足 请调亮",
    VELAFIT_HINT_PRIO_UNAVAILABLE,
    "layout test only: no verified brightness signal exists",
    "never auto-selected in real mode",
    0,
    false
  },
};

/****************************************************************************
 * Private Helpers
 ****************************************************************************/

static const velafit_hint_desc_t *hint_row(velafit_hint_id_t id)
{
  unsigned int i;

  for (i = 0; i < sizeof(g_hint_table) / sizeof(g_hint_table[0]); i++)
    {
      if (g_hint_table[i].id == id)
        {
          return &g_hint_table[i];
        }
    }

  return NULL;
}

static velafit_hint_id_t hint_resolve(const velafit_hint_input_t *input)
{
  if (input->advice_failed)
    {
      return VELAFIT_HINT_ADVICE_FAILED;
    }

  if (input->tts_failed)
    {
      return VELAFIT_HINT_TTS_FAILED;
    }

  switch (input->session_state)
    {
      case VF_FINISHED:
        return VELAFIT_HINT_SUMMARY;

      case VF_IDLE:
      case VF_WAKEUP:
      case VF_PAUSED:
        return VELAFIT_HINT_IDLE;

      case VF_BODY_CHECK:
        /* No verdict yet means "not located"; never assert an empty frame. */
        if (!input->body_valid)
          {
            return VELAFIT_HINT_STEP_IN;
          }

        switch (input->body_status)
          {
            case BODY_CHECK_NOT_FULLY_VISIBLE:
              return VELAFIT_HINT_FULL_BODY;

            case BODY_CHECK_TOO_CLOSE:
              return VELAFIT_HINT_MOVE_BACK;

            case BODY_CHECK_TOO_FAR:
              return VELAFIT_HINT_MOVE_CLOSER;

            case BODY_CHECK_ORIENTATION_INVALID:
              return VELAFIT_HINT_ORIENTATION;

            case BODY_CHECK_STABILIZING:
              return VELAFIT_HINT_HOLD_STILL;

            case BODY_CHECK_READY:
              return VELAFIT_HINT_STAND_READY;

            case BODY_CHECK_LOW_CONFIDENCE:
            default:
              return VELAFIT_HINT_STEP_IN;
          }

      case VF_READY:
        return VELAFIT_HINT_STAND_TALL;

      case VF_COUNTDOWN:
        return VELAFIT_HINT_GET_SET;

      case VF_RUNNING:
        if (!input->tracking)
          {
            /* Admission failed mid-set: the reason is not provable, so use
             * the generic pose-unavailable wording. */
            return VELAFIT_HINT_POSE_UNAVAILABLE;
          }

        if (input->go_banner)
          {
            return VELAFIT_HINT_GO;
          }

        if (input->rep_in_progress)
          {
            return VELAFIT_HINT_KEEP_RISING;
          }

        return VELAFIT_HINT_ENCOURAGE;

      default:
        return VELAFIT_HINT_IDLE;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void velafit_hint_config_default(velafit_hint_config_t *config)
{
  if (config == NULL)
    {
      return;
    }

  config->stability_ms = 300;
  config->min_display_ms = VF_UI_HINT_MIN_MS;
  config->rep_feedback_ms = 2000;
}

void velafit_hint_init(velafit_hint_state_t *state,
                       const velafit_hint_config_t *config)
{
  if (state == NULL)
    {
      return;
    }

  memset(state, 0, sizeof(*state));
  if (config != NULL)
    {
      state->config = *config;
    }
  else
    {
      velafit_hint_config_default(&state->config);
    }

  state->shown = VELAFIT_HINT_NONE;
  state->candidate = VELAFIT_HINT_NONE;
}

void velafit_hint_reset(velafit_hint_state_t *state, uint32_t generation)
{
  if (state == NULL)
    {
      return;
    }

  state->shown = VELAFIT_HINT_NONE;
  state->shown_since_ms = 0;
  state->candidate = VELAFIT_HINT_NONE;
  state->candidate_since_ms = 0;
  state->last_reps = 0;
  state->rep_feedback_until_ms = 0;
  state->generation = generation;
  state->transitions = 0;
  state->rep_events = 0;
}

void velafit_hint_update(velafit_hint_state_t *state,
                         const velafit_hint_input_t *input,
                         uint64_t now_ms, velafit_hint_result_t *result)
{
  velafit_hint_id_t desired;
  velafit_hint_id_t previous;
  unsigned int shown_prio;
  unsigned int desired_prio;
  bool rep_event = false;

  if (state == NULL || input == NULL)
    {
      return;
    }

  previous = state->shown;

  /* Counted increments are observed exactly once.  A decrease can only come
   * from a restart and is absorbed without inventing a completion event. */

  if (input->reps > state->last_reps)
    {
      state->rep_events += input->reps - state->last_reps;
      state->rep_feedback_until_ms = now_ms + state->config.rep_feedback_ms;
      rep_event = true;
    }

  state->last_reps = input->reps;

  desired = hint_resolve(input);
  desired_prio = velafit_hint_priority(desired);

  /* Completion feedback competes only with encouragement. */

  if (now_ms < state->rep_feedback_until_ms &&
      velafit_hint_priority(VELAFIT_HINT_REP_DONE) > desired_prio)
    {
      desired = VELAFIT_HINT_REP_DONE;
      desired_prio = velafit_hint_priority(desired);
    }

  if (state->shown == VELAFIT_HINT_NONE)
    {
      /* First line of a page appears immediately; there is nothing to debounce
       * against and the band must never be blank. */
      state->shown = desired;
      state->shown_since_ms = now_ms;
      state->candidate = VELAFIT_HINT_NONE;
      state->transitions++;
    }
  else if (desired == state->shown)
    {
      state->candidate = VELAFIT_HINT_NONE;
    }
  else if (desired_prio >= VELAFIT_HINT_PRIO_BLOCKING)
    {
      /* Blocking errors and session end must not wait for a debounce. */
      if (state->shown != desired)
        {
          state->transitions++;
        }

      state->shown = desired;
      state->shown_since_ms = now_ms;
      state->candidate = VELAFIT_HINT_NONE;
    }
  else
    {
      if (state->candidate != desired)
        {
          state->candidate = desired;
          state->candidate_since_ms = now_ms;
        }
      else if (now_ms - state->candidate_since_ms >=
               state->config.stability_ms)
        {
          shown_prio = velafit_hint_priority(state->shown);

          /* A lower-priority hint may replace the current one only after the
           * current one has been readable for its minimum time.  The
           * candidate keeps its own age, so the switch happens on the first
           * frame after the minimum passes. */
          if (shown_prio > desired_prio &&
              now_ms - state->shown_since_ms < state->config.min_display_ms)
            {
              /* keep showing */
            }
          else
            {
              state->shown = desired;
              state->shown_since_ms = now_ms;
              state->candidate = VELAFIT_HINT_NONE;
              state->transitions++;
            }
        }
    }

  if (result != NULL)
    {
      result->previous = previous;
      result->changed = previous != state->shown;
      result->id = state->shown;
      result->rep_event = rep_event;
    }
}

const char *velafit_hint_text(velafit_hint_id_t id)
{
  const velafit_hint_desc_t *row = hint_row(id);

  return row != NULL ? row->text : "";
}

unsigned int velafit_hint_priority(velafit_hint_id_t id)
{
  const velafit_hint_desc_t *row = hint_row(id);

  return row != NULL ? row->priority : VELAFIT_HINT_PRIO_ENCOURAGE;
}

velafit_ui_tone_t velafit_hint_tone(velafit_hint_id_t id)
{
  unsigned int priority = velafit_hint_priority(id);

  if (priority >= VELAFIT_HINT_PRIO_BLOCKING)
    {
      return VELAFIT_UI_TONE_ERROR;
    }

  if (priority >= VELAFIT_HINT_PRIO_UNAVAILABLE)
    {
      return VELAFIT_UI_TONE_WARN;
    }

  return VELAFIT_UI_TONE_NORMAL;
}

const velafit_hint_desc_t *velafit_hint_describe(velafit_hint_id_t id)
{
  return hint_row(id);
}

unsigned int velafit_hint_table(const velafit_hint_desc_t **rows,
                                unsigned int max_rows)
{
  unsigned int count = 0;
  unsigned int i;

  if (rows == NULL)
    {
      return 0;
    }

  for (i = 0; i < sizeof(g_hint_table) / sizeof(g_hint_table[0]); i++)
    {
      if (count >= max_rows)
        {
          break;
        }

      rows[count++] = &g_hint_table[i];
    }

  return count;
}

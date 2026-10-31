/****************************************************************************
 * app/velafit_ai/render/velafit_hint.h
 *
 * VelaFit hint policy: the one line shown in the hint band is chosen from a
 * fixed table.  Every entry has a trigger, a recovery condition and a
 * validity window.  The policy never runs inference, never mutates counters
 * and never invents a diagnosis: it only maps already-available signals to
 * text.  Debouncing and the minimum display time affect what is *shown*, not
 * what is counted or admitted.
 ****************************************************************************/

#ifndef __APP_VELAFIT_AI_RENDER_VELAFIT_HINT_H
#define __APP_VELAFIT_AI_RENDER_VELAFIT_HINT_H

#include <stdbool.h>
#include <stdint.h>

#include "velafit_ui.h"
#include "velafit_body_check.h"
#include "velafit_session.h"

/* Priority bands.  Higher wins; equal ranks are resolved by stability time,
 * never by arrival order.  Blocking hints bypass both the stability gate and
 * the minimum display time. */

#define VELAFIT_HINT_PRIO_ENCOURAGE    0u
#define VELAFIT_HINT_PRIO_COMPLETE     10u
#define VELAFIT_HINT_PRIO_GUIDE        20u
#define VELAFIT_HINT_PRIO_UNAVAILABLE  50u
#define VELAFIT_HINT_PRIO_BLOCKING     100u

typedef enum
{
  VELAFIT_HINT_NONE = 0,

  /* Blocking: errors and session-end operations. */

  VELAFIT_HINT_ADVICE_FAILED,
  VELAFIT_HINT_TTS_FAILED,
  VELAFIT_HINT_SUMMARY,

  /* Pose unavailable: the pose cannot be used, without naming a cause that
   * the current signals cannot prove. */

  VELAFIT_HINT_STEP_IN,
  VELAFIT_HINT_POSE_UNAVAILABLE,
  VELAFIT_HINT_FULL_BODY,
  VELAFIT_HINT_MOVE_BACK,
  VELAFIT_HINT_MOVE_CLOSER,
  VELAFIT_HINT_ORIENTATION,
  VELAFIT_HINT_HOLD_STILL,

  /* Action guidance */

  VELAFIT_HINT_STAND_READY,
  VELAFIT_HINT_STAND_TALL,
  VELAFIT_HINT_GET_SET,
  VELAFIT_HINT_GO,
  VELAFIT_HINT_KEEP_RISING,

  /* Completion feedback and the last-resort encouragement. */

  VELAFIT_HINT_REP_DONE,
  VELAFIT_HINT_ENCOURAGE,
  VELAFIT_HINT_IDLE,

  /* Layout-only entry: there is no verified brightness measurement yet, so
   * this text is never selected by real mode and only appears in ui_page. */

  VELAFIT_HINT_LIGHT_DARK,

  VELAFIT_HINT_COUNT
} velafit_hint_id_t;

typedef struct
{
  uint64_t stability_ms;     /* a lower-priority change must persist this long */
  uint64_t min_display_ms;   /* every non-blocking hint stays this long */
  uint64_t rep_feedback_ms;  /* completion feedback validity window */
} velafit_hint_config_t;

typedef struct
{
  enum velafit_session_state session_state;

  /* Body check verdict, valid only while the session is positioning. */
  velafit_body_check_status_t body_status;
  bool body_valid;

  bool tracking;          /* the current pose passed the counting admission */
  bool rep_in_progress;   /* squat FSM reports the ascending phase */
  bool go_banner;         /* the running banner window is open */

  bool advice_failed;     /* cloud advice request failed */
  bool tts_failed;        /* speech playback failed, text still readable */

  uint32_t reps;          /* counted repetitions of this session */
} velafit_hint_input_t;

typedef struct
{
  velafit_hint_config_t config;
  velafit_hint_id_t shown;
  uint64_t shown_since_ms;
  velafit_hint_id_t candidate;
  uint64_t candidate_since_ms;
  uint32_t last_reps;
  uint64_t rep_feedback_until_ms;
  uint32_t generation;
  uint32_t transitions;   /* switches made in this session, for the log */
  uint32_t rep_events;    /* counted increments seen, never suppressed */
} velafit_hint_state_t;

typedef struct
{
  velafit_hint_id_t id;
  velafit_hint_id_t previous;
  bool changed;
  bool rep_event;         /* a new repetition was counted in this update */
} velafit_hint_result_t;

typedef struct
{
  velafit_hint_id_t id;
  const char *text;
  unsigned int priority;
  const char *trigger;    /* what makes this hint eligible */
  const char *recovery;   /* what clears it */
  uint64_t validity_ms;   /* 0 = until the trigger clears */
  bool real_mode;         /* false = layout test only, never auto-selected */
} velafit_hint_desc_t;

#ifdef __cplusplus
extern "C"
{
#endif

void velafit_hint_config_default(velafit_hint_config_t *config);
void velafit_hint_init(velafit_hint_state_t *state,
                       const velafit_hint_config_t *config);

/* Drop per-session state on a new generation.  Non-zero generation values are
 * recorded so a stale set can never reuse the previous rep counter. */
void velafit_hint_reset(velafit_hint_state_t *state, uint32_t generation);

void velafit_hint_update(velafit_hint_state_t *state,
                         const velafit_hint_input_t *input,
                         uint64_t now_ms, velafit_hint_result_t *result);

const char *velafit_hint_text(velafit_hint_id_t id);
velafit_ui_tone_t velafit_hint_tone(velafit_hint_id_t id);
unsigned int velafit_hint_priority(velafit_hint_id_t id);

const velafit_hint_desc_t *velafit_hint_describe(velafit_hint_id_t id);
unsigned int velafit_hint_table(const velafit_hint_desc_t **rows,
                                unsigned int max_rows);

#ifdef __cplusplus
}
#endif

#endif /* __APP_VELAFIT_AI_RENDER_VELAFIT_HINT_H */

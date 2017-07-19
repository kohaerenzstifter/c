#ifndef _KSEQUENCER_H
#define _KSEQUENCER_H

#include <stdlib.h>
#include <alsa/asoundlib.h>

#include <kohaerenzstiftung.h>

#define CROTCHETS_PER_BAR 4
#define TICKS_PER_CROTCHET 24
#define TICKS_PER_BAR (TICKS_PER_CROTCHET * CROTCHETS_PER_BAR)

#define EVENTSTEPS_PER_BAR MAX_EVENTSTEPS_PER_BAR
#define MICROTICKS_PER_BAR EVENTSTEPS_PER_BAR

typedef struct {
    pthread_mutex_t value;
    gboolean initialised;
} mutex_t;

#define MUTEX_SYNCHRONISATION 0
#define MUTEX_SEQUENCER 1
#define MUTEX_DATA 2
#define NR_MUTEXES 3

#define LOCK_SYNCHRONISATION (1 << MUTEX_SYNCHRONISATION)
#define LOCK_SEQUENCER (1 << MUTEX_SEQUENCER)
#define LOCK_DATA (1 << MUTEX_DATA)

typedef struct {
	uint32_t mutexes[NR_MUTEXES];
} lockContext_t;

#define DECLARE_LOCKCONTEXT \
  static lockContext_t lockContext = { \
	.mutexes = { 0 } \
  };

typedef struct {
	void *pointer;
	int8_t number;
	gboolean boolean;
} null_t;

typedef ssize_t (*nullFunc_t)(void);

typedef struct {
	uint64_t sequence;
	struct {
		gboolean have;
		uint64_t value;
		uint64_t at;
	} lastTick;
	struct {
		gboolean have;
		uint64_t nanosecondsPerTick;
	} pace;
} synchronisationStatus_t;

typedef struct {
	int client;
	int port;
	char *clientName;
	char *portName;
} midiPort_t;

typedef enum {
	patternTypeDummy,
	patternTypeNote,
	patternTypeController
} patternType_t;

typedef struct {
	char *name;
	int8_t tone;
	gboolean sharp;
	int8_t octave;
	snd_seq_event_t *snd_seq_event;
} noteValue_t;

typedef struct {
	char *name;
	uint8_t value;
	snd_seq_event_t *snd_seq_event;
} controllerValue_t;

typedef struct noteEvent noteEvent_t;

typedef struct {
	noteEvent_t *onNoteEvent;
	noteEvent_t *offNoteEvent;
} noteEventStep_t;

struct noteEvent {
	noteEventStep_t *noteEventStep;
	noteValue_t *noteValue;
	union {
		struct {
			noteEvent_t *offNoteEvent;
		} on;
		struct {
			GSList *noteOffLink;
		} off;
	};
};

typedef struct {
	GSList *value;
	gboolean locked;
	gboolean slide;
	gboolean slideLocked;
	noteEvent_t *onNoteEvent;
} noteUserStep_t;

typedef struct {
	controllerValue_t *controllerValue;
} controllerEventStep_t;

typedef struct {
	GSList *value;
	gboolean locked;
} controllerUserStep_t;

typedef struct {
	gboolean set;
	gboolean locked;
} dummyUserStep_t;

typedef struct pattern {
	patternType_t patternType;
	char *name;
	struct pattern *parent;
	GSList *children;
	uint32_t bars;
	uint32_t stepsPerBar;
	union {
		struct {
			struct {
				dummyUserStep_t *user;
			} steps;
		} dummy;
		struct {
			uint8_t channel;
			GSList *values;
			struct {
				noteUserStep_t *user;
				noteEventStep_t *event;
			} steps;
		} note;
		struct {
			uint8_t channel;
			uint8_t parameter;
			GSList *values;
			struct {
				controllerUserStep_t *user;
				controllerEventStep_t *event;
			} steps;
		} controller;
	};
} pattern_t;

#define SEGFAULT_POINTER (((null_t *) NULL)->pointer)
#define SEGFAULT_NUMBER (((null_t *) NULL)->number)
#define SEGFAULT_BOOLEAN (((null_t *) NULL)->boolean)
#define SEGFAULT_FUNCTION (((nullFunc_t) NULL)())

#define PARENT(p) ((p)->parent)
#define TYPE(p) ((p)->patternType)
#define NAME(p) ((p)->name)
#define PTR_CHANNEL(p) (IS_NOTE((p)) ? (&((p)->note.channel)) : \
  IS_CONTROLLER((p)) ? (&((p)->controller.channel)) : \
  ((uint8_t *) SEGFAULT_POINTER))
#define CHANNEL(p) (*(PTR_CHANNEL((p))))
#define PTR_PARAMETER(p) \
  (IS_CONTROLLER((p)) ? (&((p)->controller.parameter)) : \
  ((uint8_t *) SEGFAULT_POINTER))
#define PARAMETER(p) (*(PTR_PARAMETER((p))))

#define PTR_CHILDREN(p) (&((p)->children))
#define CHILDREN(p) (*PTR_CHILDREN((p)))
#define PTR_VALUES(p) (IS_DUMMY((p)) ? ((GSList **) SEGFAULT_POINTER) : \
  IS_NOTE((p)) ? (&((p)->note.values)) : (&((p)->controller.values)))
#define VALUES(p) (*PTR_VALUES((p)))
#define VALUE(s, t) (((t) == patternTypeDummy) ? SEGFAULT_POINTER : \
  ((t) == patternTypeNote) ? (((noteUserStep_t *) (s))->value) : \
  (((controllerUserStep_t *) (s))->value))
#define VALUE_NAME(v, t) (((t) == patternTypeDummy) ? SEGFAULT_POINTER : \
  ((t) == patternTypeNote) ? ((noteValue_t *) v)->name : \
  ((controllerValue_t *) v)->name)

#define IS_ROOT(p) (PARENT((p)) == NULL)
#define IS_DUMMY(p) (TYPE((p)) == patternTypeDummy)
#define IS_NOTE(p) (TYPE((p)) == patternTypeNote)
#define IS_CONTROLLER(p) (TYPE((p)) == patternTypeController)

#define UNSET_STEP(p, s, i) \
  do { \
    if (IS_DUMMY((p))) { \
      ((dummyUserStep_t *) (s))->set = FALSE; \
    } else if (IS_CONTROLLER((p))) { \
      ((controllerUserStep_t *) (s))->value = NULL; \
      doSetControllerStep(((p)), ((s)), (i)); \
    } else { \
      setSlide(((p)), (s), FALSE, (i), NULL, TRUE, NULL); \
      unsetNoteStep((p), (s), (i)); \
    } \
  } while (FALSE);

#define NR_USERSTEPS_PER_BAR(p) ((p)->stepsPerBar)
#define NR_BARS(p) ((p)->bars)
#define NR_STEPS_PER_BAR(p) ((p)->stepsPerBar)
#define NR_USERSTEPS(p) (NR_BARS((p)) * NR_USERSTEPS_PER_BAR((p)))
#define NR_EVENTSTEPS(p) \
  (NR_USERSTEPS((p)) * EVENTSTEPS_PER_USERSTEP(TYPE((p))))
#define CB_STEP(t) (((t) == patternTypeDummy) ? (G_CALLBACK(cbDummyStep))  : \
  ((t) == patternTypeNote) ? (G_CALLBACK(cbNoteStep))  : \
  (G_CALLBACK(cbControllerStep)))
#define DISPLAYTEXT(s, t) ((!IS_SET((s), (t))) ? "" : \
  ((t) == patternTypeDummy) ? "X" : VALUE_NAME(((GSList *) VALUE((s), \
  (t)))->data, (t)))
#define EVENTSTEP_AT(p, i) (IS_NOTE((p)) ? ((void *) \
  &(((p)->note.steps.event[(i)]))) : \
  IS_CONTROLLER((p)) ? ((void *) &(((p)->controller.steps.event[(i)]))) : \
  SEGFAULT_POINTER)
#define USERSTEP_AT2(t, s, i) \
  (((t) == patternTypeDummy) ? ((void *) &((((dummyUserStep_t *)(s))[(i)]))) : \
  ((t) == patternTypeNote) ? ((void *) &((((noteUserStep_t *)(s))[(i)]))) : \
  ((void *) &((((controllerUserStep_t *)(s))[(i)]))))
#define USERSTEP_AT(p, i) USERSTEP_AT2((p)->patternType, USERSTEPS((p)), (i))
#define PARENTSTEP(p, i) ((!IS_ROOT((p))) ? USERSTEP_AT((p)->parent, (((i) / \
  (NR_USERSTEPS_PER_BAR((p)) / NR_USERSTEPS_PER_BAR((PARENT((p)))))) % \
  NR_USERSTEPS(PARENT((p))))) : SEGFAULT_POINTER)
#define PTR_USERSTEPS(p) (IS_DUMMY((p)) ? \
  ((char **) &((p)->dummy.steps.user)) : \
  IS_NOTE((p)) ? ((char **) (&((p)->note.steps.user))) : \
  ((char **) (&((p)->controller.steps.user))))
#define USERSTEPS(p) (*PTR_USERSTEPS((p)))
#define PTR_EVENTSTEPS(p) (IS_NOTE((p)) ? \
  ((char **) (&((p)->note.steps.event))) : \
  IS_CONTROLLER((p)) ? ((char **) (&((p)->controller.steps.event))) : \
  ((char **) SEGFAULT_POINTER))
#define EVENTSTEPS(p) (*PTR_EVENTSTEPS((p)))
#define SZ_USERSTEP(p) (IS_DUMMY((p)) ? sizeof(dummyUserStep_t) : \
  IS_NOTE((p)) ? sizeof(noteUserStep_t) : sizeof(controllerUserStep_t))
#define SZ_EVENTSTEP(p) (IS_NOTE((p)) ? sizeof(noteUserStep_t) : \
  IS_CONTROLLER((p)) ? sizeof(controllerUserStep_t) : \
  SEGFAULT_NUMBER)

#define IS_SET(s, t) (((t) == patternTypeDummy) ? \
  (((dummyUserStep_t *) (s))->set) : \
  ((t) == patternTypeNote) ? (((noteUserStep_t *) (s))->value != NULL) : \
  ((t) == patternTypeController) ? (((controllerUserStep_t *) \
  (s))->value != NULL) : SEGFAULT_BOOLEAN)
#define HAS_SLIDE(s, t) (((t) != patternTypeNote) ? \
  SEGFAULT_BOOLEAN : (((noteUserStep_t *) s)->slide))
#define NR_VALUES(p) (IS_DUMMY((p)) ? (1) : g_slist_length((VALUES((p)))))
#define LOCKED_PTR(s, t) \
  (((t) == patternTypeDummy) ? &(((dummyUserStep_t *) (s))->locked)  : \
  ((t) == patternTypeNote) ? &(((noteUserStep_t *) (s))->locked) : \
  &(((controllerUserStep_t *) (s))->locked))
#define LOCKED(s, t) (*(LOCKED_PTR(s, t)))
#define EVENTSTEPS_PER_USERSTEP(t) (((t) == patternTypeNote) ? 2 : \
  ((t) == patternTypeController) ? 1 : \
  SEGFAULT_NUMBER)
#define MAX_EVENTSTEPS_PER_BAR 512
#define MAX_STEPS_PER_BAR(p) \
  (IS_DUMMY((p)) ? (MAX_EVENTSTEPS_PER_BAR / 1) : \
  (MAX_EVENTSTEPS_PER_BAR / (EVENTSTEPS_PER_USERSTEP(TYPE((p))))))

#define SET_STEP_FROM_STEP(p, s, t, i, lst, l, e) \
  do { \
    if (IS_DUMMY((p))) { \
      dummyUserStep_t *source = (s); \
      dummyUserStep_t *target = (t); \
      terror(setDummyStep((p), target, source->set, (l), (e))) \
    } else if (IS_CONTROLLER((p))) { \
      controllerUserStep_t *source = (s); \
      controllerUserStep_t *target = (t); \
      terror(setControllerStep((p), target, source->value, (i), (l), (e))) \
    } else { \
      noteUserStep_t *source = (s); \
      noteUserStep_t *target = (t); \
      terror(setNoteStep((p), target, source->value, (i), (l), FALSE, (e))) \
      terror(setSlide((p), target, (lst) ? FALSE : \
        source->slide, (i), (l), FALSE, (e))) \
    } \
  } while (FALSE);


#ifdef _TABLE_C
#define VARIABLE(type, name, value) type name = value;
#else
#define VARIABLE(type, name, value) extern type name;
#endif

#define COMMA ,

VARIABLE( \
  gboolean, \
  goingDown, \
  FALSE \
)

VARIABLE( \
  noteEvent_t, \
  *pendingOff, \
  NULL \
)

VARIABLE ( \
  struct { \
    mutex_t value[NR_MUTEXES]; \
  }, \
  mutexes, \
  { \
    { \
      { \
        .initialised = FALSE \
      } \
    } \
  } \
)

VARIABLE ( \
  struct { \
    snd_seq_t *snd_seq; \
    volatile gboolean connected; \
	snd_seq_addr_t snd_seq_addr; \
    int myPort; \
  },
  sequencer, \
  { \
    .snd_seq = NULL COMMA \
    .connected = FALSE COMMA \
    .myPort = -1 \
  } \
)

VARIABLE( \
  struct { \
    volatile gboolean value; \
    int pipe[2]; \
  }, \
  terminate, \
  { \
    FALSE COMMA \
    { \
      -1 COMMA \
      -1 \
    } \
  } \
)

VARIABLE( \
  struct { \
    pthread_t input; \
    pthread_t output; \
  }, \
  threads, \
  { \
  } \
)

VARIABLE( \
  struct { \
    volatile pattern_t *root; \
  }, \
  patterns,
  { \
    .root = NULL \
  } \
)

VARIABLE( \
  struct { \
    volatile GSList *value;
  }, \
  notesOff,
  { \
    .value = NULL COMMA \
  } \
)

VARIABLE ( \
  long, \
  subtractSeconds, \
  0
)

VARIABLE( \
  struct { \
    pthread_cond_t wakeupConsumers; \
	synchronisationStatus_t synchronisationStatus; \
  }, \
  synchronisation, \
  { \
    .synchronisationStatus = { \
      .sequence = 0 COMMA \
      .lastTick = { \
        .have = FALSE COMMA \
        .value = 0 COMMA \
        .at = 0 \
      } COMMA \
      .pace = { \
        .have = FALSE COMMA \
        .nanosecondsPerTick = 0 \
      } \
    } \
  } \
)

VARIABLE(GSList *, midiPorts, NULL)

void freeControllerValue(controllerValue_t *controllerValue);
void freeNoteValue(noteValue_t *noteValue);
void connectToPort(lockContext_t *lockContext,
  guint client, guint port, err_t *e);
void disconnectFromPort(lockContext_t *lockContext, err_t *e);
void lockUserStep(pattern_t *pattern, uint32_t idx);
void lockSlide(pattern_t *pattern, uint32_t idx);
void setDummyStep(pattern_t *pattern, dummyUserStep_t *dummyUserStep,
  gboolean set, lockContext_t *lockContext, err_t *e);
void setNoteStep(pattern_t *pattern, noteUserStep_t *noteUserStep,
  GSList *value, uint32_t idx, lockContext_t *lockContext,
  gboolean live, err_t *e);
void setControllerStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, GSList *value,
  uint32_t idx, lockContext_t *lockContext, err_t *e);
void setSlide(pattern_t *pattern, noteUserStep_t *noteUserStep,
  gboolean slide, uint32_t idx, lockContext_t *lockContext,
  gboolean live, err_t *e);
void freePattern(pattern_t *pattern);
pattern_t *allocatePattern(pattern_t *parent);
snd_seq_event_t *getAlsaEvent(void);
noteValue_t *allocateNoteValue(void);
controllerValue_t *allocateControllerValue(void);
void setAlsaNoteEvent(noteValue_t *noteValue, uint8_t channel,
  lockContext_t *lockContext, gboolean live, err_t *e);
void setAlsaControllerEvent(controllerValue_t *controllerValue, uint8_t channel,
  uint8_t parameter, lockContext_t *lockContext, gboolean live, err_t *e);
void setAlsaControllerEvent2(snd_seq_event_t *snd_seq_event, uint8_t channel,
  uint8_t parameter, uint8_t value);
void setSteps(pattern_t *pattern);
void adjustSteps(pattern_t *pattern, uint32_t bars, uint32_t stepsPerBar,
  lockContext_t *lockContext, gboolean live, err_t *e);
void deleteChild(pattern_t *parent, GSList *child,
  lockContext_t *lockContext, err_t *e);
void freeValue(pattern_t *pattern, void *value);
void getLocks(lockContext_t *lockContext, uint32_t locks, err_t *e);
void releaseLocks(lockContext_t *lockContext, uint32_t locks, err_t *e);
gboolean anyChildStepSet(pattern_t *pattern, uint32_t idx);
void unsoundPattern(lockContext_t *lockContext, pattern_t *pattern, err_t *e);
void *output(void *param);
void *input(void *param);
void gtkSignalStep(uint64_t step);
void gtkSignalStop();
void gtkSignalSpeed(uint64_t nanosecondsPerCrotchet);
void allNotesOff(lockContext_t *lockContext, gboolean alreadyLocked, err_t *e);
void test(void);
void randomise(pattern_t *pattern, uint32_t bar, lockContext_t *lockContext);
gboolean getLocked(gboolean *unlockable,
  void *step, pattern_t *pattern, uint32_t idx);

#endif

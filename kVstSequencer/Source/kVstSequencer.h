#ifndef _KVSTSEQUENCER_H
#define _KVSTSEQUENCER_H

#include <stdlib.h>
#include <kohaerenzstiftung.h>

#if 0
#define MARK() \
  do { \
    FILE *file = getOutFile(); \
    if (file != NULL) { \
      fprintf(file, "%s (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__); \
      fflush(file); \
    } \
  } while (FALSE);
#else
#define MARK()
#endif

#ifdef __cplusplus
#include <string>
extern "C" {
#endif

#define CROTCHETS_PER_BAR 4
#define TICKS_PER_CROTCHET 24
#define TICKS_PER_BAR (TICKS_PER_CROTCHET * CROTCHETS_PER_BAR)

#define EVENTSTEPS_PER_BAR MAX_EVENTSTEPS_PER_BAR
#define MICROTICKS_PER_BAR EVENTSTEPS_PER_BAR


typedef struct {
    pthread_mutex_t value;
    gboolean initialised;
} mutex_t;

#define MUTEX_SEQUENCER 0
#define MUTEX_DATA 1
#define NR_MUTEXES 2

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

#define MIDI_PITCH(n) \
  (notes[(n)->noteValue->note].midiValue + \
  ((12 * (n)->noteValue->octave) + ((n)->noteValue->sharp ? 1 : 0)))

typedef enum {
	midiMessageTypeNoteOff,
	midiMessageTypeController,
} midiMessageType_t;

typedef struct {
	midiMessageType_t midiMessageType;
	union {
		struct {
			int32_t noteNumber;
		} noteOff;
		struct {
			uint8_t parameter;
			int8_t value;
		} controller;
	};
} midiMessage_t;

typedef enum {
	patternTypeDummy,
	patternTypeNote,
	patternTypeController
} patternType_t;

#define NOTE_C 0
#define NOTE_D 1
#define NOTE_E 2
#define NOTE_F 3
#define NOTE_G 4
#define NOTE_A 5
#define NOTE_B 6

#define NOTE2CHAR(n) \
  ((n) == NOTE_C) ? 'c' : \
  ((n) == NOTE_D) ? 'd' : \
  ((n) == NOTE_E) ? 'e' : \
  ((n) == NOTE_F) ? 'f' : \
  ((n) == NOTE_G) ? 'g' : \
  ((n) == NOTE_A) ? 'a' : \
  'b'

typedef struct {
	volatile char *name;
	volatile uint8_t note;
	volatile gboolean sharp;
	volatile int8_t octave;
} noteValue_t;

typedef struct { \
	volatile char *name;
	volatile uint8_t value;
} controllerValue_t;

typedef struct noteEvent noteEvent_t;

typedef struct {
	volatile noteEvent_t *onNoteEvent;
	volatile noteEvent_t *offNoteEvent;
} noteEventStep_t;

struct noteEvent {
	volatile noteEventStep_t *noteEventStep;
	volatile noteValue_t *noteValue;
	union {
		struct {
			uint8_t velocity;
			volatile noteEvent_t *offNoteEvent;
		} on;
		struct {
			volatile GSList *noteOffLink;
		} off;
	};
};

typedef struct {
	GSList *value;
	GSList *velocity;
	gboolean locked;
	gboolean slide;
	gboolean slideLocked;
	noteEvent_t *onNoteEvent;
} noteUserStep_t;

typedef struct {
	volatile controllerValue_t *controllerValue;
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
	volatile GSList *children;
	volatile uint32_t bars;
	volatile uint32_t stepsPerBar;
	volatile union {
		struct {
			struct {
				dummyUserStep_t *user;
			} steps;
		} dummy;
		struct {
			uint8_t channel;
			GSList *values;
			GSList *velocities;
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
#define PTR_VELOCITIES(p) \
  (IS_NOTE((p)) ? (&((p)->note.velocities)) : ((GSList **) SEGFAULT_POINTER))
#define VELOCITIES(p) (*PTR_VELOCITIES((p)))
#define VALUE(s, t) (((t) == patternTypeDummy) ? SEGFAULT_POINTER : \
  ((t) == patternTypeNote) ? (((noteUserStep_t *) (s))->value) : \
  (((controllerUserStep_t *) (s))->value))
#define VELOCITY(s, t) (((t) != patternTypeNote) ? SEGFAULT_POINTER : \
  (((noteUserStep_t *) (s))->velocity))
#define VALUE_NAME(v, t) (((t) == patternTypeDummy) ? SEGFAULT_POINTER : \
  ((t) == patternTypeNote) ? ((noteValue_t *) v)->name : \
  ((controllerValue_t *) v)->name)
#define VELOCITY_NAME(v, t) (((t) != patternTypeNote) ? SEGFAULT_POINTER : \
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
      doSetControllerStep(((p)), ((controllerUserStep_t *) (s)), (i)); \
    } else { \
      setSlide(((p)), ((noteUserStep_t *) (s)), FALSE, (i), NULL, TRUE, NULL); \
      unsetNoteStep((p), ((noteUserStep_t *) (s)), (i)); \
    } \
  } while (FALSE);

#define NR_USERSTEPS_PER_BAR(p) ((p)->stepsPerBar)
#define NR_BARS(p) ((p)->bars)
#define NR_STEPS_PER_BAR(p) ((p)->stepsPerBar)
#define NR_USERSTEPS(p) (NR_BARS((p)) * NR_USERSTEPS_PER_BAR((p)))
#define NR_EVENTSTEPS(p) \
  (NR_USERSTEPS((p)) * EVENTSTEPS_PER_USERSTEP(TYPE((p))))
#define CB_STEP(t) (((t) == patternTypeDummy) ? (cbDummyStep)  : \
  ((t) == patternTypeNote) ? (cbNoteStep)  : \
  (cbControllerStep))
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
#define NR_VELOCITIES(p) (!IS_NOTE((p)) ? SEGFAULT_NUMBER : \
  g_slist_length((VELOCITIES((p)))))
#define LOCKED_PTR(s, t) \
  (((t) == patternTypeDummy) ? &(((dummyUserStep_t *) (s))->locked)  : \
  ((t) == patternTypeNote) ? &(((noteUserStep_t *) (s))->locked) : \
  &(((controllerUserStep_t *) (s))->locked))
#define LOCKED(s, t) (*(LOCKED_PTR(s, t)))
#define EVENTSTEPS_PER_USERSTEP(t) (((t) == patternTypeNote) ? 2 : \
  ((t) == patternTypeController) ? 1 : \
  SEGFAULT_NUMBER)
#define MAX_BARS 512
#define MAX_EVENTSTEPS_PER_BAR 512
#define MAX_STEPS_PER_BAR(p) \
  (IS_DUMMY((p)) ? (MAX_EVENTSTEPS_PER_BAR / 1) : \
  (MAX_EVENTSTEPS_PER_BAR / (EVENTSTEPS_PER_USERSTEP(TYPE((p))))))

#define SET_STEP_FROM_STEP(p, s, t, i, lst, l, e) \
  do { \
    if (IS_DUMMY((p))) { \
      dummyUserStep_t *source = (dummyUserStep_t *) (s); \
      dummyUserStep_t *target = (dummyUserStep_t *) (t); \
      terror(setDummyStep((p), target, source->set, (l), (e))) \
    } else if (IS_CONTROLLER((p))) { \
      controllerUserStep_t *source = (controllerUserStep_t *) (s); \
      controllerUserStep_t *target = (controllerUserStep_t *) (t); \
      terror(setControllerStep((p), target, source->value, (i), (l), (e))) \
    } else { \
      noteUserStep_t *source = (noteUserStep_t *) (s); \
      noteUserStep_t *target = (noteUserStep_t *) (t); \
      terror(setNoteStep((p), target, source->value, \
	    source->velocity, (i), (l), FALSE, (e))) \
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
  volatile gboolean, \
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

VARIABLE ( \
  volatile GSList *, \
  midiMessages, \
  NULL \
)

VARIABLE( \
  struct { \
    uint8_t name; \
    gboolean sharpable; \
    int8_t midiValue; \
  }, \
  notes[7], \
  { \
    { NOTE_C COMMA TRUE COMMA 24 } COMMA \
    { NOTE_D COMMA TRUE COMMA 26 } COMMA \
    { NOTE_E COMMA FALSE COMMA 28 } COMMA \
    { NOTE_F COMMA TRUE COMMA 29 } COMMA \
    { NOTE_G COMMA TRUE COMMA 31 } COMMA \
    { NOTE_A COMMA TRUE COMMA 33 } COMMA \
    { NOTE_B COMMA FALSE COMMA 35 } \
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

void freeControllerValue(controllerValue_t *controllerValue);
void freeNoteValue(noteValue_t *noteValue);
void lockUserStep(pattern_t *pattern, uint32_t idx);
void lockSlide(pattern_t *pattern, uint32_t idx);
void setDummyStep(pattern_t *pattern, dummyUserStep_t *dummyUserStep,
  gboolean set, lockContext_t *lockContext, err_t *e);
void setNoteStep(pattern_t *pattern, noteUserStep_t *noteUserStep,
  GSList *value, GSList *velocity, uint32_t idx, lockContext_t *lockContext,
  gboolean live, err_t *e);
void setControllerStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, GSList *value,
  uint32_t idx, lockContext_t *lockContext, err_t *e);
void setSlide(pattern_t *pattern, noteUserStep_t *noteUserStep,
  gboolean slide, uint32_t idx, lockContext_t *lockContext,
  gboolean live, err_t *e);
#define freePattern(p) freePatternTODO((p), __LINE__)
void freePatternTODO(pattern_t *pattern, int line);
pattern_t *allocatePattern(pattern_t *parent);
noteValue_t *allocateNoteValue(void);
controllerValue_t *allocateControllerValue(void);
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
void allNotesOff(lockContext_t *lockContext, gboolean alreadyLocked, err_t *e);
void test(void);
void randomise(pattern_t *pattern, uint32_t bar, lockContext_t *lockContext);
gboolean getLocked(gboolean *unlockable,
  void *step, pattern_t *pattern, uint32_t idx);
void guiSignalStep(int step);
void guiSignalStop(void);
midiMessage_t *getNoteOffMidiMessage(noteEvent_t *noteEvent);
midiMessage_t *getControllerMidiMessage(uint8_t parameter, int8_t value);
void fireMidiMessage(lockContext_t *lockContext,
  midiMessage_t *midiMessage, err_t *e);

#ifdef __cplusplus
};
#endif

#endif

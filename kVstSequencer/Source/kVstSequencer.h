/* Copyright 2017 Martin Knappe (martin.knappe at gmail dot com) */

#ifndef _KVSTSEQUENCER_H
#define _KVSTSEQUENCER_H

#include <stdlib.h>
#include <kohaerenzstiftung.h>

#if 0
#define MARK() \
  do { \
    FILE *file = getOutFile(); \
    if (file != NULL) { \
      fprintf(file, "%s (%s:%d)\n",  __FUNCTION__, __FILE__, __LINE__); \
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

#define ZERO 0

#define CROTCHETS_PER_BAR 4
#define TICKS_PER_CROTCHET 24
#define TICKS_PER_BAR (TICKS_PER_CROTCHET * CROTCHETS_PER_BAR)

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
	noteOrControllerTypeNote,
	noteOrControllerTypeController
} noteOrControllerType_t;

typedef struct {
	noteOrControllerType_t noteOrControllerType;
	uint8_t channel;
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

/*
versionType_t and patternType_t have to be aliases
for serialisation reasons; for details, see loadStorePattern().
*/
typedef patternType_t versionType_t;
#define CURRENT_VERSION ((versionType_t) (patternTypeController + 1))

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

#define NR_BANKS 12

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

typedef struct pattern pattern_t;

struct noteEvent {
	volatile noteEventStep_t *noteEventStep;
	volatile noteValue_t *noteValue;
	volatile pattern_t *pattern;
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

struct pattern {
	patternType_t patternType;
	char *name;
	struct pattern *parent;
	volatile GSList *children;
	volatile uint32_t bars;
	volatile uint32_t stepsPerBar;
	volatile gboolean shufflingWithParent;
	volatile uint8_t shuffle;
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
};

typedef struct {
	uint8_t channel;
	double ppqPosition;
	noteOrControllerType_t noteOrControllerType;
	union {
		struct {
			noteEventStep_t *noteEventStep;
		} noteType;
		struct {
			pattern_t *pattern;
			controllerEventStep_t *controllerEventStep;
		} controllerType;
	};
} shuffled_t;

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

#define SHUFFLING_WITH_PARENT(p) ((p)->shufflingWithParent)
#define SHUFFLE(p) ((p)->shuffle)
#define ACTUAL_SHUFFLE(p) \
  SHUFFLING_WITH_PARENT((p)) ? getShuffle(PARENT((p))) : (p)->shuffle
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
      setSlide(((p)), ((noteUserStep_t *) (s)), FALSE, (i), NULL); \
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
#define EVENTSTEPS_PER_USERSTEP(t) (((t) == patternTypeNote) ? 8 : \
  ((t) == patternTypeController) ? 1 : \
  SEGFAULT_NUMBER)
#define MAX_BARS 512
#define MAX_EVENTSTEPS_PER_BAR 131072
#define BEATS_PER_BAR 4
#define QUAVERS_PER_BEAT 2
extern char dummy[((MAX_EVENTSTEPS_PER_BAR % BEATS_PER_BAR) == 0) ? 1 : -1];
#define MAX_EVENTSTEPS_PER_BEAT (MAX_EVENTSTEPS_PER_BAR / BEATS_PER_BAR)
#define MAX_EVENTSTEPS_PER_QUAVER (MAX_EVENTSTEPS_PER_BEAT / QUAVERS_PER_BEAT)
#define MAX_STEPS_PER_BAR(p) \
  (IS_DUMMY((p)) ? (MAX_EVENTSTEPS_PER_BAR / 1) : \
  (MAX_EVENTSTEPS_PER_BAR / (EVENTSTEPS_PER_USERSTEP(TYPE((p))))))

#define SET_STEP_FROM_STEP(p, s, t, i, lst, e) \
  do { \
    if (IS_DUMMY((p))) { \
      dummyUserStep_t *source = (dummyUserStep_t *) (s); \
      dummyUserStep_t *target = (dummyUserStep_t *) (t); \
      terror(setDummyStep((p), target, source->set, (e))) \
    } else if (IS_CONTROLLER((p))) { \
      controllerUserStep_t *source = (controllerUserStep_t *) (s); \
      controllerUserStep_t *target = (controllerUserStep_t *) (t); \
      terror(setControllerStep((p), target, source->value, (i), (e))) \
    } else { \
      noteUserStep_t *source = (noteUserStep_t *) (s); \
      noteUserStep_t *target = (noteUserStep_t *) (t); \
      terror(setNoteStep((p), target, source->value, \
	    source->velocity, (i), (e))) \
      terror(setSlide((p), target, (lst) ? FALSE : \
        source->slide, (i), (e))) \
      target->slideLocked = source->slideLocked; \
    } \
    LOCKED((t), TYPE((p))) = LOCKED((s), TYPE((p))); \
  } while (FALSE);


#ifdef _TABLE_C
#define VARIABLE(type, name, value) type name = value;
#else
#define VARIABLE(type, name, value) extern type name;
#endif

#define COMMA ,

VARIABLE( \
  volatile gboolean, \
  live, \
  FALSE \
)

VARIABLE ( \
  volatile int32_t, \
  velocity, \
  127 \
)

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
  volatile pattern_t *, \
  banks[NR_BANKS], \
  { NULL }
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

VARIABLE( \
  struct { \
    volatile GSList *value;
  }, \
  shuffledEvents,
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
  gboolean set, err_t *e);
void setNoteStep(pattern_t *pattern, noteUserStep_t *noteUserStep,
  GSList *value, GSList *velocity, uint32_t idx, err_t *e);
void setControllerStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, GSList *value,
  uint32_t idx, err_t *e);
void setSlide(pattern_t *pattern, noteUserStep_t *noteUserStep,
  gboolean slide, uint32_t idx, err_t *e);
void freePattern(pattern_t *pattern);
pattern_t *allocatePattern(pattern_t *parent);
noteValue_t *allocateNoteValue(void);
controllerValue_t *allocateControllerValue(void);
void setSteps(pattern_t *pattern);
void adjustSteps(pattern_t *pattern, uint32_t bars, uint32_t stepsPerBar,
  int32_t shift, err_t *e);
void deleteChild(pattern_t *parent, GSList *child,
  err_t *e);
void freeValue(pattern_t *pattern, void *value);
gboolean anyChildStepSet(pattern_t *pattern, uint32_t idx);
void unsoundPattern(pattern_t *pattern, err_t *e);
void unsoundAllPatterns(err_t *e);
void *output(void *param);
void *input(void *param);
void allNotesOff(err_t *e);
void randomise(pattern_t *pattern, uint32_t bar, uint8_t probability);
gboolean getLocked(gboolean *unlockable,
  void *step, pattern_t *pattern, uint32_t idx, gboolean onlyByParent);
void guiSignalStep(int step);
void guiSignalStop(void);
midiMessage_t *getNoteOffMidiMessage(noteEvent_t *noteEvent);
midiMessage_t *getControllerMidiMessage(uint8_t parameter,
  int8_t value, uint8_t channel);
void fireMidiMessage(midiMessage_t *midiMessage, err_t *e);
gboolean isAnyStepLockedByParent(pattern_t *pattern);
void promotePattern(pattern_t *pattern, err_t *e);
void loadStorePattern(pattern_t **pattern,
  void *stream, gboolean load, pattern_t *parent, err_t *e);
void setLive(pattern_t *newRoot, err_t *e);
void lock(gboolean deleteShuffled);
void unlock(void);
uint8_t getShuffle(pattern_t *pattern);

#ifdef __cplusplus
};
#endif

#endif

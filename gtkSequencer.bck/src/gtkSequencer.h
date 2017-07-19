#ifndef GTKSEQUENCER_H_
#define GTKSEQUENCER_H_

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdint.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include <kohaerenzstiftung.h>

pthread_mutex_t mutex;

#define MAX_EVENTSTEPS_PER_BAR 512
#define MAX_USERSTEPS_PER_BAR(p) \
  (MAX_EVENTSTEPS_PER_BAR / (EVENTSTEPS_PER_USERSTEP(p)))

#define TICKS_PER_CROTCHET 24
#define CROTCHETS_PER_BAR 4

#define EVENTSTEPS_PER_USERSTEP(p) \
  ((p->real.type == PATTERNTYPE_NOTE) ? NOTEEVENTSTEPS_PER_USERSTEP : \
  CONTROLLEREVENTSTEPS_PER_USERSTEP)

#define NUMBER_USERSTEPS(p) \
  (p->real.bars * \
  p->real.userStepsPerBar)
#define NUMBER_EVENTSTEPS(p) \
  ((NUMBER_USERSTEPS(p)) * (EVENTSTEPS_PER_USERSTEP(p)))

#define PATTERN_TYPE(p) (p->real.type)

#define ADDR_USER_STEPS(p) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? ((void **) &p->real.dummy.steps) : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? ((void **) &p->real.note.steps.user) : \
  ((void **) &p->real.controller.steps.user)

#define ADDR_EVENT_STEPS(p) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? NULL : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? ((void **) &p->real.note.steps.event) : \
  ((void **) &p->real.controller.steps.event)

#define SIZE_USERSTEP(p) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? sizeof(dummyUserStep_t) : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? sizeof(noteUserStep_t) : \
  sizeof(controllerUserStep_t)

#define SIZE_EVENTSTEP(p) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? 0 : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? sizeof(noteEventStep_t) : \
  sizeof(controllerEventStep_t)

#define USERSTEP(p, a, idx) \
  ((PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? ((void *) &(((dummyUserStep_t *) a)[idx])) : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? ((void *) &(((noteUserStep_t *) a)[idx])) : \
  ((void*) &(((controllerUserStep_t *) a)[idx])))

#define USERSTEP2(p, idx) \
  ((PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? USERSTEP(p, p->real.dummy.steps, idx) : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? USERSTEP(p, p->real.note.steps.user, idx) : \
  USERSTEP(p, p->real.controller.steps.user, idx))

#define IS_SET(p, s) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? ((dummyUserStep_t *) s)->set : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? (((noteUserStep_t *) s)->value != NULL) : \
  (((controllerUserStep_t *) s)->value != NULL)

#define STEPVALUE(p, s) \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? ((void *) (((noteUserStep_t *) s)->value)) : \
  ((void *) (((controllerUserStep_t *) s)->value))

#define CAN_RANDOMISE(p) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? TRUE : \
  (g_slist_length(GET_VALUES(p)) > 0)

#define GET_VALUES(p) \
  ((GSList *) ((PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? NULL : \
  (PATTERN_TYPE(p) == PATTERNTYPE_CONTROLLER) ? \
  p->real.controller.values : p->real.note.values))

#define VALUE_NAME(p, v) \
  ((p->real.type) == PATTERNTYPE_NOTE) ? \
  (char *) ((noteValue_t *) v)->name : (char *) ((controllerValue_t *) v)->name

#define ADDR_PATTERN_VALUES(p) \
  (p->real.type == PATTERNTYPE_NOTE) ? \
  (GSList **) &(p->real.note.values) : (GSList **) &(p->real.controller.values)

#define SET_STEP_FROM_STEP(p, s, t) \
  do { \
    if (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) { \
		dummyUserStep_t *source = s; \
		dummyUserStep_t *target = t; \
		setDummyStep((dummyUserStep_t *) target, source->set, lockCount); \
    } else if (PATTERN_TYPE(p) == PATTERNTYPE_CONTROLLER) { \
		controllerUserStep_t *source = s; \
		controllerUserStep_t *target = t; \
		setControllerStep((pattern_t *) p, target, \
		  (GSList *) source->value, lockCount); \
    } else { \
		noteUserStep_t *source = s; \
		noteUserStep_t *target = t; \
		setNoteStep(p, target, (GSList *) source->value, lockCount); \
		setNoteSlide((pattern_t *) p, target, source->slide, lockCount); \
    } \
  } while (FALSE);

#define CONTROLLEREVENTSTEPS_PER_USERSTEP 1
#define NOTEEVENTSTEPS_PER_USERSTEP 2
#define MAXEVENTSTEPS_PER_USERSTEP NOTEEVENTSTEPS_PER_USERSTEP

#define EVENTSTEPS_PER_BAR MAX_EVENTSTEPS_PER_BAR
#define ROOTSTEPS_PER_BAR EVENTSTEPS_PER_BAR
#define MICROTICKS_PER_BAR EVENTSTEPS_PER_BAR
#define TICKS_PER_BAR (TICKS_PER_CROTCHET * CROTCHETS_PER_BAR)



typedef enum {
	SYNCEVENTTYPE_TICK,
	SYNCEVENTTYPE_STOP
} syncEventType_t;

typedef struct syncEvent {
	volatile syncEventType_t syncEventType;
	volatile uint64_t at;
	volatile struct syncEvent *next;
} syncEvent_t;


typedef struct {
	volatile snd_seq_event_t *snd_seq_event;
	volatile char *name;
	volatile char tone;
	volatile int8_t octave;
	volatile gboolean sharp;
} noteValue_t;

typedef struct noteEventStep noteEventStep_t;

typedef struct noteEvent {
	volatile noteValue_t *noteValue;
	volatile noteEventStep_t *noteEventStep;
	union {
		struct {
			volatile struct noteEvent *offNoteEvent;
		} on;
		struct {
			volatile GSList *noteOffLink;
		} off;
	};
} noteEvent_t;



struct noteEventStep {
	volatile noteEvent_t *onNoteEvent;
	volatile noteEvent_t *offNoteEvent;
};

typedef enum {
	lockStatusFree = 0,
	lockStatusUnsettable,
	lockStatusUnunsettable,
	lockStatusUserlocked
} lockStatus_t;

typedef struct {
	volatile lockStatus_t lockStatus;
	volatile lockStatus_t slideLockStatus;
	volatile gboolean slide;
	volatile GSList *value;
	volatile noteEvent_t *onNoteEvent;
} noteUserStep_t;



typedef struct {
	volatile snd_seq_event_t *event;
	volatile char *name;
	volatile uint8_t value;
} controllerValue_t;

typedef struct {
	volatile snd_seq_event_t *value;
} controllerEventStep_t;

typedef struct {
	volatile lockStatus_t lockStatus;
	volatile GSList *value;
	volatile snd_seq_event_t **event;
} controllerUserStep_t;

typedef struct {
	volatile lockStatus_t lockStatus;
	volatile gboolean set;
} dummyUserStep_t;

typedef enum {
	PATTERNTYPE_DUMMY,
	PATTERNTYPE_NOTE,
	PATTERNTYPE_CONTROLLER
} patternType_t;

typedef struct pattern {
	volatile gboolean isRoot;
	volatile GSList *children;

	union {
		struct {
			//TODO
		} root;
		struct {
			volatile struct pattern *parent;
			volatile char *name;
			volatile uint8_t channel;

			struct {
				volatile uint32_t userStepsPerBar;
				volatile uint32_t bars;
			};

			volatile patternType_t type;

			union {
				struct {
					volatile dummyUserStep_t *steps;
				} dummy;
				struct {
					volatile GSList *values;
					struct {
						volatile noteEventStep_t *event;
						volatile noteUserStep_t *user;
					} steps;
				} note;
				struct {
					volatile uint8_t parameter;
					volatile GSList *values;
					struct {
						volatile controllerEventStep_t *event;
						volatile controllerUserStep_t *user;
					} steps;
				} controller;
			};
		} real;
	};
} pattern_t;


#ifndef _TABLE_C
#define VARIABLE extern
#define INITIALIZED_VARIABLE(volatile, type, name, value) extern volatile type name;
#else
#define VARIABLE
#define INITIALIZED_VARIABLE(volatile, type, name, value) volatile type name = value;
#endif
/*+++++++++++++++++++++++++++ VARIABLE DEFINITIONS +++++++++++++++++++++++++++*/

VARIABLE snd_seq_addr_t porto;

VARIABLE struct {
	snd_seq_t *value;
	gboolean connected;
	int client;
	int port;
	int myPort;
} sequencer;

/* volatile */ VARIABLE pattern_t rootPattern;

volatile VARIABLE struct {
	volatile pthread_cond_t wakeupConsumers;
	volatile pthread_mutex_t mutex;
	struct {
		volatile syncEvent_t *head;
		volatile syncEvent_t *tail;
		volatile syncEvent_t *last;
	} queue;
} syncEvents;

VARIABLE struct {
	pthread_t input;
	pthread_t output;
} threads;

VARIABLE struct {
	volatile GSList *value;
} notesOff;

INITIALIZED_VARIABLE(volatile , long, subtractSeconds, 0)
INITIALIZED_VARIABLE(volatile, gboolean, goingDown, FALSE)
INITIALIZED_VARIABLE(volatile, uint32_t, anticipation, 0)
INITIALIZED_VARIABLE(volatile, uint32_t, maxSleepNanos, -1)
INITIALIZED_VARIABLE(volatile , noteEvent_t, *pendingOff, NULL)


/*++++++++++++++++++++++++++++++++ FUNCTIONS +++++++++++++++++++++++++++++++++*/

void *outputFunction(void *param);
void *inputFunction(void *param);
void gtkFunction(int argc, char *argv[]);
void guiTest(uint32_t iterations);
void gtkSignalStep(uint64_t step);
void gtkSignalStop();
void gtkSignalSpeed(uint64_t nanosecondsPerCrotchet);
void gtkInit(void);
void gtkPrepareShutdown(void);

#endif /* GTKSEQUENCER_H_ */

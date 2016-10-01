#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>

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
  (p->real.type == PATTERNTYPE_NOTE) ? NOTEEVENTSTEPS_PER_USERSTEP : \
  CONTROLLEREVENTSTEPS_PER_USERSTEP

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
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? ((void *) &(((dummyUserStep_t *) a)[idx])) : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? ((void *) &(((noteUserStep_t *) a)[idx])) : \
  ((void*) &(((controllerUserStep_t *) a)[idx]))

#define USERSTEP2(p, idx) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? USERSTEP(p, p->real.dummy.steps, idx) : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? USERSTEP(p, p->real.note.steps.user, idx) : \
  USERSTEP(p, p->real.controller.steps.user, idx)

#define IS_SET(p, s) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? ((dummyUserStep_t *) s)->set : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? (((noteUserStep_t *) s)->value != NULL) : \
  (((controllerUserStep_t *) s)->value != NULL)

#define STEPVALUE(p, s) \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? ((void *) (((noteUserStep_t *) s)->value)) : \
  ((void *) (((controllerUserStep_t *) s)->value))

#define CAN_RANDOMISE(p) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? TRUE : \
  (PATTERN_TYPE(p) == PATTERNTYPE_CONTROLLER) ? \
  (g_slist_length(p->real.controller.values) > 0) : \
  (g_slist_length(p->real.note.values) > 0)

#define VALUE_NAME(p, v) \
  (p->real.type == PATTERNTYPE_NOTE) ? \
  ((noteValue_t *) v)->name : ((controllerValue_t *) v)->name

#define ADDR_PATTERN_VALUES(p) \
  (p->real.type == PATTERNTYPE_NOTE) ? \
  &(p->real.note.values) : &(p->real.controller.values)

#define SET_STEP_FROM_STEP(p, s, t) \
  do { \
    if (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) { \
		dummyUserStep_t *source = s; \
		dummyUserStep_t *target = t; \
		setDummyStep(target, source->set); \
    } else if (PATTERN_TYPE(p) == PATTERNTYPE_CONTROLLER) { \
		controllerUserStep_t *source = s; \
		controllerUserStep_t *target = t; \
		setControllerStep(current.pattern, target, source->value); \
    } else { \
		noteUserStep_t *source = s; \
		noteUserStep_t *target = t; \
		setNoteStep(p, target, source->value); \
		setNoteSlide(p, target, source->slide); \
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
	syncEventType_t syncEventType;
	uint64_t at;
	struct syncEvent *next;
} syncEvent_t;


typedef struct {
	snd_seq_event_t *snd_seq_event;
	char *name;
	char tone;
	int8_t octave;
	gboolean sharp;
} noteValue_t;

//snd_seq_ev_set_noteon(result->note.on, 0, note, velocity);
//snd_seq_ev_set_noteoff(result->note.off, 0, note, velocity);

typedef struct noteEventStep noteEventStep_t;

typedef struct noteEvent {
	noteValue_t *noteValue;
	noteEventStep_t *noteEventStep;
	union {
		struct {
			struct noteEvent *offNoteEvent;
		} on;
		struct {
			GSList *noteOffLink;
		} off;
	};
} noteEvent_t;



struct noteEventStep {
	noteEvent_t *onNoteEvent;
	noteEvent_t *offNoteEvent;
};


typedef struct {
	gboolean slide;
	GSList *value;
	noteEvent_t *onNoteEvent;
} noteUserStep_t;



typedef struct {
	snd_seq_event_t *event;
	char *name;
	uint8_t value;
} controllerValue_t;

typedef struct {
	snd_seq_event_t *value;
} controllerEventStep_t;

typedef struct {
	GSList *value;
	snd_seq_event_t **event;
} controllerUserStep_t;

typedef struct {
	gboolean set;
} dummyUserStep_t;

typedef enum {
	PATTERNTYPE_DUMMY,
	PATTERNTYPE_NOTE,
	PATTERNTYPE_CONTROLLER
} patternType_t;

typedef struct pattern {
	gboolean isRoot;
	GSList *children;

	union {
		struct {
			//TODO
		} root;
		struct {
			struct pattern *parent;
			char *name;
			uint8_t channel;

			struct {
				uint32_t userStepsPerBar;
				uint32_t bars;
			};

			patternType_t type;

			union {
				struct {
					dummyUserStep_t *steps;
				} dummy;
				struct {
					GSList *values;
					struct {
						noteEventStep_t *event;
						noteUserStep_t *user;
					} steps;
				} note;
				struct {
					uint8_t parameter;
					GSList *values;
					struct {
						controllerEventStep_t *event;
						controllerUserStep_t *user;
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

VARIABLE snd_seq_addr_t port;

VARIABLE struct {
	snd_seq_t *value; //ok
} sequencer;

/* volatile */ VARIABLE pattern_t rootPattern;
volatile VARIABLE uint32_t adjustment;

volatile VARIABLE struct {
	pthread_cond_t wakeupConsumers;
	pthread_mutex_t mutex;
	struct {
		syncEvent_t *head;
		syncEvent_t *tail;
		syncEvent_t *last;
	} queue;
} syncEvents;

VARIABLE struct {
	pthread_t input;
	pthread_t output;
} threads;

volatile VARIABLE struct {
	GSList *value;
} notesOff;

INITIALIZED_VARIABLE(, long, subtractSeconds, 0)
INITIALIZED_VARIABLE(volatile, gboolean, goingDown, FALSE)


/*++++++++++++++++++++++++++++++++ FUNCTIONS +++++++++++++++++++++++++++++++++*/

void *outputFunction(void *param);
void *inputFunction(void *param);
void gtkFunction(int argc, char *argv[]);
void gtkSignalStep(uint64_t step);
void gtkSignalStop();
void gtkSignalSpeed(uint64_t nanosecondsPerCrotchet);
void gtkInit(void);
void gtkPrepareShutdown(void);
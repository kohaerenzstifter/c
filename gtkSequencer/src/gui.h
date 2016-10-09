#ifndef GUI_H_
#define GUI_H_

#include "gtkSequencer.h"

#define LOCK() \
  do { \
	if (lockCount < 1) { \
	  pthread_mutex_lock(&mutex); \
	} \
	lockCount++; \
  } while (FALSE);

#define UNLOCK() \
  do { \
	lockCount--; \
	if (lockCount == 0) { \
	  pthread_mutex_unlock(&mutex); \
	} \
  } while (FALSE);

extern void setControllerStep(pattern_t *pattern,
  controllerUserStep_t *step, GSList *value, uint32_t lockCount);
extern void setDummyStep(dummyUserStep_t *step, gboolean value, uint32_t lockCount);
extern void setNoteStep(pattern_t *pattern, noteUserStep_t *step, GSList *value, uint32_t lockCount);
extern void setNoteSlide(pattern_t *pattern, noteUserStep_t *step, gboolean slide, uint32_t lockCount);
extern void setSteps(pattern_t *pattern);
extern void adjustSteps(pattern_t *pattern, uint32_t bars, uint32_t userStepsPerBar, uint32_t lockCount);
extern snd_seq_event_t *getAlsaEvent(void);
extern void setAlsaNoteEvent(uint8_t channel, noteValue_t *noteValue);
extern pattern_t *allocatePattern(pattern_t *parent);
extern pattern_t *createRealPattern2(pattern_t *parent, const gchar *name, gint channel,
  patternType_t patternType, gint controller);
extern void setupNoteValue(pattern_t  *pattern, const char *name, gboolean sharp,
  int8_t octave, char tone, noteValue_t *noteValue);
extern void unsoundPattern(pattern_t *pattern);


#endif /* GUI_H_ */
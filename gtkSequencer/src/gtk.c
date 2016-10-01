#include "gtkSequencer.h"

static struct {
	GtkWidget *window;

	GtkWidget *barsButton;
	GtkWidget *userStepsPerBarButton;
	GtkWidget *valuesButton;
	GtkWidget *randomiseButton;
	GtkWidget *previousButton;
	GtkWidget *nextButton;
	GtkWidget *parentButton;
	GtkWidget *topButton;
	GtkWidget *childrenButton;
	GtkWidget *siblingsButton;

	GtkWidget *upperArea;
	GtkWidget *middleArea;
	GtkWidget *lowerArea;

	GtkWidget *stepsView;
	GtkWidget *speedLabel;
	GtkWidget *adjustScale;
	GtkWidget *loadButton;
	GtkWidget *storeButton;
} appFrame;


static uint32_t lockCount = 0;

static struct {
	pattern_t *pattern;
	uint32_t bar;
} current = {
	.pattern = (pattern_t *) &rootPattern,
	.bar = 0
};


static GtkWidget *dialog = NULL;

static struct {
	uint64_t step;
	pattern_t *pattern;
	uint64_t generation;
	uint64_t lastGeneration;
	GtkWidget *lastLabel;
	pthread_mutex_t mutex;
	uint32_t bar;
	GSList *labels;
	uint32_t userStepsPerBar;
	uint32_t bars;
} signalling = {
	.step = 0,
	.pattern = NULL,
	.generation = 0,
	.lastGeneration = 0,
	.lastLabel = NULL,
	.labels = NULL,
	.bar = 0
};

static uint64_t nanosecondsPerCrotchet = -1;

static void unsignalStep()
{
	if (signalling.lastLabel == NULL) {
		goto finish;
	}
	if (signalling.generation != signalling.lastGeneration) {
		goto finish;
	}
	gtk_label_set_label(GTK_LABEL(signalling.lastLabel), "");

finish:
	return;
}

static gboolean doSignalStep(gpointer user_data)
{
	uint64_t minStep = 0;
	uint64_t maxStep = 0;
	uint64_t step = signalling.step;

	if (signalling.labels == NULL) {
		goto finish;
	}

	step /= (MAX_EVENTSTEPS_PER_BAR / signalling.userStepsPerBar);
	step %= (signalling.userStepsPerBar * signalling.bars);
	minStep = signalling.bar * signalling.userStepsPerBar;
	maxStep = minStep + signalling.userStepsPerBar - 1;

	if ((step < minStep) || (step > maxStep)) {
		goto finish;
	}

	unsignalStep();
	signalling.lastLabel = g_slist_nth_data(signalling.labels, step);
	gtk_label_set_label(GTK_LABEL(signalling.lastLabel), "X");
	signalling.lastGeneration = signalling.generation;
finish:

	return TRUE;
}

void gtkPrepareShutdown(void)
{
	signalling.labels = NULL;
	signalling.lastLabel = NULL;
}

static gboolean doSignalStop(gpointer user_data)
{
	unsignalStep();
	return TRUE;
}

static gboolean doSignalSpeed(gpointer user_data)
{
	static char speedString[20];
	uint32_t speed;

	if (nanosecondsPerCrotchet > 0) {
		speed = (uint32_t) (((((long double) 1000000000) /
		  ((long double) nanosecondsPerCrotchet))) * 60);
		snprintf(speedString, sizeof(speedString), "%u", speed);
	} else {
		speedString[0] = '\0';
	}

	gtk_label_set_text(GTK_LABEL(appFrame.speedLabel), speedString);

	return TRUE;
}

void gtkSignalSpeed(uint64_t nPC)
{
	nanosecondsPerCrotchet = nPC;
	g_idle_add(doSignalSpeed, NULL);
}

void gtkSignalStep(uint64_t step)
{
	pthread_mutex_lock(&(signalling.mutex));
	signalling.step = step;
	pthread_mutex_unlock(&(signalling.mutex));

	g_idle_add(doSignalStep, NULL);
}

void gtkSignalStop()
{
	g_idle_add(doSignalStop, NULL);
}

static void destroyDialog(void)
{
	if (dialog != NULL) {
		gtk_widget_destroy(dialog);
		dialog = NULL;
	}
}

static GtkWidget *getDialog(gint width, gint height)
{
	GtkWidget *result = gtk_dialog_new();

	gtk_widget_set_size_request(result, width, height);
	gtk_window_set_modal(GTK_WINDOW(result), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(result), TRUE);
	/*NOTE: gtk_dialog_new_with_buttons() and other convenience functions in
	 * GTK+ will sometimes call gtk.window_set_transient_for() on your behalf.*/
	gtk_window_set_transient_for(GTK_WINDOW(result),
	  GTK_WINDOW(appFrame.window));

	g_signal_connect(result, "delete_event", G_CALLBACK(destroyDialog), NULL);

	return result;
}

static gint runDialog(GtkWidget *runMe)
{
	destroyDialog();
	dialog = runMe;
	return gtk_dialog_run(GTK_DIALOG(runMe));
}


static void renderRootPattern()
{
	gtk_window_set_title(GTK_WINDOW(appFrame.window), "ROOT");

	gtk_widget_set_sensitive(appFrame.barsButton, FALSE);
	gtk_widget_set_sensitive(appFrame.userStepsPerBarButton, FALSE);
	gtk_widget_set_sensitive(appFrame.valuesButton, FALSE);
	gtk_widget_set_sensitive(appFrame.randomiseButton, FALSE);
	gtk_widget_set_sensitive(appFrame.previousButton, FALSE);
	gtk_widget_set_sensitive(appFrame.nextButton, FALSE);
	gtk_widget_set_sensitive(appFrame.parentButton, FALSE);
	gtk_widget_set_sensitive(appFrame.topButton, FALSE);
	gtk_widget_set_sensitive(appFrame.siblingsButton, FALSE);

	gtk_widget_show_now(appFrame.middleArea);
}

static GtkWidget *getBox(GtkOrientation orientation)
{
	GtkWidget *result = gtk_box_new(orientation, 0);

	gtk_box_set_homogeneous(GTK_BOX(result), TRUE);

	return result;
}

static void createStepsView(void)
{
	if (appFrame.stepsView != NULL) {
		gtk_widget_destroy(appFrame.stepsView);
	}
	appFrame.stepsView = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(appFrame.middleArea), appFrame.stepsView, TRUE, TRUE, 0);
}



static GtkWidget *getSpinButton(gdouble min, gdouble max, gdouble step)
{
	GtkWidget *result = NULL;

	result = gtk_spin_button_new_with_range(min, max, step);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(result), TRUE);

	return result;
}

static GtkWidget *getButton(char *text,
  void (*onClick)(GtkWidget *widget, gpointer data), gpointer data)
{
	GtkWidget *result = NULL;

	result = gtk_button_new_with_label(text);

	if (onClick != NULL) {
		g_signal_connect(G_OBJECT(result), "clicked", G_CALLBACK(onClick), data);
	}

	return result;
}

static struct {
	GtkWidget *label;
	GtkWidget *upButton;
	GtkWidget *downButton;
	void (*pickValue)(int type);
} numberpicker;

#define PICK_DUMMY 0
#define PICK_UP 1
#define PICK_DOWN 2



static void pickUp(GtkWidget *widget, gpointer data)
{
	numberpicker.pickValue(PICK_UP);
}

static void pickLess(GtkWidget *widget, gpointer data)
{
	numberpicker.pickValue(PICK_DOWN);
}

static char *intToString(int32_t number)
{
	static char string[11];

	snprintf(string, sizeof(string), "%d", number);

	return string;
}

static char *charToString(char c)
{
	static char string[2];

	snprintf(string, sizeof(string), "%c", c);

	return string;
}

static void addNumberPicker(GtkWidget *contentArea, int initial)
{
	GtkWidget *box = NULL;

	box = getBox(GTK_ORIENTATION_HORIZONTAL);
	numberpicker.label = gtk_label_new(intToString(initial));
	numberpicker.upButton = getButton("+", pickUp, NULL);
	numberpicker.downButton = getButton("-", pickLess, NULL);

	gtk_box_pack_start(GTK_BOX(box), numberpicker.downButton, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), numberpicker.label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), numberpicker.upButton, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);

	numberpicker.pickValue(PICK_DUMMY);

}

static void renderPattern();

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

static void setDummyStep(dummyUserStep_t *step, gboolean value)
{
	LOCK();

	step->set = value;

	UNLOCK();

}

static void onDummyStep(GtkWidget *widget, gpointer data)
{
	dummyUserStep_t *step = data;

	setDummyStep(step, (!step->set));

	renderPattern();
}

static snd_seq_event_t *getAlsaEvent()
{
	snd_seq_event_t *result = calloc(1, sizeof(snd_seq_event_t));

	snd_seq_ev_clear(result);
	snd_seq_ev_set_direct(result);
	result->dest = port;

	return result;
}

static void setControllerStep(pattern_t *pattern,
  controllerUserStep_t *step, GSList *value)
{
	controllerValue_t *controllerValue = NULL;
	uint32_t idx = step - pattern->real.controller.steps.user;

	LOCK();

	step->value = value;

	if (value == NULL) {
		step->event = NULL;
		goto finish;
	}

	controllerValue = value->data;
	step->event = &(pattern->real.controller.steps.event[idx].value);
	*step->event = controllerValue->event;

finish:
	UNLOCK();
}

static void onControllerStep(GtkWidget *widget, gpointer data)
{
	controllerUserStep_t *step = data;

	GSList *value = (step->value == NULL) ? current.pattern->real.controller.values :
	  g_slist_next(step->value);

	setControllerStep(current.pattern, step, value);
	renderPattern();
}

static void unsoundNoteEvent(noteEvent_t *off)
{
	if (off->off.noteOffLink == NULL) {
		goto finish;
	}

	off->noteValue->snd_seq_event->type = SND_SEQ_EVENT_NOTEOFF;
	snd_seq_event_output(sequencer.value, off->noteValue->snd_seq_event);
	snd_seq_drain_output(sequencer.value);
	notesOff.value =
	  g_slist_delete_link(notesOff.value, off->off.noteOffLink);
	off->off.noteOffLink = NULL;

finish:
	return;
}

static void redirect(uint32_t startIdx, noteEvent_t *cmp, noteEvent_t *set)
{
	uint32_t numberSteps =
	  (current.pattern->real.userStepsPerBar *
	  current.pattern->real.bars);

	for (uint32_t i = startIdx; i < numberSteps; i++) {
		if (current.pattern->real.note.steps.user[i].onNoteEvent != cmp) {
			break;
		}
		current.pattern->real.note.steps.user[i].onNoteEvent = set;
	}
}

static void unsetNoteStep(pattern_t *pattern, noteUserStep_t *step)
{
	uint32_t userIdx = step - pattern->real.note.steps.user;
	uint32_t eventIdx = userIdx * NOTEEVENTSTEPS_PER_USERSTEP;
	noteUserStep_t *previous = (userIdx < 1) ? NULL :
	  &pattern->real.note.steps.user[userIdx - 1];
	noteUserStep_t *next =
	  (userIdx >= (pattern->real.userStepsPerBar - 1)) ? NULL :
	  &pattern->real.note.steps.user[userIdx + 1];

	void moveStepOnToNext() {
		noteEventStep_t *isNoteOnEventStep = step->onNoteEvent->noteEventStep;
		noteEventStep_t *mustNoteOnEventStep =
		  isNoteOnEventStep + NOTEEVENTSTEPS_PER_USERSTEP;

		step->onNoteEvent->noteEventStep = mustNoteOnEventStep;

		mustNoteOnEventStep->onNoteEvent = isNoteOnEventStep->onNoteEvent;
		isNoteOnEventStep->onNoteEvent = NULL;
	}

	void removeNote() {
		step->onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent = NULL;
		free(step->onNoteEvent->on.offNoteEvent);
		step->onNoteEvent->noteEventStep->onNoteEvent = NULL;
		free(step->onNoteEvent);
	}

	void previousNoteOff() {
		noteEvent_t *noteEvent = previous->onNoteEvent->on.offNoteEvent;

		step->onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent = NULL;

		noteEvent->noteEventStep =
		  &(pattern->real.note.steps.event[eventIdx]);
		noteEvent->noteEventStep->offNoteEvent = noteEvent;
	}

	void nextNoteOn(noteEventStep_t *offStep, noteValue_t *noteValue) {
		noteEvent_t *onNoteEvent = NULL;
		noteEvent_t *noteOffEvent = NULL;

		noteEventStep_t *eventStep =
		  &(pattern->real.note.steps.event[eventIdx]);
		noteEventStep_t *mustStep = eventStep + NOTEEVENTSTEPS_PER_USERSTEP;

		onNoteEvent = calloc(1, sizeof(noteEvent_t));
		onNoteEvent->noteValue = noteValue;
		onNoteEvent->noteEventStep = mustStep;

		noteOffEvent = calloc(1, sizeof(noteEvent_t));
		noteOffEvent->noteValue = noteValue;
		noteOffEvent->noteEventStep = offStep;

		onNoteEvent->on.offNoteEvent = noteOffEvent;

		redirect((userIdx + 1), next->onNoteEvent, onNoteEvent);

		mustStep->onNoteEvent = onNoteEvent;
		offStep->offNoteEvent = noteOffEvent;
		eventStep->onNoteEvent = NULL;

	}

	if (step->onNoteEvent == NULL) {
		goto finish;
	}

	if ((previous == NULL)||(step->onNoteEvent != previous->onNoteEvent)) {
		if ((next != NULL)&&(step->onNoteEvent == next->onNoteEvent)) {
			moveStepOnToNext();
		} else {
			removeNote();
		}
	} else {
		noteEventStep_t *offStep = previous->onNoteEvent->on.offNoteEvent->noteEventStep;
		previousNoteOff();
		if ((next != NULL)&&(step->onNoteEvent == next->onNoteEvent)) {
			nextNoteOn(offStep, step->value->data);
		}
	}


finish:
	step->onNoteEvent = NULL;
}

static void doSetNoteStep(pattern_t *pattern, noteUserStep_t *noteUserStep)
{
	noteEvent_t *onNoteEvent = NULL;
	noteEvent_t *offNoteEvent = NULL;
	noteValue_t *noteValue = noteUserStep->value->data;
	uint32_t userIdx = noteUserStep - pattern->real.note.steps.user;
	uint32_t eventIdx = userIdx * NOTEEVENTSTEPS_PER_USERSTEP;
	noteUserStep_t *previous = (userIdx < 1) ? NULL :
	  &pattern->real.note.steps.user[userIdx - 1];
	noteUserStep_t *next =
	  (userIdx >= (pattern->real.userStepsPerBar - 1)) ? NULL :
	  &pattern->real.note.steps.user[userIdx + 1];

	if ((previous != NULL)&&(previous->slide)&&
	  (previous->value != NULL)) {
		noteValue_t *previousNoteValue = previous->value->data;
		if ((noteValue->tone == previousNoteValue->tone)&&
		  (noteValue->sharp == previousNoteValue->sharp)&&
		  (noteValue->octave == previousNoteValue->octave)) {
			onNoteEvent = previous->onNoteEvent;
			onNoteEvent->on.offNoteEvent->noteEventStep->offNoteEvent = NULL;
			free(onNoteEvent->on.offNoteEvent);
		}
	}

	if (onNoteEvent == NULL) {
		onNoteEvent = calloc(1, sizeof(noteEvent_t));
		onNoteEvent->noteEventStep =
		  &(pattern->real.note.steps.event[eventIdx]);
		onNoteEvent->noteEventStep->onNoteEvent = onNoteEvent;
		onNoteEvent->noteValue = noteUserStep->value->data;
	}

	if ((next != NULL)&&(noteUserStep->slide)&&
	  (next->value != NULL)) {
		noteValue_t *nextNoteValue = next->value->data;
		if ((noteValue->tone == nextNoteValue->tone)&&
		  (noteValue->sharp == nextNoteValue->sharp)&&
		  (noteValue->octave == nextNoteValue->octave)) {
			offNoteEvent = next->onNoteEvent->on.offNoteEvent;
			next->onNoteEvent->noteEventStep->onNoteEvent = NULL;
			free(next->onNoteEvent);
			redirect((userIdx + 1), next->onNoteEvent, onNoteEvent);
		}
	}
	if (offNoteEvent == NULL) {
		offNoteEvent = calloc(1, sizeof(noteEvent_t));
		eventIdx += NOTEEVENTSTEPS_PER_USERSTEP;
		if (!noteUserStep->slide) {
			eventIdx--;
		}
		offNoteEvent->noteEventStep =
		  &(pattern->real.note.steps.event[eventIdx]);
		offNoteEvent->noteEventStep->offNoteEvent = offNoteEvent;
		offNoteEvent->noteValue = noteUserStep->value->data;
	}

	noteUserStep->onNoteEvent = onNoteEvent;
	onNoteEvent->on.offNoteEvent = offNoteEvent;
}

static void unsoundPattern()
{
	if (current.pattern->real.type != PATTERNTYPE_NOTE) {
		goto finish;
	}
	for (uint32_t i = 0; i < current.pattern->real.bars; i++) {
		for (uint32_t j = 0; j < current.pattern->real.userStepsPerBar; j++) {
			uint32_t idx = (i * current.pattern->real.userStepsPerBar) + j;
			if (current.pattern->real.note.steps.event[idx].onNoteEvent == NULL) {
				continue;
			}
			unsoundNoteEvent(current.pattern->real.note.steps.event[idx].onNoteEvent->on.offNoteEvent);
		}
	}

finish:
	return;
}

static void setNoteStep(pattern_t *pattern, noteUserStep_t *step, GSList *value)
{
	gboolean slide = step->slide;

	LOCK();

	unsoundPattern();

	unsetNoteStep(pattern, step);

	step->value = value;
	step->slide = (value != NULL) ? slide : FALSE;

	if (step->value == NULL) {
		goto finish;
	}

	doSetNoteStep(pattern, step);

finish:
	UNLOCK();
}

static void onNoteStep(GtkWidget *widget, gpointer data)
{
	noteUserStep_t *step = data;
	GSList *value = (step->value == NULL) ? current.pattern->real.note.values :
	  g_slist_next(step->value);

	setNoteStep(current.pattern, step, value);

	renderPattern();
}


static void setNoteSlide(pattern_t *pattern, noteUserStep_t *step, gboolean slide)
{
	GSList *value = step->value;

	LOCK();

	unsoundPattern();

	unsetNoteStep(pattern, step);

	step->value = value;
	step->slide = slide;

	if (step->value == NULL) {
		goto finish;
	}

	doSetNoteStep(pattern, step);

finish:
	UNLOCK();
}

static void onNoteSlide(GtkWidget *widget, gpointer data)
{
	noteUserStep_t *step = data;

	setNoteSlide(current.pattern, step, (!step->slide));

	renderPattern();
}

static GdkColor stepButtonColor = {0, 0xffff, 0xffff, 0xffff};

static void setColor(GtkWidget *widget, guint16 color)
{
	gtk_style_context_add_class(gtk_widget_get_style_context(widget), "colorable");
	//stepButtonColor.red = color;
	stepButtonColor.green = color;
	//stepButtonColor.blue = color;
	gtk_widget_modify_bg(widget, GTK_STATE_ACTIVE, &stepButtonColor);
	gtk_widget_modify_bg(widget, GTK_STATE_PRELIGHT, &stepButtonColor);
	gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &stepButtonColor);
}

static void setColor2(GtkWidget *widget, uint32_t i,
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades)
{
	uint32_t maxIndex = (shadesSize - 1);
	uint32_t index = maxIndex;

	if (stepsPerBar <= 4) {
		goto haveIndex;
	}
	index = 0;
	while ((index < maxIndex)&&((i % 2) == 0)) {
		index++;
		i /= 2;
	}

haveIndex:
	setColor(widget, shades[index]);
}

static GtkWidget *getStepLabel(uint32_t i,
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades)
{
	GtkWidget *result = gtk_label_new(intToString(i + 1));

	setColor2(result, i, shadesSize, stepsPerBar, shades);

	return result;
}

static GtkWidget *addVerticalStepBox(uint32_t i,
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades)
{
	GtkWidget *result = NULL;
	GtkWidget *label = NULL;

	result = getBox(GTK_ORIENTATION_VERTICAL);
	gtk_container_add(GTK_CONTAINER(appFrame.stepsView), result);

	label = getStepLabel(i, shadesSize, stepsPerBar, shades);
	gtk_container_add(GTK_CONTAINER(result), label);

	label = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(result), label);
	signalling.labels = g_slist_append(signalling.labels, label);

	return result;
}

static void addDummyStep(dummyUserStep_t *step, uint32_t i,
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades,
  gboolean enabled)
{
	GtkWidget *button = NULL;
	GtkWidget *verticalBox = NULL;
	char *text = !step->set ? "" : "X";

	verticalBox = addVerticalStepBox(i, shadesSize, stepsPerBar, shades);
	button = getButton(text, onDummyStep, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);

	gtk_widget_set_sensitive(button, enabled);
}

static void addNoteStep(noteUserStep_t *step, uint32_t i,
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades,
  gboolean enabled, gboolean last)
{
	GtkWidget *stepButton = NULL;
	GtkWidget *slideButton = NULL;
	GtkWidget *verticalBox = NULL;
	char *stepText = (step->value == NULL) ? "" :
	  ((noteValue_t *) (step->value->data))->name;
	char *slideText = (step->value == NULL) ? "-" :
	  step->slide ? "!SLIDE" : "SLIDE";

	verticalBox = addVerticalStepBox(i, shadesSize, stepsPerBar, shades);
	stepButton = getButton(stepText, onNoteStep, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), stepButton);
	slideButton = getButton(slideText, onNoteSlide, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), slideButton);
	gtk_widget_set_sensitive(stepButton, enabled);
	enabled = (enabled && (step->value != NULL));
	gtk_widget_set_sensitive(slideButton, (!last)&&enabled);
}

static void addControllerStep(controllerUserStep_t *step, uint32_t i,
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades,
  gboolean enabled)
{
	GtkWidget *button = NULL;
	GtkWidget *verticalBox = NULL;
	char *text = (step->value == NULL) ? "" :
	  ((controllerValue_t *) (step->value->data))->name;

	verticalBox = addVerticalStepBox(i, shadesSize, stepsPerBar, shades);
	button = getButton(text, onControllerStep, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);

	gtk_widget_set_sensitive(button, enabled);
}


void *getParentStep(pattern_t *pattern, uint32_t stepIdx)
{
	void *result = NULL;
	pattern_t *parent = pattern->real.parent;
	uint32_t factor = 0;
	uint32_t steps = 0;

	if (parent->isRoot) {
		goto finish;
	}

	factor = pattern->real.userStepsPerBar / parent->real.userStepsPerBar;

	stepIdx /= factor;

	steps = parent->real.userStepsPerBar * parent->real.bars;

	stepIdx %= steps;

	result = USERSTEP2(parent, stepIdx);
finish:
	return result;
}

#define addStep(p, s, i, ss, spb, sh, e, l) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? \
  addDummyStep((dummyUserStep_t *) s, i, ss, spb, sh, e) : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? \
  addNoteStep((noteUserStep_t *) s, i, ss, spb, sh, e, l) : \
  addControllerStep((controllerUserStep_t *) s, i, ss, spb, sh, e)

static void renderRealPattern()
{
	char title[500];
	uint32_t userStepsPerBar =
	  current.pattern->real.userStepsPerBar;
	uint32_t stepIdx = current.bar * userStepsPerBar;
	uint32_t shadesSize = 0;
	uint32_t shadeStep = 0;
	int32_t value = 0;
	guint16 *shades = NULL;
	uint32_t maxBar = (current.pattern->real.bars - 1);
	uint32_t maxStep = (current.pattern->real.userStepsPerBar - 1);

	snprintf(title, sizeof(title), "%s (%u/%u)",
	  current.pattern->real.name, (current.bar + 1),
	  current.pattern->real.bars);
	gtk_window_set_title(GTK_WINDOW(appFrame.window), title);

	gtk_widget_set_sensitive(appFrame.barsButton,
	  (current.pattern->children == NULL));

	gtk_widget_set_sensitive(appFrame.userStepsPerBarButton,
	  (current.pattern->children == NULL));

	gtk_widget_set_sensitive(appFrame.valuesButton,
	  (!(PATTERN_TYPE(current.pattern) == PATTERNTYPE_DUMMY)));
	gtk_widget_set_sensitive(appFrame.randomiseButton,
	  CAN_RANDOMISE(current.pattern));

	gtk_widget_set_sensitive(appFrame.parentButton, TRUE);
	gtk_widget_set_sensitive(appFrame.topButton, TRUE);
	gtk_widget_set_sensitive(appFrame.siblingsButton, TRUE);

	gtk_widget_set_sensitive(appFrame.previousButton, current.bar > 0);
	gtk_widget_set_sensitive(appFrame.nextButton,
	  current.bar < (current.pattern->real.bars - 1));

	userStepsPerBar = current.pattern->real.userStepsPerBar;

	if (userStepsPerBar <= 4) {
		shadesSize = 1;
	} else {
		shadesSize = ((uint32_t) sqrt(userStepsPerBar)) - 1;
		if (shadesSize < 2) {
			shadesSize = 2;
		}
	}
	shadeStep = (shadesSize < 2) ? 0x10000 : (0x10000 / (shadesSize - 1));
	shades = calloc(shadesSize, sizeof(guint16));
	value = 0;
	for (int i = (shadesSize - 1); i >= 0; i--) {
		shades[i] = value;
		value += shadeStep;
		if (value > 0xffff) {
			value = 0xffff;
		}
	}

	for (uint32_t i = 0; i < userStepsPerBar; i++, stepIdx++) {
		gboolean last = (current.bar == maxBar)&&(i == maxStep);
		void *userStep = USERSTEP2(current.pattern, stepIdx);
		void *parentStep = getParentStep(current.pattern, stepIdx);
		gboolean enabled = ((parentStep == NULL) ||
		  (IS_SET(current.pattern->real.parent, parentStep)));
		addStep(current.pattern, userStep, i, shadesSize, userStepsPerBar, shades, enabled, last);
	}

	gtk_widget_show_now(appFrame.middleArea);

	free(shades);
}

static void setSteps(pattern_t *pattern)
{
	void **userSteps = ADDR_USER_STEPS(pattern);

	void **eventSteps = ADDR_EVENT_STEPS(pattern);

	*userSteps = calloc(NUMBER_USERSTEPS(pattern), SIZE_USERSTEP(pattern));
	if (eventSteps != NULL) {
		*eventSteps = calloc(NUMBER_EVENTSTEPS(pattern), SIZE_EVENTSTEP(pattern));
	}
}

static void renderPattern()
{
	pthread_mutex_lock(&(signalling.mutex));
	g_slist_free(signalling.labels); signalling.labels = NULL;

	createStepsView();

	if (current.pattern->isRoot) {
		renderRootPattern();
	} else {
		renderRealPattern();
	}
	gtk_widget_show_all(appFrame.middleArea);

	signalling.bar = current.bar;
	signalling.pattern = current.pattern;
	signalling.bars = current.pattern->real.bars;
	signalling.userStepsPerBar = current.pattern->real.userStepsPerBar;
	signalling.generation++;
	pthread_mutex_unlock(&(signalling.mutex));
}

static void pickBars(int type)
{
	const gchar *text = gtk_label_get_text(GTK_LABEL(numberpicker.label));
	int value = atoi(text);

	if (type == PICK_DUMMY) {
		goto finish;
	}

	if (type == PICK_UP) {
		value += current.pattern->real.parent->isRoot ? 1 :
		  current.pattern->real.parent->real.bars;
	} else {
		value -= current.pattern->real.parent->isRoot ? 1 :
		  current.pattern->real.parent->real.bars;
	}

	gtk_label_set_text(GTK_LABEL(numberpicker.label), intToString(value));

finish:
	gtk_widget_set_sensitive(numberpicker.downButton,
	  (value > current.pattern->real.parent->isRoot ? 1 :
	  current.pattern->real.parent->real.bars));
	gtk_widget_set_sensitive(numberpicker.upButton, ((value * 2) > 0));
}

static GtkWidget *getNumberPickerDialog(void (*pickValue)(int type),
  void (*onOk)(GtkWidget *widget, gpointer data), int initial)
{
	GtkWidget *result = NULL;
	GtkWidget *box = NULL;
	GtkWidget *contentArea = NULL;

	result = getDialog(200, 150);

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));
	box = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);

	numberpicker.pickValue = pickValue;
	addNumberPicker(contentArea, initial);

	gtk_box_pack_start(GTK_BOX(contentArea),
	  getButton("OK", onOk, NULL), TRUE, TRUE, 0);

	gtk_widget_show_all(result);

	return result;
}

static void adjustSteps(pattern_t *pattern, uint32_t bars, uint32_t userStepsPerBar)
{
	uint32_t haveBars = pattern->real.bars;
	uint32_t haveUserStepsPerBar = pattern->real.userStepsPerBar;
	uint32_t stepDivisor = (userStepsPerBar <= haveUserStepsPerBar) ?
	  1 : (userStepsPerBar / haveUserStepsPerBar);
	uint32_t stepMultiplier = (userStepsPerBar >= haveUserStepsPerBar) ?
	  1 : (haveUserStepsPerBar / userStepsPerBar);
	void *userSteps = *(ADDR_USER_STEPS(pattern));
	void **foo = ADDR_EVENT_STEPS(pattern);
	void *eventSteps = NULL;
	if (foo != NULL) {
		eventSteps = *(foo);
	}

	LOCK();

	unsoundPattern();

	current.pattern->real.bars = bars;
	current.pattern->real.userStepsPerBar = userStepsPerBar;
	setSteps(current.pattern);
	for (uint32_t i = 0; i < bars; i++) {
		for (uint32_t j = 0; j < userStepsPerBar; j += stepDivisor) {
			uint32_t targetStep = (i * userStepsPerBar) + j;
			uint32_t sourceBar = i % haveBars;
			uint32_t sourceStep = j / stepDivisor;
			sourceStep *= stepMultiplier;
			sourceStep += (sourceBar * haveUserStepsPerBar);
			SET_STEP_FROM_STEP(current.pattern,
			  USERSTEP(current.pattern, userSteps, sourceStep),
			  USERSTEP2(current.pattern, targetStep));
		}
	}
	UNLOCK();

	if ((userSteps) != NULL) {
		free((userSteps));
	}
	if ((eventSteps) != NULL) {
		free((eventSteps));
	}
}

static void doSetBarsCallback(GtkWidget *widget, gpointer data)
{
	adjustSteps(current.pattern,
	  atoi(gtk_label_get_text(GTK_LABEL(numberpicker.label))),
	  current.pattern->real.userStepsPerBar);

	if (current.bar >= current.pattern->real.bars) {
		current.bar = current.pattern->real.bars - 1;
	}

	destroyDialog();
	renderPattern();
}

static GtkWidget *getBarsDialog(void)
{
	return getNumberPickerDialog(pickBars, doSetBarsCallback,
	  current.pattern->real.bars);
}

static void onBars(GtkWidget *widget, gpointer data)
{
	runDialog(getBarsDialog());
}

static void doSetStepsPerBarCallback(GtkWidget *widget, gpointer data)
{
	adjustSteps(current.pattern,
	  current.pattern->real.bars,
	  atoi(gtk_label_get_text(GTK_LABEL(numberpicker.label))));
	destroyDialog();
	renderPattern();
}

static void pickStepsPerBar(int type)
{
	const gchar *text = gtk_label_get_text(GTK_LABEL(numberpicker.label));
	int value = atoi(text);

	if (type == PICK_DUMMY) {
		goto finish;
	}

	if (type == PICK_UP) {
		value *= 2;
	} else {
		value /= 2;
	}

	gtk_label_set_text(GTK_LABEL(numberpicker.label), intToString(value));

finish:
	gtk_widget_set_sensitive(numberpicker.downButton,
	  (value > current.pattern->real.parent->isRoot ? 1 :
	  current.pattern->real.parent->real.userStepsPerBar));
	gtk_widget_set_sensitive(numberpicker.upButton,
	  ((value * 2) > 0)&&((value * 2) <= MAX_USERSTEPS_PER_BAR(current.pattern)));
}

static GtkWidget *getStepsPerBarDialog(void)
{
	return getNumberPickerDialog(pickStepsPerBar, doSetStepsPerBarCallback,
	  current.pattern->real.userStepsPerBar);
}

static struct {
	gboolean isNote;
	GtkWidget *nameEntry;
	union {
		struct {
			GtkWidget *valueSpinButton;
		} controller;
		struct {
			GtkWidget *sharpCheckButton;
			GtkWidget *octaveSpinButton;
		} note;
	};
} setupvalue;

static struct {
	GtkWidget *radio;
	char name;
	gboolean sharpable;
} tones[] =
{
	{ NULL, 'c', TRUE },
	{ NULL, 'd', TRUE },
	{ NULL, 'e', FALSE },
	{ NULL, 'f', TRUE },
	{ NULL, 'g', TRUE },
	{ NULL, 'a', TRUE },
	{ NULL, 'b', FALSE }
};

static GtkWidget *getValuesDialog(void);

static void setAlsaNoteEvent(uint8_t channel, noteValue_t *noteValue)
{
	int32_t note2Int(noteValue_t *noteValue) {
		int32_t result = 0;
		struct {
			char tone;
			int32_t value;
		} values[] = {
			{'c', 24},
			{'d', 26},
			{'e', 28},
			{'f', 29},
			{'g', 31},
			{'a', 33},
			{'b', 35}
		};
	
		for (uint32_t i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
			if (values[i].tone == noteValue->tone) {
				result = values[i].value;
				break;
			}
		}
	
		if (noteValue->sharp) {
			result++;
		}
	
		result += 12 * noteValue->octave;
		
		return result;
	}

	int32_t note = note2Int(noteValue);

	snd_seq_ev_clear(noteValue->snd_seq_event);
	snd_seq_ev_set_direct(noteValue->snd_seq_event);
	snd_seq_ev_set_noteon(noteValue->snd_seq_event, (channel - 1), note, 127);
	noteValue->snd_seq_event->dest = port;
}

static void doSetupNoteValueCallback(GtkWidget *widget, gpointer data)
{
	noteValue_t *noteValue = data;
	char tone = 0;
	int8_t octave =
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(setupvalue.note.octaveSpinButton));
	gboolean sharp = FALSE;
	const char *name = gtk_entry_get_text(GTK_ENTRY(setupvalue.nameEntry));
	char namebuffer[50];

	for (uint32_t i = 0; i < ARRAY_SIZE(tones); i++) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tones[i].radio))) {
			tone = tones[i].name;
			if (tones[i].sharpable) {
				sharp =
				  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(setupvalue.note.sharpCheckButton));
			}
			//break;
		}
	}

	if (noteValue == NULL) {
		noteValue = calloc(1, sizeof(noteValue_t));
		current.pattern->real.note.values =
		  g_slist_append(current.pattern->real.note.values, noteValue);
#define NAMEFORMAT "%c%s %d"
		if (name[0] == '\0') {
			snprintf(namebuffer, sizeof(namebuffer), NAMEFORMAT, tone, sharp ? "#" : "", octave);
			name = namebuffer;
		}
		noteValue->snd_seq_event = getAlsaEvent();
	} else {
		free(noteValue->name);
	}

	noteValue->tone = tone;
	noteValue->name = strdup(name);
	noteValue->octave = octave;
	noteValue->sharp = sharp;

	setAlsaNoteEvent(current.pattern->real.channel, noteValue);

	runDialog(getValuesDialog());
}

static void triggerControllerValueCallback(GtkWidget *widget, gpointer data)
{
	uint8_t intValue =
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(setupvalue.controller.valueSpinButton));

	snd_seq_event_t *alsa = getAlsaEvent();
	snd_seq_ev_set_controller(alsa, (current.pattern->real.channel - 1),
	  current.pattern->real.controller.parameter, intValue);

	LOCK();
	snd_seq_event_output(sequencer.value, alsa);
	snd_seq_drain_output(sequencer.value);
	UNLOCK();

	free(alsa);
}



static void doSetupControllerValueCallback(GtkWidget *widget, gpointer data)
{
	controllerValue_t *value = data;
	const char *name = gtk_entry_get_text(GTK_ENTRY(setupvalue.nameEntry));
	uint8_t intValue =
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(setupvalue.controller.valueSpinButton));

	if (value == NULL) {
		value = calloc(1, sizeof(controllerValue_t));
		current.pattern->real.controller.values =
		  g_slist_append(current.pattern->real.controller.values, value);
		if (name[0] == '\0') {
			name = intToString(intValue);
		}
		value->event = getAlsaEvent();
	} else {
		free(value->name);
	}

	value->value = intValue;
	value->name = strdup(name);

	snd_seq_ev_set_controller(value->event, (current.pattern->real.channel - 1),
	  current.pattern->real.controller.parameter, value->value);

	runDialog(getValuesDialog());
}

static GtkWidget *getSetupControllerValueDialog(controllerValue_t *value)
{
	char *name = (value == NULL) ? "" : value->name;
	gdouble curValue = (value == NULL) ? 0 : value->value;
	GtkWidget *result = getDialog(400, 300);
	GtkWidget *contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	setupvalue.isNote = FALSE;

	setupvalue.nameEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(setupvalue.nameEntry), name);
	gtk_box_pack_start(GTK_BOX(contentArea), setupvalue.nameEntry, TRUE, TRUE, 0);

	setupvalue.controller.valueSpinButton =
	  getSpinButton(0, 127, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(
	  setupvalue.controller.valueSpinButton), curValue);
	gtk_box_pack_start(GTK_BOX(contentArea),
	  setupvalue.controller.valueSpinButton, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea),
	  getButton("Trigger", triggerControllerValueCallback, NULL), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(contentArea),
	  getButton("OK", doSetupControllerValueCallback, value), TRUE, TRUE, 0);

	gtk_widget_show_all(result);

	return result;
}


#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

static void onSelectTone(void)
{
	for (uint32_t i = 0; i < ARRAY_SIZE(tones); i++) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tones[i].radio))) {
			gtk_widget_set_sensitive(setupvalue.note.sharpCheckButton, tones[i].sharpable);
			break;
		}
	}
}

static GtkWidget *getSetupNoteValueDialog(noteValue_t *value)
{
	char *name = (value == NULL) ? "" : value->name;
	char tone = (value == NULL) ? tones[0].name : value->tone;
	int8_t octave = (value == NULL) ? 0 : value->octave;
	gboolean sharp = (value == NULL) ? FALSE : value->sharp;
	GtkWidget *previousRadio = NULL;
	GtkWidget *result = getDialog(400, 300);
	GtkWidget *contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));
	GtkWidget *notesBox = getBox(GTK_ORIENTATION_HORIZONTAL);
	setupvalue.note.sharpCheckButton = gtk_check_button_new_with_label("sharp");
	setupvalue.note.octaveSpinButton = getSpinButton(-128, 127, 1);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(setupvalue.note.octaveSpinButton), octave);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(setupvalue.note.sharpCheckButton), sharp);

	setupvalue.isNote = TRUE;

	setupvalue.nameEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(setupvalue.nameEntry), name);
	gtk_box_pack_start(GTK_BOX(contentArea), setupvalue.nameEntry, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), notesBox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(contentArea), setupvalue.note.sharpCheckButton, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(contentArea), setupvalue.note.octaveSpinButton, TRUE, TRUE, 0);

	for (uint32_t i = 0; i < ARRAY_SIZE(tones); i++) {
		previousRadio = tones[i].radio =
		  gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(previousRadio),
		  charToString(tones[i].name));
		g_signal_connect(G_OBJECT(tones[i].radio),
		  "clicked", G_CALLBACK(onSelectTone), NULL);

		gtk_box_pack_start(GTK_BOX(notesBox),
		  tones[i].radio, TRUE, TRUE, 0);

		if (tones[i].name == tone) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tones[i].radio), TRUE);
		}
	}

	gtk_box_pack_start(GTK_BOX(contentArea),
	  getButton("OK", doSetupNoteValueCallback, value), TRUE, TRUE, 0);

	onSelectTone();
	gtk_widget_show_all(result);

	return result;
}


static void freeValue(pattern_t *pattern, void *value)
{
	free(VALUE_NAME(pattern, value));
	free(value);
}


static void deleteValueCallback(GtkWidget *widget, gpointer data)
{
	GSList **list = ADDR_PATTERN_VALUES(current.pattern);
	GSList *link = data;

	freeValue(current.pattern, link->data);

	*list = g_slist_delete_link((*list), link);

	runDialog(getValuesDialog());
}

#define getSetupValueDialog(v) \
  (PATTERN_TYPE(current.pattern) == PATTERNTYPE_CONTROLLER) ? \
  getSetupControllerValueDialog(v) : getSetupNoteValueDialog(v)

static void setupValueCallback(GtkWidget *widget, gpointer data)
{
	runDialog(getSetupValueDialog(data));
}

static gboolean isValueInUse(pattern_t *pattern, void *value)
{
	gboolean result = FALSE;
	uint32_t numberSteps =
	  pattern->real.userStepsPerBar * pattern->real.bars;

	for (uint32_t i = 0; i < numberSteps; i++) {
		void *step = USERSTEP2(current.pattern, i);
		void *curValue = STEPVALUE(current.pattern, step);
		if (curValue == value) {
			result = TRUE;
			break;
		}
	}

	return result;
}

static GtkWidget *getValuesDialog(void)
{
	GSList *cur = NULL;
	GtkWidget *result = getDialog(400, 300);
	GtkWidget *contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	for (cur = *(ADDR_PATTERN_VALUES(current.pattern)); cur != NULL;
	  cur = g_slist_next(cur)) {
		GtkWidget *deleteButton = NULL;
		void *value = cur->data;

		GtkWidget *box = getBox(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(box),
		  gtk_label_new(VALUE_NAME(current.pattern, value)), TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(box),
		  getButton("Edit", setupValueCallback, value), TRUE, TRUE, 0);
		deleteButton = getButton("Delete", deleteValueCallback, cur);
		gtk_box_pack_start(GTK_BOX(box), deleteButton, TRUE, TRUE, 0);

		gtk_widget_set_sensitive(deleteButton, (!isValueInUse(current.pattern, cur)));
	}

	gtk_box_pack_start(GTK_BOX(contentArea), getButton("Add",
	  setupValueCallback, NULL), TRUE, TRUE, 0);

	gtk_widget_show_all(result);

	return result;
}

static void onStepsPerBar(GtkWidget *widget, gpointer data)
{
	runDialog(getStepsPerBarDialog());
}

static void onValues(GtkWidget *widget, gpointer data)
{
	runDialog(getValuesDialog());
}

static void onRandomise(GtkWidget *widget, gpointer data)
{
	//TODO
}

static void onPrevious(GtkWidget *widget, gpointer data)
{
	current.bar--;
	renderPattern();
}

static void onNext(GtkWidget *widget, gpointer data)
{
	current.bar++;
	renderPattern();
}

static void changePattern(pattern_t *pattern)
{
	current.bar = 0;
	current.pattern = pattern;
}


static void enterPattern(pattern_t *enterMe)
{
	changePattern(enterMe);
	renderPattern();
}

static void onParent(GtkWidget *widget, gpointer data)
{
	enterPattern(current.pattern->real.parent);
}

static void onTop(GtkWidget *widget, gpointer data)
{
	enterPattern(((pattern_t *) &rootPattern));
}

static void enterPatternCallback(GtkWidget *widget, gpointer data)
{
	enterPattern(data);
	destroyDialog();
}

static void destroyPattern(pattern_t *pattern, gboolean destroyChildren);

void destroyPatternCb(gpointer data, gpointer user_data)
{
	destroyPattern(data, GPOINTER_TO_INT(user_data));
}

void destroyControllerValueCb(gpointer data, gpointer user_data)
{
	controllerValue_t *controllerValue = data;

	free(controllerValue->name);
	free(controllerValue->event);
}

void destroyNoteValuesCb(gpointer data, gpointer user_data)
{
	noteValue_t *noteValue = data;

	free(noteValue->name);
	free(noteValue->snd_seq_event);
}

//TODO: notes off?
static void destroyPattern(pattern_t *pattern, gboolean destroyChildren)
{
	void **userSteps = NULL;
	void **eventSteps = NULL;

	LOCK();

	if (pattern->isRoot) {
		goto finish;
	}

	free(pattern->real.name);
	userSteps = ADDR_USER_STEPS(pattern);
	eventSteps = ADDR_EVENT_STEPS(pattern);

	free(*userSteps);
	if (eventSteps != NULL) {
		free(*eventSteps);
	}

	if (pattern->real.type == PATTERNTYPE_CONTROLLER) {
		g_slist_foreach(pattern->real.controller.values,
		  destroyControllerValueCb, NULL);
		g_slist_free(pattern->real.controller.values);
	} else if (pattern->real.type == PATTERNTYPE_NOTE) {
		g_slist_foreach(pattern->real.note.values,
		  destroyNoteValuesCb, NULL);
		g_slist_free(pattern->real.note.values);
	}

	g_slist_foreach(pattern->children, destroyPatternCb, NULL);

finish:
	free(pattern);
	UNLOCK();
}

static struct {
	pattern_t *parent;
	pattern_t *hideMe;
} patternlist;

static pattern_t *allocatePattern(pattern_t *parent)
{
	pattern_t *result = calloc(1, sizeof(pattern_t));

	result->isRoot = (parent == NULL);
	result->children = NULL;
	result->real.parent = parent;

	return result;
}

static pattern_t *createRealPattern(const gchar *name, gint channel,
  patternType_t patternType, gint controller)
{
	pattern_t *result = allocatePattern(patternlist.parent);

	result->real.name = strdup(name);
	result->real.channel = channel;

	result->real.bars =
	  patternlist.parent->isRoot ? 1 : patternlist.parent->real.bars;
	result->real.userStepsPerBar =
	  patternlist.parent->isRoot ? 1 : patternlist.parent->real.userStepsPerBar;
	PATTERN_TYPE(result) = patternType;

	if (patternType == PATTERNTYPE_CONTROLLER) {
		result->real.controller.parameter = controller;
	}

	return result;
}


static struct {
	uint32_t nextPattern;
	GtkWidget *dummyRadio;
	GtkWidget *noteRadio;
	GtkWidget *channelSpinButton;
	GtkWidget *controllerSpinButton;
	GtkWidget *nameEntry;
	patternType_t patternType;
} createpattern;

static GtkWidget *getPatternListDialog(pattern_t *hideMe);

static void doAddPatternCallback(GtkWidget *widget, gpointer data)
{
	pattern_t  *pattern = NULL;
	const char *name = gtk_entry_get_text(GTK_ENTRY(createpattern.nameEntry));

	if (name[0] == '\0') {
		name = intToString(createpattern.nextPattern++);
	}

	pattern = createRealPattern(name,
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(createpattern.channelSpinButton)),
	  createpattern.patternType,
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(createpattern.controllerSpinButton)));

	setSteps(pattern);

	LOCK();
	patternlist.parent->children = g_slist_append(patternlist.parent->children, pattern);
	UNLOCK();
	renderPattern();

	runDialog(getPatternListDialog(patternlist.hideMe));
}

static void setChannelEnabled(void)
{
	gboolean enabled = TRUE;

	if (createpattern.patternType == PATTERNTYPE_DUMMY) {
		enabled = FALSE;
	}
	gtk_widget_set_sensitive(createpattern.channelSpinButton, enabled);
}

static void setControllerEnabled(void)
{
	gboolean enabled = FALSE;

	if (createpattern.patternType == PATTERNTYPE_CONTROLLER) {
		enabled = TRUE;
	}
	gtk_widget_set_sensitive(createpattern.controllerSpinButton, enabled);
}


static void onSelectPatternType(void)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(createpattern.dummyRadio))) {
		createpattern.patternType = PATTERNTYPE_DUMMY;
	} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(createpattern.noteRadio))) {
		createpattern.patternType = PATTERNTYPE_NOTE;
	} else {
		createpattern.patternType = PATTERNTYPE_CONTROLLER;
	}
	setChannelEnabled();
	setControllerEnabled();
}

static GtkWidget *getSetupPatternDialog(void)
{
	GtkWidget *result = NULL;
	GtkWidget *box = NULL;
	GtkWidget *contentArea = NULL;

	result = getDialog(400, 300);

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));
	box = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);

	createpattern.nameEntry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(contentArea), createpattern.nameEntry, TRUE, TRUE, 0);

	createpattern.dummyRadio =
	  gtk_radio_button_new_with_label_from_widget(NULL, "DUMMY");
	createpattern.patternType = PATTERNTYPE_DUMMY;

	g_signal_connect(G_OBJECT(createpattern.dummyRadio), "clicked",
	  G_CALLBACK(onSelectPatternType), NULL);
	gtk_box_pack_start(GTK_BOX(box), createpattern.dummyRadio, TRUE, TRUE, 0);

	createpattern.noteRadio =
	  gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(
	  createpattern.dummyRadio), "NOTE");
	g_signal_connect(G_OBJECT(createpattern.noteRadio), "clicked", G_CALLBACK(onSelectPatternType), NULL);
	gtk_box_pack_start(GTK_BOX(box), createpattern.noteRadio, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box),
	  gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(createpattern.dummyRadio),
	  "CONTROLLER"), TRUE, TRUE, 0);

	createpattern.channelSpinButton = getSpinButton(1, 16, 1);
	gtk_box_pack_start(GTK_BOX(contentArea), createpattern.channelSpinButton, TRUE, TRUE, 0);
	setChannelEnabled();

	createpattern.controllerSpinButton = getSpinButton(1, 127, 1);
	gtk_box_pack_start(GTK_BOX(contentArea), createpattern.controllerSpinButton, TRUE, TRUE, 0);
	setControllerEnabled();

	gtk_box_pack_start(GTK_BOX(contentArea),
	  getButton("OK", doAddPatternCallback, NULL), TRUE, TRUE, 0);

	onSelectPatternType();

	gtk_widget_show_all(result);

	return result;
}



static void deletePatternCallback(GtkWidget *widget, gpointer data)
{
	GSList *link = data;
	pattern_t *pattern = link->data;

	LOCK();
	pattern->real.parent->children =
	  g_slist_delete_link(pattern->real.parent->children, link);
	UNLOCK();

	destroyPattern(pattern, TRUE);

	renderPattern();

	runDialog(getPatternListDialog(patternlist.hideMe));
}

static void addPatternCallback(GtkWidget *widget, gpointer data)
{
	runDialog(getSetupPatternDialog());
}

static GtkWidget *getPatternListDialog(pattern_t *hideMe)
{
	GtkWidget *contentArea = NULL;
	GSList *cur = NULL;
	GtkWidget *result = NULL;
	GtkWidget *deleteButton = NULL;

	result = getDialog(400, 300);

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	for (cur = patternlist.parent->children; cur != NULL; cur = g_slist_next(cur)) {
		pattern_t *child = cur->data;
		if (child == hideMe) {
			continue;
		}
		GtkWidget *box = getBox(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(box),
		  gtk_label_new(child->real.name), TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(box),
		  getButton("Enter", enterPatternCallback, child), TRUE, TRUE, 0);
		deleteButton = getButton("Delete", deletePatternCallback, cur);
		gtk_box_pack_start(GTK_BOX(box),
		  deleteButton, TRUE, TRUE, 0);

		gtk_widget_set_sensitive(deleteButton, ((child->children == NULL)));

	}

	gtk_box_pack_start(GTK_BOX(contentArea), getButton("Add",
	  addPatternCallback, NULL), TRUE, TRUE, 0);

	gtk_widget_show_all(result);

	return result;
}

static void onPatternList(pattern_t *p, pattern_t *hideMe)
{
	patternlist.parent = p;
	patternlist.hideMe = hideMe;
	runDialog(getPatternListDialog(hideMe));
}

static void onChildren(GtkWidget *widget, gpointer data)
{
	onPatternList(current.pattern, NULL);
}
static void onSiblings(GtkWidget *widget, gpointer data)
{
	onPatternList(current.pattern->real.parent, current.pattern);

}

static void onAdjustmentChanged(GtkRange *range, gpointer  user_data)
{
	adjustment = gtk_range_get_value(GTK_RANGE(appFrame.adjustScale));
}

#define EXTENSION ".ksq"

static char *onLoadStore(gboolean load)
{
	char *result = NULL;
	GtkWidget *dialog = gtk_file_chooser_dialog_new(load ?
	  "Open File" : "Store File", GTK_WINDOW(appFrame.window),
	  load ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE,
	  "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_ACCEPT, NULL);
	GtkFileChooser *fileChooser = GTK_FILE_CHOOSER(dialog);

	GtkFileFilter *fileFilter = gtk_file_filter_new();
	gtk_file_filter_set_name (fileFilter,
	  "KSQ sequences");
	gtk_file_filter_add_pattern(fileFilter, "*" EXTENSION);
	gtk_file_chooser_add_filter(fileChooser, fileFilter);

	if (runDialog(dialog) == GTK_RESPONSE_ACCEPT) {
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
		result = strdup(gtk_file_chooser_get_filename(chooser));
	}
	destroyDialog();
	return result;
}

typedef ssize_t (*readWriteFunc_t)(int fd, void *buf, size_t count);

#define readWriteFd(d,l,f,w,e) readWriteFdReal(d,l,f,w,e,__FUNCTION__, __FILE__, __LINE__)

static void readWriteFdReal(void *data, uint32_t length, int fd,
  gboolean writ, err_t *e, const char *function, const char *file, const int line)
{
	uint32_t pending = length;
	char *cur = data;
	readWriteFunc_t func = writ ? (readWriteFunc_t) write : read;

	while (pending > 0) {
		ssize_t just = func(fd, cur, pending);
		if (just < 1) {
			terror(failIfFalse((just < 0)&&(errno == EINTR)))
			continue;
		}
		pending -= just;
		cur += just;
	}
finish:
	return;
}

static void loadStorePattern(pattern_t **pattern, int fd, gboolean store,
  pattern_t *parent, err_t *e);

static void loadStoreChildren(GSList **children, int fd, gboolean store,
  pattern_t *parent, err_t *e)
{
	guint length = 0;

	if (store) {
		length = g_slist_length((*children));
		terror(readWriteFd(&length, sizeof(length), fd, store, e))
		GSList *cur = NULL;
		for (cur = (*children); cur !=  NULL; cur = g_slist_next(cur)) {
			terror(loadStorePattern(((pattern_t **) &(cur->data)),
			  fd, store, parent, e))
		}
	} else {
		uint32_t i = 0;
		pattern_t *child = NULL;

		terror(readWriteFd(&length, sizeof(length), fd, store, e))
		for (i = 0; i < length; i++) {
			terror(loadStorePattern(&child, fd, store, parent, e))
			*children = g_slist_append((*children), child);
		}
	}

finish:
	if ((hasFailed(e))&&(!store)) {
		g_slist_foreach((*children), destroyPatternCb, NULL);
		g_slist_free((*children));
		(*children) = NULL;
	}
}

static void loadStoreDummyUserStep(dummyUserStep_t *dummyUserStep, int fd,
  gboolean store, err_t *e)
{
	terror(readWriteFd(&(dummyUserStep->set), sizeof(dummyUserStep->set), fd, store, e))

finish:
	return;
}

static void loadStoreNoteUserStep(pattern_t *pattern, noteUserStep_t *noteUserStep,
  GSList *noteValues, int fd, gboolean store, err_t *e)
{
	gint position = -1;
	gboolean slide = FALSE;

	if (store) {
		position = g_slist_position(noteValues, noteUserStep->value);
		if (noteUserStep->value != NULL) {
			slide = noteUserStep->slide;
		}
	}

	terror(readWriteFd(&slide, sizeof(slide), fd, store, e))
	terror(readWriteFd(&position, sizeof(position), fd, store, e))

	if ((!store)&&(position > -1)) {
		setNoteStep(pattern, noteUserStep, g_slist_nth(noteValues , position));
	}
	setNoteSlide(pattern, noteUserStep, slide);

finish:
	return;
}

static void loadStoreControllerUserStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, GSList *controllerValues,
  int fd, gboolean store, err_t *e)
{
	gint position = -1;

	if ((store)&&(controllerUserStep->value != NULL)) {
		position = g_slist_position(controllerValues, controllerUserStep->value);
	}

	terror(readWriteFd(&position, sizeof(position), fd, store, e))

	if ((!store)&&(position > -1)) {
		setControllerStep(pattern, controllerUserStep,
		  g_slist_nth(controllerValues , position));
	}


finish:
	return;
}

static void loadStoreDummyPattern(pattern_t **pattern, int fd, gboolean store, err_t *e)
{
	uint32_t steps = (*pattern)->real.userStepsPerBar * (*pattern)->real.bars;

	for (uint32_t i = 0; i < steps; i++) {
		terror(loadStoreDummyUserStep(&((*pattern)->real.dummy.steps[i]),
		  fd, store, e))
	}

finish:
	return;
}

static void writeStringToFd(char *string, int fd, err_t *e)
{
	size_t length = strlen(string);

	terror(readWriteFd(&length, sizeof(length), fd, TRUE, e))
	terror(readWriteFd(string, length, fd, TRUE, e))

finish:
	return;
}

static char *readStringFromFd(int fd, err_t *e)
{
	size_t length = 0;
	char *result = NULL;
	char *_result = NULL;

	terror(readWriteFd(&length, sizeof(length), fd, FALSE, e))
	_result = calloc(1, (length + 1));
	terror(readWriteFd(_result, length, fd, FALSE, e))
	_result[length] = '\0';

	result = _result; _result = NULL;
finish:
	return result;
}

static void loadStoreNoteValue(noteValue_t *noteValue, int fd,
  gboolean store, err_t *e)
{
	if (store) {
		terror(writeStringToFd(noteValue->name, fd, e))
	} else {
		terror(noteValue->name = readStringFromFd(fd, e))
	}
	terror(readWriteFd(&(noteValue->tone), sizeof(noteValue->tone), fd, store, e))
	terror(readWriteFd(&(noteValue->octave), sizeof(noteValue->octave), fd, store, e))
	terror(readWriteFd(&(noteValue->sharp), sizeof(noteValue->sharp), fd, store, e))

finish:
	return;
}

static void loadStoreControllerValue(controllerValue_t *controllerValue, int fd,
  gboolean store, err_t *e)
{
	if (store) {
		terror(writeStringToFd(controllerValue->name, fd, e))
	} else {
		terror(controllerValue->name = readStringFromFd(fd, e))
	}
	terror(readWriteFd(&(controllerValue->value),
	  sizeof(controllerValue->value), fd, store, e))

finish:
	return;
}

static void loadStoreNotePattern(pattern_t **pattern, int fd, gboolean store, err_t *e)
{
	uint32_t i = 0;
	uint32_t noteValues = g_slist_length((*pattern)->real.note.values);
	uint32_t steps = (*pattern)->real.userStepsPerBar * (*pattern)->real.bars;
	noteValue_t *noteValue = NULL;

	terror(readWriteFd(&noteValues, sizeof(noteValues), fd, store, e))

	if (store) {
		GSList *cur = NULL;
		for (cur = (*pattern)->real.note.values; cur != NULL; cur = g_slist_next(cur)) {
			terror(loadStoreNoteValue(cur->data, fd, store, e))
		}
	} else {
		for (i = 0; i < noteValues; i++) {
			noteValue = calloc(1, sizeof(noteValue_t));
			noteValue->snd_seq_event = getAlsaEvent();
			terror(loadStoreNoteValue(noteValue, fd, store, e))
			
			//TODO
			setAlsaNoteEvent((*pattern)->real.channel, noteValue);
			(*pattern)->real.note.values =
			  g_slist_append((*pattern)->real.note.values, noteValue);
			noteValue = NULL;
		}
	}

	for (i = 0; i < steps; i++) {
		terror(loadStoreNoteUserStep(*pattern, &((*pattern)->real.note.steps.user[i]),
		  (*pattern)->real.note.values, fd, store, e))
	}

finish:
	free(noteValue);
}

static void loadStoreControllerPattern(pattern_t **pattern, int fd,
  gboolean store, err_t *e)
{
	uint32_t i = 0;
	GSList *cur = NULL;
	uint32_t controllerValues = g_slist_length((*pattern)->real.controller.values);
	uint32_t steps = (*pattern)->real.userStepsPerBar * (*pattern)->real.bars;
	controllerValue_t *controllerValue = NULL;

	terror(readWriteFd(&(*pattern)->real.controller.parameter,
	  sizeof((*pattern)->real.controller.parameter), fd, store, e))

	terror(readWriteFd(&controllerValues,
	  sizeof(controllerValues), fd, store, e))

	if (store) {
		for (cur = (*pattern)->real.controller.values; cur != NULL; cur = g_slist_next(cur)) {
			terror(loadStoreControllerValue(cur->data, fd, store, e))
		}
	} else {
		for (i = 0; i < controllerValues; i++) {
			controllerValue = calloc(1, sizeof(controllerValue_t));
			terror(loadStoreControllerValue(controllerValue, fd, store, e))
			(*pattern)->real.controller.values =
			  g_slist_append((*pattern)->real.controller.values, controllerValue);
			controllerValue = NULL;
		}
	}
	for (i = 0; i < steps; i++) {
		terror(loadStoreControllerUserStep(*pattern, &((*pattern)->real.controller.steps.user[i]),
		  (*pattern)->real.controller.values, fd, store, e))
	}

finish:
	free(controllerValue);
}

static void loadStoreRealPattern(pattern_t **pattern, int fd, gboolean store,
  pattern_t *parent, err_t *e)
{
	if (store) {
		terror(writeStringToFd((*pattern)->real.name, fd, e))
	} else {
		(*pattern) = allocatePattern(parent);
		(*pattern)->real.parent = parent;
		(*pattern)->isRoot = FALSE;
		terror((*pattern)->real.name = readStringFromFd(fd, e))
	}

	terror(readWriteFd(&((*pattern)->real.channel),
	  sizeof((*pattern)->real.channel), fd, store, e))
	terror(readWriteFd(&((*pattern)->real.userStepsPerBar),
	  sizeof((*pattern)->real.userStepsPerBar), fd, store, e))
	terror(readWriteFd(&((*pattern)->real.bars),
	  sizeof((*pattern)->real.bars), fd, store, e))
	terror(readWriteFd(&((*pattern)->real.type),
	  sizeof((*pattern)->real.type), fd, store, e))

	if (!store) {
		setSteps((*pattern));
	}

	if ((*pattern)->real.type == PATTERNTYPE_DUMMY) {
		terror(loadStoreDummyPattern(pattern, fd, store, e))
	} else if ((*pattern)->real.type == PATTERNTYPE_NOTE) {
		terror(loadStoreNotePattern(pattern, fd, store, e))
	} else {
		terror(loadStoreControllerPattern(pattern, fd, store, e))
	}

finish:
	if ((hasFailed(e))&&(!store)&&((*pattern) != NULL)) {
		destroyPattern((*pattern), TRUE); (*pattern) = NULL;
	}
}

static void loadStorePattern(pattern_t **pattern, int fd, gboolean store,
  pattern_t *parent, err_t *e)
{
	gboolean isRoot = FALSE;

	if (store) {
		terror(readWriteFd(&((*pattern)->isRoot),
		  sizeof((*pattern)->isRoot), fd, store, e))
		if ((*pattern)->isRoot) {
			//nothing
		} else {
			terror(loadStoreRealPattern(pattern, fd, store, parent, e))
		}
	} else {
		terror(readWriteFd(&isRoot, sizeof(isRoot), fd, store, e))
		if (isRoot) {
			(*pattern) = allocatePattern(NULL);
		} else {
			terror(loadStoreRealPattern(pattern, fd, store, parent, e))
		}
	}
	terror(loadStoreChildren(&((*pattern)->children), fd, store, (*pattern), e))
finish:
	if ((hasFailed(e))&&(!store)&&((*pattern) != NULL)) {
		destroyPattern((*pattern), TRUE); (*pattern) = NULL;
	}
}

/*static void freeNoteValue(noteValue_t *noteValue)
{
	free(noteValue->snd_seq_event);
	free(noteValue);
}*/

static void onLoad(GtkWidget *widget, gpointer data)
{

	void setParentCb(gpointer data, gpointer user_data) {
		pattern_t *pattern = data;
		pattern->real.parent = user_data;
	}

	char *path = onLoadStore(TRUE);
	err_t error;
	err_t *e = &error;
	int fd = -1;
	pattern_t *pattern = NULL;

	initErr(e);

	if (path == NULL) {
		goto finish;
	}

	terror(failIfFalse((fd = open(path, O_RDONLY)) >= 0))
	terror(loadStorePattern(&pattern, fd, FALSE, NULL, e))

	if (!(pattern->isRoot)) {
		LOCK();
		pattern->real.parent = &rootPattern;
		rootPattern.children =
		  g_slist_append(rootPattern.children, pattern); pattern = NULL;
		UNLOCK();
	} else {
		LOCK();
		rootPattern.children =
		  g_slist_concat(rootPattern.children, pattern->children);
		g_slist_foreach(pattern->children, setParentCb, &rootPattern);
		destroyPattern(pattern, FALSE); pattern = NULL;
		UNLOCK();
	}
	renderPattern();
finish:
	if (hasFailed(e)) {
		runDialog(gtk_message_dialog_new(GTK_WINDOW(appFrame.window),
		  GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		  "Error: %s", err2string(e)));
		destroyDialog();
	}
	if (pattern != NULL) {
		destroyPattern(pattern, TRUE);
	}
	free(path);
	if (fd >= 0) {
		close(fd);
	}
}

static void onStore(GtkWidget *widget, gpointer data)
{
	char *path = onLoadStore(FALSE);
	char *tmp;
	err_t error;
	err_t *e = &error;
	int fd = -1;

	initErr(e);

	if (path == NULL) {
		goto finish;
	}

	if ((strrstr(path, EXTENSION)) != (path + strlen(path) - (sizeof(EXTENSION) - 1))) {
		tmp = path;
		path = g_strconcat(path, EXTENSION, NULL);
		free(tmp);
	}

	terror(failIfFalse((fd =
	  open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP)) >= 0))
	terror(loadStorePattern(&(current.pattern), fd, TRUE,
	  current.pattern->isRoot ? NULL : current.pattern->real.parent, e))

finish:
	if (hasFailed(e)) {
		runDialog(gtk_message_dialog_new(GTK_WINDOW(appFrame.window),
		  GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		  "Error: %s", err2string(e)));
		destroyDialog();
	}
	free(path);
	if (fd >= 0) {
		close(fd);
	}
}

static GtkWidget *getGeneralView()
{
	GtkWidget *result = getBox(GTK_ORIENTATION_HORIZONTAL);

	appFrame.speedLabel = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(result), appFrame.speedLabel, TRUE, TRUE, 0);
	appFrame.adjustScale =
	  gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100000000, 1);
	gtk_box_pack_start(GTK_BOX(result), appFrame.adjustScale, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(appFrame.adjustScale),
	  "value-changed", G_CALLBACK(onAdjustmentChanged), NULL);
	appFrame.loadButton = getButton("Load ...", onLoad, NULL);
	gtk_box_pack_start(GTK_BOX(result), appFrame.loadButton, TRUE, TRUE, 0);
	appFrame.storeButton = getButton("Store ...", onStore, NULL);
	gtk_box_pack_start(GTK_BOX(result), appFrame.storeButton, TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getAdminView()
{
	GtkWidget *line1 = NULL;
	GtkWidget *line2 = NULL;

	GtkWidget *result = getBox(GTK_ORIENTATION_VERTICAL);

	line1 = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(result), line1, TRUE, TRUE, 0);
	line2 = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(result), line2, TRUE, TRUE, 0);

	appFrame.barsButton = getButton("Bars ...", onBars, NULL);
	gtk_box_pack_start(GTK_BOX(line1), appFrame.barsButton, TRUE, TRUE, 0);
	appFrame.userStepsPerBarButton = getButton("Steps per Bar ...", onStepsPerBar, NULL);
	gtk_box_pack_start(GTK_BOX(line1), appFrame.userStepsPerBarButton, TRUE, TRUE, 0);

	appFrame.valuesButton = getButton("Values ...", onValues, NULL);
	gtk_box_pack_start(GTK_BOX(line2), appFrame.valuesButton, TRUE, TRUE, 0);
	appFrame.randomiseButton = getButton("Randomise ...", onRandomise, NULL);
	gtk_box_pack_start(GTK_BOX(line2), appFrame.randomiseButton, TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getNavView()
{
	GtkWidget *line1 = NULL;
	GtkWidget *line2 = NULL;

	GtkWidget *result = getBox(GTK_ORIENTATION_VERTICAL);

	line1 = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(result), line1, TRUE, TRUE, 0);
	line2 = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(result), line2, TRUE, TRUE, 0);

	appFrame.previousButton = getButton("<<", onPrevious, NULL);
	gtk_box_pack_start(GTK_BOX(line1), appFrame.previousButton, TRUE, TRUE, 0);
	appFrame.nextButton = getButton(">>", onNext, NULL);
	gtk_box_pack_start(GTK_BOX(line1), appFrame.nextButton, TRUE, TRUE, 0);
	appFrame.parentButton = getButton("Parent!", onParent, NULL);
	gtk_box_pack_start(GTK_BOX(line1), appFrame.parentButton, TRUE, TRUE, 0);
	appFrame.topButton = getButton("Top!", onTop, NULL);
	gtk_box_pack_start(GTK_BOX(line1), appFrame.topButton, TRUE, TRUE, 0);

	appFrame.childrenButton = getButton("Children ...", onChildren, NULL);
	gtk_box_pack_start(GTK_BOX(line2), appFrame.childrenButton, TRUE, TRUE, 0);
	appFrame.siblingsButton = getButton("Siblings ...", onSiblings, NULL);
	gtk_box_pack_start(GTK_BOX(line2), appFrame.siblingsButton, TRUE, TRUE, 0);

	return result;
}

#define STYLE \
  ".colorable {" \
	"border-image: none;" \
	"background-image: none;" \
  "}"

void gtkFunction(int argc, char *argv[])
{
	GtkWidget *wholeArea = NULL;
	GtkCssProvider *provider = NULL;
	GdkDisplay *display = NULL;
	GdkScreen *screen = NULL;


	gtk_init(&argc, &argv);

	appFrame.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(appFrame.window, 800, 600);
	gtk_window_set_resizable(GTK_WINDOW(appFrame.window), FALSE);
	gtk_window_set_position(GTK_WINDOW(appFrame.window), GTK_WIN_POS_CENTER);

	provider = gtk_css_provider_new();
	display = gdk_display_get_default();
	screen = gdk_display_get_default_screen(display);
	gtk_style_context_add_provider_for_screen(screen,
	  GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
	  STYLE, -1, NULL);
	g_object_unref (provider);

	//-------------------------------------------------------------------------
	wholeArea = getBox(GTK_ORIENTATION_VERTICAL);
	gtk_container_add(GTK_CONTAINER(appFrame.window), wholeArea);
	//-------------------------------------------------------------------------

	appFrame.upperArea = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(wholeArea), appFrame.upperArea, TRUE, TRUE, 0);
	appFrame.middleArea = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(wholeArea), appFrame.middleArea, TRUE, TRUE, 0);
	appFrame.lowerArea = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(wholeArea), appFrame.lowerArea, TRUE, TRUE, 0);


	gtk_box_pack_start(GTK_BOX(appFrame.upperArea), getGeneralView(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(appFrame.upperArea), getAdminView(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(appFrame.lowerArea), getNavView(), TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(appFrame.window), "destroy",
	  G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(appFrame.window);

	renderPattern();

	gtk_main();

	pthread_mutex_destroy(&(signalling.mutex));

}

void gtkInit(void)
{
	pthread_mutex_init(&(signalling.mutex), NULL);

	gdk_threads_init();
}
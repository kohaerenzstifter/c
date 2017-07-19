#include "gui.h"

static uint32_t lockCount = 0;

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
	GtkWidget *tempoLabel;
	GtkWidget *portCombo;
	GtkWidget *anticipationScale;
	GtkWidget *maxSleepNanosScale;
	GtkWidget *loadButton;
	GtkWidget *storeButton;
} appFrame;

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

	gtk_label_set_text(GTK_LABEL(appFrame.tempoLabel), speedString);

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

#define getUserStepIdx(s, p) \
  ((((p->real.type)) == (PATTERNTYPE_NOTE)) ? \
  (((noteUserStep_t *) s) - p->real.note.steps.user) : \
  (((p->real.type)) == PATTERNTYPE_DUMMY) ? \
  (((dummyUserStep_t *) s) - p->real.dummy.steps) : \
  (((controllerUserStep_t *) s) - p->real.controller.steps.user))

#define getUserStep(p, idx) \
  ((p->real.type == PATTERNTYPE_NOTE) ? ((void *) &(p->real.note.steps.user[idx])) : \
  (p->real.type == PATTERNTYPE_CONTROLLER) ? ((void *) &(p->real.controller.steps.user[idx])) : \
  ((void *) &(p->real.dummy.steps[idx])))


#define getLockStatus(s, pT) \
  (((pT) == (PATTERNTYPE_NOTE)) ? ((lockStatus_t *) &(((noteUserStep_t *) s)->lockStatus)) : \
  ((pT) == PATTERNTYPE_DUMMY) ? ((lockStatus_t *) &(((dummyUserStep_t *) s)->lockStatus)) : \
  ((lockStatus_t *) &(((controllerUserStep_t *) s)->lockStatus)))


static void setChildLockStatus(pattern_t *child,
  pattern_t *parent, uint32_t userIdx, gboolean isSet)
{
	uint32_t barsFactor = child->real.bars / parent->real.bars;
	uint32_t stepsFactor = child->real.userStepsPerBar / parent->real.userStepsPerBar;

	for (uint32_t i = 0; i < barsFactor; i++)  {
		for (uint32_t j = 0; j < stepsFactor; j++) {
			lockStatus_t *lockStatus =
			  getLockStatus((USERSTEP2(child,
			  (((userIdx * stepsFactor) * i) + j))), child->real.type);
			if ((*lockStatus) != lockStatusUserlocked) {
				(*lockStatus) = (!isSet) ? lockStatusUnsettable : lockStatusFree;
			}
		}
	}
}

static void setParentLockStatus(pattern_t *pattern, uint32_t userIdx)
{
	void *parentStep = getParentStep(pattern, userIdx);

	if (parentStep == NULL) {
		goto finish;
	}

	gboolean isSet = FALSE;

	for (GSList *cur = ((GSList *) pattern->real.parent->children);
	  cur != NULL; cur = g_slist_next(cur)) {
		pattern = cur->data;

		uint32_t stepsFactor = 
		  pattern->real.userStepsPerBar / pattern->real.parent->real.userStepsPerBar;
		uint32_t barsFactor = 
		  pattern->real.bars / pattern->real.parent->real.bars;

		for (uint32_t i = 0; i < barsFactor; i++) {
			for (uint32_t j = 0; j < stepsFactor; j++) {
				void *step = getUserStep(pattern, (((userIdx * stepsFactor) * i) + j));
				if (IS_SET(pattern, step)) {
					isSet = TRUE;
					break;
				}
			}
			if (isSet) {
				break;
			}
		}
	}

	lockStatus_t *lockStatus =
	  getLockStatus(parentStep, pattern->real.parent->real.type);
	if ((*lockStatus) != lockStatusUserlocked) {
		(*lockStatus) = isSet ? lockStatusUnunsettable : lockStatusFree;
	}

finish:
	return;
}

static void setChildrenLockStatus(pattern_t *pattern, uint32_t userIdx, gboolean isSet)
{
	for (GSList *cur = (GSList *) pattern->children; cur != NULL; cur = g_slist_next(cur)) {
		setChildLockStatus(cur->data, pattern, userIdx, isSet);
	}
}

static void setParentAndChildrenLockStatus(pattern_t *pattern, uint32_t userIdx, gboolean isSet)
{
	setParentLockStatus(pattern, userIdx);
	setChildrenLockStatus(pattern, userIdx, isSet);
}

static void renderPattern();

static void onDummyStep(GtkWidget *widget, gpointer data)
{
	dummyUserStep_t *step = data;
	uint32_t userIdx = step - current.pattern->real.dummy.steps;

	setDummyStep(step, (!step->set), lockCount);

	setParentAndChildrenLockStatus(current.pattern, userIdx, step->set);

	renderPattern();
}

/*gboolean mayNotUnset(uint32_t userIdx)
{
	gboolean result = FALSE;

	for (GSList *cur = (GSList *) current.pattern->children; cur != NULL;
	  cur = g_slist_next(cur)) {
		pattern_t *pattern = cur->data;
		uint32_t factor = pattern->real.userStepsPerBar /
		  current.pattern->real.userStepsPerBar;
		
		for (uint32_t idx = (factor * userIdx);
		  idx < ((factor * userIdx) + factor); idx++) {
			if (PATTERN_TYPE(pattern) == PATTERNTYPE_NOTE) {
				result = (pattern->real.note.steps.user[idx].value != NULL);
			} else {
				result = (pattern->real.controller.steps.user[idx].value != NULL);
			}
			if (result) {
				break;
			}
		}
		if (result) {
			break;
		}
	}

	return result;
}*/

static void onControllerStep(GtkWidget *widget, gpointer data)
{
	controllerUserStep_t *step = data;
	uint32_t userIdx = step - current.pattern->real.controller.steps.user;

	GSList *value = (step->value == NULL) ?
	  (GSList *) current.pattern->real.controller.values :
	  (GSList *) g_slist_next(step->value);
	if ((value == NULL)&&(step->lockStatus == lockStatusUnunsettable)) {
		value = (GSList *) current.pattern->real.controller.values;
	}

	setControllerStep(current.pattern, step, value, lockCount);

	setParentAndChildrenLockStatus(current.pattern, userIdx, (step->value != NULL));

	renderPattern();
}

//TODO: create child pattern
//TODO: adjustSteps

static void onLockStep(gpointer step, patternType_t patternType)
{
	lockStatus_t *lockStatus = getLockStatus(step, patternType);

	if ((*lockStatus) != lockStatusUserlocked) {
		(*lockStatus) = lockStatusUserlocked;
		goto finish;
	}
	(*lockStatus) = lockStatusFree;

	if (current.pattern->children != NULL) {
		pattern_t *childPattern = current.pattern->children->data;
		uint32_t factor = 
		  childPattern->real.userStepsPerBar / current.pattern->real.parent->real.userStepsPerBar;
		setParentLockStatus(childPattern, (getUserStepIdx(step, current.pattern) * factor));
	}

	void *parentStep = getParentStep(current.pattern, getUserStepIdx(step, current.pattern));
	if (parentStep == NULL) {
		goto finish;
	}
	setChildrenLockStatus(((pattern_t *) current.pattern->real.parent),
	  getUserStepIdx(parentStep, current.pattern->real.parent),
	  IS_SET(current.pattern->real.parent, parentStep));

finish:
	renderPattern();
	return;
}

static void onLockNote(GtkWidget *widget, gpointer data)
{
	onLockStep(data, PATTERNTYPE_NOTE);
}

static void onLockSlide(GtkWidget *widget, gpointer data)
{
	//TODO
}

static void onLockController(GtkWidget *widget, gpointer data)
{
	onLockStep(data, PATTERNTYPE_CONTROLLER);
}

static void onLockDummy(GtkWidget *widget, gpointer data)
{
	onLockStep(data, PATTERNTYPE_DUMMY);
}

static void onNoteStep(GtkWidget *widget, gpointer data)
{
	noteUserStep_t *step = data;
	uint32_t userIdx = step - current.pattern->real.note.steps.user;

	GSList *value = (step->value == NULL) ?
	  (GSList *) current.pattern->real.note.values :
	  (GSList *) g_slist_next(step->value);
	if ((value == NULL)&&(step->lockStatus == lockStatusUnunsettable)) {
		value = (GSList *) current.pattern->real.note.values;
	}

	setNoteStep(current.pattern, step, value, lockCount);

	setParentAndChildrenLockStatus(current.pattern, userIdx, (step->value != NULL));

	renderPattern();
}

static void onNoteSlide(GtkWidget *widget, gpointer data)
{
	noteUserStep_t *step = data;

	setNoteSlide(current.pattern, step, (!step->slide), lockCount);

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
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades)
{
	GtkWidget *button = NULL;
	GtkWidget *verticalBox = NULL;
	char *text = !step->set ? "" : "X";

	verticalBox = addVerticalStepBox(i, shadesSize, stepsPerBar, shades);

	button = getButton(text, onDummyStep, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);
	gtk_widget_set_sensitive(button,
	  (step->lockStatus == lockStatusUnsettable) ? FALSE :
	  (step->lockStatus == lockStatusUnunsettable) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 1) :
	  (step->lockStatus == lockStatusFree) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 0) :
	  FALSE);

	button = getButton((step->lockStatus == lockStatusUserlocked) ?
	  "!LOCK" : "LOCK", onLockDummy, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);
	gtk_widget_set_sensitive(button,
	  (step->lockStatus == lockStatusUnsettable) ? FALSE :
	  (step->lockStatus == lockStatusUnunsettable) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 1) :
	  (g_slist_length(GET_VALUES(current.pattern)) > 0));
}

static void addNoteStep(noteUserStep_t *step, uint32_t i,
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades,
  gboolean last)
{
	gboolean slideEnabled = ((!last)&&IS_SET(current.pattern, step));
	GtkWidget *button = NULL;
	GtkWidget *verticalBox = NULL;
	char *stepText = (step->value == NULL) ? "" :
	  (char *) ((noteValue_t *) (step->value->data))->name;
	char *slideText = (step->value == NULL) ? "-" :
	  step->slide ? "!SLIDE" : "SLIDE";

	verticalBox = addVerticalStepBox(i, shadesSize, stepsPerBar, shades);

	button = getButton((step->lockStatus == lockStatusUserlocked) ?
	  "!LOCK" : "LOCK", onLockNote, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);
	gtk_widget_set_sensitive(button,
	  (step->lockStatus == lockStatusUnsettable) ? FALSE :
	  (step->lockStatus == lockStatusUnunsettable) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 1) :
	  (g_slist_length(GET_VALUES(current.pattern)) > 0));

	button = getButton(stepText, onNoteStep, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);
	gboolean parentAllows = TRUE;
	void *parentStep =
	  getParentStep(current.pattern, getUserStepIdx(step, current.pattern));
	if (parentStep != NULL) {
		parentAllows = IS_SET(current.pattern->real.parent, parentStep);
	}
	gtk_widget_set_sensitive(button,
	  (!parentAllows) ? FALSE :
	  (step->lockStatus == lockStatusUnsettable) ? FALSE :
	  (step->lockStatus == lockStatusUnunsettable) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 1) :
	  (step->lockStatus == lockStatusFree) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 0) :
	  FALSE);

	button = getButton(slideText, onNoteSlide, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);
	gtk_widget_set_sensitive(button, slideEnabled);

	button = getButton((step->slideLockStatus == lockStatusUserlocked) ?
	  "!LOCK" : "LOCK", onLockSlide, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);
	gtk_widget_set_sensitive(button, slideEnabled);
}

static void addControllerStep(controllerUserStep_t *step, uint32_t i,
  uint32_t shadesSize, uint32_t stepsPerBar, guint16 *shades)
{
	GtkWidget *button = NULL;
	GtkWidget *verticalBox = NULL;
	char *text = (step->value == NULL) ? "" :
	  (char *) ((controllerValue_t *) (step->value->data))->name;

	verticalBox = addVerticalStepBox(i, shadesSize, stepsPerBar, shades);

	button = getButton(text, onControllerStep, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);
	gtk_widget_set_sensitive(button,
	  (step->lockStatus == lockStatusUnsettable) ? FALSE :
	  (step->lockStatus == lockStatusUnunsettable) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 1) :
	  (step->lockStatus == lockStatusFree) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 0) :
	  FALSE);

	button = getButton((step->lockStatus == lockStatusUserlocked) ?
	  "!LOCK" : "LOCK", onLockController, step);
	gtk_container_add(GTK_CONTAINER(verticalBox), button);
	gtk_widget_set_sensitive(button,
	  (step->lockStatus == lockStatusUnsettable) ? FALSE :
	  (step->lockStatus == lockStatusUnunsettable) ?
	  (g_slist_length(GET_VALUES(current.pattern)) > 1) :
	  (g_slist_length(GET_VALUES(current.pattern)) > 0));
}


#define addStep(p, s, i, ss, spb, sh, l) \
  (PATTERN_TYPE(p) == PATTERNTYPE_DUMMY) ? \
  addDummyStep((dummyUserStep_t *) s, i, ss, spb, sh) : \
  (PATTERN_TYPE(p) == PATTERNTYPE_NOTE) ? \
  addNoteStep((noteUserStep_t *) s, i, ss, spb, sh, l) : \
  addControllerStep((controllerUserStep_t *) s, i, ss, spb, sh)

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
		addStep(current.pattern, userStep, i, shadesSize, userStepsPerBar, shades, last);
	}

	gtk_widget_show_now(appFrame.middleArea);

	free(shades);
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
	  (value > (current.pattern->real.parent->isRoot ? 1 :
	  current.pattern->real.parent->real.bars)));
	gtk_widget_set_sensitive(numberpicker.upButton, ((value * 2) > 0));
}

static GtkWidget *getNumberPickerDialog(void (*pickValue)(int type),
  void (*onOk)(GtkWidget *widget, gpointer data), int initial)
{
	GtkWidget *result = NULL;
	GtkWidget *box = NULL;
	GtkWidget *contentArea = NULL;

	result = getDialog(200, 150);

	//contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

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

static void doSetBarsCallback(GtkWidget *widget, gpointer data)
{
	adjustSteps(current.pattern,
	  atoi(gtk_label_get_text(GTK_LABEL(numberpicker.label))),
	  current.pattern->real.userStepsPerBar, lockCount);

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
	  atoi(gtk_label_get_text(GTK_LABEL(numberpicker.label))), lockCount);
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
	  (value > (current.pattern->real.parent->isRoot ? 1 :
	  current.pattern->real.parent->real.userStepsPerBar)));
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

static void doSetupNoteValueCallback(GtkWidget *widget, gpointer data)
{
	noteValue_t *noteValue = data;
	char tone = 0;
	int8_t octave =
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(setupvalue.note.octaveSpinButton));
	gboolean sharp = FALSE;
	const char *name = gtk_entry_get_text(GTK_ENTRY(setupvalue.nameEntry));

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

	setupNoteValue(current.pattern, name, sharp, octave, tone, noteValue);
	renderPattern();

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
		  g_slist_append((GSList *) current.pattern->real.controller.values, value);
		if (name[0] == '\0') {
			name = intToString(intValue);
		}
		value->event = getAlsaEvent();
	} else {
		free((char *) value->name);
	}

	value->value = intValue;
	value->name = strdup(name);

	snd_seq_ev_set_controller(value->event, (current.pattern->real.channel - 1),
	  current.pattern->real.controller.parameter, value->value);

	renderPattern();
	runDialog(getValuesDialog());
}

static GtkWidget *getSetupControllerValueDialog(controllerValue_t *value)
{
	GtkWidget *box = NULL;
	GtkWidget *label = NULL;
	char *name = (value == NULL) ? "" : (char *) value->name;
	gdouble curValue = (value == NULL) ? 0 : value->value;
	GtkWidget *result = getDialog(400, 300);
	GtkWidget *contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	setupvalue.isNote = FALSE;

	setupvalue.nameEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(setupvalue.nameEntry), name);

	label = gtk_label_new("name:");

	box = getBox(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), setupvalue.nameEntry, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);

	setupvalue.controller.valueSpinButton =
	  getSpinButton(0, 127, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(
	  setupvalue.controller.valueSpinButton), curValue);


	label = gtk_label_new("controller value:");

	box = getBox(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), setupvalue.controller.valueSpinButton, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea),
	  box, TRUE, TRUE, 0);

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
	GtkWidget *box = NULL;
	GtkWidget *label = NULL;
	char *name = (value == NULL) ? "" : (char *) value->name;
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

	box = getBox(GTK_ORIENTATION_VERTICAL);
	label = gtk_label_new("name:");
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), setupvalue.nameEntry, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), notesBox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(contentArea), setupvalue.note.sharpCheckButton, TRUE, TRUE, 0);

	box = getBox(GTK_ORIENTATION_VERTICAL);
	label = gtk_label_new("octave:");
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), setupvalue.note.octaveSpinButton, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);

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

/*static GtkWidget *getRandomiseDialog(void)
{
	//TODO
	GtkWidget *result = getDialog(400, 150);
	GtkWidget *contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));
	GtkWidget *vbox = getBox(GTK_ORIENTATION_VERTICAL);
	GtkWidget *hbox = getBox(GTK_ORIENTATION_HORIZONTAL);

	gtk_box_pack_start(GTK_BOX(contentArea),
	  (vbox), TRUE, TRUE, 0);

	hbox = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(vbox),
	  (hbox), TRUE, TRUE, 0);

	//TODO: show all steps or only current bar?
	for (uint32_t i = 0; i < current.pattern->real.userStepsPerBar; i++) {
		void *parentStep = getParentStep(current.pattern, i);
		gboolean enabled = FALSE;
		if (mayNotUnset(i)) {
			enabled = (g_slist_length(GET_VALUES(current.pattern)) > 1);
		} else {
			enabled = ((parentStep == NULL) ||
			  (IS_SET(current.pattern->real.parent, parentStep)));
		}
		GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 1);
		gtk_widget_set_sensitive(scale, enabled);
		gtk_box_pack_start(GTK_BOX(hbox), (scale), TRUE, TRUE, 0);
		gtk_range_set_value((GtkRange *) scale, enabled ? 50 : 0);
	}

	gtk_widget_show_all(result);
	return result;
}*/

static void onRandomise(GtkWidget *widget, gpointer data)
{
	//runDialog(getRandomiseDialog());
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
	enterPattern((pattern_t *) current.pattern->real.parent);
}

static void onTop(GtkWidget *widget, gpointer data)
{
	enterPattern((pattern_t *) ((pattern_t *) &rootPattern));
}

static void enterPatternCallback(GtkWidget *widget, gpointer data)
{
	enterPattern((pattern_t *) data);
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

	free((char *) controllerValue->name);
	free((snd_seq_event_t *) controllerValue->event);
}

void destroyNoteValuesCb(gpointer data, gpointer user_data)
{
	noteValue_t *noteValue = data;

	free((char *) noteValue->name);
	free((snd_seq_event_t *) noteValue->snd_seq_event);
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

	free((char *) pattern->real.name);
	userSteps = ADDR_USER_STEPS(pattern);
	eventSteps = ADDR_EVENT_STEPS(pattern);

	free(*userSteps);
	if (eventSteps != NULL) {
		free(*eventSteps);
	}

	if (pattern->real.type == PATTERNTYPE_CONTROLLER) {
		g_slist_foreach((GSList *) pattern->real.controller.values,
		  destroyControllerValueCb, NULL);
		g_slist_free((GSList *) pattern->real.controller.values);
	} else if (pattern->real.type == PATTERNTYPE_NOTE) {
		g_slist_foreach((GSList *) pattern->real.note.values,
		  destroyNoteValuesCb, NULL);
		g_slist_free((GSList *) pattern->real.note.values);
	}

	g_slist_foreach((GSList *) pattern->children, destroyPatternCb, NULL);

finish:
	free(pattern);
	UNLOCK();
}

static struct {
	pattern_t *parent;
	pattern_t *hideMe;
} patternlist;

static pattern_t *createRealPattern(const gchar *name, gint channel,
  patternType_t patternType, gint controller)
{
	return createRealPattern2(patternlist.parent, name, channel,
	  patternType, controller);
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

	//XXXX
	//setSteps(pattern);
	adjustSteps(pattern, pattern->real.bars, pattern->real.userStepsPerBar, lockCount);

	LOCK();
	patternlist.parent->children =
	  g_slist_append((GSList *) patternlist.parent->children, pattern);
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
	GtkWidget *bx = NULL;
	GtkWidget *label = NULL;
	GtkWidget *contentArea = NULL;

	result = getDialog(400, 300);

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));
	box = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);

	bx = getBox(GTK_ORIENTATION_VERTICAL);

	label = gtk_label_new("name:");
	gtk_box_pack_start(GTK_BOX(bx), label, TRUE, TRUE, 0);
	createpattern.nameEntry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(bx), createpattern.nameEntry, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), bx, TRUE, TRUE, 0);

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

	bx = getBox(GTK_ORIENTATION_VERTICAL);

	label = gtk_label_new("channel:");
	gtk_box_pack_start(GTK_BOX(bx), label, TRUE, TRUE, 0);
	createpattern.channelSpinButton = getSpinButton(1, 16, 1);
	setChannelEnabled();
	gtk_box_pack_start(GTK_BOX(bx), createpattern.channelSpinButton, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), bx, TRUE, TRUE, 0);



	bx = getBox(GTK_ORIENTATION_VERTICAL);

	label = gtk_label_new("controller:");
	gtk_box_pack_start(GTK_BOX(bx), label, TRUE, TRUE, 0);
	createpattern.controllerSpinButton = getSpinButton(1, 127, 1);
	gtk_box_pack_start(GTK_BOX(bx), createpattern.controllerSpinButton, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(contentArea), bx, TRUE, TRUE, 0);

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
	  g_slist_delete_link((GSList *) pattern->real.parent->children, link);
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

	for (cur = (GSList *) patternlist.parent->children; cur != NULL; cur = g_slist_next(cur)) {
		pattern_t *child = cur->data;
		if (child == hideMe) {
			continue;
		}
		GtkWidget *box = getBox(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(contentArea), box, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(box),
		  gtk_label_new((char *) child->real.name), TRUE, TRUE, 0);
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
	onPatternList((pattern_t *) current.pattern->real.parent, current.pattern);

}

static void onAnticipationChanged(GtkRange *range, gpointer  user_data)
{
	anticipation = gtk_range_get_value(GTK_RANGE(appFrame.anticipationScale));
}

static void onMaxSleepNanosChanged(GtkRange *range, gpointer  user_data)
{
	maxSleepNanos = gtk_range_get_value(GTK_RANGE(appFrame.maxSleepNanosScale));
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

static void readWriteFd(void *data, uint32_t length, int fd,
  gboolean writ, err_t *e)
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
		terror(readWriteFd((void *) &length, sizeof(length), fd, store, e))
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
	terror(readWriteFd((void *) &(dummyUserStep->set), sizeof(dummyUserStep->set), fd, store, e))

finish:
	return;
}

static void loadStoreNoteUserStep(pattern_t *pattern, noteUserStep_t *noteUserStep,
  GSList *noteValues, int fd, gboolean store, err_t *e)
{
	gint position = -1;
	gboolean slide = FALSE;

	if (store) {
		position = g_slist_position((GSList *) noteValues, (GSList *) noteUserStep->value);
		if (noteUserStep->value != NULL) {
			slide = noteUserStep->slide;
		}
	}

	terror(readWriteFd(&slide, sizeof(slide), fd, store, e))
	terror(readWriteFd(&position, sizeof(position), fd, store, e))

	if (!store) {
		if (position > -1) {
			setNoteStep(pattern, noteUserStep, g_slist_nth(noteValues , position), lockCount);
		}
		setNoteSlide(pattern, noteUserStep, slide, lockCount);
	}

finish:
	return;
}

static void loadStoreControllerUserStep(pattern_t *pattern,
  controllerUserStep_t *controllerUserStep, GSList *controllerValues,
  int fd, gboolean store, err_t *e)
{
	gint position = -1;

	if ((store)&&(controllerUserStep->value != NULL)) {
		position = g_slist_position((GSList *) controllerValues, (GSList *) controllerUserStep->value);
	}

	terror(readWriteFd(&position, sizeof(position), fd, store, e))

	if ((!store)&&(position > -1)) {
		setControllerStep(pattern, controllerUserStep,
		  g_slist_nth(controllerValues , position), lockCount);
	}


finish:
	return;
}

static void loadStoreDummyPattern(pattern_t **pattern, int fd, gboolean store, err_t *e)
{
	uint32_t steps = (*pattern)->real.userStepsPerBar * (*pattern)->real.bars;

	for (uint32_t i = 0; i < steps; i++) {
		terror(loadStoreDummyUserStep((dummyUserStep_t *) &((*pattern)->real.dummy.steps[i]),
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
		terror(writeStringToFd((char *) noteValue->name, fd, e))
	} else {
		terror(noteValue->name = readStringFromFd(fd, e))
	}
	terror(readWriteFd((void *) &(noteValue->tone), sizeof(noteValue->tone), fd, store, e))
	terror(readWriteFd((void *) &(noteValue->octave), sizeof(noteValue->octave), fd, store, e))
	terror(readWriteFd((void *) &(noteValue->sharp), sizeof(noteValue->sharp), fd, store, e))

finish:
	return;
}

static void loadStoreControllerValue(controllerValue_t *controllerValue, int fd,
  gboolean store, err_t *e)
{
	if (store) {
		terror(writeStringToFd((char *) controllerValue->name, fd, e))
	} else {
		terror(controllerValue->name = readStringFromFd(fd, e))
	}
	terror(readWriteFd((void *) &(controllerValue->value),
	  sizeof(controllerValue->value), fd, store, e))

finish:
	return;
}

static void loadStoreNotePattern(pattern_t **pattern, int fd, gboolean store, err_t *e)
{
	uint32_t i = 0;
	uint32_t noteValues = g_slist_length((GSList *) (*pattern)->real.note.values);
	uint32_t steps = (*pattern)->real.userStepsPerBar * (*pattern)->real.bars;
	noteValue_t *noteValue = NULL;

	terror(readWriteFd(&noteValues, sizeof(noteValues), fd, store, e))

	if (store) {
		GSList *cur = NULL;
		for (cur = (GSList *) (*pattern)->real.note.values; cur != NULL; cur = g_slist_next(cur)) {
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
			  g_slist_append((GSList *) (*pattern)->real.note.values, noteValue);
			noteValue = NULL;
		}
	}

	for (i = 0; i < steps; i++) {
		terror(loadStoreNoteUserStep(*pattern,
		  (noteUserStep_t *) &((*pattern)->real.note.steps.user[i]),
		  (GSList *) (*pattern)->real.note.values, fd, store, e))
	}

finish:
	free(noteValue);
}

static void loadStoreControllerPattern(pattern_t **pattern, int fd,
  gboolean store, err_t *e)
{
	uint32_t i = 0;
	GSList *cur = NULL;
	uint32_t controllerValues = g_slist_length((GSList *) (*pattern)->real.controller.values);
	uint32_t steps = (*pattern)->real.userStepsPerBar * (*pattern)->real.bars;
	controllerValue_t *controllerValue = NULL;

	terror(readWriteFd((void *) &(*pattern)->real.controller.parameter,
	  sizeof((*pattern)->real.controller.parameter), fd, store, e))

	terror(readWriteFd(&controllerValues,
	  sizeof(controllerValues), fd, store, e))

	if (store) {
		for (cur = (GSList *) (*pattern)->real.controller.values; cur != NULL; cur = g_slist_next(cur)) {
			terror(loadStoreControllerValue(cur->data, fd, store, e))
		}
	} else {
		for (i = 0; i < controllerValues; i++) {
			controllerValue = calloc(1, sizeof(controllerValue_t));
			terror(loadStoreControllerValue(controllerValue, fd, store, e))
			(*pattern)->real.controller.values =
			  g_slist_append((GSList *) (*pattern)->real.controller.values, controllerValue);
			controllerValue = NULL;
		}
	}
	for (i = 0; i < steps; i++) {
		terror(loadStoreControllerUserStep((pattern_t *) *pattern,
		  (controllerUserStep_t *) &((*pattern)->real.controller.steps.user[i]),
		  (GSList *) (*pattern)->real.controller.values, fd, store, e))
	}

finish:
	free(controllerValue);
}

static void loadStoreRealPattern(pattern_t **pattern, int fd, gboolean store,
  pattern_t *parent, err_t *e)
{
	if (store) {
		terror(writeStringToFd((char *) (*pattern)->real.name, fd, e))
	} else {
		(*pattern) = allocatePattern(parent);
		(*pattern)->real.parent = parent;
		(*pattern)->isRoot = FALSE;
		terror((*pattern)->real.name = readStringFromFd(fd, e))
	}

	terror(readWriteFd((void *) &((*pattern)->real.channel),
	  sizeof((*pattern)->real.channel), fd, store, e))
	terror(readWriteFd((void *) &((*pattern)->real.userStepsPerBar),
	  sizeof((*pattern)->real.userStepsPerBar), fd, store, e))
	terror(readWriteFd((void *) &((*pattern)->real.bars),
	  sizeof((*pattern)->real.bars), fd, store, e))
	terror(readWriteFd((void *) &((*pattern)->real.type),
	  sizeof((*pattern)->real.type), fd, store, e))

	if (!store) {
		adjustSteps((*pattern), (*pattern)->real.bars, (*pattern)->real.userStepsPerBar, lockCount);
		//XXXXXXXXXX
		//setSteps((*pattern));
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
		terror(readWriteFd((void *) &((*pattern)->isRoot),
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
	terror(loadStoreChildren((GSList **) &((*pattern)->children), fd, store, (*pattern), e))
finish:
	if ((hasFailed(e))&&(!store)&&((*pattern) != NULL)) {
		destroyPattern((*pattern), TRUE); (*pattern) = NULL;
	}
}


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
		  g_slist_append((GSList *) rootPattern.children, pattern); pattern = NULL;
		UNLOCK();
	} else {
		LOCK();
		rootPattern.children =
		  g_slist_concat((GSList *) rootPattern.children, (GSList *) pattern->children);
		g_slist_foreach((GSList *) pattern->children, setParentCb, &rootPattern);
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
	  current.pattern->isRoot ? NULL : (pattern_t *) current.pattern->real.parent, e))
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

static GtkWidget *getLoadStoreView(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_HORIZONTAL);

	gtk_box_pack_start(GTK_BOX(result),
	  (appFrame.loadButton = getButton("Load ...", onLoad, NULL)), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result),
	  (appFrame.storeButton = getButton("Store ...", onStore, NULL)), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getPreviousNextView(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_HORIZONTAL);

	gtk_box_pack_start(GTK_BOX(result),
	  (appFrame.previousButton = getButton("<<", onPrevious, NULL)), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result),
	  (appFrame.nextButton = getButton(">>", onNext, NULL)), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getLoadStorePreviousNextView(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_VERTICAL);

	gtk_box_pack_start(GTK_BOX(result), getLoadStoreView(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result), getPreviousNextView(), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getParentTopView(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_HORIZONTAL);

	gtk_box_pack_start(GTK_BOX(result), (appFrame.parentButton =
	  getButton("Parent!", onParent, NULL)), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result), (appFrame.topButton =
	  getButton("Top!", onTop, NULL)), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getChildrenSiblingsView(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_HORIZONTAL);

	gtk_box_pack_start(GTK_BOX(result), (appFrame.childrenButton =
	  getButton("Children ...", onChildren, NULL)), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result), (appFrame.siblingsButton =
	  getButton("Siblings ...", onSiblings, NULL)), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getTempoParentTopView(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_VERTICAL);

	gtk_box_pack_start(GTK_BOX(result),
	  (appFrame.tempoLabel = gtk_label_new(NULL)), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result),
	  getParentTopView(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result),
	  getChildrenSiblingsView(), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getGeneralView(void)
{
	GtkWidget *box = NULL;
	GtkWidget *result = getBox(GTK_ORIENTATION_HORIZONTAL);

	gtk_range_set_value((GtkRange *)appFrame.anticipationScale, anticipation);
	g_signal_connect(G_OBJECT(appFrame.anticipationScale),
	  "value-changed", G_CALLBACK(onAnticipationChanged), NULL);
	gtk_range_set_value((GtkRange *)appFrame.maxSleepNanosScale, maxSleepNanos);
	g_signal_connect(G_OBJECT(appFrame.anticipationScale),
	  "value-changed", G_CALLBACK(onMaxSleepNanosChanged), NULL);

	box = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(result), box, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(box),
	  getTempoParentTopView(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box),
	  getLoadStorePreviousNextView(), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getAdminView(void)
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
	appFrame.randomiseButton = getButton("Randomise!", onRandomise, NULL);
	gtk_box_pack_start(GTK_BOX(line2), appFrame.randomiseButton, TRUE, TRUE, 0);

	return result;
}

void connectPort(int client, int port, err_t *e)
{
	pthread_mutex_lock(((pthread_mutex_t *) &(syncEvents.mutex)));

	while (syncEvents.queue.head != NULL) {
		syncEvent_t *syncEvent = (syncEvent_t *) syncEvents.queue.head;
		syncEvents.queue.head = syncEvent->next;
		free((syncEvent_t *) syncEvent);
	}

	LOCK();

	pthread_mutex_unlock(((pthread_mutex_t *) &(syncEvents.mutex)));

	if (sequencer.connected) {
		terror(failIfFalse(snd_seq_disconnect_from(sequencer.value, sequencer.myPort,
		  sequencer.client, sequencer.port) == 0))
		terror(failIfFalse(snd_seq_disconnect_to(sequencer.value,
		  sequencer.myPort, sequencer.client, sequencer.port) == 0))
	}

	terror(failIfFalse(snd_seq_connect_from(sequencer.value, 0, client, port) == 0))
	terror(failIfFalse(snd_seq_connect_to(sequencer.value, 0, client, port) == 0))

	sequencer.client = client;
	sequencer.port = port;
	sequencer.connected = TRUE;

finish:
	UNLOCK();
}

static 	void onPortComboChanged(GtkWidget *widget, gpointer data)
{
	GtkComboBox *combo = data;
	GtkTreeIter iter;
	guint client = 0;
	guint port = 0;
	gchar *clientName = NULL;
	gchar *portName = NULL;
	GtkTreeModel *model = NULL;
	err_t err;
	err_t *e = &err;
	initErr(e);

	terror(failIfFalse(gtk_combo_box_get_active_iter(combo, &iter)))
	terror(failIfFalse(((model = gtk_combo_box_get_model(combo)) != NULL)))

	gtk_tree_model_get(model, &iter, 0, &client, -1);
	gtk_tree_model_get(model, &iter, 1, &port, -1);
	gtk_tree_model_get(model, &iter, 2, &clientName, -1);
	gtk_tree_model_get(model, &iter, 3, &portName, -1);

	connectPort(client, port, e);

#if 0
	g_print("Selected (complex): >> %s <<\n", string != NULL ? string : "NULL");
#endif

finish:
	if (clientName != NULL) {
		g_free(clientName);
	}
	if (portName != NULL) {
		g_free(portName);
	}
}

static GtkWidget *getPortComboView(void)
{
	void addPorts(GtkListStore *liststore) {
		snd_seq_client_info_t *cinfo;
		snd_seq_port_info_t *pinfo;

		snd_seq_client_info_alloca(&cinfo);
		snd_seq_port_info_alloca(&pinfo);

		snd_seq_client_info_set_client(cinfo, -1);
		LOCK();
		while (snd_seq_query_next_client(sequencer.value, cinfo) >= 0) {
			int client = snd_seq_client_info_get_client(cinfo);

			snd_seq_port_info_set_client(pinfo, client);
			snd_seq_port_info_set_port(pinfo, -1);
			while (snd_seq_query_next_port(sequencer.value, pinfo) >= 0) {
				/* port must understand MIDI messages */
				if (!(snd_seq_port_info_get_type(pinfo)
				  & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) {
					continue;
				}
				/* we need both WRITE and SUBS_WRITE */
				if ((snd_seq_port_info_get_capability(pinfo)
				  & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
				  != (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)) {
					continue;
				}
				gtk_list_store_insert_with_values(liststore, NULL, -1,
				  0, snd_seq_port_info_get_client(pinfo),
				  1, snd_seq_port_info_get_port(pinfo),
				  2, snd_seq_client_info_get_name(cinfo),
				  3, snd_seq_port_info_get_name(pinfo),
				  -1);
			}
		}
		UNLOCK();
	}

	GtkWidget *result = NULL;

	GtkCellRenderer *column = NULL;
	GtkListStore *liststore = NULL;
	//------------------------------------
	liststore = gtk_list_store_new(4, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING);

	addPorts(liststore);
#if 0
	gtk_list_store_insert_with_values(liststore, NULL, -1,
                                      0, "Don't install.",
								      1, "1",
                                      -1);
    gtk_list_store_insert_with_values(liststore, NULL, -1,
                                      0, "This user only.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
								      1, "2",
                                      -1);
    gtk_list_store_insert_with_values(liststore, NULL, -1,
                                      0, "All users.",
								      1, "3",
                                      -1);
#endif
	result = gtk_combo_box_new_with_model(GTK_TREE_MODEL(liststore));
	g_signal_connect(G_OBJECT(result), "changed", G_CALLBACK(onPortComboChanged), result);

	g_object_unref(liststore);


	column = gtk_cell_renderer_text_new();

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(result), column, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(result), column,
                                   "text", 2,
                                   NULL);

    gtk_combo_box_set_active(GTK_COMBO_BOX(result), 0);


	//------------------------------------
	return result;
}

static GtkWidget *getWideUpperView(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_VERTICAL);

	gtk_box_pack_start(GTK_BOX(result), gtk_label_new("port"), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result), (appFrame.portCombo = getPortComboView()), TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(result), gtk_label_new("anticipation"), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result), (appFrame.anticipationScale =
	  gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 25000000, 1)), TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(result), gtk_label_new("maximum sleep nanosecs"), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result), (appFrame.maxSleepNanosScale =
	  gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, ((uint32_t) -1), 1)), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getGeneralAndAdminView(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_HORIZONTAL);

	gtk_box_pack_start(GTK_BOX(result), getGeneralView(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result), getAdminView(), TRUE, TRUE, 0);

	return result;
}

static GtkWidget *getUpperArea(void)
{
	GtkWidget *result = NULL;
	GtkWidget *box = NULL;

	result = getBox(GTK_ORIENTATION_VERTICAL);

	gtk_box_pack_start(GTK_BOX(result), getWideUpperView(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(result),
	  (box = getGeneralAndAdminView()), TRUE, TRUE, 0);

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

	gtk_box_pack_start(GTK_BOX(wholeArea), getUpperArea(), TRUE, TRUE, 0);
	appFrame.middleArea = getBox(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(wholeArea), appFrame.middleArea, TRUE, TRUE, 0);

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

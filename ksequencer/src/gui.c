#include <math.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "ksequencer.h"

#if 0
#define SIGNALLING_MUTEX
#endif

#define FREE_LABELS

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define EXTENSION ".ksq"

DECLARE_LOCKCONTEXT

typedef enum {
	pickTypeDummy,
	pickTypeMore,
	pickTypeLess
} pickType_t;

typedef void (*fnPickNumber_t)(pickType_t pickType);

static struct {
	char name;
	gboolean sharpable;
} tones[] =
{
	{ 'c', TRUE },
	{ 'd', TRUE },
	{ 'e', FALSE },
	{ 'f', TRUE },
	{ 'g', TRUE },
	{ 'a', TRUE },
	{ 'b', FALSE }
};

static struct {
	pattern_t *pattern;
	gboolean isNote;
	GtkWidget *nameEntry;
	void *value;
	union {
		struct {
			GtkWidget *valueSpinButton;
		} controller;
		struct {
			GtkWidget *toneRadios[ARRAY_SIZE(tones)];
			GtkWidget *sharpCheckButton;
			GtkWidget *octaveSpinButton;
		} note;
	};
} setupValue;

static struct {
	GtkWidget *label;
	GtkWidget *upButton;
	GtkWidget *downButton;
	fnPickNumber_t fnPickNumber;
	void *ptr;
} numberPicker;

static struct {
	uint32_t nextPattern;
	pattern_t *parent;
	patternType_t patternType;
	GtkWidget *nameEntry;
	GtkWidget *dummyRadio;
	GtkWidget *noteRadio;
	GtkWidget *controllerRadio;
	GtkWidget *channelSpinButton;
	GtkWidget *controllerSpinButton;
} createPattern = {
	.nextPattern = 0
};

static struct {
	pattern_t *parent;
	pattern_t *hideMe;
} patternList;

static struct {
	uint64_t step;
	pattern_t *pattern;
	uint64_t generation;
	uint64_t lastGeneration;
	GtkWidget *lastLabel;
#ifdef SIGNALLING_MUTEX
	pthread_mutex_t mutex;
#endif
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

struct {
	pattern_t *pattern;
	uint32_t bar;
} current = {
	.pattern = NULL,
	.bar = 0
};

#define ELEVEN (11)
#define STRING_NOT_RUNNING sequencer.connected ? "not running" : "disconnected"

static GtkWidget *speedLabel = NULL;
static GtkWidget *topWindow = NULL;
static GtkWidget *stepsBox = NULL;
static GtkWidget *loadButton = NULL;
static GtkWidget *storeButton = NULL;
static GtkWidget *parentButton = NULL;
static GtkWidget *topButton = NULL;
static GtkWidget *barsButton = NULL;
static GtkWidget *stepsPerBarButton = NULL;
static GtkWidget *childrenButton = NULL;
static GtkWidget *siblingsButton = NULL;
static GtkWidget *previousButton = NULL;
static GtkWidget *nextButton = NULL;
static GtkWidget *valuesButton = NULL;
static GtkWidget *randomiseButton = NULL;
static GtkWidget *stepsView = NULL;
static GtkWidget *dialog = NULL;
static uint64_t nanosecondsPerCrotchet = 0;
static gboolean randomising = FALSE;
static GIOChannel *gIoChannel = NULL;

#define addWidget(b, w) gtk_box_pack_start(GTK_BOX((b)), (w), TRUE, TRUE, 0)
#define setEnabled(w, e) gtk_widget_set_sensitive((w), (e))

static char *intToString(int32_t number)
{
	static char string[ELEVEN];

	snprintf(string, sizeof(string), "%d", number);

	return string;
}

static void prepareShutdown(void)
{
#ifdef FREE_LABELS
	g_slist_free(signalling.labels);
#endif
	signalling.labels = NULL;
	signalling.lastLabel = NULL;

	disconnectFromPort(&lockContext, NULL);
}

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

static gboolean doSignalStop(gpointer user_data)
{
	unsignalStep();

	return FALSE;
}

static gboolean doSignalSpeed(gpointer user_data)
{
	uint32_t speed;
	uint64_t nPC = nanosecondsPerCrotchet;
	char buffer[100];

	if (nPC < 1) {
		gtk_label_set_label(GTK_LABEL(speedLabel),
		  STRING_NOT_RUNNING);
		goto finish;
	}
	speed = (uint32_t) (((((long double) 1000000000) /
	  ((long double) nPC))) * 60);
	snprintf(buffer, sizeof(buffer), "at %u bpm", speed);
	gtk_label_set_label(GTK_LABEL(speedLabel),
	  buffer);

finish:
	return FALSE;
}

static gboolean doSignalStep(gpointer user_data)
{
	uint64_t minStep = 0;
	uint64_t maxStep = 0;
	uint64_t step = signalling.step;
#ifdef SIGNALLING_MUTEX
	pthread_mutex_lock(&(signalling.mutex));
#endif

	if (signalling.labels == NULL) {
		goto finish;
	}

	step /= (MAX_EVENTSTEPS_PER_BAR / signalling.userStepsPerBar);
	step %= (signalling.userStepsPerBar * signalling.bars);
	minStep = signalling.bar * signalling.userStepsPerBar;
	maxStep = minStep + signalling.userStepsPerBar - 1;

	unsignalStep();

	if ((step < minStep) || (step > maxStep)) {
		signalling.lastLabel = NULL;
		goto finish;
	}

	step -= minStep;


	signalling.lastLabel = g_slist_nth_data(signalling.labels, step);

	gtk_label_set_label(GTK_LABEL(signalling.lastLabel), "X");
	signalling.lastGeneration = signalling.generation;
finish:
#ifdef SIGNALLING_MUTEX
	pthread_mutex_unlock(&(signalling.mutex));
#endif
	return FALSE;
}

void gtkSignalSpeed(uint64_t nPC)
{
	nanosecondsPerCrotchet = nPC;

	g_idle_add(doSignalSpeed, NULL);
}

void gtkSignalStep(uint64_t step)
{
	signalling.step = step;

	g_idle_add(doSignalStep, NULL);
}

void gtkSignalStop()
{
	g_idle_add(doSignalStop, NULL);

	gtkSignalSpeed(0);
}

static gboolean deliverSignal(GIOChannel *source, GIOCondition condition,
  gpointer d)
{
	gchar buffer[sizeof(int)];
	gsize count = 0;

	while (g_io_channel_read_chars(source, buffer, sizeof(buffer), &count,
	  NULL) == G_IO_STATUS_NORMAL);

	prepareShutdown();

	gtk_main_quit();

	return TRUE;
}

static GIOChannel *doSignals(err_t *e)
{
	GIOChannel *_result = NULL;
	GIOChannel *result = NULL;

	terror(failIfFalse(((_result =
	  g_io_channel_unix_new(terminate.pipe[PIPE_RD])) != NULL)))
	terror(failIfFalse((g_io_channel_set_flags(_result,
	  g_io_channel_get_flags(_result) | G_IO_FLAG_NONBLOCK, NULL) ==
	  G_IO_STATUS_NORMAL)))
	g_io_add_watch(_result, (G_IO_IN | G_IO_PRI), deliverSignal, NULL);

	result = _result; _result = NULL;
finish:
	if (_result != NULL)  {
		g_io_channel_shutdown(_result, TRUE, NULL);
	}
	return result;
}

static GtkWidget *getButton(char *text, GCallback callback,
  gpointer data)
{
	GtkWidget *result = gtk_button_new_with_label(text);

	if (callback != NULL) {
		g_signal_connect(G_OBJECT(result), "clicked",
		  G_CALLBACK(callback), data);
	}

	return result;
}

static GtkWidget *getLabel(char *text)
{
	GtkWidget *result = gtk_label_new(text);

	return result;
}

static GtkWidget *getBox(GtkOrientation orientation)
{
	GtkWidget *result = gtk_box_new(orientation, 0);

	gtk_box_set_homogeneous(GTK_BOX(result), TRUE);

	return result;
}

static GtkWidget *getBoxWithLabel(GtkOrientation orientation, char *text)
{
	GtkWidget *result = getBox(orientation);
	GtkWidget *label = getLabel(text);

	addWidget(result, label);

	return result;
}

static GtkListStore *getPortsListStore(void)
{
	GtkListStore *result = gtk_list_store_new(4, G_TYPE_UINT, G_TYPE_UINT,
	  G_TYPE_STRING, G_TYPE_STRING);

	for (GSList *cur = midiPorts; cur != NULL; cur = g_slist_next(cur)) {
		midiPort_t *midiPort = cur->data;

		gtk_list_store_insert_with_values(result, NULL, -1,
		  0, midiPort->client,
		  1, midiPort->port,
		  2, midiPort->clientName,
		  3, midiPort->portName,
		  -1);
	}

	return result;
}

static void cbPortsComboChanged(GtkWidget *widget, gpointer data)
{
#if 0
	gchar *clientName = NULL;
	gchar *portName = NULL;
#endif
	GtkTreeIter iter;
	guint client = 0;
	guint port = 0;
	GtkTreeModel *model = NULL;
	err_t err;
	err_t *e = &err;
	GtkComboBox *combo = data;

	initErr(e);

	terror(failIfFalse(gtk_combo_box_get_active_iter(combo, &iter)))
	model = gtk_combo_box_get_model(combo);

	gtk_tree_model_get(model, &iter, 0, &client, -1);
	gtk_tree_model_get(model, &iter, 1, &port, -1);
#if 0
	gtk_tree_model_get(model, &iter, 2, &clientName, -1);
	gtk_tree_model_get(model, &iter, 3, &portName, -1);
#endif

	terror(connectToPort(&lockContext, client, port, e))

finish:
#if 0
	if (clientName != NULL) {
		g_free(clientName);
	}
	if (portName != NULL) {
		g_free(portName);
	}
#else
	return;
#endif
}

static GtkWidget *getPortsCombo(void)
{
	GtkListStore *listStore = getPortsListStore();
	GtkWidget *result = gtk_combo_box_new_with_model(GTK_TREE_MODEL(listStore));
	GtkCellRenderer *column = gtk_cell_renderer_text_new();

	g_signal_connect(G_OBJECT(result),
	  "changed", G_CALLBACK(cbPortsComboChanged), result);

	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(result), column, TRUE);

	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(result), column,
	  "text", 2, NULL);
	gtk_combo_box_set_active(GTK_COMBO_BOX(result), 0);

	g_object_unref(listStore);

	return result;
}

static GtkWidget *getMidiPortBox(void)
{
	GtkWidget *result = getBoxWithLabel(GTK_ORIENTATION_VERTICAL, "Port");
	GtkWidget *portsCombo = getPortsCombo();

	addWidget(result, portsCombo);

	return result;
}

#if 0
static void cbScaleChanged(GtkRange *range, gpointer  user_data)
{
	uint32_t *value = (uint32_t *) user_data;
	*value = gtk_range_get_value(range);
}

static GtkWidget *getBoxWithScale(char *label, uint32_t min, uint32_t max,
  uint32_t *value)
{
	GtkWidget *result = getBoxWithLabel(GTK_ORIENTATION_VERTICAL, label);
	GtkWidget *scale =
	  gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, 1);

	if (value != NULL) {
		gtk_range_set_value((GtkRange *) scale, (*value));
		g_signal_connect(G_OBJECT(scale),
		  "value-changed", G_CALLBACK(cbScaleChanged), value);
	}
	addWidget(result, scale);

	return result;
}
#endif

#if 0
static GtkWidget *getAnticipationBox(void)
{
	GtkWidget *result = getBoxWithScale("Anticipation", 0, 25000000, NULL);

	return result;
}

static GtkWidget *getMaximumSleepBox(void)
{
	GtkWidget *result = getBoxWithScale("Maximum Sleep Nanosecs", 0,
	  ((uint32_t) -1), NULL);

	return result;
}
#endif

static GtkWidget *getUpperBox(void)
{
	GtkWidget *result = getBox(GTK_ORIENTATION_VERTICAL);
	GtkWidget *box = getMidiPortBox();

	speedLabel = getLabel(STRING_NOT_RUNNING);

	addWidget(result, box);
	addWidget(result, speedLabel);
#if 0
	box = getAnticipationBox();
	addWidget(result, box);

	box = getMaximumSleepBox();
	addWidget(result, box);
#endif
	return result;
}

static GtkWidget *addButton(GtkWidget *box, char *text, GCallback callback,
  gpointer data)
{
	GtkWidget *result = getButton(text, callback, data);

	addWidget(box, result);

	return result;
}

static void enableButtons(void)
{
	if (IS_ROOT(current.pattern)) {
		goto finish;
	}

	setEnabled(parentButton, TRUE);
	setEnabled(topButton, TRUE);
	setEnabled(barsButton,
	  (g_slist_length(((GSList *) CHILDREN(current.pattern))) < 1));
	setEnabled(stepsPerBarButton,
	  (g_slist_length(((GSList *) CHILDREN(current.pattern))) < 1));
	setEnabled(siblingsButton, TRUE);
	if (current.bar > 0) {
		setEnabled(previousButton, TRUE);
	}
	if (current.bar < (NR_BARS(current.pattern) - 1)) {
		setEnabled(nextButton, TRUE);
	}
	if (NR_VALUES(current.pattern) > 0) {
		setEnabled(randomiseButton, TRUE);
	}
	if (IS_DUMMY(current.pattern)) {
		goto finish;
	}
	setEnabled(valuesButton, TRUE);

finish:
	return;
}

static void renderPattern(void);

static void cbDummyStep(GtkWidget *widget, gpointer data)
{
	err_t err;
	err_t *e = &err;
	dummyUserStep_t *dummyUserStep = NULL;
	uint32_t idx = GPOINTER_TO_UINT(data);

	initErr(e);

	dummyUserStep = USERSTEP_AT(current.pattern, idx);
	terror(setDummyStep(current.pattern, dummyUserStep,
	  !(dummyUserStep->set), &lockContext, e))
	renderPattern();

finish:
	return;
}

static void cbNoteStep(GtkWidget *widget, gpointer data)
{
	err_t err;
	err_t *e = &err;
	uint32_t idx = GPOINTER_TO_UINT(data);
	noteUserStep_t *noteUserStep =
	  (noteUserStep_t *) (USERSTEP_AT(current.pattern, idx));
	GSList *value = (noteUserStep->value == NULL) ?
	  VALUES(current.pattern) : g_slist_next(noteUserStep->value);

	initErr(e);

	if ((value == NULL)&&anyChildStepSet(current.pattern, idx)) {
		value = VALUES(current.pattern);
	}

	terror(setNoteStep(current.pattern, noteUserStep,
	  value, idx, &lockContext, TRUE, e))
	renderPattern();

finish:
	return;
}

static void cbControllerStep(GtkWidget *widget, gpointer data)
{
	err_t err;
	err_t *e = &err;
	GSList *value = NULL;
	controllerUserStep_t *controllerUserStep = NULL;
	uint32_t idx = GPOINTER_TO_UINT(data);

	initErr(e);

	controllerUserStep = USERSTEP_AT(current.pattern, idx);
	value = (controllerUserStep->value == NULL) ?
	  VALUES(current.pattern) : g_slist_next(controllerUserStep->value);
	if ((value == NULL)&&anyChildStepSet(current.pattern, idx)) {
		value = VALUES(current.pattern);
	}

	terror(setControllerStep(current.pattern, controllerUserStep,
	  value, idx, &lockContext, e))
	renderPattern();

finish:
	return;
}

static void cbSlide(GtkWidget *widget, gpointer data)
{
	err_t err;
	err_t *e = &err;
	uint32_t idx = GPOINTER_TO_UINT(data);
	noteUserStep_t *noteUserStep =
	  (noteUserStep_t *) (USERSTEP_AT(current.pattern, idx));

	initErr(e);

	terror(setSlide(current.pattern, noteUserStep,
	  (!noteUserStep->slide), idx, &lockContext, TRUE, e))
	renderPattern();

finish:
	return;
}

static void cbLockSlide(GtkWidget *widget, gpointer data)
{
	uint32_t idx = GPOINTER_TO_UINT(data);
	
	lockSlide(current.pattern, idx);
	renderPattern();
}

static void cbLockUserStep(GtkWidget *widget, gpointer data)
{
	uint32_t idx = GPOINTER_TO_UINT(data);
	
	lockUserStep(current.pattern, idx);
	renderPattern();
}

static void addLockButton(GtkWidget *container, gboolean locked,
  gboolean enabled, uint32_t idx, gboolean slide)
{
	GtkWidget *button = NULL;
	char *text = locked ? "!LOCK" : "LOCK";
	GCallback cb = slide ? G_CALLBACK(cbLockSlide) : G_CALLBACK(cbLockUserStep);

	button = addButton(container, text, cb, GUINT_TO_POINTER(idx));
	setEnabled(button, enabled);
}

static GdkColor stepButtonColor = {0, 0xffff, 0xffff, 0xffff};

static void doSetColor(GtkWidget *widget, guint16 color)
{
	gtk_style_context_add_class(gtk_widget_get_style_context(widget), "colorable");
	stepButtonColor.green = color;
	gtk_widget_modify_bg(widget, GTK_STATE_ACTIVE, &stepButtonColor);
	gtk_widget_modify_bg(widget, GTK_STATE_PRELIGHT, &stepButtonColor);
	gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &stepButtonColor);
}

static void setColor(GtkWidget *widget, uint32_t i,
  uint32_t shadesSize, guint16 *shades)
{
	uint32_t maxIndex = (shadesSize - 1);
	uint32_t index = maxIndex;

	if (NR_USERSTEPS_PER_BAR(current.pattern) <= 4) {
		goto haveIndex;
	}
	index = 0;
	while ((index < maxIndex)&&((i % 2) == 0)) {
		index++;
		i /= 2;
	}

haveIndex:
	doSetColor(widget, shades[index]);
}

static void addStepLabels(GtkWidget *container, uint32_t idx,
  guint16 *shades, uint32_t shadesSize)
{
	GtkWidget *label = NULL;

	idx %= NR_USERSTEPS_PER_BAR(current.pattern);

	label = gtk_label_new(intToString(idx + 1));
	addWidget(container, label);
	setColor(label, idx, shadesSize, shades);

	label = gtk_label_new("");
	addWidget(container, label);
	signalling.labels = g_slist_append(signalling.labels, label);
}

static void renderUserStep(GtkWidget *container, pattern_t *pattern,
  uint32_t idx, GCallback cb, guint16 *shades, uint32_t shadesSize)
{
	gboolean locked = FALSE;
	gboolean enabled = FALSE;
	GtkWidget *button = NULL;
	gboolean unlockable = FALSE;
	void *step = USERSTEP_AT(pattern, idx);
	char *text = (char *) DISPLAYTEXT(step, TYPE(pattern));

	addStepLabels(container, idx, shades, shadesSize);

	locked = getLocked(&unlockable, step, pattern, idx);
	addLockButton(container, locked, unlockable, idx, FALSE);

	button = addButton(container, text, cb, GUINT_TO_POINTER(idx));
	enabled = ((NR_VALUES(pattern) > 0)&&(!locked));
	setEnabled(button, enabled);
}

static void renderSlide(GtkWidget *container, pattern_t *pattern, uint32_t idx)
{
	GtkWidget *button = NULL;
	noteUserStep_t *step = ((noteUserStep_t *) USERSTEP_AT(pattern, idx));
	gboolean enabled = IS_SET(step, TYPE(pattern));
	char *text = step->slide ? "!SLIDE" : "SLIDE";

	if (enabled) {
		enabled = !(step->slideLocked);
		if (enabled) {
			enabled = (idx + 1) < NR_USERSTEPS(pattern);
		}
	}

	button = addButton(container, text,
	  G_CALLBACK(cbSlide), GUINT_TO_POINTER(idx));
	setEnabled(button, enabled);

	addLockButton(container, step->slideLocked,
	  IS_SET(step, TYPE(pattern)), idx, TRUE);
}

static void renderSteps(pattern_t *pattern, uint32_t bar)
{
	guint16 *shades = NULL;
	uint32_t shadesSize = 0;
	uint32_t shadeStep = 0;
	int32_t value = 0;
	GCallback callback = CB_STEP(TYPE(pattern));
	uint32_t start = (bar * NR_USERSTEPS_PER_BAR(pattern));
	uint32_t end = (start + NR_USERSTEPS_PER_BAR(pattern));

	if (NR_USERSTEPS_PER_BAR(pattern) <= 4) {
		shadesSize = 1;
	} else {
		shadesSize = ((uint32_t) sqrt(NR_USERSTEPS_PER_BAR(pattern))) - 1;
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

	stepsView = getBox(GTK_ORIENTATION_HORIZONTAL);
	addWidget(stepsBox, stepsView);

#ifdef SIGNALLING_MUTEX
	pthread_mutex_lock(&(signalling.mutex));
#endif

	for (uint32_t i = start; i < end; i++) {
		GtkWidget *buttonBox = NULL;
		buttonBox = getBox(GTK_ORIENTATION_VERTICAL);
		addWidget(stepsView, buttonBox);

		renderUserStep(buttonBox, pattern, i, callback, shades, shadesSize);
		if (!IS_NOTE(pattern)) {
			continue;
		}
		renderSlide(buttonBox, pattern, i);
	}
	free(shades);

	gtk_widget_show_all(stepsView);

	signalling.bar = bar;
	signalling.pattern = pattern;
	signalling.bars = NR_BARS(pattern);
	signalling.userStepsPerBar = NR_USERSTEPS_PER_BAR(pattern);
	signalling.generation++;
#ifdef SIGNALLING_MUTEX
	pthread_mutex_unlock(&(signalling.mutex));
#endif
}

static void renderPattern(void)
{
	setEnabled(loadButton, TRUE);
	setEnabled(storeButton, TRUE);
	setEnabled(childrenButton, TRUE);
	setEnabled(parentButton, FALSE);
	setEnabled(topButton, FALSE);
	setEnabled(barsButton, FALSE);
	setEnabled(stepsPerBarButton, FALSE);
	setEnabled(siblingsButton, FALSE);
	setEnabled(previousButton, FALSE);
	setEnabled(nextButton, FALSE);
	setEnabled(valuesButton, FALSE);
	setEnabled(randomiseButton, FALSE);

	if (stepsView != NULL) {
		gtk_widget_destroy(stepsView); stepsView = NULL;
	}

	g_slist_free(signalling.labels); signalling.labels = NULL;

	if (IS_ROOT(current.pattern)) {
		goto finish;
	}

	enableButtons();

	renderSteps(current.pattern, current.bar);

finish:
	return;
}

static void enterPattern(pattern_t *pattern)
{
	current.pattern = pattern;
	current.bar = 0;
	renderPattern();
}

static void cbParent(GtkWidget *widget, gpointer data)
{
	enterPattern(PARENT(current.pattern));
}

static void cbTop(GtkWidget *widget, gpointer data)
{
	enterPattern(((pattern_t  *) patterns.root));
}

static void destroyDialog(void)
{
	if (dialog == NULL) {
		goto finish;
	}
	gtk_widget_destroy(dialog); dialog = NULL;

finish:
	return;
}

static GtkWidget *getLoadStoreDialog(gboolean load, err_t *e)
{
	GtkWidget *result = NULL;
	GtkWidget *_result = NULL;
	GtkFileFilter *fileFilter = NULL;

	terror(failIfFalse(((_result =
	  gtk_file_chooser_dialog_new(load ?
	  "Open File" : "Store File", GTK_WINDOW(topWindow),
	  load ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE,
	  "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_ACCEPT, NULL))
	  != NULL)))

	terror(failIfFalse((fileFilter = gtk_file_filter_new()) != NULL))
	gtk_file_filter_set_name(fileFilter, "KSQ sequences");
	gtk_file_filter_add_pattern(fileFilter, "*" EXTENSION);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(_result), fileFilter);

	result = _result; _result = NULL;
finish:
	if (_result != NULL) {
		gtk_widget_destroy(_result);
	}
	return result;
}

static gint runDialog(GtkWidget *runMe)
{
	destroyDialog();
	dialog = runMe;

	return gtk_dialog_run(GTK_DIALOG(runMe));
}

static char *getPath(gboolean load, err_t *e)
{
	char *result = NULL;
	GtkWidget *dialog = NULL;
	
	terror(dialog = getLoadStoreDialog(load, e))
	terror(failIfFalse(((runDialog(dialog) == GTK_RESPONSE_ACCEPT))))
	result = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

finish:
	destroyDialog();
	return result;
}

typedef ssize_t (*readWriteFunc_t)(int fd, void *buf, size_t count);

static void readWriteFd(void *data, uint32_t length, int fd,
  gboolean reading, err_t *e)
{
	char *cur = data;
	uint32_t pending = length;
	readWriteFunc_t func = reading ? ((readWriteFunc_t) read) :
	  ((readWriteFunc_t) write);

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

static char *readStringFromFd(int fd, err_t *e)
{
	size_t length = 0;
	char *result = NULL;
	char *_result = NULL;

	terror(readWriteFd(&length, sizeof(length), fd, TRUE, e))
	_result = calloc(1, (length + 1));
	terror(readWriteFd(_result, length, fd, TRUE, e))
	_result[length] = '\0';

	result = _result; _result = NULL;
finish:
	return result;
}

static void writeStringToFd(char *string, int fd, err_t *e)
{
	size_t length = strlen(string);

	terror(readWriteFd(&length, sizeof(length), fd, FALSE, e))
	terror(readWriteFd(string, length, fd, FALSE, e))

finish:
	return;
}

static void loadStorePattern(pattern_t **pattern, int fd, gboolean load,
  pattern_t *parent, err_t *e);

static void loadStoreChildren(pattern_t *parent, int fd,
  gboolean load, err_t *e)
{
	uint32_t count = 0;

	if (!load) {
		count = g_slist_length((GSList *) parent->children);
	}

	terror(readWriteFd(&count, sizeof(count), fd, load, e))

	for (uint32_t i = 0; i < count; i++) {
		pattern_t *child = NULL;
		if (!load) {
			child = g_slist_nth_data((GSList *) parent->children, i);
		}
		terror(loadStorePattern(&child, fd, load, parent, e))
		if (load) {
			parent->children =
			  g_slist_append((GSList *) parent->children, child);
		}
	}

finish:
	return;
}

static void loadStoreNoteValue(noteValue_t **noteValue, int fd,
  gboolean load, uint8_t channel, err_t *e)
{
	noteValue_t *freeMe = NULL;

	if (load) {
		freeMe = (*noteValue) = allocateNoteValue();
		terror((*noteValue)->name = readStringFromFd(fd, e))
	} else {
		terror(writeStringToFd((char *) (*noteValue)->name, fd, e))
	}

	terror(readWriteFd((void *) &((*noteValue)->tone),
	  sizeof((*noteValue)->tone), fd, load, e))
	terror(readWriteFd((void *) &((*noteValue)->sharp),
	  sizeof((*noteValue)->sharp), fd, load, e))
	terror(readWriteFd((void *) &((*noteValue)->octave),
	  sizeof((*noteValue)->octave), fd, load, e))
	if (load) {
		terror(setAlsaNoteEvent((*noteValue), channel, &lockContext, FALSE, e))
	}

	freeMe = NULL;
finish:
	if (freeMe != NULL) {
		freeNoteValue(freeMe);
	}
}

static void loadStoreControllerValue(controllerValue_t **controllerValue,
  int fd, gboolean load, uint8_t channel, uint8_t parameter, err_t *e)
{
	controllerValue_t *freeMe = NULL;

	if (load) {
		freeMe = (*controllerValue) = allocateControllerValue();
		terror((*controllerValue)->name = readStringFromFd(fd, e))
	} else {
		terror(writeStringToFd((char *) (*controllerValue)->name, fd, e))
	}

	terror(readWriteFd((void *) &((*controllerValue)->value),
	  sizeof((*controllerValue)->value), fd, load, e))
	if (load) {
		setAlsaControllerEvent((*controllerValue),
		  channel, parameter, &lockContext, FALSE, NULL);
	}

	freeMe = NULL;
finish:
	if (freeMe != NULL)  {
		freeControllerValue(freeMe);
	}
}

static void loadStoreValue(void **value, pattern_t *pattern, int fd,
  gboolean load, err_t *e)
{
	if (IS_NOTE(pattern)) {
		terror(loadStoreNoteValue(((noteValue_t **) value), fd,
		  load, CHANNEL(pattern), e))
	} else {
		terror(loadStoreControllerValue(((controllerValue_t **) value),
		  fd, load, CHANNEL(pattern), pattern->controller.parameter, e))
	}

finish:
	return;
}

static void loadStoreValues(pattern_t *pattern, int fd, gboolean load, err_t *e)
{
	guint length = 0;
	uint32_t i = 0;
	void *value = NULL;

	if (!load) {
		length = NR_VALUES(pattern);
	}
	terror(readWriteFd(&length, sizeof(length), fd, load, e))

	for (i = 0; i < length; i++) {
		if (!load) {
			value = g_slist_nth_data(VALUES(pattern), i);
		}
		terror(loadStoreValue(&value, pattern, fd, load, e))
		if (load) {
			VALUES(pattern) = g_slist_append(VALUES(pattern), value);
		}
	}

finish:
	return;
}

static void loadStoreStep(void *step, pattern_t *pattern, int fd,
  uint32_t idx, gboolean load, err_t *e)
{
	gint position = -1;
	noteUserStep_t *noteUserStep = NULL;
	gboolean set = FALSE;
	gboolean slide = FALSE;

	terror(readWriteFd(LOCKED_PTR(step, TYPE(pattern)),
	  sizeof(LOCKED(step, TYPE(pattern))), fd, load, e))

	if (IS_DUMMY(pattern)) {
		dummyUserStep_t *dummyUserStep = (dummyUserStep_t *) step;

		if (!load) {
			set = dummyUserStep->set;
		}
		terror(readWriteFd(&set, sizeof(set), fd, load, e))
		if ((load)&&(set)) {
			terror(setDummyStep(pattern, dummyUserStep, set, &lockContext, e))
		}
		goto finish;
	}

	if ((!load)&&(VALUE(step, TYPE(pattern)) != NULL)) {
		position =
		  g_slist_position(VALUES(pattern), VALUE(step, TYPE(pattern)));
	}
	terror(readWriteFd(&position, sizeof(position), fd, load, e))
	if ((load)&&(position > -1)) {
		if (IS_NOTE(pattern)) {
			terror(setNoteStep(pattern, step, g_slist_nth(VALUES(pattern),
			  position), idx, &lockContext, FALSE, e))
		} else {
			terror(setControllerStep(pattern, step,
			  g_slist_nth(VALUES(pattern), position), idx, &lockContext, e))
		}
	}
	if (IS_CONTROLLER(pattern)) {
		goto finish;
	}

	noteUserStep = step;
	if (!load) {
		slide = noteUserStep->slide;
	}
	terror(readWriteFd(&slide, sizeof(slide), fd, load, e))
	if (load&&slide) {
		terror(setSlide(pattern, noteUserStep, slide,
		  idx, &lockContext, FALSE, e))
	}
	terror(readWriteFd(&(noteUserStep->slideLocked),
	  sizeof(noteUserStep->slideLocked), fd, load, e))


finish:
	return;
}

static void loadStorePattern(pattern_t **pattern, int fd, gboolean load,
  pattern_t *parent, err_t *e)
{
	void *freeMe = NULL;

	if (load) {
		freeMe = (*pattern) = allocatePattern(parent);
		PARENT((*pattern)) = parent;
		terror(NAME((*pattern)) = readStringFromFd(fd, e))	
	} else {
		terror(writeStringToFd(NAME((*pattern)), fd, e))
	}

	terror(readWriteFd(&TYPE((*pattern)),
	  sizeof(TYPE((*pattern))), fd, load, e))
	if (!IS_DUMMY((*pattern))) {
		terror(readWriteFd((void *) PTR_CHANNEL((*pattern)),
		  sizeof(CHANNEL((*pattern))), fd, load, e))
	}
	terror(readWriteFd((void *) &NR_USERSTEPS_PER_BAR((*pattern)),
	  sizeof(NR_USERSTEPS_PER_BAR((*pattern))), fd, load, e))
	terror(readWriteFd((void *) &NR_BARS((*pattern)),
	  sizeof(NR_BARS((*pattern))), fd, load, e))

	if (load) {
		terror(adjustSteps((*pattern), NR_BARS((*pattern)),
		  NR_USERSTEPS_PER_BAR((*pattern)), &lockContext, FALSE, e))
	}

	if (!IS_DUMMY((*pattern))) {
		terror(loadStoreValues(*pattern, fd, load, e))
		if (!IS_NOTE((*pattern))) {
			terror(readWriteFd((void *) PTR_PARAMETER((*pattern)),
			  sizeof(PARAMETER((*pattern))), fd, load, e))
		}
	}

	for (uint32_t i = 0; i < NR_USERSTEPS((*pattern)); i++) {
		void *step = USERSTEP_AT((*pattern), i);
		terror(loadStoreStep(step, (*pattern), fd, i, load, e))
	}

	terror(loadStoreChildren((*pattern), fd, load, e))

	freeMe = NULL;
finish:
	if (freeMe != NULL) {
		freePattern(freeMe);
	}
}

static void cbLoad(GtkWidget *widget, gpointer data)
{
	err_t error;
	err_t *e = &error;
	int fd = -1;
	char *path = NULL;
	gboolean locked = FALSE;
	pattern_t *pattern = NULL;
	uint32_t locks = LOCK_DATA;

	initErr(e);

	terror(path = getPath(TRUE, e))

	terror(failIfFalse((fd = open(path, O_RDONLY)) >= 0))
	terror(loadStorePattern(&pattern, fd, TRUE, (pattern_t *) patterns.root, e))

	terror(getLocks(&lockContext, locks, e))
	locked = TRUE;

	patterns.root->children =
	  g_slist_append((GSList *) patterns.root->children, pattern);
	pattern = NULL;
	terror(releaseLocks(&lockContext, locks, e))
	locked = FALSE;
	
	renderPattern();

finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
	if (pattern != NULL) {
		freePattern(pattern);
	}
	if (fd >= 0) {
		close(fd);
	}
	free(path);
	if (hasFailed(e)) {
		runDialog(gtk_message_dialog_new(GTK_WINDOW(topWindow),
		  GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		  "Error: %s", err2string(e)));
		destroyDialog();
	}
}

static void cbStore(GtkWidget *widget, gpointer data)
{
	int fd = -1;
	char *path = NULL;
	char *tmp = NULL;
	err_t error;
	err_t *e = &error;

	initErr(e);

	terror(path = getPath(FALSE, e))

	if ((strrstr(path, EXTENSION)) !=
	  (path + strlen(path) - (sizeof(EXTENSION) - 1))) {
		tmp = path;
		path = g_strconcat(path, EXTENSION, NULL);
		free(tmp);
	}

	terror(failIfFalse((fd =
	  open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP)) >= 0))
	terror(loadStorePattern(&current.pattern, fd,
	  FALSE, (pattern_t *) patterns.root, e))

finish:
	if (fd >= 0) {
		close(fd);
	}
	free(path);
	if (hasFailed(e)) {
		runDialog(gtk_message_dialog_new(GTK_WINDOW(topWindow),
		  GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		  "Error: %s", err2string(e)));
		destroyDialog();
	}
}

static GtkWidget *getDialog(gint width, gint height)
{
	GtkWidget *result = gtk_dialog_new();

	gtk_widget_set_size_request(result, width, height);
	gtk_window_set_modal(GTK_WINDOW(result), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(result), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(result), GTK_WINDOW(topWindow));
	g_signal_connect(result, "delete_event", G_CALLBACK(destroyDialog), NULL);

	return result;
}

static void cbPickMore(GtkWidget *widget, gpointer data)
{
	numberPicker.fnPickNumber(pickTypeMore);
}

static void cbPickLess(GtkWidget *widget, gpointer data)
{
	numberPicker.fnPickNumber(pickTypeLess);
}

static GtkWidget *getNumberPickerBox(fnPickNumber_t fnPickNumber,
  int value, void *ptr)
{
	GtkWidget *label = NULL;
	GtkWidget *upButton = NULL;
	GtkWidget *downButton = NULL;
	GtkWidget *result = getBox(GTK_ORIENTATION_HORIZONTAL);
	
	downButton = getButton("-", G_CALLBACK(cbPickLess), NULL);
	addWidget(GTK_BOX(result), downButton);

	label = getLabel(intToString(value));
	addWidget(GTK_BOX(result), label);

	upButton = getButton("+", G_CALLBACK(cbPickMore), NULL);
	addWidget(GTK_BOX(result), upButton);

	numberPicker.fnPickNumber = fnPickNumber;
	numberPicker.downButton = downButton; downButton = NULL;
	numberPicker.upButton = upButton; upButton = NULL;
	numberPicker.label = label; label = NULL;
	numberPicker.ptr = ptr;

	return result;
}

static GtkWidget *getNumberPickerDialog(fnPickNumber_t fnPickNumber,
  GCallback onOk, int initial, void *ptr)
{
	GtkWidget *box = NULL;
	GtkWidget *okButton = NULL;
	GtkWidget *contentArea = NULL;
	GtkWidget *result = getDialog(200, 150);

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	box = getNumberPickerBox(fnPickNumber, initial, ptr);
	addWidget(GTK_BOX(contentArea), box);
	numberPicker.fnPickNumber(pickTypeDummy);

	okButton = getButton("OK", onOk, NULL);
	addWidget(GTK_BOX(contentArea), okButton);

	gtk_widget_show_all(result);

	return result;
}

static void cbPickBars(pickType_t pickType)
{
	pattern_t *pattern = numberPicker.ptr;
	const gchar *text = gtk_label_get_text(GTK_LABEL(numberPicker.label));
	int value = atoi(text);

	if (pickType == pickTypeDummy) {
		goto finish;
	}

	if (pickType == pickTypeMore) {
		value += NR_BARS(PARENT(pattern));
	} else {
		value -= NR_BARS(PARENT(pattern));
	}

	gtk_label_set_text(GTK_LABEL(numberPicker.label),
	  intToString(value));

finish:
	setEnabled(numberPicker.downButton, (value > NR_BARS(PARENT(pattern))));
	setEnabled(numberPicker.upButton, ((value * 2) > 0));
}

static void cbPickStepsPerBar(pickType_t pickType)
{
	int doubled = 0;
	pattern_t *pattern = numberPicker.ptr;
	const gchar *text = gtk_label_get_text(GTK_LABEL(numberPicker.label));
	int value = atoi(text);

	if (pickType == pickTypeDummy) {
		goto finish;
	}

	if (pickType == pickTypeMore) {
		value *= 2;
	} else {
		value /= 2;
	}

	gtk_label_set_text(GTK_LABEL(numberPicker.label),
	  intToString(value));

finish:
	setEnabled(numberPicker.downButton,
	  (value > NR_STEPS_PER_BAR(PARENT(pattern))));
	setEnabled(numberPicker.upButton,
	  ((doubled = (value * 2)) > 0)&&(doubled <= MAX_STEPS_PER_BAR(pattern)));
}

static void cbSetBars(GtkWidget *widget, gpointer data)
{
	pattern_t *pattern = numberPicker.ptr;
	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(adjustSteps(pattern,
	  atoi(gtk_label_get_text(GTK_LABEL(numberPicker.label))),
	  NR_STEPS_PER_BAR(pattern), &lockContext, TRUE, e))

	if (current.bar >= NR_BARS(pattern)) {
		current.bar = NR_BARS(pattern) - 1;
	}

finish:
	destroyDialog();
	renderPattern();
}

static void cbSetStepsPerBar(GtkWidget *widget, gpointer data)
{
	pattern_t *pattern = numberPicker.ptr;
	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(adjustSteps(pattern, NR_BARS(pattern),
	  atoi(gtk_label_get_text(GTK_LABEL(numberPicker.label))),
	  &lockContext, TRUE, e))

finish:
	destroyDialog();
	renderPattern();
}

static GtkWidget *getBarsDialog(void)
{
	GtkWidget *result = NULL;

	result =
	  getNumberPickerDialog(cbPickBars, G_CALLBACK(cbSetBars),
	  NR_BARS(current.pattern), current.pattern);

	return result;
}

static GtkWidget *getStepsPerBarDialog(void)
{
	GtkWidget *result = NULL;

	result =
	  getNumberPickerDialog(cbPickStepsPerBar, G_CALLBACK(cbSetStepsPerBar),
	  NR_STEPS_PER_BAR(current.pattern), current.pattern);

	return result;
}

static void showDialog(GtkWidget * (*fnGetDialog)(void))
{
	GtkWidget *dialog = NULL;

	dialog = fnGetDialog();
	runDialog(dialog);
}

static void cbBars(GtkWidget *widget, gpointer data)
{
	showDialog(getBarsDialog);
}

static void cbStepsPerBar(GtkWidget *widget, gpointer data)
{
	showDialog(getStepsPerBarDialog);
}

static void cbEnterPattern(GtkWidget *widget, gpointer data)
{
	enterPattern(data);
	destroyDialog();
}

static GtkWidget *getPatternListDialog();

static void cbDeletePattern(GtkWidget *widget, gpointer data)
{
	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(deleteChild(patternList.parent, data, &lockContext, e))

	renderPattern();
	showDialog(getPatternListDialog);

finish:
	return;
}

static void setChannelEnabled(void)
{
	setEnabled(createPattern.channelSpinButton,
	  (!(createPattern.patternType == patternTypeDummy)));
}

static void setControllerEnabled(void)
{
	setEnabled(createPattern.controllerSpinButton,
	  (createPattern.patternType == patternTypeController));
}

static void cbSelectPatternType(GtkWidget *widget, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
	  createPattern.dummyRadio))) {
		createPattern.patternType = patternTypeDummy;
	} else if (gtk_toggle_button_get_active(
	  GTK_TOGGLE_BUTTON(createPattern.noteRadio))) {
		createPattern.patternType = patternTypeNote;
	} else {
		createPattern.patternType = patternTypeController;
	}

	setChannelEnabled();
	setControllerEnabled();
}

static pattern_t *creatPattern(pattern_t *parent, const char *name,
  patternType_t patternType, uint8_t channel, uint8_t parameter)
{
	pattern_t *result = allocatePattern(parent);

	PARENT(result) = parent;
	NAME(result) = strdup(name);
	NR_BARS(result) = NR_BARS(parent);
	NR_USERSTEPS_PER_BAR(result) = NR_USERSTEPS_PER_BAR(parent);
	TYPE(result) = patternType;

	if (IS_DUMMY(result)) {
		goto finish;
	}

	CHANNEL(result) = channel;

	if (IS_NOTE(result)) {
		goto finish;
	}

	PARAMETER(result) = parameter;

finish:
	return result;
}

static void cbDoAddPattern(GtkWidget *widget, gpointer data)
{
	err_t err;
	err_t *e = &err;
	pattern_t  *pattern = NULL;
	gboolean locked = FALSE;
	uint32_t locks = LOCK_DATA;
	const char *name = gtk_entry_get_text(GTK_ENTRY(createPattern.nameEntry));

	initErr(e);

	if (name[0] == '\0') {
		name = intToString(createPattern.nextPattern++);
	}

	pattern = creatPattern(createPattern.parent, name,
	  createPattern.patternType,
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(
	  createPattern.channelSpinButton)),
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(
	  createPattern.controllerSpinButton)));

	terror(adjustSteps(pattern, NR_BARS(pattern),
	  NR_USERSTEPS_PER_BAR(pattern), &lockContext, FALSE, e))

	terror(getLocks(&lockContext, locks, e))
	locked = TRUE;

	CHILDREN(createPattern.parent) =
	  g_slist_append((GSList *) CHILDREN(createPattern.parent), pattern);

	terror(releaseLocks(&lockContext, locks, e))
	locked = FALSE;

	renderPattern();

	showDialog(getPatternListDialog);


finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
}

static GtkWidget *getSpinButton(int32_t min, int32_t max, uint32_t step)
{
	GtkWidget *result = NULL;

	result = gtk_spin_button_new_with_range(min, max, step);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(result), TRUE);

	return result;
}

static char *charToString(char c)
{
	static char string[2];

	snprintf(string, sizeof(string), "%c", c);

	return string;
}

static void cbSelectTone(void)
{
	for (uint32_t i = 0; i < ARRAY_SIZE(tones); i++) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
		  setupValue.note.toneRadios[i]))) {
			setEnabled(setupValue.note.sharpCheckButton, tones[i].sharpable);
			break;
		}
	}
}

static GtkWidget *getValuesDialog(void);

static void cbSetupNoteValue(GtkWidget *widget, gpointer data)
{
	GSList *value = NULL;
	gboolean locked = FALSE;
	noteValue_t *appendMe = NULL;
	char tone = 0;
	uint32_t i = 0;
	char namebuffer[50];
	noteValue_t *noteValue = data;
	int8_t octave = gtk_spin_button_get_value_as_int(
	  GTK_SPIN_BUTTON(setupValue.note.octaveSpinButton));
	gboolean sharp = FALSE;
	uint32_t locks = (LOCK_DATA | LOCK_SEQUENCER);
	const char *name = gtk_entry_get_text(GTK_ENTRY(setupValue.nameEntry));
	err_t err;
	err_t *e = &err;

	initErr(e);

	for (uint32_t i = 0; i < ARRAY_SIZE(tones); i++) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
		  setupValue.note.toneRadios[i]))) {
			tone = tones[i].name;
			if (tones[i].sharpable) {
				sharp = gtk_toggle_button_get_active(
				  GTK_TOGGLE_BUTTON(setupValue.note.sharpCheckButton));
			}
			break;
		}
	}

	terror(getLocks(&lockContext, locks, e))
	locked = TRUE;

	terror(unsoundPattern(&lockContext, setupValue.pattern, e))

	if (noteValue == NULL) {
		appendMe = noteValue = allocateNoteValue();
	} else {
		free((char *) noteValue->name);
	}

	if (name[0] == '\0') {
#define NAMEFORMAT "%c%s %d"
		snprintf(namebuffer, sizeof(namebuffer), NAMEFORMAT,
		  tone, sharp ? "#" : "", octave);
		name = namebuffer;
	}

	noteValue->name = strdup(name);
	noteValue->tone = tone;
	noteValue->sharp = sharp;
	noteValue->octave = octave;

	setAlsaNoteEvent(noteValue, CHANNEL(setupValue.pattern),
	  &lockContext, TRUE,  NULL);

	if (appendMe != NULL) {
		VALUES(setupValue.pattern) =
		  g_slist_append(VALUES(setupValue.pattern), appendMe);
	}

	value = g_slist_find(VALUES(setupValue.pattern), noteValue);

	for (i = 0; i < NR_USERSTEPS(setupValue.pattern); i++) {
		noteUserStep_t *noteUserStep = USERSTEP_AT(setupValue.pattern, i);

		if (noteUserStep->value != value) {
			continue;
		}
		terror(setNoteStep(setupValue.pattern, noteUserStep, NULL,
		  i, &lockContext, FALSE, e))
		terror(setNoteStep(setupValue.pattern, noteUserStep, value,
		  i, &lockContext, FALSE, e))
	}

	renderPattern();
	showDialog(getValuesDialog);

finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
}

static GtkWidget *getSetupNoteValueDialog(noteValue_t *noteValue)
{
	uint8_t activeIdx = 0;
	GtkWidget *box = NULL;
	GtkWidget *button = NULL;
	GtkWidget *result = NULL;
	GtkWidget *nameEntry = NULL;
	GtkWidget *contentArea = NULL;
	GtkWidget *sharpCheckButton = NULL;
	GtkWidget *octaveSpinButton = NULL;
	GtkWidget *toneRadios[ARRAY_SIZE(tones)];
	int8_t octave = (noteValue == NULL) ? 0 : noteValue->octave;
	gboolean sharp = (noteValue == NULL) ? FALSE : noteValue->sharp;
	char *name = (noteValue == NULL) ? "" : (char *) noteValue->name;
	char tone = (noteValue == NULL) ? tones[0].name : noteValue->tone;

	result = getDialog(400, 300);
	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));
	box = getBoxWithLabel(GTK_ORIENTATION_VERTICAL, "name:");
	addWidget(GTK_BOX(contentArea), box);
	nameEntry = gtk_entry_new();
	addWidget(box, nameEntry);
	gtk_entry_set_text(GTK_ENTRY(nameEntry), name);
	box = getBoxWithLabel(GTK_ORIENTATION_HORIZONTAL, "note:");
	addWidget(GTK_BOX(contentArea), box);
	GtkWidget *previous = NULL;
	for (uint32_t i = 0; i < ARRAY_SIZE(toneRadios); i++) {
		previous = toneRadios[i] =
		  gtk_radio_button_new_with_label_from_widget(
		  GTK_RADIO_BUTTON(previous), charToString(tones[i].name));
		addWidget(box, toneRadios[i]);
		if (tones[i].name == tone) {
			activeIdx = i;
		}
		g_signal_connect(G_OBJECT(toneRadios[i]),
		  "clicked", G_CALLBACK(cbSelectTone), NULL);
	}
	sharpCheckButton = gtk_check_button_new_with_label("sharp");
	addWidget(contentArea, sharpCheckButton);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sharpCheckButton), sharp);
	box = getBoxWithLabel(GTK_ORIENTATION_VERTICAL, "octave:");
	addWidget(GTK_BOX(contentArea), box);
	octaveSpinButton = getSpinButton(-128, 127, 1);
	addWidget(box, octaveSpinButton);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(octaveSpinButton), octave);
	button = getButton("OK", G_CALLBACK(cbSetupNoteValue), noteValue);
	addWidget(contentArea, button);
	gtk_widget_show_all(result);
	setupValue.nameEntry = nameEntry;
	memcpy(setupValue.note.toneRadios, toneRadios, sizeof(toneRadios));
	setupValue.note.sharpCheckButton = sharpCheckButton;
	setupValue.note.octaveSpinButton = octaveSpinButton;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toneRadios[activeIdx]),
	  TRUE);

	return result;
}

static void cbTriggerControllerValue(GtkWidget *widget, gpointer data)
{
	gboolean locked = FALSE;
	snd_seq_event_t *snd_seq_event = getAlsaEvent();
	uint8_t intValue =
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(
	  setupValue.controller.valueSpinButton));
	uint32_t locks = LOCK_SEQUENCER;
	err_t err;
	err_t *e = &err;

	initErr(e);

	setAlsaControllerEvent2(snd_seq_event, CHANNEL(setupValue.pattern),
      PARAMETER(setupValue.pattern), intValue);
	snd_seq_event->dest = sequencer.snd_seq_addr;

	terror(getLocks(&lockContext, locks, e))
	locked = TRUE;

	snd_seq_event_output(sequencer.snd_seq, snd_seq_event);
	snd_seq_drain_output(sequencer.snd_seq);

finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
	free(snd_seq_event);
}

static void cbSetupControllerValue(GtkWidget *widget, gpointer data)
{
	controllerValue_t *appendMe = NULL;
	gboolean locked = FALSE;
	uint32_t locks = LOCK_DATA;
	controllerValue_t *controllerValue = data;
	const char *name = gtk_entry_get_text(GTK_ENTRY(setupValue.nameEntry));
	uint8_t intValue =
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(
	  setupValue.controller.valueSpinButton));
	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(getLocks(&lockContext, locks, e))
	locked = TRUE;

	if (controllerValue == NULL) {
		appendMe = controllerValue = allocateControllerValue();
	} else {
		free((char *) controllerValue->name);
	}

	if (name[0] == '\0') {
		name = intToString(intValue);
	}

	controllerValue->value = intValue;
	controllerValue->name = strdup(name);

	setAlsaControllerEvent(controllerValue, CHANNEL(setupValue.pattern),
	  PARAMETER(setupValue.pattern), &lockContext, TRUE, NULL);

	if (appendMe != NULL) {
		VALUES(setupValue.pattern) =
		  g_slist_append(VALUES(setupValue.pattern), appendMe);
	}

	renderPattern();
	showDialog(getValuesDialog);

finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
}

static GtkWidget *getSetupControllerValueDialog(controllerValue_t
  *controllerValue)
{
	GtkWidget *box = NULL;
	GtkWidget *button = NULL;
	GtkWidget *result = NULL;
	GtkWidget *nameEntry = NULL;
	GtkWidget *contentArea = NULL;
	GtkWidget *valueSpinButton = NULL;
	char *name = (controllerValue == NULL) ? "" :
	  ((char *) controllerValue->name);
	gdouble curValue = (controllerValue == NULL) ? 0 : controllerValue->value;

	result = getDialog(400, 300);
	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	box = getBoxWithLabel(GTK_ORIENTATION_VERTICAL, "name:");
	addWidget(GTK_BOX(contentArea), box);

	nameEntry = gtk_entry_new();
	addWidget(GTK_BOX(box), nameEntry);
	gtk_entry_set_text(GTK_ENTRY(nameEntry), name);

	box = getBoxWithLabel(GTK_ORIENTATION_VERTICAL, "controller value:");
	addWidget(GTK_BOX(contentArea), box);

	valueSpinButton = getSpinButton(0, 127, 1);
	addWidget(GTK_BOX(box), valueSpinButton);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON( valueSpinButton), curValue);

	button = getButton("Trigger", G_CALLBACK(cbTriggerControllerValue), NULL);
	addWidget(GTK_BOX(contentArea), button);
	button =
	  getButton("OK", G_CALLBACK(cbSetupControllerValue), controllerValue);
	addWidget(GTK_BOX(contentArea), button);

	gtk_widget_show_all(result);

	setupValue.nameEntry = nameEntry;
	setupValue.controller.valueSpinButton = valueSpinButton;

	return result;
}

static GtkWidget *getSetupValueDialog(void)
{
	GtkWidget *result = NULL;

	if (IS_NOTE(current.pattern)) {
		result = getSetupNoteValueDialog(setupValue.value);
	} else {
		result = getSetupControllerValueDialog(setupValue.value);
	}

	return result;
}

static GtkWidget *getSetupPatternDialog(void)
{
	GtkWidget *box = NULL;
	GtkWidget *button = NULL;
	GtkWidget *nameEntry = NULL;
	GtkWidget *noteRadio = NULL;
	GtkWidget *firstRadio = NULL;
	GtkWidget *dummyRadio = NULL;
	GtkWidget *contentArea = NULL;
	GtkWidget *controllerRadio = NULL;
	GtkWidget *channelSpinButton = NULL;
	GtkWidget *controllerSpinButton = NULL;
	GtkWidget *result = getDialog(400, 300);

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	box = getBox(GTK_ORIENTATION_HORIZONTAL);
	addWidget(GTK_BOX(contentArea), box);

	firstRadio = dummyRadio =
	  gtk_radio_button_new_with_label_from_widget(NULL, "DUMMY");
	addWidget(GTK_BOX(box), dummyRadio);
	g_signal_connect(G_OBJECT(dummyRadio), "clicked",
	  G_CALLBACK(cbSelectPatternType), NULL);

	noteRadio =
	  gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(
	  firstRadio), "NOTE");
	addWidget(GTK_BOX(box), noteRadio);
	g_signal_connect(G_OBJECT(noteRadio), "clicked",
	  G_CALLBACK(cbSelectPatternType), NULL);

	controllerRadio =
	  gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(firstRadio),
	  "CONTROLLER");
	addWidget(GTK_BOX(box), controllerRadio);
	g_signal_connect(G_OBJECT(controllerRadio), "clicked",
	  G_CALLBACK(cbSelectPatternType), NULL);

	box = getBoxWithLabel(GTK_ORIENTATION_VERTICAL, "name:");
	addWidget(GTK_BOX(contentArea), box);
	nameEntry = gtk_entry_new();
	addWidget(GTK_BOX(box), nameEntry);

	box =
	  getBoxWithLabel(GTK_ORIENTATION_VERTICAL, "channel:");
	addWidget(GTK_BOX(contentArea), box);
	channelSpinButton = getSpinButton(1, 16, 1);
	addWidget(GTK_BOX(box), channelSpinButton);

	box =
	  getBoxWithLabel(GTK_ORIENTATION_VERTICAL, "controller:");
	addWidget(GTK_BOX(contentArea), box);
	controllerSpinButton = getSpinButton(0, 127, 1);
	addWidget(GTK_BOX(box), controllerSpinButton);

	button = getButton("OK", G_CALLBACK(cbDoAddPattern), NULL);
	addWidget(GTK_BOX(contentArea), button);

	createPattern.parent = patternList.parent;
	createPattern.nameEntry = nameEntry;
	createPattern.dummyRadio = dummyRadio;
	createPattern.noteRadio = noteRadio;
	createPattern.controllerRadio = controllerRadio;
	createPattern.channelSpinButton = channelSpinButton;
	createPattern.controllerSpinButton = controllerSpinButton;

	cbSelectPatternType(NULL, NULL);
	setChannelEnabled();
	setControllerEnabled();

	gtk_widget_show_all(result);

	return result;
}

static void cbAddPattern(GtkWidget *widget, gpointer data)
{
	showDialog(getSetupPatternDialog);
}

static GtkWidget *getPatternListDialog()
{
	GtkWidget *box = NULL;
	GtkWidget *button = NULL;
	GtkWidget *contentArea = NULL;
	GtkWidget *result = getDialog(400, 300);

	contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	for (GSList *cur = (GSList *) CHILDREN(patternList.parent); cur != NULL;
	  cur = g_slist_next(cur)) {
		pattern_t *pattern = cur->data;

		if (pattern == patternList.hideMe) {
			continue;
		}

		box = getBoxWithLabel(GTK_ORIENTATION_HORIZONTAL,
		  NAME(pattern));
		addWidget(GTK_BOX(contentArea), box);
		button = getButton("Enter", G_CALLBACK(cbEnterPattern), pattern);
		addWidget(GTK_BOX(box), button);
		button = getButton("Delete", G_CALLBACK(cbDeletePattern), cur);
		setEnabled(button, (CHILDREN(pattern) == NULL));
		addWidget(GTK_BOX(box), button);

	}

	button = getButton("Add", G_CALLBACK(cbAddPattern), NULL);
	addWidget(GTK_BOX(contentArea), button);

	gtk_widget_show_all(result);

	return result;
}

static void doChildren(pattern_t *parent, pattern_t *hideMe)
{
	patternList.parent = parent;
	patternList.hideMe = hideMe;

	showDialog(getPatternListDialog);
}

static void cbChildren(GtkWidget *widget, gpointer data)
{
	doChildren(current.pattern, NULL);
}

static void cbSiblings(GtkWidget *widget, gpointer data)
{
	doChildren(PARENT(current.pattern), current.pattern);
}

static void cbPrevious(GtkWidget *widget, gpointer data)
{
	current.bar--;
	renderPattern();
}

static void cbNext(GtkWidget *widget, gpointer data)
{
	current.bar++;
	renderPattern();
}

static void cbSetupValue(GtkWidget *widget, gpointer data)
{
	setupValue.pattern = current.pattern;
	setupValue.value = data;
	setupValue.isNote = IS_NOTE(current.pattern);
	showDialog(getSetupValueDialog);
}

static void cbDeleteValue(GtkWidget *widget, gpointer data)
{
	GSList *link = data;

	freeValue(current.pattern, link->data);

	VALUES(current.pattern) =
	  g_slist_delete_link(VALUES(current.pattern), link);

	renderPattern();
	showDialog(getValuesDialog);
}

static gboolean isValueInUse(pattern_t *pattern, GSList *value)
{
	gboolean result = FALSE;

	for (uint32_t i = 0; i < NR_USERSTEPS(pattern); i++) {
		if (value == VALUE(USERSTEP_AT(pattern, i), TYPE(pattern))) {
			result = TRUE;
			break;
		}
	}

	return result;
}

static GtkWidget *getValuesDialog(void)
{
	GSList *cur = NULL;
	GtkWidget *box = NULL;
	GtkWidget *button = NULL;
	GtkWidget *result = getDialog(400, 300);
	GtkWidget *contentArea = gtk_dialog_get_content_area(GTK_DIALOG(result));

	for (cur = VALUES(current.pattern); cur != NULL; cur = g_slist_next(cur)) {
		gboolean sensitive = FALSE;

		void *value = cur->data;
		box = getBoxWithLabel(GTK_ORIENTATION_HORIZONTAL,
		  (char *) VALUE_NAME(value, TYPE(current.pattern)));
		addWidget(GTK_BOX(contentArea), box);
		button = getButton("Edit", G_CALLBACK(cbSetupValue), value);
		addWidget(GTK_BOX(box), button);
		button = getButton("Delete", G_CALLBACK(cbDeleteValue), cur);
		addWidget(GTK_BOX(box), button);
		sensitive = (!isValueInUse(current.pattern, cur));
		setEnabled(button, sensitive);
	}

	button = getButton("Add", G_CALLBACK(cbSetupValue), NULL);
	addWidget(GTK_BOX(contentArea), button);
	gtk_widget_show_all(result);

	return result;
}

static void cbValues(GtkWidget *widget, gpointer data)
{
	showDialog(getValuesDialog);
}

static void cbRandomise(GtkWidget *widget, gpointer data)
{
	if (randomising) {
		goto finish;
	}
	randomising = TRUE;
	randomise(current.pattern, current.bar, &lockContext);
#if 0
	uint32_t lastUserstep = NR_USERSTEPS(current.pattern) - 1;
	uint32_t start = (current.bar * NR_USERSTEPS_PER_BAR(current.pattern));
	uint32_t end = (start + NR_USERSTEPS_PER_BAR(current.pattern));

	for (uint32_t i = start; i < end; i++) {
		noteUserStep_t *noteUserStep = NULL;
		GSList *value = NULL;
		gboolean slide = FALSE;
		uint32_t nrValues = NR_VALUES(current.pattern);
		void *step = USERSTEP_AT(current.pattern, i);

		if (getLocked(NULL, USERSTEP_AT(current.pattern, i),
		  current.pattern, i)) {
			continue;
		}
		if (IS_DUMMY(current.pattern)) {
			if ((rand() % 2) == 0) {
				continue;
			}
			setDummyStep(current.pattern, step,
			  !IS_SET(step, TYPE(current.pattern)), &lockContext, NULL);
			continue;
		}
		if (!anyChildStepSet(current.pattern, i)) {
			nrValues++;
		}
		value = g_slist_nth(VALUES(current.pattern), (rand() % nrValues));
		if (value == VALUE(step, TYPE(current.pattern))) {
			continue;
		}
		if (IS_CONTROLLER(current.pattern)) {
			setControllerStep(current.pattern,
			  step, value, i, &lockContext, NULL);
			continue;
		}
		setNoteStep(current.pattern, step, value, i, &lockContext, TRUE, NULL);
		if (value == NULL) {
			continue;
		}
		if (i >= lastUserstep) {
			continue;
		}
		noteUserStep = USERSTEP_AT(current.pattern, i);
		if (noteUserStep->slideLocked) {
			continue;
		}
		slide = ((rand() % 2) == 0);
		if (HAS_SLIDE(step, TYPE(current.pattern)) == slide) {
			continue;
		}
		setSlide(current.pattern, step, slide, i, &lockContext, TRUE, NULL);
	}
#endif
	renderPattern();
	randomising = FALSE;

finish:
	return;
}

static GtkWidget *getCentreBox(void)
{

	GtkWidget *box = NULL;
	GtkWidget *result = getBox(GTK_ORIENTATION_VERTICAL);

	box = getBox(GTK_ORIENTATION_HORIZONTAL);
	addWidget(result, box);
	parentButton = addButton(box, "Parent!", G_CALLBACK(cbParent), NULL);
	topButton = addButton(box, "Top!", G_CALLBACK(cbTop), NULL);
	loadButton = addButton(box, "Load ...", G_CALLBACK(cbLoad), NULL);
	storeButton = addButton(box, "Store ...", G_CALLBACK(cbStore), NULL);
	barsButton = addButton(box, "Bars ...", G_CALLBACK(cbBars), NULL);
	stepsPerBarButton = addButton(box, "Steps per Bar ...",
	  G_CALLBACK(cbStepsPerBar), NULL);

	box = getBox(GTK_ORIENTATION_HORIZONTAL);
	addWidget(result, box);
	childrenButton =
	  addButton(box, "Children ...", G_CALLBACK(cbChildren), NULL);
	siblingsButton =
	  addButton(box, "Siblings ...", G_CALLBACK(cbSiblings), NULL);
	previousButton = addButton(box, "Previous", G_CALLBACK(cbPrevious), NULL);
	nextButton = addButton(box, "Next", G_CALLBACK(cbNext), NULL);
	valuesButton = addButton(box, "Values ...", G_CALLBACK(cbValues), NULL);
	randomiseButton =
	  addButton(box, "Randomise!", G_CALLBACK(cbRandomise), NULL);

	return result;
}

static GtkWidget *getLowerBox(void)
{
	GtkWidget *result = NULL;

	result = getBox(GTK_ORIENTATION_VERTICAL);

	return result;
}

static GtkWidget *getContent(err_t *e)
{
	GtkWidget *scrolledWindow = NULL;
	GtkWidget *box = NULL;
	GtkWidget *result = getBox(GTK_ORIENTATION_VERTICAL);

	box = getUpperBox();
	addWidget(result, box);

	box = getCentreBox();
	addWidget(result, box);


	scrolledWindow = gtk_scrolled_window_new(NULL, NULL);

	box = getLowerBox();
	gtk_container_add(GTK_CONTAINER(scrolledWindow), box);
	stepsBox = box;

	addWidget(result, scrolledWindow);

	return result;
}

static void setupStyles(err_t *e)
{
	GdkScreen *screen = NULL;
	GdkDisplay *display = NULL;
	GtkCssProvider *provider = NULL;

	terror(failIfFalse(((provider = gtk_css_provider_new()) != NULL)))
	terror(failIfFalse(((display = gdk_display_get_default()) != NULL)))
	terror(failIfFalse(((screen =
	  gdk_display_get_default_screen(display)) != NULL)))
	gtk_style_context_add_provider_for_screen(screen,
	  GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

#define STYLE \
  ".colorable {" \
	"border-image: none;" \
	"background-image: none;" \
  "}"

	terror(failIfFalse((gtk_css_provider_load_from_data(
	  GTK_CSS_PROVIDER(provider), STYLE, -1, NULL))))
	
finish:
	if (provider != NULL) {
		g_object_unref(provider);
	}
}

static GtkWidget *getTopWindow(err_t *e)
{
	GtkWidget *result = NULL;
	GtkWidget *_result = NULL;
	GtkWidget *content = NULL;

	terror(failIfFalse((_result = gtk_window_new(GTK_WINDOW_TOPLEVEL)) != NULL))
	gtk_widget_set_size_request(_result, WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_resizable(GTK_WINDOW(_result), FALSE);
	gtk_window_set_position(GTK_WINDOW(_result), GTK_WIN_POS_CENTER);
	g_signal_connect(G_OBJECT(_result), "destroy",
	  G_CALLBACK(gtk_main_quit), NULL);

	terror(content = getContent(e))
	gtk_container_add(GTK_CONTAINER(_result), content);
	gtk_widget_show_all(_result);
	
	result = _result; _result = NULL;
finish:
	return result;
}

static void setup(int argc, char *argv[], err_t *e)
{
#ifdef SIGNALLING_MUTEX
	pthread_mutex_init(&(signalling.mutex), NULL);
#endif
	gtk_init(&argc, &argv);
	gdk_threads_init();

	terror(setupStyles(e))
	terror(topWindow = getTopWindow(e))
	terror(gIoChannel = doSignals(e))

finish:
	return;	
}

static void teardown(void)
{
	if (gIoChannel != NULL) {
		g_io_channel_shutdown(gIoChannel, TRUE, NULL);
	}

	destroyDialog();
#ifdef SIGNALLING_MUTEX
	pthread_mutex_destroy(&(signalling.mutex));
#endif
}

void gui(int argc, char *argv[], err_t *e)
{
	terror(setup(argc, argv, e))

	enterPattern(((pattern_t *) patterns.root));

	gtk_main();

finish:
	teardown();
}

/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "kVstSequencer.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define EXTENSION ".ksq"
#define FILE_PATTERN "*" EXTENSION

DECLARE_LOCKCONTEXT

static void enterPattern(pattern_t *pattern);

static Rectangle<int> getSubRectangle(gboolean vertical,
  Rectangle<int> *rectangle, uint32_t fraction)
{
	MARK();

	Rectangle<int> result;

	if (vertical) {
		result = rectangle->removeFromTop(rectangle->getHeight() / fraction);
	} else {
		result = rectangle->removeFromLeft(rectangle->getWidth() / fraction);
	}

	return result;
}

typedef enum {
	pickTypeDummy,
	pickTypeMore,
	pickTypeLess
} pickType_t;

typedef void (*fnButtonCallback_t)(void *data);
typedef void (*fnPickNumber_t)(pickType_t pickType);

typedef struct {
	gboolean isContainer;
	union {
		Component *component;
		Container *container;
	};
} element_t;

static gint compareElements(gconstpointer a, gconstpointer b)
{
	MARK();

	gint result = 0;
	element_t *first = (element_t *) a;
	element_t *second = (element_t *) b;

	if (first->isContainer != second->isContainer)  {
		result = first->isContainer ? 1 : -1;
	} else {
		void *ptr1 = first->isContainer ?
		  (void *) first->container : (void *) first->component;
		void *ptr2 = first->isContainer ?
		  (void *) second->container : (void *) second->component;

		result = (ptr1 > ptr2) ? 1 : (ptr2 < ptr1) ? -1 : 0;
	}

	return result;
}

class SimpleLabel : public Label
{
public:
	SimpleLabel(void) : Label(), string(NULL) {
		MARK();
	}
	virtual ~SimpleLabel() override {
		MARK();

		if (this->string != NULL) {
			//delete string;
		}
	}
	void setText(const String &newText, NotificationType notification) {
		MARK();

		Label::setText(newText, notification);
	}
	void setText(const char *text) {
		MARK();
		
		String *string = new String(text);
		setText(*string, dontSendNotification);
		if (this->string != NULL) {
			delete string;
		}
		this->string = string;
	}

private:
	String *string;
};

static struct {
	pattern_t *pattern;
	uint32_t bar;
	GSList *labels;
	uint32_t userStepsPerBar;
	uint32_t bars;
	uint64_t generation;
	uint64_t lastGeneration;
	SimpleLabel *lastLabel;
} signalling = {
	.pattern = NULL,
	.bar = 0,
	.labels = NULL,
	.userStepsPerBar = 0,
	.bars = 0,
	.generation = 0,
	.lastGeneration = 0,
	.lastLabel = NULL,
};

static void unsignalStep()
{
	MARK();

	if (signalling.lastLabel == NULL) {
		goto finish;
	}
	if (signalling.generation != signalling.lastGeneration) {
		goto finish;
	}
	signalling.lastLabel->setText("");

finish:
	return;
}

static void doSignalStep(int step)
{
	MARK();

	int minStep = 0;
	int maxStep = 0;

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

	signalling.lastLabel =
	  (SimpleLabel *) g_slist_nth_data(signalling.labels, step);

	signalling.lastLabel->setText("X");
	signalling.lastGeneration = signalling.generation;

finish:
	return;
}

static gboolean initialised = FALSE;

void KVstSequencerAudioProcessorEditor::handleCommandMessage(int commandId)
{
	MARK();

	if (commandId < 0) {
		unsignalStep();
	} else {
		doSignalStep(commandId);
	}
}

static struct {
	Container *container;
	KVstSequencerAudioProcessorEditor *component;
} application = {
	.container = NULL,
	.component = NULL
};

void guiSignalStep(int step)
{
	MARK();

	if (!initialised) {
		goto finish;
	}

	application.component->postCommandMessage(step);

finish:
	return;
}

void guiSignalStop(void)
{
	MARK();

	if (!initialised) {
		goto finish;
	}

	application.component->postCommandMessage(-1);

finish:
	return;
}

Container::Container(Component *component, gboolean vertical) : elements(NULL)
{
	MARK();

	this->component = component;
	this->vertical = vertical;
}

Container::~Container()
{
	MARK();

	for (GSList *cur = this->elements; cur != NULL;
	  cur = g_slist_next(cur)) {
		element_t *element = (element_t *) (cur->data);
		if (element->isContainer) {
			delete element->container;
		} else {
			delete element->component;
		}
		free(element);
	}
	g_slist_free(this->elements); this->elements = NULL;
}

void Container::addElement(void *element)
{
	MARK();

	this->elements = g_slist_append(this->elements, element);
}

void Container::addContainer(Container *container)
{
	MARK();

	element_t *element = (element_t *) calloc(1, sizeof((*element)));

	element->isContainer = TRUE;
	element->container = container;

	this->addElement(element);
}

void Container::addComponent(Component *component)
{
	MARK();

	element_t *element = (element_t *) calloc(1, sizeof((*element)));

	element->isContainer = FALSE;
	element->component = component;

	this->addElement(element);

	this->component->addAndMakeVisible(component);
}

void Container::removeElement(void *element)
{
	MARK();

	err_t err;
	err_t *e = &err;

	initErr(e);

	GSList *link =
	  g_slist_find_custom(this->elements, element, compareElements);
	terror(failIfFalse((link != NULL)))
	free((element_t *) link->data);
	this->elements = g_slist_delete_link(this->elements, link);

finish:
	return;
}

void Container::removeContainer(Container *container)
{
	MARK();

	element_t element;

	element.isContainer = TRUE;
	element.container = container;
MARK();
	this->removeElement(&element);
}

void Container::removeComponent(Component *component)
{
	MARK();

	element_t element;

	element.isContainer = FALSE;
	element.component = component;

	this->removeElement(&element);

	this->component->removeChildComponent(component);
}

void Container::layout(Rectangle<int> rectangle)
{
	MARK();

	GSList *cur = NULL;
	uint32_t fraction = g_slist_length(this->elements);

	for (cur = this->elements; cur != NULL; cur = g_slist_next(cur))  {
		element_t *element = (element_t *) cur->data;
		Rectangle<int> subRectangle =
		  getSubRectangle(this->vertical, &rectangle, fraction--);

		if (element->isContainer) {
			element->container->layout(subRectangle);
		} else {
			element->component->setBounds(subRectangle);
		}
	}
}

static gboolean randomising = FALSE;
static struct {
	pattern_t *pattern;
	uint32_t bar;
} current = {
	.pattern = NULL,
	.bar = 0
};

static struct {
	SimpleLabel *label;
	TextButton *upButton;
	TextButton *downButton;
	fnPickNumber_t fnPickNumber;
	void *ptr;
} numberPicker;

static void slide(uint32_t idx, err_t *e)
{
	MARK();

	noteUserStep_t *noteUserStep =
	  (noteUserStep_t *) (USERSTEP_AT(current.pattern, idx));

	terror(setSlide(current.pattern, noteUserStep,
	  (!noteUserStep->slide), idx, &lockContext, TRUE, e))

finish:
	return;
}

class SimpleButtonListener : public Button::Listener
{
public:
	SimpleButtonListener(fnButtonCallback_t callback, void *data) :
	  callback(callback), data(data) {
		MARK();
	}
	virtual ~SimpleButtonListener() override {
		MARK();
	}
    void buttonClicked (Button *button) override {
		MARK();

		this->callback(this->data);
	}
	void buttonStateChanged (Button *button) override {
		MARK();
	}

private:
	fnButtonCallback_t callback;
	void *data;
};

class SimpleButton : public TextButton
{
public:
	SimpleButton(const char *text, fnButtonCallback_t callback, void *data) :
	  TextButton(text), listener(NULL) {
		MARK();

		if (callback != NULL) {
			this->listener = new SimpleButtonListener(callback, data);
			this->addListener(this->listener);
		}
	}
	virtual ~SimpleButton() override {
		MARK();

		if (listener != NULL) delete listener;
	}
private:
	SimpleButtonListener *listener;
};

class SimpleToggleButton : public ToggleButton
{
public:
	SimpleToggleButton(const char *text, fnButtonCallback_t callback,
	  void *data) : ToggleButton(text), listener(NULL) {
		MARK();

		if (callback != NULL) {
			this->listener = new SimpleButtonListener(callback, data);
			this->addListener(this->listener);
		}
	}
	virtual ~SimpleToggleButton() override {
		MARK();

		if (listener != NULL) {
			delete listener;
		}
	}
private:
	SimpleButtonListener *listener;
};

static char *intToString(int32_t number);
static SimpleLabel *getLabel(Container *container, const char *text);
static SimpleButton *getSimpleButton(Container *container, const char *text,
  fnButtonCallback_t callback, void *data);
static void renderPattern(void);
static void cbDecrement(void *data);
static void cbIncrement(void *data);

#define ELEVEN (11)

class SpinButton : public Container, Label::Listener
{
public:
	SimpleLabel *label;
	int32_t value;
	SpinButton(Component *component, int32_t min, int32_t max, int32_t value)
	  : Container(component, FALSE), min(min), max(max), value(value) {
		MARK();

		this->downButton = getSimpleButton(this, "-", cbDecrement, this);
		this->label = getLabel(this, intToString(this->value));
		this->label->addListener(this);
		this->label->setEditable(TRUE, FALSE, TRUE); 
		this->upButton = getSimpleButton(this, "+", cbIncrement, this);

		setButtonsEnabled();
	}
    virtual ~SpinButton() override {
		MARK();
	}
	void setButtonsEnabled(void) {
		MARK();

		this->downButton->setEnabled(this->value > this->min);
		this->upButton->setEnabled(this->value < this->max);
	}
	virtual void setEnabled(gboolean enabled) {
		MARK();

		this->label->setEnabled(enabled);
		if (enabled) {
			setButtonsEnabled();
		} else {
			this->downButton->setEnabled(FALSE);
			this->upButton->setEnabled(FALSE);
		}
	}

	virtual void labelTextChanged(Label *label) {
		MARK();

		char *copy = strdup(((char *) label->getText(FALSE).toRawUTF8()));
		gchar *string = g_strstrip(((gchar *) copy));
		uint32_t i = 0;
		int32_t number = 0;
		gboolean valid = TRUE;
		uint32_t startIdx = 1;
		gboolean zeroLength = TRUE;

		if (string[0] == '-') {
			valid = (string[1] != '0');
		} else if (string[0] == '0') {
			valid = (string[1] == '\0');
			zeroLength = FALSE;
		} else {
			startIdx = 0;
		}

		for (i = startIdx; string[i] != '\0'; i++) {
			if (!valid){
				break;
			}
			zeroLength = FALSE;
			if (!isdigit(string[i])) {
				valid = FALSE;
			}
			number = (number * 10) + (string[i] - '0');
		}

		if (zeroLength) {
			valid = FALSE;
		}
		if ((number < this->min)||(number > this->max)) {
			valid = FALSE;
		}


		free(copy);
		if (valid) {
			this->value = (string[0] == '-') ? -number : number;
		} else {
			char buffer[ELEVEN];
			snprintf(buffer, sizeof(buffer), "%u", this->value);
			((SimpleLabel *) label)->setText(buffer);
		}
	}

	virtual void editorShown(Label *label, TextEditor &textEditor) {
		MARK();
	}
	virtual void editorHidden(Label *label, TextEditor &textEditor) {
		MARK();
	}

private:
	TextButton *downButton;
	TextButton *upButton;
	int32_t min;
	int32_t max;
	String *lastResult;
};

static struct {
	pattern_t *pattern;
	TextEditor *nameEntry;
	void *value;
	gboolean noteSetup;
	union {
		struct {
			SpinButton *valueSpinButton;
		} controllerOrVelocity;
		struct {
			SimpleToggleButton *noteRadios[ARRAY_SIZE(notes)];
			SimpleToggleButton *sharpCheckButton;
			SpinButton *octaveSpinButton;
		} note;
	};
} setupValue;

static struct {
	uint32_t nextPattern;
	pattern_t *parent;
	patternType_t patternType;
	TextEditor *nameEntry;
	SimpleToggleButton *dummyRadio;
	SimpleToggleButton *noteRadio;
	SimpleToggleButton *controllerRadio;
	SpinButton *channelSpinButton;
	SpinButton *controllerSpinButton;
} createPattern = {
	.nextPattern = 0
};

static void refreshSpinButton(SpinButton *spinButton)
{
	MARK();

	spinButton->label->setText(intToString(spinButton->value));
	spinButton->setButtonsEnabled();
}

static void cbDecrement(void *data)
{
	MARK();

	SpinButton *spinButton = (SpinButton *) data;

	spinButton->value--;
	refreshSpinButton(spinButton);
}

static void cbIncrement(void *data)
{
	MARK();

	SpinButton *spinButton = (SpinButton *) data;

	spinButton->value++;
	refreshSpinButton(spinButton);
}

static struct {
	DialogWindow *window;
} dialog = {
	.window = NULL
};

class DialogComponent : public Component
{
public:
	Container *container;
	DialogComponent() : Component(), container(NULL) {
		MARK();
	}
    virtual ~DialogComponent() {
		MARK();

		if (this->container != NULL) {
			delete this->container;
		}
		dialog.window = NULL;
	}

private:
};

static void layout(Component *component, Container *container)
{
	MARK();

	Rectangle<int> rectangle = component->getLocalBounds();

	if (container != NULL) {
		container->layout(rectangle);
	}
}

static void destroyDialog(void)
{
	MARK();

	if (dialog.window != NULL) {
		delete dialog.window;
		dialog.window = NULL;
	}
}

static struct {
	pattern_t *parent;
	pattern_t *hideMe;
} patternList;

static Container *getPatternListDialog(Component *component);
static void showDialog(Container * (*fnGetDialog)(Component *));

static SimpleToggleButton *getSimpleToggleButton(Container *container,
  const char *text, int groupId, fnButtonCallback_t callback, void *data)
{
	MARK();

	SimpleToggleButton *result = new SimpleToggleButton(text, callback, data);

	if (groupId >= 0) {
		result->setRadioGroupId(groupId, dontSendNotification);
	}

	container->addComponent(result);

	return result;
}

static Container *getBoxWithLabel(Component *component, Container *container,
  gboolean vertical, const char *labelText);

static TextEditor *getTextEditor(Container *container)
{
	MARK();

	TextEditor *result = new TextEditor();

	container->addComponent(result);

	return result;
}

static SpinButton *getSpinButton(Component *component, Container *container,
  int32_t min, int32_t max, int value)
{
	MARK();

	SpinButton *result = new SpinButton(component, min, max, value);

	container->addContainer(result);

	return result;
}

static pattern_t *creatPattern(pattern_t *parent, const char *name,
  patternType_t patternType, uint8_t channel, uint8_t parameter)
{
	MARK();

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

static void cbDoAddPattern(void *data)
{
	MARK();

	err_t err;
	err_t *e = &err;
	pattern_t  *pattern = NULL;
	gboolean locked = FALSE;
	uint32_t locks = LOCK_DATA;
	const char *name = createPattern.nameEntry->getText().toRawUTF8();

	initErr(e);

	if (name[0] == '\0') {
		name = intToString(createPattern.nextPattern++);
	}

	pattern = creatPattern(createPattern.parent, name,
	  createPattern.patternType,
	  createPattern.channelSpinButton->value,
	  createPattern.controllerSpinButton->value);

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

static void setChannelEnabled(void)
{
	MARK();

	createPattern.channelSpinButton->setEnabled
	  (!(createPattern.patternType == patternTypeDummy));
}

static void setControllerEnabled(void)
{
	MARK();

	createPattern.controllerSpinButton->setEnabled(
	  (createPattern.patternType == patternTypeController));
}

static void cbSelectPatternType(void *data)
{
	MARK();

	createPattern.patternType = ((patternType_t) GPOINTER_TO_UINT(data));

	setChannelEnabled();
	setControllerEnabled();
}

static void cbSelectTone(void *data)
{
	MARK();

	uint32_t idx = GPOINTER_TO_UINT(data);

	setupValue.note.sharpCheckButton->setEnabled(notes[idx].sharpable);
}

static Container *getValuesDialog(Component *component);
static Container *getVelocitiesDialog(Component *component);

static void cbSetupNoteValue(void *data)
{
	MARK();

	GSList *value = NULL;
	GSList *backupVelocity = NULL;
	gboolean locked = FALSE;
	noteValue_t *appendMe = NULL;
	uint8_t note = 0;
	uint32_t i = 0;
	char namebuffer[50];
	noteValue_t *noteValue = (noteValue_t *) data;
	int8_t octave = setupValue.note.octaveSpinButton->value;
	gboolean sharp = FALSE;
	uint32_t locks = (LOCK_DATA | LOCK_SEQUENCER);
	const char *name = setupValue.nameEntry->getText().toRawUTF8();

	err_t err;
	err_t *e = &err;

	initErr(e);

	for (uint32_t i = 0; i < ARRAY_SIZE(notes); i++) {
		if (setupValue.note.noteRadios[i]->getToggleState()) {
			note = notes[i].name;
			if (notes[i].sharpable) {
				sharp = setupValue.note.sharpCheckButton->getToggleState();
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
		  NOTE2CHAR(note), sharp ? "#" : "", octave);
		name = namebuffer;
	}

	noteValue->name = strdup(name);
	noteValue->note = note;
	noteValue->sharp = sharp;
	noteValue->octave = octave;

	if (appendMe != NULL) {
		VALUES(setupValue.pattern) =
		  g_slist_append(VALUES(setupValue.pattern), appendMe);
	}

	value = g_slist_find(VALUES(setupValue.pattern), noteValue);

	for (i = 0; i < NR_USERSTEPS(setupValue.pattern); i++) {
		noteUserStep_t *noteUserStep =
		  (noteUserStep_t *) USERSTEP_AT(setupValue.pattern, i);

		if (noteUserStep->value != value) {
			continue;
		}
		backupVelocity = noteUserStep->velocity;
		terror(setNoteStep(setupValue.pattern, noteUserStep, NULL,
		  NULL, i, &lockContext, FALSE, e))
		terror(setNoteStep(setupValue.pattern, noteUserStep, value,
		  backupVelocity, i, &lockContext, FALSE, e))
	}

	renderPattern();
	showDialog(getValuesDialog);

finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
}

static void cbTriggerControllerValue(void *data)
{
	MARK();

	midiMessage_t *midiMessage = NULL;
	gboolean locked = FALSE;
	uint32_t locks = LOCK_SEQUENCER;
	uint8_t intValue = setupValue.controllerOrVelocity.valueSpinButton->value;
	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(getLocks(&lockContext, locks, e))
	locked = TRUE;

	midiMessage =
	  getControllerMidiMessage(setupValue.pattern->controller.parameter,
	  intValue);
	fireMidiMessage(&lockContext, midiMessage, NULL);

finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
}

static void cbSetupControllerValue(void *data)
{
	MARK();

	controllerValue_t *appendMe = NULL;
	GSList *velocity = NULL;
	GSList *backupValue = NULL;
	gboolean locked = FALSE;
	uint32_t locks = LOCK_DATA;
	uint32_t i = 0;
	controllerValue_t *controllerValue = (controllerValue_t *) data;
	const char *name = setupValue.nameEntry->getText().toRawUTF8();
	uint8_t intValue = setupValue.controllerOrVelocity.valueSpinButton->value;
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

	if (appendMe != NULL) {
		if (IS_NOTE(current.pattern)) {
			VELOCITIES(current.pattern) =
			  g_slist_append(VELOCITIES(setupValue.pattern), appendMe);
		} else {
			VALUES(setupValue.pattern) =
			  g_slist_append(VALUES(setupValue.pattern), appendMe);
		}
	}

	if (IS_NOTE(current.pattern)) {
		velocity =
		  g_slist_find(VELOCITIES(setupValue.pattern), controllerValue);

		for (i = 0; i < NR_USERSTEPS(setupValue.pattern); i++) {
			noteUserStep_t *noteUserStep =
			  (noteUserStep_t *) USERSTEP_AT(setupValue.pattern, i);

			if (noteUserStep->velocity != velocity) {
				continue;
			}
			backupValue = noteUserStep->value;
			terror(setNoteStep(setupValue.pattern, noteUserStep, NULL,
			  NULL, i, &lockContext, FALSE, e))
			terror(setNoteStep(setupValue.pattern, noteUserStep, backupValue,
			  velocity, i, &lockContext, FALSE, e))
		}
	}

	renderPattern();

	if (IS_NOTE(current.pattern)) {
		showDialog(getVelocitiesDialog);
	} else {
		showDialog(getValuesDialog);
	}

finish:
	if (locked) {
		releaseLocks(&lockContext, locks, NULL);
	}
}

static Container *getSetupControllerValueDialog(Component *component,
  controllerValue_t *controllerValue)
{
	MARK();

	Container *box = NULL;
	SimpleButton *button = NULL;
	TextEditor *nameEntry = NULL;
	SpinButton *valueSpinButton = NULL;
	const char *name = (controllerValue == NULL) ? "" :
	  ((char *) controllerValue->name);
	uint32_t curValue = (controllerValue == NULL) ? 0 : controllerValue->value;
	Container *result = new Container(component, TRUE);

	box = getBoxWithLabel(component, result, TRUE, "name:");
	nameEntry = getTextEditor(box);
	nameEntry->setText(name);

	box = getBoxWithLabel(component, result, TRUE,
	  (IS_NOTE(current.pattern)) ?
	  "velocity value:" : "controller value");

	valueSpinButton = getSpinButton(component, box, 0, 127, 0);
	valueSpinButton->label->setText(intToString(curValue),
	  sendNotificationSync);

	if (!IS_NOTE(current.pattern)) {
		button =
		  getSimpleButton(result, "Trigger", cbTriggerControllerValue, NULL);
	}
	button =
	  getSimpleButton(result, "OK", cbSetupControllerValue, controllerValue);

	setupValue.nameEntry = nameEntry;
	setupValue.controllerOrVelocity.valueSpinButton = valueSpinButton;

	component->setSize(400, 300);

	return result;
}

static char *charToString(char c)
{
	MARK();

	static char string[2];

	snprintf(string, sizeof(string), "%c", c);

	return string;
}

static Container *getSetupNoteValueDialog(Component *component,
  noteValue_t *noteValue)
{
	MARK();

	uint8_t activeIdx = 0;
	Container *box = NULL;
	SimpleButton *button = NULL;
	TextEditor *nameEntry = NULL;
	SimpleToggleButton *sharpCheckButton = NULL;
	SpinButton *octaveSpinButton = NULL;
	SimpleToggleButton *noteRadios[ARRAY_SIZE(notes)];
	int8_t octave = (noteValue == NULL) ? 0 : noteValue->octave;
	gboolean sharp = (noteValue == NULL) ? FALSE : noteValue->sharp;
	const char *name = (noteValue == NULL) ? "" : (char *) noteValue->name;
	char note = (noteValue == NULL) ? notes[0].name : noteValue->note;
	Container *result = new Container(component, TRUE);

	box = getBoxWithLabel(component, result, TRUE, "name:");
	nameEntry = getTextEditor(box);
	nameEntry->setText(name);
	box = getBoxWithLabel(component, result, FALSE, "note:");
	for (uint32_t i = 0; i < ARRAY_SIZE(noteRadios); i++) {
		noteRadios[i] =
		  getSimpleToggleButton(box, charToString(NOTE2CHAR(notes[i].name)),
		  42, cbSelectTone, GUINT_TO_POINTER(i));
		if (notes[i].name == note) {
			activeIdx = i;
		}
	}
	sharpCheckButton = getSimpleToggleButton(result, "sharp", -1, NULL, NULL);
	sharpCheckButton->setToggleState(sharp, sendNotificationSync);
	box = getBoxWithLabel(component, result, TRUE, "octave:");
	octaveSpinButton = getSpinButton(component, box, -128, 127, 0);
	octaveSpinButton->label->setText(intToString(octave), sendNotificationSync);
	button = getSimpleButton(result, "OK", cbSetupNoteValue, noteValue);

	setupValue.nameEntry = nameEntry;
	memcpy(setupValue.note.noteRadios, noteRadios, sizeof(noteRadios));
	setupValue.note.sharpCheckButton = sharpCheckButton;
	setupValue.note.octaveSpinButton = octaveSpinButton;

	noteRadios[activeIdx]->setToggleState(TRUE, sendNotificationSync);

	component->setSize(400, 300);

	return result;
}

static Container *getSetupPatternDialog(Component *component)
{
	MARK();

	SimpleToggleButton *dummyRadio = NULL;
	SimpleToggleButton *noteRadio = NULL;
	SimpleToggleButton *controllerRadio = NULL;
	TextEditor *nameEntry = NULL;
	SpinButton *channelSpinButton = NULL;
	SpinButton *controllerSpinButton = NULL;
	TextButton *button = NULL;
	Container *result = new Container(component, TRUE);
	Container *box  = new Container(component, FALSE);

	result->addContainer(box);
	dummyRadio = getSimpleToggleButton(box, "DUMMY", 42,
	  cbSelectPatternType, GUINT_TO_POINTER(patternTypeDummy));

	noteRadio = getSimpleToggleButton(box, "NOTE", 42,
	  cbSelectPatternType, GUINT_TO_POINTER(patternTypeNote));

	controllerRadio = getSimpleToggleButton(box, "CONTROLLER", 42,
	  cbSelectPatternType, GUINT_TO_POINTER(patternTypeController));

	box = getBoxWithLabel(component, result, TRUE, "name:");
	nameEntry = getTextEditor(box);

	box = getBoxWithLabel(component, result, TRUE, "channel:");

	channelSpinButton = getSpinButton(component, box, 1, 16, 1);
	box = getBoxWithLabel(component, result, TRUE, "controller:");
	controllerSpinButton =  getSpinButton(component, box, 1, 127, 1);

	button = getSimpleButton(result, "OK", cbDoAddPattern, NULL);

	createPattern.parent = patternList.parent;
	createPattern.nameEntry = nameEntry;
	createPattern.dummyRadio = dummyRadio;
	createPattern.noteRadio = noteRadio;
	createPattern.controllerRadio = controllerRadio;
	createPattern.channelSpinButton = channelSpinButton;
	createPattern.controllerSpinButton = controllerSpinButton;

	createPattern.dummyRadio->setToggleState(TRUE, sendNotificationSync);

	component->setSize(400, 300);

	return result;
}

static Container *getSetupValueDialog(Component *component)
{
	MARK();

	Container *result = NULL;

	
	if (setupValue.noteSetup) {
		result = getSetupNoteValueDialog(component,
		  ((noteValue_t *) setupValue.value));
	} else {
		result = getSetupControllerValueDialog(component,
		  ((controllerValue_t *) setupValue.value));
	}

	return result;
}

static Container *stepsBox = NULL;
static Container *stepsView = NULL;
static SimpleButton *parentButton = NULL;
static SimpleButton *topButton = NULL;
static SimpleButton *loadButton = NULL;
static SimpleButton *storeButton = NULL;
static SimpleButton *barsButton = NULL;
static SimpleButton *stepsPerBarButton = NULL;
static SimpleButton *childrenButton = NULL;
static SimpleButton *siblingsButton = NULL;
static SimpleButton *previousButton = NULL;
static SimpleButton *nextButton = NULL;
static SimpleButton *valuesButton = NULL;
static SimpleButton *velocitiesButton = NULL;
static SimpleButton *randomiseButton = NULL;

static void enableButtons(void)
{
	MARK();

	uint32_t nrValues = NR_VALUES(current.pattern);

	if (IS_ROOT(current.pattern)) {
		goto finish;
	}

	if (IS_NOTE(current.pattern)) {
		nrValues *= NR_VELOCITIES(current.pattern);
	}

	parentButton->setEnabled(TRUE);
	topButton->setEnabled(TRUE);
	barsButton->setEnabled(
	  (g_slist_length(((GSList *) CHILDREN(current.pattern))) < 1));
	stepsPerBarButton->setEnabled(
	  (g_slist_length(((GSList *) CHILDREN(current.pattern))) < 1));
	siblingsButton->setEnabled(TRUE);
	if (current.bar > 0) {
		previousButton->setEnabled(TRUE);
	}
	if (current.bar < (NR_BARS(current.pattern) - 1)) {
		nextButton->setEnabled(TRUE);
	}
	if (nrValues > 0) {
		randomiseButton->setEnabled(TRUE);
	}
	if (IS_DUMMY(current.pattern)) {
		goto finish;
	}
	valuesButton->setEnabled(TRUE);
	velocitiesButton->setEnabled(IS_NOTE(current.pattern));

finish:
	return;
}

static Container *getBox(Component *component, gboolean vertical)
{
	MARK();

	return new Container(component, vertical);
}

static char *intToString(int32_t number)
{
	MARK();

	static char string[ELEVEN];

	snprintf(string, sizeof(string), "%d", number);

	return string;
}

static void doSetColor(SimpleLabel *label, guint16 color)
{
	MARK();

	uint8_t intensity = ((color * 0xff) / 0xffff);

	Colour *colour = new Colour(intensity, intensity, 0xff);

	label->setColour(Label::backgroundColourId, *colour);
	delete colour;
}

static void setColor(SimpleLabel *label, uint32_t i,
  uint32_t shadesSize, guint16 *shades)
{
	MARK();

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
	doSetColor(label, shades[index]);
}

static void addStepLabels(Container *container, uint32_t idx,
  guint16 *shades, uint32_t shadesSize)
{
	MARK();

	SimpleLabel *label = NULL;

	idx %= NR_USERSTEPS_PER_BAR(current.pattern);

	label = getLabel(container, intToString(idx + 1));
	setColor(label, idx, shadesSize, shades);

	label = getLabel(container, NULL);
	signalling.labels = g_slist_append(signalling.labels, label);
}

static void cbLockSlide(void *data)
{
	MARK();

	uint32_t idx = GPOINTER_TO_UINT(data);
	
	lockSlide(current.pattern, idx);
	renderPattern();
}

static void cbLockUserStep(void *data)
{
	MARK();

	uint32_t idx = GPOINTER_TO_UINT(data);
	
	lockUserStep(current.pattern, idx);
	renderPattern();
}

static void addLockButton(Container *container, gboolean locked,
  gboolean enabled, uint32_t idx, gboolean slide)
{
	MARK();

	SimpleButton *button = NULL;
	const char *text = locked ? "!LOCK" : "LOCK";
	fnButtonCallback_t cb = slide ? cbLockSlide : cbLockUserStep;

	button = getSimpleButton(container, text, cb, GUINT_TO_POINTER(idx));
	button->setEnabled(enabled);
}

static void renderUserStep(Container *container, pattern_t *pattern,
  uint32_t idx, fnButtonCallback_t cb, guint16 *shades, uint32_t shadesSize)
{
	MARK();

	uint8_t value = 255;
	Colour *colour = NULL;
	gboolean locked = FALSE;
	gboolean enabled = FALSE;
	SimpleButton *button = NULL;
	gboolean unlockable = FALSE;
	uint32_t nrValues = NR_VALUES(pattern);
	void *step = USERSTEP_AT(pattern, idx);
	char *text = (char *) DISPLAYTEXT(step, TYPE(pattern));

	if (IS_NOTE(pattern)) {
		nrValues *= NR_VELOCITIES(pattern);
	}

	addStepLabels(container, idx, shades, shadesSize);

	locked = getLocked(&unlockable, step, pattern, idx);
	addLockButton(container, locked, unlockable, idx, FALSE);
	button = getSimpleButton(container, text, cb, GUINT_TO_POINTER(idx));
	if (IS_SET(step, TYPE(pattern)))  {
		if (IS_CONTROLLER(pattern))  {
			value = ((controllerValue_t *)
			  (((controllerUserStep_t *) step)->value->data))->value;
		} else if (IS_NOTE(pattern)) {
			value = ((controllerValue_t *)
			  (((noteUserStep_t *) step)->velocity->data))->value;
		}

		value = 255 - value;
		colour = new Colour(value, value, 0xff);
		button->setColour(TextButton::buttonColourId, *colour);
		delete colour;
	}
	enabled = ((nrValues > 0)&&(!locked));
	button->setEnabled(enabled);
}

static void cbSlide(void *data)
{
	MARK();

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

static void renderSlide(Container *container, pattern_t *pattern,
  uint32_t idx)
{
	MARK();

	TextButton *button = NULL;
	noteUserStep_t *step = ((noteUserStep_t *) USERSTEP_AT(pattern, idx));
	gboolean enabled = IS_SET(step, TYPE(pattern));
	const char *text = step->slide ? "!SLIDE" : "SLIDE";

	if (enabled) {
		enabled = !(step->slideLocked);
		if (enabled) {
			enabled = (idx + 1) < NR_USERSTEPS(pattern);
		}
	}

	button = getSimpleButton(container, text, cbSlide, GUINT_TO_POINTER(idx));
	button->setEnabled(enabled);

	addLockButton(container, step->slideLocked,
	  IS_SET(step, TYPE(pattern)), idx, TRUE);
}

static void cbDummyStep(void *data)
{
	MARK();

	err_t err;
	err_t *e = &err;
	dummyUserStep_t *dummyUserStep = NULL;
	uint32_t idx = GPOINTER_TO_UINT(data);

	initErr(e);

	dummyUserStep = (dummyUserStep_t *) USERSTEP_AT(current.pattern, idx);
	terror(setDummyStep(current.pattern, dummyUserStep,
	  !(dummyUserStep->set), &lockContext, e))
	renderPattern();

finish:
	return;
}

static void cbNoteStep(void *data)
{
	MARK();

	err_t err;
	err_t *e = &err;
	uint32_t idx = GPOINTER_TO_UINT(data);
	noteUserStep_t *noteUserStep =
	  (noteUserStep_t *) (USERSTEP_AT(current.pattern, idx));
	GSList *value = noteUserStep->value;
	GSList *velocity = noteUserStep->velocity;

	initErr(e);

	do {
		velocity = (velocity == NULL) ?
		  VELOCITIES(current.pattern) : g_slist_next(velocity);

		if (velocity == NULL) {
			value = g_slist_next(value);
			if (value != NULL) {
				velocity = VELOCITIES(current.pattern);
			}
		} else if (value == NULL) {
			value = VALUES(current.pattern);
		}
	} while ((value == NULL)&&anyChildStepSet(current.pattern, idx));

	terror(setNoteStep(current.pattern, noteUserStep,
	  value, velocity, idx, &lockContext, TRUE, e))
	renderPattern();

finish:
	return;
}

static void cbControllerStep(void *data)
{
	MARK();

	err_t err;
	err_t *e = &err;
	GSList *value = NULL;
	controllerUserStep_t *controllerUserStep = NULL;
	uint32_t idx = GPOINTER_TO_UINT(data);

	initErr(e);

	controllerUserStep =
	  (controllerUserStep_t *) USERSTEP_AT(current.pattern, idx);
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

static void renderSteps(pattern_t *pattern, uint32_t bar)
{
	MARK();

	guint16 *shades = NULL;
	uint32_t shadesSize = 0;
	uint32_t shadeStep = 0;
	int32_t value = 0;
	fnButtonCallback_t callback = CB_STEP(TYPE(pattern));
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
	shades = (guint16 *) calloc(shadesSize, sizeof(guint16));
	value = 0;
	for (int i = (shadesSize - 1); i >= 0; i--) {
		shades[i] = value;
		value += shadeStep;
		if (value > 0xffff) {
			value = 0xffff;
		}
	}

	stepsView = getBox(application.component, FALSE);
	stepsBox->addContainer(stepsView);
	
	for (uint32_t i = start; i < end; i++) {
		Container *buttonBox = getBox(application.component, TRUE);
		stepsView->addContainer(buttonBox);

		renderUserStep(buttonBox, pattern, i, callback, shades, shadesSize);
		if (!IS_NOTE(pattern)) {
			continue;
		}

		renderSlide(buttonBox, pattern, i);
	}

	free(shades);

	signalling.bar = bar;
	signalling.pattern = pattern;
	signalling.bars = NR_BARS(pattern);
	signalling.userStepsPerBar = NR_USERSTEPS_PER_BAR(pattern);
}

static void renderPattern(void)
{
	MARK();

	gboolean locked = FALSE;
	uint32_t width = (WINDOW_WIDTH / (NR_USERSTEPS_PER_BAR(current.pattern))) <
	  (width = (WINDOW_WIDTH / 16)) ?
	  (width * NR_USERSTEPS_PER_BAR(current.pattern)) : WINDOW_WIDTH;

	application.component->setSize(width, WINDOW_HEIGHT);
	loadButton->setEnabled(TRUE);
	storeButton->setEnabled(TRUE);
	childrenButton->setEnabled(TRUE);
	parentButton->setEnabled(FALSE);
	topButton->setEnabled(FALSE);
	barsButton->setEnabled(FALSE);
	stepsPerBarButton->setEnabled(FALSE);
	siblingsButton->setEnabled(FALSE);
	previousButton->setEnabled(FALSE);
	nextButton->setEnabled(FALSE);
	valuesButton->setEnabled(FALSE);
	velocitiesButton->setEnabled(FALSE);
	randomiseButton->setEnabled(FALSE);
	if (stepsView !=  NULL) {
		stepsBox->removeContainer(stepsView);
		delete stepsView; stepsView = NULL;
	}
	g_slist_free(signalling.labels); signalling.labels = NULL;
	if (IS_ROOT(current.pattern)) {
		goto finish;
	}
	enableButtons();
	renderSteps(current.pattern, current.bar);
finish:
	signalling.generation++;
	layout(application.component, application.container);
}

static Container *getBoxWithLabel(Component *component, Container *container,
  gboolean vertical, const char *labelText)
{
	MARK();

	Container *result = new Container(component, vertical);
	SimpleLabel *label = getLabel(result, labelText);
	
	container->addContainer(result);

	return result;
}

static SimpleButton *getSimpleButton(Container *container, const char *text,
  fnButtonCallback_t callback, void *data)
{
	MARK();

	SimpleButton *result = new SimpleButton(text, callback, data);

	container->addComponent(result);

	return result;
}

static void cbSetBars(void *data)
{
	MARK();

	pattern_t *pattern = (pattern_t *) numberPicker.ptr;
	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(adjustSteps(pattern,
	  numberPicker.label->getText(FALSE).getIntValue(),
	  NR_STEPS_PER_BAR(pattern), &lockContext, TRUE, e))

	if (current.bar >= NR_BARS(pattern)) {
		current.bar = NR_BARS(pattern) - 1;
	}

finish:
	destroyDialog();
	renderPattern();
}

static void cbSetStepsPerBar(void *data)
{
	MARK();

	pattern_t *pattern = (pattern_t *) numberPicker.ptr;
	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(adjustSteps(pattern, NR_BARS(pattern),
	  numberPicker.label->getText(FALSE).getIntValue(),
	  &lockContext, TRUE, e))

finish:
	destroyDialog();
	renderPattern();
}

static void cbPickBars(pickType_t pickType)
{
	MARK();

	pattern_t *pattern = (pattern_t *) numberPicker.ptr;
	int value = numberPicker.label->getText(FALSE).getIntValue();

	if (pickType == pickTypeDummy) {
		goto finish;
	}

	if (pickType == pickTypeMore) {
		value += NR_BARS(PARENT(pattern));
	} else {
		value -= NR_BARS(PARENT(pattern));
	}

	numberPicker.label->setText(intToString(value));

finish:
	numberPicker.downButton->setEnabled((value > NR_BARS(PARENT(pattern))));
	numberPicker.upButton->setEnabled(((value * 2) <= MAX_BARS));
}

static void cbPickStepsPerBar(pickType_t pickType)
{
	MARK();

	int doubled = 0;
	pattern_t *pattern = (pattern_t *) numberPicker.ptr;
	int value = numberPicker.label->getText(FALSE).getIntValue();

	if (pickType == pickTypeDummy) {
		goto finish;
	}

	if (pickType == pickTypeMore) {
		value *= 2;
	} else {
		value /= 2;
	}

	numberPicker.label->setText(intToString(value));

finish:
	numberPicker.downButton->setEnabled(
	  (value > NR_STEPS_PER_BAR(PARENT(pattern))));
	numberPicker.upButton->setEnabled(
	  ((doubled = (value * 2)) > 0)&&(doubled <= MAX_STEPS_PER_BAR(pattern)));
}

static void cbPickMore(void *data)
{
	MARK();

	numberPicker.fnPickNumber(pickTypeMore);
}

static void cbPickLess(void *data)
{
	MARK();

	numberPicker.fnPickNumber(pickTypeLess);
}

static Container *getNumberPickerBox(Component *component,
  fnPickNumber_t fnPickNumber, int value, void *ptr)
{
	MARK();

	SimpleLabel *label = NULL;
	TextButton *upButton = NULL;
	TextButton *downButton = NULL;
	Container *result = getBox(component, FALSE);
	
	downButton = getSimpleButton(result, "-", cbPickLess, NULL);

	label = getLabel(result, intToString(value));

	upButton = getSimpleButton(result, "+", cbPickMore, NULL);

	numberPicker.fnPickNumber = fnPickNumber;
	numberPicker.downButton = downButton; downButton = NULL;
	numberPicker.upButton = upButton; upButton = NULL;
	numberPicker.label = label; label = NULL;
	numberPicker.ptr = ptr;

	return result;
}

static Container *getNumberPickerDialog(Component *component,
  fnPickNumber_t fnPickNumber, fnButtonCallback_t onOk, int initial, void *ptr)
{
	MARK();

	Container *result = new Container(component, TRUE);
	Container *box = NULL;
	TextButton *okButton = NULL;

	box = getNumberPickerBox(component, fnPickNumber, initial, ptr);
	result->addContainer(box);
	numberPicker.fnPickNumber(pickTypeDummy);

	okButton = getSimpleButton(result, "OK", onOk, NULL);

	component->setSize(200, 150);

	return result;
}

static void cbEnterPattern(void *data)
{
	MARK();

	enterPattern(((pattern_t *) data));
	destroyDialog();
}

static void cbDeletePattern(void *data)
{
	MARK();

	err_t err;
	err_t *e = &err;

	initErr(e);

	terror(deleteChild(patternList.parent, ((GSList *) data), &lockContext, e))

	renderPattern();
	showDialog(getPatternListDialog);

finish:
	return;
}

static void cbAddPattern(void *data)
{
	MARK();

	showDialog(getSetupPatternDialog);
}

static void cbSetupValue(void *data)
{
	MARK();

	setupValue.pattern = current.pattern;
	setupValue.value = data;
	setupValue.noteSetup = IS_NOTE(current.pattern);
	showDialog(getSetupValueDialog);
}

static void cbSetupVelocity(void *data)
{
	MARK();

	setupValue.pattern = current.pattern;
	setupValue.value = data;
	setupValue.noteSetup = FALSE;
	showDialog(getSetupValueDialog);
}

static void cbDeleteValue(void *data)
{
	MARK();

	GSList *link = (GSList *) data;

	freeValue(current.pattern, link->data);

	VALUES(current.pattern) =
	  g_slist_delete_link(VALUES(current.pattern), link);

	renderPattern();
	showDialog(getValuesDialog);
}

static void cbDeleteVelocity(void *data)
{
	MARK();

	GSList *link = (GSList *) data;

	freeControllerValue(((controllerValue_t *) link->data));

	VELOCITIES(current.pattern) =
	  g_slist_delete_link(VELOCITIES(current.pattern), link);

	renderPattern();
	showDialog(getVelocitiesDialog);
}

static gboolean isValueInUse(pattern_t *pattern, GSList *value)
{
	MARK();

	gboolean result = FALSE;

	for (uint32_t i = 0; i < NR_USERSTEPS(pattern); i++) {
		if (value == VALUE(USERSTEP_AT(pattern, i), TYPE(pattern))) {
			result = TRUE;
			break;
		}
	}

	return result;
}

static gboolean isVelocityInUse(pattern_t *pattern, GSList *value)
{
	MARK();

	gboolean result = FALSE;

	for (uint32_t i = 0; i < NR_USERSTEPS(pattern); i++) {
		if (value == VELOCITY(USERSTEP_AT(pattern, i), TYPE(pattern))) {
			result = TRUE;
			break;
		}
	}

	return result;
}

static Container *getValuesVelocitiesDialog(Component *component,
  gboolean velocities)
{
	MARK();

	GSList *cur = NULL;
	Container *box = NULL;
	TextButton *button = NULL;
	Container *result = new Container(component, TRUE);

	for (cur = velocities ? VELOCITIES(current.pattern) :
	  VALUES(current.pattern); cur != NULL; cur = g_slist_next(cur)) {
		void *value = cur->data;
		box = getBoxWithLabel(component, result, FALSE,
		  velocities ? (char *) VELOCITY_NAME(value, TYPE(current.pattern)) :
		  (char *) VALUE_NAME(value, TYPE(current.pattern)));
		button = getSimpleButton(box, "Edit", velocities ?
		  cbSetupVelocity : cbSetupValue, value);
		button = getSimpleButton(box, "Delete", velocities ?
		  cbDeleteVelocity : cbDeleteValue, cur);
		button->setEnabled(velocities ?
		  (!isVelocityInUse(current.pattern, cur)) :
		  (!isValueInUse(current.pattern, cur)));
	}

	button = getSimpleButton(result, "Add",
	  velocities ? cbSetupVelocity : cbSetupValue, NULL);

	component->setSize(400, 300);

	return result;
}

static Container *getValuesDialog(Component *component)
{
	return getValuesVelocitiesDialog(component, FALSE);
}

static Container *getVelocitiesDialog(Component *component)
{
	return getValuesVelocitiesDialog(component, TRUE);
}

static Container *getPatternListDialog(Component *component)
{
	MARK();

	TextButton *button = NULL;
	Container *box = NULL;
	Container *result = new Container(component, TRUE);

	for (GSList *cur = (GSList *) CHILDREN(patternList.parent); cur != NULL;
	  cur = g_slist_next(cur)) {
		pattern_t *pattern = (pattern_t*) cur->data;

		if (pattern == patternList.hideMe) {
			continue;
		}

		box = getBoxWithLabel(component, result, FALSE, NAME(pattern));
		button = getSimpleButton(box, "Enter", cbEnterPattern, pattern);
		button = getSimpleButton(box, "Delete", cbDeletePattern, cur);
		button->setEnabled((CHILDREN(pattern) == NULL));

	}

	button = getSimpleButton(result, "Add", cbAddPattern, NULL);

	component->setSize(400, 300);

	return result;
}

static Container *getBarsDialog(Component *component)
{
	MARK();

	Container *result = getNumberPickerDialog(component, cbPickBars, cbSetBars,
	  NR_BARS(current.pattern), current.pattern);

	return result;
}

static Container *getStepsPerBarDialog(Component *component)
{
	MARK();

	Container *result =
	  getNumberPickerDialog(component, cbPickStepsPerBar, cbSetStepsPerBar,
	  NR_STEPS_PER_BAR(current.pattern), current.pattern);

	return result;
}

static void deleteContainer(Container *container)
{
	MARK();

	delete container;
}

static void cbParent(void *data)
{
	MARK();

	enterPattern(PARENT(current.pattern));
}

static void cbTop(void *data)
{
	MARK();

	enterPattern(((pattern_t  *) patterns.root));
}

static char *getPath(gboolean load, err_t *e)
{
	MARK();

	char *result = NULL;
	bool success = FALSE;
	FileChooser fileChooser("Please select the moose you want to load...",
	  File::getSpecialLocation (File::userHomeDirectory),
	  FILE_PATTERN);

	if (load) {
		success = fileChooser.browseForFileToOpen();
	} else {
		success = fileChooser.browseForFileToSave(TRUE);
	}
	terror(failIfFalse(success))

	result = strdup(fileChooser.getResult().getFullPathName().toRawUTF8());

finish:
	return result;
}

typedef ssize_t (*readWriteFunc_t)(int fd, void *buf, size_t count);

static void readWriteFd(void *data, uint32_t length, int fd,
  gboolean reading, err_t *e)
{
	MARK();

	char *cur = (char *) data;
	uint32_t pending = length;
	readWriteFunc_t func = reading ? ((readWriteFunc_t) read) :
	  ((readWriteFunc_t) write);

	while (pending > 0) {
		ssize_t just = func(fd, cur, pending);
		if (just < 1) {
			terror(failIfFalse((just < 0)&&((errno ==
			  EINTR)/*||(errno == EAGAIN)*/)))
			continue;
		}
		pending -= just;
		cur += just;
	}
finish:
	return;
}

static void loadStorePattern(pattern_t **pattern, int fd, gboolean load,
  pattern_t *parent, err_t *e);

static void loadStoreChildren(pattern_t *parent, int fd,
  gboolean load, err_t *e)
{
	MARK();

	uint32_t count = 0;

	if (!load) {
		count = g_slist_length((GSList *) parent->children);
	}

	terror(readWriteFd(&count, sizeof(count), fd, load, e))

	for (uint32_t i = 0; i < count; i++) {
		pattern_t *child = NULL;
		if (!load) {
			child =
			  (pattern_t *) g_slist_nth_data((GSList *) parent->children, i);
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

static char *readStringFromFd(int fd, err_t *e)
{
	MARK();

	size_t length = 0;
	char *result = NULL;
	char *_result = NULL;

	terror(readWriteFd(&length, sizeof(length), fd, TRUE, e))
	_result = (char *) calloc(1, (length + 1));
	terror(readWriteFd(_result, length, fd, TRUE, e))
	_result[length] = '\0';

	result = _result; _result = NULL;
finish:
	return result;
}

static void writeStringToFd(char *string, int fd, err_t *e)
{
	MARK();

	size_t length = strlen(string);

	terror(readWriteFd(&length, sizeof(length), fd, FALSE, e))
	terror(readWriteFd(string, length, fd, FALSE, e))

finish:
	return;
}

static void loadStoreNoteValue(noteValue_t **noteValue, int fd,
  gboolean load, err_t *e)
{
	MARK();

	noteValue_t *freeMe = NULL;

	if (load) {
		freeMe = (*noteValue) = allocateNoteValue();
		terror((*noteValue)->name = readStringFromFd(fd, e))
	} else {
		terror(writeStringToFd((char *) (*noteValue)->name, fd, e))
	}

	terror(readWriteFd((void *) &((*noteValue)->note),
	  sizeof((*noteValue)->note), fd, load, e))
	terror(readWriteFd((void *) &((*noteValue)->sharp),
	  sizeof((*noteValue)->sharp), fd, load, e))
	terror(readWriteFd((void *) &((*noteValue)->octave),
	  sizeof((*noteValue)->octave), fd, load, e))

	freeMe = NULL;
finish:
	if (freeMe != NULL) {
		freeNoteValue(freeMe);
	}
}

static void loadStoreControllerValue(controllerValue_t **controllerValue,
  int fd, gboolean load, err_t *e)
{
	MARK();

	controllerValue_t *freeMe = NULL;

	if (load) {
		freeMe = (*controllerValue) = allocateControllerValue();
		terror((*controllerValue)->name = readStringFromFd(fd, e))
	} else {
		terror(writeStringToFd((char *) (*controllerValue)->name, fd, e))
	}

	terror(readWriteFd((void *) &((*controllerValue)->value),
	  sizeof((*controllerValue)->value), fd, load, e))

	freeMe = NULL;
finish:
	if (freeMe != NULL)  {
		freeControllerValue(freeMe);
	}
}

static void loadStoreValue(void **value, pattern_t *pattern, int fd,
  gboolean load, gboolean velocities, err_t *e)
{
	MARK();

	if (velocities || IS_CONTROLLER(pattern)) {
		terror(loadStoreControllerValue(((controllerValue_t **) value),
		  fd, load, e))
	} else {
		terror(loadStoreNoteValue(((noteValue_t **) value), fd, load, e))
	}

finish:
	return;
}

static void loadStoreValuesVelocities(pattern_t *pattern, int fd,
  gboolean load, gboolean velocities, err_t *e)
{
	MARK();

	guint length = 0;
	uint32_t i = 0;
	void *value = NULL;

	if (!load) {
		length = velocities ? NR_VELOCITIES(pattern) : NR_VALUES(pattern);
	}
	terror(readWriteFd(&length, sizeof(length), fd, load, e))

	for (i = 0; i < length; i++) {
		if (!load) {
			value = g_slist_nth_data(velocities ?
			  VELOCITIES(pattern) : VALUES(pattern), i);
		}
		terror(loadStoreValue(&value, pattern, fd, load, velocities, e))
		if (load) {
			if (velocities) {
				VELOCITIES(pattern) =
				  g_slist_append(VELOCITIES(pattern), value);
			} else {
				VALUES(pattern) = g_slist_append(VALUES(pattern), value);
			}
		}
	}

finish:
	return;
}

static void loadStoreValues(pattern_t *pattern, int fd, gboolean load, err_t *e)
{
	terror(loadStoreValuesVelocities(pattern, fd, load, FALSE, e))

finish:
	return;
}

static void loadStoreVelocities(pattern_t *pattern, int fd,
  gboolean load, err_t *e)
{
	terror(loadStoreValuesVelocities(pattern, fd, load, TRUE, e))

finish:
	return;
}

static void loadStoreStep(void *step, pattern_t *pattern, int fd,
  uint32_t idx, gboolean load, err_t *e)
{
	MARK();

	gint valuePosition = -1;
	gint velocityPosition = -1;
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
		valuePosition = g_slist_position(VALUES(pattern),
		  (GSList *) VALUE(step, TYPE(pattern)));
		if (IS_NOTE(pattern))  {
			velocityPosition = g_slist_position(VELOCITIES(pattern),
			  (GSList *) VELOCITY(step, TYPE(pattern)));
		}
	}
	terror(readWriteFd(&valuePosition, sizeof(valuePosition), fd, load, e))
	if (IS_NOTE(pattern)) {
		terror(readWriteFd(&velocityPosition,
		  sizeof(velocityPosition), fd, load, e))
	}
	if ((load)&&(valuePosition > -1)) {
		if (IS_NOTE(pattern)) {
			terror(setNoteStep(pattern, (noteUserStep_t *) step,
			  g_slist_nth(VALUES(pattern), valuePosition),
			  g_slist_nth(VELOCITIES(pattern), velocityPosition),
			  idx, &lockContext, FALSE, e))
		} else {
			terror(setControllerStep(pattern, (controllerUserStep_t *) step,
			  g_slist_nth(VALUES(pattern), valuePosition), idx,
			  &lockContext, e))
		}
	}
	if (IS_CONTROLLER(pattern)) {
		goto finish;
	}

	noteUserStep = (noteUserStep_t *) step;
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
	MARK();

	void *freeMe = NULL;
	pattern_t *p = NULL;

	if (load) {
		freeMe = p = allocatePattern(parent);
		PARENT(p) = parent;
		terror(NAME(p) = readStringFromFd(fd, e))	
	} else {
		p = *pattern;
		terror(writeStringToFd(NAME(p), fd, e))
	}

	terror(readWriteFd(&TYPE(p),
	  sizeof(TYPE(p)), fd, load, e))
	if (!IS_DUMMY(p)) {
		terror(readWriteFd((void *) PTR_CHANNEL(p),
		  sizeof(CHANNEL(p)), fd, load, e))
	}
	terror(readWriteFd((void *) &NR_USERSTEPS_PER_BAR(p),
	  sizeof(NR_USERSTEPS_PER_BAR(p)), fd, load, e))
	terror(readWriteFd((void *) &NR_BARS(p),
	  sizeof(NR_BARS(p)), fd, load, e))

	if (load) {
		terror(adjustSteps(p, NR_BARS(p),
		  NR_USERSTEPS_PER_BAR(p), &lockContext, FALSE, e))
	}

	if (!IS_DUMMY(p)) {
		terror(loadStoreValues(p, fd, load, e))
		if (!IS_NOTE(p)) {
			terror(readWriteFd((void *) PTR_PARAMETER(p),
			  sizeof(PARAMETER(p)), fd, load, e))
		}
		terror(loadStoreVelocities(p, fd, load, e))
	}

	for (uint32_t i = 0; i < NR_USERSTEPS(p); i++) {
		void *step = USERSTEP_AT(p, i);
		terror(loadStoreStep(step, p, fd, i, load, e))
	}

	terror(loadStoreChildren(p, fd, load, e))

	if (load) {
		*pattern = p;
	}
	freeMe = NULL;
finish:
	if (freeMe != NULL) {
		freePattern((pattern_t *) freeMe);
	}
}



static void cbLoad(void *data)
{
	MARK();

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
		NativeMessageBox::showMessageBox(AlertWindow::WarningIcon, "Error",
		  err2string(e), application.component);
	}
}

static void cbStore(void *data)
{
	MARK();

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
		NativeMessageBox::showMessageBox(AlertWindow::WarningIcon, "Error",
		  err2string(e), application.component);
	}
}

static void cbBars(void *data)
{
	MARK();

	showDialog(getBarsDialog);
}

static void cbStepsPerBar(void *data)
{
	MARK();

	showDialog(getStepsPerBarDialog);
}

static void doChildren(pattern_t *parent, pattern_t *hideMe)
{
	MARK();

	patternList.parent = parent;
	patternList.hideMe = hideMe;

	showDialog(getPatternListDialog);
}

static void cbChildren(void *data)
{
	MARK();

	doChildren(current.pattern, NULL);
}

static void cbSiblings(void *data)
{
	MARK();

	doChildren(PARENT(current.pattern), current.pattern);
}

static void cbPrevious(void *data)
{
	MARK();

	current.bar--;
	renderPattern();
}

static void cbNext(void *data)
{
	MARK();

	current.bar++;
	renderPattern();
}

static void cbValues(void *data)
{
	MARK();

	showDialog(getValuesDialog);
}

static void cbVelocities(void *data)
{
	MARK();

	showDialog(getVelocitiesDialog);
}

static void cbRandomise(void *data)
{
	MARK();

	if (randomising) {
		goto finish;
	}
	randomising = TRUE;
	randomise(current.pattern, current.bar, &lockContext);

	renderPattern();
	randomising = FALSE;

finish:
	return;
}

static Container *getCentreBox(void)
{
	MARK();

	Container * valuesBox = getBox(application.component, TRUE);
	Container *result = getBox(application.component, TRUE);
	Container *box = getBox(application.component, FALSE);

	result->addContainer(box);

	parentButton = getSimpleButton(box, "Parent!", cbParent, NULL);
	topButton = getSimpleButton(box, "Top!", cbTop, NULL);
	loadButton = getSimpleButton(box, "Load", cbLoad, NULL);
	storeButton = getSimpleButton(box, "Store ...", cbStore, NULL);
	barsButton = getSimpleButton(box, "Bars ...", cbBars, NULL);
	stepsPerBarButton = getSimpleButton(box, "Steps per Bar ...",
	  cbStepsPerBar, NULL);

	box = getBox(application.component, FALSE);
	result->addContainer(box);
	childrenButton = getSimpleButton(box, "Children ...", cbChildren, NULL);
	siblingsButton = getSimpleButton(box, "Siblings ...", cbSiblings, NULL);
	previousButton = getSimpleButton(box, "Previous", cbPrevious, NULL);
	nextButton = getSimpleButton(box, "Next", cbNext, NULL);
	box->addContainer(valuesBox);
	valuesButton = getSimpleButton(valuesBox, "Values ...", cbValues, NULL);
	velocitiesButton =
	  getSimpleButton(valuesBox, "Velocities ...", cbVelocities, NULL);
	randomiseButton = getSimpleButton(box, "Randomise!", cbRandomise, NULL);

	return result;
}

static Container *getLowerBox(void)
{
	MARK();

	return getBox(application.component, TRUE);
}

static Container *getApplicationContainer(void)
{
	MARK();

	Container *box = NULL;
	Container *result = getBox(application.component, TRUE);

	box = getCentreBox();
	result->addContainer(box);

	//this initialisation is necessary because the editor may
	//have been destroyed and created again => we must prevent
	//stepsView to be delete()d in renderPattern()
	stepsView = NULL;
	stepsBox = box = getLowerBox();
	result->addContainer(box);

	return result;
}

static void setup(KVstSequencerAudioProcessorEditor *editor)
{
	MARK();

	application.component = editor;
	application.container = getApplicationContainer();
}

static void enterPattern(pattern_t *pattern)
{
	MARK();

	current.pattern = pattern;
	current.bar = 0;

	renderPattern();
}

static void teardown(void)
{
	MARK();

	if (signalling.labels != NULL) {
		g_slist_free(signalling.labels); signalling.labels = NULL;
	}

	destroyDialog();
	delete application.container;
}

//==============================================================================
KVstSequencerAudioProcessorEditor::KVstSequencerAudioProcessorEditor(
  KVstSequencerAudioProcessor& p) : AudioProcessorEditor (&p), processor (p)
{
	MARK();

	setup(this);
	enterPattern(((pattern_t *) patterns.root));

	initialised = TRUE;
}

KVstSequencerAudioProcessorEditor::~KVstSequencerAudioProcessorEditor()
{
	MARK();

	initialised = FALSE;

	teardown();

finish:
	return;
}

//==============================================================================
void KVstSequencerAudioProcessorEditor::paint (Graphics& g)
{
	MARK();

    g.fillAll (Colours::white);

    g.setColour (Colours::black);
    g.setFont (15.0f);
}

void KVstSequencerAudioProcessorEditor::resized()
{
	MARK();

	layout(application.component, application.container);
}

static SimpleLabel *getLabel(Container *container, const char *text)
{
	MARK();

	SimpleLabel *result = new SimpleLabel();

	if (text != NULL) {
		result->setText(text);
	}

	container->addComponent(result);

	return result;
}

static void runDialog(DialogComponent *dialogComponent)
{
	MARK();

	DialogWindow::LaunchOptions launchOptions;

	destroyDialog();
	launchOptions.content.setOwned(dialogComponent);
	layout(dialogComponent, dialogComponent->container);
	dialog.window = launchOptions.launchAsync();
}

static void showDialog(Container * (*fnGetDialog)(Component *))
{
	MARK();

	DialogComponent *dialogComponent = new DialogComponent();

	dialogComponent->container = fnGetDialog(dialogComponent);
	runDialog(dialogComponent);
}

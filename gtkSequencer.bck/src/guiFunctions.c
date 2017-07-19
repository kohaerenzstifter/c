



pattern_t *createRealPattern2(pattern_t *parent,
  const gchar *name, gint channel, patternType_t patternType, gint controller)
{
	pattern_t *result = allocatePattern(patternlist.parent);

	result->real.name = strdup(name);
	result->real.channel = channel;

	result->real.bars =
	  parent->isRoot ? 1 : parent->real.bars;
	result->real.userStepsPerBar =
	  parent->isRoot ? 1 : parent->real.userStepsPerBar;
	PATTERN_TYPE(result) = patternType;

	if (patternType == PATTERNTYPE_CONTROLLER) {
		result->real.controller.parameter = controller;
	}

	return result;
}
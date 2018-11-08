//***********************************************************************************************
//Multi-phrase 32 step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the SA-100 Stepper Acid sequencer by Transistor Sounds Labs
//
//Acknowledgements: please see README.md
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "PhraseSeqUtil.hpp"


struct PhraseSeq32 : Module {
	enum ParamIds {
		LEFT_PARAM,
		RIGHT_PARAM,
		RIGHT8_PARAM,// not used
		EDIT_PARAM,
		SEQUENCE_PARAM,
		RUN_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		ENUMS(OCTAVE_PARAM, 7),
		GATE1_PARAM,
		GATE2_PARAM,
		SLIDE_BTN_PARAM,
		SLIDE_KNOB_PARAM,
		ATTACH_PARAM,
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),
		RUNMODE_PARAM,
		TRAN_ROT_PARAM,
		GATE1_KNOB_PARAM,
		GATE1_PROB_PARAM,
		TIE_PARAM,// Legato
		CPMODE_PARAM,
		ENUMS(STEP_PHRASE_PARAMS, 32),
		CONFIG_PARAM,
		// -- 0.6.11 ^^
		KEYNOTE_PARAM,
		KEYGATE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		CV_INPUT,
		RESET_INPUT,
		CLOCK_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		// -- 0.6.4 ^^
		MODECV_INPUT,
		GATE1CV_INPUT,
		GATE2CV_INPUT,
		TIEDCV_INPUT,
		SLIDECV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CVA_OUTPUT,
		GATE1A_OUTPUT,
		GATE2A_OUTPUT,
		CVB_OUTPUT,
		GATE1B_OUTPUT,
		GATE2B_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 32 * 2),// room for GreenRed
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12 * 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE1_LIGHT, 2),// room for GreenRed
		ENUMS(GATE2_LIGHT, 2),// room for GreenRed
		SLIDE_LIGHT,
		ATTACH_LIGHT,
		GATE1_PROB_LIGHT,
		TIE_LIGHT,
		KEYNOTE_LIGHT,
		ENUMS(KEYGATE_LIGHT, 2),// room for GreenRed
		RES_LIGHT,
		NUM_LIGHTS
	};
	
	// Constants
	enum DisplayStateIds {DISP_NORMAL, DISP_MODE, DISP_LENGTH, DISP_TRANSPOSE, DISP_ROTATE};
	static constexpr float CONFIG_PARAM_INIT_VALUE = 0.0f;// so that module constructor is coherent with widget initialization, since module created before widget

	// Need to save
	int panelTheme = 0;
	int expansion = 0;
	bool autoseq;
	int pulsesPerStep;// 1 means normal gate mode, alt choices are 4, 6, 12, 24 PPS (Pulses per step)
	bool running;
	int runModeSeq[32];
	int runModeSong;
	int sequence;
	int lengths[32];//1 to 32
	int phrase[32];// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 32
	float cv[32][32];// [-3.0 : 3.917]. First index is patten number, 2nd index is step
	int attributes[32][32];// First index is patten number, 2nd index is step (see enum AttributeBitMasks for details)
	bool resetOnRun;
	bool attached;
	int transposeOffsets[32];

	// No need to save
	int stepIndexEdit;
	int stepIndexRun[2];
	int phraseIndexEdit;
	int phraseIndexRun;
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate
	float editingGateCV;// no need to initialize, this is a companion to editingGate (output this only when editingGate > 0)
	int editingGateKeyLight;// no need to initialize, this is a companion to editingGate (use this only when editingGate > 0)
	int editingChannel;// 0 means channel A, 1 means channel B. no need to initialize, this is a companion to editingGate
	unsigned long stepIndexRunHistory;
	unsigned long phraseIndexRunHistory;
	int displayState;
	unsigned long slideStepsRemain[2];// 0 when no slide under way, downward step counter when sliding
	float slideCVdelta[2];// no need to initialize, this is a companion to slideStepsRemain
	float cvCPbuffer[32];// copy paste buffer for CVs
	int attribOrPhraseCPbuffer[32];
	int lengthCPbuffer;
	int modeCPbuffer;
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	int startCP;
	int rotateOffset;// no need to initialize, this is companion to displayMode = DISP_ROTATE
	long clockIgnoreOnReset;
	unsigned long clockPeriod;// counts number of step() calls upward from last clock (reset after clock processed)
	long tiedWarning;// 0 when no warning, positive downward step counter timer when warning
	int gate1Code[2];
	int gate2Code[2];
	bool attachedChanB;
	long revertDisplay;
	long editingGateLength;// 0 when no info, positive when gate1, negative when gate2
	long lastGateEdit;
	long editingPpqn;// 0 when no info, positive downward step counter timer when editing ppqn
	int ppqnCount;
	int stepConfig;
	

	int stepConfigSync = 0;// 0 means no sync requested, 1 means soft sync (no reset lengths), 2 means hard (reset lengths)
	unsigned int lightRefreshCounter = 0;
	float resetLight = 0.0f;
	int sequenceKnob = 0;
	SchmittTrigger resetTrigger;
	SchmittTrigger leftTrigger;
	SchmittTrigger rightTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger octTriggers[7];
	SchmittTrigger octmTrigger;
	SchmittTrigger gate1Trigger;
	SchmittTrigger gate1ProbTrigger;
	SchmittTrigger gate2Trigger;
	SchmittTrigger slideTrigger;
	SchmittTrigger keyTriggers[12];
	SchmittTrigger writeTrigger;
	SchmittTrigger attachedTrigger;
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger modeTrigger;
	SchmittTrigger rotateTrigger;
	SchmittTrigger transposeTrigger;
	SchmittTrigger tiedTrigger;
	SchmittTrigger stepTriggers[32];
	SchmittTrigger keyNoteTrigger;
	SchmittTrigger keyGateTrigger;
	HoldDetect modeHoldDetect;
	int lengthsBuffer[32];// buffer from Json for thread safety


	inline bool isEditingSequence(void) {return params[EDIT_PARAM].value > 0.5f;}
	inline int getStepConfig(float paramValue) {// 1 = 2x16 = 1.0f,  2 = 1x32 = 0.0f
		return (paramValue > 0.5f) ? 1 : 2;
	}

	inline void initAttrib(int seq, int step) {attributes[seq][step] = ATT_MSK_GATE1;}
	inline bool getGate1(int seq, int step) {return getGate1a(attributes[seq][step]);}
	inline bool getGate1P(int seq, int step) {return getGate1Pa(attributes[seq][step]);}
	inline bool getGate2(int seq, int step) {return getGate2a(attributes[seq][step]);}
	inline bool getSlide(int seq, int step) {return getSlideA(attributes[seq][step]);}
	inline bool getTied(int seq, int step) {return getTiedA(attributes[seq][step]);}
	inline int getGate1Mode(int seq, int step) {return getGate1aMode(attributes[seq][step]);}
	inline int getGate2Mode(int seq, int step) {return getGate2aMode(attributes[seq][step]);}
	
	inline void setGate1Mode(int seq, int step, int gateMode) {attributes[seq][step] &= ~ATT_MSK_GATE1MODE; attributes[seq][step] |= (gateMode << gate1ModeShift);}
	inline void setGate2Mode(int seq, int step, int gateMode) {attributes[seq][step] &= ~ATT_MSK_GATE2MODE; attributes[seq][step] |= (gateMode << gate2ModeShift);}
	inline void setGate1P(int seq, int step, bool gate1Pstate) {setGate1Pa(&attributes[seq][step], gate1Pstate);}
	inline void toggleGate1(int seq, int step) {toggleGate1a(&attributes[seq][step]);}

	inline void fillStepIndexRunVector(int runMode, int len) {
		if (runMode != MODE_RN2) 
			stepIndexRun[1] = stepIndexRun[0];
		else
			stepIndexRun[1] = randomu32() % len;
	}
	
		
	PhraseSeq32() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		for (int i = 0; i < 32; i++)
			lengthsBuffer[i] = 16;
		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		stepConfig = getStepConfig(CONFIG_PARAM_INIT_VALUE);
		autoseq = false;
		pulsesPerStep = 1;
		running = false;
		runModeSong = MODE_FWD;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		sequence = 0;
		phrases = 4;
		for (int i = 0; i < 32; i++) {
			for (int s = 0; s < 32; s++) {
				cv[i][s] = 0.0f;
				initAttrib(i, s);
			}
			runModeSeq[i] = MODE_FWD;
			phrase[i] = 0;
			lengths[i] = 16 * stepConfig;
			cvCPbuffer[i] = 0.0f;
			attribOrPhraseCPbuffer[i] = ATT_MSK_GATE1;
			transposeOffsets[i] = 0;
		}
		initRun(true);
		lengthCPbuffer = 32;
		modeCPbuffer = MODE_FWD;
		countCP = 32;
		startCP = 0;
		editingGate = 0ul;
		infoCopyPaste = 0l;
		displayState = DISP_NORMAL;
		slideStepsRemain[0] = 0ul;
		slideStepsRemain[1] = 0ul;
		attached = true;
		clockPeriod = 0ul;
		tiedWarning = 0ul;
		attachedChanB = false;
		revertDisplay = 0l;
		resetOnRun = false;
		editingGateLength = 0l;
		lastGateEdit = 1l;
		editingPpqn = 0l;
	}
	
	
	void onRandomize() override {
		stepConfig = getStepConfig(params[CONFIG_PARAM].value);
		runModeSong = randomu32() % 5;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		sequence = randomu32() % 32;
		phrases = 1 + (randomu32() % 32);
		for (int i = 0; i < 32; i++) {
			runModeSeq[i] = randomu32() % NUM_MODES;
			phrase[i] = randomu32() % 32;
			lengths[i] = 1 + (randomu32() % (16 * stepConfig));
			transposeOffsets[i] = 0;
			for (int s = 0; s < 32; s++) {
				cv[i][s] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
				attributes[i][s] = randomu32() & 0x1FFF;// 5 bit for normal attributes + 2 * 4 bits for advanced gate modes
				if (getTied(i,s)) {
					attributes[i][s] = ATT_MSK_TIED;// clear other attributes if tied
					applyTiedStep(i, s, lengths[i]);
				}
			}
		}
		initRun(true);
	}
	
	
	void initRun(bool hard) {// run button activated or run edge in run input jack
		if (hard) {
			phraseIndexRun = (runModeSong == MODE_REV ? phrases - 1 : 0);
			phraseIndexRunHistory = 0;
		}
		int seq = (isEditingSequence() ? sequence : phrase[phraseIndexRun]);
		if (hard) {
			stepIndexRun[0] = (runModeSeq[seq] == MODE_REV ? lengths[seq] - 1 : 0);
			fillStepIndexRunVector(runModeSeq[seq], lengths[seq]);
			stepIndexRunHistory = 0;
		}
		ppqnCount = 0;
		for (int i = 0; i < 2; i += stepConfig) {
			gate1Code[i] = calcGate1Code(attributes[seq][(i * 16) + stepIndexRun[i]], 0, pulsesPerStep, params[GATE1_KNOB_PARAM].value);
			gate2Code[i] = calcGate2Code(attributes[seq][(i * 16) + stepIndexRun[i]], 0, pulsesPerStep);
		}
		slideStepsRemain[0] = 0ul;
		slideStepsRemain[1] = 0ul;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
	}	

	
	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// expansion
		json_object_set_new(rootJ, "expansion", json_integer(expansion));

		// autoseq
		json_object_set_new(rootJ, "autoseq", json_boolean(autoseq));
		
		// pulsesPerStep
		json_object_set_new(rootJ, "pulsesPerStep", json_integer(pulsesPerStep));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// runModeSeq
		json_t *runModeSeqJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(runModeSeqJ, i, json_integer(runModeSeq[i]));
		json_object_set_new(rootJ, "runModeSeq3", runModeSeqJ);

		// runModeSong
		json_object_set_new(rootJ, "runModeSong3", json_integer(runModeSong));

		// sequence
		json_object_set_new(rootJ, "sequence", json_integer(sequence));

		// lengths
		json_t *lengthsJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(lengthsJ, i, json_integer(lengths[i]));
		json_object_set_new(rootJ, "lengths", lengthsJ);

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase", phraseJ);

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// CV
		json_t *cvJ = json_array();
		for (int i = 0; i < 32; i++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(cvJ, s + (i * 32), json_real(cv[i][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// attributes
		json_t *attributesJ = json_array();
		for (int i = 0; i < 32; i++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(attributesJ, s + (i * 32), json_integer(attributes[i][s]));
			}
		json_object_set_new(rootJ, "attributes", attributesJ);

		// attached
		json_object_set_new(rootJ, "attached", json_boolean(attached));

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));
	
		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

		// transposeOffsets
		json_t *transposeOffsetsJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(transposeOffsetsJ, i, json_integer(transposeOffsets[i]));
		json_object_set_new(rootJ, "transposeOffsets", transposeOffsetsJ);

		return rootJ;
	}

	
	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// expansion
		json_t *expansionJ = json_object_get(rootJ, "expansion");
		if (expansionJ)
			expansion = json_integer_value(expansionJ);

		// autoseq
		json_t *autoseqJ = json_object_get(rootJ, "autoseq");
		if (autoseqJ)
			autoseq = json_is_true(autoseqJ);

		// pulsesPerStep
		json_t *pulsesPerStepJ = json_object_get(rootJ, "pulsesPerStep");
		if (pulsesPerStepJ)
			pulsesPerStep = json_integer_value(pulsesPerStepJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// runModeSeq
		json_t *runModeSeqJ = json_object_get(rootJ, "runModeSeq3");
		if (runModeSeqJ) {
			for (int i = 0; i < 32; i++)
			{
				json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
				if (runModeSeqArrayJ)
					runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
			}			
		}		
		else {// legacy
			runModeSeqJ = json_object_get(rootJ, "runModeSeq2");
			if (runModeSeqJ) {
				for (int i = 0; i < 16; i++)// bug, should be 32 but keep since legacy patches were written with 16
				{
					json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
					if (runModeSeqArrayJ) {
						runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
						if (runModeSeq[i] >= MODE_PEN)// this mode was not present in version runModeSeq2
							runModeSeq[i]++;
					}
				}			
			}			
		}
		
		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, "runModeSong3");
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
		else {// legacy
			json_t *runModeSongJ = json_object_get(rootJ, "runModeSong");
			if (runModeSongJ) {
				runModeSong = json_integer_value(runModeSongJ);
				if (runModeSong >= MODE_PEN)// this mode was not present in original version
					runModeSong++;
			}
		}
		
		// sequence
		json_t *sequenceJ = json_object_get(rootJ, "sequence");
		if (sequenceJ)
			sequence = json_integer_value(sequenceJ);
		
		// lengths
		json_t *lengthsJ = json_object_get(rootJ, "lengths");
		if (lengthsJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
				if (lengthsArrayJ)
					lengthsBuffer[i] = json_integer_value(lengthsArrayJ);
			}
			
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase");
		if (phraseJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int i = 0; i < 32; i++)
				for (int s = 0; s < 32; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (i * 32));
					if (cvArrayJ)
						cv[i][s] = json_number_value(cvArrayJ);
				}
		}
		
		// attributes
		json_t *attributesJ = json_object_get(rootJ, "attributes");
		if (attributesJ) {
			for (int i = 0; i < 32; i++)
				for (int s = 0; s < 32; s++) {
					json_t *attributesArrayJ = json_array_get(attributesJ, s + (i * 32));
					if (attributesArrayJ)
						attributes[i][s] = json_integer_value(attributesArrayJ);
				}
		}
		
		// attached
		json_t *attachedJ = json_object_get(rootJ, "attached");
		if (attachedJ)
			attached = json_is_true(attachedJ);
		
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		// transposeOffsets
		json_t *transposeOffsetsJ = json_object_get(rootJ, "transposeOffsets");
		if (transposeOffsetsJ) {
			for (int i = 0; i < 32; i++)
			{
				json_t *transposeOffsetsArrayJ = json_array_get(transposeOffsetsJ, i);
				if (transposeOffsetsArrayJ)
					transposeOffsets[i] = json_integer_value(transposeOffsetsArrayJ);
			}			
		}

		stepConfigSync = 1;// signal a sync from fromJson so that step will get lengths from lengthsBuffer
	}

	void rotateSeq(int seqNum, bool directionRight, int seqLength, bool chanB_16) {
		// set chanB_16 to false to rotate chan A in 2x16 config (length will be <= 16) or single chan in 1x32 config (length will be <= 32)
		// set chanB_16 to true  to rotate chan B in 2x16 config (length must be <= 16)
		float rotCV;
		int rotAttributes;
		int iStart = chanB_16 ? 16 : 0;
		int iEnd = iStart + seqLength - 1;
		int iRot = iStart;
		int iDelta = 1;
		if (directionRight) {
			iRot = iEnd;
			iDelta = -1;
		}
		rotCV = cv[seqNum][iRot];
		rotAttributes = attributes[seqNum][iRot];
		for ( ; ; iRot += iDelta) {
			if (iDelta == 1 && iRot >= iEnd) break;
			if (iDelta == -1 && iRot <= iStart) break;
			cv[seqNum][iRot] = cv[seqNum][iRot + iDelta];
			attributes[seqNum][iRot] = attributes[seqNum][iRot + iDelta];
		}
		cv[seqNum][iRot] = rotCV;
		attributes[seqNum][iRot] = rotAttributes;
	}
	

	void step() override {
		float sampleRate = engineGetSampleRate();
		static const float gateTime = 0.4f;// seconds
		static const float copyPasteInfoTime = 0.5f;// seconds
		static const float revertDisplayTime = 0.7f;// seconds
		static const float tiedWarningTime = 0.7f;// seconds
		static const float holdDetectTime = 2.0f;// seconds
		static const float editGateLengthTime = 3.5f;// seconds
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		// Edit mode
		bool editingSequence = isEditingSequence();// true = editing sequence, false = editing song
		
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUNCV_INPUT].value)) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running)
				initRun(resetOnRun);
			displayState = DISP_NORMAL;
		}

		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {

			// Config switch
			if (stepConfigSync != 0) {
				stepConfig = getStepConfig(params[CONFIG_PARAM].value);
				if (stepConfigSync == 1) {// sync from fromJson, so read lengths from lengthsBuffer
					for (int i = 0; i < 32; i++)
						lengths[i] = lengthsBuffer[i];
				}
				else if (stepConfigSync == 2) {// sync from a real mouse drag event on the switch itself, so init lengths
					for (int i = 0; i < 32; i++)
						lengths[i] = 16 * stepConfig;
				}
				initRun(true);			
				attachedChanB = false;
				stepConfigSync = 0;
			}
			
			// Seq CV input
			if (inputs[SEQCV_INPUT].active) {
				sequence = (int) clamp( round(inputs[SEQCV_INPUT].value * (32.0f - 1.0f) / 10.0f), 0.0f, (32.0f - 1.0f) );
			}
			
			// Mode CV input
			if (inputs[MODECV_INPUT].active) {
				if (editingSequence)
					runModeSeq[sequence] = (int) clamp( round(inputs[MODECV_INPUT].value * ((float)NUM_MODES - 1.0f) / 10.0f), 0.0f, (float)NUM_MODES - 1.0f );
			}
			
			// Attach button
			if (attachedTrigger.process(params[ATTACH_PARAM].value)) {
				attached = !attached;
				if (running && attached && editingSequence && stepConfig == 1 ) 
					attachedChanB = stepIndexEdit >= 16;
				displayState = DISP_NORMAL;			
			}
			if (running && attached) {
				if (editingSequence) {
					if (attachedChanB && stepConfig == 1)
						stepIndexEdit = stepIndexRun[1] + 16;
					else
						stepIndexEdit = stepIndexRun[0] + 0;
				}
				else
					phraseIndexEdit = phraseIndexRun;
			}
			
			// Copy button
			if (copyTrigger.process(params[COPY_PARAM].value)) {
				startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
				countCP = 32;
				if (params[CPMODE_PARAM].value > 1.5f)// all
					startCP = 0;
				else if (params[CPMODE_PARAM].value < 0.5f)// 4
					countCP = min(4, 32 - startCP);
				else// 8
					countCP = min(8, 32 - startCP);
				if (editingSequence) {
					for (int i = 0, s = startCP; i < countCP; i++, s++) {
						cvCPbuffer[i] = cv[sequence][s];
						attribOrPhraseCPbuffer[i] = attributes[sequence][s];
					}
					lengthCPbuffer = lengths[sequence];
					modeCPbuffer = runModeSeq[sequence];
				}
				else {
					for (int i = 0, p = startCP; i < countCP; i++, p++)
						attribOrPhraseCPbuffer[i] = phrase[p];
					lengthCPbuffer = -1;// so that a cross paste can be detected
				}
				infoCopyPaste = (long) (copyPasteInfoTime * sampleRate / displayRefreshStepSkips);
				displayState = DISP_NORMAL;
			}
			// Paste button
			if (pasteTrigger.process(params[PASTE_PARAM].value)) {
				infoCopyPaste = (long) (-1 * copyPasteInfoTime * sampleRate / displayRefreshStepSkips);
				startCP = 0;
				if (countCP <= 8) {
					startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
					countCP = min(countCP, 32 - startCP);
				}
				// else nothing to do for ALL

				if (editingSequence) {
					if (lengthCPbuffer >= 0) {// non-crossed paste (seq vs song)
						for (int i = 0, s = startCP; i < countCP; i++, s++) {
							cv[sequence][s] = cvCPbuffer[i];
							attributes[sequence][s] = attribOrPhraseCPbuffer[i];
						}
						if (params[CPMODE_PARAM].value > 1.5f) {// all
							lengths[sequence] = lengthCPbuffer;
							runModeSeq[sequence] = modeCPbuffer;
							transposeOffsets[sequence] = 0;
						}
					}
					else {// crossed paste to seq (seq vs song)
						if (params[CPMODE_PARAM].value > 1.5f) { // ALL (init steps)
							for (int s = 0; s < 16; s++) {
								cv[sequence][s] = 0.0f;
								initAttrib(sequence, s);
							}
							transposeOffsets[sequence] = 0;
						}
						else if (params[CPMODE_PARAM].value < 0.5f) {// 4 (randomize CVs)
							for (int s = 0; s < 32; s++)
								cv[sequence][s] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
							transposeOffsets[sequence] = 0;
						}
						else {// 8 (randomize gate 1)
							for (int s = 0; s < 32; s++)
								if ( (randomu32() & 0x1) != 0)
									toggleGate1(sequence, s);
						}
						startCP = 0;
						countCP = 32;
						infoCopyPaste *= 2l;
					}
				}
				else {
					if (lengthCPbuffer < 0) {// non-crossed paste (seq vs song)
						for (int i = 0, p = startCP; i < countCP; i++, p++)
							phrase[p] = attribOrPhraseCPbuffer[i];
					}
					else {// crossed paste to song (seq vs song)
						if (params[CPMODE_PARAM].value > 1.5f) { // ALL (init phrases)
							for (int p = 0; p < 32; p++)
								phrase[p] = 0;
						}
						else if (params[CPMODE_PARAM].value < 0.5f) {// 4 (phrases increase from 1 to 32)
							for (int p = 0; p < 32; p++)
								phrase[p] = p;						
						}
						else {// 8 (randomize phrases)
							for (int p = 0; p < 32; p++)
								phrase[p] = randomu32() % 32;
						}
						startCP = 0;
						countCP = 32;
						infoCopyPaste *= 2l;
					}					
				}
				displayState = DISP_NORMAL;
			}
			
			// Write input (must be before Left and Right in case route gate simultaneously to Right and Write for example)
			//  (write must be to correct step)
			bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].value);
			if (writeTrig) {
				if (editingSequence) {
					cv[sequence][stepIndexEdit] = inputs[CV_INPUT].value;
					applyTiedStep(sequence, stepIndexEdit, ((stepIndexEdit >= 16 && stepConfig == 1) ? 16 : 0) + lengths[sequence]);
					editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
					editingGateCV = cv[sequence][stepIndexEdit];
					editingGateKeyLight = -1;
					editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
					// Autostep (after grab all active inputs)
					if (params[AUTOSTEP_PARAM].value > 0.5f) {
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
						if (stepIndexEdit == 0 && autoseq)
							sequence = moveIndex(sequence, sequence + 1, 32);
					}
				}
				displayState = DISP_NORMAL;
			}
			// Left and right CV inputs
			int delta = 0;
			if (leftTrigger.process(inputs[LEFTCV_INPUT].value)) { 
				delta = -1;
				if (displayState != DISP_LENGTH)
					displayState = DISP_NORMAL;
			}
			if (rightTrigger.process(inputs[RIGHTCV_INPUT].value)) {
				delta = +1;
				if (displayState != DISP_LENGTH)
					displayState = DISP_NORMAL;
			}
			if (delta != 0) {
				if (displayState == DISP_LENGTH) {
					if (editingSequence) {
						lengths[sequence] += delta;
						if (lengths[sequence] > (16 * stepConfig)) lengths[sequence] = (16 * stepConfig);
						if (lengths[sequence] < 1 ) lengths[sequence] = 1;
						lengths[sequence] = ((lengths[sequence] - 1) % (16 * stepConfig)) + 1;
					}
					else {
						phrases += delta;
						if (phrases > 32) phrases = 32;
						if (phrases < 1 ) phrases = 1;
					}
				}
				else {
					if (!running || !attached) {// don't move heads when attach and running
						if (editingSequence) {
							stepIndexEdit += delta;
							if (stepIndexEdit < 0)
								stepIndexEdit = ((stepConfig == 1) ? 16 : 0) + lengths[sequence] - 1;
							if (stepIndexEdit >= 32)
								stepIndexEdit = 0;
							if (!getTied(sequence,stepIndexEdit)) {// play if non-tied step
								if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
									editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
									editingGateCV = cv[sequence][stepIndexEdit];
									editingGateKeyLight = -1;
									editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
								}
							}
						}
						else {
							phraseIndexEdit = moveIndex(phraseIndexEdit, phraseIndexEdit + delta, 32);
							if (!running)
								phraseIndexRun = phraseIndexEdit;	
						}						
					}
				}
			}

			// Step button presses
			int stepPressed = -1;
			for (int i = 0; i < 32; i++) {
				if (stepTriggers[i].process(params[STEP_PHRASE_PARAMS + i].value))
					stepPressed = i;
			}
			if (stepPressed != -1) {
				if (displayState == DISP_LENGTH) {
					if (editingSequence)
						lengths[sequence] = (stepPressed % (16 * stepConfig)) + 1;
					else
						phrases = stepPressed + 1;
					revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
				}
				else {
					if (!running || !attached) {// not running or detached
						if (editingSequence) {
							stepIndexEdit = stepPressed;
							if (!getTied(sequence,stepIndexEdit)) {// play if non-tied step
								editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
								editingGateCV = cv[sequence][stepIndexEdit];
								editingGateKeyLight = -1;
								editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
							}
						}
						else {
							phraseIndexEdit = stepPressed;
							if (!running)
								phraseIndexRun = stepPressed;
						}
					}
					else {// attached and running
						if (editingSequence) {
							if ((stepPressed < 16) && attachedChanB)
								attachedChanB = false;
							if ((stepPressed >= 16) && !attachedChanB)
								attachedChanB = true;					
						}
					}
					displayState = DISP_NORMAL;
				}
			} 
			
			// Mode/Length button
			if (modeTrigger.process(params[RUNMODE_PARAM].value)) {
				if (editingPpqn != 0l)
					editingPpqn = 0l;			
				if (displayState == DISP_NORMAL || displayState == DISP_TRANSPOSE || displayState == DISP_ROTATE)
					displayState = DISP_LENGTH;
				else if (displayState == DISP_LENGTH)
					displayState = DISP_MODE;
				else
					displayState = DISP_NORMAL;
				modeHoldDetect.start((long) (holdDetectTime * sampleRate / displayRefreshStepSkips));
			}
			
			// Transpose/Rotate button
			if (transposeTrigger.process(params[TRAN_ROT_PARAM].value)) {
				if (editingSequence) {
					if (displayState == DISP_NORMAL || displayState == DISP_MODE || displayState == DISP_LENGTH) {
						displayState = DISP_TRANSPOSE;
						//transposeOffset = 0;
					}
					else if (displayState == DISP_TRANSPOSE) {
						displayState = DISP_ROTATE;
						rotateOffset = 0;
					}
					else 
						displayState = DISP_NORMAL;
				}
			}			
			
			// Sequence knob 
			float seqParamValue = params[SEQUENCE_PARAM].value;
			int newSequenceKnob = (int)roundf(seqParamValue * 7.0f);
			if (seqParamValue == 0.0f)// true when constructor or fromJson() occured
				sequenceKnob = newSequenceKnob;
			int deltaKnob = newSequenceKnob - sequenceKnob;
			if (deltaKnob != 0) {
				if (abs(deltaKnob) <= 3) {// avoid discontinuous step (initialize for example)
					if (editingPpqn != 0) {
						pulsesPerStep = indexToPps(ppsToIndex(pulsesPerStep) + deltaKnob);// indexToPps() does clamping
						if (pulsesPerStep < 2)
							editingGateLength = 0l;
						editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODE) {
						if (editingSequence) {
							if (!inputs[MODECV_INPUT].active) {
								runModeSeq[sequence] += deltaKnob;
								if (runModeSeq[sequence] < 0) runModeSeq[sequence] = 0;
								if (runModeSeq[sequence] >= NUM_MODES) runModeSeq[sequence] = NUM_MODES - 1;
							}
						}
						else {
							runModeSong += deltaKnob;
							if (runModeSong < 0) runModeSong = 0;
							if (runModeSong >= 6) runModeSong = 6 - 1;
						}
					}
					else if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							lengths[sequence] += deltaKnob;
							if (lengths[sequence] > (16 * stepConfig)) lengths[sequence] = (16 * stepConfig);
							if (lengths[sequence] < 1 ) lengths[sequence] = 1;
						}
						else {
							phrases += deltaKnob;
							if (phrases > 32) phrases = 32;
							if (phrases < 1 ) phrases = 1;
						}
					}
					else if (displayState == DISP_TRANSPOSE) {
						if (editingSequence) {
							transposeOffsets[sequence] += deltaKnob;
							if (transposeOffsets[sequence] > 99) transposeOffsets[sequence] = 99;
							if (transposeOffsets[sequence] < -99) transposeOffsets[sequence] = -99;						
							// Tranpose by this number of semi-tones: deltaKnob
							float transposeOffsetCV = ((float)(deltaKnob))/12.0f;
							if (stepConfig == 1){ // 2x16 (transpose only the 16 steps corresponding to row where stepIndexEdit is located)
								int offset = stepIndexEdit < 16 ? 0 : 16;
								for (int s = offset; s < offset + 16; s++) 
									cv[sequence][s] += transposeOffsetCV;
							}
							else { // 1x32 (transpose all 32 steps)
								for (int s = 0; s < 32; s++) 
									cv[sequence][s] += transposeOffsetCV;
							}
						}
					}
					else if (displayState == DISP_ROTATE) {
						if (editingSequence) {
							rotateOffset += deltaKnob;
							if (rotateOffset > 99) rotateOffset = 99;
							if (rotateOffset < -99) rotateOffset = -99;	
							if (deltaKnob > 0 && deltaKnob < 99) {// Rotate right, 99 is safety
								for (int i = deltaKnob; i > 0; i--)
									rotateSeq(sequence, true, lengths[sequence], stepConfig == 1 && stepIndexEdit >= 16);
							}
							if (deltaKnob < 0 && deltaKnob > -99) {// Rotate left, 99 is safety
								for (int i = deltaKnob; i < 0; i++)
									rotateSeq(sequence, false, lengths[sequence], stepConfig == 1 && stepIndexEdit >= 16);
							}
						}						
					}
					else {// DISP_NORMAL
						if (editingSequence) {
							if (!inputs[SEQCV_INPUT].active) {
								sequence += deltaKnob;
								if (sequence < 0) sequence = 0;
								if (sequence >= 32) sequence = (32 - 1);
							}
						}
						else {
							int newPhrase = phrase[phraseIndexEdit] + deltaKnob;
							if (newPhrase < 0)
								newPhrase += (1 - newPhrase / 32) * 32;// newPhrase now positive
							newPhrase = newPhrase % 32;
							phrase[phraseIndexEdit] = newPhrase;
						}
					}
				}
				sequenceKnob = newSequenceKnob;
			}	
			
			// Octave buttons
			int newOct = -1;
			for (int i = 0; i < 7; i++) {
				if (octTriggers[i].process(params[OCTAVE_PARAM + i].value)) {
					newOct = 6 - i;
					displayState = DISP_NORMAL;
				}
			}
			if (newOct >= 0 && newOct <= 6) {
				if (editingSequence) {
					if (getTied(sequence,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else {			
						float newCV = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
						newCV = newCV - floor(newCV) + (float) (newOct - 3);
						if (newCV >= -3.0f && newCV < 4.0f) {
							cv[sequence][stepIndexEdit] = newCV;
							applyTiedStep(sequence, stepIndexEdit, ((stepIndexEdit >= 16 && stepConfig == 1) ? 16 : 0) + lengths[sequence]);
						}
						editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
						editingGateCV = cv[sequence][stepIndexEdit];
						editingGateKeyLight = -1;
						editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
					}
				}
			}		
			
			// Keyboard buttons
			for (int i = 0; i < 12; i++) {
				if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
					if (editingSequence) {
						if (editingGateLength != 0l) {
							int newMode = keyIndexToGateMode(i, pulsesPerStep);
							if (editingGateLength > 0l) {
								if (newMode != -1)
									setGate1Mode(sequence, stepIndexEdit, newMode);
								else
									editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
							}
							else {
								if (newMode != -1)
									setGate2Mode(sequence, stepIndexEdit, newMode);
								else
									editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
							}
						}
						else if (getTied(sequence,stepIndexEdit)) {
							if (params[KEY_PARAMS + i].value > 1.5f)
								stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
							else
								tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
						}
						else {			
							cv[sequence][stepIndexEdit] = floor(cv[sequence][stepIndexEdit]) + ((float) i) / 12.0f;
							applyTiedStep(sequence, stepIndexEdit, ((stepIndexEdit >= 16 && stepConfig == 1) ? 16 : 0) + lengths[sequence]);
							editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							editingGateCV = cv[sequence][stepIndexEdit];
							editingGateKeyLight = -1;
							editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
							if (params[KEY_PARAMS + i].value > 1.5f) {
								stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
								editingGateKeyLight = i;
							}
						}						
					}
					displayState = DISP_NORMAL;
				}
			}
			
			// Keyboard mode (note or gate type)
			if (keyNoteTrigger.process(params[KEYNOTE_PARAM].value)) {
				editingGateLength = 0l;
			}
			if (keyGateTrigger.process(params[KEYGATE_PARAM].value)) {
				if (pulsesPerStep < 2) {
					editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
				}
				else {
					if (editingGateLength == 0l) {
						editingGateLength = lastGateEdit;
					}
					else {
						editingGateLength *= -1l;
						lastGateEdit = editingGateLength;
					}
				}
			}

			// Gate1, Gate1Prob, Gate2, Slide and Tied buttons
			if (gate1Trigger.process(params[GATE1_PARAM].value + inputs[GATE1CV_INPUT].value)) {
				if (editingSequence) {
					toggleGate1a(&attributes[sequence][stepIndexEdit]);
				}
				displayState = DISP_NORMAL;
			}		
			if (gate1ProbTrigger.process(params[GATE1_PROB_PARAM].value)) {
				if (editingSequence) {
					if (getTied(sequence,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else
						toggleGate1Pa(&attributes[sequence][stepIndexEdit]);
				}
				displayState = DISP_NORMAL;
			}		
			if (gate2Trigger.process(params[GATE2_PARAM].value + inputs[GATE2CV_INPUT].value)) {
				if (editingSequence) {
					toggleGate2a(&attributes[sequence][stepIndexEdit]);
				}
				displayState = DISP_NORMAL;
			}		
			if (slideTrigger.process(params[SLIDE_BTN_PARAM].value + inputs[SLIDECV_INPUT].value)) {
				if (editingSequence) {
					if (getTied(sequence,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else
						toggleSlideA(&attributes[sequence][stepIndexEdit]);
				}
				displayState = DISP_NORMAL;
			}		
			if (tiedTrigger.process(params[TIE_PARAM].value + inputs[TIEDCV_INPUT].value)) {
				if (editingSequence) {
					toggleTiedA(&attributes[sequence][stepIndexEdit]);
					if (getTied(sequence,stepIndexEdit)) {
						setGate1a(&attributes[sequence][stepIndexEdit], false);
						setGate1Pa(&attributes[sequence][stepIndexEdit], false);
						setGate2a(&attributes[sequence][stepIndexEdit], false);
						setSlideA(&attributes[sequence][stepIndexEdit], false);
						applyTiedStep(sequence, stepIndexEdit, ((stepIndexEdit >= 16 && stepConfig == 1) ? 16 : 0) + lengths[sequence]);
					}
				}
				displayState = DISP_NORMAL;
			}		
			
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if (running && clockIgnoreOnReset == 0l) {
				ppqnCount++;
				if (ppqnCount >= pulsesPerStep)
					ppqnCount = 0;

				int newSeq = sequence;// good value when editingSequence, overwrite if not editingSequence
				if (ppqnCount == 0) {
					float slideFromCV[2] = {0.0f, 0.0f};
					if (editingSequence) {
						for (int i = 0; i < 2; i += stepConfig)
							slideFromCV[i] = cv[sequence][(i * 16) + stepIndexRun[i]];
						moveIndexRunMode(&stepIndexRun[0], lengths[sequence], runModeSeq[sequence], &stepIndexRunHistory);
					}
					else {
						for (int i = 0; i < 2; i += stepConfig)
							slideFromCV[i] = cv[phrase[phraseIndexRun]][(i * 16) + stepIndexRun[i]];
						if (moveIndexRunMode(&stepIndexRun[0], lengths[phrase[phraseIndexRun]], runModeSeq[phrase[phraseIndexRun]], &stepIndexRunHistory)) {
							moveIndexRunMode(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
							stepIndexRun[0] = (runModeSeq[phrase[phraseIndexRun]] == MODE_REV ? lengths[phrase[phraseIndexRun]] - 1 : 0);// must always refresh after phraseIndexRun has changed
						}
						newSeq = phrase[phraseIndexRun];
					}
					fillStepIndexRunVector(runModeSeq[newSeq], lengths[newSeq]);

					// Slide
					for (int i = 0; i < 2; i += stepConfig) {
						if (getSlide(newSeq, (i * 16) + stepIndexRun[i])) {
							slideStepsRemain[i] =   (unsigned long) (((float)clockPeriod  * pulsesPerStep) * params[SLIDE_KNOB_PARAM].value / 2.0f);
							float slideToCV = cv[newSeq][(i * 16) + stepIndexRun[i]];
							slideCVdelta[i] = (slideToCV - slideFromCV[i])/(float)slideStepsRemain[i];
						}
					}
				}
				else {
					if (!editingSequence)
						newSeq = phrase[phraseIndexRun];
				}
				for (int i = 0; i < 2; i += stepConfig) {
					if (gate1Code[i] != -1 || ppqnCount == 0)
						gate1Code[i] = calcGate1Code(attributes[newSeq][(i * 16) + stepIndexRun[i]], ppqnCount, pulsesPerStep, params[GATE1_KNOB_PARAM].value);
					gate2Code[i] = calcGate2Code(attributes[newSeq][(i * 16) + stepIndexRun[i]], ppqnCount, pulsesPerStep);	
				}
			}
			clockPeriod = 0ul;
		}
		clockPeriod++;
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			initRun(true);// must be after sequence reset
			resetLight = 1.0f;
			displayState = DISP_NORMAL;
		}
		
		
		//********** Outputs and lights **********
				
		// CV and gates outputs
		int seq = editingSequence ? (sequence) : (running ? phrase[phraseIndexRun] : phrase[phraseIndexEdit]);
		int step0 = editingSequence ? (running ? stepIndexRun[0] : stepIndexEdit) : (stepIndexRun[0]);
		if (running) {
			bool muteGate1A = !editingSequence && ((params[GATE1_PARAM].value + inputs[GATE1CV_INPUT].value) > 0.5f);// live mute
			bool muteGate1B = muteGate1A;
			bool muteGate2A = !editingSequence && ((params[GATE2_PARAM].value + inputs[GATE2CV_INPUT].value) > 0.5f);// live mute
			bool muteGate2B = muteGate2A;
			if (!attached && (muteGate1B || muteGate2B) && stepConfig == 1) {
				// if not attached in 2x16, mute only the channel where phraseIndexEdit is located (hack since phraseIndexEdit's row has no relation to channels)
				if (phraseIndexEdit < 16) {
					muteGate1B = false;
					muteGate2B = false;
				}
				else {
					muteGate1A = false;
					muteGate2A = false;
				}
			}
			float slideOffset[2];
			for (int i = 0; i < 2; i += stepConfig)
				slideOffset[i] = (slideStepsRemain[i] > 0ul ? (slideCVdelta[i] * (float)slideStepsRemain[i]) : 0.0f);
			outputs[CVA_OUTPUT].value = cv[seq][step0] - slideOffset[0];
			outputs[GATE1A_OUTPUT].value = (calcGate(gate1Code[0], clockTrigger, clockPeriod, sampleRate) && !muteGate1A) ? 10.0f : 0.0f;
			outputs[GATE2A_OUTPUT].value = (calcGate(gate2Code[0], clockTrigger, clockPeriod, sampleRate) && !muteGate2A) ? 10.0f : 0.0f;
			if (stepConfig == 1) {
				int step1 = editingSequence ? (running ? stepIndexRun[1] : stepIndexEdit) : (stepIndexRun[1]);
				outputs[CVB_OUTPUT].value = cv[seq][16 + step1] - slideOffset[1];
				outputs[GATE1B_OUTPUT].value = (calcGate(gate1Code[1], clockTrigger, clockPeriod, sampleRate) && !muteGate1B) ? 10.0f : 0.0f;
				outputs[GATE2B_OUTPUT].value = (calcGate(gate2Code[1], clockTrigger, clockPeriod, sampleRate) && !muteGate2B) ? 10.0f : 0.0f;
			} 
			else {
				outputs[CVB_OUTPUT].value = 0.0f;
				outputs[GATE1B_OUTPUT].value = 0.0f;
				outputs[GATE2B_OUTPUT].value = 0.0f;
			}
		}
		else {// not running 
			if (editingChannel == 0) {
				outputs[CVA_OUTPUT].value = (editingGate > 0ul) ? editingGateCV : cv[seq][step0];
				outputs[GATE1A_OUTPUT].value = (editingGate > 0ul) ? 10.0f : 0.0f;
				outputs[GATE2A_OUTPUT].value = (editingGate > 0ul) ? 10.0f : 0.0f;
				outputs[CVB_OUTPUT].value = 0.0f;
				outputs[GATE1B_OUTPUT].value = 0.0f;
				outputs[GATE2B_OUTPUT].value = 0.0f;
			}
			else {
				outputs[CVA_OUTPUT].value = 0.0f;
				outputs[GATE1A_OUTPUT].value = 0.0f;
				outputs[GATE2A_OUTPUT].value = 0.0f;
				outputs[CVB_OUTPUT].value = (editingGate > 0ul) ? editingGateCV : cv[seq][step0];
				outputs[GATE1B_OUTPUT].value = (editingGate > 0ul) ? 10.0f : 0.0f;
				outputs[GATE2B_OUTPUT].value = (editingGate > 0ul) ? 10.0f : 0.0f;
			}
		}
		for (int i = 0; i < 2; i++)
			if (slideStepsRemain[i] > 0ul)
				slideStepsRemain[i]--;

		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;
		
			// Step/phrase lights
			if (infoCopyPaste != 0l) {
				for (int i = 0; i < 32; i++) {
					if (i >= startCP && i < (startCP + countCP))
						lights[STEP_PHRASE_LIGHTS + (i<<1)].value = 0.5f;// Green when copy interval
					else
						lights[STEP_PHRASE_LIGHTS + (i<<1)].value = 0.0f; // Green (nothing)
					lights[STEP_PHRASE_LIGHTS + (i<<1) + 1].value = 0.0f;// Red (nothing)
				}
			}
			else {
				for (int i = 0; i < 32; i++) {
					int col = (stepConfig == 1 ? (i & 0xF) : i);//i % (16 * stepConfig);// optimized
					if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							if (col < (lengths[sequence] - 1))
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 0.1f, 0.0f);
							else if (col == (lengths[sequence] - 1))
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 1.0f, 0.0f);
							else 
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 0.0f, 0.0f);
						}
						else {
							if (i < phrases - 1)
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 0.1f, 0.0f);
							else
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, (i == phrases - 1) ? 1.0f : 0.0f, 0.0f);
						}
					}
					else {// normal led display (i.e. not length)
						float red = 0.0f;
						float green = 0.0f;
						int row = i >> (3 + stepConfig);//i / (16 * stepConfig);// optimized (not equivalent code, but in this case has same effect)
						// Run cursor (green)
						if (editingSequence)
							green = ((running && (col == stepIndexRun[row])) ? 1.0f : 0.0f);
						else {
							green = ((running && (i == phraseIndexRun)) ? 1.0f : 0.0f);
							green += ((running && (col == stepIndexRun[row]) && i != phraseIndexEdit) ? 0.1f : 0.0f);
							green = clamp(green, 0.0f, 1.0f);
						}
						// Edit cursor (red)
						if (editingSequence)
							red = (i == stepIndexEdit ? 1.0f : 0.0f);
						else
							red = (i == phraseIndexEdit ? 1.0f : 0.0f);
						
						setGreenRed(STEP_PHRASE_LIGHTS + i * 2, green, red);
					}
				}
			}
		
			// Octave lights
			float octCV = 0.0f;
			if (editingSequence)
				octCV = cv[sequence][stepIndexEdit];
			else
				octCV = cv[phrase[phraseIndexEdit]][stepIndexRun[0]];
			int octLightIndex = (int) floor(octCV + 3.0f);
			for (int i = 0; i < 7; i++) {
				if (!editingSequence && (!attached || !running || (stepConfig == 1)))// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
												// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
												// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
												// [3] makes no sense, which sequence would be displayed, top or bottom row!
					lights[OCTAVE_LIGHTS + i].value = 0.0f;
				else {
					if (tiedWarning > 0l) {
						bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
						lights[OCTAVE_LIGHTS + i].value = (warningFlashState && (i == (6 - octLightIndex))) ? 1.0f : 0.0f;
					}
					else				
						lights[OCTAVE_LIGHTS + i].value = (i == (6 - octLightIndex) ? 1.0f : 0.0f);
				}
			}
			
			// Keyboard lights (can only show channel A when running attached in 1x16 mode, does not pose problem for all other situations)
			float cvValOffset;
			if (editingSequence) 
				cvValOffset = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
			else	
				cvValOffset = cv[phrase[phraseIndexEdit]][stepIndexRun[0]] + 10.0f;//to properly handle negative note voltages
			int keyLightIndex = (int) clamp(  roundf( (cvValOffset-floor(cvValOffset)) * 12.0f ),  0.0f,  11.0f);
			if (editingGateLength != 0l && editingSequence) {
				int modeLightIndex = gateModeToKeyLightIndex(attributes[sequence][stepIndexEdit], editingGateLength > 0l);
				for (int i = 0; i < 12; i++) {
					if (i == modeLightIndex) {
						lights[KEY_LIGHTS + i * 2 + 0].value = editingGateLength > 0l ? 1.0f : 0.2f;
						lights[KEY_LIGHTS + i * 2 + 1].value = editingGateLength > 0l ? 0.2f : 1.0f;
					}
					else { 
						lights[KEY_LIGHTS + i * 2 + 0].value = 0.0f;
						if (i == keyLightIndex) 
							lights[KEY_LIGHTS + i * 2 + 1].value = 0.1f;	
						else 
							lights[KEY_LIGHTS + i * 2 + 1].value = 0.0f;
					}
				}
			}
			else {
				for (int i = 0; i < 12; i++) {
					lights[KEY_LIGHTS + i * 2 + 0].value = 0.0f;
					if (!editingSequence && (!attached || !running || (stepConfig == 1)))// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
													// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
													// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
													// [3] makes no sense, which sequence would be displayed, top or bottom row!
						lights[KEY_LIGHTS + i * 2 + 1].value = 0.0f;
					else {
						if (tiedWarning > 0l) {
							bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
							lights[KEY_LIGHTS + i * 2 + 1].value = (warningFlashState && i == keyLightIndex) ? 1.0f : 0.0f;
						}
						else {
							if (editingGate > 0ul && editingGateKeyLight != -1)
								lights[KEY_LIGHTS + i * 2 + 1].value = (i == editingGateKeyLight ? ((float) editingGate / (float)(gateTime * sampleRate / displayRefreshStepSkips)) : 0.0f);
							else
								lights[KEY_LIGHTS + i * 2 + 1].value = (i == keyLightIndex ? 1.0f : 0.0f);
						}
					}
				}
			}		

			// Key mode light (note or gate type)
			lights[KEYNOTE_LIGHT].value = editingGateLength == 0l ? 10.0f : 0.0f;
			if (editingGateLength == 0l)
				setGreenRed(KEYGATE_LIGHT, 0.0f, 0.0f);
			else if (editingGateLength > 0l)
				setGreenRed(KEYGATE_LIGHT, 1.0f, 0.2f);
			else
				setGreenRed(KEYGATE_LIGHT, 0.2f, 1.0f);
			
			// Res light
			long editingPpqnInit = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
			if ( ((editingPpqn > 0l) && (editingPpqn < (editingPpqnInit / 6l))) ||
				 ((editingPpqn > (editingPpqnInit * 2l / 6l)) && (editingPpqn < (editingPpqnInit * 3l / 6l))) ||
				 ((editingPpqn > (editingPpqnInit * 4l / 6l)) && (editingPpqn < (editingPpqnInit * 5l / 6l))) )
				lights[RES_LIGHT].value = 1.0f;
			else 
				lights[RES_LIGHT].value = 0.0f;

			// Gate1, Gate1Prob, Gate2, Slide and Tied lights (can only show channel A when running attached in 1x32 mode, does not pose problem for all other situations)
			if (!editingSequence && (!attached || !running || (stepConfig == 1))) {// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
											// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
											// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
											// [3] makes no sense, which sequence would be displayed, top or bottom row!
				setGateLight(false, GATE1_LIGHT);
				setGateLight(false, GATE2_LIGHT);
				lights[GATE1_PROB_LIGHT].value = 0.0f;
				lights[SLIDE_LIGHT].value = 0.0f;
				lights[TIE_LIGHT].value = 0.0f;
			}
			else {
				int attributesVal = attributes[sequence][stepIndexEdit];
				if (!editingSequence)
					attributesVal = attributes[phrase[phraseIndexEdit]][stepIndexRun[0]];
				//
				setGateLight(getGate1a(attributesVal), GATE1_LIGHT);
				setGateLight(getGate2a(attributesVal), GATE2_LIGHT);
				lights[GATE1_PROB_LIGHT].value = getGate1Pa(attributesVal) ? 1.0f : 0.0f;
				lights[SLIDE_LIGHT].value = getSlideA(attributesVal) ? 1.0f : 0.0f;
				if (tiedWarning > 0l) {
					bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
					lights[TIE_LIGHT].value = (warningFlashState) ? 1.0f : 0.0f;
				}
				else
					lights[TIE_LIGHT].value = getTiedA(attributesVal) ? 1.0f : 0.0f;
			}
			
			// Attach light
			lights[ATTACH_LIGHT].value = (attached ? 1.0f : 0.0f);
			
			// Reset light
			lights[RESET_LIGHT].value =	resetLight;
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime() * displayRefreshStepSkips;
			
			// Run light
			lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;

			if (editingGate > 0ul)
				editingGate--;
			if (infoCopyPaste != 0l) {
				if (infoCopyPaste > 0l)
					infoCopyPaste --;
				if (infoCopyPaste < 0l)
					infoCopyPaste ++;
			}
			if (editingPpqn > 0l)
				editingPpqn--;
			if (tiedWarning > 0l)
				tiedWarning--;
			if (modeHoldDetect.process(params[RUNMODE_PARAM].value)) {
				displayState = DISP_NORMAL;
				editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
			}
			if (revertDisplay > 0l) {
				if (revertDisplay == 1)
					displayState = DISP_NORMAL;
				revertDisplay--;
			}
		}// lightRefreshCounter
				
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// step()
	

	inline void setGreenRed(int id, float green, float red) {
		lights[id + 0].value = green;
		lights[id + 1].value = red;
	}

	void applyTiedStep(int seqNum, int indexTied, int seqLength) {
		// Start on indexTied and loop until seqLength
		// Called because either:
		//   case A: tied was activated for given step
		//   case B: the given step's CV was modified
		// These cases are mutually exclusive
		
		// copy previous CV over to current step if tied
		if (getTied(seqNum,indexTied) && (indexTied > 0))
			cv[seqNum][indexTied] = cv[seqNum][indexTied - 1];
		
		// Affect downstream CVs of subsequent tied note chain (can be 0 length if next note is not tied)
		for (int i = indexTied + 1; i < seqLength && getTied(seqNum,i); i++) 
			cv[seqNum][i] = cv[seqNum][indexTied];
	}
	
	inline void setGateLight(bool gateOn, int lightIndex) {
		if (!gateOn) {
			lights[lightIndex + 0].value = 0.0f;
			lights[lightIndex + 1].value = 0.0f;
		}
		else if (pulsesPerStep == 1) {
			lights[lightIndex + 0].value = 0.0f;
			lights[lightIndex + 1].value = 1.0f;
		}
		else {
			lights[lightIndex + 0].value = lightIndex == GATE1_LIGHT ? 1.0f : 0.2f;
			lights[lightIndex + 1].value = lightIndex == GATE1_LIGHT ? 0.2f : 1.0f;
		}
	}

};



struct PhraseSeq32Widget : ModuleWidget {
	PhraseSeq32 *module;
	DynamicSVGPanel *panel;
	int oldExpansion;
	int expWidth = 60;
	IMPort* expPorts[5];
	
	struct SequenceDisplayWidget : TransparentWidget {
		PhraseSeq32 *module;
		std::shared_ptr<Font> font;
		char displayStr[4];
		
		SequenceDisplayWidget() {
			font = Font::load(assetPlugin(plugin, "res/fonts/Segment14.ttf"));
		}
		
		void runModeToStr(int num) {
			if (num >= 0 && num < NUM_MODES)
				snprintf(displayStr, 4, "%s", modeLabels[num].c_str());
		}

		void draw(NVGcontext *vg) override {
			NVGcolor textColor = prepareDisplay(vg, &box, 18);
			nvgFontFaceId(vg, font->handle);
			bool editingSequence = module->isEditingSequence();

			Vec textPos = Vec(6, 24);
			nvgFillColor(vg, nvgTransRGBA(textColor, 16));
			nvgText(vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(vg, textColor);
			if (module->infoCopyPaste != 0l) {
				if (module->infoCopyPaste > 0l)
					snprintf(displayStr, 4, "CPY");
				else {
					int lenCP = module->lengthCPbuffer;
					float cpMode = module->params[PhraseSeq32::CPMODE_PARAM].value;
					if (editingSequence && lenCP == -1) {// cross paste to seq
						if (cpMode > 1.5f)// All = init
							snprintf(displayStr, 4, "CLR");
						else if (cpMode < 0.5f)// 4 = random CV
							snprintf(displayStr, 4, "RCV");
						else// 8 = random gate 1
							snprintf(displayStr, 4, "RG1");
					}
					else if (!editingSequence && lenCP != -1) {// cross paste to song
						if (cpMode > 1.5f)// All = init
							snprintf(displayStr, 4, "CLR");
						else if (cpMode < 0.5f)// 4 = increase by 1
							snprintf(displayStr, 4, "INC");
						else// 8 = random phrases
							snprintf(displayStr, 4, "RPH");
					}
					else
						snprintf(displayStr, 4, "PST");
				}
			}
			else if (module->editingPpqn != 0ul) {
				snprintf(displayStr, 4, "x%2u", (unsigned) module->pulsesPerStep);
			}
			else if (module->displayState == PhraseSeq32::DISP_MODE) {
				if (editingSequence)
					runModeToStr(module->runModeSeq[module->sequence]);
				else
					runModeToStr(module->runModeSong);
			}
			else if (module->displayState == PhraseSeq32::DISP_LENGTH) {
				if (editingSequence)
					snprintf(displayStr, 4, "L%2u", (unsigned) module->lengths[module->sequence]);
				else
					snprintf(displayStr, 4, "L%2u", (unsigned) module->phrases);
			}
			else if (module->displayState == PhraseSeq32::DISP_TRANSPOSE) {
				snprintf(displayStr, 4, "+%2u", (unsigned) abs(module->transposeOffsets[module->sequence]));
				if (module->transposeOffsets[module->sequence] < 0)
					displayStr[0] = '-';
			}
			else if (module->displayState == PhraseSeq32::DISP_ROTATE) {
				snprintf(displayStr, 4, ")%2u", (unsigned) abs(module->rotateOffset));
				if (module->rotateOffset < 0)
					displayStr[0] = '(';
			}
			else {// DISP_NORMAL
				snprintf(displayStr, 4, " %2u", (unsigned) (editingSequence ? 
					module->sequence : module->phrase[module->phraseIndexEdit]) + 1 );
			}
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	struct PanelThemeItem : MenuItem {
		PhraseSeq32 *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	struct ExpansionItem : MenuItem {
		PhraseSeq32 *module;
		void onAction(EventAction &e) override {
			module->expansion = module->expansion == 1 ? 0 : 1;
		}
	};
	struct ResetOnRunItem : MenuItem {
		PhraseSeq32 *module;
		void onAction(EventAction &e) override {
			module->resetOnRun = !module->resetOnRun;
		}
	};
	struct AutoseqItem : MenuItem {
		PhraseSeq32 *module;
		void onAction(EventAction &e) override {
			module->autoseq = !module->autoseq;
		}
	};
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		PhraseSeq32 *module = dynamic_cast<PhraseSeq32*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = lightPanelID;// ImpromptuModular.hpp
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = darkPanelID;// ImpromptuModular.hpp
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		ResetOnRunItem *rorItem = MenuItem::create<ResetOnRunItem>("Reset on Run", CHECKMARK(module->resetOnRun));
		rorItem->module = module;
		menu->addChild(rorItem);

		AutoseqItem *aseqItem = MenuItem::create<AutoseqItem>("AutoSeq when writing via CV inputs", CHECKMARK(module->autoseq));
		aseqItem->module = module;
		menu->addChild(aseqItem);

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *expansionLabel = new MenuLabel();
		expansionLabel->text = "Expansion module";
		menu->addChild(expansionLabel);

		ExpansionItem *expItem = MenuItem::create<ExpansionItem>(expansionMenuLabel, CHECKMARK(module->expansion != 0));
		expItem->module = module;
		menu->addChild(expItem);
		
		return menu;
	}	
	
	void step() override {
		if(module->expansion != oldExpansion) {
			if (oldExpansion!= -1 && module->expansion == 0) {// if just removed expansion panel, disconnect wires to those jacks
				for (int i = 0; i < 5; i++)
					gRackWidget->wireContainer->removeAllWires(expPorts[i]);
			}
			oldExpansion = module->expansion;		
		}
		box.size.x = panel->box.size.x - (1 - module->expansion) * expWidth;
		Widget::step();
	}
	
	struct CKSSNotify : CKSS {
		CKSSNotify() {};
		void onDragStart(EventDragStart &e) override {
			ToggleSwitch::onDragStart(e);
			((PhraseSeq32*)(module))->stepConfigSync = 2;// signal a sync from switch so that steps get initialized
		}	
	};
	
	PhraseSeq32Widget(PhraseSeq32 *module) : ModuleWidget(module) {
		this->module = module;
		oldExpansion = -1;
		
		// Main panel from Inkscape
        panel = new DynamicSVGPanel();
        panel->mode = &module->panelTheme;
		panel->expWidth = &expWidth;
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/PhraseSeq32.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/dark/PhraseSeq32_dark.svg")));
        box.size = panel->box.size;
		box.size.x = box.size.x - (1 - module->expansion) * expWidth;
        addChild(panel);
		
		// Screws
		addChild(createDynamicScrew<IMScrew>(Vec(15, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(15, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 365), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30-expWidth, 0), &module->panelTheme));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30-expWidth, 365), &module->panelTheme));

		
		
		// ****** Top row ******
		
		static const int rowRulerT0 = 48;
		static const int columnRulerT0 = 18;//30;// Step/Phase LED buttons
		static const int columnRulerT3 = 377;// Attach 
		static const int columnRulerT4 = 430;// Config 

		// Step/Phrase LED buttons
		int posX = columnRulerT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		for (int x = 0; x < 16; x++) {
			// First row
			addParam(createParam<LEDButton>(Vec(posX, rowRulerT0 - 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_PARAMS + x, 0.0f, 1.0f, 0.0f));
			addChild(createLight<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRulerT0 - 10 + 3), module, PhraseSeq32::STEP_PHRASE_LIGHTS + (x * 2)));
			// Second row
			addParam(createParam<LEDButton>(Vec(posX, rowRulerT0 + 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_PARAMS + x + 16, 0.0f, 1.0f, 0.0f));
			addChild(createLight<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRulerT0 + 10 + 3), module, PhraseSeq32::STEP_PHRASE_LIGHTS + ((x + 16) * 2)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// Attach button and light
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerT3 - 4, rowRulerT0 - 6 + 2 + offsetTL1105), module, PhraseSeq32::ATTACH_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerT3 + 12 + offsetMediumLight, rowRulerT0 - 6 + offsetMediumLight), module, PhraseSeq32::ATTACH_LIGHT));		
		// Config switch
		addParam(createParam<CKSSNotify>(Vec(columnRulerT4 + hOffsetCKSS + 1, rowRulerT0 - 6 + vOffsetCKSS), module, PhraseSeq32::CONFIG_PARAM, 0.0f, 1.0f, PhraseSeq32::CONFIG_PARAM_INIT_VALUE));

		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const float octLightsIntY = 20.0f;
		for (int i = 0; i < 7; i++) {
			addParam(createParam<LEDButton>(Vec(15 + 3, 82 + 24 + i * octLightsIntY- 4.4f), module, PhraseSeq32::OCTAVE_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(createLight<MediumLight<RedLight>>(Vec(15 + 3 + 4.4f, 82 + 24 + i * octLightsIntY), module, PhraseSeq32::OCTAVE_LIGHTS + i));
		}
		// Keys and Key lights
		static const int keyNudgeX = 7;
		static const int KeyBlackY = 103;
		static const int KeyWhiteY = 141;
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 16;
		// Black keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(65+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 1, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(65+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 1 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(93+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 3, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(93+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 3 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(150+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 6, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(150+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 6 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(178+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 8, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(178+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 8 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(206+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 10, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(206+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 10 * 2));
		// White keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(51+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 0, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(51+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 0 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(79+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 2, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(79+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 2 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(107+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 4, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(107+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 4 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(136+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 5, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(136+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 5 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(164+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 7, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(164+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 7 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(192+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 9, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(192+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 9 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(220+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 11, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(220+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 11 * 2));
		
		// Key mode LED buttons	
		static const int colRulerKM = 267;
		addParam(createParam<LEDButton>(Vec(colRulerKM, KeyBlackY + offsetKeyLEDy - 4.4f), module, PhraseSeq32::KEYNOTE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MediumLight<RedLight>>(Vec(colRulerKM + 4.4f,  KeyBlackY + offsetKeyLEDy), module, PhraseSeq32::KEYNOTE_LIGHT));
		addParam(createParam<LEDButton>(Vec(colRulerKM, KeyWhiteY + offsetKeyLEDy - 4.4f), module, PhraseSeq32::KEYGATE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerKM + 4.4f,  KeyWhiteY + offsetKeyLEDy), module, PhraseSeq32::KEYGATE_LIGHT));

		
		
		// ****** Right side control area ******

		static const int rowRulerMK0 = 101;// Edit mode row
		static const int rowRulerMK1 = rowRulerMK0 + 56; // Run row
		static const int rowRulerMK2 = rowRulerMK1 + 54; // Copy-paste Tran/rot row
		static const int columnRulerMK0 = 307;// Edit mode column
		static const int columnRulerMK2 = columnRulerT4;// Mode/Len column
		static const int columnRulerMK1 = 366;// Display column
		
		// Edit mode switch
		addParam(createParam<CKSS>(Vec(columnRulerMK0 + 2 + hOffsetCKSS, rowRulerMK0 + vOffsetCKSS), module, PhraseSeq32::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(columnRulerMK1-15, rowRulerMK0 + 3 + vOffsetDisplay);
		displaySequence->box.size = Vec(55, 30);// 3 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Len/mode button
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK0 + 0 + offsetCKD6b), module, PhraseSeq32::RUNMODE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLight<SmallLight<RedLight>>(Vec(columnRulerMK2 + offsetCKD6b + 24, rowRulerMK0 + 0 + offsetCKD6b + 31), module, PhraseSeq32::RES_LIGHT));

		// Autostep
		addParam(createParam<CKSS>(Vec(columnRulerMK0 + 2 + hOffsetCKSS, rowRulerMK1 + 7 + vOffsetCKSS), module, PhraseSeq32::AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f));		
		// Sequence knob
		addParam(createDynamicParam<IMBigKnobInf>(Vec(columnRulerMK1 + 1 + offsetIMBigKnob, rowRulerMK0 + 55 + offsetIMBigKnob), module, PhraseSeq32::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));		
		// Transpose/rotate button
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK1 + 4 + offsetCKD6b), module, PhraseSeq32::TRAN_ROT_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// Reset LED bezel and light
		addParam(createParam<LEDBezel>(Vec(columnRulerMK0 - 43 + offsetLEDbezel, rowRulerMK2 + 5 + offsetLEDbezel), module, PhraseSeq32::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MuteLight<GreenLight>>(Vec(columnRulerMK0 - 43 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK2 + 5 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq32::RESET_LIGHT));
		// Run LED bezel and light
		addParam(createParam<LEDBezel>(Vec(columnRulerMK0 + 3 + offsetLEDbezel, rowRulerMK2 + 5 + offsetLEDbezel), module, PhraseSeq32::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MuteLight<GreenLight>>(Vec(columnRulerMK0 + 3 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK2 + 5 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq32::RUN_LIGHT));
		// Copy/paste buttons
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerMK1 - 10, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq32::COPY_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerMK1 + 20, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq32::PASTE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Copy-paste mode switch (3 position)
		addParam(createParam<CKSSThreeInv>(Vec(columnRulerMK2 + hOffsetCKSS + 1, rowRulerMK2 - 3 + vOffsetCKSSThree), module, PhraseSeq32::CPMODE_PARAM, 0.0f, 2.0f, 2.0f));	// 0.0f is top position

		
		
		// ****** Gate and slide section ******
		
		static const int rowRulerMB0 = 214;
		static const int columnRulerMBspacing = 70;
		static const int columnRulerMB2 = 130;// Gate2
		static const int columnRulerMB1 = columnRulerMB2 - columnRulerMBspacing;// Gate1 
		static const int columnRulerMB3 = columnRulerMB2 + columnRulerMBspacing;// Tie
		static const int posLEDvsButton = + 25;
		
		// Gate 1 light and button
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(columnRulerMB1 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::GATE1_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB1 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::GATE1_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Gate 2 light and button
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(columnRulerMB2 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::GATE2_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB2 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::GATE2_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Tie light and button
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerMB3 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::TIE_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB3 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::TIE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));

						
		
		// ****** Bottom two rows ******
		
		static const int inputJackSpacingX = 54;
		static const int outputJackSpacingX = 45;
		static const int rowRulerB0 = 323;
		static const int rowRulerB1 = 269;
		static const int columnRulerB0 = 22;
		static const int columnRulerB1 = columnRulerB0 + inputJackSpacingX;
		static const int columnRulerB2 = columnRulerB1 + inputJackSpacingX;
		static const int columnRulerB3 = columnRulerB2 + inputJackSpacingX;
		static const int columnRulerB4 = columnRulerB3 + inputJackSpacingX;
		static const int columnRulerB8 = columnRulerMK2 + 1;
		static const int columnRulerB7 = columnRulerB8 - outputJackSpacingX;
		static const int columnRulerB6 = columnRulerB7 - outputJackSpacingX;
		static const int columnRulerB5 = columnRulerB6 - outputJackSpacingX - 4;// clock and reset

		
		// Gate 1 probability light and button
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerB0 + posLEDvsButton + offsetMediumLight, rowRulerB1 + offsetMediumLight), module, PhraseSeq32::GATE1_PROB_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerB0 + offsetCKD6b, rowRulerB1 + offsetCKD6b), module, PhraseSeq32::GATE1_PROB_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Gate 1 probability knob
		addParam(createDynamicParam<IMSmallKnob>(Vec(columnRulerB1 + offsetIMSmallKnob, rowRulerB1 + offsetIMSmallKnob), module, PhraseSeq32::GATE1_KNOB_PARAM, 0.0f, 1.0f, 1.0f, &module->panelTheme));
		// Slide light and button
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerB2 + posLEDvsButton + offsetMediumLight, rowRulerB1 + offsetMediumLight), module, PhraseSeq32::SLIDE_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerB2 + offsetCKD6b, rowRulerB1 + offsetCKD6b), module, PhraseSeq32::SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Slide knob
		addParam(createDynamicParam<IMSmallKnob>(Vec(columnRulerB3 + offsetIMSmallKnob, rowRulerB1 + offsetIMSmallKnob), module, PhraseSeq32::SLIDE_KNOB_PARAM, 0.0f, 2.0f, 0.2f, &module->panelTheme));
		// CV in
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB4, rowRulerB1), Port::INPUT, module, PhraseSeq32::CV_INPUT, &module->panelTheme));
		// Clock input
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB5, rowRulerB1), Port::INPUT, module, PhraseSeq32::CLOCK_INPUT, &module->panelTheme));
		// Channel A outputs
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB6, rowRulerB1), Port::OUTPUT, module, PhraseSeq32::CVA_OUTPUT, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB7, rowRulerB1), Port::OUTPUT, module, PhraseSeq32::GATE1A_OUTPUT, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB8, rowRulerB1), Port::OUTPUT, module, PhraseSeq32::GATE2A_OUTPUT, &module->panelTheme));


		// CV control Inputs 
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB0, rowRulerB0), Port::INPUT, module, PhraseSeq32::LEFTCV_INPUT, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB1, rowRulerB0), Port::INPUT, module, PhraseSeq32::RIGHTCV_INPUT, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB2, rowRulerB0), Port::INPUT, module, PhraseSeq32::SEQCV_INPUT, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB3, rowRulerB0), Port::INPUT, module, PhraseSeq32::RUNCV_INPUT, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB4, rowRulerB0), Port::INPUT, module, PhraseSeq32::WRITE_INPUT, &module->panelTheme));
		// Reset input
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB5, rowRulerB0), Port::INPUT, module, PhraseSeq32::RESET_INPUT, &module->panelTheme));
		// Channel B outputs
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB6, rowRulerB0), Port::OUTPUT, module, PhraseSeq32::CVB_OUTPUT, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB7, rowRulerB0), Port::OUTPUT, module, PhraseSeq32::GATE1B_OUTPUT, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB8, rowRulerB0), Port::OUTPUT, module, PhraseSeq32::GATE2B_OUTPUT, &module->panelTheme));
		
		
		// Expansion module
		static const int rowRulerExpTop = 65;
		static const int rowSpacingExp = 60;
		static const int colRulerExp = 497;
		addInput(expPorts[0] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 0), Port::INPUT, module, PhraseSeq32::GATE1CV_INPUT, &module->panelTheme));
		addInput(expPorts[1] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 1), Port::INPUT, module, PhraseSeq32::GATE2CV_INPUT, &module->panelTheme));
		addInput(expPorts[2] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 2), Port::INPUT, module, PhraseSeq32::TIEDCV_INPUT, &module->panelTheme));
		addInput(expPorts[3] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 3), Port::INPUT, module, PhraseSeq32::SLIDECV_INPUT, &module->panelTheme));
		addInput(expPorts[4] = createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 4), Port::INPUT, module, PhraseSeq32::MODECV_INPUT, &module->panelTheme));
	}
};

Model *modelPhraseSeq32 = Model::create<PhraseSeq32, PhraseSeq32Widget>("Impromptu Modular", "Phrase-Seq-32", "SEQ - Phrase-Seq-32", SEQUENCER_TAG);

/*CHANGE LOG

0.6.13:
fix run mode bug (history not reset when hard reset)
fix slide bug when reset happens during a slide and run stays on
fix transposeOffset not initialized bug
add live mute on Gate1 and Gate2 buttons in song mode

0.6.12:
input refresh optimization
add buttons for note vs advanced-gate selection (remove timeout method)
transposition amount stays persistent and is saved (reset to 0 on module init or paste ALL)

0.6.11:
step optimization of lights refresh
change behavior of extra CV inputs (Gate1, Gate2, Tied, Slide), such that they act when triggered and not when write 
add RN2 run mode
implement copy-paste in song mode
implement cross paste trick for init and randomize seq/song
add AutoSeq option when writing via CV inputs 

0.6.10:
add advanced gate mode
unlock gates when tied (turn off when press tied, but allow to be turned back on)

0.6.9:
add FW2, FW3 and FW4 run modes for sequences (but not for song)
right click on notes now does same as left click but with autostep

0.6.8:
allow rollover when selecting sequences in a song phrase (easier access to higher numbered seqs)

0.6.7:
allow full edit capabilities in Seq and song mode
no reset on run by default, with switch added in context menu
reset does not revert seq or song number to 1
gate 2 is off by default
fix emitted monitoring gates to depend on gate states instead of always triggering

0.6.6:
config and knob bug fixes when loading patch

0.6.5:
paste 4, 8 doesn't loop over to overwrite first steps
small adjustements to gates and CVs used in monitoring write operations
add GATE1, GATE2, TIED, SLIDE CV inputs 
add MODE CV input (affects only selected sequence and in Seq mode)
add expansion panel option
swap MODE/LEN so that length happens first (update manual)

0.6.4:
initial release of PS32
*/

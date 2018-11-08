//***********************************************************************************************
//Multi-track multi-phrase 32 step sequencer module for VCV Rack by Marc Boulé
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
#include "PhraseSeq32ExUtil.hpp"


struct PhraseSeq32Ex : Module {
	enum ParamIds {
		LEFT_PARAM,
		RIGHT_PARAM,
		EDIT_PARAM,
		SEQUENCE_PARAM,
		RUN_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		ENUMS(OCTAVE_PARAM, 7),
		GATE_PARAM,
		SLIDE_BTN_PARAM,
		ATTACH_PARAM,
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),
		RUNMODE_PARAM,
		TRAN_ROT_PARAM,
		GATE_PROB_PARAM,
		TIE_PARAM,// Legato
		CPMODE_PARAM,
		ENUMS(STEP_PHRASE_PARAMS, 32),
		KEYNOTE_PARAM,
		KEYGATE_PARAM,
		TRACK_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		ENUMS(CV_INPUTS, 4),
		RESET_INPUT,
		CLOCK_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		MODECV_INPUT,
		GATECV_INPUT,
		GATEPCV_INPUT,
		TIEDCV_INPUT,
		SLIDECV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, 4),
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 32 * 2),// room for GreenRed
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12 * 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE_LIGHT, 2),// room for GreenRed
		SLIDE_LIGHT,
		ATTACH_LIGHT,
		GATE_PROB_LIGHT,
		TIE_LIGHT,
		KEYNOTE_LIGHT,
		ENUMS(KEYGATE_LIGHT, 2),// room for GreenRed
		RES_LIGHT,
		NUM_LIGHTS
	};
	
	// Constants
	enum DisplayStateIds {DISP_NORMAL, DISP_MODE, DISP_LENGTH, DISP_TRANSPOSE, DISP_ROTATE, DISP_PROBVAL, DISP_SLIDEVAL, DISP_REPS};

// INDENTED FULL LEFT WILL BE MOVE TO AN OBJECT	
	
	// Need to save
	int panelTheme = 0;
	int expansion = 0;
	bool autoseq;
//int pulsesPerStepIndex;// 0 to NUM_PPS_VALUES-1; 0 means normal gate mode of 1 pps; this is an index into ppsValues[]
	bool running;
//int runModeSeq[32];
//int runModeSong;
	int sequence;
//int lengths[32];// 1 to 32
//int phrase[32];// This is the song (series of phases; a phrase is a sequence number)
//int phraseReps[32];// a rep is 1 to 99
//int phrases;// number of phrases (song steps) 1 to 32
float cv[32][32];// [-3.0 : 3.917]. First index is sequence number, 2nd index is step
Attribute attributes[32][32];// First index is sequence number, 2nd index is step
	bool resetOnRun;
	bool attached;
//int transposeOffsets[32];
	SequencerKernel seq[4];

	// No need to save
	int stepIndexEdit;
//int stepIndexRun;
//unsigned long stepIndexRunHistory;
	int phraseIndexEdit;
//int phraseIndexRun;
//unsigned long phraseIndexRunHistory;
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate
	float editingGateCV;// no need to initialize, this goes with editingGate (output this only when editingGate > 0)
	int editingGateKeyLight;// no need to initialize, this goes with editingGate (use this only when editingGate > 0)
	int displayState;
// unsigned long slideStepsRemain;// 0 when no slide under way, downward step counter when sliding
// float slideCVdelta;// no need to initialize, this goes with slideStepsRemain
	float cvCPbuffer[32];// copy paste buffer for CVs
	Attribute attribCPbuffer[32];
	int phraseCPbuffer[32];
	int lengthCPbuffer;
	int modeCPbuffer;
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	int startCP;
	int rotateOffset;// no need to initialize, this goes with displayMode = DISP_ROTATE
	long clockIgnoreOnReset;
	unsigned long clockPeriod;// counts number of step() calls upward from last clock (reset after clock processed)
	long tiedWarning;// 0 when no warning, positive downward step counter timer when warning
//int gateCode;
	long revertDisplay;
	bool keyboardEditingGates;// 0 when no info, positive when gate1
	long editingPpqn;// 0 when no info, positive downward step counter timer when editing ppqn
//int ppqnCount;
	

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
	SchmittTrigger gateProbTrigger;
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


	inline bool isEditingSequence(void) {return params[EDIT_PARAM].value > 0.5f;}

inline void initAttrib(int seqn, int step) {attributes[seqn][step] = ATT_MSK_INITSTATE;}
inline void randomizeAttribute(int seqn, int step, int length) {
	attributes[seqn][step] = ((randomu32() & 0xF) & ((randomu32() % NUM_GATES) << gate1TypeShift) & ((randomu32() % 101) << GatePValShift) & ((randomu32() % 101) << slideValShift));
	if (getTied(seqn,step)) {
		attributes[seqn][step] &= 0xF;// clear other attributes if tied
		attributes[seqn][step] |= ATT_MSK_TIED;
		applyTiedStep(seqn, step, length);
	}
}

inline bool getGate(int seqn, int step) {return getGateA(attributes[seqn][step]);}
inline bool getTied(int seqn, int step) {return getTiedA(attributes[seqn][step]);}
inline bool getGateP(int seqn, int step) {return getGatePa(attributes[seqn][step]);}
inline int getGatePVal(int seqn, int step) {return getGatePValA(attributes[seqn][step]);}
inline bool getSlide(int seqn, int step) {return getSlideA(attributes[seqn][step]);}
inline int getSlideVal(int seqn, int step) {return getSlideValA(attributes[seqn][step]);}
inline int getGateType(int seqn, int step) {return getGateAType(attributes[seqn][step]);}

inline void setGate(int seqn, int step, bool gateState) {setGateA(&attributes[seqn][step], gateState);}
inline void setGateP(int seqn, int step, bool GatePstate) {setGatePa(&attributes[seqn][step], GatePstate);}
inline void setGatePVal(int seqn, int step, int gatePval) {setGatePValA(&attributes[seqn][step], gatePval);}
inline void setSlide(int seqn, int step, bool slideState) {setSlideA(&attributes[seqn][step], slideState);}
inline void setSlideVal(int seqn, int step, int slideVal) {setSlideValA(&attributes[seqn][step], slideVal);}
inline void setGateType(int seqn, int step, int gateType) {setGateTypeA(&attributes[seqn][step], gateType);}

inline void toggleGate(int seqn, int step) {toggleGateA(&attributes[seqn][step]);}
inline void toggleTied(int seqn, int step) {toggleTiedA(&attributes[seqn][step]);}
inline void toggleGateP(int seqn, int step) {toggleGatePa(&attributes[seqn][step]);}
inline void toggleSlide(int seqn, int step) {toggleSlideA(&attributes[seqn][step]);}



		
	PhraseSeq32Ex() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		seq[0].setId(0);
		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		autoseq = false;
		//pulsesPerStepIndex = 0;
		running = false;
		// runModeSong = MODE_FWD;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		sequence = 0;
		//phrases = 4;
		for (int i = 0; i < 32; i++) {
			for (int s = 0; s < 32; s++) {
				cv[i][s] = 0.0f;
				initAttrib(i, s);
			}
			//runModeSeq[i] = MODE_FWD;
			//phrase[i] = 0;
			//phraseReps[i] = 1;
			//lengths[i] = 32;
			cvCPbuffer[i] = 0.0f;
			attribCPbuffer[i] = ATT_MSK_INITSTATE;// lengthCPbuffer non negative will mean buf is for attribs not phrases
			phraseCPbuffer[i] = 0;
			//transposeOffsets[i] = 0;
		}
		initRun(true);
		lengthCPbuffer = 32;
		modeCPbuffer = MODE_FWD;
		countCP = 32;
		startCP = 0;
		editingGate = 0ul;
		infoCopyPaste = 0l;
		displayState = DISP_NORMAL;
		// slideStepsRemain = 0ul;
		attached = true;
		clockPeriod = 0ul;
		tiedWarning = 0ul;
		revertDisplay = 0l;
		resetOnRun = false;
		keyboardEditingGates = false;
		editingPpqn = 0l;
		
		seq[0].onReset();
	}
	
	
	void onRandomize() override {
		// runModeSong = randomu32() % 5;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		sequence = randomu32() % 32;
		//phrases = 1 + (randomu32() % 32);
		for (int i = 0; i < 32; i++) {
			//runModeSeq[i] = randomu32() % NUM_MODES;
			//phrase[i] = randomu32() % 32;
			//phraseReps[i] = randomu32() % 4 + 1;
			//lengths[i] = 1 + (randomu32() % 32);
			//transposeOffsets[i] = 0;
			for (int s = 0; s < 32; s++) {
				cv[i][s] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
				randomizeAttribute(i, s, seq[0].getLength(i));
			}
		}
		seq[0].onRandomize();
		
		initRun(true);
	}
	
	
	void initRun(bool hard) {// run button activated or run edge in run input jack
		// if (hard) {
			// phraseIndexRun = (seq[0].getRunModeSong() == MODE_REV ? seq[0].getPhrases() - 1 : 0);
			// phraseIndexRunHistory = 0;
		// }
		int seqn = (isEditingSequence() ? sequence : seq[0].getPhrase(seq[0].getPhraseIndexRun())); // redundant for now, still need it for below
		// if (hard) {
			// stepIndexRun = (seq[0].getRunModeSeq(seqn) == MODE_REV ? seq[0].getLength(seqn) - 1 : 0);
			// stepIndexRunHistory = 0;
		// }
		// ppqnCount = 0;
		// seq[0].calcGateCodeEx(attributes[seqn][stepIndexRun]);// moved below seq[0].initRun
		// slideStepsRemain = 0ul;
		
		seq[0].initRun(hard, isEditingSequence(), sequence);
				
		seq[0].calcGateCodeEx(attributes[seqn][seq[0].getStepIndexRun()]);

		
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
		
		// pulsesPerStepIndex
		//json_object_set_new(rootJ, "pulsesPerStepIndex", json_integer(pulsesPerStepIndex));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// runModeSeq
		// json_t *runModeSeqJ = json_array();
		// for (int i = 0; i < 32; i++)
			// json_array_insert_new(runModeSeqJ, i, json_integer(runModeSeq[i]));
		// json_object_set_new(rootJ, "runModeSeq", runModeSeqJ);

		// runModeSong
		// json_object_set_new(rootJ, "runModeSong", json_integer(runModeSong));

		// sequence
		json_object_set_new(rootJ, "sequence", json_integer(sequence));

		// lengths
		//json_t *lengthsJ = json_array();
		//for (int i = 0; i < 32; i++)
			//json_array_insert_new(lengthsJ, i, json_integer(lengths[i]));
		//json_object_set_new(rootJ, "lengths", lengthsJ);

		// phrase 
		// json_t *phraseJ = json_array();
		// for (int i = 0; i < 32; i++)
			// json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		// json_object_set_new(rootJ, "phrase", phraseJ);

		// phraseReps 
		// json_t *phraseRepsJ = json_array();
		// for (int i = 0; i < 32; i++)
			// json_array_insert_new(phraseRepsJ, i, json_integer(phraseReps[i]));
		// json_object_set_new(rootJ, "phraseReps", phraseRepsJ);

		// phrases
		//json_object_set_new(rootJ, "phrases", json_integer(phrases));

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
		// json_t *transposeOffsetsJ = json_array();
		// for (int i = 0; i < 32; i++)
			// json_array_insert_new(transposeOffsetsJ, i, json_integer(transposeOffsets[i]));
		// json_object_set_new(rootJ, "transposeOffsets", transposeOffsetsJ);

		seq[0].toJson(rootJ);
		
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

		// pulsesPerStepIndex
		//json_t *pulsesPerStepIndexJ = json_object_get(rootJ, "pulsesPerStepIndex");
		//if (pulsesPerStepIndexJ)
			//pulsesPerStepIndex = json_integer_value(pulsesPerStepIndexJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// runModeSeq
		// json_t *runModeSeqJ = json_object_get(rootJ, "runModeSeq");
		// if (runModeSeqJ) {
			// for (int i = 0; i < 32; i++)
			// {
				// json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
				// if (runModeSeqArrayJ)
					// runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
			// }			
		// }		
		
		// runModeSong
		// json_t *runModeSongJ = json_object_get(rootJ, "runModeSong");
		// if (runModeSongJ)
			// runModeSong = json_integer_value(runModeSongJ);
		
		// sequence
		json_t *sequenceJ = json_object_get(rootJ, "sequence");
		if (sequenceJ)
			sequence = json_integer_value(sequenceJ);
		
		// lengths
		// json_t *lengthsJ = json_object_get(rootJ, "lengths");
		// if (lengthsJ)
			// for (int i = 0; i < 32; i++)
			// {
				// json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
				// if (lengthsArrayJ)
					// lengths[i] = json_integer_value(lengthsArrayJ);
			// }
			
		// phrase
		// json_t *phraseJ = json_object_get(rootJ, "phrase");
		// if (phraseJ)
			// for (int i = 0; i < 32; i++)
			// {
				// json_t *phraseArrayJ = json_array_get(phraseJ, i);
				// if (phraseArrayJ)
					// phrase[i] = json_integer_value(phraseArrayJ);
			// }
		
		// phraseReps
		// json_t *phraseRepsJ = json_object_get(rootJ, "phraseReps");
		// if (phraseRepsJ)
			// for (int i = 0; i < 32; i++)
			// {
				// json_t *phraseRepsArrayJ = json_array_get(phraseRepsJ, i);
				// if (phraseRepsArrayJ)
					// phraseReps[i] = json_integer_value(phraseRepsArrayJ);
			// }
		
		// phrases
		//json_t *phrasesJ = json_object_get(rootJ, "phrases");
		//if (phrasesJ)
			//phrases = json_integer_value(phrasesJ);
		
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
		// json_t *transposeOffsetsJ = json_object_get(rootJ, "transposeOffsets");
		// if (transposeOffsetsJ) {
			// for (int i = 0; i < 32; i++)
			// {
				// json_t *transposeOffsetsArrayJ = json_array_get(transposeOffsetsJ, i);
				// if (transposeOffsetsArrayJ)
					// transposeOffsets[i] = json_integer_value(transposeOffsetsArrayJ);
			// }			
		// }
		
		seq[0].fromJson(rootJ);
		
		// Initialize dependants after everything loaded
		initRun(true);
	}

	void rotateSeq(int seqNum, bool directionRight, int seqLength) {
		float rotCV;
		Attribute rotAttributes;
		int iStart = 0;
		int iEnd = seqLength - 1;
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
			
			// Seq CV input
			if (inputs[SEQCV_INPUT].active) {
				sequence = (int) clamp( round(inputs[SEQCV_INPUT].value * (32.0f - 1.0f) / 10.0f), 0.0f, (32.0f - 1.0f) );
			}
			
			// Mode CV input
			if (inputs[MODECV_INPUT].active) {
				if (editingSequence)
					seq[0].setRunModeSeq(sequence, (int) clamp( round(inputs[MODECV_INPUT].value * ((float)NUM_MODES - 1.0f) / 10.0f), 0.0f, (float)NUM_MODES - 1.0f ));
			}
			
			// Attach button
			if (attachedTrigger.process(params[ATTACH_PARAM].value)) {
				attached = !attached;
				displayState = DISP_NORMAL;			
			}
			if (running && attached) {
				if (editingSequence)
					stepIndexEdit = seq[0].getStepIndexRun();
				else
					phraseIndexEdit = seq[0].getPhraseIndexRun();
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
						attribCPbuffer[i] = attributes[sequence][s];
					}
					lengthCPbuffer = seq[0].getLength(sequence);
					modeCPbuffer = seq[0].getRunModeSeq(sequence);
				}
				else {
					for (int i = 0, p = startCP; i < countCP; i++, p++)
						phraseCPbuffer[i] = seq[0].getPhrase(p);
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
							attributes[sequence][s] = attribCPbuffer[i];
						}
						if (params[CPMODE_PARAM].value > 1.5f) {// all
							seq[0].setLength(sequence, lengthCPbuffer);
							seq[0].setRunModeSeq(sequence, modeCPbuffer);
							seq[0].resetTransposeOffset(sequence);
						}
					}
					else {// crossed paste to seq (seq vs song)
						if (params[CPMODE_PARAM].value > 1.5f) { // ALL (init steps)
							for (int s = 0; s < 16; s++) {
								cv[sequence][s] = 0.0f;
								initAttrib(sequence, s);
							}
							seq[0].resetTransposeOffset(sequence);
						}
						else if (params[CPMODE_PARAM].value < 0.5f) {// 4 (randomize CVs)
							for (int s = 0; s < 32; s++)
								cv[sequence][s] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
							seq[0].resetTransposeOffset(sequence);
						}
						else {// 8 (randomize gate 1)
							for (int s = 0; s < 32; s++)
								if ( (randomu32() & 0x1) != 0)
									toggleGate(sequence, s);
						}
						startCP = 0;
						countCP = 32;
						infoCopyPaste *= 2l;
					}
				}
				else {
					if (lengthCPbuffer < 0) {// non-crossed paste (seq vs song)
						for (int i = 0, p = startCP; i < countCP; i++, p++)
							seq[0].setPhrase(p, phraseCPbuffer[i]);
					}
					else {// crossed paste to song (seq vs song)
						if (params[CPMODE_PARAM].value > 1.5f) { // ALL (init phrases)
							for (int p = 0; p < 32; p++)
								seq[0].setPhrase(p, 0);
						}
						else if (params[CPMODE_PARAM].value < 0.5f) {// 4 (phrases increase from 1 to 32)
							for (int p = 0; p < 32; p++)
								seq[0].setPhrase(p, p);						
						}
						else {// 8 (randomize phrases)
							for (int p = 0; p < 32; p++)
								seq[0].setPhrase(p, randomu32() % 32);
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
					cv[sequence][stepIndexEdit] = inputs[CV_INPUTS + 0].value;
					applyTiedStep(sequence, stepIndexEdit, seq[0].getLength(sequence));
					editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
					editingGateCV = cv[sequence][stepIndexEdit];
					editingGateKeyLight = -1;
					// Autostep (after grab all active inputs)
					if (params[AUTOSTEP_PARAM].value > 0.5f) {
						stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, 32);
						if (stepIndexEdit == 0 && autoseq)
							sequence = moveIndexEx(sequence, sequence + 1, 32);
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
						seq[0].modLength(sequence, delta);
					}
					else {
						seq[0].modPhrases(delta);
					}
				}
				else {
					if (!running || !attached) {// don't move heads when attach and running
						if (editingSequence) {
							stepIndexEdit += delta;
							if (stepIndexEdit < 0)
								stepIndexEdit = seq[0].getLength(sequence) - 1;
							if (stepIndexEdit >= 32)
								stepIndexEdit = 0;
							if (!getTied(sequence,stepIndexEdit)) {// play if non-tied step
								if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
									editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
									editingGateCV = cv[sequence][stepIndexEdit];
									editingGateKeyLight = -1;
								}
							}
						}
						else {
							phraseIndexEdit = moveIndexEx(phraseIndexEdit, phraseIndexEdit + delta, 32);
							if (!running)
								seq[0].setPhraseIndexRun(phraseIndexEdit);	
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
						seq[0].setLength(sequence, stepPressed + 1);
					else
						seq[0].setPhrases(stepPressed + 1);
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
							}
						}
						else {
							phraseIndexEdit = stepPressed;
							if (!running)
								seq[0].setPhraseIndexRun(stepPressed);
						}
					}
					displayState = DISP_NORMAL;
				}
			} 
			
			// Mode/Length button
			if (modeTrigger.process(params[RUNMODE_PARAM].value)) {
				if (editingPpqn != 0l)
					editingPpqn = 0l;			
				if (displayState != DISP_LENGTH && displayState != DISP_MODE)
					displayState = DISP_LENGTH;
				else if (displayState == DISP_LENGTH)
					displayState = DISP_MODE;
				else
					displayState = DISP_NORMAL;
				modeHoldDetect.start((long) (holdDetectTime * sampleRate / displayRefreshStepSkips));
			}
			
			// Transpose/Rotate/Reps button
			if (transposeTrigger.process(params[TRAN_ROT_PARAM].value)) {
				if (editingSequence) {
					if (displayState != DISP_TRANSPOSE && displayState != DISP_ROTATE) {
						displayState = DISP_TRANSPOSE;
					}
					else if (displayState == DISP_TRANSPOSE) {
						displayState = DISP_ROTATE;
						rotateOffset = 0;
					}
					else 
						displayState = DISP_NORMAL;
				}
				else {
					if (displayState != DISP_REPS)
						displayState = DISP_REPS;
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
						seq[0].modPulsesPerStepIndex(deltaKnob);
						if (seq[0].getPulsesPerStep() == 1)
							keyboardEditingGates = false;
						editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODE) {
						if (editingSequence) {
							if (!inputs[MODECV_INPUT].active) {
								seq[0].modRunModeSeq(sequence, deltaKnob);
							}
						}
						else {
							seq[0].modRunModeSong(deltaKnob);
						}
					}
					else if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							seq[0].modLength(sequence, deltaKnob);
						}
						else {
							seq[0].modPhrases(deltaKnob);
						}
					}
					else if (displayState == DISP_TRANSPOSE) {
						if (editingSequence) {
							seq[0].modTransposeOffset(sequence, deltaKnob);
							// Tranpose by this number of semi-tones: deltaKnob
							float transposeOffsetCV = ((float)(deltaKnob))/12.0f;
							for (int s = 0; s < 32; s++) 
								cv[sequence][s] += transposeOffsetCV;
						}
					}
					else if (displayState == DISP_ROTATE) {
						if (editingSequence) {
							rotateOffset += deltaKnob;
							if (rotateOffset > 99) rotateOffset = 99;
							if (rotateOffset < -99) rotateOffset = -99;	
							if (deltaKnob > 0 && deltaKnob < 99) {// Rotate right, 99 is safety
								for (int i = deltaKnob; i > 0; i--)
									rotateSeq(sequence, true, seq[0].getLength(sequence));
							}
							if (deltaKnob < 0 && deltaKnob > -99) {// Rotate left, 99 is safety
								for (int i = deltaKnob; i < 0; i++)
									rotateSeq(sequence, false, seq[0].getLength(sequence));
							}
						}						
					}					
					else if (displayState == DISP_PROBVAL) {
						if (editingSequence) {
							int pVal = getGatePVal(sequence, stepIndexEdit);
							pVal += deltaKnob * 2;
							if (pVal > 100) pVal = 100;
							if (pVal < 0) pVal = 0;
							setGatePVal(sequence, stepIndexEdit, pVal);						
						}
					}
					else if (displayState == DISP_SLIDEVAL) {
						if (editingSequence) {
							int sVal = getSlideVal(sequence, stepIndexEdit);
							sVal += deltaKnob * 2;
							if (sVal > 100) sVal = 100;
							if (sVal < 0) sVal = 0;
							setSlideVal(sequence, stepIndexEdit, sVal);						
						}
					}
					else if (displayState == DISP_REPS) {
						if (!editingSequence) {
							seq[0].modPhraseReps(phraseIndexEdit, deltaKnob);
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
							seq[0].modPhrase(phraseIndexEdit, deltaKnob);
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
							applyTiedStep(sequence, stepIndexEdit, seq[0].getLength(sequence));
						}
						editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
						editingGateCV = cv[sequence][stepIndexEdit];
						editingGateKeyLight = -1;
					}
				}
			}		
			
			// Keyboard buttons
			for (int i = 0; i < 12; i++) {
				if (keyTriggers[i].process(params[KEY_PARAMS + i].value)) {
					if (editingSequence) {
						if (keyboardEditingGates) {
							int newMode = seq[0].keyIndexToGateTypeEx(i);
							if (newMode != -1)
								setGateType(sequence, stepIndexEdit, newMode);
							else
								editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
						}
						else if (getTied(sequence,stepIndexEdit)) {
							if (params[KEY_PARAMS + i].value > 1.5f)
								stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, 32);
							else
								tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
						}
						else {			
							cv[sequence][stepIndexEdit] = floor(cv[sequence][stepIndexEdit]) + ((float) i) / 12.0f;
							applyTiedStep(sequence, stepIndexEdit, seq[0].getLength(sequence));
							editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							editingGateCV = cv[sequence][stepIndexEdit];
							editingGateKeyLight = -1;
							if (params[KEY_PARAMS + i].value > 1.5f) {
								stepIndexEdit = moveIndexEx(stepIndexEdit, stepIndexEdit + 1, 32);
								editingGateKeyLight = i;
							}
						}						
					}
					displayState = DISP_NORMAL;
				}
			}
			
			// Keyboard mode (note or gate type)
			if (keyNoteTrigger.process(params[KEYNOTE_PARAM].value)) {
				keyboardEditingGates = false;
			}
			if (keyGateTrigger.process(params[KEYGATE_PARAM].value)) {
				if (seq[0].getPulsesPerStep() == 1) {
					editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
				}
				else {
					keyboardEditingGates = true;
				}
			}

			// Gate, GateProb, Slide and Tied buttons
			if (gate1Trigger.process(params[GATE_PARAM].value + inputs[GATECV_INPUT].value)) {
				if (editingSequence) {
					toggleGate(sequence, stepIndexEdit);
				}
				displayState = DISP_NORMAL;
			}		
			if (gateProbTrigger.process(params[GATE_PROB_PARAM].value + inputs[GATEPCV_INPUT].value)) {
				displayState = DISP_NORMAL;
				if (editingSequence) {
					if (getTied(sequence,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else {
						toggleGateP(sequence, stepIndexEdit);
						if (getGateP(sequence,stepIndexEdit))
							displayState = DISP_PROBVAL;
					}
				}
			}		
			if (slideTrigger.process(params[SLIDE_BTN_PARAM].value + inputs[SLIDECV_INPUT].value)) {
				displayState = DISP_NORMAL;
				if (editingSequence) {
					if (getTied(sequence,stepIndexEdit))
						tiedWarning = (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips);
					else {
						toggleSlide(sequence, stepIndexEdit);
						if (getSlide(sequence,stepIndexEdit))
							displayState = DISP_SLIDEVAL;
					}
				}
			}		
			if (tiedTrigger.process(params[TIE_PARAM].value + inputs[TIEDCV_INPUT].value)) {
				if (editingSequence) {
					toggleTied(sequence, stepIndexEdit);
					if (getTied(sequence,stepIndexEdit)) {
						setGate(sequence, stepIndexEdit, false);
						setGateP(sequence, stepIndexEdit, false);
						setSlide(sequence, stepIndexEdit, false);
						applyTiedStep(sequence, stepIndexEdit, seq[0].getLength(sequence));
					}
				}
				displayState = DISP_NORMAL;
			}		
			
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].value)) {
			if (running && clockIgnoreOnReset == 0l) {
				int newSeq = sequence;// good value when editingSequence, overwrite if not editingSequence
				if (seq[0].incPpqnCountAndCmpWithZero()) {
					float slideFromCV = 0.0f;
					if (editingSequence) {
						slideFromCV = cv[sequence][seq[0].getStepIndexRun()];
						seq[0].moveIndexRunMode(true, seq[0].getLength(sequence), seq[0].getRunModeSeq(sequence),  1);// 1 count is enough, since the return boundaryCross boolean is ignored (it will loop the same seq continually)
					}
					else {
						slideFromCV = cv[seq[0].getPhrase(seq[0].getPhraseIndexRun())][seq[0].getStepIndexRun()];
						if (seq[0].moveIndexRunMode(true, seq[0].getLength(seq[0].getPhrase(seq[0].getPhraseIndexRun())), seq[0].getRunModeSeq(seq[0].getPhrase(seq[0].getPhraseIndexRun())), seq[0].getPhraseReps(seq[0].getPhraseIndexRun()))) {
							seq[0].moveIndexRunMode(false, seq[0].getPhrases(), seq[0].getRunModeSong(), 1);// 1 count is enough, since the return boundaryCross boolean is ignored (it will loop the song continually)
							seq[0].setStepIndexRun(seq[0].getRunModeSeq(seq[0].getPhrase(seq[0].getPhraseIndexRun())) == MODE_REV ? seq[0].getLength(seq[0].getPhrase(seq[0].getPhraseIndexRun())) - 1 : 0);// must always refresh after phraseIndexRun has changed
						}
						newSeq = seq[0].getPhrase(seq[0].getPhraseIndexRun());
					}

					// Slide
					if (getSlide(newSeq, seq[0].getStepIndexRun())) {
						float slideToCV = cv[newSeq][seq[0].getStepIndexRun()];
						unsigned long slideStepsRemaining = (unsigned long) (((float)clockPeriod * seq[0].getPulsesPerStep()) * ((float)getSlideVal(newSeq, seq[0].getStepIndexRun()) / 100.0f));
						seq[0].setSlideStepsRemainAndCVdelta(slideStepsRemaining, slideToCV - slideFromCV);
					}
				}
				else {
					if (!editingSequence)
						newSeq = seq[0].getPhrase(seq[0].getPhraseIndexRun());
				}
				seq[0].calcGateCodeEx(attributes[newSeq][seq[0].getStepIndexRun()]);
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
		int seqn = editingSequence ? (sequence) : (seq[0].getPhrase(running ? seq[0].getPhraseIndexRun() : phraseIndexEdit));
		int step0 = editingSequence ? (running ? seq[0].getStepIndexRun() : stepIndexEdit) : (seq[0].getStepIndexRun());
		if (running) {
			bool muteGateA = !editingSequence && ((params[GATE_PARAM].value + inputs[GATECV_INPUT].value) > 0.5f);// live mute
			outputs[CV_OUTPUTS + 0].value = cv[seqn][step0] - seq[0].calcSlideOffset();
			outputs[GATE_OUTPUTS + 0].value = (seq[0].calcGate(clockTrigger, clockPeriod, sampleRate) && !muteGateA) ? 10.0f : 0.0f;
		}
		else {// not running 
			outputs[CV_OUTPUTS + 0].value = (editingGate > 0ul) ? editingGateCV : cv[seqn][step0];
			outputs[GATE_OUTPUTS + 0].value = (editingGate > 0ul) ? 10.0f : 0.0f;
		}
		seq[0].decSlideStepsRemain();

		
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
					if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							int seqEnd = seq[0].getLength(sequence) - 1;
							if (i < seqEnd)
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 0.1f, 0.0f);
							else if (i == seqEnd)
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 1.0f, 0.0f);
							else 
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 0.0f, 0.0f);
						}
						else {
							int phrasesEnd = seq[0].getPhrases() - 1;
							if (i < phrasesEnd)
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, 0.1f, 0.0f);
							else
								setGreenRed(STEP_PHRASE_LIGHTS + i * 2, (i == phrasesEnd) ? 1.0f : 0.0f, 0.0f);
						}
					}
					else {// normal led display (i.e. not length)
						float red = 0.0f;
						float green = 0.0f;
						// Run cursor (green)
						if (editingSequence)
							green = ((running && (i == seq[0].getStepIndexRun())) ? 1.0f : 0.0f);
						else {
							green = ((running && (i == seq[0].getPhraseIndexRun())) ? 1.0f : 0.0f);
							green += ((running && (i == seq[0].getStepIndexRun()) && i != phraseIndexEdit) ? 0.1f : 0.0f);
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
				octCV = cv[seq[0].getPhrase(phraseIndexEdit)][seq[0].getStepIndexRun()];
			int octLightIndex = (int) floor(octCV + 3.0f);
			for (int i = 0; i < 7; i++) {
				float red = 0.0f;
				if (editingSequence || running) {
					if (tiedWarning > 0l) {
						bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
						red = (warningFlashState && (i == (6 - octLightIndex))) ? 1.0f : 0.0f;
					}
					else				
						red = (i == (6 - octLightIndex) ? 1.0f : 0.0f);
				}
				lights[OCTAVE_LIGHTS + i].value = red;
			}
			
			// Keyboard lights
			float cvValOffset;
			if (editingSequence) 
				cvValOffset = cv[sequence][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
			else	
				cvValOffset = cv[seq[0].getPhrase(phraseIndexEdit)][seq[0].getStepIndexRun()] + 10.0f;//to properly handle negative note voltages
			int keyLightIndex = (int) clamp(  roundf( (cvValOffset-floor(cvValOffset)) * 12.0f ),  0.0f,  11.0f);
			if (keyboardEditingGates && editingSequence) {
				int modeLightIndex = getGateType(sequence, stepIndexEdit);
				for (int i = 0; i < 12; i++) {
					if (i == modeLightIndex)
						setGreenRed(KEY_LIGHTS + i * 2, 1.0f, 1.0f);
					else
						setGreenRed(KEY_LIGHTS + i * 2, 0.0f, i == keyLightIndex ? 0.1f : 0.0f);
				}
			}
			else {
				for (int i = 0; i < 12; i++) {
					float red = 0.0f;
					float green = 0.0f;
					if (editingSequence || running) {
						if (tiedWarning > 0l) {
							bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
							red = (warningFlashState && i == keyLightIndex) ? 1.0f : 0.0f;
						}
						else {
							if (editingGate > 0ul && editingGateKeyLight != -1)
								red = (i == editingGateKeyLight ? ((float) editingGate / (float)(gateTime * sampleRate / displayRefreshStepSkips)) : 0.0f);
							else
								red = (i == keyLightIndex ? 1.0f : 0.0f);
						}
					}
					setGreenRed(KEY_LIGHTS + i * 2, green, red);
				}
			}		

			// Gate, GateProb, Slide and Tied lights 
			if (editingSequence  || running) {
				Attribute attributesVal = attributes[sequence][stepIndexEdit];
				if (!editingSequence)
					attributesVal = attributes[seq[0].getPhrase(phraseIndexEdit)][seq[0].getStepIndexRun()];
				//
				if (!getGateA(attributesVal)) 
					setGreenRed(GATE_LIGHT, 0.0f, 0.0f);
				else if (seq[0].getPulsesPerStep() == 1) 
					setGreenRed(GATE_LIGHT, 0.0f, 1.0f);
				else 
					setGreenRed(GATE_LIGHT, 1.0f, 1.0f);
				lights[GATE_PROB_LIGHT].value = getGatePa(attributesVal) ? 1.0f : 0.0f;
				lights[SLIDE_LIGHT].value = getSlideA(attributesVal) ? 1.0f : 0.0f;
				if (tiedWarning > 0l) {
					bool warningFlashState = calcWarningFlash(tiedWarning, (long) (tiedWarningTime * sampleRate / displayRefreshStepSkips));
					lights[TIE_LIGHT].value = (warningFlashState) ? 1.0f : 0.0f;
				}
				else
					lights[TIE_LIGHT].value = getTiedA(attributesVal) ? 1.0f : 0.0f;			
			}
			else {
				setGreenRed(GATE_LIGHT, 0.0f, 0.0f);
				lights[GATE_PROB_LIGHT].value = 0.0f;
				lights[SLIDE_LIGHT].value = 0.0f;
				lights[TIE_LIGHT].value = 0.0f;
			}
			
			// Key mode light (note or gate type)
			lights[KEYNOTE_LIGHT].value = !keyboardEditingGates ? 10.0f : 0.0f;
			if (!keyboardEditingGates)
				setGreenRed(KEYGATE_LIGHT, 0.0f, 0.0f);
			else
				setGreenRed(KEYGATE_LIGHT, 1.0f, 1.0f);
			
			// Clk Res light
			long editingPpqnInit = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
			if ( ((editingPpqn > 0l) && (editingPpqn < (editingPpqnInit / 6l))) ||
				 ((editingPpqn > (editingPpqnInit * 2l / 6l)) && (editingPpqn < (editingPpqnInit * 3l / 6l))) ||
				 ((editingPpqn > (editingPpqnInit * 4l / 6l)) && (editingPpqn < (editingPpqnInit * 5l / 6l))) )
				lights[RES_LIGHT].value = 1.0f;
			else 
				lights[RES_LIGHT].value = 0.0f;

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
	
};



struct PhraseSeq32ExWidget : ModuleWidget {
	PhraseSeq32Ex *module;
	DynamicSVGPanel *panel;
	int oldExpansion;
	int expWidth = 60;
	IMPort* expPorts[5];
	
	struct SequenceDisplayWidget : TransparentWidget {
		PhraseSeq32Ex *module;
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
					float cpMode = module->params[PhraseSeq32Ex::CPMODE_PARAM].value;
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
				snprintf(displayStr, 4, "x%2u", (unsigned) module->seq[0].getPulsesPerStep());
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_MODE) {
				if (editingSequence)
					runModeToStr(module->seq[0].getRunModeSeq(module->sequence));
				else
					runModeToStr(module->seq[0].getRunModeSong());
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_LENGTH) {
				if (editingSequence)
					snprintf(displayStr, 4, "L%2u", (unsigned) module->seq[0].getLength(module->sequence));
				else
					snprintf(displayStr, 4, "L%2u", (unsigned) module->seq[0].getPhrases());
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_TRANSPOSE) {
				int tranOffset = module->seq[0].getTransposeOffset(module->sequence);
				snprintf(displayStr, 4, "+%2u", (unsigned) abs(tranOffset));
				if (tranOffset < 0)
					displayStr[0] = '-';
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_ROTATE) {
				snprintf(displayStr, 4, ")%2u", (unsigned) abs(module->rotateOffset));
				if (module->rotateOffset < 0)
					displayStr[0] = '(';
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_PROBVAL) {
				int prob = module->getGatePVal(module->sequence, module->stepIndexEdit);
				if ( prob>= 100)
					snprintf(displayStr, 4, "1,0");
				else if (prob >= 10)
					snprintf(displayStr, 4, ",%2u", (unsigned) prob);
				else if (prob >= 1)
					snprintf(displayStr, 4, " ,%1u", (unsigned) prob);
				else
					snprintf(displayStr, 4, "  0");
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_SLIDEVAL) {
				int slide = module->getSlideVal(module->sequence, module->stepIndexEdit);
				if ( slide>= 100)
					snprintf(displayStr, 4, "1,0");
				else if (slide >= 10)
					snprintf(displayStr, 4, ",%2u", (unsigned) slide);
				else if (slide >= 1)
					snprintf(displayStr, 4, " ,%1u", (unsigned) slide);
				else
					snprintf(displayStr, 4, "  0");
			}
			else if (module->displayState == PhraseSeq32Ex::DISP_REPS) {
				snprintf(displayStr, 4, "R%2u", (unsigned) abs(module->seq[0].getPhraseReps(module->phraseIndexEdit)));
			}
			else {// DISP_NORMAL
				snprintf(displayStr, 4, " %2u", (unsigned) (editingSequence ? 
					module->sequence : module->seq[0].getPhrase(module->phraseIndexEdit)) + 1 );
			}
			nvgText(vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	struct PanelThemeItem : MenuItem {
		PhraseSeq32Ex *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	struct ExpansionItem : MenuItem {
		PhraseSeq32Ex *module;
		void onAction(EventAction &e) override {
			module->expansion = module->expansion == 1 ? 0 : 1;
		}
	};
	struct ResetOnRunItem : MenuItem {
		PhraseSeq32Ex *module;
		void onAction(EventAction &e) override {
			module->resetOnRun = !module->resetOnRun;
		}
	};
	struct AutoseqItem : MenuItem {
		PhraseSeq32Ex *module;
		void onAction(EventAction &e) override {
			module->autoseq = !module->autoseq;
		}
	};
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		PhraseSeq32Ex *module = dynamic_cast<PhraseSeq32Ex*>(this->module);
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
		
	PhraseSeq32ExWidget(PhraseSeq32Ex *module) : ModuleWidget(module) {
		this->module = module;
		oldExpansion = -1;
		
		// Main panel from Inkscape
        panel = new DynamicSVGPanel();
        panel->mode = &module->panelTheme;
		panel->expWidth = &expWidth;
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/PhraseSeq32Ex.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/PhraseSeq32Ex.svg")));
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
		static const int columnRulerT0 = 16;//30;// Step/Phase LED buttons
		static const int columnRulerT3 = 377;// Attach 
		static const int columnRulerT4 = 460;// Track 

		// Step/Phrase LED buttons
		int posX = columnRulerT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		for (int x = 0; x < 16; x++) {
			// First row
			addParam(createParam<LEDButton>(Vec(posX, rowRulerT0 - 10 + 3 - 4.4f), module, PhraseSeq32Ex::STEP_PHRASE_PARAMS + x, 0.0f, 1.0f, 0.0f));
			addChild(createLight<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRulerT0 - 10 + 3), module, PhraseSeq32Ex::STEP_PHRASE_LIGHTS + (x * 2)));
			// Second row
			addParam(createParam<LEDButton>(Vec(posX, rowRulerT0 + 10 + 3 - 4.4f), module, PhraseSeq32Ex::STEP_PHRASE_PARAMS + x + 16, 0.0f, 1.0f, 0.0f));
			addChild(createLight<MediumLight<GreenRedLight>>(Vec(posX + 4.4f, rowRulerT0 + 10 + 3), module, PhraseSeq32Ex::STEP_PHRASE_LIGHTS + ((x + 16) * 2)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// Attach button and light
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerT3 - 4, rowRulerT0 - 6 + 2 + offsetTL1105), module, PhraseSeq32Ex::ATTACH_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerT3 + 12 + offsetMediumLight, rowRulerT0 - 6 + offsetMediumLight), module, PhraseSeq32Ex::ATTACH_LIGHT));		
		// Track knob
		addParam(createDynamicParam<IMSmallKnob>(Vec(columnRulerT4 + offsetIMSmallKnob, rowRulerT0 - 6 + offsetIMSmallKnob), module, PhraseSeq32Ex::TRACK_PARAM, 0.0f, 4.0f, 0.0f, &module->panelTheme));
		
		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const float octLightsIntY = 20.0f;
		for (int i = 0; i < 7; i++) {
			addParam(createParam<LEDButton>(Vec(15 + 1, 82 + 24 + i * octLightsIntY- 4.4f), module, PhraseSeq32Ex::OCTAVE_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(createLight<MediumLight<RedLight>>(Vec(15 + 1 + 4.4f, 82 + 24 + i * octLightsIntY), module, PhraseSeq32Ex::OCTAVE_LIGHTS + i));
		}
		// Keys and Key lights
		static const int keyNudgeX = 5;
		static const int KeyBlackY = 103;
		static const int KeyWhiteY = 141;
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 16;
		// Black keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(65+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 1, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(65+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 1 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(93+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 3, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(93+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 3 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(150+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 6, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(150+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 6 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(178+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 8, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(178+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 8 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(206+keyNudgeX, KeyBlackY), module, PhraseSeq32Ex::KEY_PARAMS + 10, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(206+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 10 * 2));
		// White keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(51+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 0, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(51+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 0 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(79+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 2, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(79+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 2 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(107+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 4, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(107+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 4 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(136+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 5, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(136+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 5 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(164+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 7, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(164+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 7 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(192+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 9, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(192+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 9 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(220+keyNudgeX, KeyWhiteY), module, PhraseSeq32Ex::KEY_PARAMS + 11, 0.0, 1.0, 0.0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(220+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32Ex::KEY_LIGHTS + 11 * 2));
		
		// Key mode LED buttons	
		static const int colRulerKM = 265;
		addParam(createParam<LEDButton>(Vec(colRulerKM, KeyBlackY + offsetKeyLEDy - 4.4f), module, PhraseSeq32Ex::KEYNOTE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MediumLight<RedLight>>(Vec(colRulerKM + 4.4f,  KeyBlackY + offsetKeyLEDy), module, PhraseSeq32Ex::KEYNOTE_LIGHT));
		addParam(createParam<LEDButton>(Vec(colRulerKM, KeyWhiteY + offsetKeyLEDy - 4.4f), module, PhraseSeq32Ex::KEYGATE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerKM + 4.4f,  KeyWhiteY + offsetKeyLEDy), module, PhraseSeq32Ex::KEYGATE_LIGHT));

		
		
		// ****** Right side control area ******

		static const int rowRulerMK0 = 101;// Edit mode row
		static const int rowRulerMK1 = rowRulerMK0 + 56; // Run row
		static const int rowRulerMK2 = rowRulerMK1 + 54; // Copy-paste Tran/rot row
		static const int columnRulerMK0 = 305;// Edit mode column
		static const int columnRulerMK2 = columnRulerT4;// Mode/Len column
		static const int columnRulerMK1 = columnRulerMK0 + 74;// Display column
		
		// Edit mode switch
		addParam(createParam<CKSS>(Vec(columnRulerMK0 + 2 + hOffsetCKSS, rowRulerMK0 + vOffsetCKSS), module, PhraseSeq32Ex::EDIT_PARAM, 0.0f, 1.0f, 1.0f));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(columnRulerMK1-15, rowRulerMK0 + 3 + vOffsetDisplay);
		displaySequence->box.size = Vec(55, 30);// 3 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Len/mode button
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK0 + 0 + offsetCKD6b), module, PhraseSeq32Ex::RUNMODE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLight<SmallLight<RedLight>>(Vec(columnRulerMK2 + offsetCKD6b + 24, rowRulerMK0 + 0 + offsetCKD6b + 31), module, PhraseSeq32Ex::RES_LIGHT));

		// Run LED bezel and light
		addParam(createParam<LEDBezel>(Vec(columnRulerMK0 + 3 + offsetLEDbezel, rowRulerMK1 + 7 + offsetLEDbezel), module, PhraseSeq32Ex::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MuteLight<GreenLight>>(Vec(columnRulerMK0 + 3 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK1 + 7 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq32Ex::RUN_LIGHT));
		// Sequence knob
		addParam(createDynamicParam<IMBigKnobInf>(Vec(columnRulerMK1 + 1 + offsetIMBigKnob, rowRulerMK0 + 55 + offsetIMBigKnob), module, PhraseSeq32Ex::SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, &module->panelTheme));		
		// Transpose/rotate button
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK1 + 4 + offsetCKD6b), module, PhraseSeq32Ex::TRAN_ROT_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// Reset LED bezel and light
		addParam(createParam<LEDBezel>(Vec(columnRulerMK0 + 3 + offsetLEDbezel, rowRulerMK2 + 5 + offsetLEDbezel), module, PhraseSeq32Ex::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(createLight<MuteLight<GreenLight>>(Vec(columnRulerMK0 + 3 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK2 + 5 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq32Ex::RESET_LIGHT));
		// Copy/paste buttons
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerMK1 - 10, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq32Ex::COPY_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerMK1 + 20, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq32Ex::PASTE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Copy-paste mode switch (3 position)
		addParam(createParam<CKSSThreeInv>(Vec(columnRulerMK2 + hOffsetCKSS + 1, rowRulerMK2 - 3 + vOffsetCKSSThree), module, PhraseSeq32Ex::CPMODE_PARAM, 0.0f, 2.0f, 2.0f));	// 0.0f is top position

		
		
		// ****** Gate and slide section ******
		
		static const int rowRulerMB0 = 218;
		static const int columnRulerMB1 = 56;// Gate 
		static const int columnRulerMBspacing = 62;
		static const int columnRulerMB2 = columnRulerMB1 + columnRulerMBspacing;// Tie
		static const int columnRulerMB3 = columnRulerMB2 + columnRulerMBspacing;// GateP
		static const int columnRulerMB4 = columnRulerMB3 + columnRulerMBspacing;// Slide
		static const int posLEDvsButton = + 25;
		
		// Gate 1 light and button
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(columnRulerMB1 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + offsetMediumLight), module, PhraseSeq32Ex::GATE_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB1 + offsetCKD6b, rowRulerMB0 + offsetCKD6b), module, PhraseSeq32Ex::GATE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Tie light and button
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerMB2 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + offsetMediumLight), module, PhraseSeq32Ex::TIE_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB2 + offsetCKD6b, rowRulerMB0 + offsetCKD6b), module, PhraseSeq32Ex::TIE_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Gate 1 probability light and button
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerMB3 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + offsetMediumLight), module, PhraseSeq32Ex::GATE_PROB_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB3 + offsetCKD6b, rowRulerMB0 + offsetCKD6b), module, PhraseSeq32Ex::GATE_PROB_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// Slide light and button
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerMB4 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + offsetMediumLight), module, PhraseSeq32Ex::SLIDE_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB4 + offsetCKD6b, rowRulerMB0 + offsetCKD6b), module, PhraseSeq32Ex::SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));

						
		
		// ****** Bottom two rows ******
		
		static const int inputJackSpacingX = 53;
		static const int outputJackSpacingX = 45;
		static const int rowRulerB0 = 323;
		static const int rowRulerB1 = 269;
		static const int columnRulerB0 = 18;
		static const int columnRulerB1 = columnRulerB0 + inputJackSpacingX;
		static const int columnRulerB2 = columnRulerB1 + inputJackSpacingX;
		static const int columnRulerB3 = columnRulerB2 + inputJackSpacingX;
		static const int columnRulerB4 = columnRulerB3 + inputJackSpacingX;
		static const int columnRulerB5 = columnRulerB4 + inputJackSpacingX;// clock and reset
		
		
		
		
		static const int columnRulerB9 = columnRulerMK2 + 6;// gate track 2
		static const int columnRulerB8 = columnRulerB9 - outputJackSpacingX;// cv track 2
		static const int columnRulerB7 = columnRulerB8 - outputJackSpacingX;// gate track 1
		static const int columnRulerB6 = columnRulerB7 - outputJackSpacingX;// cv track 1

		
		// CV inputs
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB3, rowRulerB0), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB4, rowRulerB0), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 1, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB3, rowRulerB1), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 3, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB4, rowRulerB1), Port::INPUT, module, PhraseSeq32Ex::CV_INPUTS + 4, &module->panelTheme));
		// Clock input
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB5, rowRulerB1), Port::INPUT, module, PhraseSeq32Ex::CLOCK_INPUT, &module->panelTheme));
		// CV+Gate outputs
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB6, rowRulerB1), Port::OUTPUT, module, PhraseSeq32Ex::CV_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB7, rowRulerB1), Port::OUTPUT, module, PhraseSeq32Ex::CV_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB6, rowRulerB0), Port::OUTPUT, module, PhraseSeq32Ex::CV_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB7, rowRulerB0), Port::OUTPUT, module, PhraseSeq32Ex::CV_OUTPUTS + 3, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB8, rowRulerB1), Port::OUTPUT, module, PhraseSeq32Ex::GATE_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB9, rowRulerB1), Port::OUTPUT, module, PhraseSeq32Ex::GATE_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB8, rowRulerB0), Port::OUTPUT, module, PhraseSeq32Ex::GATE_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB9, rowRulerB0), Port::OUTPUT, module, PhraseSeq32Ex::GATE_OUTPUTS + 3, &module->panelTheme));


		// CV control Inputs 
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB0, rowRulerB0), Port::INPUT, module, PhraseSeq32Ex::LEFTCV_INPUT, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB1, rowRulerB0), Port::INPUT, module, PhraseSeq32Ex::RIGHTCV_INPUT, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB0, rowRulerB1), Port::INPUT, module, PhraseSeq32Ex::SEQCV_INPUT, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB1, rowRulerB1), Port::INPUT, module, PhraseSeq32Ex::RUNCV_INPUT, &module->panelTheme));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB2, rowRulerB0), Port::INPUT, module, PhraseSeq32Ex::WRITE_INPUT, &module->panelTheme));
		// Autostep
		addParam(createParam<CKSS>(Vec(columnRulerB2 + hOffsetCKSS, rowRulerB1 + vOffsetCKSS), module, PhraseSeq32Ex::AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f));		
		// Reset input
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB5, rowRulerB0), Port::INPUT, module, PhraseSeq32Ex::RESET_INPUT, &module->panelTheme));
		
		
		// Expansion module
		static const int rowRulerExpTop = 78;
		static const int rowSpacingExp = 60;
		static const int colRulerExp = 540;
		addInput(expPorts[0] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 0), Port::INPUT, module, PhraseSeq32Ex::GATECV_INPUT, &module->panelTheme));
		addInput(expPorts[1] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 1), Port::INPUT, module, PhraseSeq32Ex::GATEPCV_INPUT, &module->panelTheme));
		addInput(expPorts[2] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 2), Port::INPUT, module, PhraseSeq32Ex::TIEDCV_INPUT, &module->panelTheme));
		addInput(expPorts[3] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 3), Port::INPUT, module, PhraseSeq32Ex::SLIDECV_INPUT, &module->panelTheme));
		addInput(expPorts[4] = createDynamicPortCentered<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 4), Port::INPUT, module, PhraseSeq32Ex::MODECV_INPUT, &module->panelTheme));
	}
};

Model *modelPhraseSeq32Ex = Model::create<PhraseSeq32Ex, PhraseSeq32ExWidget>("Impromptu Modular", "Phrase-Seq-32Ex", "SEQ - Phrase-Seq-32Ex", SEQUENCER_TAG);

/*CHANGE LOG

0.6.13:
created

*/
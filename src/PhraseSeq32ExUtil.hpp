//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"

using namespace rack;


int moveIndexEx(int index, int indexNext, int numSteps);
	
	
struct Attribute {
	// Attributes of a step
	unsigned long attribute;
	
	static const unsigned long ATT_MSK_GATE = 0x01;
	static const unsigned long ATT_MSK_GATEP = 0x02;
	static const unsigned long ATT_MSK_SLIDE = 0x04;
	static const unsigned long ATT_MSK_TIED = 0x08;
	static const unsigned long ATT_MSK_GATETYPE = 0xF0;
	static const unsigned long gate1TypeShift = 4;
	static const unsigned long ATT_MSK_GATEP_VAL = 0xFF00;
	static const unsigned long GatePValShift = 8;
	static const unsigned long ATT_MSK_SLIDE_VAL = 0xFF0000;
	static const unsigned long slideValShift = 16;
	static const unsigned long ATT_MSK_INITSTATE = (ATT_MSK_GATE | (0 << gate1TypeShift) | (50 << GatePValShift) | (10 << slideValShift));

	inline void init() {attribute = ATT_MSK_INITSTATE;}
	inline void randomize(int numGateTypes) {attribute =  ((randomu32() & 0xF) & ((randomu32() % numGateTypes) << gate1TypeShift) & ((randomu32() % 101) << GatePValShift) & ((randomu32() % 101) << slideValShift));}
	
	inline bool getGate() {return (attribute & ATT_MSK_GATE) != 0;}
	inline bool getTied() {return (attribute & ATT_MSK_TIED) != 0;}
	inline bool getGateP() {return (attribute & ATT_MSK_GATEP) != 0;}
	inline int getGatePVal() {return (int)((attribute & ATT_MSK_GATEP_VAL) >> GatePValShift);}
	inline bool getSlide() {return (attribute & ATT_MSK_SLIDE) != 0;}
	inline int getSlideVal() {return (int)((attribute & ATT_MSK_SLIDE_VAL) >> slideValShift);}
	inline int getGateType() {return ((int)(attribute & ATT_MSK_GATETYPE) >> gate1TypeShift);}

	inline void setGate(bool gate1State) {attribute &= ~ATT_MSK_GATE; if (gate1State) attribute |= ATT_MSK_GATE;}
	inline void setTied(bool tiedState) {attribute &= ~ATT_MSK_TIED; if (tiedState) attribute |= ATT_MSK_TIED;}
	inline void setGateP(bool GatePState) {attribute &= ~ATT_MSK_GATEP; if (GatePState) attribute |= ATT_MSK_GATEP;}
	inline void setGatePVal(int gatePval) {attribute &= ~ATT_MSK_GATEP_VAL; attribute |= (((unsigned long)gatePval) << GatePValShift);}
	inline void setSlide(bool slideState) {attribute &= ~ATT_MSK_SLIDE; if (slideState) attribute |= ATT_MSK_SLIDE;}
	inline void setSlideVal(int slideVal) {attribute &= ~ATT_MSK_SLIDE_VAL; attribute |= (((unsigned long)slideVal) << slideValShift);}
	inline void setGateType(int gate1Type) {attribute &= ~ATT_MSK_GATETYPE; attribute |= (((unsigned long)gate1Type) << gate1TypeShift);}

	inline void toggleGate() {attribute ^= ATT_MSK_GATE;}
	inline void toggleTied() {attribute ^= ATT_MSK_TIED;}
	inline void toggleGateP() {attribute ^= ATT_MSK_GATEP;}
	inline void toggleSlide() {attribute ^= ATT_MSK_SLIDE;}	
	
	inline void applyTied() {attribute &= 0xF; attribute |= ATT_MSK_TIED;}// clear other attributes if tied
};



struct SequencerKernel {
	// General constants
	// ----------------

	// Sequencer dimensions
	static const int MAX_STEPS = 32;
	static const int MAX_SEQS = 32;
	static const int MAX_PHRASES = 32;

	// Clock resolution										
	static const int NUM_PPS_VALUES = 33;
	static const int ppsValues[NUM_PPS_VALUES];

	// Run modes
	enum RunModeIds {MODE_FWD, MODE_REV, MODE_PPG, MODE_PEN, MODE_BRN, MODE_RND, NUM_MODES};
	static const std::string modeLabels[NUM_MODES];
	
	// Gate types
	static const int NUM_GATES = 12;	
	static const uint64_t advGateHitMaskLow[NUM_GATES];		
	static const uint64_t advGateHitMaskHigh[NUM_GATES];		


	
	// Member data
	// ----------------
	
	int id;
	std::string ids;
	
	// Need to save
	int runModeSong;	
	int runModeSeq[MAX_SEQS];
	int pulsesPerStepIndex;// 0 to NUM_PPS_VALUES-1; 0 means normal gate mode of 1 pps; this is an index into ppsValues[]
	int phrases;// number of phrases (song steps), min value is 1
	int phraseReps[MAX_PHRASES];// a rep is 1 to 99
	int phrase[MAX_PHRASES];// This is the song (series of phases; a phrase is a sequence number)	
	int lengths[MAX_SEQS];// number of steps in each sequence, min value is 1
	float cv[MAX_SEQS][MAX_STEPS];// [-3.0 : 3.917]. First index is sequence number, 2nd index is step
	Attribute attributes[MAX_SEQS][MAX_STEPS];// First index is sequence number, 2nd index is step
	int transposeOffsets[MAX_SEQS];
	
	// No need to save
	int stepIndexRun;
	unsigned long stepIndexRunHistory;
	int phraseIndexRun;
	unsigned long phraseIndexRunHistory;
	int ppqnCount;
	int gateCode;
	unsigned long slideStepsRemain;// 0 when no slide under way, downward step counter when sliding
	float slideCVdelta;// no need to initialize, this is only used when slideStepsRemain is not 0
	
	
	
	// Get, set, mod
	// ----------------
	
	// get
	inline int getRunModeSong() {return runModeSong;}
	inline int getRunModeSeq(int seqn) {return runModeSeq[seqn];}
	inline int getPhrases() {return phrases;}
	inline int getLength(int seqn) {return lengths[seqn];}
	inline int getPhrase(int phrn) {return phrase[phrn];}
	inline int getPhraseReps(int phrn) {return phraseReps[phrn];}
	inline int getPulsesPerStep() {return ppsValues[pulsesPerStepIndex];}
	inline int getTransposeOffset(int seqn) {return transposeOffsets[seqn];}
	inline int getStepIndexRun() {return stepIndexRun;}
	inline int getPhraseIndexRun() {return phraseIndexRun;}
	inline float getCV(int seqn, int stepn) {return cv[seqn][stepn];}
	inline Attribute getAttribute(int seqn, int stepn) {return attributes[seqn][stepn];}
	inline bool getGate(int seqn, int stepn) {return attributes[seqn][stepn].getGate();}
	inline bool getTied(int seqn, int stepn) {return attributes[seqn][stepn].getTied();}
	inline bool getGateP(int seqn, int stepn) {return attributes[seqn][stepn].getGateP();}
	inline int getGatePVal(int seqn, int stepn) {return attributes[seqn][stepn].getGatePVal();}
	inline bool getSlide(int seqn, int stepn) {return attributes[seqn][stepn].getSlide();}
	inline int getSlideVal(int seqn, int stepn) {return attributes[seqn][stepn].getSlideVal();}
	inline int getGateType(int seqn, int stepn) {return attributes[seqn][stepn].getGateType();}
	
	
	// set
	inline void setRunModeSeq(int seqn, int runmode) {runModeSeq[seqn] = runmode;}
	inline void setPhrases(int _phrases) {phrases = _phrases;}
	inline void setLength(int seqn, int _length) {lengths[seqn] = _length;}
	inline void setPhrase(int phrn, int seqn) {phrase[phrn] = seqn;}
	inline void setSlideStepsRemainAndCVdelta(unsigned long _slideStepsRemain, float cvdelta) {
		slideStepsRemain = _slideStepsRemain; 
		slideCVdelta = cvdelta/(float)slideStepsRemain;
	}
	inline void setStepIndexRun(int _stepIndexRun) {stepIndexRun = _stepIndexRun;}
	inline void setPhraseIndexRun(int _phraseIndexRun) {phraseIndexRun = _phraseIndexRun;}
	inline void setCV(int seqn, int stepn, float _cv) {cv[seqn][stepn] = _cv;}
	inline void setAttribute(int seqn, int stepn, Attribute _attribute) {attributes[seqn][stepn] = _attribute;}
	inline void setGate(int seqn, int stepn, bool gateState) {attributes[seqn][stepn].setGate(gateState);}
	inline void setGateP(int seqn, int stepn, bool GatePstate) {attributes[seqn][stepn].setGateP(GatePstate);}
	inline void setGatePVal(int seqn, int stepn, int gatePval) {attributes[seqn][stepn].setGatePVal(gatePval);}
	inline void setSlide(int seqn, int stepn, bool slideState) {attributes[seqn][stepn].setSlide(slideState);}
	inline void setSlideVal(int seqn, int stepn, int slideVal) {attributes[seqn][stepn].setSlideVal(slideVal);}
	inline void setGateType(int seqn, int stepn, int gateType) {attributes[seqn][stepn].setGateType(gateType);}

	
	// mod, dec, toggle
	inline void modRunModeSong(int delta) {runModeSong += delta; if (runModeSong < 0) runModeSong = 0; if (runModeSong >= NUM_MODES) runModeSong = NUM_MODES - 1;}
	inline void modRunModeSeq(int seqn, int delta) {
		runModeSeq[seqn] += delta;
		if (runModeSeq[seqn] < 0) runModeSeq[seqn] = 0;
		if (runModeSeq[seqn] >= NUM_MODES) runModeSeq[seqn] = NUM_MODES - 1;
	}
	inline void modPhrases(int delta) {phrases += delta; if (phrases > MAX_PHRASES) phrases = MAX_PHRASES; if (phrases < 1 ) phrases = 1;}
	inline void modLength(int seqn, int delta) {
		lengths[seqn] += delta; 
		if (lengths[seqn] > MAX_STEPS) lengths[seqn] = MAX_STEPS; 
		if (lengths[seqn] < 1 ) lengths[seqn] = 1;
	}
	inline void modPhrase(int phrn, int delta) {
		int newPhrase = phrase[phrn] + delta;
		if (newPhrase < 0)
			newPhrase += (1 - newPhrase / MAX_SEQS) * MAX_SEQS;// newPhrase now positive
		newPhrase = newPhrase % MAX_SEQS;
		phrase[phrn] = newPhrase;
	}
	inline void modPhraseReps(int phrn, int delta) {
		int newPhraseReps = phraseReps[phrn] + delta - 1;
		if (newPhraseReps < 0)
			newPhraseReps += (1 - newPhraseReps / 99) * 99;// newPhraseReps now positive
		newPhraseReps = newPhraseReps % 99;
		phraseReps[phrn] = newPhraseReps + 1;
	}		
	inline void modPulsesPerStepIndex(int delta) {
		pulsesPerStepIndex += delta;
		if (pulsesPerStepIndex < 0) pulsesPerStepIndex = 0;
		if (pulsesPerStepIndex >= NUM_PPS_VALUES) pulsesPerStepIndex = NUM_PPS_VALUES - 1;
	}
	inline void modTransposeOffset(int seqn, int delta) {
		transposeOffsets[seqn] += delta;
		if (transposeOffsets[seqn] > 99) transposeOffsets[seqn] = 99;
		if (transposeOffsets[seqn] < -99) transposeOffsets[seqn] = -99;						
	}
	inline void modGatePVal(int seqn, int stepn, int delta) {
		int pVal = getGatePVal(seqn, stepn);
		pVal += delta;
		if (pVal > 100) pVal = 100;
		if (pVal < 0) pVal = 0;
		setGatePVal(seqn, stepn, pVal);						
	}		
	inline void modSlideVal(int seqn, int stepn, int delta) {
		int sVal = getSlideVal(seqn, stepn);
		sVal += delta;
		if (sVal > 100) sVal = 100;
		if (sVal < 0) sVal = 0;
		setSlideVal(seqn, stepn, sVal);						
	}		
	inline void decSlideStepsRemain() {if (slideStepsRemain > 0ul) slideStepsRemain--;}	
	inline void toggleGate(int seqn, int stepn) {attributes[seqn][stepn].toggleGate();}
	inline void toggleTied(int seqn, int stepn) {// will clear other attribs if new state is on
		attributes[seqn][stepn].toggleTied();
		if (attributes[seqn][stepn].getTied()) {
			attributes[seqn][stepn].setGate(false);
			attributes[seqn][stepn].setGateP(false);
			attributes[seqn][stepn].setSlide(false);
			applyTiedStep(seqn, stepn);
		}
	}
	inline void toggleGateP(int seqn, int step) {attributes[seqn][step].toggleGateP();}
	inline void toggleSlide(int seqn, int step) {attributes[seqn][step].toggleSlide();}	
	
	
	// init
	inline void initSequence(int seqn) {
		for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
			cv[seqn][stepn] = 0.0f;
			attributes[seqn][stepn].init();
		}
		transposeOffsets[seqn] = 0;
	}
	inline void initSong() {
		for (int phrn = 0; phrn < MAX_PHRASES; phrn++) {
			phrase[phrn] = 0;
			phraseReps[phrn] = 1;
		}
	}

	// randomize and staircase
	inline void randomizeGates(int seqn) {
		for (int stepn = 0; stepn < MAX_STEPS; stepn++)
			if ( (randomu32() & 0x1) != 0)
				toggleGate(seqn, stepn);
	}
	inline void randomizeAttributes(int seqn) {// uses lengths[] so make sure that is set up first
		for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
			attributes[seqn][stepn].randomize(NUM_GATES);
			if (getTied(seqn,stepn)) {
				attributes[seqn][stepn].applyTied();
				applyTiedStep(seqn, stepn);
			}
		}
	}
	inline void randomizeCVs(int seqn) {
		for (int stepn = 0; stepn < MAX_STEPS; stepn++)
			cv[seqn][stepn] = ((float)(randomu32() % 7)) + ((float)(randomu32() % 12)) / 12.0f - 3.0f;
		transposeOffsets[seqn] = 0;
	}
	inline void randomizeSequence(int seqn) {
		randomizeCVs(seqn);
		randomizeAttributes(seqn);
	}
	inline void randomizeSong() {
		for (int phrn = 0; phrn < MAX_PHRASES; phrn++) {
			phrase[phrn] = randomu32() % MAX_PHRASES;
			phraseReps[phrn] = randomu32() % 4 + 1;
		}
	}
	inline void staircaseSong() {
		for (int phrn = 0; phrn < MAX_PHRASES; phrn++) {
			phrase[phrn] = phrn;
			phraseReps[phrn] = 1;	
		}			
	}		

	// copy-paste sequence or song
	inline void copySequence(float* cvCPbuffer, Attribute* attribCPbuffer, int* lengthCPbuffer, int* modeCPbuffer, int seqn, int startCP, int countCP) {
		for (int i = 0, stepn = startCP; i < countCP; i++, stepn++) {
			cvCPbuffer[i] = cv[seqn][stepn];
			attribCPbuffer[i] = attributes[seqn][stepn];
		}
		*lengthCPbuffer = lengths[seqn];
		*modeCPbuffer = runModeSeq[seqn];
	}
	inline void copyPhrase(int* phraseCPbuffer, int* repCPbuffer, int* lengthCPbuffer, int startCP, int countCP) {				
		for (int i = 0, phrn = startCP; i < countCP; i++, phrn++) {
			phraseCPbuffer[i] = phrase[phrn];
			repCPbuffer[i] = phraseReps[phrn];
		}
		*lengthCPbuffer = -1;// so that a cross paste can be detected
	}
	inline void pasteSequence(float* cvCPbuffer, Attribute* attribCPbuffer, int* lengthCPbuffer, int* modeCPbuffer, int seqn, int startCP, int countCP) {
		for (int i = 0, stepn = startCP; i < countCP; i++, stepn++) {
			cv[seqn][stepn] = cvCPbuffer[i];
			attributes[seqn][stepn] = attribCPbuffer[i];
		}
		if (countCP == MAX_STEPS) {// all
			lengths[seqn] = *lengthCPbuffer;
			runModeSeq[seqn] = *modeCPbuffer;
			transposeOffsets[seqn] = 0;
		}
	}
	inline void pastePhrase(int* phraseCPbuffer, int* repCPbuffer, int startCP, int countCP) {	
		for (int i = 0, phrn = startCP; i < countCP; i++, phrn++) {
			phrase[phrn] = phraseCPbuffer[i];
			phraseReps[phrn] = repCPbuffer[i];
		}
	}
	
	
	// Main methods
	// ----------------
	
	void setId(int _id) {
		id = _id;
		ids = "id" + std::to_string(id) + "_";
	}
	
	
	void reset() {
		pulsesPerStepIndex = 0;
		runModeSong = MODE_FWD;
		phrases = 4;
		initSong();
		for (int seqn = 0; seqn < MAX_SEQS; seqn++) {
			runModeSeq[seqn] = MODE_FWD;
			lengths[seqn] = MAX_SEQS;			
			initSequence(seqn);		
		}
		slideStepsRemain = 0ul;
		// no need to call initRun() here since user of the kernel does it in its onReset() via its initRun()
	}
	
	
	void randomize() {
		runModeSong = randomu32() % 5;
		phrases = 1 + (randomu32() % MAX_PHRASES);
		randomizeSong();
		for (int seqn = 0; seqn < MAX_SEQS; seqn++) {
			runModeSeq[seqn] = randomu32() % NUM_MODES;
			lengths[seqn] = 1 + (randomu32() % MAX_SEQS);
			randomizeSequence(seqn);// uses lengths[] so must be after lengths[] randomized
		}
		// no need to call initRun() here since user of the kernel does it in its onRandomize() via its initRun()
	}
	
	
	void initRun(bool hard, bool isEditingSequence, int sequence) {
		if (hard) {
			phraseIndexRun = (runModeSong == MODE_REV ? phrases - 1 : 0);
			phraseIndexRunHistory = 0;
		}
		int seqn = (isEditingSequence ? sequence : phrase[phraseIndexRun]);
		if (hard) {
			stepIndexRun = (runModeSeq[seqn] == MODE_REV ? lengths[seqn] - 1 : 0);
			stepIndexRunHistory = 0;
		}
		ppqnCount = 0;
		calcGateCodeEx(seqn);// uses stepIndexRun as the step
		slideStepsRemain = 0ul;
	}
	
	
	void toJson(json_t *rootJ) {
		// pulsesPerStepIndex
		json_object_set_new(rootJ, (ids + "pulsesPerStepIndex").c_str(), json_integer(pulsesPerStepIndex));

		// runModeSong
		json_object_set_new(rootJ, (ids + "runModeSong").c_str(), json_integer(runModeSong));

		// runModeSeq
		json_t *runModeSeqJ = json_array();
		for (int i = 0; i < MAX_SEQS; i++)
			json_array_insert_new(runModeSeqJ, i, json_integer(runModeSeq[i]));
		json_object_set_new(rootJ, (ids + "runModeSeq").c_str(), runModeSeqJ);

		// phrases
		json_object_set_new(rootJ, (ids + "phrases").c_str(), json_integer(phrases));
		
		// phraseReps 
		json_t *phraseRepsJ = json_array();
		for (int i = 0; i < MAX_PHRASES; i++)
			json_array_insert_new(phraseRepsJ, i, json_integer(phraseReps[i]));
		json_object_set_new(rootJ, (ids + "phraseReps").c_str(), phraseRepsJ);

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < MAX_PHRASES; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, (ids + "phrase").c_str(), phraseJ);

		// CV
		json_t *cvJ = json_array();
		for (int seqn = 0; seqn < MAX_SEQS; seqn++)
			for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
				json_array_insert_new(cvJ, stepn + (seqn * MAX_STEPS), json_real(cv[seqn][stepn]));
			}
		json_object_set_new(rootJ, (ids + "cv").c_str(), cvJ);

		// attributes
		json_t *attributesJ = json_array();
		for (int seqn = 0; seqn < MAX_SEQS; seqn++)
			for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
				json_array_insert_new(attributesJ, stepn + (seqn * MAX_STEPS), json_integer(attributes[seqn][stepn].attribute));
			}
		json_object_set_new(rootJ, (ids + "attributes").c_str(), attributesJ);

		// lengths
		json_t *lengthsJ = json_array();
		for (int i = 0; i < MAX_SEQS; i++)
			json_array_insert_new(lengthsJ, i, json_integer(lengths[i]));
		json_object_set_new(rootJ, (ids + "lengths").c_str(), lengthsJ);
		
		// transposeOffsets
		json_t *transposeOffsetsJ = json_array();
		for (int i = 0; i < MAX_SEQS; i++)
			json_array_insert_new(transposeOffsetsJ, i, json_integer(transposeOffsets[i]));
		json_object_set_new(rootJ, (ids + "transposeOffsets").c_str(), transposeOffsetsJ);

	}
	
	
	void fromJson(json_t *rootJ) {
		// pulsesPerStepIndex
		json_t *pulsesPerStepIndexJ = json_object_get(rootJ, (ids + "pulsesPerStepIndex").c_str());
		if (pulsesPerStepIndexJ)
			pulsesPerStepIndex = json_integer_value(pulsesPerStepIndexJ);

		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, (ids + "runModeSong").c_str());
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
				
		// runModeSeq
		json_t *runModeSeqJ = json_object_get(rootJ, (ids + "runModeSeq").c_str());
		if (runModeSeqJ) {
			for (int i = 0; i < MAX_SEQS; i++)
			{
				json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
				if (runModeSeqArrayJ)
					runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
			}			
		}		
		
		// phrases	
		json_t *phrasesJ = json_object_get(rootJ, (ids + "phrases").c_str());
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// phraseReps
		json_t *phraseRepsJ = json_object_get(rootJ, (ids + "phraseReps").c_str());
		if (phraseRepsJ)
			for (int i = 0; i < MAX_PHRASES; i++)
			{
				json_t *phraseRepsArrayJ = json_array_get(phraseRepsJ, i);
				if (phraseRepsArrayJ)
					phraseReps[i] = json_integer_value(phraseRepsArrayJ);
			}

		// phrase
		json_t *phraseJ = json_object_get(rootJ, (ids + "phrase").c_str());
		if (phraseJ)
			for (int i = 0; i < MAX_PHRASES; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
		
		// CV
		json_t *cvJ = json_object_get(rootJ, (ids + "cv").c_str());
		if (cvJ) {
			for (int seqn = 0; seqn < MAX_SEQS; seqn++)
				for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
					json_t *cvArrayJ = json_array_get(cvJ, stepn + (seqn * MAX_STEPS));
					if (cvArrayJ)
						cv[seqn][stepn] = json_number_value(cvArrayJ);
				}
		}

		// attributes
		json_t *attributesJ = json_object_get(rootJ, (ids + "attributes").c_str());
		if (attributesJ) {
			for (int seqn = 0; seqn < MAX_SEQS; seqn++)
				for (int stepn = 0; stepn < MAX_STEPS; stepn++) {
					json_t *attributesArrayJ = json_array_get(attributesJ, stepn + (seqn * MAX_STEPS));
					if (attributesArrayJ)
						attributes[seqn][stepn].attribute = json_integer_value(attributesArrayJ);
				}
		}

		// lengths
		json_t *lengthsJ = json_object_get(rootJ, (ids + "lengths").c_str());
		if (lengthsJ)
			for (int i = 0; i < MAX_SEQS; i++)
			{
				json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
				if (lengthsArrayJ)
					lengths[i] = json_integer_value(lengthsArrayJ);
			}

		// transposeOffsets
		json_t *transposeOffsetsJ = json_object_get(rootJ, (ids + "transposeOffsets").c_str());
		if (transposeOffsetsJ) {
			for (int i = 0; i < MAX_SEQS; i++)
			{
				json_t *transposeOffsetsArrayJ = json_array_get(transposeOffsetsJ, i);
				if (transposeOffsetsArrayJ)
					transposeOffsets[i] = json_integer_value(transposeOffsetsArrayJ);
			}			
		}		
	}
	
	
	inline void transposeSeq(int seqNum, float offsetCV) {
		for (int stepn = 0; stepn < MAX_STEPS; stepn++) 
			cv[seqNum][stepn] += offsetCV;
	}

	
	void rotateSeq(int seqNum, bool directionRight) {
		float rotCV;
		Attribute rotAttributes;
		int iStart = 0;
		int iEnd = lengths[seqNum] - 1;
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

	
	void applyTiedStep(int seqNum, int indexTied) {
		int seqLength = lengths[seqNum];
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
	
	
	inline float calcSlideOffset() {return (slideStepsRemain > 0ul ? (slideCVdelta * (float)slideStepsRemain) : 0.0f);}
	

	inline bool incPpqnCountAndCmpWithZero() {
		ppqnCount++;
		if (ppqnCount >= ppsValues[pulsesPerStepIndex])
			ppqnCount = 0;
		return ppqnCount == 0;
	}

	
	inline bool calcGate(SchmittTrigger clockTrigger, unsigned long clockStep, float sampleRate) {
		if (gateCode < 2) 
			return gateCode == 1;
		if (gateCode == 2)
			return clockTrigger.isHigh();
		return clockStep < (unsigned long) (sampleRate * 0.001f);
	}
	
	
	int keyIndexToGateTypeEx(int keyIndex) {
		int pulsesPerStep = ppsValues[pulsesPerStepIndex];
		if (((pulsesPerStep % 6) != 0) && (keyIndex == 1 || keyIndex == 3 || keyIndex == 6 || keyIndex == 8 || keyIndex == 10))// TRIs
			return -1;
		if (((pulsesPerStep % 4) != 0) && (keyIndex == 0 || keyIndex == 4 || keyIndex == 7 || keyIndex == 9))// DOUBLEs
			return -1;
		return keyIndex;// keyLight index now matches gate types, so no mapping table needed anymore
	}
	

	void calcGateCodeEx(int seqn) {// uses stepIndexRun as the step
		Attribute attribute = attributes[seqn][stepIndexRun];
		int gateType;

		if (gateCode != -1 || ppqnCount == 0) {// always calc on first ppqnCount, avoid thereafter if gate will be off for whole step
			// -1 = gate off for whole step, 0 = gate off for current ppqn, 1 = gate on, 2 = clock high, 3 = trigger
			if ( ppqnCount == 0 && attribute.getGateP() && !(randomUniform() < ((float)attribute.getGatePVal() / 100.0f)) ) {// randomUniform is [0.0, 1.0), see include/util/common.hpp
				gateCode = -1;// must do this first in this method since it will kill rest of step if prob turns off the step
			}
			else if (!attribute.getGate()) {
				gateCode = 0;
			}
			else if (pulsesPerStepIndex == 0) {
				gateCode = 2;// clock high pulse
			}
			else {
				gateType = attribute.getGateType();
				if (gateType == 11) {
					gateCode = (ppqnCount == 0 ? 3 : 0);
				}
				else {
					uint64_t shiftAmt = ppqnCount * (96 / ppsValues[pulsesPerStepIndex]);
					if (shiftAmt >= 64)
						gateCode = (int)((advGateHitMaskHigh[gateType] >> (shiftAmt - (uint64_t)64)) & (uint64_t)0x1);
					else
						gateCode = (int)((advGateHitMaskLow[gateType] >> shiftAmt) & (uint64_t)0x1);
				}
			}
		}
	}
	
	
	bool moveIndexRunMode(bool stepPhraseN, int numSteps, int runMode, int reps) {	
		// assert((reps * numSteps) <= 0xFFF); // for BRN and RND run modes, history is not a span count but a step count
		
		int* index;
		unsigned long* history;
		
		if (stepPhraseN) {// true when moving sequence, false when moving song
			index = &stepIndexRun;
			history = &stepIndexRunHistory;
		}
		else {
			index = &phraseIndexRun;
			history = &phraseIndexRunHistory;
		}
		
		bool crossBoundary = false;
		
		switch (runMode) {
		
			// history 0x0000 is reserved for reset
			
			case MODE_REV :// reverse; history base is 0x2000
				if ((*history) < 0x2001 || (*history) > 0x2FFF)
					(*history) = 0x2000 + reps;
				(*index)--;
				if ((*index) < 0) {
					(*index) = numSteps - 1;
					(*history)--;
					if ((*history) <= 0x2000)
						crossBoundary = true;
				}
			break;
			
			case MODE_PPG :// forward-reverse; history base is 0x3000
				if ((*history) < 0x3001 || (*history) > 0x3FFF) // even means going forward, odd means going reverse
					(*history) = 0x3000 + reps * 2;
				if (((*history) & 0x1) == 0) {// even so forward phase
					(*index)++;
					if ((*index) >= numSteps) {
						(*index) = numSteps - 1 ;
						(*history)--;
					}
				}
				else {// odd so reverse phase
					(*index)--;
					if ((*index) < 0) {
						(*index) = 0;
						(*history)--;
						if ((*history) <= 0x3000)
							crossBoundary = true;
					}
				}
			break;

			case MODE_PEN :// forward-reverse; history base is 0x4000
				if ((*history) < 0x4001 || (*history) > 0x4FFF) // even means going forward, odd means going reverse
					(*history) = 0x4000 + reps * 2;
				if (((*history) & 0x1) == 0) {// even so forward phase
					(*index)++;
					if ((*index) >= numSteps) {
						(*index) = numSteps - 2;
						(*history)--;
						if ((*index) < 1) {// if back at 0 after turnaround, then no reverse phase needed
							(*index) = 0;
							(*history)--;
							if ((*history) <= 0x4000)
								crossBoundary = true;
						}
					}
				}
				else {// odd so reverse phase
					(*index)--;
					if ((*index) < 1) {
						(*index) = 0;
						(*history)--;
						if ((*history) <= 0x4000)
							crossBoundary = true;
					}
				}
			break;
			
			case MODE_BRN :// brownian random; history base is 0x5000
				if ((*history) < 0x5001 || (*history) > 0x5FFF) 
					(*history) = 0x5000 + numSteps * reps;
				(*index) += (randomu32() % 3) - 1;
				if ((*index) >= numSteps) {
					(*index) = 0;
				}
				if ((*index) < 0) {
					(*index) = numSteps - 1;
				}
				(*history)--;
				if ((*history) <= 0x5000) {
					crossBoundary = true;
				}
			break;
			
			case MODE_RND :// random; history base is 0x6000
				if ((*history) < 0x6001 || (*history) > 0x6FFF) 
					(*history) = 0x6000 + numSteps * reps;
				(*index) = (randomu32() % numSteps) ;
				(*history)--;
				if ((*history) <= 0x6000) {
					crossBoundary = true;
				}
			break;
			
			default :// MODE_FWD  forward; history base is 0x1000
				if ((*history) < 0x1001 || (*history) > 0x1FFF)
					(*history) = 0x1000 + reps;
				(*index)++;
				if ((*index) >= numSteps) {
					(*index) = 0;
					(*history)--;
					if ((*history) <= 0x1000)
						crossBoundary = true;
				}
		}

		return crossBoundary;
	}
		
};// struct SequencerKernel 





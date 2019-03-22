//***********************************************************************************************
//Chain-able clock module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé, Nigel Sixsmith, Xavier Belmont and Steve Baker
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"


class Clock {
	// The -1.0 step is used as a reset state every double-period so that 
	//   lengths can be re-computed; it will stay at -1.0 when a clock is inactive.
	// a clock frame is defined as "length * iterations + syncWait", and
	//   for master, syncWait does not apply and iterations = 1

	
	double step;// -1.0 when stopped, [0 to 2*period[ for clock steps (*2 is because of swing, so we do groups of 2 periods)
	double length;// double period
	double sampleTime;
	int iterations;// run this many double periods before going into sync if sub-clock
	Clock* syncSrc = nullptr; // only subclocks will have this set to master clock
	static constexpr double guard = 0.0005;// in seconds, region for sync to occur right before end of length of last iteration; sub clocks must be low during this period
	bool *resetClockOutputsHigh;
	
	public:
	
	Clock() {
		reset();
	}
	
	inline void reset() {
		step = -1.0;
	}
	inline bool isReset() {
		return step == -1.0;
	}
	inline double getStep() {
		return step;
	}
	void setup(Clock* clkGiven, bool *resetClockOutputsHighPtr) {
		syncSrc = clkGiven;
		resetClockOutputsHigh = resetClockOutputsHighPtr;
	}
	inline void start() {
		step = 0.0;
	}
	
	inline void setup(double lengthGiven, int iterationsGiven, double sampleTimeGiven) {
		length = lengthGiven;
		iterations = iterationsGiven;
		sampleTime = sampleTimeGiven;
	}

	void stepClock() {// here the clock was output on step "step", this function is called at end of module::step()
		if (step >= 0.0) {// if active clock
			step += sampleTime;
			if ( (syncSrc != nullptr) && (iterations == 1) && (step > (length - guard)) ) {// if in sync region
				if (syncSrc->isReset()) {
					reset();
				}// else nothing needs to be done, just wait and step stays the same
			}
			else {
				if (step >= length) {// reached end iteration
					iterations--;
					step -= length;
					if (iterations <= 0) 
						reset();// frame done
				}
			}
		}
	}
	
	void applyNewLength(double lengthStretchFactor) {
		if (step != -1.0)
			step *= lengthStretchFactor;
		length *= lengthStretchFactor;
	}
	
	int isHigh(float swing, float pulseWidth) {
		// last 0.5ms (guard time) must be low so that sync mechanism will work properly (i.e. no missed pulses)
		//   this will automatically be the case, since code below disallows any pulses or inter-pulse times less than 1ms
		int high = 0;
		if (step >= 0.0) {
			float swParam = swing;// swing is [-1 : 1]
			
			// all following values are in seconds
			float onems = 0.001f;
			float period = (float)length / 2.0f;
			float swing = (period - 2.0f * onems) * swParam;
			float p2min = onems;
			float p2max = period - onems - fabsf(swing);
			if (p2max < p2min) {
				p2max = p2min;
			}
			
			//double p1 = 0.0;// implicit, no need 
			double p2 = (double)((p2max - p2min) * pulseWidth + p2min);// pulseWidth is [0 : 1]
			double p3 = (double)(period + swing);
			double p4 = ((double)(period + swing)) + p2;
			
			if (step < p2)
				high = 1;
			else if ((step >= p3) && (step < p4))
				high = 2;
		}
		else if (*resetClockOutputsHigh)
			high = 1;
		return high;
	}	
};


//*****************************************************************************


class ClockDelay {
	long stepCounter;
	int lastWriteValue;
	bool readState;
	long stepRise1;
	long stepFall1;
	long stepRise2;
	long stepFall2;
	
	public:
	
	ClockDelay() {
		reset(true);
	}
	
	void setup() {
	}
	
	void reset(bool resetClockOutputsHigh) {
		stepCounter = 0l;
		lastWriteValue = 0;
		readState = resetClockOutputsHigh;
		stepRise1 = 0l;
		stepFall1 = 0l;
		stepRise2 = 0l;
		stepFall2 = 0l;
	}
	
	void write(int value) {
		if (value == 1) {// first pulse is high
			if (lastWriteValue == 0) // if got rise 1
				stepRise1 = stepCounter;
		}
		else if (value == 2) {// second pulse is high
			if (lastWriteValue == 0) // if got rise 2
				stepRise2 = stepCounter;
		}
		else {// value = 0 (pulse is low)
			if (lastWriteValue == 1) // if got fall 1
				stepFall1 = stepCounter;
			else if (lastWriteValue == 2) // if got fall 2
				stepFall2 = stepCounter;
		}
		
		lastWriteValue = value;
	}
	
	bool read(long delaySamples) {
		long delayedStepCounter = stepCounter - delaySamples;
		if (delayedStepCounter == stepRise1 || delayedStepCounter == stepRise2)
			readState = true;
		else if (delayedStepCounter == stepFall1 || delayedStepCounter == stepFall2)
			readState = false;
		stepCounter++;
		if (stepCounter > 1e8) {// keep within long's bounds (could go higher or could allow negative)
			stepCounter -= 1e8;// 192000 samp/s * 2s * 64 * (3/4) = 18.4 Msamp
			stepRise1 -= 1e8;
			stepFall1 -= 1e8;
			stepRise2 -= 1e8;
			stepFall2 -= 1e8;
		}
		return readState;
	}
};


//*****************************************************************************


struct Clocked : Module {
	enum ParamIds {
		ENUMS(RATIO_PARAMS, 4),// master is index 0
		ENUMS(SWING_PARAMS, 4),// master is index 0
		ENUMS(PW_PARAMS, 4),// master is index 0
		RESET_PARAM,
		RUN_PARAM,
		ENUMS(DELAY_PARAMS, 4),// index 0 is unused
		BPMMODE_DOWN_PARAM,
		BPMMODE_UP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(PW_INPUTS, 4),// master is index 0
		RESET_INPUT,
		RUN_INPUT,
		BPM_INPUT,
		ENUMS(SWING_INPUTS, 4),// master is index 0
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CLK_OUTPUTS, 4),// master is index 0
		RESET_OUTPUT,
		RUN_OUTPUT,
		BPM_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RESET_LIGHT,
		RUN_LIGHT,
		ENUMS(CLK_LIGHTS, 4),// master is index 0 (not used)
		ENUMS(BPMSYNC_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	
	// Expander
	float consumerMessage[16] = {};// this module must read from here
	float producerMessage[16] = {};// expander will write into here
		

	// Constants
	const float delayValues[8] = {0.0f,  0.0625f, 0.125f, 0.25f, 1.0f/3.0f, 0.5f , 2.0f/3.0f, 0.75f};
	const float ratioValues[34] = {1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 23, 24, 29, 31, 32, 37, 41, 43, 47, 48, 53, 59, 61, 64};
	static const int bpmMax = 300;
	static const int bpmMin = 30;
	static constexpr float masterLengthMax = 120.0f / bpmMin;// a length is a double period
	static constexpr float masterLengthMin = 120.0f / bpmMax;// a length is a double period
	static constexpr float delayInfoTime = 3.0f;// seconds
	static constexpr float swingInfoTime = 2.0f;// seconds
	
	
	// Need to save
	int panelTheme = 0;
	bool displayDelayNoteMode;
	bool bpmDetectionMode;
	bool emitResetOnStopRun;
	int ppqn;
	bool running;
	bool resetClockOutputsHigh;

	
	// No need to save
	Clock clk[4];
	ClockDelay delay[3];// only channels 1 to 3 have delay
	bool syncRatios[4];// 0 index unused
	int ratiosDoubled[4];
	int extPulseNumber;// 0 to ppqn - 1
	double extIntervalTime;
	double timeoutTime;
	float newMasterLength;
	float masterLength;
	long editingBpmMode;// 0 when no edit bpmMode, downward step counter timer when edit, negative upward when show can't edit ("--") 
	float pulseWidth[4];
	float swingAmount[4];
	long delaySamples[4];
	double sampleRate;
	double sampleTime;
	
	bool scheduledReset = false;
	int notifyingSource[4] = {-1, -1, -1, -1};
	long notifyInfo[4] = {0l, 0l, 0l, 0l};// downward step counter when swing to be displayed, 0 when normal display
	long cantRunWarning = 0l;// 0 when no warning, positive downward step counter timer when warning
	unsigned int lightRefreshCounter = 0;
	float resetLight = 0.0f;
	Trigger resetTrigger;
	Trigger runTrigger;
	Trigger bpmDetectTrigger;
	Trigger bpmModeUpTrigger;
	Trigger bpmModeDownTrigger;
	dsp::PulseGenerator resetPulse;
	dsp::PulseGenerator runPulse;

	
	inline float getBpmKnob() {
		float knobBPM = params[RATIO_PARAMS + 0].getValue();// already integer BPM since using snap knob
		if (knobBPM < (float)bpmMin)// safety in case module step() starts before widget defaults take effect.
			return (float)bpmMin;	
		return knobBPM;
	}
	
	int getRatioDoubled(int ratioKnobIndex) {
		// ratioKnobIndex is 0 for master BPM's ratio (1 is implicitly returned), and 1 to 3 for other ratio knobs
		// returns a positive ratio for mult, negative ratio for div (0 never returned)
		if (ratioKnobIndex < 1) 
			return 1;
		bool isDivision = false;
		int i = (int) round( params[RATIO_PARAMS + ratioKnobIndex].getValue() );// [ -(numRatios-1) ; (numRatios-1) ]
		if (i < 0) {
			i *= -1;
			isDivision = true;
		}
		if (i >= 34) {
			i = 34 - 1;
		}
		int ret = (int) (ratioValues[i] * 2.0f + 0.5f);
		if (isDivision) 
			return -1l * ret;
		return ret;
	}
	
	void updatePulseSwingDelay() {
		for (int i = 0; i < 4; i++) {
			// Pulse Width
			pulseWidth[i] = params[PW_PARAMS + i].getValue();
			if (i < 3 && consumerMessage[i * 2 + 0] > 0.5f/*inputs[PW_INPUTS + i].isConnected()*/) {
				pulseWidth[i] += (consumerMessage[i * 2 + 1]/*inputs[PW_INPUTS + i].getVoltage()*/ / 10.0f) - 0.5f;
				pulseWidth[i] = clamp(pulseWidth[i], 0.0f, 1.0f);
			}
			
			// Swing
			swingAmount[i] = params[SWING_PARAMS + i].getValue();
			if (i < 3 && consumerMessage[(i + 4) * 2 + 0] > 0.5f/*inputs[SWING_INPUTS + i].isConnected()*/) {
				swingAmount[i] += (consumerMessage[(i + 4) * 2 + 1]/*inputs[SWING_INPUTS + i].getVoltage()*/ / 5.0f) - 1.0f;
				swingAmount[i] = clamp(swingAmount[i], -1.0f, 1.0f);
			}
		}

		// Delay
		delaySamples[0] = 0ul;
		for (int i = 1; i < 4; i++) {	
			int delayKnobIndex = (int)(params[DELAY_PARAMS + i].getValue() + 0.5f);
			float delayFraction = delayValues[delayKnobIndex];
			float ratioValue = ((float)ratiosDoubled[i]) / 2.0f;
			if (ratioValue < 0)
				ratioValue = 1.0f / (-1.0f * ratioValue);
			delaySamples[i] = (long)(masterLength * delayFraction * sampleRate / (ratioValue * 2.0));
		}				
	}
	
	
	// called from the main thread (step() can not be called until all modules created)
	Clocked() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		rightProducerMessage = producerMessage;
		rightConsumerMessage = consumerMessage;

		params[RATIO_PARAMS + 0].config((float)(bpmMin), (float)(bpmMax), 120.0f, "Main BPM");// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		params[RESET_PARAM].config(0.0f, 1.0f, 0.0f, "Reset");
		params[RUN_PARAM].config(0.0f, 1.0f, 0.0f, "Run");
		params[BPMMODE_DOWN_PARAM].config(0.0f, 1.0f, 0.0f, "Bpm mode left");
		params[BPMMODE_UP_PARAM].config(0.0f, 1.0f, 0.0f,  "Bpm mode right");
		params[SWING_PARAMS + 0].config(-1.0f, 1.0f, 0.0f, "Swing clk 0");
		params[PW_PARAMS + 0].config(0.0f, 1.0f, 0.5f, "Pulse width clk 0");			
		char strBuf[32];
		for (int i = 0; i < 3; i++) {// Row 2-4 (sub clocks)
			// Ratio1 knob
			snprintf(strBuf, 32, "Ratio clk %i", i);
			params[RATIO_PARAMS + 1 + i].config((34.0f - 1.0f)*-1.0f, 34.0f - 1.0f, 0.0f, strBuf);		
			// Swing knobs
			snprintf(strBuf, 32, "Swing clk %i", i);
			params[SWING_PARAMS + 1 + i].config(-1.0f, 1.0f, 0.0f, strBuf);
			// PW knobs
			snprintf(strBuf, 32, "Pulse width clk %i", i);
			params[PW_PARAMS + 1 + i].config(0.0f, 1.0f, 0.5f,strBuf);
			// Delay knobs
			snprintf(strBuf, 32, "Delay clk %i", i);
			params[DELAY_PARAMS + 1 + i].config(0.0f, 8.0f - 1.0f, 0.0f, strBuf);
		}
		
		for (int i = 1; i < 4; i++) {
			clk[i].setup(&clk[0], &resetClockOutputsHigh);		
		}
		onReset();
	}
	

	void onReset() override {
		sampleRate = (double)(APP->engine->getSampleRate());
		sampleTime = 1.0 / sampleRate;
		displayDelayNoteMode = true;
		bpmDetectionMode = false;
		emitResetOnStopRun = false;
		ppqn = 4;
		running = true;
		resetClockOutputsHigh = true;
		editingBpmMode = 0l;
		resetClocked(true);		
	}
	
	
	void onRandomize() override {
		resetClocked(false);
	}

	
	void resetClocked(bool hardReset) {// set hardReset to true to revert learned BPM to 120 in sync mode, or else when false, learned bmp will stay persistent
		for (int i = 0; i < 4; i++) {
			clk[i].reset();
			if (i < 3) 
				delay[i].reset(resetClockOutputsHigh);
			syncRatios[i] = false;
			ratiosDoubled[i] = getRatioDoubled(i);
			updatePulseSwingDelay();
			outputs[CLK_OUTPUTS + i].setVoltage((resetClockOutputsHigh ? 10.0f : 0.0f));
		}
		extPulseNumber = -1;
		extIntervalTime = 0.0;
		timeoutTime = 2.0 / ppqn + 0.1;// worst case. This is a double period at 30 BPM (4s), divided by the expected number of edges in the double period 
									   //   which is 2*ppqn, plus epsilon. This timeoutTime is only used for timingout the 2nd clock edge
		if (inputs[BPM_INPUT].isConnected()) {
			if (bpmDetectionMode) {
				if (hardReset)
					newMasterLength = 1.0f;// 120 BPM
			}
			else
				newMasterLength = 1.0f / powf(2.0f, inputs[BPM_INPUT].getVoltage());// bpm = 120*2^V, 2T = 120/bpm = 120/(120*2^V) = 1/2^V
		}
		else
			newMasterLength = 120.0f / getBpmKnob();
		newMasterLength = clamp(newMasterLength, masterLengthMin, masterLengthMax);
		masterLength = newMasterLength;
	}	
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// displayDelayNoteMode
		json_object_set_new(rootJ, "displayDelayNoteMode", json_boolean(displayDelayNoteMode));
		
		// bpmDetectionMode
		json_object_set_new(rootJ, "bpmDetectionMode", json_boolean(bpmDetectionMode));
		
		// emitResetOnStopRun
		json_object_set_new(rootJ, "emitResetOnStopRun", json_boolean(emitResetOnStopRun));
		
		// ppqn
		json_object_set_new(rootJ, "ppqn", json_integer(ppqn));
		
		// resetClockOutputsHigh
		json_object_set_new(rootJ, "resetClockOutputsHigh", json_boolean(resetClockOutputsHigh));
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// displayDelayNoteMode
		json_t *displayDelayNoteModeJ = json_object_get(rootJ, "displayDelayNoteMode");
		if (displayDelayNoteModeJ)
			displayDelayNoteMode = json_is_true(displayDelayNoteModeJ);

		// bpmDetectionMode
		json_t *bpmDetectionModeJ = json_object_get(rootJ, "bpmDetectionMode");
		if (bpmDetectionModeJ)
			bpmDetectionMode = json_is_true(bpmDetectionModeJ);

		// emitResetOnStopRun
		json_t *emitResetOnStopRunJ = json_object_get(rootJ, "emitResetOnStopRun");
		if (emitResetOnStopRunJ)
			emitResetOnStopRun = json_is_true(emitResetOnStopRunJ);

		// ppqn
		json_t *ppqnJ = json_object_get(rootJ, "ppqn");
		if (ppqnJ)
			ppqn = json_integer_value(ppqnJ);

		// resetClockOutputsHigh
		json_t *resetClockOutputsHighJ = json_object_get(rootJ, "resetClockOutputsHigh");
		if (resetClockOutputsHighJ)
			resetClockOutputsHigh = json_is_true(resetClockOutputsHighJ);

		scheduledReset = true;
	}

	
	void onSampleRateChange(const ProcessArgs &args) {
		sampleRate = (double)args.sampleRate;
		sampleTime = 1.0 / sampleRate;
		resetClocked(false);
	}		
	

	void process(const ProcessArgs &args) override {	
		// Scheduled reset
		if (scheduledReset) {
			resetClocked(false);		
			scheduledReset = false;
		}
		
		// Run button
		if (runTrigger.process(params[RUN_PARAM].getValue() + inputs[RUN_INPUT].getVoltage())) {// no input refresh here, don't want to introduce clock skew
			if (!(bpmDetectionMode && inputs[BPM_INPUT].isConnected()) || running) {// toggle when not BPM detect, turn off only when BPM detect (allows turn off faster than timeout if don't want any trailing beats after stoppage). If allow manually start in bpmDetectionMode   the clock will not know which pulse is the 1st of a ppqn set, so only allow stop
				running = !running;
				runPulse.trigger(0.001f);
				if (!running && emitResetOnStopRun) {
					resetClocked(false);
					resetPulse.trigger(0.001f);
					resetLight = 1.0f;
				}
			}
			else
				cantRunWarning = (long) (0.7 * sampleRate / displayRefreshStepSkips);
		}

		// Reset (has to be near top because it sets steps to 0, and 0 not a real step (clock section will move to 1 before reaching outputs)
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue())) {
			resetLight = 1.0f;
			resetPulse.trigger(0.001f);
			resetClocked(false);	
		}	

		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {

			updatePulseSwingDelay();
		
			// BPM mode
			bool trigUp = bpmModeUpTrigger.process(params[BPMMODE_UP_PARAM].getValue());
			bool trigDown = bpmModeDownTrigger.process(params[BPMMODE_DOWN_PARAM].getValue());
			if (trigUp || trigDown) {
				if (editingBpmMode != 0ul) {// force active before allow change
					if (bpmDetectionMode == false) {
						bpmDetectionMode = true;
						ppqn = (trigUp ? 2 : 24);
					}
					else {
						if (ppqn == 2) {
							if (trigUp) ppqn = 4;
							else bpmDetectionMode = false;
						}
						else if (ppqn == 4)
							ppqn = (trigUp ? 8 : 2);
						else if (ppqn == 8)
							ppqn = (trigUp ? 12 : 4);
						else if (ppqn == 12)
							ppqn = (trigUp ? 16 : 8);
						else if (ppqn == 16)
							ppqn = (trigUp ? 24 : 12);
						else {// 24
							if (trigUp) bpmDetectionMode = false;
							else ppqn = 16;
						}
					}
				}
				editingBpmMode = (long) (3.0 * sampleRate / displayRefreshStepSkips);
			}
			
		}// userInputs refresh
	
		// BPM input and knob
		newMasterLength = masterLength;
		if (inputs[BPM_INPUT].isConnected()) { 
			bool trigBpmInValue = bpmDetectTrigger.process(inputs[BPM_INPUT].getVoltage());
			
			// BPM Detection method
			if (bpmDetectionMode) {
				// rising edge detect
				if (trigBpmInValue) {
					if (!running) {
						// this must be the only way to start runnning when in bpmDetectionMode or else
						//   when manually starting, the clock will not know which pulse is the 1st of a ppqn set
						running = true;
						runPulse.trigger(0.001f);
						resetClocked(false);
					}
					if (running) {
						extPulseNumber++;
						if (extPulseNumber >= ppqn * 2)// *2 because working with double_periods
							extPulseNumber = 0;
						if (extPulseNumber == 0)// if first pulse, start interval timer
							extIntervalTime = 0.0;
						else {
							// all other ppqn pulses except the first one. now we have an interval upon which to plan a strecth 
							double timeLeft = extIntervalTime * (double)(ppqn * 2 - extPulseNumber) / ((double)extPulseNumber);
							newMasterLength = clamp(clk[0].getStep() + timeLeft, masterLengthMin / 1.5f, masterLengthMax * 1.5f);// extended range for better sync ability (20-450 BPM)
							timeoutTime = extIntervalTime * ((double)(1 + extPulseNumber) / ((double)extPulseNumber)) + 0.1; // when a second or higher clock edge is received, 
							//  the timeout is the predicted next edge (whici is extIntervalTime + extIntervalTime / extPulseNumber) plus epsilon
						}
					}
				}
				if (running) {
					extIntervalTime += sampleTime;
					if (extIntervalTime > timeoutTime) {
						running = false;
						runPulse.trigger(0.001f);
						if (emitResetOnStopRun) {
							resetClocked(false);
							resetPulse.trigger(0.001f);
							resetLight = 1.0f;
						}
					}
				}
			}
			// BPM CV method
			else {// bpmDetectionMode not active
				newMasterLength = clamp(1.0f / powf(2.0f, inputs[BPM_INPUT].getVoltage()), masterLengthMin, masterLengthMax);// bpm = 120*2^V, 2T = 120/bpm = 120/(120*2^V) = 1/2^V
				// no need to round since this clocked's master's BPM knob is a snap knob thus already rounded, and with passthru approach, no cumul error
			}
		}
		else {// BPM_INPUT not active
			newMasterLength = clamp(120.0f / getBpmKnob(), masterLengthMin, masterLengthMax);
		}
		if (newMasterLength != masterLength) {
			double lengthStretchFactor = ((double)newMasterLength) / ((double)masterLength);
			for (int i = 0; i < 4; i++) {
				clk[i].applyNewLength(lengthStretchFactor);
			}
			masterLength = newMasterLength;
		}
		
		
		// main clock engine and outputs
		if (running) {
			// See if clocks finished their prescribed number of iteratios of double periods (and syncWait for sub) or 
			//    if they were forced reset and if so, recalc and restart them
			
			// Master clock
			if (clk[0].isReset()) {
				// See if ratio knobs changed (or unitinialized)
				for (int i = 1; i < 4; i++) {
					if (syncRatios[i]) {// unused (undetermined state) for master
						clk[i].reset();// force reset (thus refresh) of that sub-clock
						ratiosDoubled[i] = getRatioDoubled(i);
						syncRatios[i] = false;
					}
				}
				clk[0].setup(masterLength, 1, sampleTime);// must call setup before start. length = double_period
				clk[0].start();
			}
			outputs[CLK_OUTPUTS + 0].setVoltage(clk[0].isHigh(swingAmount[0], pulseWidth[0]) ? 10.0f : 0.0f);		
			
			// Sub clocks
			for (int i = 1; i < 4; i++) {
				if (clk[i].isReset()) {
					double length;
					int iterations;
					int ratioDoubled = ratiosDoubled[i];
					if (ratioDoubled < 0) { // if div 
						ratioDoubled *= -1;
						length = masterLength * ((double)ratioDoubled) / 2.0;
						iterations = 1l + (ratioDoubled % 2);		
						clk[i].setup(length, iterations, sampleTime);
					}
					else {// mult 
						length = (2.0f * masterLength) / ((double)ratioDoubled);
						iterations = ratioDoubled / (2l - (ratioDoubled % 2l));							
						clk[i].setup(length, iterations, sampleTime);
					}
					clk[i].start();
				}
				delay[i - 1].write(clk[i].isHigh(swingAmount[i], pulseWidth[i]));
				outputs[CLK_OUTPUTS + i].setVoltage(delay[i - 1].read(delaySamples[i]) ? 10.0f : 0.0f);
			}

			// Step clocks
			for (int i = 0; i < 4; i++)
				clk[i].stepClock();
		}
			
		// Chaining outputs
		outputs[RESET_OUTPUT].setVoltage((resetPulse.process((float)sampleTime) ? 10.0f : 0.0f));
		outputs[RUN_OUTPUT].setVoltage((runPulse.process((float)sampleTime) ? 10.0f : 0.0f));
		outputs[BPM_OUTPUT].setVoltage( inputs[BPM_INPUT].isConnected() ? inputs[BPM_INPUT].getVoltage() : log2f(1.0f / masterLength));
			
		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			// Reset light
			lights[RESET_LIGHT].value =	resetLight;	
			resetLight -= (resetLight / lightLambda) * (float)sampleTime * displayRefreshStepSkips;
			
			// Run light
			lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;
			
			// BPM light
			bool warningFlashState = true;
			if (cantRunWarning > 0l) 
				warningFlashState = calcWarningFlash(cantRunWarning, (long) (0.7 * sampleRate / displayRefreshStepSkips));
			lights[BPMSYNC_LIGHT + 0].value = (bpmDetectionMode && warningFlashState) ? 1.0f : 0.0f;
			lights[BPMSYNC_LIGHT + 1].value = (bpmDetectionMode && warningFlashState) ? (float)((ppqn - 2)*(ppqn - 2))/440.0f : 0.0f;			
			
			// ratios synched lights
			for (int i = 1; i < 4; i++)
				lights[CLK_LIGHTS + i].value = (syncRatios[i] && running) ? 1.0f: 0.0f;

			// info notification counters
			for (int i = 0; i < 4; i++) {
				notifyInfo[i]--;
				if (notifyInfo[i] < 0l)
					notifyInfo[i] = 0l;
			}
			if (cantRunWarning > 0l)
				cantRunWarning--;
			editingBpmMode--;
			if (editingBpmMode < 0l)
				editingBpmMode = 0l;
		}// lightRefreshCounter
	}// process()
};


struct ClockedWidget : ModuleWidget {
	struct RatioDisplayWidget : TransparentWidget {
		Clocked *module;
		int knobIndex;
		std::shared_ptr<Font> font;
		char displayStr[4];
		const std::string delayLabelsClock[8] = {"D 0", "/16",   "1/8",  "1/4", "1/3",     "1/2", "2/3",     "3/4"};
		const std::string delayLabelsNote[8]  = {"D 0", "/64",   "/32",  "/16", "/8t",     "1/8", "/4t",     "/8d"};

		
		RatioDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			//nvgTextLetterSpacing(args.vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(args.vg, textColor);
			if (module == NULL) {
				if (knobIndex == 0)
					snprintf(displayStr, 4, "120");
				else
					snprintf(displayStr, 4, "X 1");
			}
			else if (module->notifyInfo[knobIndex] > 0l)
			{
				int srcParam = module->notifyingSource[knobIndex];
				if ( (srcParam >= Clocked::SWING_PARAMS + 0) && (srcParam <= Clocked::SWING_PARAMS + 3) ) {
					float swValue = module->swingAmount[knobIndex];//module->params[Clocked::SWING_PARAMS + knobIndex].getValue();
					int swInt = (int)round(swValue * 99.0f);
					snprintf(displayStr, 4, " %2u", (unsigned) abs(swInt));
					if (swInt < 0)
						displayStr[0] = '-';
					if (swInt >= 0)
						displayStr[0] = '+';
				}
				else if ( (srcParam >= Clocked::DELAY_PARAMS + 1) && (srcParam <= Clocked::DELAY_PARAMS + 3) ) {				
					int delayKnobIndex = (int)(module->params[Clocked::DELAY_PARAMS + knobIndex].getValue() + 0.5f);
					if (module->displayDelayNoteMode)
						snprintf(displayStr, 4, "%s", (delayLabelsNote[delayKnobIndex]).c_str());
					else
						snprintf(displayStr, 4, "%s", (delayLabelsClock[delayKnobIndex]).c_str());
				}					
				else if ( (srcParam >= Clocked::PW_PARAMS + 0) && (srcParam <= Clocked::PW_PARAMS + 3) ) {				
					float pwValue = module->pulseWidth[knobIndex];//module->params[Clocked::PW_PARAMS + knobIndex].getValue();
					int pwInt = ((int)round(pwValue * 98.0f)) + 1;
					snprintf(displayStr, 4, "_%2u", (unsigned) abs(pwInt));
				}					
			}
			else {
				if (knobIndex > 0) {// ratio to display
					bool isDivision = false;
					int ratioDoubled = module->getRatioDoubled(knobIndex);
					if (ratioDoubled < 0) {
						ratioDoubled = -1 * ratioDoubled;
						isDivision = true;
					}
					if ( (ratioDoubled % 2) == 1 )
						snprintf(displayStr, 4, "%c,5", 0x30 + (char)(ratioDoubled / 2));
					else {
						snprintf(displayStr, 4, "X%2u", (unsigned)(ratioDoubled / 2));
						if (isDivision)
							displayStr[0] = '/';
					}
				}
				else {// BPM to display
					if (module->editingBpmMode != 0l) {
						if (!module->bpmDetectionMode)
							snprintf(displayStr, 4, " CV");
						else
							snprintf(displayStr, 4, "P%2u", (unsigned) module->ppqn);
					}
					else
						snprintf(displayStr, 4, "%3u", (unsigned)((120.0f / module->masterLength) + 0.5f));
				}
			}
			displayStr[3] = 0;// more safety
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	struct PanelThemeItem : MenuItem {
		Clocked *module;
		int theme;
		void onAction(const widget::ActionEvent &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	struct DelayDisplayNoteItem : MenuItem {
		Clocked *module;
		void onAction(const widget::ActionEvent &e) override {
			module->displayDelayNoteMode = !module->displayDelayNoteMode;
		}
	};
	struct EmitResetItem : MenuItem {
		Clocked *module;
		void onAction(const widget::ActionEvent &e) override {
			module->emitResetOnStopRun = !module->emitResetOnStopRun;
		}
	};	
	struct ResetHighItem : MenuItem {
		Clocked *module;
		void onAction(const widget::ActionEvent &e) override {
			module->resetClockOutputsHigh = !module->resetClockOutputsHigh;
			module->resetClocked(true);
		}
	};	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Clocked *module = dynamic_cast<Clocked*>(this->module);
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
		
		DelayDisplayNoteItem *ddnItem = createMenuItem<DelayDisplayNoteItem>("Display delay values in notes", CHECKMARK(module->displayDelayNoteMode));
		ddnItem->module = module;
		menu->addChild(ddnItem);

		EmitResetItem *erItem = createMenuItem<EmitResetItem>("Reset when run is turned off", CHECKMARK(module->emitResetOnStopRun));
		erItem->module = module;
		menu->addChild(erItem);

		ResetHighItem *rhItem = createMenuItem<ResetHighItem>("Outputs reset high when not running", CHECKMARK(module->resetClockOutputsHigh));
		rhItem->module = module;
		menu->addChild(rhItem);
	}	
	
	struct IMSmallKnobNotify : IMSmallKnob {
		IMSmallKnobNotify() {};
		void onDragMove(const widget::DragMoveEvent &e) override {
			Clocked *module = dynamic_cast<Clocked*>(this->paramQuantity->module);
			int dispIndex = 0;
			int paramId = paramQuantity->paramId;
			if ( (paramId >= Clocked::SWING_PARAMS + 0) && (paramId <= Clocked::SWING_PARAMS + 3) )
				dispIndex = paramId - Clocked::SWING_PARAMS;
			else if ( (paramId >= Clocked::DELAY_PARAMS + 1) && (paramId <= Clocked::DELAY_PARAMS + 3) )
				dispIndex = paramId - Clocked::DELAY_PARAMS;
			else if ( (paramId >= Clocked::PW_PARAMS + 0) && (paramId <= Clocked::PW_PARAMS + 3) )
				dispIndex = paramId - Clocked::PW_PARAMS;
			module->notifyingSource[dispIndex] = paramId;
			module->notifyInfo[dispIndex] = (long) (Clocked::delayInfoTime * module->sampleRate / displayRefreshStepSkips);
			Knob::onDragMove(e);
		}
	};
	struct IMSmallSnapKnobNotify : IMSmallKnobNotify {
		IMSmallSnapKnobNotify() {
			snap = true;
			smooth = false;
		}
	};
	struct IMBigSnapKnobNotify : IMBigSnapKnob {
		IMBigSnapKnobNotify() {}
		void randomize() override {ParamWidget::randomize();}
		void onChange(const widget::ChangeEvent &e) override {
			Clocked *module = dynamic_cast<Clocked*>(this->paramQuantity->module);
			int dispIndex = 0;
			int paramId = paramQuantity->paramId;
			if ( (paramId >= Clocked::RATIO_PARAMS + 1) && (paramId <= Clocked::RATIO_PARAMS + 3) ) {
				dispIndex = paramId - Clocked::RATIO_PARAMS;
				module->syncRatios[dispIndex] = true;
			}
			module->notifyInfo[dispIndex] = 0l;
			SVGKnob::onChange(e);		
		}
	};

	
	ClockedWidget(Clocked *module) {
		setModule(module);
		
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->mode = module ? &module->panelTheme : NULL;
        panel->addPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/Clocked.svg")));
        panel->addPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/Clocked.svg")));//dark/Clocked_dark.svg")));
        box.size = panel->box.size;
        addChild(panel);		
		
		// Screws
		addChild(createDynamicScrew<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicScrew<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicScrew<IMScrew>(Vec(panel->box.size.x-30, 365), module ? &module->panelTheme : NULL));


		static const int rowRuler0 = 50;//reset,run inputs, master knob and bpm display
		static const int rowRuler1 = rowRuler0 + 55;// reset,run switches
		//
		static const int rowRuler2 = rowRuler1 + 55;// clock 1
		static const int rowSpacingClks = 50;
		static const int rowRuler5 = rowRuler2 + rowSpacingClks * 2 + 55;// reset,run outputs, pw inputs
		
		
		static const int colRulerL = 18;// reset input and button, ratio knobs
		// First two rows and last row
		static const int colRulerSpacingT = 47;
		static const int colRulerT1 = colRulerL + colRulerSpacingT;// run input and button
		static const int colRulerT2 = colRulerT1 + colRulerSpacingT;// in and pwMaster inputs
		static const int colRulerT3 = colRulerT2 + colRulerSpacingT + 5;// swingMaster knob
		static const int colRulerT4 = colRulerT3 + colRulerSpacingT;// pwMaster knob
		static const int colRulerT5 = colRulerT4 + colRulerSpacingT;// clkMaster output
		// Three clock rows
		static const int colRulerM0 = colRulerL + 5;// ratio knobs
		static const int colRulerM1 = colRulerL + 60;// ratio displays
		static const int colRulerM2 = colRulerT3;// swingX knobs
		static const int colRulerM3 = colRulerT4;// pwX knobs
		static const int colRulerM4 = colRulerT5;// clkX outputs
		
		RatioDisplayWidget *displayRatios[4];
		
		// Row 0
		// Reset input
		addInput(createDynamicPort<IMPort>(Vec(colRulerL, rowRuler0), true, module, Clocked::RESET_INPUT, module ? &module->panelTheme : NULL));
		// Run input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT1, rowRuler0), true, module, Clocked::RUN_INPUT, module ? &module->panelTheme : NULL));
		// In input
		addInput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler0), true, module, Clocked::BPM_INPUT, module ? &module->panelTheme : NULL));
		// Master BPM knob
		addParam(createDynamicParam<IMBigSnapKnobNotify>(Vec(colRulerT3 + 1 + offsetIMBigKnob, rowRuler0 + offsetIMBigKnob), module, Clocked::RATIO_PARAMS + 0, module ? &module->panelTheme : NULL));// must be a snap knob, code in step() assumes that a rounded value is read from the knob	(chaining considerations vs BPM detect)
		// BPM display
		displayRatios[0] = new RatioDisplayWidget();
		displayRatios[0]->box.pos = Vec(colRulerT4 + 11, rowRuler0 + vOffsetDisplay);
		displayRatios[0]->box.size = Vec(55, 30);// 3 characters
		displayRatios[0]->module = module;
		displayRatios[0]->knobIndex = 0;
		addChild(displayRatios[0]);
		
		// Row 1
		// Reset LED bezel and light
		addParam(createParam<LEDBezel>(Vec(colRulerL + offsetLEDbezel, rowRuler1 + offsetLEDbezel), module, Clocked::RESET_PARAM));
		addChild(createLight<MuteLight<GreenLight>>(Vec(colRulerL + offsetLEDbezel + offsetLEDbezelLight, rowRuler1 + offsetLEDbezel + offsetLEDbezelLight), module, Clocked::RESET_LIGHT));
		// Run LED bezel and light
		addParam(createParam<LEDBezel>(Vec(colRulerT1 + offsetLEDbezel, rowRuler1 + offsetLEDbezel), module, Clocked::RUN_PARAM));
		addChild(createLight<MuteLight<GreenLight>>(Vec(colRulerT1 + offsetLEDbezel + offsetLEDbezelLight, rowRuler1 + offsetLEDbezel + offsetLEDbezelLight), module, Clocked::RUN_LIGHT));
		// BPM mode buttons
		addParam(createDynamicParam<IMPushButton>(Vec(colRulerT2 + offsetTL1105 - 12, rowRuler1 + offsetTL1105), module, Clocked::BPMMODE_DOWN_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<IMPushButton>(Vec(colRulerT2 + offsetTL1105 + 12, rowRuler1 + offsetTL1105), module, Clocked::BPMMODE_UP_PARAM, module ? &module->panelTheme : NULL));
		// BPM mode light
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(colRulerT2 + offsetMediumLight, rowRuler1 + 22), module, Clocked::BPMSYNC_LIGHT));		
		// Swing master knob
		addParam(createDynamicParam<IMSmallKnobNotify>(Vec(colRulerT3 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Clocked::SWING_PARAMS + 0, module ? &module->panelTheme : NULL));
		// PW master knob
		addParam(createDynamicParam<IMSmallKnobNotify>(Vec(colRulerT4 + offsetIMSmallKnob, rowRuler1 + offsetIMSmallKnob), module, Clocked::PW_PARAMS + 0, module ? &module->panelTheme : NULL));
		// Clock master out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler1), false, module, Clocked::CLK_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		
		
		// Row 2-4 (sub clocks)		
		for (int i = 0; i < 3; i++) {
			// Ratio1 knob
			addParam(createDynamicParam<IMBigSnapKnobNotify>(Vec(colRulerM0 + offsetIMBigKnob, rowRuler2 + i * rowSpacingClks + offsetIMBigKnob), module, Clocked::RATIO_PARAMS + 1 + i, module ? &module->panelTheme : NULL));		
			// Ratio display
			displayRatios[i + 1] = new RatioDisplayWidget();
			displayRatios[i + 1]->box.pos = Vec(colRulerM1, rowRuler2 + i * rowSpacingClks + vOffsetDisplay);
			displayRatios[i + 1]->box.size = Vec(55, 30);// 3 characters
			displayRatios[i + 1]->module = module;
			displayRatios[i + 1]->knobIndex = i + 1;
			addChild(displayRatios[i + 1]);
			// Sync light
			addChild(createLight<SmallLight<RedLight>>(Vec(colRulerM1 + 62, rowRuler2 + i * rowSpacingClks + 10), module, Clocked::CLK_LIGHTS + i + 1));		
			// Swing knobs
			addParam(createDynamicParam<IMSmallKnobNotify>(Vec(colRulerM2 + offsetIMSmallKnob, rowRuler2 + i * rowSpacingClks + offsetIMSmallKnob), module, Clocked::SWING_PARAMS + 1 + i, module ? &module->panelTheme : NULL));
			// PW knobs
			addParam(createDynamicParam<IMSmallKnobNotify>(Vec(colRulerM3 + offsetIMSmallKnob, rowRuler2 + i * rowSpacingClks + offsetIMSmallKnob), module, Clocked::PW_PARAMS + 1 + i, module ? &module->panelTheme : NULL));
			// Delay knobs
			addParam(createDynamicParam<IMSmallSnapKnobNotify>(Vec(colRulerM4 + offsetIMSmallKnob, rowRuler2 + i * rowSpacingClks + offsetIMSmallKnob), module, Clocked::DELAY_PARAMS + 1 + i, module ? &module->panelTheme : NULL));
		}

		// Last row
		// Reset out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerL, rowRuler5), false, module, Clocked::RESET_OUTPUT, module ? &module->panelTheme : NULL));
		// Run out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT1, rowRuler5), false, module, Clocked::RUN_OUTPUT, module ? &module->panelTheme : NULL));
		// Out out
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT2, rowRuler5), false, module, Clocked::BPM_OUTPUT, module ? &module->panelTheme : NULL));
		// Sub-clock outputs
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT3, rowRuler5), false, module, Clocked::CLK_OUTPUTS + 1, module ? &module->panelTheme : NULL));	
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT4, rowRuler5), false, module, Clocked::CLK_OUTPUTS + 2, module ? &module->panelTheme : NULL));	
		addOutput(createDynamicPort<IMPort>(Vec(colRulerT5, rowRuler5), false, module, Clocked::CLK_OUTPUTS + 3, module ? &module->panelTheme : NULL));	

		// Expansion module
		// static const int rowRulerExpTop = 60;
		// static const int rowSpacingExp = 50;
		// static const int colRulerExp = 497 - 30 -150;// Clocked is (2+10)HP less than PS32
		// addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 0), true, module, Clocked::PW_INPUTS + 0, module ? &module->panelTheme : NULL));
		// addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 1), true, module, Clocked::PW_INPUTS + 1, module ? &module->panelTheme : NULL));
		// addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 2), true, module, Clocked::PW_INPUTS + 2, module ? &module->panelTheme : NULL));
		// addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 3), true, module, Clocked::SWING_INPUTS + 0, module ? &module->panelTheme : NULL));
		// addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 4), true, module, Clocked::SWING_INPUTS + 1, module ? &module->panelTheme : NULL));
		// addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 5), true, module, Clocked::SWING_INPUTS + 2, module ? &module->panelTheme : NULL));
	}
};

// Model *modelClocked = createModel<Clocked, ClockedWidget>("Impromptu Modular", "Clocked", "CLK - Clocked", CLOCK_TAG);
Model *modelClocked = createModel<Clocked, ClockedWidget>("Clocked");

/*CHANGE LOG

*/

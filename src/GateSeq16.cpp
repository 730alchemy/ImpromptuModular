//***********************************************************************************************
//Gate sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept by Nigel Sixsmith and Marc Boulé
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "dsp/digital.hpp"


struct GateSeq16 : Module {
	enum ParamIds {
		ENUMS(STEP_PARAMS, 64),
		GATE_PARAM,
		LENGTH_PARAM,
		GATEP_PARAM,
		MODE_PARAM,
		GATETRIG_PARAM,
		ENUMS(RUN_PARAMS, 4),
		RUNALL_PARAM,
		ENUMS(PROB_PARAMS, 4),
		CONFIG_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RUNCV_MODE_PARAM,
		RESET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(CLOCK_INPUTS, 4),
		RESET_INPUT,
		ENUMS(RUNCV_INPUTS, 4),
		RUNALLCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_LIGHTS, 64 * 4),// room for GreenYellowRedBlue
		ENUMS(GATE_LIGHT, 1),
		ENUMS(LENGTH_LIGHT, 1),
		ENUMS(GATEP_LIGHT, 1),
		ENUMS(MODE_LIGHT, 1),
		ENUMS(GATETRIG_LIGHT, 1),
		ENUMS(RUN_LIGHTS, 4),
		RESET_LIGHT,
		NUM_LIGHTS
	};
	
	enum DisplayStateIds {DISP_GATE, DISP_LENGTH, DISP_GATEP, DISP_MODE, DISP_GATETRIG, DISP_COPY, DISP_PASTE};
	
	// Need to save
	bool running[4] = {};
	bool gate[64] = {};
	int length[4] = {};// values are 1 to 16
	bool gatep[64] = {};
	int mode[4] = {};
	bool trig[4] = {};

	// No need to save
	int displayState;
	int indexStep[4] = {};
	int stepIndexRunHistory[4] = {};// no need to initialize
	bool gateCP[16] = {};// copy-paste only one row
	int lengthCP;// copy-paste only one row; values are 1 to 16
	bool gatepCP[16] = {};// copy-paste only one row
	int modeCP;// copy-paste only one row
	bool trigCP;// copy-paste only one row
	int rowCP;// row selected for copy or paste operation
	long feedbackCP;// downward step counter for CP feedback
	long confirmCP;// downward positive step counter for Copy confirmation, up neg Paste, 0 when nothing to confirm
	bool gateRandomEnable; 
	float resetLight = 0.0f;
	long feedbackCPinit;// no need to initialize
	bool gatePulsesValue[4] = {};
	
	SchmittTrigger gateTrigger;
	SchmittTrigger lengthTrigger;
	SchmittTrigger gatePTrigger;
	SchmittTrigger modeTrigger;
	SchmittTrigger gateTrigTrigger;
	SchmittTrigger stepTriggers[64];
	SchmittTrigger copyTrigger;
	SchmittTrigger pasteTrigger;
	SchmittTrigger runningTriggers[4];
	SchmittTrigger clockTriggers[4];
	SchmittTrigger resetTrigger;
	PulseGenerator gatePulses[4];

		
	GateSeq16() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override {
		displayState = DISP_GATE;
		for (int i = 0; i < 64; i++) {
			gate[i] = false;
			gatep[i] = false;
		}
		for (int i = 0; i < 4; i++) {
			running[i] = false;
			indexStep[i] = 0;
			length[i] = 16;
			mode[i] = MODE_FWD;
			trig[i] = false;
		}
		for (int i = 0; i < 16; i++) {
			gateCP[i] = 0;
			gatepCP[i] = 0;
		}
		lengthCP = 0;
		modeCP = 0;
		trigCP = false;
		rowCP = -1;
		feedbackCP = 0l;
		confirmCP = 0l;
		gateRandomEnable = false;
	}

	void onRandomize() override {
		// TODO
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_t *runningJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(runningJ, i, json_integer((int) running[i]));
		json_object_set_new(rootJ, "running", runningJ);
		
		// gate
		json_t *gateJ = json_array();
		for (int i = 0; i < 64; i++)
			json_array_insert_new(gateJ, i, json_integer((int) gate[i]));
		json_object_set_new(rootJ, "gate", gateJ);
		
		// length
		json_t *lengthJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(lengthJ, i, json_integer(length[i]));
		json_object_set_new(rootJ, "length", lengthJ);
		
		// gatep
		json_t *gatepJ = json_array();
		for (int i = 0; i < 64; i++)
			json_array_insert_new(gatepJ, i, json_integer((int) gatep[i]));
		json_object_set_new(rootJ, "gatep", gatepJ);
		
		// mode
		json_t *modeJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(modeJ, i, json_integer(mode[i]));
		json_object_set_new(rootJ, "mode", modeJ);
		
		// trig
		json_t *trigJ = json_array();
		for (int i = 0; i < 4; i++)
			json_array_insert_new(trigJ, i, json_integer((int) trig[i]));
		json_object_set_new(rootJ, "trig", trigJ);
		
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ) {
			for (int i = 0; i < 4; i++) {
				json_t *runningArrayJ = json_array_get(runningJ, i);
				if (runningArrayJ)
					running[i] = !!json_integer_value(runningArrayJ);
			}
		}
		
		// gate
		json_t *gateJ = json_object_get(rootJ, "gate");
		if (gateJ) {
			for (int i = 0; i < 64; i++) {
				json_t *gateArrayJ = json_array_get(gateJ, i);
				if (gateArrayJ)
					gate[i] = !!json_integer_value(gateArrayJ);
			}
		}
		
		// length
		json_t *lengthJ = json_object_get(rootJ, "length");
		if (lengthJ)
			for (int i = 0; i < 4; i++)
			{
				json_t *lengthArrayJ = json_array_get(lengthJ, i);
				if (lengthArrayJ)
					length[i] = json_integer_value(lengthArrayJ);
			}
		
		// gatep
		json_t *gatepJ = json_object_get(rootJ, "gatep");
		if (gatepJ) {
			for (int i = 0; i < 64; i++) {
				json_t *gatepArrayJ = json_array_get(gatepJ, i);
				if (gatepArrayJ)
					gatep[i] = !!json_integer_value(gatepArrayJ);
			}
		}
		
		// mode
		json_t *modeJ = json_object_get(rootJ, "mode");
		if (modeJ)
			for (int i = 0; i < 4; i++)
			{
				json_t *modeArrayJ = json_array_get(modeJ, i);
				if (modeArrayJ)
					mode[i] = json_integer_value(modeArrayJ);
			}
		
		// trig
		json_t *trigJ = json_object_get(rootJ, "trig");
		if (trigJ) {
			for (int i = 0; i < 4; i++) {
				json_t *trigArrayJ = json_array_get(trigJ, i);
				if (trigArrayJ)
					trig[i] = !!json_integer_value(trigArrayJ);
			}
		}
	}


	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {
		static const float copyPasteInfoTime = 3.0f;// seconds
		static const float copyPasteConfirmTime = 0.4f;// seconds
		float engineSampleRate = engineGetSampleRate();
		feedbackCPinit = (long) (copyPasteInfoTime * engineSampleRate);
		
		// Run state and light
		for (int i = 0; i < 4; i++) {
			if (runningTriggers[i].process(params[RUN_PARAMS + i].value + inputs[RUNCV_INPUTS + i].value)) {
				running[i] = !running[i];
				if (running[i]) {
					indexStep[i] = 0;
				}
			}
			lights[RUN_LIGHTS + i].value = (running[i]);
		}
		
		// Gate button
		if (gateTrigger.process(params[GATE_PARAM].value)) {
			displayState = DISP_GATE;
		}
		
		// Length button
		if (lengthTrigger.process(params[LENGTH_PARAM].value)) {
			if (displayState != DISP_LENGTH)
				displayState = DISP_LENGTH;
			else
				displayState = DISP_GATE;
		}
		// GateP button
		if (gatePTrigger.process(params[GATEP_PARAM].value)) {
			info("gatep trigged");
			if (displayState != DISP_GATEP)
				displayState = DISP_GATEP;
			else
				displayState = DISP_GATE;
		}
		// Mode button
		if (modeTrigger.process(params[MODE_PARAM].value)) {
			if (displayState != DISP_MODE)
				displayState = DISP_MODE;
			else
				displayState = DISP_GATE;
		}
		// GateTrig button
		if (gateTrigTrigger.process(params[GATETRIG_PARAM].value)) {
			if (displayState != DISP_GATETRIG)
				displayState = DISP_GATETRIG;
			else
				displayState = DISP_GATE;
		}
		// Copy button
		if (copyTrigger.process(params[COPY_PARAM].value)) {
			if (displayState != DISP_COPY) {
				displayState = DISP_COPY;
				feedbackCP = feedbackCPinit;
				rowCP = -1;// used for confirmCP, and to tell when aborted copy so that no confirmCP
			}
			else {
				displayState = DISP_GATE;
			}
		}
		// Paste button
		if (pasteTrigger.process(params[PASTE_PARAM].value)) {
			if (displayState != DISP_PASTE) {
				displayState = DISP_PASTE;
				feedbackCP = feedbackCPinit;
				rowCP = -1;// used for confirmCP, and to tell when aborted copy so that no confirmCP
			}
			else {
				displayState = DISP_GATE;
			}
		}
	
		// Step LED button presses
		for (int i = 0; i < 64; i++) {
			if (stepTriggers[i].process(params[STEP_PARAMS + i].value)) {
				int row = i / 16;
				int col = i % 16;
				if (displayState == DISP_GATE) {
					gate[i] = !gate[i];
				}
				if (displayState == DISP_LENGTH) {
					length[row] = col + 1;
				}
				if (displayState == DISP_GATEP) {
					gatep[i] = !gatep[i];
				}
				if (displayState == DISP_MODE) {
					if (col >= 4 && col <= 8)
						mode[row] = col - 4;
				}
				if (displayState == DISP_GATETRIG) {
					if (col == 10)
						trig[row] = false;
					else if (col == 11)
						trig[row] = true;
				}
				if (displayState == DISP_COPY) {
					for (int i = 0; i < 16; i++) {
						gateCP[i] = gate[row * 16 + i];
						gatepCP[i] = gatep[row * 16 + i];
					}
					lengthCP = length[row];
					modeCP = mode[row];
					trigCP = trig[row];
					rowCP = row;
					confirmCP = (long) (copyPasteConfirmTime * engineSampleRate);
					displayState = DISP_GATE;
				}
				if (displayState == DISP_PASTE) {
					for (int i = 0; i < 16; i++) {
						gate[row * 16 + i] = gateCP[i];
						gatep[row * 16 + i] = gatepCP[i];
					}
					length[row]= lengthCP;
					mode[row] = modeCP;
					trig[row] = trigCP;
					rowCP = row;
					confirmCP = (long) (-1 * copyPasteConfirmTime * engineSampleRate);					
					displayState = DISP_GATE;	
				}

			}
		}
		
		// Clock
		for (int i = 0; i < 4; i++) {
			if (clockTriggers[i].process(inputs[CLOCK_INPUTS + i].value)) {// TODO replicate clock to unconnected inputs
				if (running[i]) {
					moveIndexRunMode(&indexStep[i], length[i], mode[i], &stepIndexRunHistory[i]);
					gatePulses[i].trigger(0.001f);
					gateRandomEnable = gatep[i * 16 + indexStep[i]];// not random yet
					if (gateRandomEnable)
						gateRandomEnable = randomUniform() < (params[PROB_PARAMS + i].value);// randomUniform is [0.0, 1.0), see include/util/common.hpp
					else 
						gateRandomEnable = true;
				}
			}
		}
		
		// Process PulseGenerators (even if may not use)
		for (int i = 0; i < 4; i++)
			gatePulsesValue[i] = gatePulses[i].process(1.0 / engineSampleRate);
		
		// gate outputs
		for (int i = 0; i < 4; i++) {
			if (running[i]) {
				if (trig[i])
					outputs[GATE_OUTPUTS + i].value = (gatePulsesValue[i] && gateRandomEnable && gate[i * 16 + indexStep[i]]) ? 10.0f : 0.0f;
				else
					outputs[GATE_OUTPUTS + i].value = (clockTriggers[i].isHigh() && gateRandomEnable && gate[i * 16 + indexStep[i]]) ? 10.0f : 0.0f;
			}
			else {// not running 
				outputs[GATE_OUTPUTS + i].value = 0.0f;
			}		
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			for (int i = 0; i < 4; i++)
				indexStep[i] = 0;
			resetLight = 1.0f;
		}
		else
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime();		
		
		// Step LED button lights
		if (confirmCP != 0l) {
			if (confirmCP > 0l) {// copy confirm
				for (int i = 0; i < 64; i++)
					setFourLightGreen(STEP_LIGHTS + i * 4, (i / 16) == rowCP ? 1.0f : 0.0f);
			}
			else {// paste confirm
				for (int i = 0; i < 64; i++)
					setFourLightGreen(STEP_LIGHTS + i * 4, (i / 16) == rowCP ? 1.0f : 0.0f);
			}
		}
		else {
			if (displayState == DISP_GATE) {
				for (int i = 0; i < 64; i++) {
					if (gate[i]) {
						if (gatep[i])
							setFourLightYellow(STEP_LIGHTS + i * 4, 1.0f);
						else
							setFourLightGreen(STEP_LIGHTS + i * 4, 1.0f);
					}
					else {
						int row = i / 16;
						int col = i % 16;
						setFourLightGreen(STEP_LIGHTS + i * 4, indexStep[row] == col ? 0.1f : 0.0f);
					}
				}
			}
			else if (displayState == DISP_LENGTH) {
				for (int i = 0; i < 64; i++) {
					int row = i / 16;
					int col = i % 16;
					if (col < (length[row] - 1))
						setFourLightGreen(STEP_LIGHTS + i * 4, 0.1f);
					else if (col == (length[row] - 1))
						setFourLightGreen(STEP_LIGHTS + i * 4, 1.0f);
					else 
						setFourLightGreen(STEP_LIGHTS + i * 4, 0.0f);
				}
			}
			else if (displayState == DISP_GATEP) {
				for (int i = 0; i < 64; i++)
					if (gatep[i])
						setFourLightRed(STEP_LIGHTS + i * 4, 1.0f);
					else 
						setFourLightRed(STEP_LIGHTS + i * 4, gate[i] ? 0.1f : 0.0f);
			}
			else if (displayState == DISP_MODE) {
				for (int i = 0; i < 64; i++) {
					int row = i / 16;
					int col = i % 16;
					if (col < 4 || col > 8) {
						setFourLightRed(STEP_LIGHTS + i * 4, 0.0f);// off
					}
					else { 
						if (col - 4 == mode[row])
							setFourLightBlue(STEP_LIGHTS + i * 4, 1.0f);
						else
							setFourLightBlue(STEP_LIGHTS + i * 4, 0.05f);
					}
				}
			}
			else if (displayState == DISP_GATETRIG) {
				for (int i = 0; i < 64; i++) {
					int row = i / 16;
					int col = i % 16;
					if (col < 10 || col > 11) {
						setFourLightRed(STEP_LIGHTS + i * 4, 0.0f);// off
					}
					else {
						if (col - 10 == (trig[row] ? 1 : 0))
							setFourLightBlue(STEP_LIGHTS + i * 4, 1.0f);
						else
							setFourLightBlue(STEP_LIGHTS + i * 4, 0.05f);
					}
				}
			}
			else if (displayState == DISP_COPY) {
				int rowToLight = CalcRowToLight(feedbackCP, feedbackCPinit);
				for (int i = 0; i < 64; i++) {
					int row = i / 16;
					if (row == rowToLight)
						setFourLightGreen(STEP_LIGHTS + i * 4, 0.5f);
				}
			}
			else if (displayState == DISP_PASTE) {
				int rowToLight = CalcRowToLight(feedbackCP, feedbackCPinit);
				for (int i = 0; i < 64; i++) {
					int row = i / 16;
					if (row == rowToLight)
						setFourLightGreen(STEP_LIGHTS + i * 4, 0.5f);
				}
			}
			else {
				for (int i = 0; i < 64; i++)
					setFourLightRed(STEP_LIGHTS + i * 4, 0.0f);// should never happen
			}
		}
		
		// Main button lights
		lights[GATE_LIGHT].value = displayState == DISP_GATE ? 1.0f : 0.0f;// green
		lights[LENGTH_LIGHT].value = displayState == DISP_LENGTH ? 1.0f : 0.0f;// green
		lights[GATEP_LIGHT].value = displayState == DISP_GATEP ? 1.0f : 0.0f;// red
		lights[MODE_LIGHT].value = displayState == DISP_MODE ? 1.0f : 0.0f;// blue		
		lights[GATETRIG_LIGHT].value = displayState == DISP_GATETRIG ? 1.0f : 0.0f;// blue

		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	

		
		if (confirmCP != 0l) {
			if (confirmCP > 0l)
				confirmCP --;
			if (confirmCP < 0l)
				confirmCP ++;
		}
		if (feedbackCP > 0l)			
			feedbackCP--;	
		else
			feedbackCP = feedbackCPinit;// roll over
		
	}// step()
	
	void setFourLightRed(int id, float value) {
		lights[id + 0].value = value;
		lights[id + 1].value = 0.0f;
		lights[id + 2].value = 0.0f;
		lights[id + 3].value = 0.0f;
	}
	void setFourLightYellow(int id, float value) {
		lights[id + 0].value = 0.0f;
		lights[id + 1].value = value;
		lights[id + 2].value = 0.0f;
		lights[id + 3].value = 0.0f;
	}
	void setFourLightGreen(int id, float value) {
		lights[id + 0].value = 0.0f;
		lights[id + 1].value = 0.0f;
		lights[id + 2].value = value;
		lights[id + 3].value = 0.0f;
	}
	void setFourLightBlue(int id, float value) {
		lights[id + 0].value = 0.0f;
		lights[id + 1].value = 0.0f;
		lights[id + 2].value = 0.0f;
		lights[id + 3].value = value;
	}

	int CalcRowToLight(long feedbackCP, long feedbackCPinit) {
		int rowToLight = -1;
		long onDelta = feedbackCPinit / 14;
		long onThreshold;// top based
		
		onThreshold = feedbackCPinit;
		if (feedbackCP < onThreshold && feedbackCP > (onThreshold - onDelta))
			rowToLight = 0;
		else {
			onThreshold = feedbackCPinit * 3 / 4;
			if (feedbackCP < onThreshold && feedbackCP > (onThreshold - onDelta))
				rowToLight = 1;
			else {
				onThreshold = feedbackCPinit * 2 / 4;
				if (feedbackCP < onThreshold && feedbackCP > (onThreshold - onDelta))
					rowToLight = 2;
				else {
					onThreshold = feedbackCPinit * 1 / 4;
					if (feedbackCP < onThreshold && feedbackCP > (onThreshold - onDelta))
						rowToLight = 3;
				}
			}
		}
		return rowToLight;
	}
};// GateSeq16 : module

struct GateSeq16Widget : ModuleWidget {
		
	GateSeq16Widget(GateSeq16 *module) : ModuleWidget(module) {
		// Main panel from Inkscape
		setPanel(SVG::load(assetPlugin(plugin, "res/GateSeq16.svg")));

		// Screw holes (optical illustion makes screws look oval, remove for now)
		/*addChild(new ScrewHole(Vec(15, 0)));
		addChild(new ScrewHole(Vec(box.size.x-30, 0)));
		addChild(new ScrewHole(Vec(15, 365)));
		addChild(new ScrewHole(Vec(box.size.x-30, 365)));*/
		
		// Screws
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 0)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilverRandomRot>(Vec(box.size.x-30, 365)));

		
		
		// ****** Sides (8 rows) ******
		
		static const int rowRuler0 = 40;
		static const int colRuler0 = 20;
		static const int colRuler6 = 406;
		static const int rowSpacingSides = 40;
		
		// Clock inputs
		int iSides = 0;
		for (; iSides < 4; iSides++) {
			addInput(Port::create<PJ301MPortS>(Vec(colRuler0, rowRuler0 + iSides * rowSpacingSides), Port::INPUT, module, GateSeq16::CLOCK_INPUTS + iSides));
		}
		// Run CVs
		for (; iSides < 8; iSides++) {
			addInput(Port::create<PJ301MPortS>(Vec(colRuler0, rowRuler0 + 5 + iSides * rowSpacingSides), Port::INPUT, module, GateSeq16::RUNCV_INPUTS + iSides - 4));
			
		}
		// Run LED bezel and light, four times
		for (iSides = 4; iSides < 8; iSides++) {
			addParam(ParamWidget::create<LEDBezel>(Vec(colRuler0 + 43 + offsetLEDbezel, rowRuler0 + 5 + iSides * rowSpacingSides + offsetLEDbezel), module, GateSeq16::RUN_PARAMS + iSides - 4, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRuler0 + 43 + offsetLEDbezel + offsetLEDbezelLight, rowRuler0 + 5 + iSides * rowSpacingSides + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq16::RUN_LIGHTS + iSides - 4));
		}

		// Outputs
		iSides = 0;
		for (; iSides < 4; iSides++) {
			addOutput(Port::create<PJ301MPortS>(Vec(colRuler6, rowRuler0 + iSides * rowSpacingSides), Port::OUTPUT, module, GateSeq16::GATE_OUTPUTS + iSides));
		}
		// Prob knobs
		for (; iSides < 8; iSides++) {
			addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(colRuler6 + offsetRoundSmallBlackKnob, rowRuler0 + 5 + iSides * rowSpacingSides + offsetRoundSmallBlackKnob), module, GateSeq16::PROB_PARAMS + iSides - 4, 0.0f, 1.0f, 1.0f));
		}
		
		
		
		// ****** Steps LED buttons ******
		
		static int colRulerSteps = 60;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		
		for (int y = 0; y < 4; y++) {
			int posX = colRulerSteps;
			for (int x = 0; x < 16; x++) {
				addParam(ParamWidget::create<LEDButton>(Vec(posX, rowRuler0 + 8 + y * rowSpacingSides - 4.4f), module, GateSeq16::STEP_PARAMS + y * 16 + x, 0.0f, 1.0f, 0.0f));
				addChild(ModuleLightWidget::create<MediumLight<RedYellowGreenBlueLight>>(Vec(posX + 4.4f, rowRuler0 + 8 + y * rowSpacingSides), module, GateSeq16::STEP_LIGHTS + (y * 16 + x) * 4));
				posX += spacingSteps;
				if ((x + 1) % 4 == 0)
					posX += spacingSteps4;
			}
		}
			
		
		
		// ****** 4x3 Main center bottom half Control section ******
		
		static int colRulerC0 = 110;
		static int colRulerSpacing = 72;
		static int colRulerC1 = colRulerC0 + colRulerSpacing;
		static int colRulerC2 = colRulerC1 + colRulerSpacing;
		static int colRulerC3 = colRulerC2 + colRulerSpacing;
		static int rowRulerC0 = 217;
		static int rowRulerSpacing = 48;
		static int rowRulerC1 = rowRulerC0 + rowRulerSpacing;
		static int rowRulerC2 = rowRulerC1 + rowRulerSpacing;
		static const int posLEDvsButton = + 25;
		
		// Length light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC0 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq16::LENGTH_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC0 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq16::LENGTH_LIGHT));		
		// Mode light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC1 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq16::MODE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<BlueLight>>(Vec(colRulerC1 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq16::MODE_LIGHT));		
		// GateTrig light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC2 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq16::GATETRIG_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<BlueLight>>(Vec(colRulerC2 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq16::GATETRIG_LIGHT));		
		// Gate light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC3 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq16::GATE_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(colRulerC3 + posLEDvsButton + offsetMediumLight, rowRulerC0 + offsetMediumLight), module, GateSeq16::GATE_LIGHT));		
		
		// Runall button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC0 + offsetCKD6b, rowRulerC1 + offsetCKD6b), module, GateSeq16::RUNALL_PARAM, 0.0f, 1.0f, 0.0f));
		// Reset LED bezel and light
		addParam(ParamWidget::create<LEDBezel>(Vec(colRulerC1 + offsetLEDbezel, rowRulerC1 + offsetLEDbezel), module, GateSeq16::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MuteLight<GreenLight>>(Vec(colRulerC1 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC1 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq16::RESET_LIGHT));
		// Copy/paste switches
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2 - 10, rowRulerC1 + offsetTL1105), module, GateSeq16::COPY_PARAM, 0.0f, 1.0f, 0.0f));
		addParam(ParamWidget::create<TL1105>(Vec(colRulerC2 + 20, rowRulerC1 + offsetTL1105), module, GateSeq16::PASTE_PARAM, 0.0f, 1.0f, 0.0f));
		// Gate p light and button
		addParam(ParamWidget::create<CKD6b>(Vec(colRulerC3 + offsetCKD6b, rowRulerC1 + offsetCKD6b), module, GateSeq16::GATEP_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(colRulerC3 + posLEDvsButton + offsetMediumLight, rowRulerC1 + offsetMediumLight), module, GateSeq16::GATEP_LIGHT));		
		
		// Runall CV input
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC0, rowRulerC2), Port::INPUT, module, GateSeq16::RUNALLCV_INPUT));
		// Reset
		addInput(Port::create<PJ301MPortS>(Vec(colRulerC1, rowRulerC2 ), Port::INPUT, module, GateSeq16::RESET_INPUT));
		// Run CV mode switch
		addParam(ParamWidget::create<CKSS>(Vec(colRulerC2 + hOffsetCKSS, rowRulerC2 + vOffsetCKSS), module, GateSeq16::RUNCV_MODE_PARAM, 0.0f, 1.0f, 1.0f)); // 1.0f is top position
		// Config switch (3 position)
		addParam(ParamWidget::create<CKSSThreeInv>(Vec(colRulerC3 + hOffsetCKSS, rowRulerC2 + vOffsetCKSSThree), module, GateSeq16::CONFIG_PARAM, 0.0f, 2.0f, 0.0f));	// 0.0f is top position
	}
};

Model *modelGateSeq16 = Model::create<GateSeq16, GateSeq16Widget>("Impromptu Modular", "Gate-Seq-16", "Gate-Seq-16", SEQUENCER_TAG);

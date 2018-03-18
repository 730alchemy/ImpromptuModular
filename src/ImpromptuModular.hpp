//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//***********************************************************************************************




#include "rack.hpp"


using namespace rack;


extern Plugin *plugin;

// All modules that are part of plugin go here
extern Model *modelWriteSeq32;
extern Model *modelWriteSeq64;



// General constants
static const float lightLambda = 0.075f;


// Constants for displaying notes

static const char noteLettersSharp[12] = {'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B'};
static const char noteLettersFlat [12] = {'C', 'D', 'D', 'E', 'E', 'F', 'G', 'G', 'A', 'A', 'B', 'B'};
static const char isBlackKey      [12] = { 0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0 };


// Component offset constants

static const int hOffsetCKSS = 5;
static const int vOffsetCKSS = 2;
static const int vOffsetCKSSThree = -2;//TODO adjust
static const int hOffsetCKSSH = 2;
static const int vOffsetCKSSH = 5;
static const int offsetCKD6 = -1;//does both h and v
static const int vOffsetDisplay = -2;
static const int offsetDavies1900 = -6;//does both h and v
static const int offsetMediumLight = 9;
static const float offsetLEDbutton = 3.0f;//does both h and v
static const float offsetLEDbuttonLight = 4.4f;//does both h and v
static const int offsetTL1105 = 4;//does both h and v
static const int offsetLEDbezel = 1;//does both h and v
static const float offsetLEDbezelLight = 2.2f;//does both h and v
static const int offsetTrimpot = 3;//does both h and v



// Variations on existing knobs, lights, etc

struct Davies1900hBlackSnapKnob : Davies1900hBlackKnob {
	Davies1900hBlackSnapKnob() {
		snap = true;
		smooth = false;
	}
};

struct Davies1900hBlackKnobNoTick : Davies1900hKnob {
	Davies1900hBlackKnobNoTick() {
		setSVG(SVG::load(assetPlugin(plugin, "res/Davies1900hBlackNoTick.svg")));
		speed = 0.9f;
		smooth = false;
	}
	//void reset() override {} // called when initialize module (right click)
};


struct CKSSThreeInv : SVGSwitch, ToggleSwitch {
	CKSSThreeInv() {
		addFrame(SVG::load(assetGlobal("res/ComponentLibrary/CKSSThree_2.svg")));
		addFrame(SVG::load(assetGlobal("res/ComponentLibrary/CKSSThree_1.svg")));
		addFrame(SVG::load(assetGlobal("res/ComponentLibrary/CKSSThree_0.svg")));
	}
};

template <typename BASE>
struct MuteLight : BASE {
	MuteLight() {
		this->box.size = mm2px(Vec(6.0f, 6.0f));
	}
};

struct CKSSH : SVGSwitch, ToggleSwitch {
	CKSSH() {
		addFrame(SVG::load(assetPlugin(plugin, "res/CKSSH_0.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/CKSSH_1.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};
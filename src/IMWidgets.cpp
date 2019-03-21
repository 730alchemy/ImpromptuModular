//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé 
//
//Based on code from Valley Rack Free by Dale Johnson
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "IMWidgets.hpp"



// Dynamic SVGScrew


ScrewCircle::ScrewCircle(float _angle) {
	static const float highRadius = 1.4f;// radius for 0 degrees (screw looks like a +)
	static const float lowRadius = 1.1f;// radius for 45 degrees (screw looks like an x)
	angle = _angle;
	_angle = fabsf(angle - ((float)M_PI)/4.0f);
	radius = ((highRadius - lowRadius)/(M_PI/4.0f)) * _angle + lowRadius;
}

void ScrewCircle::draw(const DrawArgs &args) {
	NVGcolor backgroundColor = nvgRGB(0x72, 0x72, 0x72); 
	NVGcolor borderColor = nvgRGB(0x72, 0x72, 0x72);
	nvgBeginPath(args.vg);
	nvgCircle(args.vg, box.size.x/2.0f, box.size.y/2.0f, radius);// box, radius
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1.0);
	nvgStrokeColor(args.vg, borderColor);
	nvgStroke(args.vg);
}

DynamicSVGScrew::DynamicSVGScrew() {
    mode = NULL;
    oldMode = -1;
 
	
	// for random rotated screw used in primary mode (code copied from ImpromptuModular.cpp ScrewSilverRandomRot::ScrewSilverRandomRot())
	// **********
	float angle0_90 = random::uniform()*M_PI/2.0f;
	//float angle0_90 = random::uniform() > 0.5f ? M_PI/4.0f : 0.0f;// for testing
	
	tw = new TransformWidget();
	addChild(tw);
	
	sw = new widget::SvgWidget();
	tw->addChild(sw);
	sw->setSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/ScrewSilver.svg")));
	
	sc = new ScrewCircle(angle0_90);
	sc->box.size = sw->box.size;
	tw->addChild(sc);
	
	box.size = sw->box.size;
	tw->box.size = sw->box.size; 
	tw->identity();
	// Rotate SVG
	Vec center = sw->box.getCenter();
	tw->translate(center);
	tw->rotate(angle0_90);
	tw->translate(center.neg());	

	// for fixed svg screw used in alternate mode
	// **********
	swAlt = new widget::SvgWidget();
	swAlt->visible = false;
    addChild(swAlt);
}

void DynamicSVGScrew::addSVGalt(std::shared_ptr<Svg> svg) {
    if(!swAlt->svg) {
        swAlt->setSvg(svg);
    }
}

void DynamicSVGScrew::step() {
    if(mode != NULL && *mode != oldMode) {
        if ((*mode) == 0) {
			sw->visible = true;
			swAlt->visible = false;
		}
		else {
			sw->visible = false;
			swAlt->visible = true;
		}
        oldMode = *mode;
        dirty = true;
    }
	FramebufferWidget::step();
}



// Dynamic SVGPanel

void PanelBorderWidget::draw(const DrawArgs &args) {  // first 6 lines were copied from app/SvgPanel.cpp
    NVGcolor borderColor = nvgRGBAf(0.5, 0.5, 0.5, 0.5);
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);// full rect of module (including expansion area if a module has one)
    nvgStrokeColor(args.vg, borderColor);
    nvgStrokeWidth(args.vg, 1.0);
    nvgStroke(args.vg);
}

DynamicSVGPanel::DynamicSVGPanel() {
    mode = NULL;
    oldMode = -1;
    visiblePanel = new widget::SvgWidget();
    addChild(visiblePanel);
    border = new PanelBorderWidget();
    addChild(border);
}

void DynamicSVGPanel::addPanel(std::shared_ptr<Svg> svg) {
    panels.push_back(svg);
    if(!visiblePanel->svg) {
        visiblePanel->setSvg(svg);
        box.size = visiblePanel->box.size.div(RACK_GRID_SIZE).round().mult(RACK_GRID_SIZE);
        border->box.size = box.size;
    }
}
void DynamicSVGPanel::dupPanel() {
    panels.push_back(panels[panels.size() - 1]);
}

void DynamicSVGPanel::step() { 
    if(mode != NULL && *mode != oldMode) {
        visiblePanel->setSvg(panels[*mode]);
        oldMode = *mode;
        dirty = true;
    }
	FramebufferWidget::step();
}



// Dynamic SVGPort

DynamicSVGPort::DynamicSVGPort() {
    mode = NULL;
    oldMode = -1;
	//SvgPort constructor automatically called
}

void DynamicSVGPort::addFrame(std::shared_ptr<Svg> svg) {
    frames.push_back(svg);
    if(!sw->svg)
        SVGPort::setSvg(svg);
}

void DynamicSVGPort::step() {
    if(mode != NULL && *mode != oldMode) {
        sw->setSvg(frames[std::min(*mode, (int)(frames.size() - 1))]);
        oldMode = *mode;
        fb->dirty = true;
    }
	PortWidget::step();
}



// Dynamic SVGSwitch

DynamicSVGSwitch::DynamicSVGSwitch() {
    mode = NULL;
    oldMode = -1;
	//SvgSwitch constructor automatically called
}

void DynamicSVGSwitch::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 2) {
		addFrame(framesAll[0]);
		addFrame(framesAll[1]);
	}
}

void DynamicSVGSwitch::step() {
    if(mode != NULL && *mode != oldMode) {
        if ((*mode) == 0) {
			frames[0]=framesAll[0];
			frames[1]=framesAll[1];
		}
		else {
			frames[0]=framesAll[2];
			frames[1]=framesAll[3];
		}
        oldMode = *mode;
		onChange(*(new widget::ChangeEvent()));// required because of the way SVGSwitch changes images, we only change the frames above.
		fb->dirty = true;// dirty is not sufficient when changing via frames assignments above (i.e. onChange() is required)
    }
	SvgSwitch::step();
}



// Dynamic SVGKnob

DynamicSVGKnob::DynamicSVGKnob() {
    mode = NULL;
    oldMode = -1;
	effect = new widget::SvgWidget();
	//SvgKnob constructor automatically called
}

void DynamicSVGKnob::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 1) {
		setSvg(svg);
	}
}

void DynamicSVGKnob::addEffect(std::shared_ptr<Svg> svg) {
    effect->setSvg(svg);
	effect->visible = false;
	addChild(effect);
}

void DynamicSVGKnob::step() {
    if(mode != NULL && *mode != oldMode) {
        if ((*mode) == 0) {
			setSvg(framesAll[0]);
			effect->visible = false;
		}
		else {
			setSvg(framesAll[1]);
			effect->visible = true;
		}
        oldMode = *mode;
		fb->dirty = true;
    }
	SvgKnob::step();
}



// Dynamic IMTactile
/*
DynamicIMTactile::DynamicIMTactile() {
	snap = false;
	//smooth = false;// must be false or else DynamicIMTactile::changeValue() call from module will crash Rack
	wider = nullptr;
	paramReadRequest = nullptr;
	oldWider = -1.0f;
	box.size = Vec(padWidth, padHeight);
}

void DynamicIMTactile::process(const ProcessArgs &args) {
   if(wider != nullptr && *wider != oldWider) {
        if ((*wider) > 0.5f) {
			box.size = Vec(padWidthWide, padHeight);
		}
		else {
			box.size = Vec(padWidth, padHeight);
		}
        oldWider = *wider;
    }	
	if (paramReadRequest != nullptr) {
		float readVal = *paramReadRequest;
		if (readVal != -10.0f) {
			setValue(readVal);
			*paramReadRequest = -10.0f;
		}
	}
	FramebufferWidget::step();
}
*/

/*
void DynamicIMTactile::onDragStart(EventDragStart &e) {
	dragValue = value;
	dragY = gRackWidget->lastMousePos.y;
}

void DynamicIMTactile::onDragMove(EventDragMove &e) {
	float rangeValue = maxValue - minValue;// infinite not supported (not relevant)
	float newDragY = gRackWidget->lastMousePos.y;
	float delta = -(newDragY - dragY) * rangeValue / box.size.y;
	dragY = newDragY;
	dragValue += delta;
	float dragValueClamped = clamp2(dragValue, minValue, maxValue);
	if (snap)
		dragValueClamped = roundf(dragValueClamped);
	setValue(dragValueClamped);
}
*/

/*
void DynamicIMTactile::onButton(const widget::ButtonEvent &e) {
	if (e.action == GLFW_PRESS) {
		float val = rescale(e.pos.y, box.size.y, 0.0f , minValue, maxValue);
		if (snap)
			val = roundf(val);
		setValue(val);
	}
	ParamWidget::onButton(e);
}
*/

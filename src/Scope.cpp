#include <string.h>
#ifdef PQ_VERSION
#include "PQ.hpp"
#include "Common/Branding.hpp"
#define FONT "res/fonts/LessPerfectDOSVGA.ttf"
#define LETTER_SPACING -1.25
#define FONT_SIZE 12
#else
#include "Fundamental.hpp"
#define FONT "res/fonts/Sudo.ttf"
#define LETTER_SPACING -2
#define FONT_SIZE 13
#endif
#include "dsp/digital.hpp"

#define BUFFER_SIZE 512

struct Scope : Module {
	enum ParamIds {
		X_SCALE_PARAM,
		X_POS_PARAM,
		Y_SCALE_PARAM,
		Y_POS_PARAM,
		TIME_PARAM,
		LISSAJOUS_PARAM,
		TRIG_PARAM,
		EXTERNAL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		Y_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		PLOT_LIGHT,
		LISSAJOUS_LIGHT,
		INTERNAL_LIGHT,
		EXTERNAL_LIGHT,
		NUM_LIGHTS
	};

	float bufferX[BUFFER_SIZE] = {};
	float bufferY[BUFFER_SIZE] = {};
	int bufferIndex = 0;
	float frameIndex = 0;

	SchmittTrigger sumTrigger;
	SchmittTrigger extTrigger;
	bool lissajous = false;
	bool external = false;
	SchmittTrigger resetTrigger;
	Port *xPort, *yPort;

	Scope() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;

	json_t *toJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "lissajous", json_integer((int) lissajous));
		json_object_set_new(rootJ, "external", json_integer((int) external));
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *sumJ = json_object_get(rootJ, "lissajous");
		if (sumJ)
			lissajous = json_integer_value(sumJ);

		json_t *extJ = json_object_get(rootJ, "external");
		if (extJ)
			external = json_integer_value(extJ);
	}

	void onReset() override {
		lissajous = false;
		external = false;
	}
};


void Scope::step() {
	// Modes
	if (sumTrigger.process(params[LISSAJOUS_PARAM].value)) {
		lissajous = !lissajous;
	}
	lights[PLOT_LIGHT].value = lissajous ? 0.0f : 1.0f;
	lights[LISSAJOUS_LIGHT].value = lissajous ? 1.0f : 0.0f;

	if (extTrigger.process(params[EXTERNAL_PARAM].value)) {
		external = !external;
	}
	lights[INTERNAL_LIGHT].value = external ? 0.0f : 1.0f;
	lights[EXTERNAL_LIGHT].value = external ? 1.0f : 0.0f;

	// Compute time
	float deltaTime = powf(2.0f, params[TIME_PARAM].value);
	int frameCount = (int)ceilf(deltaTime * engineGetSampleRate());

	// Add frame to buffer
	if (bufferIndex < BUFFER_SIZE) {
		if (++frameIndex > frameCount) {
			frameIndex = 0;
			bufferX[bufferIndex] = inputs[X_INPUT].value;
			bufferY[bufferIndex] = inputs[Y_INPUT].value;
			bufferIndex++;
		}
	}

	// Are we waiting on the next trigger?
	if (bufferIndex >= BUFFER_SIZE) {
		// Trigger immediately if external but nothing plugged in, or in Lissajous mode
		if (lissajous || (external && !inputs[TRIG_INPUT].active)) {
			bufferIndex = 0;
			frameIndex = 0;
			return;
		}

		// Reset the Schmitt trigger so we don't trigger immediately if the input is high
		if (frameIndex == 0) {
			resetTrigger.reset();
		}
		frameIndex++;

		// Must go below 0.1fV to trigger
		float gate = external ? inputs[TRIG_INPUT].value : inputs[X_INPUT].value;

		// Reset if triggered
		float holdTime = 0.1f;
		if (resetTrigger.process(rescale(gate, params[TRIG_PARAM].value - 0.1f, params[TRIG_PARAM].value, 0.f, 1.f)) || (frameIndex >= engineGetSampleRate() * holdTime)) {
			bufferIndex = 0; frameIndex = 0; return;
		}

		// Reset if we've waited too long
		if (frameIndex >= engineGetSampleRate() * holdTime) {
			bufferIndex = 0; frameIndex = 0; return;
		}
	}
}


struct ScopeDisplay : TransparentWidget {
	Scope *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vrms, vpp, vmin, vmax;
		void calculate(float *values) {
			vrms = 0.0f;
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float v = values[i];
				vrms += v*v;
				vmax = fmaxf(vmax, v);
				vmin = fminf(vmin, v);
			}
			vrms = sqrtf(vrms / BUFFER_SIZE);
			vpp = vmax - vmin;
		}
	};
	Stats statsX, statsY;

	ScopeDisplay() {
		font = Font::load(assetPlugin(plugin, FONT));
	}

	void drawWaveform(NVGcontext *vg, float *valuesX, float *valuesY) {
		if (!valuesX)
			return;
		nvgSave(vg);
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(vg);
		// Draw maximum display left to right
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float x, y;
			if (valuesY) {
				x = valuesX[i] / 2.0f + 0.5f;
				y = valuesY[i] / 2.0f + 0.5f;
			}
			else {
				x = (float)i / (BUFFER_SIZE - 1);
				y = valuesX[i] / 2.0f + 0.5f;
			}
			Vec p;
			p.x = b.pos.x + b.size.x * x;
			p.y = b.pos.y + b.size.y * (1.0f - y);
			if (i == 0)
				nvgMoveTo(vg, p.x, p.y);
			else
				nvgLineTo(vg, p.x, p.y);
		}
		nvgLineCap(vg, NVG_ROUND);
		nvgMiterLimit(vg, 2.0f);
		nvgStrokeWidth(vg, 1.5f);
		nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
		nvgStroke(vg);
		nvgResetScissor(vg);
		nvgRestore(vg);
	}

	void drawTrig(NVGcontext *vg, float value) {
		Rect b = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
		nvgScissor(vg, b.pos.x, b.pos.y, b.size.x, b.size.y);

		value = value / 2.0f + 0.5f;
		Vec p = Vec(box.size.x, b.pos.y + b.size.y * (1.0f - value));

		// Draw line
		nvgStrokeColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
		{
			nvgBeginPath(vg);
			nvgMoveTo(vg, p.x - 13, p.y);
			nvgLineTo(vg, 0, p.y);
			nvgClosePath(vg);
		}
		nvgStroke(vg);

		// Draw indicator
		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x60));
		{
			nvgBeginPath(vg);
			nvgMoveTo(vg, p.x - 2, p.y - 4);
			nvgLineTo(vg, p.x - 9, p.y - 4);
			nvgLineTo(vg, p.x - 13, p.y);
			nvgLineTo(vg, p.x - 9, p.y + 4);
			nvgLineTo(vg, p.x - 2, p.y + 4);
			nvgClosePath(vg);
		}
		nvgFill(vg);

		nvgFontSize(vg, 9);
		nvgFontFaceId(vg, font->handle);
		nvgFillColor(vg, nvgRGBA(0x1e, 0x28, 0x2b, 0xff));
		nvgText(vg, p.x - 8, p.y + 3, "T", NULL);
		nvgResetScissor(vg);
	}

	void drawStats(NVGcontext *vg, Vec pos, const char *title, Stats *stats, NVGcolor color) {
		nvgFontSize(vg, FONT_SIZE);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, LETTER_SPACING);

		//color.a = 0x90;
		nvgFillColor(vg, color);
		nvgText(vg, pos.x + 6, pos.y + 11, title, NULL);

		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		char text[128];
		snprintf(text, sizeof(text), "pp % 06.2f  max % 06.2f  min % 06.2f", stats->vpp, stats->vmax, stats->vmin);
		nvgText(vg, pos.x + 22, pos.y + 11, text, NULL);
	}

	void draw(NVGcontext *vg) override {
		assert(module->xPort);
		assert(module->yPort);
		float gainX = powf(2.0f, roundf(module->params[Scope::X_SCALE_PARAM].value));
		float gainY = powf(2.0f, roundf(module->params[Scope::Y_SCALE_PARAM].value));
		float offsetX = module->params[Scope::X_POS_PARAM].value;
		float offsetY = module->params[Scope::Y_POS_PARAM].value;

		float valuesX[BUFFER_SIZE];
		float valuesY[BUFFER_SIZE];
		for (int i = 0; i < BUFFER_SIZE; i++) {
			int j = i;
			// Lock display to buffer if buffer update deltaTime <= 2^-11
			if (module->lissajous)
				j = (i + module->bufferIndex) % BUFFER_SIZE;
			valuesX[i] = (module->bufferX[j] + offsetX) * gainX / 10.0f;
			valuesY[i] = (module->bufferY[j] + offsetY) * gainY / 10.0f;
		}

		// Compute colors
		NVGcolor xColor, yColor;
		xColor = getPortColor(module->xPort);
		yColor = getPortColor(module->yPort);
		if (memcmp(&xColor, &yColor, sizeof(xColor)) == 0 ) {
			// if the colors are the same pick novel colors
			NVGcolor tmp = xColor;
			tmp.r = xColor.g;
			tmp.g = xColor.b;
			tmp.b = xColor.r;
			xColor = tmp;
			tmp.r = yColor.b;
			tmp.g = yColor.r;
			tmp.b = yColor.g;
			yColor = tmp;
		}

		// Draw waveforms
		if (module->lissajous) {
			// X x Y
			if (module->inputs[Scope::X_INPUT].active || module->inputs[Scope::Y_INPUT].active) {
				nvgStrokeColor(vg, getPortColor(nullptr));
				drawWaveform(vg, valuesX, valuesY);
			}
		}
		else {
			// Y
			if (module->inputs[Scope::Y_INPUT].active) {
				nvgStrokeColor(vg, yColor);
				drawWaveform(vg, valuesY, NULL);
			}

			// X
			if (module->inputs[Scope::X_INPUT].active) {
				nvgStrokeColor(vg, xColor);
				drawWaveform(vg, valuesX, NULL);
			}

			float valueTrig = (module->params[Scope::TRIG_PARAM].value + offsetX) * gainX / 10.0f;
			drawTrig(vg, valueTrig);
		}

		// Calculate and draw stats
		if (++frame >= 4) {
			frame = 0;
			statsX.calculate(module->bufferX);
			statsY.calculate(module->bufferY);
		}

		NVGcolor statsColor = nvgRGBA(0xff, 0xff, 0xff, 0x40);
		if (!module->inputs[Scope::X_INPUT].active) {
			xColor = statsColor;
		}
		if (!module->inputs[Scope::Y_INPUT].active) {
			yColor = statsColor;
		}
		drawStats(vg, Vec(0, 0), "X", &statsX, xColor);
		drawStats(vg, Vec(0, box.size.y - 15), "Y", &statsY, yColor);
	}

	NVGcolor getPortColor(Port* port) {
		WireWidget *wire = gRackWidget->wireContainer->getTopWire(port);
		NVGcolor color = wire ? wire->color : nvgRGBA(0x9f, 0xe4, 0x36, 0xc0);
		//color.a = 0xc0;
		return color;
	}

};


struct ScopeWidget : ModuleWidget {
	ScopeWidget(Scope *module);
};

ScopeWidget::ScopeWidget(Scope *module) : ModuleWidget(module) {
	setPanel(SVG::load(assetPlugin(plugin, "res/Scope.svg")));

	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));

	{
		ScopeDisplay *display = new ScopeDisplay();
		display->module = module;
		display->box.pos = Vec(0, 44);
		display->box.size = Vec(box.size.x, 140);
		addChild(display);
	}

	addParam(ParamWidget::create<RoundBlackSnapKnob>(Vec(15, 209), module, Scope::X_SCALE_PARAM, -2.0f, 8.0f, 0.0f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(15, 263), module, Scope::X_POS_PARAM, -10.0f, 10.0f, 0.0f));
	addParam(ParamWidget::create<RoundBlackSnapKnob>(Vec(61, 209), module, Scope::Y_SCALE_PARAM, -2.0f, 8.0f, 0.0f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(61, 263), module, Scope::Y_POS_PARAM, -10.0f, 10.0f, 0.0f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(107, 209), module, Scope::TIME_PARAM, -6.0f, -16.0f, -14.0f));
	addParam(ParamWidget::create<CKD6>(Vec(106, 262), module, Scope::LISSAJOUS_PARAM, 0.0f, 1.0f, 0.0f));
	addParam(ParamWidget::create<RoundBlackKnob>(Vec(153, 209), module, Scope::TRIG_PARAM, -10.0f, 10.0f, 0.0f));
	addParam(ParamWidget::create<CKD6>(Vec(152, 262), module, Scope::EXTERNAL_PARAM, 0.0f, 1.0f, 0.0f));

	module->xPort = dynamic_cast<Port*>(Port::create<PJ301MPort>(Vec(17, 319), Port::INPUT, module, Scope::X_INPUT));
	addInput(module->xPort);
	module->yPort = dynamic_cast<Port*>(Port::create<PJ301MPort>(Vec(63, 319), Port::INPUT, module, Scope::Y_INPUT));
	addInput(module->yPort);

	addInput(Port::create<PJ301MPort>(Vec(154, 319), Port::INPUT, module, Scope::TRIG_INPUT));

	addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(104, 251), module, Scope::PLOT_LIGHT));
	addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(104, 296), module, Scope::LISSAJOUS_LIGHT));
	addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(150, 251), module, Scope::INTERNAL_LIGHT));
	addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(Vec(150, 296), module, Scope::EXTERNAL_LIGHT));
}


#ifdef PQ_VERSION
Model *modelScope = Model::create<Scope, ScopeWidget>("Pulsum Quadratum", "Scope", "Color Scope", VISUAL_TAG);
#else
Model *modelScope = Model::create<Scope, ScopeWidget>("Pulsum Quadratum", "GenericScope", "Generic Color Scope", VISUAL_TAG);
#endif

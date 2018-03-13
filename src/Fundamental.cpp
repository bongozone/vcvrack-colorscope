#include "Fundamental.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "PulsumQuadratum-Fundamental";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif
	p->website = "https://github.com/WIZARDISHUNGRY/vcvrack-colorscope";

	p->addModel(createModel<ScopeWidget>("Pulsum Quadratum", "Scope", "Color Scope", VISUAL_TAG));
}

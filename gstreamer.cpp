#include <node.h>
#include <v8.h>

#include "GLibHelpers.h"
#include "GObjectWrap.h"
#include "Pipeline.h"


void init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE exports) {
	gst_init(NULL, NULL);
	GObjectWrap::Init();
	Pipeline::Init(exports);
}

NODE_MODULE(gstreamer_superficial, init);

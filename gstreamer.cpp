#include <node.h>
#include <v8.h>

#include "GLibHelpers.h"
#include "GObjectWrap.h"
#include "Pipeline.h"


void init( v8::Handle<v8::Object> exports ) {
	gst_init( NULL, NULL );
	GObjectWrap::Init();
	Pipeline::Init(exports);
}

NODE_MODULE(gstreamer_superficial, init);

#include <nan.h>

#include <gst/gst.h>
#include <gst/video/video.h>

#include "GLibHelpers.h"
#include "GObjectWrap.h"
#include "Pipeline.h"

Nan::Persistent<Function> Pipeline::constructor;

#define NANOS_TO_DOUBLE(nanos)(((double)nanos)/1000000000)
//FIXME: guint64 overflow
#define DOUBLE_TO_NANOS(secs)((guint64)(secs*1000000000))

Pipeline::Pipeline(const char *launch) {
	GError *err = NULL;

	pipeline = (GstPipeline*)GST_BIN(gst_parse_launch(launch, &err));
	if(err) {
		fprintf(stderr,"GstError: %s\n", err->message);
		Nan::ThrowError(err->message);
	}
}

Pipeline::Pipeline(GstPipeline* pipeline) {
	this->pipeline = pipeline;
}

void Pipeline::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE exports) {
	Nan::HandleScope scope;
	
	Local<FunctionTemplate> ctor = Nan::New<FunctionTemplate>(Pipeline::New);
	ctor->InstanceTemplate()->SetInternalFieldCount(1);
	ctor->SetClassName(Nan::New("Pipeline").ToLocalChecked());
	
	Local<ObjectTemplate> proto = ctor->PrototypeTemplate();
	// Prototype
	Nan::SetPrototypeMethod(ctor, "play", Play);
	Nan::SetPrototypeMethod(ctor, "pause", Pause);
	Nan::SetPrototypeMethod(ctor, "stop", Stop);
	Nan::SetPrototypeMethod(ctor, "forceKeyUnit", ForceKeyUnit);
	Nan::SetPrototypeMethod(ctor, "findChild", FindChild);
	Nan::SetPrototypeMethod(ctor, "pollBus", PollBus);
			
	Nan::SetAccessor(proto, Nan::New("auto-flush-bus").ToLocalChecked(), GetAutoFlushBus, SetAutoFlushBus);
	Nan::SetAccessor(proto, Nan::New("delay").ToLocalChecked(), GetDelay, SetDelay);
	Nan::SetAccessor(proto, Nan::New("latency").ToLocalChecked(), GetLatency, SetLatency);
	
	constructor.Reset(ctor->GetFunction());
	exports->Set(Nan::New("Pipeline").ToLocalChecked(), ctor->GetFunction());
}

NAN_METHOD(Pipeline::New) {
	if (!info.IsConstructCall())
		return Nan::ThrowTypeError("Class constructors cannot be invoked without 'new'");

	String::Utf8Value launch(info[0]->ToString());

	Pipeline* obj = new Pipeline(*launch);
	obj->Wrap(info.This());
	info.GetReturnValue().Set(info.This());
}

void Pipeline::play() {
	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
}

NAN_METHOD(Pipeline::Play) {
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	obj->play();
}

void Pipeline::stop() {
	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
}

NAN_METHOD(Pipeline::Stop) {
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	obj->stop();
}

void Pipeline::pause() {
	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PAUSED);
}

NAN_METHOD(Pipeline::Pause) {
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	obj->pause();
}

void Pipeline::forceKeyUnit(GObject *sink, int cnt) {
	GstPad *sinkpad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
	gst_pad_push_event(sinkpad,(GstEvent*) gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, cnt));
}

NAN_METHOD(Pipeline::ForceKeyUnit) {
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	String::Utf8Value name(info[0]->ToString());
	GObject *o = obj->findChild(*name);
	int cnt(info[1]->Int32Value());
	obj->forceKeyUnit(o, cnt);
	info.GetReturnValue().Set(Nan::True());
}

GObject * Pipeline::findChild(const char *name) {
	GstElement *e = gst_bin_get_by_name(GST_BIN(pipeline), name);
	return G_OBJECT(e);
}

NAN_METHOD(Pipeline::FindChild) {
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	String::Utf8Value name(info[0]->ToString());
	GObject *o = obj->findChild(*name);
	if(o)
		info.GetReturnValue().Set(GObjectWrap::NewInstance(info, o));
	else
		info.GetReturnValue().Set(Nan::Undefined());
}

struct BusRequest {
	uv_work_t request;
	Nan::Persistent<Function> callback;
	Pipeline *obj;
	GstMessage *msg;
};

void Pipeline::_doPollBus(uv_work_t *req) {
	BusRequest *br = static_cast<BusRequest*>(req->data);
	GstBus *bus = gst_element_get_bus(GST_ELEMENT(br->obj->pipeline));
	if(!bus) return;
	br->msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
}

void Pipeline::_polledBus(uv_work_t *req, int n) {
	BusRequest *br = static_cast<BusRequest*>(req->data);

	if(br->msg) {
		Nan::HandleScope scope;
		Local<Object> m = Nan::New<Object>();
		m->Set(Nan::New("type").ToLocalChecked(), Nan::New(GST_MESSAGE_TYPE_NAME(br->msg)).ToLocalChecked());
	
		const GstStructure *structure = (GstStructure*)gst_message_get_structure(br->msg);
		if(structure)
			gst_structure_to_v8(m, structure);

		if(GST_MESSAGE_TYPE(br->msg) == GST_MESSAGE_ERROR) {
			GError *err = NULL;
			gchar *name = gst_object_get_path_string(br->msg->src);
			m->Set(Nan::New("name").ToLocalChecked(), Nan::New(name).ToLocalChecked());
			gst_message_parse_error(br->msg, &err, NULL);
			m->Set(Nan::New("message").ToLocalChecked(), Nan::New(err->message).ToLocalChecked());
		}

		Local<Value> argv[1] = { m };
		Local<Function> cbCallbackLocal = Nan::New(br->callback);
		cbCallbackLocal->Call(Nan::GetCurrentContext()->Global(), 1, argv);
		
		gst_message_unref(br->msg);
	}
	if(GST_STATE(br->obj->pipeline) != GST_STATE_NULL)
		uv_queue_work(uv_default_loop(), &br->request, _doPollBus, _polledBus);
}

NAN_METHOD(Pipeline::PollBus) {
	Nan::EscapableHandleScope scope;
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());

	if(info.Length() == 0 || !info[0]->IsFunction()) {
		Nan::ThrowError("Callback is required and must be a Function.");
		scope.Escape(Nan::Undefined());
		return;
	}
  
	Handle<Function> callback = Handle<Function>::Cast(info[0]);
	
	BusRequest * br = new BusRequest();
	br->request.data = br;
	br->callback.Reset(callback);
	br->obj = obj;
	obj->Ref();
	
	uv_queue_work(uv_default_loop(), &br->request, _doPollBus, _polledBus);

	scope.Escape(Nan::Undefined());
}


NAN_GETTER(Pipeline::GetAutoFlushBus) {
	Pipeline *obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	info.GetReturnValue().Set(Nan::New<Boolean>(gst_pipeline_get_auto_flush_bus(obj->pipeline)));
}

NAN_SETTER(Pipeline::SetAutoFlushBus) {
	if(value->IsBoolean()) {
		Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
		gst_pipeline_set_auto_flush_bus(obj->pipeline, value->BooleanValue());
	}
}

NAN_GETTER(Pipeline::GetDelay) {
	Pipeline *obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	double secs = NANOS_TO_DOUBLE(gst_pipeline_get_delay(obj->pipeline));
	info.GetReturnValue().Set(Nan::New<Number>(secs));
}

NAN_SETTER(Pipeline::SetDelay) {
	if(value->IsNumber()) {
		Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
		gst_pipeline_set_delay(obj->pipeline, DOUBLE_TO_NANOS(value->NumberValue()));
	}
}
NAN_GETTER(Pipeline::GetLatency) {
	Pipeline *obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	double secs = NANOS_TO_DOUBLE(gst_pipeline_get_latency(obj->pipeline));
	info.GetReturnValue().Set(Nan::New<Number>(secs));
}
NAN_SETTER(Pipeline::SetLatency) {
	if(value->IsNumber()) {
		Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
		gst_pipeline_set_latency(obj->pipeline, DOUBLE_TO_NANOS(value->NumberValue()));
	}
}

#include <nan.h>

#include <gst/gst.h>
#include <gst/video/video.h>

#include "GLibHelpers.h"
#include "GObjectWrap.h"
#include "Pipeline.h"

Pipeline::Pipeline( const char *launch ) {
	GError *err = NULL;

	pipeline = GST_BIN(gst_parse_launch(launch, &err));
	if( err ) {
		fprintf(stderr,"GstError: %s\n", err->message );
		Nan::ThrowError(err->message);
	}
}

Pipeline::~Pipeline() {
}

void Pipeline::play() {
    gst_element_set_state( GST_ELEMENT(pipeline), GST_STATE_PLAYING );
}

void Pipeline::stop() {
    gst_element_set_state( GST_ELEMENT(pipeline), GST_STATE_NULL );
}

void Pipeline::pause() {
    gst_element_set_state( GST_ELEMENT(pipeline), GST_STATE_PAUSED );
}

void Pipeline::forceKeyUnit(GObject *sink, int cnt) {
    GstPad *sinkpad = gst_element_get_static_pad (GST_ELEMENT(sink), "sink");
    gst_pad_push_event (sinkpad, (GstEvent*) gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, cnt));
}

GObject * Pipeline::findChild( const char *name ) {
    GstElement *e = gst_bin_get_by_name( GST_BIN(pipeline), name );
    return G_OBJECT(e);
}

struct BusRequest {
	uv_work_t request;
	Nan::Persistent<v8::Function> callback;
	Pipeline *obj;
	GstMessage *msg;
};

void Pipeline::_doPollBus( uv_work_t *req ) {
	BusRequest *br = static_cast<BusRequest*>(req->data);
	GstBus *bus = gst_element_get_bus( GST_ELEMENT(br->obj->pipeline));
	if( !bus ) return;
	br->msg = gst_bus_timed_pop( bus, GST_CLOCK_TIME_NONE );
}

void Pipeline::_polledBus( uv_work_t *req, int n ) {
	BusRequest *br = static_cast<BusRequest*>(req->data);

	if( br->msg ) {
		Nan::HandleScope scope;

		v8::Local<v8::Object> m = Nan::New<v8::Object>();
		
		m->Set(
			Nan::New<v8::String>("type").ToLocalChecked(), 
			Nan::New<v8::String>(GST_MESSAGE_TYPE_NAME(br->msg)).ToLocalChecked()
			);
	
//        if( br->msg->structure ) {
//            gst_structure_to_v8( m, br->msg->structure );
//        }

		if( GST_MESSAGE_TYPE(br->msg) == GST_MESSAGE_ERROR ) {
			GError *err = NULL;
			gchar *name;
			name = gst_object_get_path_string (br->msg->src);
			m->Set(
				Nan::New<v8::String>("name").ToLocalChecked(), 
				Nan::New<v8::String>( name ).ToLocalChecked()
				);
			gst_message_parse_error (br->msg, &err, NULL);
			m->Set(
				Nan::New<v8::String>("message").ToLocalChecked(), 
				Nan::New<v8::String>( err->message).ToLocalChecked()
				);
		}

		v8::Local<v8::Value> argv[1] = { m };
		v8::Local<v8::Function> cbCallbackLocal = Nan::New(br->callback);
		cbCallbackLocal->Call(Nan::GetCurrentContext()->Global(), 1, argv);
		
		gst_message_unref( br->msg );
	}
	uv_queue_work( uv_default_loop(), &br->request, _doPollBus, _polledBus );
}

NAN_METHOD(Pipeline::_pollBus) {
	Nan::EscapableHandleScope scope;
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());

	if( info.Length() == 0 || !info[0]->IsFunction() ) {
		Nan::ThrowError("Callback is required and must be a Function.");
		scope.Escape(Nan::Undefined());
		return;
	}
  
	v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(info[0]);
	
	BusRequest * br = new BusRequest();
	br->request.data = br;
	br->callback.Reset(callback);
	br->obj = obj;
	obj->Ref();
	
	uv_queue_work( uv_default_loop(), &br->request, _doPollBus, _polledBus );

	scope.Escape(Nan::Undefined());
}

void Pipeline::Init( v8::Handle<v8::Object> exports ) {
	v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
	tpl->SetClassName(Nan::New<v8::String>("Pipeline").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Prototype
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("play").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_play));
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("pause").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_pause));
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("stop").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_stop));
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("forceKeyUnit").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_forceKeyUnit));
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("findChild").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_findChild));
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("pollBus").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_pollBus));

	Nan::Persistent<v8::Function> constructorPersist;
	constructorPersist.Reset(tpl->GetFunction());

	v8::Local<v8::Function> constructor = Nan::New(constructorPersist);

	exports->Set(Nan::New<v8::String>("Pipeline").ToLocalChecked(), constructor);
}

NAN_METHOD(Pipeline::New) {
	Nan::EscapableHandleScope scope;

	v8::String::Utf8Value launch( info[0]->ToString() );

	Pipeline* obj = new Pipeline( *launch );
	obj->Wrap(info.This());

	scope.Escape(info.This());
}

NAN_METHOD(Pipeline::_play) {
	Nan::EscapableHandleScope scope;
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	obj->play();
	scope.Escape(Nan::True());
}
NAN_METHOD(Pipeline::_pause) {
	Nan::EscapableHandleScope scope;
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	obj->pause();
	scope.Escape(Nan::True());
}
NAN_METHOD(Pipeline::_stop) {
	Nan::EscapableHandleScope scope;
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	obj->stop();
	scope.Escape(Nan::True());
}
NAN_METHOD(Pipeline::_forceKeyUnit) {
	Nan::EscapableHandleScope scope;
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());
	v8::String::Utf8Value name( info[0]->ToString() );
	GObject *o = obj->findChild( *name );
	int cnt( info[1]->Int32Value() );
	obj->forceKeyUnit( o, cnt );
	scope.Escape(Nan::True());
}
NAN_METHOD(Pipeline::_findChild) {
	Nan::EscapableHandleScope scope;
	Pipeline* obj = Nan::ObjectWrap::Unwrap<Pipeline>(info.This());

	v8::String::Utf8Value name( info[0]->ToString() );
	GObject *o = obj->findChild( *name );
	if ( o ) {
		scope.Escape( GObjectWrap::NewInstance( info, o ) );
	} else {
		scope.Escape(Nan::Undefined());
	}
}

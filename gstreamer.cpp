#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <v8.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

v8::Handle<v8::Value> gvalue_to_v8( const GValue *gv );

v8::Handle<v8::Object> createBuffer(char *data, int length) {
	Nan::EscapableHandleScope scope;

	v8::Local<v8::Object> buffer = Nan::CopyBuffer(data, length).ToLocalChecked();

	return scope.Escape(buffer);
}

v8::Handle<v8::Value> gstsample_to_v8( GstSample *sample ) {
	GstMapInfo map;
	GstBuffer *buf = gst_sample_get_buffer( sample );
	if( gst_buffer_map( buf, &map, GST_MAP_READ ) ) {
		const unsigned char *data = map.data;
		int length = map.size;
		v8::Handle<v8::Object> frame = createBuffer( (char *)data, length );
		return frame;
	}

	return Nan::Undefined();
}

v8::Handle<v8::Value> gstvaluearray_to_v8( const GValue *gv ) {
	if( !GST_VALUE_HOLDS_ARRAY(gv) ) {
		Nan::ThrowTypeError("not a GstValueArray");
		return Nan::Undefined();
	}

	int size = gst_value_array_get_size(gv);
	v8::Handle<v8::Array> array = Nan::New<v8::Array>(gst_value_array_get_size(gv));

	for( int i=0; i<size; i++ ) {
		array->Set( Nan::New<v8::Number>(i), gvalue_to_v8(gst_value_array_get_value(gv,i)) );
	}
	return array;
}

v8::Handle<v8::Value> gvalue_to_v8( const GValue *gv ) {
	switch( G_VALUE_TYPE(gv) ) {
		case G_TYPE_STRING:
			return Nan::New<v8::String>( g_value_get_string(gv) ).ToLocalChecked();
		case G_TYPE_BOOLEAN:
			return Nan::New<v8::Boolean>( g_value_get_boolean(gv) );
		case G_TYPE_INT:
			return Nan::New<v8::Number>( g_value_get_int(gv) );
		case G_TYPE_UINT:
			return Nan::New<v8::Number>( g_value_get_uint(gv) );
		case G_TYPE_FLOAT:
			return Nan::New<v8::Number>( g_value_get_float(gv) );
		case G_TYPE_DOUBLE:
			return Nan::New<v8::Number>( g_value_get_double(gv) );
	}

	if( GST_VALUE_HOLDS_ARRAY(gv) ) {
		return gstvaluearray_to_v8( gv );
		/* FIXME
    } else if( GST_VALUE_HOLDS_BUFFER(gv) ) {
        GstBuffer *buf = gst_value_get_buffer(gv);
        if( buf == NULL ) {
            return v8::Undefined();
        }
        return gstbuffer_to_v8( buf );
        */
	}

	GValue b = G_VALUE_INIT;
	/* Attempt to transform it into a GValue of type STRING */
	g_value_init (&b, G_TYPE_STRING);
	if( !g_value_type_transformable (G_TYPE_INT, G_TYPE_STRING) ) {
		return Nan::Undefined();
	}

//	printf("Value is of unhandled type %s\n", g_type_name( G_VALUE_TYPE(gv) ) );

	g_value_transform( gv, &b );

	const char *str = g_value_get_string( &b );
	if( str == NULL ) return Nan::Undefined();
	return Nan::New<v8::String>( g_value_get_string (&b) ).ToLocalChecked();
}

void v8_to_gvalue( v8::Handle<v8::Value> v, GValue *gv ) {
	if( v->IsNumber() ) {
        g_value_init( gv, G_TYPE_FLOAT );
        g_value_set_float( gv, v->NumberValue() );
	} else if( v->IsString() ) {
		v8::String::Utf8Value value( v->ToString() );
        g_value_init( gv, G_TYPE_STRING );
        g_value_set_string( gv, *value );
	} else if( v->IsBoolean() ) {
        g_value_init( gv, G_TYPE_BOOLEAN );
        g_value_set_boolean( gv, v->BooleanValue() );
	}

	return;
}


/* --------------------------------------------------
    GstStructure helpers
   -------------------------------------------------- */
gboolean gst_structure_to_v8_value_iterate( GQuark field_id, const GValue *val, gpointer user_data ) {
	v8::Handle<v8::Object> *obj = (v8::Handle<v8::Object>*)user_data;
	v8::Handle<v8::Value> v = gvalue_to_v8( val );
	v8::Local<v8::String> name = Nan::New<v8::String>((const char *)g_quark_to_string(field_id)).ToLocalChecked();
	(*obj)->Set(name, v);
	return true;
}

v8::Handle<v8::Object> gst_structure_to_v8( v8::Handle<v8::Object> obj, GstStructure *struc ) {
	const gchar *name = gst_structure_get_name(struc);
	obj->Set(Nan::New<v8::String>("name").ToLocalChecked(), Nan::New<v8::String>(name).ToLocalChecked());
	gst_structure_foreach( struc, gst_structure_to_v8_value_iterate, &obj );
	return obj;
}


class GObjectWrap : public Nan::ObjectWrap {
	public:
		static void Init();
		static v8::Handle<v8::Value> NewInstance( const Nan::FunctionCallbackInfo<v8::Value>& info, GObject *obj );

		void set( const char *name, const v8::Handle<v8::Value> value );

		void play();
		void pause();
		void stop();
		
	private:
		GObjectWrap();
		~GObjectWrap();
		
		GObject *obj;
		
		static Nan::Persistent<v8::Function> constructor;
		static NAN_METHOD(New);
		static NAN_METHOD(_get);
		static NAN_METHOD(_set);
		static NAN_METHOD(_play);
		static NAN_METHOD(_pause);
		static NAN_METHOD(_stop);

//		static v8::Handle<v8::Value> _onBufferAvailable(const v8::Arguments& info);
		static void _doPullBuffer( uv_work_t *req );
		static void _pulledBuffer( uv_work_t *req, int );
		static NAN_METHOD(_pull);
};

Nan::Persistent<v8::Function> GObjectWrap::constructor;

GObjectWrap::GObjectWrap() {
}

GObjectWrap::~GObjectWrap() {
}

void GObjectWrap::Init() {
	v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
	tpl->SetClassName(Nan::New<v8::String>("GObjectWrap").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Prototype
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("get").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_get)->GetFunction());
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("set").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_set)->GetFunction());

	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("pull").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_pull)->GetFunction());

	constructor.Reset(tpl->GetFunction());
}

NAN_METHOD(GObjectWrap::New) {
  	GObjectWrap* obj = new GObjectWrap();
	obj->Wrap(info.This());

	info.GetReturnValue().Set(info.This());
}

v8::Handle<v8::Value> GObjectWrap::NewInstance( const Nan::FunctionCallbackInfo<v8::Value>& info, GObject *obj ) {
	Nan::EscapableHandleScope scope;
	const unsigned argc = 1;
	v8::Local<v8::Function> constructorLocal = Nan::New(constructor);
	v8::Handle<v8::Value> argv[argc] = { info[0] };
	v8::Local<v8::Object> instance = constructorLocal->NewInstance(argc, argv);

	GObjectWrap* wrap = Nan::ObjectWrap::Unwrap<GObjectWrap>(instance);
  	wrap->obj = obj;
	
  	info.GetReturnValue().Set(instance);
  	return scope.Escape(instance);
}

NAN_METHOD(GObjectWrap::_get) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());

	v8::Local<v8::String> nameString = info[0]->ToString();
	v8::String::Utf8Value name( nameString );
	
	GObject *o = obj->obj;
    GParamSpec *spec = g_object_class_find_property( G_OBJECT_GET_CLASS(o), *name );
    if( !spec ) {
		Nan::ThrowError(v8::String::Concat(
				Nan::New<v8::String>("No such GObject property: ").ToLocalChecked(),
				nameString
		));
		scope.Escape(Nan::Undefined());
		return;
    }

    GValue gv;
    memset( &gv, 0, sizeof( gv ) );
    g_value_init( &gv, G_PARAM_SPEC_VALUE_TYPE(spec) );
    g_object_get_property( o, *name, &gv );
        
    scope.Escape( gvalue_to_v8( &gv ) );
}

void GObjectWrap::set( const char *name, const v8::Handle<v8::Value> value ) {
	GParamSpec *spec = g_object_class_find_property( G_OBJECT_GET_CLASS(obj), name );
	if( !spec ) {
		Nan::ThrowError(v8::String::Concat(
				Nan::New<v8::String>("No such GObject property: ").ToLocalChecked(),
				Nan::New<v8::String>(name).ToLocalChecked()
		));
		return;
	}

	GValue gv;
	memset( &gv, 0, sizeof( gv ) );
	v8_to_gvalue( value, &gv );
	g_object_set_property( obj, name, &gv );
}

void GObjectWrap::play() {
    gst_element_set_state( GST_ELEMENT(obj), GST_STATE_PLAYING );
}

void GObjectWrap::stop() {
    gst_element_set_state( GST_ELEMENT(obj), GST_STATE_NULL );
}

void GObjectWrap::pause() {
    gst_element_set_state( GST_ELEMENT(obj), GST_STATE_PAUSED );
}


NAN_METHOD(GObjectWrap::_set) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());

	if( info[0]->IsString() ) {
		v8::String::Utf8Value name( info[0]->ToString() );
		obj->set( *name, info[1] );
		
	} else if( info[0]->IsObject() ) {
		v8::Local<v8::Object> o = info[0]->ToObject();
		
		const v8::Local<v8::Array> props = o->GetPropertyNames();
		const int length = props->Length();
		for( int i=0; i<length; i++ ) {
			const v8::Local<v8::Value> key = props->Get(i);
			v8::String::Utf8Value name( key->ToString() );
			const v8::Local<v8::Value> value = o->Get(key);
			obj->set( *name, value );
		}		
	
	} else {
		Nan::ThrowTypeError("set expects name,value or object");
		scope.Escape(Nan::Undefined());
		return;
	}
	scope.Escape( info[1] );
}

NAN_METHOD(GObjectWrap::_play) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	obj->play();
	scope.Escape(Nan::True());
}
NAN_METHOD(GObjectWrap::_pause) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	obj->pause();
	scope.Escape(Nan::True());
}
NAN_METHOD(GObjectWrap::_stop) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());
	obj->stop();
	scope.Escape(Nan::True());
}

struct SampleRequest {
	uv_work_t request;
	Nan::Persistent<v8::Function> cb_buffer, cb_caps;
	GstSample *sample;
	GstCaps *caps;
	GObjectWrap *obj;
};

void GObjectWrap::_doPullBuffer( uv_work_t *req ) {
	SampleRequest *br = static_cast<SampleRequest*>(req->data);

	GstAppSink *sink = GST_APP_SINK(br->obj->obj);
	br->sample = gst_app_sink_pull_sample( sink );
}

void GObjectWrap::_pulledBuffer( uv_work_t *req, int n ) {
	SampleRequest *br = static_cast<SampleRequest*>(req->data);

	if( br->sample ) {
		Nan::HandleScope scope;
/* FIXME
		GstCaps *caps = gst_buffer_get_caps (br->buffer); 
		if( caps && !gst_caps_is_equal( caps, br->caps )) {
			br->caps = caps;

			GstStructure *structure = gst_caps_get_structure (caps, 0);
			
			v8::Local<v8::Object> _caps = v8::Object::New();
		    if( structure ) {
		        gst_structure_to_v8( _caps, structure );
		    }
			
			v8::Local<v8::Value> argv[1] = { _caps };
			br->cb_caps->Call(v8::Context::GetCurrent()->Global(), 1, argv);
		}
*/
		v8::Handle<v8::Value> buf = gstsample_to_v8( br->sample );
		
		
		v8::Handle<v8::Value> argv[1] = { buf };

		v8::Local<v8::Function> cbBufferLocal = Nan::New(br->cb_buffer);

		cbBufferLocal->Call(Nan::GetCurrentContext()->Global(), 1, argv);

		gst_sample_unref(br->sample);
		br->sample = NULL;
		
		return;
	}

	uv_queue_work( uv_default_loop(), &br->request, _doPullBuffer, _pulledBuffer );
}

NAN_METHOD(GObjectWrap::_pull) {
	Nan::EscapableHandleScope scope;
	GObjectWrap* obj = Nan::ObjectWrap::Unwrap<GObjectWrap>(info.This());

	if( info.Length() < 2 || !info[0]->IsFunction() || !info[1]->IsFunction() ) {
		Nan::ThrowError("Callbacks are required and must be Functions.");
		scope.Escape(Nan::Undefined());
		return;
	}
  
    if( !GST_IS_APP_SINK( obj->obj ) ) {
		Nan::ThrowError("not a GstAppSink");
		scope.Escape(Nan::Undefined());
		return;
    }

	v8::Handle<v8::Function> cb_buffer = v8::Handle<v8::Function>::Cast(info[0]);
	v8::Handle<v8::Function> cb_caps = v8::Handle<v8::Function>::Cast(info[1]);

	SampleRequest * br = new SampleRequest();
	br->request.data = br;
	br->cb_buffer.Reset(cb_buffer);
	br->cb_caps.Reset(cb_caps);
	br->obj = obj;
	obj->Ref();
	
	uv_queue_work( uv_default_loop(), &br->request, _doPullBuffer, _pulledBuffer );

	scope.Escape(Nan::Undefined());
}


class Pipeline : public Nan::ObjectWrap {
	public:
		static void Init( v8::Handle<v8::Object> exports );
		
		void play();
		void pause();
		void stop();
		void forceKeyUnit(GObject* sink, int cnt);
		
		GObject *findChild( const char *name );
		v8::Handle<v8::Value> pollBus();
		
	private:
		Pipeline( const char *launch );
		~Pipeline();
		
		GstBin *pipeline;
		
		static NAN_METHOD(New);
		static NAN_METHOD(_play);
		static NAN_METHOD(_pause);
		static NAN_METHOD(_stop);
		static NAN_METHOD(_forceKeyUnit);
		static NAN_METHOD(_findChild);

		static void _doPollBus( uv_work_t *req );
		static void _polledBus( uv_work_t *req, int );
		static NAN_METHOD(_pollBus);
		
};

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
			Nan::New<v8::FunctionTemplate>(_play)->GetFunction());
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("pause").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_pause)->GetFunction());
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("stop").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_stop)->GetFunction());
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("forceKeyUnit").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_forceKeyUnit)->GetFunction());
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("findChild").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_findChild)->GetFunction());
	tpl->PrototypeTemplate()->Set(Nan::New<v8::String>("pollBus").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(_pollBus)->GetFunction());

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

void init( v8::Handle<v8::Object> exports ) {
	gst_init( NULL, NULL );
	GObjectWrap::Init();
	Pipeline::Init(exports);
}

NODE_MODULE(gstreamer_superficial, init);
